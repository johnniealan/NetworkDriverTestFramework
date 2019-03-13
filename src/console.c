/**=============================================================================
  $Workfile: console.c$

  File Description: This file contains function definitions for updating the
  					test status on the console UI and also for updating the logs
  					on in the debug window.

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


#include "console.h"

sConsolePriv_t sConsolePriv;
char *tstStats[] = {"","RUNNING","PASSED","FAILED","TIMEOUT","ERROR"};


/**=============================================================================

	Function Name   : drawDbgWindow
    Description     : Function to draw the debug window on the console.
    Arguments       :
    			      Name            	Dir    	Description
    			      @move				In		true or false

    Returns         : None

  ============================================================================*/

void drawDbgWindow(bool move)
{
    wattron(sConsolePriv.dbgwin,COLOR_PAIR(DBG_BG));
    mvwprintw(sConsolePriv.dbgwin,1,1,"Debug Window");
    wclrtoeol(sConsolePriv.dbgwin);
    wattroff(sConsolePriv.dbgwin,COLOR_PAIR(DBG_BG));
    wborder(sConsolePriv.dbgwin,ACS_VLINE,ACS_VLINE,ACS_HLINE,ACS_HLINE,
    	0,0,0,0);
    wrefresh(sConsolePriv.dbgwin);
    mvwhline(sConsolePriv.dbgwin,2,1,ACS_HLINE,sConsolePriv.iDbgWindowCols - 2);
    if (move) wmove(sConsolePriv.dbgwin,3,0);
    nodelay(sConsolePriv.dbgwin,true);
    wrefresh(sConsolePriv.dbgwin);
}


/**=============================================================================

	Function Name   : writeDbgMessage
    Description     : Function to write the message to debug window.
    Arguments       :
    			      Name            	Dir    	Description
    			      @msg				In		Message to be displayed
    			      @fmt				In		Variable arguments list
    Returns         : None

  ============================================================================*/

void writeDbgMessage(char *msg, va_list fmt)
{
    //va_list fmt;
    int x,y;
    getyx(sConsolePriv.dbgwin,y,x);
    if (y < sConsolePriv.iDbgWindowLines )
    {
        wmove(sConsolePriv.dbgwin,3,0);
    }
    wmove(sConsolePriv.dbgwin,y,x+1);
    //va_start(fmt,msg);
    vwprintw(sConsolePriv.dbgwin,msg,fmt);
    va_end(fmt);
    getyx(sConsolePriv.dbgwin,y,x);
    drawDbgWindow(false);
    wmove(sConsolePriv.dbgwin,y,x);
}


/*void writeToDbgWindow(char *msg, ...)
{
    va_list fmt;
    int x,y;
    getyx(sConsolePriv.dbgwin,y,x);
    if (y < sConsolePriv.iDbgWindowLines )
    {
        wmove(sConsolePriv.dbgwin,3,0);
    }
    wmove(sConsolePriv.dbgwin,y,x+1);
    va_start(fmt,msg);
    vwprintw(sConsolePriv.dbgwin,msg,fmt);
    va_end(fmt);
    getyx(sConsolePriv.dbgwin,y,x);
    drawDbgWindow(false);
    wmove(sConsolePriv.dbgwin,y,x);
}*/


/**=============================================================================

	Function Name   : consoleInit
    Description     : Function to draw the main window where test case status
    				  will be displayed and to initialize the same.
    Arguments       : None
    Returns         : None

  ============================================================================*/

void consoleInit(void)
{
    initscr();
    refresh();
    getmaxyx(stdscr, sConsolePriv.iNbMainWindowLines,
    	sConsolePriv.iNbMainWindowCols);

    //20% of max size
    sConsolePriv.iDbgWindowLines = 0.2*sConsolePriv.iNbMainWindowLines;
    sConsolePriv.iNbMainWindowLines = 0.8*sConsolePriv.iNbMainWindowLines;
    sConsolePriv.iDbgWindowCols = sConsolePriv.iNbMainWindowCols;
    sConsolePriv.mainwin = newwin(sConsolePriv.iNbMainWindowLines,
    	sConsolePriv.iNbMainWindowCols,0,0);
    noecho();
    nodelay(sConsolePriv.mainwin, true);
    scrollok(sConsolePriv.mainwin,true);
    use_default_colors();
    start_color();
    init_pair(TST_RUNNING, COLOR_BLACK, COLOR_WHITE);
    init_pair(TST_PASSED, COLOR_BLACK, COLOR_GREEN);
    init_pair(TST_FAILED, COLOR_WHITE, COLOR_RED);
    init_pair(TST_TIMEOUT, COLOR_BLACK, COLOR_CYAN);
    init_pair(TST_ERROR, COLOR_WHITE, COLOR_RED);
    init_pair(DBG_BG, COLOR_WHITE, COLOR_MAGENTA);
    mvwprintw(sConsolePriv.mainwin,0,1,"TEST CASE,%c",ACS_HLINE);
    mvwprintw(sConsolePriv.mainwin,0,sConsolePriv.iNbMainWindowCols/4,
    	"TIME TAKEN");
    mvwprintw(sConsolePriv.mainwin,0,sConsolePriv.iNbMainWindowCols/1.5,
    	"TEST STATUS");
    mvwhline(sConsolePriv.mainwin,1,1,'-',
    	(sConsolePriv.iNbMainWindowCols/1.5 + 11));
    wrefresh(sConsolePriv.mainwin);
    wattron(sConsolePriv.mainwin,A_NORMAL);
    wrefresh(sConsolePriv.mainwin);
    sConsolePriv.tstIdx = 1;
    sConsolePriv.dbgwin = newwin(sConsolePriv.iDbgWindowLines,
    	sConsolePriv.iDbgWindowCols,sConsolePriv.iNbMainWindowLines+1,0);
    scrollok(sConsolePriv.dbgwin,true);
    new_panel(sConsolePriv.dbgwin);
    drawDbgWindow(true);
}


/**=============================================================================

	Function Name   : consoleExit
    Description     : Function to exit from the main window.
    Arguments       : None
    Returns         : None

  ============================================================================*/

void consoleExit(void)
{
    delwin(sConsolePriv.mainwin);
    endwin();
}


/**=============================================================================

	Function Name   : fnAddTestNameToScreen
    Description     : Function to add test case name to main window
    Arguments       :
    				  Name            	Dir    	Description
    				  @tstName			In		Test case name
    				  @timeout			In		Test case timeout
    				  @status			In		Test case status

    Returns         : Returns test case Id.

  ============================================================================*/

int fnAddTestNameToScreen(char* tstName, int timeout, int status)
{
    int iRet;
    if ( status <= 0 ) return -1;

    if ( sConsolePriv.tstIdx == sConsolePriv.iNbMainWindowLines -1 )
    {
          //writeToDbgWindow("sConsolePriv.tstIdx-%d\n",sConsolePriv.tstIdx);
          //writeToDbgWindow("tstName-%s\n",tstName);
          wmove(sConsolePriv.mainwin,0,0);
          wclear(sConsolePriv.mainwin);
          mvwprintw(sConsolePriv.mainwin,0,1,"TEST CASE,%c",ACS_VLINE);
          mvwprintw(sConsolePriv.mainwin,0,sConsolePriv.iNbMainWindowCols/4,
          	  "TIME TAKEN");
          mvwprintw(sConsolePriv.mainwin,0,sConsolePriv.iNbMainWindowCols/1.5,
          	  "TEST STATUS");
          mvwhline(sConsolePriv.mainwin,1,1,'-',
          	  (sConsolePriv.iNbMainWindowCols/1.5 + 11));
          sConsolePriv.tstIdx = 1;
          wrefresh(sConsolePriv.mainwin);
    }
    iRet = ++sConsolePriv.tstIdx;
    mvwprintw(sConsolePriv.mainwin,sConsolePriv.tstIdx,1,"%s_%d",
    	tstName,sConsolePriv.tstIdx);
    mvwprintw(sConsolePriv.mainwin,sConsolePriv.tstIdx,
    	sConsolePriv.iNbMainWindowCols/4,"%f",timeout);
    wattron(sConsolePriv.mainwin,COLOR_PAIR(status));
    mvwprintw(sConsolePriv.mainwin,sConsolePriv.tstIdx,
    	sConsolePriv.iNbMainWindowCols/1.5,"%s",tstStats[status]);
    wattroff(sConsolePriv.mainwin,COLOR_PAIR(status));
    use_default_colors();
    wrefresh(sConsolePriv.mainwin);
   return iRet;
}


/**=============================================================================

	Function Name   : fnUpdateTstStatusToScreen
    Description     : Function to update the status of the test case added to
    				  main window
    Arguments       :
    				  Name            	Dir    	Description
    				  @tstIdx			In		Test case Id
    				  @timeout			In		Test case timeout
    				  @status			In		Test case status

    Returns         : None

  ============================================================================*/

int fnUpdateTstStatusToScreen(int tstIdx, float timeout, int status)
{
    mvwhline(sConsolePriv.mainwin,tstIdx,sConsolePriv.iNbMainWindowCols/4,' ',
    	sConsolePriv.iNbMainWindowCols-sConsolePriv.iNbMainWindowCols/4);
    mvwprintw(sConsolePriv.mainwin,tstIdx,sConsolePriv.iNbMainWindowCols/4,
    	"%2f",timeout);
    wattron(sConsolePriv.mainwin,COLOR_PAIR(status));
    mvwprintw(sConsolePriv.mainwin,tstIdx,sConsolePriv.iNbMainWindowCols/1.5,
    	"%s",tstStats[status]);
    wattroff(sConsolePriv.mainwin,COLOR_PAIR(status));
    use_default_colors();
    wrefresh(sConsolePriv.mainwin);
}
