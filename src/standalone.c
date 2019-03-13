/**=============================================================================
  $Workfile: standalone.c$

  File Description: Test framework Standalone mode of operation. This file
                    includes the functionalities required to operate the test
                    framework in Standlone mode.

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
#include <pthread.h>
#include <time.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>

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
    E_FRAMEWORK_INITIALIZED,
    E_TST_SUITE_FOUND,
    E_TST_SUITE_LIST_COMPLETED,
    E_TST_CASE_FOUND,
    E_TST_CASE_LIST_COMPLETED,
    E_TST_CASE_EXECUTE,
    E_TST_CASE_EXECUTED,
    E_GENERATE_XML_REPORT,
    E_CLOSE_FRAMEWOERK
}E_STANDALONE_MACHINE_STATE;

E_STANDALONE_MACHINE_STATE eStandaloneStateMachine = E_NONE;
E_STANDALONE_MACHINE_STATE eCurrentStatus;

sTestCase_t *pTestcase;
sTestSuite_t *pSuite;

pthread_t g_standaloneMainThread, g_standaloneWorkerThread;

unsigned char szCurrentTestSuite[30];
unsigned char szCurrentTestCase[30];

pid_t cpid;
sigset_t mask;

int nCommChannel[2];					/* Socket Pair */

/*----------------- F U N C T I O N   D E C L A R A T I O N S ----------------*/

void *pStandaloneMainThread(void *pFd);
void childProcess(void);
static void createTimer(void);

/*------------------ F U N C T I O N   D E F I N I T I O N S -----------------*/


/**=============================================================================

	Function Name   : timerHandler
    Description     : This is the signal handler. It is called when the timer
    				  expires and it also sends the SIGKILL to child process
    				  created in standalone thread.
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
    Description     : This function is used to create standalone thread and to
				      wait on thread till it completes the execution.
    Arguments       : None
    Returns         : None

  ============================================================================*/

void initializeStandaloneDevice(void)
{
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, nCommChannel) == -1)
	{
		deviceDrvTstFWDebug(LOG_TO_USR,"Error in creating socketpair\n");
		freeMemory(1);
	}

	/* Create a thread in parent process */
	pthread_create(&g_standaloneMainThread, NULL, pStandaloneMainThread, NULL);

	pthread_join(g_standaloneMainThread, NULL);
}


/**=============================================================================

	Function Name   : childProcess
    Description     : This function is used to execute the testcases scheduled
    				  by the standalone thread and send back the execution
    				  status to standalone thread.
    Arguments       : None
    Returns         : None

  ============================================================================*/

void childProcess(void)
{
    int nScreenCol = 0, nRet=0, nErr = 0;
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

    Function Name   : pStandaloneMainThread
    Description     : This is a thread function. It is used to traverse over the
    				  test suite and test case list and scheduled the testcase
    				  to execute in child process.
    Arguments       : None
    Returns         : None

  ============================================================================*/

void *pStandaloneMainThread(void *pFd)
{
    int nScreenCol = 0, nRet=0, nErr = 0;
	fd_set readset;
    sInterProcessMsg_t ToChild;

    /* Initialize time out struct for select() */
    struct timeval Tv;
    Tv.tv_sec = 0;
    Tv.tv_usec = 10000;

    /* Initialize test framework */
    nRet = initializeTestFramework();
	if (nRet == 0)
	{
		eStandaloneStateMachine = E_FRAMEWORK_INITIALIZED;
		pSuite = g_pSuiteHead;

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
			/* Parent process */
			while(1)
			{
				memset(&ToChild, 0, sizeof(ToChild));

				switch (eStandaloneStateMachine)
				{
					case E_FRAMEWORK_INITIALIZED:
						if (pSuite != NULL)
						{
							deviceDrvTstFWDebug(LOG_TO_DBG,
								"\tTest suite name: %s [%s:%d]\n",
								pSuite->szName, __FILENAME__, __LINE__);
							strcpy(szCurrentTestSuite, pSuite->szName);
							pTestcase = pSuite->sTestCaseList;
							eStandaloneStateMachine = E_TST_SUITE_FOUND;
							pSuite->eStatus = E_FOUND;
						}
						else
						{
							deviceDrvTstFWDebug(LOG_TO_DBG,
								"\tE_TST_SUITE_LIST_COMPLETED [%s:%d]\n",
								__FILENAME__, __LINE__);
							eStandaloneStateMachine =
								E_TST_SUITE_LIST_COMPLETED;
						}
					break;

					case E_TST_SUITE_FOUND:
						if (pTestcase!= NULL)
						{
							deviceDrvTstFWDebug(LOG_TO_DBG,
								"\tTest case name: %s [%s:%d]\n",
								pTestcase->szName, __FILENAME__, __LINE__);
							strcpy(szCurrentTestCase, pTestcase->szName);
							eStandaloneStateMachine = E_TST_CASE_FOUND;
						}
						else
						{
							deviceDrvTstFWDebug(LOG_TO_DBG,
								"\tE_TST_CASE_LIST_COMPLETED [%s:%d]\n",
								__FILENAME__, __LINE__);
							eStandaloneStateMachine = E_TST_CASE_LIST_COMPLETED;
							pSuite = pSuite->hh.next;
						}
					break;


					case E_TST_CASE_FOUND:
						ToChild.nType = 0x01;
						ToChild.pTestcase = pTestcase;
						eStandaloneStateMachine = E_TST_CASE_EXECUTE;

						send(nCommChannel[0], &ToChild,
							sizeof(ToChild), 0);
						checkTimer(&g_timerId, pTestcase->uTimeout, true);

						if (g_bConsoleInitialized)
							nScreenCol =
                            fnAddTestNameToScreen(pTestcase->szName, 0,
                            	TST_RUNNING);

						deviceDrvTstFWDebug(LOG_TO_DBG,
							"\tTest case is running... [%s:%d]\n",
							__FILENAME__, __LINE__);
					break;


					case E_TST_CASE_EXECUTED:
						pTestcase->dElapsedTime = (pTestcase->uTimeout/1000) -
							checkTimer(&g_timerId, pTestcase->uTimeout, false);

                        /* If test case failed */
						if (pTestcase->eStatus == E_FAILED)
						{
							deviceDrvTstFWDebug(LOG_TO_DBG,
								"\tTest case %s failed [%s:%d]\n",
								szCurrentTestCase, __FILENAME__, __LINE__);

							if (g_bConsoleInitialized)
								fnUpdateTstStatusToScreen(nScreenCol,
									pTestcase->dElapsedTime, TST_FAILED);

							g_pSummary->uNumberOfTestsFailed++;
							pTestcase = pTestcase->hh.next;
							eStandaloneStateMachine = E_TST_SUITE_FOUND;
						}
						/* If test case timedout */
						else if (pTestcase->eStatus == E_TIMEOUT)
						{
							deviceDrvTstFWDebug(LOG_TO_DBG,
								"\tTest case %s timeout [%s:%d]\n",
								szCurrentTestCase, __FILENAME__, __LINE__);

							if (g_bConsoleInitialized)
								fnUpdateTstStatusToScreen(nScreenCol, 0,
									TST_TIMEOUT);

							g_pSummary->uNumberOfTestsTimeout++;
							pTestcase = pTestcase->hh.next;
							eStandaloneStateMachine = E_TST_SUITE_FOUND;
						}
						/* If test case is passed */
						else if (pTestcase->eStatus == E_PASSED)
						{
							deviceDrvTstFWDebug(LOG_TO_DBG,
								"\tTest case %s passed [%s:%d]\n",
								szCurrentTestCase, __FILENAME__, __LINE__);

							if (g_bConsoleInitialized)
								fnUpdateTstStatusToScreen(nScreenCol,
									pTestcase->dElapsedTime, TST_PASSED);

							g_pSummary->uNumberOfTestsPassed++;
							pTestcase = pTestcase->hh.next;
							eStandaloneStateMachine = E_TST_SUITE_FOUND;
						}
					break;

					case E_TST_CASE_LIST_COMPLETED:
						eStandaloneStateMachine = E_FRAMEWORK_INITIALIZED;
					break;

					case E_TST_SUITE_LIST_COMPLETED:
						eStandaloneStateMachine = E_GENERATE_XML_REPORT;
					break;
				}

				if (eStandaloneStateMachine == E_GENERATE_XML_REPORT)
				{
					break;
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

					eStandaloneStateMachine = ToChild.nType;
					pTestcase->eStatus = ToChild.eStatus;
				}
			}
		}
	}
	else
	{
		deviceDrvTstFWDebug(LOG_TO_USR, "Framework initialization failed\n");
		freeMemory(1);
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
