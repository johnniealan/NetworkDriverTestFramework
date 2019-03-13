// Author: Johnnie Alan
#include "testStruct.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int testFunc1(void);
int testFunc2(void);
int testFunc3(void);

void cleanup(void)
{
    printf("Cleaning up...\n");
}

struct user_test_case aTestCase1[3] = {"TESTCASE_1", 10000, "testFunc1",
                              		"TESTCASE_2",10000, "testFunc2",
                                     TEST_CASE_END, 0, NULL};

struct user_test_case aTestCase2[2] = {"TESTCASE_1", 5000, "testFunc3",
                                    TEST_CASE_END, 0, NULL};


struct user_test_suite aTestSuite[3] = {"TESTSUIT_1", "cleanup", aTestCase1,
                                     "TESTSUIT_2", "cleanup", aTestCase2,
                                      TEST_SUITE_END, 0, NULL};

int testFunc1(void)
{
    printf("In test case 1\n");
    sleep(5);
    return 1;
}

int testFunc2(void)
{
    printf("In test case 2\n");
    sleep(1);
    return 2;
}

int testFunc3(void)
{
    printf("In test case 1\n");
    sleep(10);
    return 1;
}

