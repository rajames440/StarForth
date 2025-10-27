/*
                                  ***   StarForth   ***

  interruptpoly.c- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:03.557-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/polyml-5.9.1/src/interruptpoly/interruptpoly.c
 */

/*
   This program generates an interrupt Windows Poly/ML program in a similar
   way to sending a SIGINT (Control-C) interrupt in Unix. 
*/

#include <windows.h>
#include <ddeml.h>

// DDE Commands.
#define INTERRUPT_POLY  "[Interrupt]"
#define TERMINATE_POLY  "[Terminate]"

// Default DDE service name.
#define POLYMLSERVICE   "PolyML"


DWORD dwDDEInstance;


// DDE Callback function.
HDDEDATA CALLBACK DdeCallBack(UINT uType, UINT uFmt, HCONV hconv,
                              HSZ hsz1, HSZ hsz2, HDDEDATA hdata,
                              DWORD dwData1, DWORD dwData2)
{
    return (HDDEDATA) NULL; 
}

void SendDDEMessage(LPCTSTR lpszMessage)
{
    HCONV hcDDEConv;
    HDDEDATA res;
    // Send a DDE message to the process.
    DWORD dwInst = dwDDEInstance;
    HSZ hszServiceName =
        DdeCreateStringHandle(dwInst, POLYMLSERVICE, CP_WINANSI);

    hcDDEConv =
        DdeConnect(dwInst, hszServiceName, hszServiceName, NULL); 
    DdeFreeStringHandle(dwInst, hszServiceName);
    res =
        DdeClientTransaction((LPBYTE)lpszMessage, sizeof(INTERRUPT_POLY),
            hcDDEConv, 0L, 0, XTYP_EXECUTE, TIMEOUT_ASYNC, NULL);
    if (res) DdeFreeDataHandle(res);
}

// Interrupt the ML process as though control-C had been pressed.
void RunInterrupt() 
{
    SendDDEMessage(INTERRUPT_POLY);
}


int WINAPI WinMain(
  HINSTANCE hInstance,  // handle to current instance
  HINSTANCE hPrevInstance,  // handle to previous instance
  LPSTR lpCmdLine,      // pointer to command line
  int nCmdShow          // show state of window
)
{
    // Initialise DDE.  We only want to be a client.
    DdeInitialize(&dwDDEInstance, DdeCallBack,
        APPCMD_CLIENTONLY | CBF_SKIP_CONNECT_CONFIRMS | CBF_SKIP_DISCONNECTS, 0);

    RunInterrupt();

    DdeNameService(dwDDEInstance, 0L, 0L, DNS_UNREGISTER);

    // Unitialise DDE.
    DdeUninitialize(dwDDEInstance);

    return 0;
}
