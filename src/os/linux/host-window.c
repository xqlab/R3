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
**  Title: <platform> Windowing support
**  Author: Richard Smolak
**  File:  host-window.c
**  Purpose: Provides functions for windowing.
**
************************************************************************
**
**  NOTE to PROGRAMMERS:
**
**    1. Keep code clear and simple.
**    2. Document unusual code, reasoning, or gotchas.
**    3. Use same style for code, vars, indent(4), comments, etc.
**    4. Keep in mind Linux, OS X, BSD, big/little endian CPUs.
**    5. Test everything, then test it again.
**
***********************************************************************/

#include <string.h>
#include <math.h>
#include <unistd.h>
#include "reb-host.h"
#include "host-compositor.h"

#include "host-lib.h"
#include  <X11/Xlib.h>
#include  <X11/Xatom.h>
#include  <X11/Xutil.h>

#include "host-window.h"

//***** Constants *****

#define GOB_HWIN(gob)	(Find_Window(gob))
#define GOB_COMPOSITOR(gob)	(Find_Compositor(gob)) //gets handle to window's compositor

//***** Externs *****
extern REBGOBWINDOWS *Gob_Windows;
extern void Free_Window(REBGOB *gob);
extern void* Find_Compositor(REBGOB *gob);
extern REBINT Alloc_Window(REBGOB *gob);
extern void Draw_Window(REBGOB *wingob, REBGOB *gob);

x_info_t *global_x_info = NULL;
//***** Locals *****

#define MAX_WINDOWS 64 //must be in sync with os/host-view.c

REBGOB *Find_Gob_By_Window(Window win)
{
	int i = 0;
	for(i = 0; i < MAX_WINDOWS; i ++ ){
		host_window_t *hw = Gob_Windows[i].win;
		if (hw && hw->x_window == win){
			return Gob_Windows[i].gob;
		}
	}
	return NULL;
}

static REBXYF Zero_Pair = {0, 0};
//**********************************************************************
//** OSAL Library Functions ********************************************
//**********************************************************************

/***********************************************************************
**
*/	void OS_Init_Windows()
/*
**		Initialize special variables of the graphics subsystem.
**
***********************************************************************/
{
	int depth;
	int red_mask, green_mask, blue_mask;
	global_x_info = OS_Make(sizeof(x_info_t));
	global_x_info->display = XOpenDisplay(NULL);
	if (global_x_info->display == NULL){
		RL_Print("XOpenDisplay failed");
	}else{
		RL_Print("XOpenDisplay succeeded: x_dislay = %x\n", global_x_info->display);
	}

	global_x_info->default_screen = DefaultScreenOfDisplay(global_x_info->display);
	global_x_info->default_visual = DefaultVisualOfScreen(global_x_info->default_screen);
	depth = DefaultDepthOfScreen(global_x_info->default_screen);

	red_mask = global_x_info->default_visual->red_mask;
	green_mask = global_x_info->default_visual->green_mask;
	blue_mask = global_x_info->default_visual->blue_mask;
	if (depth < 15 || red_mask == 0 || green_mask == 0 || blue_mask == 0){
		XCloseDisplay(global_x_info->display);
		Host_Crash("Not supported X window system");
	}
	global_x_info->default_depth = depth;
	global_x_info->sys_pixmap_format = pix_format_undefined;
	switch (global_x_info->default_depth){
		case 15:
			global_x_info->bpp = 16;
			if(red_mask = 0x7C00 && green_mask == 0x3E0 && blue_mask == 0x1F)
				global_x_info->sys_pixmap_format = pix_format_bgr555;
			break;
		case 16:
			global_x_info->bpp = 16;
			if(red_mask = 0xF800 && green_mask == 0x7E0 && blue_mask == 0x1F)
				global_x_info->sys_pixmap_format = pix_format_bgr565;
			break;
		case 24:
		case 32:
			global_x_info->bpp = 32;
			if (red_mask = 0xFF0000 && green_mask == 0xFF00 && blue_mask == 0xFF)
				global_x_info->sys_pixmap_format = pix_format_bgra32;
			else if (blue_mask = 0xFF0000 && green_mask == 0xFF00 && red_mask == 0xFF)
				global_x_info->sys_pixmap_format = pix_format_rgba32;
			break;
		defaut:
			break;
	}

	if (global_x_info->sys_pixmap_format == pix_format_undefined) {
		Host_Crash("System Pixmap format couldn't be determined");
	}
}

/***********************************************************************
**
*/	void OS_Update_Window(REBGOB *gob)
/*
**		Update window parameters.
**
***********************************************************************/
{
	RL_Print("updating window:");
	REBINT x = GOB_LOG_X_INT(gob);
	REBINT y = GOB_LOG_Y_INT(gob);
	REBINT w = GOB_LOG_W_INT(gob);
	REBINT h = GOB_LOG_H_INT(gob);
	RL_Print("x: %d, y: %d, width: %d, height: %d\n", x, y, w, h);
	host_window_t *window = GOB_HWIN(gob);
	Resize_Window(gob, FALSE);
	if (x != GOB_XO_INT(gob) || y != GOB_YO_INT(gob)){
		XMoveWindow(global_x_info->display, window->x_window, x, y);
	}
}

/***********************************************************************
**
*/  void* OS_Open_Window(REBGOB *gob)
/*
**		Initialize the graphics window.
**
**		The window handle is returned, but not expected to be used
**		other than for debugging conditions.
**
***********************************************************************/
{
	REBINT windex;
	REBINT x = GOB_LOG_X_INT(gob);
	REBINT y = GOB_LOG_Y_INT(gob);
	REBINT w = GOB_LOG_W_INT(gob);
	REBINT h = GOB_LOG_H_INT(gob);

	REBCHR *title;
	REBYTE os_string = FALSE;

	Window window;
	int screen_num;
	u32 mask = 0;
	u32 values[6];
	//xcb_drawable_t d;
	
	Display *display = global_x_info->display;
	Window root;
	XSetWindowAttributes swa;

	host_window_t *reb_host_window;

	RL_Print("x: %d, y: %d, width: %d, height: %d\n", x, y, w, h);

	root = DefaultRootWindow(display);
	swa.event_mask  =  ExposureMask | PointerMotionMask | KeyPressMask | KeyReleaseMask| ButtonPressMask |ButtonReleaseMask | StructureNotifyMask | FocusChangeMask;

	window = XCreateWindow(display, 
						   root,
						   x, y, w, h,
						   REB_WINDOW_BORDER_WIDTH,
						   CopyFromParent, InputOutput,
						   CopyFromParent, CWEventMask,
						   &swa);

	if (IS_GOB_STRING(gob))
        os_string = As_OS_Str(GOB_CONTENT(gob), (REBCHR**)&title);
    else
        title = TXT("REBOL Window");

	XTextProperty title_prop;
	Atom title_atom = XInternAtom(display, "_NET_WM_NAME", False);
	XmbTextListToTextProperty(display, (char **)&title, 1, XUTF8StringStyle, &title_prop);
	XSetTextProperty(display, window, &title_prop, title_atom);
	XStoreName(display, window, title); //backup for non NET Wms

	XClassHint *class_hint = XAllocClassHint();
	if (class_hint) {
		class_hint->res_name = title;
		class_hint->res_class = title;
		int status = XSetClassHint(display, window, class_hint);
		if (status != Success) {
			RL_Print("Failed to set class hint: %d", status);
		}
		XFree(class_hint);
	}

	if (os_string)
		OS_Free(title);

	Atom wmDelete=XInternAtom(display, "WM_DELETE_WINDOW", 1);
	XSetWMProtocols(display, window, &wmDelete, 1);

	Atom window_type_atom = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
	Atom window_type;
	if (GET_FLAGS(gob->flags, GOBF_NO_TITLE, GOBF_NO_BORDER)) {
		window_type = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU", False);
	} else {
		if (GET_GOB_FLAG(gob, GOBF_MODAL)) {
			Atom wm_state = XInternAtom(display, "_NET_WM_STATE", True);
			Atom wm_state_modal = XInternAtom(display, "_NET_WM_STATE_MODAL", True);
			host_window_t *parent_win = GOB_HWIN(GOB_TMP_OWNER(gob));
			XSetTransientForHint(display, window, parent_win->x_window);
			int status = XChangeProperty(display, window, wm_state, XA_ATOM, 32,
										 PropModeReplace, (unsigned char*)&wm_state_modal, 1);
			window_type = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DIALOG", False);
		} else {
			window_type = XInternAtom(display, "_NET_WM_WINDOW_TYPE_NORMAL", False);
		}
	}
	XChangeProperty(display, window, window_type_atom, XA_ATOM, 32,
					PropModeReplace,
					(unsigned char *)&window_type, 1); 

	Atom wm_allowed_action = XInternAtom(display, "_NET_WM_ALLOWED_ACTIONS", True);
	if (GET_GOB_FLAG(gob, GOBF_RESIZE)) {
		//RL_Print("Resizable\n");
		Atom wm_actions[] = {
			XInternAtom(display, "_NET_WM_ACTION_MOVE", True),
			XInternAtom(display, "_NET_WM_ACTION_RESIZE", True),
			XInternAtom(display, "_NET_WM_ACTION_CLOSE", True)
		};
		XChangeProperty(display, window, wm_allowed_action, XA_ATOM, 32,
						PropModeReplace, (unsigned char*)&wm_actions[0],
						sizeof(wm_actions)/sizeof(wm_actions[0]));
	} else {
		//RL_Print("Non-Resizable\n");
		Atom wm_actions[] = {
			XInternAtom(display, "_NET_WM_ACTION_MOVE", True),
			XInternAtom(display, "_NET_WM_ACTION_CLOSE", True)
		};
		XChangeProperty(display, window, wm_allowed_action, XA_ATOM, 32,
						PropModeReplace, (unsigned char*)&wm_actions[0],
						sizeof(wm_actions)/sizeof(wm_actions[0])); /* FIXME, this didn't work */

		XSizeHints *size_hints = XAllocSizeHints ();
		if (size_hints) {
			//RL_Print("Setting normal size hints %dx%d\n", w, h);
			size_hints->min_width = size_hints->max_width = w;
			size_hints->min_height = size_hints->max_height = h;
			size_hints->flags = PMinSize | PMaxSize;
			XSetWMNormalHints(display, window, size_hints);
			XFree(size_hints);
		}
	}

	XMapWindow(display, window);

	windex = Alloc_Window(gob);

	if (windex < 0) Host_Crash("Too many windows");

	GC gc = XCreateGC(display, window, 0, 0);
	screen_num = DefaultScreen(display);
	unsigned long black = BlackPixel(display, screen_num);
	unsigned long white = WhitePixel(display, screen_num);
	XSetBackground(display, gc, white);
	XSetForeground(display, gc, black);

	host_window_t *ew = OS_Make(sizeof(host_window_t));
	memset(ew, 0, sizeof(host_window_t));
	ew->x_window = window;
	ew->x_gc = gc;
	Gob_Windows[windex].win = ew;
	Gob_Windows[windex].compositor = rebcmp_create(Gob_Root, gob);

	CLEAR_GOB_STATE(gob);
	SET_GOB_STATE(gob, GOBS_NEW);

	SET_GOB_FLAG(gob, GOBF_WINDOW);
	SET_GOB_FLAG(gob, GOBF_ACTIVE);	
	SET_GOB_STATE(gob, GOBS_OPEN);

	return ew;
}

/***********************************************************************
**
*/  void OS_Close_Window(REBGOB *gob)
/*
**		Close the window.
**
***********************************************************************/
{
	RL_Print("Closing %x\n", gob);
	if (GET_GOB_FLAG(gob, GOBF_WINDOW)) {
		host_window_t *win = GOB_HWIN(gob);
		if (win != NULL){
			XDestroyImage(win->x_image); //frees win->pixbuf as well
			XFreeGC(global_x_info->display, win->x_gc);
			XUnmapWindow(global_x_info->display, win->x_window);
			XDestroyWindow(global_x_info->display, win->x_window);
			OS_Free(win);

			Free_Window(gob);
		}
	}
}