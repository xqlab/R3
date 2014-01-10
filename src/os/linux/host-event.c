/***********************************************************************
**
**  REBOL [R3] Language Interpreter and Run-time Environment
**
**  Copyright 2012 REBOL Technologies
**  REBOL is a trademark of REBOL Technologies
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
**  Title: Device: Event handler for X window
**  Author: Shixin Zeng
**  Purpose:
**      Processes X events to pass to REBOL
*/

#include <math.h>
#include  <X11/Xlib.h>

#include "reb-host.h"

#include "host-window.h"
#include "host-compositor.h"
#include "keysym2ucs.h"

extern x_info_t *global_x_info;
REBGOB *Find_Gob_By_Window(Window win);
void* Find_Compositor(REBGOB *gob);
REBEVT *RL_Find_Event (REBINT model, REBINT type);

typedef struct rebcmp_ctx REBCMP_CTX;
void rebcmp_compose_region(REBCMP_CTX* ctx, REBGOB* winGob, REBGOB* gob, XRectangle *rect, REBOOL only);

#define GOB_COMPOSITOR(gob)	(Find_Compositor(gob)) //gets handle to window's compositor
#define DOUBLE_CLICK_DIFF 300 /* in milliseconds */

// Virtual key conversion table, sorted by first column.
const REBCNT keysym_to_event[] = {
    /* 0xff09 */    XK_Tab,     	EVK_NONE,   //EVK_NONE means it is passed 'as-is'
	/* 0xff50 */	XK_Home,		EVK_HOME,
	/* 0xff51 */	XK_Left,		EVK_LEFT,
	/* 0xff52 */	XK_Up,			EVK_UP,
	/* 0xff53 */	XK_Right,		EVK_RIGHT,
	/* 0xff54 */	XK_Down,		EVK_DOWN,
	/* 0xff55 */	XK_Page_Up,		EVK_PAGE_UP,
	/* 0xff56 */	XK_Page_Down,	EVK_PAGE_DOWN,
	/* 0xff57 */	XK_End,			EVK_END,
	/* 0xff63 */	XK_Insert,		EVK_INSERT,

	/* 0xff91 */	XK_KP_F1,		EVK_F1,
	/* 0xff92 */	XK_KP_F2,		EVK_F2,
	/* 0xff93 */	XK_KP_F3,		EVK_F3,
	/* 0xff94 */	XK_KP_F4,		EVK_F4,
	/* 0xff95 */	XK_KP_Home,		EVK_HOME,
	/* 0xff96 */	XK_KP_Left,		EVK_LEFT,
	/* 0xff97 */	XK_KP_Up,		EVK_UP,
	/* 0xff98 */	XK_KP_Right,	EVK_RIGHT,
	/* 0xff99 */	XK_KP_Down,		EVK_DOWN,
	/* 0xff9a */	XK_KP_Page_Up,	EVK_PAGE_UP,
	/* 0xff9b */	XK_KP_Page_Down, EVK_PAGE_DOWN,
	/* 0xff9c */	XK_KP_End,		EVK_END,
	/* 0xff9e */	XK_KP_Insert,	EVK_INSERT,
	/* 0xff9f */	XK_KP_Delete,   EVK_DELETE,

	/* 0xffbe */	XK_F1,			EVK_F1,
	/* 0xffbf */	XK_F2,			EVK_F2,
	/* 0xffc0 */	XK_F3,			EVK_F3,
	/* 0xffc1 */	XK_F4,			EVK_F4,
	/* 0xffc2 */	XK_F5,			EVK_F5,
	/* 0xffc3 */	XK_F6,			EVK_F6,
	/* 0xffc4 */	XK_F7,			EVK_F7,
	/* 0xffc5 */	XK_F8,			EVK_F8,
	/* 0xffc6 */	XK_F9,			EVK_F9,
	/* 0xffc7 */	XK_F10,			EVK_F10,
	/* 0xffc8 */	XK_F11,			EVK_F11,
	/* 0xffc9 */	XK_F12,			EVK_F12,
	/* 0xffff */	XK_Delete,		EVK_DELETE,
					0x0,			0

};

static void Add_Event_XY(REBGOB *gob, REBINT id, REBINT xy, REBINT flags)
{
	REBEVT evt;

	evt.type  = id;
	evt.flags = (u8) (flags | (1<<EVF_HAS_XY));
	evt.model = EVM_GUI;
	evt.data  = xy;
	evt.ser = (void*)gob;

	RL_Event(&evt);	// returns 0 if queue is full
}

static void Update_Event_XY(REBGOB *gob, REBINT id, REBINT xy, REBINT flags)
{
	REBEVT evt;

	evt.type  = id;
	evt.flags = (u8) (flags | (1<<EVF_HAS_XY));
	evt.model = EVM_GUI;
	evt.data  = xy;
	evt.ser = (void*)gob;

	RL_Update_Event(&evt);
}

static void Add_Event_Key(REBGOB *gob, REBINT id, REBINT key, REBINT flags)
{
	REBEVT evt;

	evt.type  = id;
	evt.flags = flags;
	evt.model = EVM_GUI;
	evt.data  = key;
	evt.ser = (void*)gob;

	RL_Event(&evt);	// returns 0 if queue is full
}

static Check_Modifiers(REBINT flags, unsigned state)
{
	if (state & ShiftMask) flags |= (1<<EVF_SHIFT);
	if (state & ControlMask) flags |= (1<<EVF_CONTROL);
	return flags;
}

void Dispatch_Event(XEvent *ev)
{
	REBGOB *gob = NULL;
	// Handle XEvents and flush the input 
    REBINT keysyms_per_keycode_return;
	REBINT xyd = 0;
	XConfigureEvent xce;
	REBEVT *evt = NULL;
	REBINT flags = 0;
	static Time last_click = 0;
	static REBINT last_click_button = 0;
	switch (ev->type) {
		case Expose:
			//RL_Print("exposed\n");
			gob = Find_Gob_By_Window(ev->xexpose.window);
			if (gob != NULL){
				/* find wingob, copied from Draw_Window */
				REBGOB *wingob = gob;
				while (GOB_PARENT(wingob) && GOB_PARENT(wingob) != Gob_Root
					   && GOB_PARENT(wingob) != wingob) // avoid infinite loop
					wingob = GOB_PARENT(wingob);

				//check if it is really open
				if (!IS_WINDOW(wingob) || !GET_GOB_STATE(wingob, GOBS_OPEN)) return;

				void *compositor = GOB_COMPOSITOR(gob);
				XRectangle rect = {ev->xexpose.x, ev->xexpose.y, ev->xexpose.width, ev->xexpose.height};
				//RL_Print("exposed: x %d, y %d, w %d, h %d\n", rect.x, rect.y, rect.width, rect.height);
				rebcmp_compose_region(compositor, wingob, gob, &rect, FALSE);
				rebcmp_blit(compositor);
			}
			break;
		case ButtonPress:
		case ButtonRelease:
			//RL_Print("Button %d event at %d\n", ev->xbutton.button, ev->xbutton.time);
			gob = Find_Gob_By_Window(ev->xbutton.window);
			if (gob != NULL) {
				xyd = (ROUND_TO_INT(PHYS_COORD_X(ev->xbutton.x))) + (ROUND_TO_INT(PHYS_COORD_Y(ev->xbutton.y)) << 16);
				REBINT id = 0, flags = 0;
				flags = Check_Modifiers(0, ev->xbutton.state);
				if (ev->xbutton.button < 4) {
					if (ev->type == ButtonPress
						&& last_click_button == ev->xbutton.button
						&& ev->xbutton.time - last_click < DOUBLE_CLICK_DIFF){
						/* FIXME, a hack to detect double click: a double click would be a single click followed by a double click */
						flags |= EVF_DOUBLE;
						//RL_Print("Button %d double clicked\n", ev->xbutton.button);
					}
					switch (ev->xbutton.button){
						case 1: //left button
							id = (ev->type == ButtonPress)? EVT_DOWN: EVT_UP;
							break;
						case 2: //middle button
							id = (ev->type == ButtonPress)? EVT_AUX_DOWN: EVT_AUX_UP;
							break;
						case 3: //right button
							id = (ev->type == ButtonPress)? EVT_ALT_DOWN: EVT_ALT_UP;
							break;
					}
					Add_Event_XY(gob, id, xyd, flags);
				} else {
					switch (ev->xbutton.button){
						case 4: //wheel scroll up
							if (ev->type == ButtonRelease) {
								//RL_Print("Scrolling up by 1 line\n");
								evt = RL_Find_Event(EVM_GUI,
													ev->xbutton.state & ControlMask? EVT_SCROLL_PAGE: EVT_SCROLL_LINE);
								if (evt != NULL){
									//RL_Print("Current line = %x\n", evt->data >> 16);
									if (evt->data < 0){
										evt->data = 0;
									}
									evt->data += 3 << 16;
								} else {
									Add_Event_XY(gob,
												 ev->xbutton.state & ControlMask? EVT_SCROLL_PAGE: EVT_SCROLL_LINE,
												 1 << 16, 0);
								}
							}
							break;
						case 5: //wheel scroll down
							if (ev->type == ButtonRelease) {
								//RL_Print("Scrolling down by 1 line\n");
								evt = RL_Find_Event(EVM_GUI, 
													ev->xbutton.state & ControlMask? EVT_SCROLL_PAGE: EVT_SCROLL_LINE);
								if (evt != NULL){
									//RL_Print("Current line = %x\n", evt->data >> 16);
									if (evt->data > 0){
										evt->data = 0;
									}
									evt->data -= 3 << 16;
								} else {
									Add_Event_XY(gob, 
												 ev->xbutton.state & ControlMask? EVT_SCROLL_PAGE: EVT_SCROLL_LINE, 
												 -1 << 16, 0);
								}
							}
							break;
						default:
							RL_Print("Unrecognized mouse button %d", ev->xbutton.button);
					}
				}
			}
			if (ev->type == ButtonPress) {
				last_click_button = ev->xbutton.button;
				last_click = ev->xbutton.time;
			}
			break;

		case MotionNotify:
			//RL_Print ("mouse motion\n");
			gob = Find_Gob_By_Window(ev->xmotion.window);
			if (gob != NULL){
				xyd = (ROUND_TO_INT(PHYS_COORD_X(ev->xmotion.x))) + (ROUND_TO_INT(PHYS_COORD_Y(ev->xmotion.y)) << 16);
				Update_Event_XY(gob, EVT_MOVE, xyd, 0);
			}
			break;
		case KeyPress:
		case KeyRelease:
			gob = Find_Gob_By_Window(ev->xkey.window);
			if(gob != NULL){
				KeySym keysym;
				flags = Check_Modifiers(0, ev->xkey.state);
				char key_string[8];
				XComposeStatus compose_status;
				int i = 0, key = -1;
				int len = XLookupString(&ev->xkey, key_string, sizeof(key_string), &keysym, &compose_status);
				key_string[len] = '\0';
				//RL_Print ("key %s (%x) is released\n", key_string, key_string[0]);


				for (i = 0; keysym_to_event[i] && keysym > keysym_to_event[i]; i += 2);
				if (keysym == keysym_to_event[i]) {
					key = keysym_to_event[i + 1] << 16;
				} else {
					key = keysym2ucs(keysym);
					if (key < 0 && len > 0){
						key = key_string[0]; /* FIXME, key_string could be longer than 1 */
					}
				}

				if (key > 0){
				   Add_Event_Key(gob,
								 ev->type == KeyPress? EVT_KEY : EVT_KEY_UP, 
								 key, flags);

				   /*
					RL_Print ("Key event %s with key %x (flags: %x) is sent\n",
								 ev->type == KeyPress? "EVT_KEY" : "EVT_KEY_UP", 
								 key,
								 flags);
					 */
				}
			}

			break;
		case ResizeRequest:
			//RL_Print ("request to resize to %dx%d", ev->xresizerequest.width, ev->xresizerequest.height);
			break;
		case FocusIn:
			if (ev->xfocus.mode != NotifyWhileGrabbed) {
				//RL_Print ("FocusIn, type = %d, window = %x\n", ev->xfocus.type, ev->xfocus.window);
				gob = Find_Gob_By_Window(ev->xfocus.window);
				if (gob && !GET_GOB_STATE(gob, GOBS_ACTIVE)) {
					SET_GOB_STATE(gob, GOBS_ACTIVE);
					Add_Event_XY(gob, EVT_ACTIVE, 0, 0);
				}
			}
			break;
		case FocusOut:
			if (ev->xfocus.mode != NotifyWhileGrabbed) {
				//RL_Print ("FocusOut, type = %d, window = %x\n", ev->xfocus.type, ev->xfocus.window);
				gob = Find_Gob_By_Window(ev->xfocus.window);
				if (gob && GET_GOB_STATE(gob, GOBS_ACTIVE)) {
					CLR_GOB_STATE(gob, GOBS_ACTIVE);
					Add_Event_XY(gob, EVT_INACTIVE, 0, 0);
				}
			}
			break;
		case DestroyNotify:
			//RL_Print ("destroyed %x\n", ev->xdestroywindow.window);
			gob = Find_Gob_By_Window(ev->xdestroywindow.window);
			if (gob != NULL){
				Free_Window(gob);
			}
			break;
		case ClientMessage:
			//RL_Print ("closed\n");
			gob = Find_Gob_By_Window(ev->xclient.window);
			if (gob != NULL){
				Add_Event_XY(gob, EVT_CLOSE, 0, 0);
			}
			break;
		case ConfigureNotify:
			xce = ev->xconfigure;
			/*
			RL_Print("configuranotify, x = %d, y = %d, w = %d, h = %d\n",
					 xce.x, xce.y, xce.width, xce.height);
			*/
			gob = Find_Gob_By_Window(ev->xconfigure.window);
			if (gob != NULL) {
				if (gob->offset.x != xce.x || gob->offset.y != xce.y){
					xyd = (ROUND_TO_INT(xce.x)) + (ROUND_TO_INT(xce.y) << 16);
					//RL_Print("%s, %s, %d: EVT_OFFSET is sent\n", __FILE__, __func__, __LINE__);
					gob->offset.x = xce.x;
					gob->offset.y = xce.y;
					Update_Event_XY(gob, EVT_OFFSET, xyd, 0);
				}
				//RL_Print("WM_MOVE: %x\n", xyd);
				xyd = (ROUND_TO_INT(xce.width)) + (ROUND_TO_INT(xce.height) << 16);
				gob->size.x = xce.width;
				gob->size.y = xce.height;
				Resize_Window(gob, TRUE);
				//RL_Print("%s, %s, %d: EVT_RESIZE is sent: %x\n", __FILE__, __func__, __LINE__, xyd);
				Update_Event_XY(gob, EVT_RESIZE, xyd, 0); //This is needed even when Resize_Window returns false, in which case, Rebol changed the window size and OS_Update_Window has been called.
			}
			break;
		default:
			//RL_Print("default event type\n");
			break;
	}
}

void X_Event_Loop(int at_most)
{
	XEvent ev;
	int n = 0;
	while(XPending(global_x_info->display) && (at_most < 0 || n < at_most)) {
		++ n;
		XNextEvent(global_x_info->display, &ev);
		Dispatch_Event(&ev);
	}
}