/* 
 * ckMain.c --
 *
 *	This file contains a generic main program for Ck-based applications.
 *	It can be used as-is for many applications, just by supplying a
 *	different appInitProc procedure for each specific application.
 *	Or, it can be used as a template for creating new main programs
 *	for applications.
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 * Copyright (c) 1995 Christian Werner
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "ckPort.h"
#include "ck.h"

/*
 * Global variables used by the main program:
 */

static Tcl_Interp *interp;	/* Interpreter for this application. */
static char *fileName = NULL;	/* Script to source, if any. */

#ifdef TCL_MEM_DEBUG
static char dumpFile[100];      /* Records where to dump memory allocation
                                 * information. */
static int quitFlag = 0;        /* 1 means the "checkmem" command was
                                 * invoked, so the application should quit
                                 * and dump memory allocation information. */
static int	CheckmemCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char *argv[]));
#endif

/*
 *----------------------------------------------------------------------
 *
 * Ck_Main --
 *
 *	Main program for curses wish.
 *
 * Results:
 *	None. This procedure never returns (it exits the process when
 *	it's done.
 *
 * Side effects:
 *	This procedure initializes the toolkit and then starts
 *	interpreting commands;  almost anything could happen, depending
 *	on the script being interpreted.
 *
 *----------------------------------------------------------------------
 */

void
Ck_Main(argc, argv, appInitProc)
    int argc;				/* Number of arguments. */
    char **argv;			/* Array of argument strings. */
    int (*appInitProc)();               /* Application-specific initialization
					 * procedure to call after most
					 * initialization but before starting
					 * to execute commands. */
{
    char *args, *msg, *argv0;
    char buf[20];
    int code;
    Tcl_Channel errChannel;

    Tcl_FindExecutable(argv[0]);

    interp = Tcl_CreateInterp();

#ifndef __WIN32__
    if (!isatty(0) || !isatty(1)) {
	errChannel = Tcl_GetStdChannel(TCL_STDERR);
	if (errChannel)
	    Tcl_Write(errChannel,
		"standard input/output must be terminal\n", -1);
	Tcl_Eval(interp, "exit 1");
	Tcl_Exit(1);
	exit(1);    /* Just in case */
#endif
    }

#ifdef TCL_MEM_DEBUG
    Tcl_InitMemory(interp);
    Tcl_CreateCommand(interp, "checkmem", CheckmemCmd, (ClientData) 0,
            (Tcl_CmdDeleteProc *) NULL);
#endif

    /*
     * Parse command-line arguments. Argv[1] must contain the name
     * of the script file to process.
     */

    argv0 = argv[0];
    if (argc > 1) {
        fileName = argv[1];
        argc--;
        argv++;
    }

    /*
     * Make command-line arguments available in the Tcl variables "argc"
     * and "argv".
     */

    args = Tcl_Merge(argc-1, argv+1);
    Tcl_SetVar(interp, "argv", args, TCL_GLOBAL_ONLY);
    ckfree(args);
    sprintf(buf, "%d", argc-1);
    Tcl_SetVar(interp, "argc", buf, TCL_GLOBAL_ONLY);
    Tcl_SetVar(interp, "argv0", (fileName != NULL) ? fileName : argv0,
	TCL_GLOBAL_ONLY);
    Tcl_SetVar(interp, "tcl_interactive", (fileName == NULL) ? "1" : "0",
	TCL_GLOBAL_ONLY);

    /*
     * Invoke application-specific initialization.
     */

    if ((*appInitProc)(interp) != TCL_OK) {
	errChannel = Tcl_GetStdChannel(TCL_STDERR);
	if (errChannel) {
	    Tcl_Write(errChannel,
		"application-specific initialization failed: ", -1);
	    Tcl_Write(errChannel, Tcl_GetString(Tcl_GetObjResult(interp)), -1);
	    Tcl_Write(errChannel, "\n", 1);
	}
	msg = Tcl_GetVar(interp, "errorInfo", TCL_GLOBAL_ONLY);
	goto errorExit;
    }

    /*
     * Invoke the script specified on the command line, if any.
     */
 
    if (fileName != NULL) {
	code = Tcl_VarEval(interp, "source ", fileName, (char *) NULL);
	if (code != TCL_OK)
	    goto error;
	Tcl_ResetResult(interp);
	goto mainLoop;
    }

    /*
     * We're running interactively.  Source a user-specific startup
     * file if the application specified one and if the file exists.
     */

    fileName = Tcl_GetVar(interp, "tcl_rcFileName", TCL_GLOBAL_ONLY);
    if (fileName != NULL) {
        Tcl_Channel c;
	Tcl_DString temp;
        char *fullName;

        Tcl_DStringInit(&temp);
        fullName = Tcl_TranslateFileName(interp, fileName, &temp);
        if (fullName == NULL) {
            errChannel = Tcl_GetStdChannel(TCL_STDERR);
            if (errChannel) {
	      Tcl_Write(errChannel, Tcl_GetString(Tcl_GetObjResult(interp)), -1);
                Tcl_Write(errChannel, "\n", 1);
            }
        } else {

	    /*
	     * Test for the existence of the rc file before trying to read it.
	     */

	    c = Tcl_OpenFileChannel(NULL, fullName, "r", 0);
            if (c != (Tcl_Channel) NULL) {
                Tcl_Close(NULL, c);
                if (Tcl_EvalFile(interp, fullName) != TCL_OK) {
                    errChannel = Tcl_GetStdChannel(TCL_STDERR);
                    if (errChannel) {
                        Tcl_Write(errChannel, Tcl_GetString(Tcl_GetObjResult(interp)), -1);
                        Tcl_Write(errChannel, "\n", 1);
                    }
                }
	    }
	    Tcl_DStringFree(&temp);
	}
    }

mainLoop:
    /*
     * Loop infinitely, waiting for commands to execute.
     */

#ifdef TCL_MEM_DEBUG
    Tcl_Eval(interp, "proc exit {{code 0}} {destroy .}");
#endif

    Ck_MainLoop();

#ifdef TCL_MEM_DEBUG
    if (quitFlag) {
	Tcl_DeleteInterp(interp);
	Tcl_DumpActiveMemory(dumpFile);
    }
#endif

    /*
     * Invoke Tcl exit command.
     */

    Tcl_Eval(interp, "after idle exit");
    Tcl_Exit(1);

error:
    msg = Tcl_GetVar(interp, "errorInfo", TCL_GLOBAL_ONLY);
    if (msg == NULL) {
	msg = Tcl_GetString(Tcl_GetObjResult(interp));
    }
errorExit:
    if (msg != NULL) {
	errChannel = Tcl_GetStdChannel(TCL_STDERR);
	if (errChannel) {
	    Tcl_Write(errChannel, msg, -1);
	    Tcl_Write(errChannel, "\n", 1);
	}
    }
    Tcl_Eval(interp, "after idle {exit 1}");
    Tcl_Exit(1);
}

/*
 *----------------------------------------------------------------------
 *
 * CheckmemCmd --
 *
 *	This is the command procedure for the "checkmem" command, which
 *	causes the application to exit after printing information about
 *	memory usage to the file passed to this command as its first
 *	argument.
 *
 * Results:
 *	Returns a standard Tcl completion code.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
#ifdef TCL_MEM_DEBUG

static int
CheckmemCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Not used. */
    Tcl_Interp *interp;			/* Interpreter for evaluation. */
    int argc;				/* Number of arguments. */
    char *argv[];			/* String values of arguments. */
{
    if (argc != 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" fileName\"", (char *) NULL);
	return TCL_ERROR;
    }
    strcpy(dumpFile, argv[1]);
    quitFlag = 1;
    return TCL_OK;
}
#endif

