/**=============================================================================
  $Workfile: testStruct.h $

  File Description: This file includes data structures definitions required for
  					writing user test suite and test case

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


/*------------------------ D A T A  S T R U C T U R E S ----------------------*/

#ifndef __TEST_STRUCT__H__
#define __TEST_STRUCT__H__

#include <stdbool.h>

#define TEST_SUITE_END  "EOL"
#define TEST_CASE_END   "EOL"

typedef struct user_test_case
{
    unsigned char szName[30];
    unsigned int uTimeout;
    char *pTestCaseFuncName;
}sUserTestCase_t;

typedef struct user_test_suite
{
    unsigned char szName[30];
    char *pCleanupFuncName;
    sUserTestCase_t *pTestCase;
}sUserTestSuite_t;

#endif //__TEST_STRUCT__H__
