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
**  Module:  p-timer.c
**  Summary: timer port interface
**  Section: ports
**  Author:  Andreas Bolka
**
************************************************************************
**
** Todo:
**
** - Use WAIT-like time specification (time! as duration, integer! and
**   decimal! as duration in seconds).
**
** - Optional "once" and "restart" keywords?
**
***********************************************************************/
/*
	Carl Sassenrath's original notes on the general idea of timer usage:

	t: open timer://name
	write t 10	; set timer - also allow: 1.23 1:23
	wait t
	clear t		; reset or delete?
	read t		; get timer value
	t/awake: func [event] [print "timer!"]
	one-shot vs restart timer
*/

#include "sys-core.h"


/***********************************************************************
**
*/	static int Timer_Actor(REBVAL *ds, REBSER *port, REBCNT action)
/*
***********************************************************************/
{
	REBVAL *arg;
	REBREQ *req;

	Validate_Port(port, action);

	arg = D_ARG(2);
	req = Use_Port_State(port, RDI_TIMER, sizeof(REBREQ));

	switch (action) {
	case A_WRITE:
		// @@ WRITE should accept integer! as DATA.
		if (!(IS_BLOCK(arg)
				&& VAL_BLK_LEN(arg) == 1
				&& IS_INTEGER(VAL_BLK(arg))
				&& VAL_INT32(VAL_BLK(arg)) >= 0)) {
			Trap1(RE_INVALID_PORT_ARG, arg);
		}

		if (!IS_OPEN(req)) {
			if (OS_DO_DEVICE(req, RDC_OPEN)) {
				Trap_Port(RE_CANNOT_OPEN, port, req->error);
			}
		}

		req->length = VAL_INT32(VAL_BLK(arg));
		OS_DO_DEVICE(req, RDC_WRITE);
		break;

	case A_OPEN:
		if (OS_DO_DEVICE(req, RDC_OPEN)) {
			Trap_Port(RE_CANNOT_OPEN, port, req->error);
		}
		break;

	case A_CLOSE:
		OS_DO_DEVICE(req, RDC_CLOSE);
		break;

	case A_OPENQ:
		if (IS_OPEN(req)) {
			return R_TRUE;
		}
		return R_FALSE;

	case A_UPDATE:
		// Update the port object after before invoking the AWAKE handler. This
		// is needed by (and normally called by) the WAKE-UP function.
		return R_FALSE;

	default:
		Trap_Action(REB_PORT, action);
	}

	return R_ARG1; // port
}


/***********************************************************************
**
*/	void Init_Timer_Scheme(void)
/*
***********************************************************************/
{
	//Debug_Fmt("Initialising timer:// scheme.");
	Register_Scheme(SYM_TIMER, 0, Timer_Actor);
}
