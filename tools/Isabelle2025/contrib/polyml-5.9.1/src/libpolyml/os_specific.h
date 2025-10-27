/*
    Title:      Operating Specific functions.

    Copyright (c) 2000, 2016 David C. J. Matthews

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

#ifndef _OSSPEC_H
#define _OSSPEC_H

class SaveVecEntry;
typedef SaveVecEntry *Handle;
class TaskData;

extern Handle OS_spec_dispatch_c(TaskData *taskData, Handle args, Handle code);

extern struct _entrypts osSpecificEPT[];

#endif
