/***********************************************************************
**
**  REBOL [R3] Language Interpreter and Run-time Environment
**
**  Copyright 2012 REBOL Technologies
**  REBOL is a trademark of REBOL Technologies
**
**  Additional code modifications and improvements Copyright 2012 Saphirion AG
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
**	Title: Host environment main entry point
**	Note: OS independent
**  Author: Carl Sassenrath
**  Purpose:
**		Provides the outer environment that calls the REBOL lib.
**		This module is more or less just an example and includes
**		a very simple console prompt.
**
************************************************************************
**
**  WARNING to PROGRAMMERS:
**
**		This open source code is strictly managed to maintain
**		source consistency according to our standards, not yours.
**
**		1. Keep code clear and simple.
**		2. Document odd code, your reasoning, or gotchas.
**		3. Use our source style for code, indentation, comments, etc.
**		4. It must work on Win32, Linux, OS X, BSD, big/little endian.
**		5. Test your code really well before submitting it.
**
***********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#ifdef _WIN32
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#include <windows.h>
#endif

#include "reb-host.h"		// standard host include files
#include "host-table.inc"

#ifdef CUSTOM_STARTUP
#include "host-init.h"
#endif

/**********************************************************************/

REBARGS Main_Args;

#ifdef TO_WINDOWS
HINSTANCE App_Instance = 0;
#endif

#ifndef REB_CORE
extern void Init_Windows(void);
extern void OS_Init_Graphics(void);
extern void OS_Destroy_Graphics(void);
#endif

extern void Init_Core_Ext(void);

//#define TEST_EXTENSIONS
#ifdef TEST_EXTENSIONS
extern void Init_Ext_Test(void);	// see: host-ext-test.c
#endif

// Host bare-bones stdio functs:
extern void Open_StdIO(void);
extern void Put_Str(REBYTE *buf);
extern REBYTE *Get_Str();

/* coverity[+kill] */
void Host_Crash(const char *reason) {
	OS_Crash(cb_cast("REBOL Host Failure"), cb_cast(reason));
}


/***********************************************************************
**
**  MAIN ENTRY POINT
**
**	Win32 args:
**		inst:  current instance of the application (app handle)
**		prior: always NULL (use a mutex for single inst of app)
**		cmd:   command line string (or use GetCommandLine)
**	    show:  how app window is to be shown (e.g. maximize, minimize, etc.)
**
**	Win32 return:
**		If the function succeeds, terminating when it receives a WM_QUIT
**		message, it should return the exit value contained in that
**		message's wParam parameter. If the function terminates before
**		entering the message loop, it should return zero.
**
**  Posix args: as you would expect in C.
**  Posix return: ditto.
**
***********************************************************************/

// Using a main entry point for a console program (as opposed to WinMain)
// so that we can connect to the console.  See the StackOverflow question
// "Can one executable be both a console and a GUI application":
//
//     http://stackoverflow.com/questions/493536/
//
// int WINAPI WinMain(HINSTANCE inst, HINSTANCE prior, LPSTR cmd, int show)

int main(int argc, char **argv_ansi)
{
	REBYTE vers[8];
	REBYTE *line;
	REBINT err_num;
	REBYTE *embedded_script = NULL;
	REBI64 embedded_size = 0;

	REBCHR **argv;

	// As defined, Put_Str takes non-const data
	REBYTE prompt_str[] = ">> ";
	REBYTE result_str[] = "== ";

#ifdef TO_WINDOWS
	// Were we using WinMain we'd be getting our arguments in Unicode, but
	// since we're using an ordinary main() we do not.  However, this call
	// lets us slip out and pick up the arguments in Unicode form.
	argv = CommandLineToArgvW(GetCommandLineW(), &argc);
#else
	// Assume no wide character support, and just take the ANSI C args
	argv = argv_ansi;
#endif

	Host_Lib = &Host_Lib_Init;

	embedded_script = OS_Read_Embedded(&embedded_size);
	Parse_Args(argc, argv, &Main_Args);

	vers[0] = 5; // len
	RL_Version(&vers[0]);

	// Must be done before an console I/O can occur. Does not use reb-lib,
	// so this device should open even if there are other problems.
	Open_StdIO();  // also sets up interrupt handler

	// Initialize the REBOL library (reb-lib):
	if (!CHECK_STRUCT_ALIGN) Host_Crash("Incompatible struct alignment");
	if (!Host_Lib) Host_Crash("Missing host lib");
	// !!! Second part will become vers[2] < RL_REV on release!!!
	if (vers[1] != RL_VER || vers[2] != RL_REV) Host_Crash("Incompatible reb-lib DLL");
	err_num = RL_Init(&Main_Args, Host_Lib);
	if (err_num == 1) Host_Crash("Host-lib wrong size");
	if (err_num == 2) Host_Crash("Host-lib wrong version/checksum");

	//Initialize core extension commands
	Init_Core_Ext();
#ifdef EXT_LICENSING
	Init_Licensing_Ext();
#endif //EXT_LICENSING

#ifdef TEST_EXTENSIONS
	Init_Ext_Test();
#endif

#ifdef TO_WINDOWS
	// no console, we must be the child process
	if (GetStdHandle(STD_OUTPUT_HANDLE) == 0)
	{
		App_Instance = GetModuleHandle(NULL);
	}
#ifdef REB_CORE
	else //use always the console for R3/core
	{
		// GetWindowsLongPtr support 32 & 64 bit windows
		App_Instance = (HINSTANCE)GetWindowLongPtr(GetConsoleWindow(), GWLP_HINSTANCE);
	}
#else
	//followinng R3/view code behaviors when compiled as:
	//-"console app" mode: stdio redirection works but blinking console window during start
	//-"GUI app" mode stdio redirection doesn't work properly, no blinking console window during start
	else if (argc > 1) // we have command line args
	{
		// GetWindowsLongPtr support 32 & 64 bit windows
		App_Instance = (HINSTANCE)GetWindowLongPtr(GetConsoleWindow(), GWLP_HINSTANCE);
	}
	else // no command line args but a console - launch child process so GUI is initialized and exit
	{
		DWORD dwCreationFlags = CREATE_DEFAULT_ERROR_MODE | DETACHED_PROCESS;
		STARTUPINFO startinfo;
		PROCESS_INFORMATION procinfo;
		ZeroMemory(&startinfo, sizeof(startinfo));
		startinfo.cb = sizeof(startinfo);
		if (!CreateProcess(NULL, argv[0], NULL, NULL, FALSE, dwCreationFlags, NULL, NULL, &startinfo, &procinfo))
			MessageBox(0, L"CreateProcess() failed :(", L"", 0);
		exit(0);
	}
#endif //REB_CORE
#endif //TO_WINDOWS

	// Common code for console & GUI version
#ifndef REB_CORE
	Init_Windows();
	OS_Init_Graphics();
#endif // REB_CORE

	// Call sys/start function. If a compressed script is provided, it will be
	// decompressed, stored in system/options/boot-host, loaded, and evaluated.
	// Returns: 0: ok, -1: error, 1: bad data.
#ifdef CUSTOM_STARTUP
	// For custom startup, you can provide compressed script code here:
	err_num = RL_Start(
		&Reb_Init_Code[0], REB_INIT_SIZE,
		embedded_script, embedded_size, 0
	);
#else
	err_num = RL_Start(0, 0, embedded_script, embedded_size, 0);
#endif

#if !defined(ENCAP)
	// !!! What should an encapped executable do with a --do?  Here we just
	// ignore it, as the assumption is that it is a packaged system that
	// doesn't necessarily want to present itself as an arbitrary interpreter

	// Previously this command line option was handled by the Rebol Core
	// itself, in Mezzanine initialization.  However, Ren/C is catering to
	// needs of other kinds of clients.  So rather than having those clients
	// figure out how to send Rebol a "--do" option in a "command line
	// arguments buffer", it is turned the other way so that if something
	// does have a command line it needs to call APIs to run them.  This
	// "pulled out" piece of command line processing uses the RL_Api still,
	// with RL_Do_String (more options will be available with Ren/C proper)

	// !!! NOTE: Encapping needs to be thought of similarly; it is not a
	// Ren/C feature, rather a feature that some client (e.g. a console
	// client named "Rebol") would implement.

	// !!! The command line processing tells us if we have just '--do' with
	// nothing afterward by setting do_arg to NULL.  When all the command
	// line processing is taken out of Ren/C's concern that kind of decision
	// can be revisited.  In the meantime, we test for NULL.

	if (err_num >= 0 && (Main_Args.options & RO_DO) && Main_Args.do_arg) {
		RXIARG result;
		REBYTE *do_arg_utf8;
		REBCNT len_predicted;
		REBCNT len_encoded;

		// On Windows, do_arg is a REBCHR*.  We need to get it into UTF8.
		// !!! Better helpers needed than this; Ren/C can call host's OS_ALLOC
		// so this should be more seamless.
	#ifdef TO_WINDOWS
		len_predicted = RL_Length_As_UTF8(
			Main_Args.do_arg, wcslen(Main_Args.do_arg), TRUE, TRUE
		);
		do_arg_utf8 = OS_ALLOC_ARRAY(REBYTE, len_predicted + 1);
		len_encoded = len_predicted;
		RL_Encode_UTF8(
			do_arg_utf8,
			len_predicted + 1,
			Main_Args.do_arg,
			&len_encoded,
			TRUE,
			TRUE
		);

		// Sanity check; we shouldn't get a different answer.
		assert(len_predicted == len_encoded);

		// Encoding doesn't NULL-terminate on its own.
		do_arg_utf8[len_encoded] = '\0';
	#else
		do_arg_utf8 = b_cast(Main_Args.do_arg);
	#endif

		RL_Do_String(do_arg_utf8, 0, &result);

	#ifdef TO_WINDOWS
		OS_FREE(do_arg_utf8);
	#endif

		// We ignore the result; and only ask for it to help prevent an
		// item from being put on the stack that we'd have to print out.
		// (there is no RL_DS_DROP, while Ren/C would just use DS_DROP)

		// The above may request a QUIT, and thus bubble out to the topmost
		// Rebol handler.  Or it may have some kind of error.  We lack
		// any way here to tell if there was an error.  While quitting
		// is not necessarily ideal, it was the previous behavior when
		// a --do was provided.  However it would be nice if we could
		// have the option of examining the crash state.

		// !!! Under RenC, this client would be able to catch the error
		// itself and cleanly shut down the system (if it wished).

		RL_Do_String(cb_cast("quit"), 0, 0);
	}

	// Console line input loop (just an example, can be improved):
	if (
		!(Main_Args.options & RO_CGI)
		&& (
			!Main_Args.script // no script was provided
			|| err_num < 0         // script halted or had error
			|| Main_Args.options & RO_HALT  // --halt option
		)
	){
		err_num = 0;  // reset error code (but should be able to set it below too!)
		while (TRUE) {
			Put_Str(prompt_str);
			if ((line = Get_Str())) {
				RL_Do_String(line, 0, 0);
				RL_Print_TOS(0, result_str);
				OS_FREE(line);
			}
			else break; // EOS
		}
	}
#endif //!ENCAP
	OS_Quit_Devices(0);
#ifndef REB_CORE
	OS_Destroy_Graphics();
#endif

	// A QUIT does not exit this way, so the only valid return code is zero.
	return 0;
}

