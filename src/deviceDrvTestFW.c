/**=============================================================================
  $Workfile: deviceDrvTestFW.c $

  File Description: Generic test framework, to test any kind of device driver or
                    network driver.

  Author: Johnnie Alan

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

      * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
  TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
  PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
  OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  ============================================================================*/

/*----------------------- I N C L U D E S   S E C T I O N --------------------*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <stdarg.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlreader.h>

#include "deviceDrvTestFW.h"
#include "console.h"

#define LOG_TO_USR 		    1
#define LOG_TO_DBG  		2

#define RET_SUCCESS		    0
#define RET_FAILURE	        -1

#define NODE_TYPE_ELEMENT  	1
#define NODE_TYPE_TEXT     	3
#define NODE_DEPTH_2       	2

#define TESTFW_CFG    "testFWCfg.xml"


/*------------------ G L O B A L   D E C L A R A T I O N S -------------------*/


unsigned int g_uObjectHandleCount;      /* Object handle reference counter */

int g_nMasterSockfd;                  	/* Master device socket */
int g_nSlaveSockfd;                   	/* Slave device socket */

unsigned char *g_pszIPAddress;       	/* IP address of master device */
unsigned char *g_pszSharedObject;   	/* Pointer to shared object */

unsigned char g_szFileName[100];        /* To store the test report filename */
unsigned char g_szLogBuffer[255];       /* To store the string to be logged in
                                           Test report*/

bool g_bSocketFlag;                     /* To check whether socket created */

void *g_hObjectHandle;                  /* Handle for a shared object */

E_MODE_TYPE g_eMode;                    /* Device mode of operation */

struct sockaddr_in DeviceAddress;       /* Master/Slave device address */
sUserTestSuite_t *g_pUserTestSuite;     /* Pointer to user test suite */

FILE *g_pDebugLogFile;					/* File pointer for storing debug log */
bool g_bDebugLogFlag;					/* Flag to check whether debug file
										   created */

pid_t cpid;

/*------------------ F U N C T I O N   D E F I N I T I O N S -----------------*/

/**=============================================================================

    Function Name   : deviceDrvTstFWDebug
    Description     : Function to send message to console UI or debug log file.
    Arguments       :
                      Name              Dir         Description
                      @type             In          Log type
                      @pszMsg			In			Message to be logged
                      @variable argument list

    Returns         : None

  ============================================================================*/

void deviceDrvTstFWDebug(int ntype, char *pszMsg, ...)
{
	unsigned char szDateTime[20];
    unsigned char szDbgFileName[30] = "debug_fw_";
    unsigned char szLog[200];
    struct tm *pTimeInfo;

	va_list fmt;
	va_start(fmt, pszMsg);

	time_t Rawtime;
    time(&Rawtime);

    memset(szLog, 0, 200);

    switch (ntype)
	{
		case LOG_TO_USR:
			if (g_bConsoleInitialized)
			{
				writeDbgMessage(pszMsg, fmt);
			}
			else
			{
				vfprintf(stderr, pszMsg, fmt);
			}
		break;

		case LOG_TO_DBG:
			if (g_bConsoleInitialized)
			{
				if(g_bDebugLogFlag == false)
				{
					if(g_eMode == E_SLAVE)
						strcat(szDbgFileName, SLAVE);
					else if(g_eMode == E_MASTER)
						strcat(szDbgFileName, MASTER);
					else if(g_eMode == E_STANDALONE)
						strcat(szDbgFileName, STANDALONE);

					strcat(szDbgFileName, ".txt");

					g_pDebugLogFile = fopen(szDbgFileName, "a");

					g_bDebugLogFlag = true;
				}

				/* Get the local time into the timeinfo structure */
				pTimeInfo = localtime(&Rawtime);

				strftime(szDateTime, sizeof(szDateTime), "%m/%d/%Y_%H:%M:%S",
					pTimeInfo);

				vsprintf(szLog, pszMsg, fmt);

				fprintf(g_pDebugLogFile, "%s%s", szDateTime, szLog);
			}
			else
			{
				vfprintf(stderr, pszMsg, fmt);
			}
		break;
	}

	va_end(fmt);
}


/**=============================================================================

    Function Name   : getValue
    Description     : This function is for fetching the value of the node from
    				  XML file
    Arguments       :
                      Name              Dir         Description
                      @pReader          In          XML reader pointer
                      @out              Out         Output parameter

    Returns         : None

  ============================================================================*/

static void getValue(xmlTextReaderPtr pReader, unsigned char **pszOut)
{
    const xmlChar *pValue = NULL;
    int nRet = 0;

    /* Read Next Node */
    nRet = xmlTextReaderRead(pReader);
    if (nRet != 1)
    {
    	return;
    }

    pValue = xmlTextReaderConstValue(pReader);

    if (NULL != pValue  && xmlTextReaderDepth(pReader) == NODE_DEPTH_2 &&
    	xmlTextReaderNodeType(pReader) == NODE_TYPE_TEXT)
    {
         memcpy(*pszOut, pValue, xmlStrlen(pValue));
    }
}


/**=============================================================================

    Function Name   : getTokenValue
    Description     : This function is used to parse XML file.
    Arguments       :
                      Name              Dir         Description
                      @pszFile          In          File name with path
                      @pszToken         In          Key to fetch value
                      @pszValue         Out         Output value for the token

    Returns         : Returns RET_FAILURE on failure, RET_SUCCESS on success

  ============================================================================*/

int getTokenValue(char *pszFile, const char *pszToken, unsigned char *pszValue)
{
    xmlTextReaderPtr pConfigRead = NULL;
    const char *pszName = NULL;
    int nRet = 0, eRet = RET_SUCCESS;

    LIBXML_TEST_VERSION;

    if (NULL == pszValue)
    {
    	return RET_FAILURE;
    }

    pConfigRead = xmlReaderForFile(pszFile, NULL, 0);
    if (pConfigRead != NULL)
    {
        nRet = xmlTextReaderRead(pConfigRead);
        while (nRet == 1)
        {
            pszName = (char*)xmlTextReaderConstName(pConfigRead);

            /* Consumes the Entire node */
            if ((strcmp(pszToken,pszName) == 0) &&
                (xmlTextReaderNodeType(pConfigRead) == NODE_TYPE_ELEMENT))
            {
                 getValue(pConfigRead,&pszValue);
                 nRet = 0,eRet = RET_FAILURE;
                 break;
            }
            nRet = xmlTextReaderRead(pConfigRead);
        }
        xmlFreeTextReader(pConfigRead);
    }
    else
    {
    	eRet = RET_FAILURE;
        fprintf(stderr, "Error in reading xml file\n");
    }

    xmlCleanupParser();
    return eRet;
}


/**=============================================================================

    Function Name   : createSocket
    Description     : This function is used to create Ethernet socket which will
                      be used to handshake with the other device for testing
    Arguments       : None
    Returns         : None

  ============================================================================*/

void createSocket(void)
{
    int nTest = 0;
    unsigned int uPortNumber = 0;
    unsigned int uAttempts = 0;
    time_t RandomPortGenerator;

    /* Device address length */
    unsigned int uAddressLength = sizeof(DeviceAddress);

    /* Create socket for master mode */
    if (g_eMode == E_MASTER)
    {
    	//fprintf(stderr, "EXNAK = %2x\n",nTest);

        fprintf(stderr, "Allocating socket...\n");

        if ((g_nMasterSockfd = socket(AF_INET, SOCK_STREAM, 0)) <0)
        {
            fprintf(stderr, "Socket allocation failed\n");
            freeMemory(1);
        }

        fprintf(stderr, "Socket allocated successfully\n");

        /* Set flag to indicate that socket is created */
        g_bSocketFlag = true;

        /* Initialize random number generator*/
        srand((unsigned) time(&RandomPortGenerator));

        /* Generate random port number and bind the master to it*/
        for (uAttempts = 0; uAttempts < 100; ++uAttempts)
        {
            uPortNumber = 1024 + rand() % 65536;
			memset(&DeviceAddress, '0', uAddressLength);
            DeviceAddress.sin_family = AF_INET;
            DeviceAddress.sin_addr.s_addr = INADDR_ANY;
            DeviceAddress.sin_port = htons(uPortNumber);

            if (bind(g_nMasterSockfd, (struct sockaddr *)&DeviceAddress,
            	uAddressLength)<0)
            {
                fprintf(stderr, "Bind to socket failed\n");
                freeMemory(1);
            }
            else
            {
                fprintf(stderr, "Port Number: %d\n", uPortNumber);
                break;
            }
        }

        if (listen(g_nMasterSockfd, 3) < 0)
        {
            fprintf(stderr, "Listen failed\n");
            freeMemory(1);
        }

        fprintf(stderr, "Waiting for the slave to connect...\n");

        /* Accept the connection from the slave */
        if ((g_nSlaveSockfd = accept(g_nMasterSockfd,
            (struct sockaddr *)&DeviceAddress,
            (socklen_t*)&uAddressLength)) < 0)
        {
            fprintf(stderr, "Client connection failed\n");
            freeMemory(1);
		}
        else
        {
            fprintf(stderr, "Client connected successfully\n");
        }
    }

    /* Create socket for slave mode and connect to the port number
     * on which master is listening
     */
    else if (g_eMode == E_SLAVE)
    {
        fprintf(stderr, "Enter master port number: ");

        scanf("%d", &uPortNumber);

        fprintf(stderr, "Allocating socket...\n");
        if ((g_nSlaveSockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            fprintf(stderr, "Socket allocation failed\n");
            freeMemory(1);
        }

        fprintf(stderr, "Socket allocated successfully...\n");

        /* Set flag to indicate that socket is created */
        g_bSocketFlag = true;

        memset(&DeviceAddress, '0', uAddressLength);

        DeviceAddress.sin_family = AF_INET;
        DeviceAddress.sin_port = htons(uPortNumber);

        /* Check if master ip address is valid */
        if (inet_pton(AF_INET, g_pszIPAddress, &DeviceAddress.sin_addr) <= 0)
        {
            fprintf(stderr, "Invalid IP address/IP address not supported\n");
            freeMemory(1);
        }

        fprintf(stderr, "Connecting on port number %d...\n",
        	uPortNumber);
        if (connect(g_nSlaveSockfd, (struct sockaddr *)&DeviceAddress,
        	uAddressLength) < 0)
        {
            fprintf(stderr, "Connect failed \n");
            freeMemory(1);
        }

        fprintf(stderr, "Connected successfully\n");
    }
}


/**=============================================================================

    Function Name   : loadSharedObject
    Description     : Function to load the shared object containing test cases.
    Arguments       : None
    Returns         : 0 ON SUCCESS, -1 ON FAILURE

  ============================================================================*/

int loadSharedObject(void)
{
    unsigned char szFilenameBuffer[30] = "./";

    strcat(szFilenameBuffer, g_pszSharedObject);
    strcat(szFilenameBuffer, ".so");

    deviceDrvTstFWDebug(LOG_TO_DBG, "\tLoading shared object: %s [%s:%d]\n",
    	g_pszSharedObject, __FILENAME__, __LINE__);

    /* Load shared object */
    g_hObjectHandle = dlopen(szFilenameBuffer, RTLD_LAZY);

    /* If loading shared object failed, then exit */
    if (!g_hObjectHandle)
    {
        deviceDrvTstFWDebug(LOG_TO_USR, "Loading shared object failed - %s\n",
        	dlerror());
        return -1;
    }
    else
    {
        deviceDrvTstFWDebug(LOG_TO_DBG,
        	"\tShared object loaded successfully [%s:%d]\n",
        	__FILENAME__, __LINE__);
        g_uObjectHandleCount++;
    }

    /* Clear any existing error */
    dlerror();

    return 0;
}


/**=============================================================================

    Function Name   : initializeTestSummary
    Description     : Function to initialize the test summary, which stores the
                      test case and test suite summary.
    Arguments       : None
    Returns         : 0 ON SUCCESS, -1 ON FAILURE

  ============================================================================*/

int initializeTestSummary(void)
{
    deviceDrvTstFWDebug(LOG_TO_DBG,
    	"\tInitializing test summary...[%s:%d]\n", __FILENAME__, __LINE__);

    /* Allocating memory for test summary */
    g_pSummary =
        (sTestSummary_t*)malloc(sizeof(sTestSummary_t));

    memset(g_pSummary, 0, sizeof(g_pSummary));

    /* Initializing the members of the test summary */
    if (g_pSummary != NULL)
    {
        g_pSummary->uNumberOfTests = 0;
        g_pSummary->uNumberOfSuites = 0;
        g_pSummary->uNumberOfTestsPassed = 0;
        g_pSummary->uNumberOfTestsFailed = 0;
        g_pSummary->uNumberOfTestsTimeout = 0;
        g_pSummary->uNumberOfTestsNotFound = 0;
        g_pSummary->uNumberOfSuitesNotFound = 0;

        deviceDrvTstFWDebug(LOG_TO_DBG,
            "\tTest summary initialized successfully [%s:%d]\n",
            __FILENAME__, __LINE__);

        return 0;
    }
    else
    {
        deviceDrvTstFWDebug(LOG_TO_USR,
            "No memory to initialize the test summary\n");
        return -1;
    }
}


/**=============================================================================

    Function Name   : addSuitesAndCases
    Description     : Function to add test cases and test suites from the shared
                      objects to hash list test framework.
    Arguments       : None
    Returns         : 0 ON SUCCESS, -1 ON FAILURE

  ============================================================================*/

int addSuitesAndCases(void)
{
    sTestCase_t *pHead1 = NULL, *pIndividual = NULL, *pTemp1 = NULL;
    sTestCase_t *pTemp2 = NULL;
    sTestSuite_t *pSuite = NULL, *pTemp3 = NULL;
    int i = 1;

    deviceDrvTstFWDebug(LOG_TO_DBG,
    	"\tAdding test suites and test cases... [%s:%d]\n",
    	__FILENAME__, __LINE__);

    if (strcmp(g_pUserTestSuite->szName, TEST_SUITE_END) != 0)
    {
		while (strcmp(g_pUserTestSuite->szName, TEST_SUITE_END) != 0)
		{
			/* Add test case */
			pSuite = malloc(sizeof(sTestSuite_t));

			if (!pSuite)
			{
				deviceDrvTstFWDebug(LOG_TO_USR,
					"No memory to add test suite\n");
				freeMemory(1);
			}

			memset(pSuite, 0, sizeof(sTestSuite_t));

			if(strcmp(g_pUserTestSuite->pTestCase->szName, TEST_CASE_END) != 0)
			{

				/* Iterate over the array of user test case structure */
				while (
					strcmp(g_pUserTestSuite->pTestCase->szName, TEST_CASE_END) != 0)
				{
					/* Add test case */
					pIndividual = malloc(sizeof(sTestCase_t));
					if (!pIndividual)
					{
						deviceDrvTstFWDebug(LOG_TO_USR, "No memory to add test case\n");
						freeMemory(1);
					}

					memset(pIndividual, 0, sizeof(sTestCase_t));
					strcpy(pIndividual->szName, g_pUserTestSuite->pTestCase->szName);
					pIndividual->nTestCaseIden = i++;
					pIndividual->uTimeout = g_pUserTestSuite->pTestCase->uTimeout;
					pIndividual->fnPtrTestCase = dlsym(g_hObjectHandle,
						g_pUserTestSuite->pTestCase->pTestCaseFuncName);
					pIndividual->eStatus = E_NOT_FOUND;

					deviceDrvTstFWDebug(LOG_TO_DBG, "\tAdding test case %s [%s:%d]\n",
						pIndividual->szName, __FILENAME__, __LINE__);

					HASH_ADD(hh, pHead1, szName, strlen(pIndividual->szName),
						pIndividual);

					g_pUserTestSuite->pTestCase++;
					pIndividual=NULL;
					g_pSummary->uNumberOfTests++;
				}

				strcpy(pSuite->szName, g_pUserTestSuite->szName);
				pSuite->fnPtrCleanup = dlsym(g_hObjectHandle,
					g_pUserTestSuite->pCleanupFuncName);
				pSuite->sTestCaseList = pHead1;


				deviceDrvTstFWDebug(LOG_TO_DBG,
					"\tAdding test suite %s [%s:%d]\n",
				   pSuite->szName, __FILENAME__, __LINE__);

				HASH_ADD_STR(g_pSuiteHead, szName, pSuite);
				pHead1 = NULL;
				g_pUserTestSuite++;
				g_pSummary->uNumberOfSuites++;
			}
			else
			{
				if(g_bDebugLogFlag)
					deviceDrvTstFWDebug(LOG_TO_DBG,
						"\tTest suite %s is empty [%s:%d]\n",
						g_pUserTestSuite->szName, __FILENAME__, __LINE__);

				deviceDrvTstFWDebug(LOG_TO_USR,
					"Test suite %s is empty\n", g_pUserTestSuite->szName);

				free(pSuite);
				g_pUserTestSuite++;
			}
		}
		deviceDrvTstFWDebug(LOG_TO_DBG,
			"\tTest suites and test cases added successfully [%s:%d]\n",
			__FILENAME__, __LINE__);

		return 0;
	}
	else
	{
		if(g_bDebugLogFlag)
			deviceDrvTstFWDebug(LOG_TO_DBG,
			"\tNo test suites and test cases in the shared object [%s:%d]\n",
			__FILENAME__, __LINE__);

		deviceDrvTstFWDebug(LOG_TO_USR,
			"No test suites and test cases in the shared object\n");

		return -1;
	}
}


/**=============================================================================

    Function Name   : initializeTestFramework
    Description     : Function to initialize the test framework.
                      Below are the steps of initialization.
                      Steps:    1. Load shared object
                                2. Initialize the test summary
                                3. Add test suites and test cases
    Arguments       : None
    Returns         : 0 ON SUCCESS, exits ON FAILURE

  ============================================================================*/

int initializeTestFramework(void)
{
	int nRet = 0;
    deviceDrvTstFWDebug(LOG_TO_DBG,"\tInitializing test framework... [%s:%d]\n",
    	__FILENAME__, __LINE__);

    /* Load shared object */
    nRet = loadSharedObject();
    if(nRet != 0)
    	return -1;

    g_pUserTestSuite = dlsym(g_hObjectHandle, "aTestSuite");

    /* Check for symbol */
    if (!g_pUserTestSuite)
    {
    	if(g_bDebugLogFlag)
        	deviceDrvTstFWDebug(LOG_TO_DBG,
            "\taTestSuite array not found in shared object [%s:%d]\n",
            __FILENAME__, __LINE__);

        deviceDrvTstFWDebug(LOG_TO_USR,
        	"Test suite array not found in shared object, Please check and try again...\n");

        return -1;
    }

    /* Initialize test summary */
    nRet = initializeTestSummary();
    if(nRet != 0)
    	return -1;

    /* Add test cases and suites */
    nRet = addSuitesAndCases();
    if (nRet != 0)
    	return -1;

    deviceDrvTstFWDebug(LOG_TO_DBG,
        "\tTest framework initialized successfully [%s:%d]\n",
        __FILENAME__, __LINE__);
    return 0;
}


/**=============================================================================

    Function Name   : constructFrameworkMsg
    Description     : Function to construct the handshake message packet which
                      will be transmitted over ethernet.
    Arguments       :
                      Name              Dir     Description
                      @pMsg             In      Pointer to msg
                      @szOutBuffer      Out     Output buffer

    Returns         : Message packet length

  ============================================================================*/

int constructFrameworkMsg(sHandshakeMsg_t *pMsg, unsigned char szOutBuffer[])
{
    unsigned char szMsgBuffer[1024] = {0};
    unsigned int uMsgLength = 0;
    unsigned int uMsgChecksum = 0;


    /* Create a packet */

    /* Add slave id */
    szMsgBuffer[1] = pMsg->uSlaveId;

    /* Add message type to be sent */
    szMsgBuffer[2] = pMsg->eMsgType;
    uMsgLength = 3;

    switch (pMsg->eMsgType)
    {
    	case E_MSG_HELLO:
        case E_MSG_ACK:
        case E_MSG_FRAMEWORK_INIT:
        case E_MSG_NACK:
        case E_MSG_TST_CASE_LIST_COMPLETED:
        case E_MSG_TST_SUITE_LIST_COMPLETED:
        case E_MSG_CLOSE:
        case E_MSG_TST_RUN:
        break;

        /* Add test case / test suite name to packet*/
        case E_MSG_TST_CASE_NAME:
        case E_MSG_TST_SUITE_NAME:
        {
            uMsgLength ++;
            szMsgBuffer[3] = strlen(pMsg->Msg.szName);
            uMsgLength += strlen(pMsg->Msg.szName);
            memcpy(&szMsgBuffer[4],pMsg->Msg.szName,strlen(pMsg->Msg.szName));
            memset(pMsg->Msg.szName,0,30);
        }
        break;

        case E_MSG_STATUS:
        {
            uMsgLength += sizeof(E_TST_STATUS);
            szMsgBuffer[3] = pMsg->Msg.eStatus ;
        }
        break;
    }

    /* Total length of the packet */
    szMsgBuffer[0] = uMsgLength+sizeof(uMsgChecksum);

    uMsgChecksum = calculateChecksum(szMsgBuffer, uMsgLength);

    /* Copy checksum LSB */
    szMsgBuffer[uMsgLength] = (unsigned char)(uMsgChecksum & 0xff);

    /* Copy checksum MSB */
    szMsgBuffer[uMsgLength + 1] = (unsigned char)((uMsgChecksum>>8) & 0xff);

    uMsgLength = uMsgLength+sizeof(uMsgChecksum);

    /* Copy the packet to the output buffer */
    memset(szOutBuffer,0,512);
    memcpy(szOutBuffer,szMsgBuffer,uMsgLength);

    /* Return total packet length */
    return uMsgLength;
}


/**=============================================================================

    Function Name   : parseFrameworkMsg
    Description     : Function to parse the handshake message packet which
                      will be received over ethernet.
    Arguments       :
                      Name              Dir     Description
                      @pMsg             In      Pointer to msg
                      @szInBuffer       In      Output buffer

    Returns         : None

  ============================================================================*/

void parseFrameworkMsg(sHandshakeMsg_t *pMsg, unsigned char szInBuffer[])
{
    unsigned int uMsgLength = szInBuffer[0];
    unsigned int uMsgNameLength = 0;
    unsigned int uMsgChecksum = 0;

    uMsgChecksum = (szInBuffer[uMsgLength - sizeof(uMsgChecksum)] & 0xff)
     | ((szInBuffer[(uMsgLength - sizeof(uMsgChecksum))+1]<<8) & 0xff00);

    if(uMsgChecksum!=calculateChecksum(szInBuffer,
                                    (uMsgLength - sizeof(uMsgChecksum))))
    {
        deviceDrvTstFWDebug(LOG_TO_USR, "Corrupted Message\n");
        freeMemory(1);
    }

    pMsg->uSlaveId = szInBuffer[1];
    pMsg->eMsgType = szInBuffer[2];

    switch (pMsg->eMsgType)
    {
        case E_MSG_HELLO:
        case E_MSG_FRAMEWORK_INIT:
        case E_MSG_ACK:
        case E_MSG_NACK:
        case E_MSG_TST_SUITE_LIST_COMPLETED:
        case E_MSG_TST_CASE_LIST_COMPLETED:
        case E_MSG_CLOSE:
        case E_MSG_TST_RUN:
        break;

        case E_MSG_TST_CASE_NAME:
        case E_MSG_TST_SUITE_NAME:
        {
            uMsgNameLength = szInBuffer[3];
            uMsgLength += strlen(pMsg->Msg.szName);

            memset(pMsg->Msg.szName, 0, 30);

            /* Copy the test case/test suite name received from the packet */
            memcpy(pMsg->Msg.szName,&szInBuffer[4],uMsgNameLength);
        }
        break;
        case E_MSG_STATUS:
        {
            pMsg->Msg.eStatus = szInBuffer[3] ;
        }
        break;
    }
}

/*******************************************************************************
*
*   Function Name   : calculateChecksum
*   Description     : Function to calculate checksum.
*   Arguments       :
*                     Name              Dir     Description
*                     @pszBuffer        In      Pointer to msg
*                     @uLength     		In      Length of the buffer
*
*   Returns         : Checksum value
*
*******************************************************************************/

unsigned short int calculateChecksum(unsigned char *pszBuffer,
    unsigned int uLength)
{
    unsigned short int uChecksum = 0;

    while(uLength!=0)
    {
        uChecksum = uChecksum + *pszBuffer;
        pszBuffer++;
        uLength--;
    }

    return uChecksum;
}

/**=============================================================================

    Function Name   : sendMessage
    Description     : Function to send the constructed message packet over
                      ethernet.
    Arguments       :
                      Name              Dir     Description
                      @pMsg             In      Pointer to msg

    Returns         : None

  ============================================================================*/

void sendMessage(sHandshakeMsg_t *pMsg)
{
    unsigned int uMsgLength = 0;
    unsigned char szOutBuffer[1024];

    /* Construct Packet to be sent over ethernet */
    uMsgLength = constructFrameworkMsg(pMsg, szOutBuffer);


    /*
    if (g_eMode == E_SLAVE)
    {
        sleep(1);	//for testing only
    }
    */

    /* Send the packet */
    send(g_nSlaveSockfd, szOutBuffer, uMsgLength, 0);
}


/**=============================================================================

    Function Name   : receiveMessage
    Description     : Function to receive the constructed message over ethernet.
    Arguments       :
                      Name              Dir     Description
                      @pMsg             In      Pointer to msg

    Returns         : None

  ============================================================================*/

int receiveMessage(sHandshakeMsg_t *pMsg)
{
    int nRet = 0;
    unsigned char szInBuffer[1024] = {0};

    /* Receive the packet */
    nRet = recv(g_nSlaveSockfd, szInBuffer, 1024,0);

    if(nRet == 0)
    {
    	return nRet;
    }

    /* Parse the packet to get message type and test case/test suite name */
    parseFrameworkMsg(pMsg, szInBuffer);

    return nRet;
}


/**=============================================================================

    Function Name   : generateFileName
    Description     : Function to generate a filename with the timestamp for
    				  XML test report.
    Arguments       : None
    Returns         : None

  ============================================================================*/

void generateFileName(void)
{
    /* Structure containing date and time */
    struct tm *pTimeInfo;

    time_t Rawtime;
    time(&Rawtime);

    /* Get the local time into the timeinfo structure */
    pTimeInfo = localtime(&Rawtime);

    /* Format the date and time and append to the filename */
    strftime(g_szFileName, sizeof(g_szFileName),
        "Test_Report_%m%d%Y_%H%M%S.xml", pTimeInfo);
}


/**=============================================================================

    Function Name   : generateXMLReport
    Description     : Function to generate test report in the XML format.
                      Furthur XSL(XML Style Sheet) is used to display report
                      in tabular format.
    Arguments       : None
    Returns         : None

  ============================================================================*/

void generateXMLReport(void)
{
    sTestSuite_t *pSuite = NULL, *pTemp1 = NULL;
    sTestCase_t *pTestcase = NULL, *pTemp2 = NULL;
    xmlOutputBufferPtr *buf = NULL;
    /* XML document pointer */
    xmlDocPtr pDoc = NULL;

    /* XML node pointers */
    xmlNodePtr pPiNode = NULL;
    xmlNodePtr pRootNode = NULL;
    xmlNodePtr pTestSuiteNode = NULL;
    xmlNodePtr pTestCaseNode = NULL;
    xmlNodePtr pSummaryNode = NULL;

    deviceDrvTstFWDebug(LOG_TO_DBG,
    	"\tGenerating Test Report...[%s:%d]\n", __FILENAME__, __LINE__);

    /* Generate filename with timestamp */
    generateFileName();

    /* LIBXML_TEST_VERSION
     * This macro must always be called once, and only once, in the main thread
     * of execution. It will initialize the data structures of the libxml2
     * library and check for any ABI mis-matches between the library the program
     * was compiled against and the one it is running with.
     */

    //LIBXML_TEST_VERSION;

    /* Create document node */
    pDoc = xmlNewDoc(BAD_CAST "1.0");

    /* Create root node */
    pRootNode = xmlNewNode(NULL, BAD_CAST "TestFramework");

    /* Add root node to the document node */
    xmlDocSetRootElement(pDoc, pRootNode);

    /* Create processing instruction node */
    pPiNode = xmlNewDocPI(pDoc, BAD_CAST "xml-stylesheet",
                BAD_CAST "type='text/xsl' href='testRsltStyle.xsl'");

    /* Adding processing instruction node */
    xmlAddPrevSibling(pRootNode, pPiNode);

    /* Create filename node */
    xmlNewChild(pRootNode, NULL, BAD_CAST "Filename", BAD_CAST g_szFileName);

    /* Create summary node */
    pSummaryNode = xmlNewChild(pRootNode, NULL, BAD_CAST "Summary", NULL);

    /* Adding child nodes of summary node */
    sprintf(g_szLogBuffer, "%d", g_pSummary->uNumberOfSuites);
    xmlNewChild(pSummaryNode, NULL, BAD_CAST "Suites",
                                      BAD_CAST g_szLogBuffer);

    sprintf(g_szLogBuffer, "%d", g_pSummary->uNumberOfTests);
    xmlNewChild(pSummaryNode, NULL, BAD_CAST "Tests", BAD_CAST g_szLogBuffer);

    /* Traverse through the test cases and add fetch the test case status
     * and elapsed time
     */

    /* Check whether test summary is created */
    if (g_pSummary == NULL)
    {
        deviceDrvTstFWDebug(LOG_TO_USR, "No active test summary\n");
        freeMemory(1);
    }

    HASH_ITER(hh, g_pSuiteHead, pSuite, pTemp1)
    {
        if(pSuite->eStatus == E_FOUND)
        {
            HASH_ITER(hh,pSuite->sTestCaseList, pTestcase, pTemp2)
            {
               /* Create test suite node */
               pTestSuiteNode = xmlNewChild(pRootNode, NULL,
                                       BAD_CAST "TestSuite", NULL);
               xmlNewChild(pTestSuiteNode, NULL, BAD_CAST "Name",
                              BAD_CAST pSuite->szName);

               /* Create test case node */
               pTestCaseNode = xmlNewChild(pTestSuiteNode, NULL,
                                       BAD_CAST "TestCase", NULL);
               xmlNewChild(pTestCaseNode, NULL, BAD_CAST "Name",
                           BAD_CAST pTestcase->szName);


               if (pTestcase->eStatus == E_PASSED)
               {
                   /* Create test case result node */
                   xmlNewChild(pTestCaseNode, NULL, BAD_CAST "Result",
                                                   BAD_CAST "PASSED");

                   /* Create elapsed time node */
                   sprintf(g_szLogBuffer, "%lf",
                                       pTestcase->dElapsedTime);
                   xmlNewChild(pTestCaseNode, NULL,
                       BAD_CAST "ElapsedTime", BAD_CAST g_szLogBuffer);
               }
               else if (pTestcase->eStatus == E_FAILED)
               {
                   /* Create test case result node */
                   xmlNewChild(pTestCaseNode, NULL, BAD_CAST "Result",
                                                   BAD_CAST "FAILED");

                   /* Create elapsed time node */
                   sprintf(g_szLogBuffer, "%lf",
                                       pTestcase->dElapsedTime);
                   xmlNewChild(pTestCaseNode, NULL,
                       BAD_CAST "ElapsedTime", BAD_CAST g_szLogBuffer);
               }
               else if (pTestcase->eStatus == E_TIMEOUT)
               {
                   /* Create test case result node */
                   xmlNewChild(pTestCaseNode, NULL, BAD_CAST "Result",
                                                   BAD_CAST "TIMEOUT");

                   /* Create elapsed time node */
                   xmlNewChild(pTestCaseNode, NULL,
                       BAD_CAST "ElapsedTime", BAD_CAST "-");
               }
               else if (pTestcase->eStatus == E_NOT_FOUND)
               {
                   /* Create test case result node */
                   xmlNewChild(pTestCaseNode, NULL, BAD_CAST "Result",
                                                   BAD_CAST "NOT FOUND");

                   /* Create elapsed time node */
                   xmlNewChild(pTestCaseNode, NULL,
                       BAD_CAST "ElapsedTime", BAD_CAST "-");
               }
           }
       }
       else
       {
                /* Create test suite node */
               pTestSuiteNode = xmlNewChild(pRootNode, NULL,
                                       BAD_CAST "TestSuite", NULL);
               xmlNewChild(pTestSuiteNode, NULL, BAD_CAST "Name",
                              BAD_CAST pSuite->szName);

               /* Create test case node */
               pTestCaseNode = xmlNewChild(pTestSuiteNode, NULL,
                                       BAD_CAST "TestCase", NULL);

               xmlNewChild(pTestCaseNode, NULL, BAD_CAST "Name", BAD_CAST "-");

               /* Create test suite result node */
               xmlNewChild(pTestCaseNode, NULL, BAD_CAST "Result",
                                                   BAD_CAST "NOT FOUND");

               /* Create elapsed time node */
               xmlNewChild(pTestCaseNode, NULL,
                       BAD_CAST "ElapsedTime", BAD_CAST "-");
       }
   }

    /* Create child nodes of summary node*/
    sprintf(g_szLogBuffer, "%d", g_pSummary->uNumberOfTestsNotFound);
    xmlNewChild(pSummaryNode, NULL, BAD_CAST "TestsNotFound",
                                                    BAD_CAST g_szLogBuffer);

    sprintf(g_szLogBuffer, "%d", g_pSummary->uNumberOfSuitesNotFound);
    xmlNewChild(pSummaryNode, NULL, BAD_CAST "SuitesNotFound",
                                                    BAD_CAST g_szLogBuffer);

    sprintf(g_szLogBuffer, "%d", g_pSummary->uNumberOfTestsPassed);
    xmlNewChild(pSummaryNode, NULL, BAD_CAST "TestsPassed",
                                                    BAD_CAST g_szLogBuffer);

    sprintf(g_szLogBuffer, "%d", g_pSummary->uNumberOfTestsFailed);
    xmlNewChild(pSummaryNode, NULL, BAD_CAST "TestsFailed",
                                                    BAD_CAST g_szLogBuffer);

    sprintf(g_szLogBuffer, "%d", g_pSummary->uNumberOfTestsTimeout);
    xmlNewChild(pSummaryNode, NULL, BAD_CAST "TestsTimeout",
                                                    BAD_CAST g_szLogBuffer);

    /* Save the xml tree to the filename */
    xmlSaveFormatFileEnc(g_szFileName, pDoc, "UTF-8", 1);

    deviceDrvTstFWDebug(LOG_TO_DBG, "\tTest Report Generated [%s:%d]\n",
    	__FILENAME__, __LINE__);

    /* Free document node pointer */
    xmlFreeDoc(pDoc);

    /* Cleanup memory allocated by the library */
    xmlCleanupParser();

    xmlMemoryDump();
}


/**=============================================================================

    Function Name   : peek
    Description     : Function to fetch the number of bytes available to read
    Arguments       : None
    Returns         : Number of bytes

  ============================================================================*/

int peek(void)
{
    int nBytesAvailable = 0, nError = 0;
    fd_set readset, errors;

    /* Initialize time out struct for select() */
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 10000;

    /* Initialize the set */
    FD_ZERO(&readset);
    FD_SET(g_nSlaveSockfd, &readset);
    FD_ZERO(&errors);
    FD_SET(g_nSlaveSockfd, &errors);

    /* Now, check for readability */
    nError = select(g_nSlaveSockfd+1, &readset, NULL, &errors, &tv);
    if (nError > 0)
    {
        if (FD_ISSET(g_nSlaveSockfd, &errors))
        {
            nBytesAvailable=-1;
        }
        else if (FD_ISSET(g_nSlaveSockfd, &readset))
        {
            nBytesAvailable=1;
        }
        /* Clear flags */
        FD_CLR(g_nSlaveSockfd, &readset);
        FD_CLR(g_nSlaveSockfd, &errors);
    }
    return nBytesAvailable;
}


/**=============================================================================

    Function Name   : checkTimer
    Description     : Function to set the timer or to get the remaining
    				  timer value
    Arguments       :
                      Name                  Dir         Description
                      @pTimerID				In			Pointer to timer
                      @nMsec				In			Time in milliseconds
                      @bSet					In			Flag
    Returns         : 0 if bSet is true, else milliseconds.

  ============================================================================*/

uint64_t checkTimer(timer_t *pTimerID, unsigned int nMsec, bool bSet)
{
    struct itimerspec its;
    if (true == bSet)
    {
    	its.it_interval.tv_sec = 0;
        its.it_interval.tv_nsec = 0;
        its.it_value.tv_sec = nMsec/1000;
        its.it_value.tv_nsec = (nMsec%1000)*1000000;

        timer_settime(*pTimerID, 0, &its, NULL);
        return 0;
    }
    else
    {
    	timer_gettime(*pTimerID, &its);
    	return its.it_value.tv_sec;
    }
}


/**=============================================================================

    Function Name   : printUsage
    Description     : Function to display the usage of test frameowrk
    Arguments       : None
    Returns         : None

  ============================================================================*/

void printUsage(void)
{
    fprintf(stderr,
       "\n Usage: ./testFW <mode> <IP-address> <shared-object-filename>\n");

    fprintf(stderr, "\n mode                     ");
    fprintf(stderr, "\t\tIt can be Master/Slave/Standalone/--help/-h\n");

    fprintf(stderr, "\n IP-address               ");
    fprintf(stderr, "Master IP address and will be used ");
    fprintf(stderr, "only in case of slave mode\n");

    fprintf(stderr, "\n shared-object-filename   ");
    fprintf(stderr, "Name of the shared object which contains test cases\n");

    fprintf(stderr, "\n For example: ./testFW Slave 127.0.0.1 testcases\n");
    fprintf(stderr, "              ./testFW Master testcases\n");
    fprintf(stderr, "              ./testFW Standalone testcases\n\n");

    freeMemory(1);
}


/**=============================================================================

    Function Name   : printHeader
    Description     : Function to display the test framework header with the
                      current version.
    Arguments       : None
    Returns         : None

  ============================================================================*/

void printHeader(void)
{
    deviceDrvTstFWDebug(LOG_TO_USR, "\n");
    deviceDrvTstFWDebug(LOG_TO_USR,
        "*************************************************************\n\n");
    deviceDrvTstFWDebug(LOG_TO_USR,
        "                   A Generic Testing Framework                 \n");
    deviceDrvTstFWDebug(LOG_TO_USR,
        "                          Version 1.0                        \n\n");
    deviceDrvTstFWDebug(LOG_TO_USR,
        "*************************************************************\n\n");
}

/**=============================================================================

    Function Name   : freeMemory
    Description     : Function to free the memory and the resources.
    Arguments       :
                      Name            	Dir    	Description
                      @status  			In		Either 0(SUCCESS) or 1(FAILURE)

    Returns         : None

  ============================================================================*/

void freeMemory(bool status)
{
    sTestSuite_t *pSuite = NULL,*pTemp1 = NULL;
    sTestCase_t *pTestcase = NULL, *pTemp2 = NULL, *pTemp3 = NULL;

    deviceDrvTstFWDebug(LOG_TO_USR, "Closing test framework...\n");

    if(g_bDebugLogFlag)
    	deviceDrvTstFWDebug(LOG_TO_DBG, "\tClosing test framework... [%s:%d]\n",
    	__FILENAME__, __LINE__);

    /* Free memory allocated to test summary */
    if (g_pSummary != NULL)
    {
        free(g_pSummary);
    }

    /* Free the memory allocated to test suite and test cases */
    if(g_pSuiteHead != NULL)
    {
        HASH_ITER(hh, g_pSuiteHead, pSuite, pTemp1)
        {
             HASH_ITER(hh,pSuite->sTestCaseList, pTestcase, pTemp2)
             {
                 HASH_DEL(pSuite->sTestCaseList, pTestcase);
                 free(pTestcase);
             }

             HASH_DEL(g_pSuiteHead, pSuite);
             free(pSuite);
        }
    }
    switch (g_eMode)
    {
        case E_SLAVE:
            if (g_bSocketFlag == true)
            {
                /* Close the socket */
                close(g_nSlaveSockfd);
            }

            if(g_bTimerCreationFlag == true)
            {
                /* Delete the timer created for select() */
                timer_delete(g_timerId);
            }

            if(cpid > 0)
			{
				/* Kill child process */
				kill(cpid, SIGKILL);
				wait(NULL);
			}
        break;

        case E_MASTER:
            if (g_bSocketFlag == true)
            {
                /* Close the socket */
                close(g_nMasterSockfd);
            }

            if(g_bTimerCreationFlag == true)
            {
                /* Delete the timer created for select() */
                timer_delete(g_timerId);
            }

            if(cpid > 0)
			{
				/* Kill child process */
				kill(cpid, SIGKILL);
				wait(NULL);
			}
        break;

        case E_STANDALONE:
            if(g_bTimerCreationFlag == true)
            {
                /* Delete the timer created for select() */
                timer_delete(g_timerId);
            }

            if(cpid > 0)
			{
				/* Kill child process */
				kill(cpid, SIGKILL);
				wait(NULL);
			}
        break;
    }

    if(g_uObjectHandleCount == 1)
    {
        /* Close dynamically loaded shared object */
        dlclose(g_hObjectHandle);
    }
    if(g_bDebugLogFlag == true)
    {
    	fclose(g_pDebugLogFile);
    }
    if (g_bConsoleInitialized)
    {
        consoleExit();
    }

    exit(status);
}

/**=============================================================================
---------------------------- M A I N   F U N C T I O N -------------------------
==============================================================================*/

int main(int argc, char *argv[])
{
    char szConsole[10] = {0};
    int nTempConsolVar = 0;

    if (2 > argc){
        fprintf(stderr, "Invalid number of arguments\n");
        printUsage();
    }

    /* Get the mode from commandline argument and validate */
    if ((0 == strcmp(HELP, argv[1])) || (0 == strcmp("-h", argv[1])))
    {
        printUsage();
    }
    else if (0 == strcmp(SLAVE, argv[1]))
    {
        g_eMode = E_SLAVE;
    }
    else if (0 == strcmp(MASTER, argv[1]))
    {
        g_eMode = E_MASTER;
    }
    else if (0 == strcmp(STANDALONE, argv[1]))
    {
        g_eMode = E_STANDALONE;
    }
    else
    {
        fprintf(stderr, "Invalid mode, please enter correct mode\n");
        printUsage();
    }

    /* Initialize the test framework and do initial handshaking.
     */
    switch (g_eMode)
    {
        case E_SLAVE:
            if (argc < 4 || argc > 4)
            {
                fprintf(stderr, "Invalid number of arguments\n");
                printUsage();
                freeMemory(1);
            }

            /* IP Address should be specified only in case of slave.
             * It is the address of Master device which will be supporting for
             * Network driver testing
             */
            g_pszIPAddress = argv[2];

            /* Pointer to shared object */
            g_pszSharedObject = argv[3];

            /* Create ethernet socket */
            createSocket();

            /* Console Initialization */
            g_bConsoleInitialized = false;
			getTokenValue(TESTFW_CFG, "CONSOLE_NEEDED", szConsole);
			nTempConsolVar = atoi(szConsole);
			g_bWriteToConsole = (atoi(szConsole)==0)?false:true;

			if (g_bWriteToConsole)
			{
				consoleInit();
				g_bConsoleInitialized = true;
			}

			deviceDrvTstFWDebug(LOG_TO_DBG,
				"\tWrite to console %d,%s [%s:%d]\n",
				g_bWriteToConsole, TESTFW_CFG, __FILENAME__, __LINE__);

			deviceDrvTstFWDebug(LOG_TO_USR, "Console Settings by user is %s\n",
				(atoi(szConsole) == 0) ? "false" : "true");

            deviceDrvTstFWDebug(LOG_TO_USR,
            	"Device mode of operation: Slave\n");

            /* Slave Main Thread */
            initializeSlaveDevice();

            if(bGenerateReport == true)
            {
            	/* Generate Test Report */
            	generateXMLReport();
            }

        break;

        case E_MASTER:
            if (argc < 3 || argc > 3)
            {
                fprintf(stderr, "Invalid number of arguments\n");
                printUsage();
                freeMemory(1);
            }

            /* Only used in case of Slave device */
            g_pszIPAddress = NULL;

            /* Pointer to shared object */
            g_pszSharedObject = argv[2];

            /* Create ethernet socket */
            createSocket();

            /* Console Initialization */
            g_bConsoleInitialized = false;
			getTokenValue(TESTFW_CFG, "CONSOLE_NEEDED", szConsole);
			nTempConsolVar = atoi(szConsole);
			g_bWriteToConsole = (atoi(szConsole)==0)?false:true;

			if (g_bWriteToConsole)
			{
				consoleInit();
				g_bConsoleInitialized = true;
			}

			deviceDrvTstFWDebug(LOG_TO_DBG,
				"\tWrite to console %d,%s [%s:%d]\n",
				g_bWriteToConsole, TESTFW_CFG, __FILENAME__, __LINE__);

			deviceDrvTstFWDebug(LOG_TO_USR, "Console Settings by user is %s\n",
				(atoi(szConsole) == 0) ? "false" : "true");

            deviceDrvTstFWDebug(LOG_TO_USR,
            	"Device mode of operation: Master\n");

            /* Initialiaze master device */
            initializeMasterDevice();

        break;

        case E_STANDALONE:

        	if (argc < 3 || argc > 3)
            {
                fprintf(stderr,
                    "Invalid number of arguments\n");
                printUsage();
                freeMemory(1);
            }

            /* Pointer to shared object */
            g_pszSharedObject = argv[2];

            /* Console Initialization */
            g_bConsoleInitialized = false;
			getTokenValue(TESTFW_CFG, "CONSOLE_NEEDED", szConsole);
			nTempConsolVar = atoi(szConsole);
			g_bWriteToConsole = (atoi(szConsole)==0)?false:true;

			if (g_bWriteToConsole)
			{
				consoleInit();
				g_bConsoleInitialized = true;
			}

			deviceDrvTstFWDebug(LOG_TO_DBG,
				"\tWrite to console %d,%s [%s:%d]\n", g_bWriteToConsole,
				TESTFW_CFG, __FILENAME__, __LINE__);

			deviceDrvTstFWDebug(LOG_TO_USR, "Console Settings by user is %s\n",
				(atoi(szConsole) == 0) ? "false" : "true");

            deviceDrvTstFWDebug(LOG_TO_USR,
                "Device mode of operation: Standalone\n");

            /* Initialiaze device */
            initializeStandaloneDevice();

            if(bGenerateReport == true)
            {
            	/* Generate Test Report */
            	generateXMLReport();
            }

        break;
    }

    freeMemory(0);

    return 0;
}
