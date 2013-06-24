/***********************************************************************
**
**  REBOL [R3] Language Interpreter and Run-time Environment
**
**  Copyright 2013 Andreas Bolka
**
**  Licensed under the Apache License, Version 2.0 (the "License");
**  you may not use this file except in compliance with the License.
**  You may obtain a copy of the License at
**
**  http://www.apache.org/licenses/LICENSE-2.0
**
**  Unless required by applicable law or agreed to in writing, software
**  distributed under the License is distributed on an "AS IS" BASIS,
**  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
**  See the License for the specific language governing permissions and
**  limitations under the License.
**
************************************************************************
**
**  Title: Timer device for Win32
**  Author: Andreas Bolka
**
***********************************************************************/

#include <windows.h>

#include "reb-host.h"
#include "host-lib.h"


// @@ Move or remove globals?
static HWND Timer_Handle = 0;


/***********************************************************************
**
*/	static LRESULT CALLBACK REBOL_Timer_Proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
/*
***********************************************************************/
{
	REBREQ *req = (REBREQ*)wparam;
	switch (msg) {
	case WM_TIMER:
		KillTimer(hwnd, wparam);
		if (IS_OPEN(req)) {
			Signal_Device(req, EVT_TIME);
		}
		break;

	default:
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}
	return 0;
}


/***********************************************************************
**
*/	DEVICE_CMD Init_Timer(REBREQ *dr)
/*
**		Initialize the timer device.
**
**		Create a hidden window to handle timer events, so that we can
**		use the timer_id to associate a timer with its R3 port.
**
***********************************************************************/
{
	REBDEV *dev = (REBDEV*)dr;
	WNDCLASSEX wc = {0};

	// Register event object class:
	wc.cbSize = sizeof(wc);
	wc.lpszClassName = TEXT("REBOL-Timers");
	wc.lpfnWndProc = REBOL_Timer_Proc;
	if (!RegisterClassEx(&wc)) {
		return DR_ERROR;
	}

	// Create the hidden window:
	Timer_Handle = CreateWindowEx(
		0,
		wc.lpszClassName,
		wc.lpszClassName,
		0,
		0,0,0,0,
		(HWND)-3 /*HWND_MESSAGE*/,
		NULL,
		NULL,
		NULL
	);
	if (!Timer_Handle) {
		return DR_ERROR;
	}

	SET_FLAG(dev->flags, RDF_INIT);
	return DR_DONE;
}


/***********************************************************************
**
*/	DEVICE_CMD Open_Timer(REBREQ *req)
/*
***********************************************************************/
{
	SET_OPEN(req);
	return DR_DONE;
}


/***********************************************************************
**
*/	DEVICE_CMD Close_Timer(REBREQ *req)
/*
***********************************************************************/
{
	SET_CLOSED(req);
	return DR_DONE;
}


/***********************************************************************
**
*/	DEVICE_CMD Write_Timer(REBREQ *req)
/*
**	Create or replace a timer for a timeout sepecified by req->length.
**
***********************************************************************/
{
	UINT_PTR timer_id = SetTimer(
		Timer_Handle,
		(UINT_PTR)req,
		req->length,
		(TIMERPROC)REBOL_Timer_Proc
	);
	if (!timer_id) {
		return DR_ERROR;
	}
	return DR_DONE;
}


/***********************************************************************
**
**	Command Dispatch Table (RDC_ enum order)
**
***********************************************************************/

static DEVICE_CMD_FUNC Dev_Cmds[RDC_MAX] = {
	Init_Timer,				// init device driver resources
	0,	// RDC_QUIT,		// cleanup device driver resources
	Open_Timer,				// open device unit (port)
	Close_Timer,			// close device unit
	0,	// RDC_READ,		// read from unit
	Write_Timer,			// write to unit
	0,
};

DEFINE_DEV(Dev_Timer, "Timer", 1, Dev_Cmds, RDC_MAX, 0);
