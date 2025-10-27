/*
Title:      Windows start-up code.

Copyright (c) 2019 David C. J. Matthews

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License version 2.1 as published by the Free Software Foundation.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

*/

#ifndef _WINSTARTUP_H
#define _WINSTARTUP_H

#include <windows.h>

extern bool useConsole;

extern HINSTANCE hApplicationInstance; /* Application instance */

                                       /* DDE requests. */
extern HCONV StartDDEConversation(TCHAR *serviceName, TCHAR *topicName);
extern void CloseDDEConversation(HCONV hConv);
extern LRESULT ExecuteDDE(char *command, HCONV hConv);

extern void SetupDDEHandler(const TCHAR *lpszName);

class WinStream;
// The stream objects created by winguiconsole.
// Standard streams. 
extern WinStream *standardInput, *standardOutput, *standardError;

#endif