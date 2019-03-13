/**=============================================================================
  $Workfile: deviceDrvTestFW.h$

  File Description: This file includes common data structures, enums, function
                    declararions and global variable declarations required by
                    the test framework

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

#ifndef  __DEVICE_DRV_TEST_FW_H__
#define  __DEVICE_DRV_TEST_FW_H__

#include "testStruct.h"
#include "uthash.h"
#include "console.h"

/*------------------------ D E F I N E S   S E C T I O N ---------------------*/

#define PACKAGE tframework

#define NO_MEMORY       NULL
#define HELP            "--help"
#define SLAVE           "Slave"
#define MASTER          "Master"
#define STANDALONE      "Standalone"
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

/*----------------- D A T A  S T R U C T U R E S  &  E N U M S ---------------*/

/* Master-Slave communication messages */
typedef enum
{
    E_MSG_HELLO = 1,
    E_MSG_FRAMEWORK_INIT,
    E_MSG_TST_SUITE_NAME,
    E_MSG_TST_CASE_NAME,
    E_MSG_TST_SUITE_LIST_COMPLETED,
    E_MSG_TST_CASE_LIST_COMPLETED,
    E_MSG_TST_RUN,
    E_MSG_ACK,
    E_MSG_NACK,
    E_MSG_STATUS,
    E_MSG_CLOSE
}E_MSG_TYPE;

typedef enum
{
    E_PASSED = 1,
    E_FAILED,
    E_TIMEOUT,
    E_RUNNING,
    E_ERROR,
    E_NOT_FOUND,
    E_FOUND
}E_TST_STATUS;

typedef enum
{
    E_SLAVE = 1,
    E_MASTER,
    E_STANDALONE
}E_MODE_TYPE;


/* Timeout flag function pointer*/
typedef E_TST_STATUS (*pTimeoutFlagFuncPtr)(bool);
pTimeoutFlagFuncPtr fnPtrSetTimeoutFlag;

/* Test case function pointer*/
typedef E_TST_STATUS (*pTestCaseFuncPtr)(void);

/* Test case cleanup function pointer */
typedef E_TST_STATUS (*pCleanupFuncPtr)(void);

/* Test case data structure */
typedef struct framework_test_case
{
    int nTestCaseIden;
    unsigned char szName[30];               /* Test case name  - key */
    unsigned int uTimeout;                  /* Timeout in seconds */
    pTestCaseFuncPtr fnPtrTestCase;         /* Test case function pointer */
    E_TST_STATUS eStatus;                   /* Test case status */
    double dElapsedTime;                    /* Test case execution time */
    UT_hash_handle hh;						/* Hash table handle */
}sTestCase_t;

/* Test suite data structure */
typedef struct framework_test_suite
{
    unsigned char szName[30];               /* Test suite name */
    E_TST_STATUS eStatus;                   /* Test suite status */
    pCleanupFuncPtr fnPtrCleanup;           /* Test suite cleanup function ptr*/
    UT_hash_handle hh;						/* Hash table handle */
    sTestCase_t *sTestCaseList;				/* Pointer to test case list */
}sTestSuite_t;

/* Test summary data structure */
typedef struct framework_test_summary
{
    /* Total number of test cases */
    unsigned int uNumberOfTests;

    /* Total number of test suites */
    unsigned int uNumberOfSuites;

    /* Total number of test cases passed */
    unsigned int uNumberOfTestsPassed;

    /* Total number of test cases failed */
    unsigned int uNumberOfTestsFailed;

    /* Total number of test cases timeout */
    unsigned int uNumberOfTestsTimeout;

    /* Total number of test cases not found */
    unsigned int uNumberOfTestsNotFound;

    /* Total number of test suites not found */
    unsigned int uNumberOfSuitesNotFound;
}sTestSummary_t;

/* Eth0 handshake message struct */
typedef struct handshake_msg
{
    unsigned int uSlaveId;              /* Slave Id */
    E_MSG_TYPE eMsgType;                /* Message Type */
	union{
        unsigned char szName[30];  		/* Test case or Test suite name */
        E_TST_STATUS eStatus;			/* Status of the command */
    }Msg;
}sHandshakeMsg_t;

/* Inter-process communication msg struct */
typedef struct inter_process_msg
{
       int nType;						/* Type */
       char szBuffer[30];				/* Test case name */
       sTestCase_t *pTestcase;			/* Pointer to test case */
       E_TST_STATUS eStatus;			/* Status of test case execution */

}sInterProcessMsg_t;

/*------------------ G L O B A L   D E C L A R A T I O N S -------------------*/

bool g_bWriteToConsole;

bool g_bConsoleInitialized;

bool g_bTimerCreationFlag;

sTestSummary_t *g_pSummary;	            /* Pointer to summary */
sTestSuite_t *g_pSuiteHead;             /* Pointer to test suite */

timer_t g_timerId;                      /* Timer Id */

bool bGenerateReport;


/*------------------- F U N C T I O N   P R O T O T Y P E --------------------*/

void deviceDrvTstFWDebug(int ntype, char *pszMsg, ...);
int getTokenValue(char *pszFile, const char *pszToken, unsigned char *pszValue);
void createSocket(void);
int loadSharedObject(void);
int initializeTestSummary(void);
int addSuitesAndCases(void);
int initializeTestFramework(void);

int constructFrameworkMsg(sHandshakeMsg_t *pMsg, unsigned char szOutBuffer[]);
void parseFrameworkMsg(sHandshakeMsg_t *pMsg, unsigned char szInBuffer[]);
unsigned short int calculateChecksum(unsigned char *pszBuffer,
    unsigned int uLength);
void sendMessage(sHandshakeMsg_t *pMsg);
int receiveMessage(sHandshakeMsg_t *pMsg);

void generateFileName(void);
void generateXMLReport(void);

int peek(void);
uint64_t checkTimer(timer_t *pTimerID, unsigned int nMsec, bool bSet);

void printUsage(void);
void printHeader(void);

void freeMemory(bool status);

void initializeSlaveDevice(void);
void initializeMasterDevice(void);
void initializeStandaloneDevice(void);


#endif //__DEVICE_DRV_TEST_FW_H__
