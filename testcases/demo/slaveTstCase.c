// Author: Johnnie Alan
#include "testStruct.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

int testFunc1(void);
int testFunc2(void);
int testFunc3(void);
bool flag = false;

void child_handler(int sig, siginfo_t *siginfo, void *context)
{
	if(sig == SIGUSR1)
	{
		printf("Timeout, Cleaning up...\n");
		flag = true;
	}
}


struct user_test_case aTestCase1[] = {/*"TESTCASE_1", 10000, "testFunc1",
                              		"TESTCASE_2",10000, "testFunc2",
									"TESTCASE_3", 5000, "testFunc3",*/
									/*"TESTCASE_1", 10000, "testFunc1",
                              		"TESTCASE_2",10000, "testFunc2",
									"TESTCASE_3", 5000, "testFunc3",
									"TESTCASE_1", 10000, "testFunc1",
                              		"TESTCASE_2",10000, "testFunc2",
									"TESTCASE_3", 5000, "testFunc3",
									"TESTCASE_1", 10000, "testFunc1",
                              		"TESTCASE_2",10000, "testFunc2",
									"TESTCASE_3", 5000, "testFunc3",*/
                                     TEST_CASE_END};


struct user_test_case aTestCase2[] = {"TESTCASE_1", 10000, "testFunc1",
                              		"TESTCASE_2",10000, "testFunc2",
									"TESTCASE_3", 5000, "testFunc3",
									/*"TESTCASE_1", 10000, "testFunc1",
                              		"TESTCASE_2",10000, "testFunc2",
									"TESTCASE_3", 5000, "testFunc3",
									"TESTCASE_1", 10000, "testFunc1",
                              		"TESTCASE_2",10000, "testFunc2",
									"TESTCASE_3", 5000, "testFunc3",
									"TESTCASE_1", 10000, "testFunc1",
                              		"TESTCASE_2",10000, "testFunc2",
									"TESTCASE_3", 5000, "testFunc3",*/
                                     TEST_CASE_END};


struct user_test_suite aTestSuite[] = {"TESTSUIT_1", "cleanup", aTestCase1,
                                     "TESTSUIT_2", "cleanup", aTestCase2,
                                      TEST_SUITE_END};

int testFunc1(void)
{
    printf("In test case 1\n");
    sleep(5);
    return 1;
}

int testFunc2(void)
{
    printf("In test case 20\n");
    return 2;
}

int testFunc3(void)
{
	struct sigaction act;
	memset(&act, '\0', sizeof(act));
	act.sa_sigaction = child_handler;
	act.sa_flags = SA_SIGINFO;
	if (sigaction(SIGUSR1, &act, NULL) < 0) {
		perror("sigaction");
		exit(1);
	}

    printf("In test case 3\n");
    while(1){
    	if(flag == true)
    		break;
    	sleep(1);
    }
    flag = false;
    return 3;

}
