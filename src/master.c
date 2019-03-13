/**=============================================================================
  $Workfile: master.c$

  File Description: Test framework Master mode of operation. This file
                    includes the functionalities required to operate the test
                    framework in master mode.

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
    E_HELLO_RCVD,
    E_SEND_STATUS,
    E_FRAMEWORK_INITIALIZED,
    E_MSG_SUIT_NAME_RCVD,
    E_MSG_TST_NAME_RCVD,
    E_MSG_TST_RUN_RCVD,
    E_CLOSE_FRAMEWOERK
}E_MASTER_STATE_MACHINE;

E_MASTER_STATE_MACHINE eMasterStateMachine = E_NONE;

/* Execution status */
E_TST_STATUS g_eExecutionStatus;
E_MSG_TYPE eReceivedMsgType = 0;

pthread_t g_masterMainThread;
sTestCase_t *pTestcase = NULL;
sTestSuite_t *pSuite = NULL;

extern pid_t cpid;
sigset_t mask;

int nCommChannel[2];						/* Socket Pair */

sHandshakeMsg_t ReceivedMsg;

/*----------------- F U N C T I O N   D E C L A R A T I O N S ----------------*/

void *pMasterWorkerThread(void *pFd);
void *pMasterMainThread(void *pFd);
static void createTimer(void);
static void initialHandshake(void);
static void childProcess(void);

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

    Function Name   : initializeMasterDevice
    Description     : Function to create master thread and also to receive the
    			      messages sent by the slave machine.
    Arguments       : None
    Returns         : None

  ============================================================================*/

void initializeMasterDevice(void)
{
	int nDataAvailable = 0, nRet = 0;
	sHandshakeMsg_t Receive, Send;

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, nCommChannel) == -1)
	{
		deviceDrvTstFWDebug(LOG_TO_USR, "Error in creating socketpair\n");
		freeMemory(1);
	}

	/* Create a thread in parent process */
	pthread_create(&g_masterMainThread, NULL, pMasterMainThread, NULL);

	memset(&ReceivedMsg, 0, sizeof(ReceivedMsg));

	while(1)
	{
		memset(&Receive,0,sizeof(Receive));
		memset(&Send,0,sizeof(Send));

		nDataAvailable = 0;

		/* Check if data available */
		nDataAvailable = peek();
		if (nDataAvailable == -1)
		{
			deviceDrvTstFWDebug(LOG_TO_USR, "Socket closed\n");
			freeMemory(1);
		}

		/* Receive data if avaialble */
		if ((nDataAvailable > 0) && (eMasterStateMachine != E_CLOSE_FRAMEWOERK))
		{
			nRet = receiveMessage(&Receive);

			/* if socket is closed abruptly */
			if(nRet == 0)
			{
				freeMemory(1);
			}

			if(g_eExecutionStatus != E_RUNNING)
			{
				memcpy(&ReceivedMsg, &Receive, sizeof(sHandshakeMsg_t));
			}
		}

		if (Receive.eMsgType == E_MSG_STATUS)
		{
			deviceDrvTstFWDebug(LOG_TO_DBG,
				"\tReceived << E_MSG_STATUS [%s:%d]\n",
				__FILENAME__, __LINE__);

			deviceDrvTstFWDebug(LOG_TO_DBG,
				"\tSend >> E_MSG_STATUS [%s:%d]\n",
				__FILENAME__, __LINE__);
			Send.eMsgType = E_MSG_STATUS;
			Send.Msg.eStatus = g_eExecutionStatus;
			sendMessage(&Send);
		}

		if(eMasterStateMachine == E_CLOSE_FRAMEWOERK)
		{
			break;
		}
	}
	pthread_join(g_masterMainThread, NULL);
}


/**=============================================================================

	Function Name   : childProcess
    Description     : This function is used to execute the testcases scheduled
    				  by the master thread and send back the execution status to
    				  master thread.
    Arguments       : None
    Returns         : None

  ============================================================================*/

static void childProcess(void)
{
    int nScreenCol = 0, nRet=0;
    int nErr = 0;
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
					ToParent.nType = E_MSG_SUIT_NAME_RCVD;
					ToParent.eStatus = pTestcase->eStatus;
					send(nCommChannel[1], &ToParent, sizeof(ToParent), 0);
				break;
			}
		}
	}
}


/**=============================================================================

    Function Name   : pMasterMainThread
    Description     : This is a thread function. It is used to find the test
    				  suite and test case sent by the slave machine in the list
    				  and send the status. If found, same test case will be
    				  scheduled to execute in child process.
    Arguments       : None
    Returns         : None

  ============================================================================*/

void *pMasterMainThread(void *pFd)
{
	int nScreenCol = 0, nDataAvailable =0, nRet = 0, nErr = 0;
	fd_set readset;
	sInterProcessMsg_t ToChild;
	sHandshakeMsg_t Send;

	/* Initialize time out struct for select() */
    struct timeval Tv;
    Tv.tv_sec = 0;
    Tv.tv_usec = 10000;

	/* Send hello message and initialize test framework */
	initialHandshake();
	if (eMasterStateMachine == E_FRAMEWORK_INITIALIZED)
	{
		createTimer();

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
				memset(&Send,0,sizeof(Send));
				memset(&ToChild,0,sizeof(ToChild));

				switch (ReceivedMsg.eMsgType)
				{
					case E_MSG_TST_SUITE_NAME:
						deviceDrvTstFWDebug(LOG_TO_DBG,
							"\tReceived << E_MSG_TST_SUITE_NAME [%s:%d]\n",
							__FILENAME__, __LINE__);

						g_eExecutionStatus = E_RUNNING;

						if (eMasterStateMachine != E_FRAMEWORK_INITIALIZED)
						{
							deviceDrvTstFWDebug(LOG_TO_DBG,
								"\tSend >> E_MSG_NACK [%s:%d]\n",
								__FILENAME__, __LINE__);
							Send.eMsgType = E_MSG_NACK;
							sendMessage(&Send);
						}
						else
						{
							deviceDrvTstFWDebug(LOG_TO_DBG,
								"\tSend >> E_MSG_ACK [%s:%d]\n",
								__FILENAME__, __LINE__);
							Send.eMsgType = E_MSG_ACK;
							sendMessage(&Send);

							eMasterStateMachine = E_MSG_SUIT_NAME_RCVD;

							deviceDrvTstFWDebug(LOG_TO_DBG,
								"\tReceived Test suite name: %s [%s:%d]\n",
								ReceivedMsg.Msg.szName, __FILENAME__, __LINE__);

							HASH_FIND_STR(g_pSuiteHead, ReceivedMsg.Msg.szName,
								pSuite);
							if (pSuite)
							{
								deviceDrvTstFWDebug(LOG_TO_DBG,
									"\tTest suite %s found [%s:%d]\n",
									ReceivedMsg.Msg.szName, __FILENAME__,
									__LINE__);

								g_eExecutionStatus = E_FOUND;
							}
							else
							{
								deviceDrvTstFWDebug(LOG_TO_DBG,
									"\tTest suite %s Not-found [%s:%d]\n",
									ReceivedMsg.Msg.szName, __FILENAME__,
									__LINE__);

								g_eExecutionStatus = E_NOT_FOUND;
								eMasterStateMachine = E_FRAMEWORK_INITIALIZED;
							}
						}
						memset(&ReceivedMsg, 0, sizeof(ReceivedMsg));
					break;

					case E_MSG_TST_SUITE_LIST_COMPLETED:
						if (eMasterStateMachine != E_FRAMEWORK_INITIALIZED)
						{
							deviceDrvTstFWDebug(LOG_TO_DBG,
								"\tSend >> E_MSG_NACK [%s:%d]\n",
								__FILENAME__, __LINE__);

							Send.eMsgType = E_MSG_NACK;
							sendMessage(&Send);
						}
						else
						{
							deviceDrvTstFWDebug(LOG_TO_DBG,
								"\tReceived << E_MSG_TST_SUITE_LIST_COMPLETED [%s:%d]\n",
								__FILENAME__, __LINE__);

							deviceDrvTstFWDebug(LOG_TO_DBG,
								"\tSend >> E_MSG_ACK [%s:%d]\n",
								__FILENAME__, __LINE__);

							Send.eMsgType = E_MSG_ACK;
							sendMessage(&Send);
						}
						memset(&ReceivedMsg, 0, sizeof(ReceivedMsg));
					break;

					case E_MSG_TST_CASE_NAME:
						deviceDrvTstFWDebug(LOG_TO_DBG,
							"\tReceived << E_MSG_TST_CASE_NAME [%s:%d]\n",
							__FILENAME__, __LINE__);

						g_eExecutionStatus = E_RUNNING;

						if (eMasterStateMachine != E_MSG_SUIT_NAME_RCVD)
						{
							deviceDrvTstFWDebug(LOG_TO_DBG,
								"\tSend >> E_MSG_NACK [%s:%d]\n",
								__FILENAME__, __LINE__);
							Send.eMsgType = E_MSG_NACK;
							sendMessage(&Send);
						}
						else
						{
							deviceDrvTstFWDebug(LOG_TO_DBG,
								"\tSend >> E_MSG_ACK [%s:%d]\n",
								__FILENAME__, __LINE__);
							Send.eMsgType = E_MSG_ACK;
							sendMessage(&Send);

							eMasterStateMachine = E_MSG_TST_NAME_RCVD;

							deviceDrvTstFWDebug(LOG_TO_DBG,
								"\tReceived Test case name: %s [%s:%d]\n",
								ReceivedMsg.Msg.szName, __FILENAME__, __LINE__);

							HASH_FIND(hh,pSuite->sTestCaseList,
								ReceivedMsg.Msg.szName,
								strlen(ReceivedMsg.Msg.szName),
								pTestcase);

							if (g_bConsoleInitialized)
								nScreenCol =
									fnAddTestNameToScreen(
										ReceivedMsg.Msg.szName, 0, TST_RUNNING);

							if (pTestcase != NULL)
							{
								deviceDrvTstFWDebug(LOG_TO_DBG,
									"\tTest case %s found [%s:%d]\n",
									ReceivedMsg.Msg.szName, __FILENAME__,
									__LINE__);

								g_eExecutionStatus = E_FOUND;
							}
							else
							{
								deviceDrvTstFWDebug(LOG_TO_DBG,
									"\tTest case %s not found [%s:%d]\n",
									ReceivedMsg.Msg.szName, __FILENAME__,
									__LINE__);

								g_eExecutionStatus = E_NOT_FOUND;
								eMasterStateMachine = E_MSG_SUIT_NAME_RCVD;

								if (g_bConsoleInitialized)
									fnUpdateTstStatusToScreen(nScreenCol, 0,
										TST_ERROR);
							}
						}
						memset(&ReceivedMsg, 0, sizeof(ReceivedMsg));
					break;

					case E_MSG_TST_RUN:
						deviceDrvTstFWDebug(LOG_TO_DBG,
							"\tReceived << E_MSG_TST_RUN [%s:%d]\n",
							__FILENAME__, __LINE__);
						g_eExecutionStatus  = E_RUNNING;

						if (eMasterStateMachine != E_MSG_TST_NAME_RCVD)
						{
							deviceDrvTstFWDebug(LOG_TO_DBG,
								"\tSend >> E_MSG_NACK [%s:%d]\n",
								__FILENAME__, __LINE__);

							Send.eMsgType = E_MSG_NACK;
							sendMessage(&Send);
						}
						else
						{
							deviceDrvTstFWDebug(LOG_TO_DBG,
								"\tSend >> E_MSG_ACK [%s:%d]\n",
								__FILENAME__, __LINE__);

							Send.eMsgType = E_MSG_ACK;
							sendMessage(&Send);

							eMasterStateMachine = E_MSG_TST_RUN_RCVD;

							ToChild.nType = 0x01;
							ToChild.pTestcase = pTestcase;
							send(nCommChannel[0], &ToChild,
								sizeof(ToChild), 0);
							checkTimer(&g_timerId, pTestcase->uTimeout, true);

							deviceDrvTstFWDebug(LOG_TO_DBG,
								"\tTest case is running... [%s:%d]\n",
								__FILENAME__, __LINE__);
						}
						memset(&ReceivedMsg, 0, sizeof(ReceivedMsg));
					break;

					case E_MSG_TST_CASE_LIST_COMPLETED:
						if (eMasterStateMachine != E_MSG_SUIT_NAME_RCVD)
						{
							deviceDrvTstFWDebug(LOG_TO_DBG,
								"\tSend >> E_MSG_NACK [%s:%d]\n",
								__FILENAME__, __LINE__);
							Send.eMsgType = E_MSG_NACK;
							sendMessage(&Send);
						}
						else
						{
							deviceDrvTstFWDebug(LOG_TO_DBG,
								"\tReceived << E_MSG_TST_CASE_LIST_COMPLETED [%s:%d]\n",
								__FILENAME__, __LINE__);

							deviceDrvTstFWDebug(LOG_TO_DBG,
								"\tSend >> E_MSG_ACK [%s:%d]\n",
								__FILENAME__, __LINE__);

							Send.eMsgType = E_MSG_ACK;
							sendMessage(&Send);

							eMasterStateMachine = E_FRAMEWORK_INITIALIZED;
						}
						memset(&ReceivedMsg, 0, sizeof(ReceivedMsg));
					break;

					case E_MSG_CLOSE:
						deviceDrvTstFWDebug(LOG_TO_DBG,
							"\tReceived << E_MSG_CLOSE [%s:%d]\n",
							__FILENAME__, __LINE__);
						deviceDrvTstFWDebug(LOG_TO_DBG,
							"\tSend >> E_MSG_ACK [%s:%d]\n",
							__FILENAME__, __LINE__);
						Send.eMsgType = E_MSG_ACK;
						sendMessage(&Send);

						eMasterStateMachine = E_CLOSE_FRAMEWOERK;
						memset(&ReceivedMsg, 0, sizeof(ReceivedMsg));
					break;
				}

				if(eMasterStateMachine == E_CLOSE_FRAMEWOERK)
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

					eMasterStateMachine = ToChild.nType;
					pTestcase->eStatus = ToChild.eStatus;
					pTestcase->dElapsedTime = (pTestcase->uTimeout/1000) -
							checkTimer(&g_timerId, pTestcase->uTimeout, false);

					g_eExecutionStatus = pTestcase->eStatus;

                    if(pTestcase->eStatus == E_PASSED)
                    {
                        deviceDrvTstFWDebug(LOG_TO_DBG,
                        	"\tTest case %s passed [%s:%d]\n",
                            pTestcase->szName, __FILENAME__, __LINE__);

                        if (g_bConsoleInitialized)
							fnUpdateTstStatusToScreen(nScreenCol,
								pTestcase->dElapsedTime, TST_PASSED);
                    }
                    else if(pTestcase->eStatus == E_FAILED)
                    {
                        deviceDrvTstFWDebug(LOG_TO_DBG,
                        	"\tTest case %s failed [%s:%d]\n",
                            pTestcase->szName, __FILENAME__, __LINE__);

                        if (g_bConsoleInitialized)
							fnUpdateTstStatusToScreen(nScreenCol,
								pTestcase->dElapsedTime, TST_FAILED);
                    }
                    else if(pTestcase->eStatus == E_TIMEOUT)
                    {
                        deviceDrvTstFWDebug(LOG_TO_DBG,
                        	"\tTest case %s Timeout [%s:%d]\n",
                            pTestcase->szName, __FILENAME__, __LINE__);

                        if (g_bConsoleInitialized)
							fnUpdateTstStatusToScreen(nScreenCol, 0,
								TST_TIMEOUT);
                    }
				}
			}
		}
	}
	else
	{
		pthread_exit(NULL);
	}
}


/**=============================================================================

	Function Name   : initialHandshake
    Description     : This function is used to do initial handshaking with
    				  slave machine and also to initialize the test framework
    				  (loading shared object, adding test suites and test cases
    				  to list, allocating memory to store test summary).
    Arguments       : None
    Returns         : None

  ============================================================================*/

static void initialHandshake(void)
{
	int nDataAvailable = 0, nRet = 0;
	sHandshakeMsg_t Send;

	while(1)
	{
		memset(&Send, 0, sizeof(Send));

		switch(ReceivedMsg.eMsgType)
		{
			case E_MSG_HELLO:

				deviceDrvTstFWDebug(LOG_TO_DBG,
					"\tReceived << E_MSG_HELLO [%s:%d]\n",
					__FILENAME__, __LINE__);

				deviceDrvTstFWDebug(LOG_TO_DBG,
					"\tSend >> E_MSG_ACK [%s:%d]\n",
					__FILENAME__, __LINE__);

				Send.eMsgType = E_MSG_ACK;
				eMasterStateMachine = E_HELLO_RCVD;
				sendMessage(&Send);
				memset(&ReceivedMsg, 0, sizeof(ReceivedMsg));
			break;

			case E_MSG_FRAMEWORK_INIT:
				deviceDrvTstFWDebug(LOG_TO_DBG,
					"\tReceived << E_MSG_FRAMEWORK_INIT [%s:%d]\n",
					__FILENAME__, __LINE__);
				if (eMasterStateMachine != E_HELLO_RCVD)
				{
					deviceDrvTstFWDebug(LOG_TO_DBG,
						"\tSend >> E_MSG_NACK [%s:%d]\n",
						__FILENAME__, __LINE__);
					Send.eMsgType = E_MSG_NACK;
					sendMessage(&Send);
				}
				else
				{
					g_eExecutionStatus = E_RUNNING;
					deviceDrvTstFWDebug(LOG_TO_DBG,
						"\tSend >> E_MSG_ACK [%s:%d]\n",
						__FILENAME__, __LINE__);
					Send.eMsgType = E_MSG_ACK;
					sendMessage(&Send);

					nRet = initializeTestFramework();
					if (nRet==0)
                    {
                        g_eExecutionStatus = E_PASSED;
                        eMasterStateMachine = E_FRAMEWORK_INITIALIZED;
                    }
                    else
                    {
                        g_eExecutionStatus = E_FAILED;
                    }
				}
				memset(&ReceivedMsg, 0, sizeof(ReceivedMsg));
			break;

			case E_MSG_CLOSE:
		        deviceDrvTstFWDebug(LOG_TO_USR,
		        	"\tReceived << E_MSG_CLOSE [%s:%d]\n",
		        	__FILENAME__, __LINE__);

				deviceDrvTstFWDebug(LOG_TO_DBG, "\tSend >> E_MSG_ACK [%s:%d]\n",
					__FILENAME__, __LINE__);

				Send.eMsgType = E_MSG_ACK;
				sendMessage(&Send);

				eMasterStateMachine = E_CLOSE_FRAMEWOERK;
				memset(&ReceivedMsg, 0, sizeof(ReceivedMsg));
			break;
		}

		if((eMasterStateMachine == E_FRAMEWORK_INITIALIZED) ||
			(eMasterStateMachine == E_CLOSE_FRAMEWOERK))
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
