/**=============================================================================
  $Workfile: console.h $

  File Description: Contains Macros and declarations of functions related to
                    displaying the log on the console UI

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

#ifndef __CONSOLE_H__
#define __CONSOLE_H__


#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <curses.h>


/*------------------------ D E F I N E S   S E C T I O N ---------------------*/

#define TST_RUNNING  1
#define TST_PASSED   2
#define TST_FAILED   3
#define TST_TIMEOUT  4
#define TST_ERROR    5
#define DBG_BG       6


typedef struct{
  WINDOW *mainwin;
  WINDOW *dbgwin;
  int iNbMainWindowLines;
  int iNbMainWindowCols;
  int iDbgWindowLines;
  int iDbgWindowCols;
  int tstIdx;
}sConsolePriv_t;


/*------------------- F U N C T I O N   P R O T O T Y P E --------------------*/

void writeDbgMessage(char *pszMsg, va_list fmt);
void writeToDbgWindow(char *pszMsg, ...);
void consoleInit(void);
void consoleExit(void);
int fnAddTestNameToScreen(char* pszTstName, int nTimeout, int nStatus);
int fnUpdateTstStatusToScreen(int nTstIdx, float fTimeout, int nStatus);

#endif //__CONSOLE_H__
