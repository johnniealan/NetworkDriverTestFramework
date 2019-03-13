/**=============================================================================
  $Workfile: slave.c$

  File Description: Test framework Slave mode of operation. This file
                    includes the functionalities required to operate the test
                    framework in slave mode.

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
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>
#include <signal.h>
#include <stdarg.h>

#include "deviceDrvTestFW.h"
#include "testStruct.h"
#include "console.h"

#define LOG_TO_USR 		1
#define LOG_TO_DBG  	2

#define RET_SUCCESS		0
#define RET_FAILURE	    -1

#define SIG SIGRTMIN

/*------------------- G L O B A L   D E C L A R A T I O N S ------------------*/

typedef enum{
    E_NONE = 1,
    E_MSG_HELLO_SENT,
    E_MSG_HELLO_ACK,
    E_MSG_FRAMEWORK_INIT_SENT,
    E_MSG_FRAMEWORK_INIT_ACK,
    E_FRAMEWORK_INITIALIZED,
    E_FRAMEWORK_INIT_FAILED,
    E_MSG_TST_SUITE_NAME_SENT,
    E_MSG_TST_SUITE_NAME_ACK,
    E_TST_SUITE_FOUND,
    E_MSG_TST_SUITE_LIST_COMPLETED_SENT,
    E_MSG_TST_SUITE_LIST_COMPLETED_ACK,
    E_MSG_TST_CASE_NAME_SENT,
    E_MSG_TST_CASE_NAME_ACK,
    E_TST_CASE_FOUND,
    E_MSG_TST_CASE_LIST_COMPLETED_SENT,
    E_MSG_TST_CASE_LIST_COMPLETED_ACK,
    E_MSG_TST_RUN_SENT,
    E_MSG_TST_RUN_ACK,
    E_TST_CASE_EXECUTE,
    E_TST_CASE_EXECUTED,
    E_MSG_STATUS_SENT,
    E_CLOSE_FRAMEWOERK,
    E_MSG_CLOSE_SENT,
    E_MSG_CLOSE_ACK
}E_SLAVE_STATE_MACHINE;

E_SLAVE_STATE_MACHINE eSlaveStateMachine = E_NONE;
E_SLAVE_STATE_MACHINE eCurrentStatus;

sTestCase_t *pTestcase;
sTestSuite_t *pSuite;

pthread_t g_slaveMainThread;
unsigned char szCurrentTestSuite[30];
unsigned char szCurrentTestCase[30];

extern pid_t cpid;
sigset_t mask;

int nCommChannel[2]; 				/* Socket Pair */
int nScreenCol = 0;

/*----------------- F U N C T I O N   D E C L A R A T I O N S ----------------*/

static void timerHandler(int sig, siginfo_t *si, void *uc);
static void childProcess(void);
void *pSlaveMainThread(void *pFd);
void defineSlaveStatus(E_TST_STATUS eReceivedStatus);
static void initialHandshake(void);
static void createTimer(void);


/*------------------ F U N C T I O N   D E F I N I T I O N S -----------------*/

/**=============================================================================

	Function Name   : timerHandler
    Description     : This is the signal handler. It is called when the timer
    				  expires and it also sends the SIGKILL to child process
    				  created in slave thread.
    Arguments       :
    				  Name            	Dir    	Description
                      @sig  			In		Signal number
                      @si				In		Signal information
                      @uc				In		Pointer to ucontext_t
    Returns         : None

  ============================================================================*/

static void timerHandler(int sig, siginfo_t *si, void *uc)
{
	kill(cpid, SIGUSR1);
}


/**=============================================================================

	Function Name   : initializeSlaveDevice
    Description     : This function is used to create slave thread and to wait
    				  on thread till it completes the execution.
    Arguments       : None
    Returns         : None

  ============================================================================*/

void initializeSlaveDevice(void)
{
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, nCommChannel) == -1)
	{
		deviceDrvTstFWDebug(LOG_TO_USR, "Error in creating socketpair\n");
		freeMemory(1);
	}

	/* Create a thread in parent process */
	pthread_create(&g_slaveMainThread, NULL, pSlaveMainThread, NULL);

	pthread_join(g_slaveMainThread, NULL);

}


/**=============================================================================

	Function Name   : childProcess
    Description     : This function is used to execute the testcases scheduled
    				  by the slave thread and send back the execution status to
    				  slave thread.
    Arguments       : None
    Returns         : None

  ============================================================================*/

static void childProcess(void)
{
    int nRet=0, nErr = 0;
    fd_set readset;
    sInterProcessMsg_t ToParent;

    E_TST_STATUS testStatus;

	/* Initialize time out struct for select() */
    struct timeval Tv;
    Tv.tv_sec = 0;
    Tv.tv_usec = 10000;

    while (1)
    {
		/* Initialize the set */
        FD_ZERO(&readset);
        FD_SET(nCommChannel[1], &readset);

        /* Now, check for readability */
        nErr = select(nCommChannel[1]+1, &readset, NULL, NULL, &Tv);
        if (nErr > 0 && FD_ISSET(nCommChannel[1], &readset))
        {
        	/* Clear flags */
            FD_CLR(nCommChannel[1], &readset);

            /* Do a simple read on data */
            memset(&ToParent,0,sizeof(ToParent));
            read(nCommChannel[1], &ToParent, sizeof(ToParent));

            switch (ToParent.nType)
			{
				/* Execute test case */
				case 0x01:
					pTestcase = ToParent.pTestcase;
					pTestcase->eStatus = pTestcase->fnPtrTestCase();
					memset(&ToParent,0,sizeof(ToParent));
					ToParent.nType = E_TST_CASE_EXECUTED;
					ToParent.eStatus = pTestcase->eStatus;
					send(nCommChannel[1], &ToParent, sizeof(ToParent), 0);
				break;
			}
		}
	}
}


/**=============================================================================

	Function Name   : pSlaveMainThread
    Description     : This is a thread function. It is used to traverse over the
    				  test suite and test case list and if the same test suite
    				  and test case is found on master machine, it will be
    				  scheduled to execute in child process.
    Arguments       : None
    Returns         : None

  ============================================================================*/

void *pSlaveMainThread(void *pFd)
{
    int nDataAvailable =0, nRet = 0, nErr = 0;
    bool bSend = false;
    fd_set readset;
	sInterProcessMsg_t ToChild;
	sHandshakeMsg_t Receive, Send;

	/* Initialize time out struct for select() */
    struct timeval Tv;
    Tv.tv_sec = 0;
    Tv.tv_usec = 10000;

	/* Send hello message and initialize test framework */
	initialHandshake();

	if (eSlaveStateMachine == E_FRAMEWORK_INITIALIZED)
	{
		createTimer();

		bGenerateReport = true;

		/* Create a child process */
		cpid = fork();
		if(cpid == 0)
		{
			childProcess();
		}
		else
		{
			while (1)
			{
				memset(&Receive, 0, sizeof(Receive));
				memset(&Send, 0, sizeof(Send));
				memset(&ToChild, 0, sizeof(ToChild));

				nDataAvailable = 0;
				bSend = false;

				/* Check if data available */
				nDataAvailable = peek();
				if (nDataAvailable == -1)
				{
					deviceDrvTstFWDebug(LOG_TO_USR, "Socket closed\n");
					freeMemory(1);
				}

				/* Receive data if avaialble */
				if ((nDataAvailable > 0) &&
					(eSlaveStateMachine != E_MSG_CLOSE_ACK))
				{
					nRet = receiveMessage(&Receive);

					/* if socket is closed abruptly */
					if(nRet == 0)
					{
						freeMemory(1);
					}
				}

				switch (eSlaveStateMachine)
				{
					case E_MSG_TST_SUITE_NAME_ACK:
					case E_MSG_TST_CASE_NAME_ACK:
					case E_TST_CASE_EXECUTED:
						deviceDrvTstFWDebug(LOG_TO_DBG,
							"\tSend >> E_MSG_STATUS [%s:%d]\n",
							__FILENAME__, __LINE__);
						Send.eMsgType = E_MSG_STATUS;
						eCurrentStatus = eSlaveStateMachine;
						eSlaveStateMachine = E_MSG_STATUS_SENT;
						bSend = true;
					break;

					case E_MSG_STATUS_SENT:
						defineSlaveStatus(Receive.Msg.eStatus);
					break;

					case E_FRAMEWORK_INITIALIZED:
						if (pSuite != NULL)
						{
							deviceDrvTstFWDebug(LOG_TO_DBG,
								"\tSend >> Test suite name: %s [%s:%d]\n",
								pSuite->szName, __FILENAME__, __LINE__);
							Send.eMsgType = E_MSG_TST_SUITE_NAME;
							strcpy(Send.Msg.szName, pSuite->szName);
							strcpy(szCurrentTestSuite, pSuite->szName);
							pTestcase = pSuite->sTestCaseList;
							eSlaveStateMachine = E_MSG_TST_SUITE_NAME_SENT;
							bSend = true;
						}
						else
						{
							deviceDrvTstFWDebug(LOG_TO_DBG,
							"\tSend >> E_MSG_TST_SUITE_LIST_COMPLETED [%s:%d]\n",
								__FILENAME__, __LINE__);
							Send.eMsgType = E_MSG_TST_SUITE_LIST_COMPLETED;
							eSlaveStateMachine =
								E_MSG_TST_SUITE_LIST_COMPLETED_SENT;
							bSend = true;
						}
					break;

					case E_TST_SUITE_FOUND:
						if (pTestcase != NULL)
						{
							deviceDrvTstFWDebug(LOG_TO_DBG,
								"\tSend >> Test case name: %s [%s:%d]\n",
								pTestcase->szName, __FILENAME__, __LINE__);
							Send.eMsgType = E_MSG_TST_CASE_NAME;
							strcpy(Send.Msg.szName, pTestcase->szName);
							strcpy(szCurrentTestCase, pTestcase->szName);

							if (g_bConsoleInitialized)
							nScreenCol =
                            	fnAddTestNameToScreen(pTestcase->szName, 0,
                            		TST_RUNNING);

							eSlaveStateMachine = E_MSG_TST_CASE_NAME_SENT;
							bSend = true;
						}
						else
						{
							deviceDrvTstFWDebug(LOG_TO_DBG,
							"\tSend >> E_MSG_TST_CASE_LIST_COMPLETED [%s:%d]\n",
								__FILENAME__, __LINE__);
							Send.eMsgType = E_MSG_TST_CASE_LIST_COMPLETED;
							eSlaveStateMachine =
								E_MSG_TST_CASE_LIST_COMPLETED_SENT;
							pSuite = pSuite->hh.next;
							bSend = true;
						}
					break;

					case E_TST_CASE_FOUND:
						deviceDrvTstFWDebug(LOG_TO_DBG,
							"\tSend >> E_MSG_TST_RUN [%s:%d]\n",
							__FILENAME__, __LINE__);
						Send.eMsgType = E_MSG_TST_RUN;
						eSlaveStateMachine = E_MSG_TST_RUN_SENT;
						bSend = true;
					break;

					case E_MSG_TST_RUN_ACK:
						ToChild.nType = 0x01;
						ToChild.pTestcase = pTestcase;
						eSlaveStateMachine = E_TST_CASE_EXECUTE;

						send(nCommChannel[0], &ToChild,
							sizeof(ToChild), 0);
						checkTimer(&g_timerId, pTestcase->uTimeout, true);

						deviceDrvTstFWDebug(LOG_TO_DBG,
							"\tTest case is running... [%s:%d]\n",
							__FILENAME__, __LINE__);

					break;

					case E_MSG_TST_SUITE_NAME_SENT:
					case E_MSG_TST_CASE_NAME_SENT:
					case E_MSG_TST_SUITE_LIST_COMPLETED_SENT:
					case E_MSG_TST_CASE_LIST_COMPLETED_SENT:
					case E_MSG_TST_RUN_SENT:
					case E_MSG_CLOSE_SENT:
						if (Receive.eMsgType == E_MSG_ACK)
						{
							deviceDrvTstFWDebug(LOG_TO_DBG,
								"\tReceived << E_MSG_ACK [%s:%d]\n",
								__FILENAME__, __LINE__);
							if (eSlaveStateMachine == E_MSG_TST_SUITE_NAME_SENT)
							{
								eSlaveStateMachine = E_MSG_TST_SUITE_NAME_ACK;
							}
							else if (eSlaveStateMachine ==
								E_MSG_TST_CASE_NAME_SENT)
							{
								eSlaveStateMachine = E_MSG_TST_CASE_NAME_ACK;
							}
							else if (eSlaveStateMachine == E_MSG_TST_RUN_SENT)
							{
								eSlaveStateMachine = E_MSG_TST_RUN_ACK;
							}
							else if (eSlaveStateMachine ==
								E_MSG_TST_CASE_LIST_COMPLETED_SENT)
							{
								eSlaveStateMachine = E_FRAMEWORK_INITIALIZED;
							}
							else if (eSlaveStateMachine ==
								E_MSG_TST_SUITE_LIST_COMPLETED_SENT)
							{
								eSlaveStateMachine =
									E_MSG_TST_SUITE_LIST_COMPLETED_ACK;
							}
							else if (eSlaveStateMachine == E_MSG_CLOSE_SENT)
							{
								eSlaveStateMachine = E_MSG_CLOSE_ACK;
							}
						}
						else if(Receive.eMsgType == E_MSG_NACK)
						{
							deviceDrvTstFWDebug(LOG_TO_DBG,
								"\tReceived << E_MSG_NACK [%s:%d]\n",
								__FILENAME__, __LINE__);
							eSlaveStateMachine = E_FRAMEWORK_INIT_FAILED;
						}
					break;

					case E_MSG_TST_SUITE_LIST_COMPLETED_ACK:
						eSlaveStateMachine = E_CLOSE_FRAMEWOERK;
					break;

					case E_FRAMEWORK_INIT_FAILED:
					case E_CLOSE_FRAMEWOERK:
						deviceDrvTstFWDebug(LOG_TO_DBG,
							"\tSend >> E_MSG_CLOSE [%s:%d]\n",
							__FILENAME__, __LINE__);
						Send.eMsgType = E_MSG_CLOSE;
						eSlaveStateMachine = E_MSG_CLOSE_SENT;
						bSend = true;
					break;
				}
				if(eSlaveStateMachine == E_MSG_CLOSE_ACK)
				{
					break;
				}
				if (bSend)
				{
					sendMessage(&Send);
				}

				/* Initialize the set */
				FD_ZERO(&readset);
				FD_SET(nCommChannel[0], &readset);

				/* Now, check for readability */
				nErr = select(nCommChannel[0]+1, &readset, NULL, NULL, &Tv);
				if (nErr > 0 && FD_ISSET(nCommChannel[0], &readset))
				{
					/* Clear flags */
					FD_CLR(nCommChannel[0], &readset);

					memset(&ToChild, 0, sizeof(ToChild));

					/* Do a simple read on data */
					read(nCommChannel[0], &ToChild,
						sizeof(ToChild));

					eSlaveStateMachine = ToChild.nType;
					pTestcase->eStatus = ToChild.eStatus;
				}
			}
		}
	}
}


/**=============================================================================

    Function Name   : defineSlaveStatus
    Description     : Function to updates the machine state and status of
    				  of execution based on the response received from the
    				  master machine.
    Arguments       :
                      Name                  Dir     Description
                      @eReceivedStatus      In      Status received from master

    Returns         : None

  ============================================================================*/

void defineSlaveStatus(E_TST_STATUS eReceivedStatus)
{
	sHandshakeMsg_t Receive, Send;
	memset(&Send,0,sizeof(Send));

	if (eReceivedStatus == E_RUNNING)
	{
		deviceDrvTstFWDebug(LOG_TO_DBG,
			"\tReceived << E_RUNNING [%s:%d]\n",
			__FILENAME__, __LINE__);
		deviceDrvTstFWDebug(LOG_TO_DBG,
			"\tSend >> E_MSG_STATUS [%s:%d]\n",
			__FILENAME__, __LINE__);
		Send.eMsgType = E_MSG_STATUS;
		sendMessage(&Send);
	}
    /* If test Suite found in master device */
    if ((eReceivedStatus == E_FOUND) &&
    	(eCurrentStatus == E_MSG_TST_SUITE_NAME_ACK))
    {
        deviceDrvTstFWDebug(LOG_TO_DBG,
        	"\tTest suite %s found in master [%s:%d]\n",
            szCurrentTestSuite, __FILENAME__, __LINE__);
        pSuite->eStatus = E_FOUND;
        eSlaveStateMachine = E_TST_SUITE_FOUND;
    }
    /* If test Suite not found in master device */
    else if ((eReceivedStatus == E_NOT_FOUND) &&
		(eCurrentStatus == E_MSG_TST_SUITE_NAME_ACK))
    {
        g_pSummary->uNumberOfSuitesNotFound++;
        deviceDrvTstFWDebug(LOG_TO_DBG,
        	"\tTest suite %s not found in master [%s:%d]\n",
            szCurrentTestSuite, __FILENAME__, __LINE__);
        pSuite->eStatus = E_NOT_FOUND;
        pSuite = pSuite->hh.next;
        eSlaveStateMachine = E_FRAMEWORK_INITIALIZED;
    }
    /* If test case found in master device */
    else if ((eReceivedStatus == E_FOUND) &&
    	(eCurrentStatus == E_MSG_TST_CASE_NAME_ACK))
    {
        deviceDrvTstFWDebug(LOG_TO_DBG,
        	"\tTest case %s found in master [%s:%d]\n",
            szCurrentTestCase, __FILENAME__, __LINE__);
        pTestcase->eStatus = E_FOUND;
        eSlaveStateMachine = E_TST_CASE_FOUND;
    }
    /* If test case not found in master */
    else if ((eReceivedStatus == E_NOT_FOUND) &&
    	(eCurrentStatus == E_MSG_TST_CASE_NAME_ACK))
    {
        g_pSummary->uNumberOfTestsNotFound++;

        deviceDrvTstFWDebug(LOG_TO_DBG,
        	"\tTest case %s not found in master [%s:%d]\n",
            szCurrentTestCase, __FILENAME__, __LINE__);

        if (g_bConsoleInitialized)
        	fnUpdateTstStatusToScreen(nScreenCol, 0, TST_ERROR);

        pTestcase->eStatus = E_NOT_FOUND;
        pTestcase = pTestcase->hh.next;
        eSlaveStateMachine = E_TST_SUITE_FOUND;
    }
    /* If test case failed either in master/slave */
    else if (((eReceivedStatus == E_FAILED) || (pTestcase->eStatus == E_FAILED))
    	&& (eCurrentStatus == E_TST_CASE_EXECUTED))
    {
        deviceDrvTstFWDebug(LOG_TO_DBG,"\tTest case %s failed [%s:%d]\n",
            szCurrentTestCase, __FILENAME__, __LINE__);
        pTestcase->dElapsedTime = (pTestcase->uTimeout/1000) -
			checkTimer(&g_timerId, pTestcase->uTimeout, false);

		if (g_bConsoleInitialized)
			fnUpdateTstStatusToScreen(nScreenCol,
				pTestcase->dElapsedTime, TST_FAILED);

        pTestcase->eStatus = E_FAILED;
        g_pSummary->uNumberOfTestsFailed++;
        pTestcase = pTestcase->hh.next;
        eSlaveStateMachine = E_TST_SUITE_FOUND;
    }
    /* If test case timedout either in master/slave */
    else if (((eReceivedStatus == E_TIMEOUT) ||
    	(pTestcase->eStatus == E_TIMEOUT)) &&
    	(eCurrentStatus == E_TST_CASE_EXECUTED))
    {
        deviceDrvTstFWDebug(LOG_TO_DBG,"\tTest case %s timeout [%s:%d]\n",
            szCurrentTestCase, __FILENAME__, __LINE__);

        if (g_bConsoleInitialized)
			fnUpdateTstStatusToScreen(nScreenCol, 0, TST_TIMEOUT);

        pTestcase->eStatus = E_TIMEOUT;
        g_pSummary->uNumberOfTestsTimeout++;
        pTestcase = pTestcase->hh.next;
        eSlaveStateMachine = E_TST_SUITE_FOUND;
    }
    /* If test case is passed on master as well as slave */
    else if (((eReceivedStatus == E_PASSED) && (pTestcase->eStatus == E_PASSED))
    	&& (eCurrentStatus == E_TST_CASE_EXECUTED))
    {

        deviceDrvTstFWDebug(LOG_TO_DBG,"\tTest case %s passed [%s:%d]\n",
            szCurrentTestCase, __FILENAME__, __LINE__);
        pTestcase->dElapsedTime = (pTestcase->uTimeout/1000) -
			checkTimer(&g_timerId, pTestcase->uTimeout, false);

		if (g_bConsoleInitialized)
			fnUpdateTstStatusToScreen(nScreenCol,
				pTestcase->dElapsedTime, TST_PASSED);

        pTestcase->eStatus = E_PASSED;
        g_pSummary->uNumberOfTestsPassed++;
        pTestcase = pTestcase->hh.next;
        eSlaveStateMachine = E_TST_SUITE_FOUND;
    }
}


/**=============================================================================

	Function Name   : initialHandshake
    Description     : This function is used to do initial handshaking with
    				  master machine and also to initialize the test framework
    				  (loading shared object, adding test suites and test cases
    				  to list, allocating memory to store test summary).
    Arguments       : None
    Returns         : None

  ============================================================================*/

static void initialHandshake(void)
{
	int nDataAvailable =0, nRet = 0;
	sHandshakeMsg_t Receive, Send;

	while(1)
	{
		memset(&Receive, 0, sizeof(Receive));
		memset(&Send, 0, sizeof(Send));

		nDataAvailable = 0;

		/* Check if data available */
		nDataAvailable = peek();
		if (nDataAvailable == -1)
		{
			deviceDrvTstFWDebug(LOG_TO_USR, "Socket closed\n");
			freeMemory(1);
		}

		/* Receive data if avaialble */
		if ((nDataAvailable > 0) && (eSlaveStateMachine != E_MSG_CLOSE_ACK))
		{
			receiveMessage(&Receive);
		}

		switch (eSlaveStateMachine)
		{
			case E_NONE:
				deviceDrvTstFWDebug(LOG_TO_DBG,
					"\tSend >> E_MSG_HELLO [%s:%d]\n", __FILENAME__, __LINE__);
				Send.eMsgType = E_MSG_HELLO;
				eSlaveStateMachine = E_MSG_HELLO_SENT;
				sendMessage(&Send);
			break;

			case E_MSG_HELLO_ACK:
		        deviceDrvTstFWDebug(LOG_TO_DBG,
		        	"\tSend >> E_MSG_FRAMEWORK_INIT [%s:%d]\n",
		        	__FILENAME__, __LINE__);
		        Send.eMsgType = E_MSG_FRAMEWORK_INIT;
		        eSlaveStateMachine = E_MSG_FRAMEWORK_INIT_SENT;
		        sendMessage(&Send);
            break;

        	case E_MSG_FRAMEWORK_INIT_ACK:
        		deviceDrvTstFWDebug(LOG_TO_DBG,
        			"\tSend >> E_MSG_STATUS [%s:%d]\n", __FILENAME__, __LINE__);
                Send.eMsgType = E_MSG_STATUS;
                eCurrentStatus = eSlaveStateMachine;
                eSlaveStateMachine = E_MSG_STATUS_SENT;
                sendMessage(&Send);
			break;

			case E_MSG_STATUS_SENT:
                if (Receive.Msg.eStatus == E_RUNNING)
				{
					deviceDrvTstFWDebug(LOG_TO_DBG,
						"\tReceived << E_RUNNING [%s:%d]\n",
						__FILENAME__, __LINE__);
					deviceDrvTstFWDebug(LOG_TO_DBG,
						"\tSend >> E_MSG_STATUS [%s:%d]\n",
						__FILENAME__, __LINE__);
					Send.eMsgType = E_MSG_STATUS;
					sendMessage(&Send);
				}
				/* If test framework is initialized in master device */
				else if (Receive.Msg.eStatus == E_PASSED &&
					eCurrentStatus == E_MSG_FRAMEWORK_INIT_ACK)
				{
					deviceDrvTstFWDebug(LOG_TO_DBG,
						"\tReceived << E_PASSED [%s:%d]\n",
						__FILENAME__, __LINE__);
					eSlaveStateMachine = E_FRAMEWORK_INITIALIZED;
				}
				/* If test framework is not initialized in master device */
				else if (Receive.Msg.eStatus == E_FAILED &&
					eCurrentStatus == E_MSG_FRAMEWORK_INIT_ACK)
				{
					deviceDrvTstFWDebug(LOG_TO_DBG,
						"\tReceived << E_FAILED [%s:%d]\n",
						__FILENAME__, __LINE__);
					deviceDrvTstFWDebug(LOG_TO_USR,
						"Test framework initialization failed in master\n");
					eSlaveStateMachine = E_FRAMEWORK_INIT_FAILED;
				}
            break;

			case E_MSG_HELLO_SENT:
			case E_MSG_FRAMEWORK_INIT_SENT:
			case E_MSG_CLOSE_SENT:
				if (Receive.eMsgType == E_MSG_ACK)
				{
					deviceDrvTstFWDebug(LOG_TO_DBG,
						"\tReceived << E_MSG_ACK [%s:%d]\n",
						__FILENAME__, __LINE__);
					if (eSlaveStateMachine == E_MSG_HELLO_SENT)
                    {
                        eSlaveStateMachine = E_MSG_HELLO_ACK;
                    }
                    else if (eSlaveStateMachine == E_MSG_FRAMEWORK_INIT_SENT)
                    {
                        nRet = initializeTestFramework();
                        if (nRet == 0)
                        {
                            eSlaveStateMachine = E_MSG_FRAMEWORK_INIT_ACK;
                            pSuite = g_pSuiteHead;
                        }
                        else
                        {
                            eSlaveStateMachine = E_FRAMEWORK_INIT_FAILED;
                            deviceDrvTstFWDebug(LOG_TO_USR,
							 "\tTest framework initialization failed [%s:%d]\n",
								__FILENAME__, __LINE__);
                        }
                    }
                    else if (eSlaveStateMachine == E_MSG_CLOSE_SENT)
                    {
                        eSlaveStateMachine = E_MSG_CLOSE_ACK;
                    }
				}
				else if(Receive.eMsgType == E_MSG_NACK)
				{
					deviceDrvTstFWDebug(LOG_TO_DBG,
						"\tReceived << E_MSG_NACK [%s:%d]\n",
						__FILENAME__, __LINE__);
					eSlaveStateMachine = E_CLOSE_FRAMEWOERK;
				}
			break;

			case E_FRAMEWORK_INIT_FAILED:
			case E_CLOSE_FRAMEWOERK:
				deviceDrvTstFWDebug(LOG_TO_DBG,
					"\tSend >> E_MSG_CLOSE [%s:%d]\n", __FILENAME__, __LINE__);
                Send.eMsgType = E_MSG_CLOSE;
                eSlaveStateMachine = E_MSG_CLOSE_SENT;
                sendMessage(&Send);
			break;
		}
		if ((eSlaveStateMachine == E_FRAMEWORK_INITIALIZED) ||
			(eSlaveStateMachine == E_MSG_CLOSE_ACK))
		{
			break;
		}
	}
}

/**=============================================================================

	Function Name   : createTimer
    Description     : This function is used to create a realtime timer and also
    				  to register the realtime signal, which will be called when
    				  timer expires.
    Arguments       : None
    Returns         : None

  ============================================================================*/

static void createTimer(void)
{
	struct sigevent sev;
	struct sigaction sa;

	memset(&sev, 0, sizeof(struct sigevent));
	memset(&sa, 0, sizeof(struct sigaction));

	/* Establish handler for timer signal */
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = timerHandler;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIG, &sa, NULL) == -1)
	{
		deviceDrvTstFWDebug(LOG_TO_USR, "Error in registering signal\n");
		freeMemory(1);
	}

    /* Create the timer */
    sev.sigev_notify = SIGEV_SIGNAL;
	sev.sigev_signo = SIG;
	sev.sigev_value.sival_ptr = &g_timerId;
	if(timer_create(CLOCK_REALTIME, &sev, &g_timerId) == -1)
	{
		deviceDrvTstFWDebug(LOG_TO_USR, "Error in creating timer\n");
		freeMemory(1);
	}

	g_bTimerCreationFlag = true;
}


