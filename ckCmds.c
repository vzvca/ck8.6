/* 
 * ckCmds.c --
 *
 *	This file contains a collection of Ck-related Tcl commands
 *	that didn't fit in any particular file of the toolkit.
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

static char *     WaitVariableProc _ANSI_ARGS_((ClientData clientData,
		      Tcl_Interp *interp, char *name1, char *name2,
                      int flags));
static void       WaitVisibilityProc _ANSI_ARGS_((ClientData clientData,
                      CkEvent *eventPtr));
static void       WaitWindowProc _ANSI_ARGS_((ClientData clientData,
                      CkEvent *eventPtr));


/*
 *----------------------------------------------------------------------
 *
 * Ck_DestroyCmdObj --
 *
 *	This procedure is invoked to process the "destroy" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Ck_DestroyCmdObj(clientData, interp, objc, objv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj* CONST objv[];      /* Tcl_Obj* array of arguments. */
{
    CkWindow *winPtr;
    CkWindow *mainPtr = (CkWindow *) clientData;
    int i;

    for (i = 1; i < objc; i++) {
      winPtr = Ck_NameToWindow(interp, Tcl_GetString(objv[i]), mainPtr);
	if (winPtr == NULL)
	    return TCL_ERROR;
	Ck_DestroyWindow(winPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_ExitCmdObj --
 *
 *	This procedure is invoked to process the "exit" Tcl command.
 *	See the user documentation for details on what it does.
 *	Note: this command replaces the Tcl "exit" command in order
 *	to properly destroy all windows.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Ck_ExitCmdObj(clientData, interp, objc, objv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj* CONST objv[];      /* Tcl_Obj* array of arguments. */
{
    extern CkMainInfo *ckMainInfo;
    int index = 1, noclear = 0, value = 0;

    if (objc > 3) {
      Tcl_WrongNumArgs(interp, 1, objv, "?-noclear? ?returnCode?");
      return TCL_ERROR;
    }
    if (objc > 1 && strcmp(Tcl_GetString(objv[1]), "-noclear") == 0) {
	index++;
	noclear++;
    }
    if (objc > index &&
	Tcl_GetIntFromObj(interp, objv[index], &value) != TCL_OK) {
	return TCL_ERROR;
    }

    if (ckMainInfo != NULL) {
      if (noclear) {
	ckMainInfo->flags |= CK_NOCLR_ON_EXIT;
      } else {
	ckMainInfo->flags &= ~CK_NOCLR_ON_EXIT;
      }
      Ck_DestroyWindow((CkWindow *) clientData);
    }
    CkpEndMouse();
    endwin();	/* just in case */
    Tcl_Exit(value);
    /* NOTREACHED */
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_LowerCmdObj --
 *
 *	This procedure is invoked to process the "lower" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Ck_LowerCmdObj(clientData, interp, objc, objv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj* CONST objv[];      /* Tcl_Obj* array of arguments. */
{
    CkWindow *mainPtr = (CkWindow *) clientData;
    CkWindow *winPtr, *other;

    if ((objc != 2) && (objc != 3)) {
      Tcl_WrongNumArgs(interp, 1, objv, "window ?belowThis?");
	return TCL_ERROR;
    }

    winPtr = Ck_NameToWindow(interp, Tcl_GetString(objv[1]), mainPtr);
    if (winPtr == NULL)
	return TCL_ERROR;
    if (objc == 2)
	other = NULL;
    else {
      other = Ck_NameToWindow(interp, Tcl_GetString(objv[2]), mainPtr);
      if (other == NULL)
	return TCL_ERROR;
    }
    if (Ck_RestackWindow(winPtr, CK_BELOW, other) != TCL_OK) {
      Tcl_AppendResult(interp,
		       "can't lower \"", Tcl_GetString(objv[1]),
		       "\" below \"", Tcl_GetString(objv[2]), "\"",
		       (char *) NULL);
      return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_RaiseCmdObj --
 *
 *	This procedure is invoked to process the "raise" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Ck_RaiseCmdObj(clientData, interp, objc, objv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj* CONST objv[];      /* Tcl_Obj* array of arguments. */
{
    CkWindow *mainPtr = (CkWindow *) clientData;
    CkWindow *winPtr, *other;

    if ((objc != 2) && (objc != 3)) {
      Tcl_WrongNumArgs(interp, 1, objv, "window ?aboveThis?");
      return TCL_ERROR;
    }

    winPtr = Ck_NameToWindow(interp, Tcl_GetString(objv[1]), mainPtr);
    if (winPtr == NULL)
	return TCL_ERROR;
    if (objc == 2)
	other = NULL;
    else {
      other = Ck_NameToWindow(interp, Tcl_GetString(objv[2]), mainPtr);
      if (other == NULL)
	return TCL_ERROR;
    }
    if (Ck_RestackWindow(winPtr, CK_ABOVE, other) != TCL_OK) {
      Tcl_AppendResult(interp,
		       "can't raise \"", Tcl_GetString(objv[1]),
		       "\" above \"", Tcl_GetString(objv[2]), "\"",
		       (char *) NULL);
      return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_BellCmdObj --
 *
 *	This procedure is invoked to process the "bell" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Ck_BellCmdObj(clientData, interp, objc, objv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj* CONST objv[];      /* Tcl_Obj* array of arguments. */
{
    beep();
    doupdate();
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_UpdateCmdObj --
 *
 *	This procedure is invoked to process the "update" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Ck_UpdateCmdObj(clientData, interp, objc, objv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj* CONST objv[];      /* Tcl_Obj* array of arguments. */
{
    CkWindow *mainPtr = (CkWindow *) clientData;
    int flags;

    if (objc == 1)
	flags = TK_DONT_WAIT;
    else if (objc == 2) {
      char *argv1 = Tcl_GetString(objv[1]);
      if (strncmp(argv1, "screen", strlen(argv1)) == 0) {
	wrefresh(curscr);
	Ck_EventuallyRefresh(mainPtr);
	return TCL_OK;
      }
      if (strncmp(argv1, "idletasks", strlen(argv1)) != 0) {
	Tcl_AppendResult(interp, "bad argument \"", argv1,
			 "\": must be idletasks or screen", (char *) NULL);
	return TCL_ERROR;
      }
	flags = TK_IDLE_EVENTS;
    } else {
      Tcl_WrongNumArgs(interp, 1, objv, "?idletasks|screen?");
      return TCL_ERROR;
    }

    /*
     * Handle all pending events, and repeat over and over
     * again until all pending events have been handled.
     */

    while (Tk_DoOneEvent(flags) != 0) {
	/* Empty loop body */
    }

    /*
     * Must clear the interpreter's result because event handlers could
     * have executed commands.
     */

    Tcl_ResetResult(interp);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_CursesCmd --
 *
 *	This procedure is invoked to process the "curses" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Ck_CursesCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    CkWindow *winPtr = (CkWindow *) clientData;
    CkMainInfo *mainPtr = winPtr->mainPtr;
    int length;
    char c;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
	    argv[0], " option ?arg?\"", (char *) NULL);
	return TCL_ERROR;
    }
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'b') && (strncmp(argv[1], "barcode", length) == 0)) {
	return CkBarcodeCmd(clientData, interp, argc, argv);
    } else if ((c == 'b') && (strncmp(argv[1], "baudrate", length) == 0)) {
	char buf[32];

	if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args: must be \"", argv[0],
		" ", argv[1], "\"", (char *) NULL);
	    return TCL_ERROR;
        }
	sprintf(buf, "%d", baudrate());
	Tcl_AppendResult(interp, buf, (char *) NULL);
	return TCL_OK;
    } else if ((c == 'e') && (strncmp(argv[1], "encoding", length) == 0)) {
	if (argc == 2)
	    return Ck_GetEncoding(interp);
	else if (argc == 3)
	    return Ck_SetEncoding(interp, argv[2]);
	else {
	    Tcl_AppendResult(interp, "wrong # args: must be \"", argv[0],
		" ", argv[1], " ?name?\"", (char *) NULL);
	    return TCL_ERROR;
        }
    } else if ((c == 'g') && (strncmp(argv[1], "gchar", length) == 0)) {
      char buf[64];
	long gchar;
	int gc;

	if (argc == 3) {
	    if (Ck_GetGChar(interp, argv[2], &gchar) != TCL_OK)
		return TCL_ERROR;
	    sprintf(buf, "%ld", gchar);
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(buf,-1));
	} else if (argc == 4) {
	    if (Tcl_GetInt(interp, argv[3], &gc) != TCL_OK)
		return TCL_ERROR;
	    gchar = gc;
	    if (Ck_SetGChar(interp, argv[2], gchar) != TCL_OK)
		return TCL_ERROR;
	} else {
	    Tcl_AppendResult(interp, "wrong # args: must be \"", argv[0],
		" ", argv[1], " charName ?value?\"", (char *) NULL);
	    return TCL_ERROR;
        }
    } else if ((c == 'h') && (strncmp(argv[1], "haskey", length) == 0)) {
	if (argc > 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " haskey ?keySym?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 2)
	    return CkAllKeyNames(interp);
	return CkTermHasKey(interp, argv[2]);
    } else if ((c == 'p') && (strncmp(argv[1], "purgeinput", length) == 0)) {
	if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " purgeinput\"", (char *) NULL);
	    return TCL_ERROR;
	}
	while (getch() != ERR) {
	    /* Empty loop body. */
	}
	return TCL_OK;
    } else if ((c == 'r') && (strncmp(argv[1], "refreshdelay", length) == 0)) {
	if (argc == 2) {
	    char buf[32];

	    sprintf(buf, "%d", mainPtr->refreshDelay);
	    Tcl_AppendResult(interp, buf, (char *) NULL);
	    return TCL_OK;
	} else if (argc == 3) {
	    int delay;

	    if (Tcl_GetInt(interp, argv[2], &delay) != TCL_OK)
		return TCL_ERROR;
	    mainPtr->refreshDelay = delay < 0 ? 0 : delay;
	    return TCL_OK;
	} else {
	    Tcl_AppendResult(interp, "wrong # args: must be \"", argv[0],
		" ", argv[1], " ?milliseconds?\"", (char *) NULL);
	    return TCL_ERROR;
	}
    } else if ((c == 'r') && (strncmp(argv[1], "reversekludge", length)
        == 0)) {
	int onoff;

	if (argc == 2) {
	  Tcl_SetObjResult(interp,
			   Tcl_NewStringObj((mainPtr->flags & CK_REVERSE_KLUDGE) ? "1" : "0", -1));
	} else if (argc == 3) {
	    if (Tcl_GetBoolean(interp, argv[2], &onoff) != TCL_OK)
		return TCL_ERROR;
	    mainPtr->flags |= CK_REVERSE_KLUDGE;
	} else {
	    Tcl_AppendResult(interp, "wrong # args: must be \"", argv[0],
		" ", argv[1], " ?bool?\"", (char *) NULL);
	    return TCL_ERROR;
        }
    } else if ((c == 's') && (strncmp(argv[1], "screendump", length) == 0)) {
	Tcl_DString buffer;
	char *fileName;
#ifdef HAVE_SCR_DUMP
	int ret;
#endif

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: must be \"", argv[0],
		" ", argv[1], " filename\"", (char *) NULL);
	    return TCL_ERROR;
	}
	fileName = Tcl_TildeSubst(interp, argv[2], &buffer);
	if (fileName == NULL) {
	    Tcl_DStringFree(&buffer);
	    return TCL_ERROR;
	}
#ifdef HAVE_SCR_DUMP
	ret = scr_dump(fileName);
	Tcl_DStringFree(&buffer);
	if (ret != OK) {
	  Tcl_SetObjResult(interp, Tcl_NewStringObj("screen dump failed",-1));
	    return TCL_ERROR;
	}
	return TCL_OK;
#else
	Tcl_SetObjResult(interp, Tcl_NewStringObj("screen dump not supported by this curses",-1));
	return TCL_ERROR;
#endif
    } else if ((c == 's') && (strncmp(argv[1], "suspend", length) == 0)) {
	if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args: must be \"", argv[0],
		" ", argv[1], "\"", (char *) NULL);
	    return TCL_ERROR;
	}
#ifndef __WIN32__
	curs_set(1);
	endwin();
#ifdef SIGTSTP
	kill(getpid(), SIGTSTP);
#else
	kill(getpid(), SIGSTOP);
#endif
	Ck_EventuallyRefresh(winPtr);
#endif
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
	    "\": must be barcode, baudrate, encoding, gchar, haskey, ",
	    "purgeinput, refreshdelay, reversekludge, screendump or suspend",
	    (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_CursesCmdObj --
 *
 *	This procedure is invoked to process the "curses" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Ck_CursesCmdObj(clientData, interp, objc, objv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj* CONST objv[];      /* Tcl_Obj* array of arguments. */
{
  static char *commands[] =
    {
     "barcode",
     "baudrate",
     "encoding",
     "gchar",
     "haskey",
     "purgeinput",
     "refreshdelay",
     "reversekludge",
     "screendump",
     "suspend",
     NULL
    };
  enum
  {
   CMD_BARCODE,
   CMD_BAUDRATE,
   CMD_ENCODING,
   CMD_GCHAR,
   CMD_HASKEY,
   CMD_PURGEINPUT,
   CMD_REFRESHDELAY,
   CMD_REVERSEKLUDGE,
   CMD_SCREENDUMP,
   CMD_SUSPEND
  };

  CkWindow *winPtr = (CkWindow *) clientData;
  CkMainInfo *mainPtr = winPtr->mainPtr;
  int index;
  

  if (objc < 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "option ?arg?");
    return TCL_ERROR;
  }
  if (Tcl_GetIndexFromObj( interp, objv[1], commands, "option", TCL_EXACT, &index) != TCL_OK) {
    return TCL_ERROR;
  }

  switch(index) {
  case CMD_BARCODE:
    return CkBarcodeCmdObj(clientData, interp, objc, objv);

  case CMD_BAUDRATE:
    {
      if (objc != 2) {
	Tcl_WrongNumArgs(interp, 2, objv, "");
	return TCL_ERROR;
      }
      Tcl_SetObjResult( interp, Tcl_NewIntObj(baudrate()));
      return TCL_OK;
    }
    break;
  case CMD_ENCODING:
    {
      if (objc == 2) {
	return Ck_GetEncoding(interp);
      }
      else if (objc == 3) {
	return Ck_SetEncoding(interp, Tcl_GetString(objv[2]));
      }
      else {
	Tcl_WrongNumArgs(interp, 2, objv, "?name?");
	return TCL_ERROR;
      }
    }
    break;
  case CMD_GCHAR:
    {
      char buf[64];
      long gchar;
      int gc;

      if (objc == 3) {
	if (Ck_GetGChar(interp, Tcl_GetString(objv[2]), &gchar) != TCL_OK) {
	  return TCL_ERROR;
	}
	Tcl_SetObjResult( interp, Tcl_NewLongObj(gchar));
      }
      else if (objc == 4) {
	if (Tcl_GetIntFromObj(interp, objv[3], &gc) != TCL_OK) {
	  return TCL_ERROR;
	}
	gchar = gc;
	if (Ck_SetGChar(interp, Tcl_GetString(objv[2]), gchar) != TCL_OK) {
	  return TCL_ERROR;
	}
      }
      else {
	Tcl_WrongNumArgs( interp, 2, objv, "charName ?value?");
	return TCL_ERROR;
      }
    }
    break;
  case CMD_HASKEY:
    {
	if (objc > 3) {
	  Tcl_WrongNumArgs(interp, 2, objv, "?keySym?");
	  return TCL_ERROR;
	}
	if (objc == 2) {
	    return CkAllKeyNames(interp);
	}
	return CkTermHasKey(interp, Tcl_GetString(objv[2]));
    }
    break;
  case CMD_PURGEINPUT:
    {
	if (objc != 2) {
	  Tcl_WrongNumArgs( interp, 2, objv, "");
	  return TCL_ERROR;
	}
	while (getch() != ERR) {
	  /* Empty loop body. */
	}
	return TCL_OK;
    }
    break;
  case CMD_REFRESHDELAY:
    {
	if (objc == 2) {
	  Tcl_SetObjResult( interp, Tcl_NewIntObj(mainPtr->refreshDelay));
	  return TCL_OK;
	}
	else if (objc == 3) {
	    int delay;

	    if (Tcl_GetIntFromObj(interp, objv[2], &delay) != TCL_OK) {
		return TCL_ERROR;
	    }
	    mainPtr->refreshDelay = delay < 0 ? 0 : delay;
	    return TCL_OK;
	}
	else {
	  Tcl_WrongNumArgs( interp, 2, objv, "?milliseconds?");
	  return TCL_ERROR;
	}
    }
    break;
  case CMD_REVERSEKLUDGE:
    {
	int onoff;

	if (objc == 2) {
	  Tcl_SetObjResult(interp, Tcl_NewIntObj(!!(mainPtr->flags & CK_REVERSE_KLUDGE)));
	}
	else if (objc == 3) {
	  if (Tcl_GetBooleanFromObj(interp, objv[2], &onoff) != TCL_OK) {
	    return TCL_ERROR;
	  }
	  mainPtr->flags |= CK_REVERSE_KLUDGE;
	}
	else {
	    Tcl_WrongNumArgs( interp, 2, objv, "?bool?");
	    return TCL_ERROR;
        }
    }
    break;
  case CMD_SCREENDUMP:
    {
	Tcl_DString buffer;
	char *fileName;
#ifdef HAVE_SCR_DUMP
	int ret;
#endif

	if (objc != 3) {
	  Tcl_WrongNumArgs( interp, 2, objv, "filename");
	  return TCL_ERROR;
	}
	Tcl_DStringInit(&buffer);
	fileName = Tcl_TildeSubst(interp, Tcl_GetString(objv[2]), &buffer);
	if (fileName == NULL) {
	    Tcl_DStringFree(&buffer);
	    return TCL_ERROR;
	}
#ifdef HAVE_SCR_DUMP
	ret = scr_dump(fileName);
	Tcl_DStringFree(&buffer);
	if (ret != OK) {
	  Tcl_SetObjResult(interp, Tcl_NewStringObj("screen dump failed",-1));
	  return TCL_ERROR;
	}
	return TCL_OK;
#else
	Tcl_SetObjResult(interp, Tcl_NewStringObj("screen dump not supported by this curses",-1));
	return TCL_ERROR;
#endif
    }
    break;
    case CMD_SUSPEND:
      {
	if (objc != 2) {
	  Tcl_WrongNumArgs( interp, 2, objv, "");
	  return TCL_ERROR;
	}
#ifndef __WIN32__
	curs_set(1);
	endwin();
#ifdef SIGTSTP
	kill(getpid(), SIGTSTP);
#else
	kill(getpid(), SIGSTOP);
#endif
	Ck_EventuallyRefresh(winPtr);
#endif
      }
      break;
  default:
    {
      /* -- should never be reached -- */
      Tcl_AppendResult(interp, "bad option \"", Tcl_GetString(objv[1]),
		       "\": must be barcode, baudrate, encoding, gchar, haskey, ",
		       "purgeinput, refreshdelay, reversekludge, screendump or suspend",
		       (char *) NULL);
      return TCL_ERROR;
    }
  }
  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_WinfoCmd --
 *
 *	This procedure is invoked to process the "winfo" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Ck_WinfoCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    CkWindow *mainPtr = (CkWindow *) clientData;
    int length;
    char c, *argName;
    CkWindow *winPtr;

#define SETUP(name) \
    if (argc != 3) {\
	argName = name; \
	goto wrongArgs; \
    } \
    winPtr = Ck_NameToWindow(interp, argv[2], mainPtr); \
    if (winPtr == NULL) { \
	return TCL_ERROR; \
    }


    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " option ?arg?\"", (char *) NULL);
	return TCL_ERROR;
    }
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'c') && (strncmp(argv[1], "children", length) == 0)
	    && (length >= 2)) {
	SETUP("children");
	for (winPtr = winPtr->childList; winPtr != NULL;
		winPtr = winPtr->nextPtr) {
	    Tcl_AppendElement(interp, winPtr->pathName);
	}
    } else if ((c == 'c') && (strncmp(argv[1], "containing", length) == 0)
	    && (length >= 2)) {
	int x, y;

	argName = "containing";
	if (argc != 4)
	    goto wrongArgs;
	if (Tcl_GetInt(interp, argv[2], &x) != TCL_OK ||
	    Tcl_GetInt(interp, argv[3], &y) != TCL_OK) {
	    return TCL_ERROR;
	}
	winPtr = Ck_GetWindowXY(mainPtr->mainPtr, &x, &y, 0);
	if (winPtr != NULL) {
	  Tcl_SetObjResult(interp, Tcl_NewStringObj(winPtr->pathName, -1));
	}
    } else if ((c == 'd') && (strncmp(argv[1], "depth", length) == 0)) {
	SETUP("depth");
	Tcl_SetObjResult(interp, Tcl_NewStringObj((winPtr->mainPtr->flags & CK_HAS_COLOR) ? "3" : "1", -1));
    } else if ((c == 'e') && (strncmp(argv[1], "exists", length) == 0)) {
	if (argc != 3) {
	    argName = "exists";
	    goto wrongArgs;
	}
	if (Ck_NameToWindow(interp, argv[2], mainPtr) == NULL) {
	  Tcl_SetObjResult(interp, Tcl_NewStringObj("0",-1));
	} else {
	  Tcl_SetObjResult(interp, Tcl_NewStringObj("1",-1));
	}
    } else if ((c == 'g') && (strncmp(argv[1], "geometry", length) == 0)) {
      char buf[128];
	SETUP("geometry");
	sprintf(buf, "%dx%d+%d+%d", winPtr->width, winPtr->height, winPtr->x, winPtr->y);
	Tcl_SetObjResult( interp, Tcl_NewStringObj(buf,-1));
    } else if ((c == 'h') && (strncmp(argv[1], "height", length) == 0)) {
      char buf[128];
	SETUP("height");
	sprintf(buf, "%d", winPtr->height);
	Tcl_SetObjResult( interp, Tcl_NewStringObj(buf,-1));
    } else if ((c == 'i') && (strncmp(argv[1], "ismapped", length) == 0)
	    && (length >= 2)) {
	SETUP("ismapped");
	Tcl_SetObjResult(interp, Tcl_NewStringObj((winPtr->flags & CK_MAPPED) ? "1" : "0", -1));
    } else if ((c == 'm') && (strncmp(argv[1], "manager", length) == 0)) {
	SETUP("manager");
	if (winPtr->geomMgrPtr != NULL)
	  Tcl_SetObjResult(interp, Tcl_NewStringObj(winPtr->geomMgrPtr->name,-1));
    } else if ((c == 'n') && (strncmp(argv[1], "name", length) == 0)) {
	SETUP("name");
	Tcl_SetObjResult(interp, Tcl_NewStringObj((char *) winPtr->nameUid,-1));
    } else if ((c == 'c') && (strncmp(argv[1], "class", length) == 0)) {
	SETUP("class");
	Tcl_SetObjResult(interp, Tcl_NewStringObj((char *) winPtr->classUid,-1));
    } else if ((c == 'p') && (strncmp(argv[1], "parent", length) == 0)) {
	SETUP("parent");
	if (winPtr->parentPtr != NULL)
	  Tcl_SetObjResult(interp, Tcl_NewStringObj(winPtr->parentPtr->pathName,-1));
    } else if ((c == 'r') && (strncmp(argv[1], "reqheight", length) == 0)
	    && (length >= 4)) {
      char buf[128];
	SETUP("reqheight");
	sprintf(buf, "%d", winPtr->reqHeight);
	Tcl_SetObjResult( interp, Tcl_NewStringObj(buf,-1));
    } else if ((c == 'r') && (strncmp(argv[1], "reqwidth", length) == 0)
	    && (length >= 4)) {
      char buf[128];
	SETUP("reqwidth");
	sprintf(buf, "%d", winPtr->reqWidth);
	Tcl_SetObjResult( interp, Tcl_NewStringObj(buf,-1));
    } else if ((c == 'r') && (strncmp(argv[1], "rootx", length) == 0)
	    && (length >= 4)) {
      char buf[128];
        int x;

	SETUP("rootx");
        Ck_GetRootGeometry(winPtr, &x, NULL, NULL, NULL);
	sprintf(buf, "%d", x);
	Tcl_SetObjResult( interp, Tcl_NewStringObj(buf,-1));
    } else if ((c == 'r') && (strncmp(argv[1], "rooty", length) == 0)
	    && (length >= 4)) {
      char buf[128];
	int y;

	SETUP("rooty");
	Ck_GetRootGeometry(winPtr, NULL, &y, NULL, NULL);
	sprintf(buf, "%d", y);
	Tcl_SetObjResult( interp, Tcl_NewStringObj(buf,-1));
    } else if ((c == 's') && (strncmp(argv[1], "screenheight", length) == 0)
	    && (length >= 7)) {
      char buf[128];
	SETUP("screenheight");
	sprintf(buf, "%d", winPtr->mainPtr->winPtr->height);
	Tcl_SetObjResult( interp, Tcl_NewStringObj(buf,-1));
    } else if ((c == 's') && (strncmp(argv[1], "screenwidth", length) == 0)
	    && (length >= 7)) {
      char buf[128];
	SETUP("screenwidth");
	sprintf(buf, "%d", winPtr->mainPtr->winPtr->width);
	Tcl_SetObjResult( interp, Tcl_NewStringObj(buf,-1));
    } else if ((c == 't') && (strncmp(argv[1], "toplevel", length) == 0)) {
        SETUP("toplevel");
	for (; winPtr != NULL; winPtr = winPtr->parentPtr) {
	    if (winPtr->flags & CK_TOPLEVEL) {
	      Tcl_SetObjResult( interp, Tcl_NewStringObj(winPtr->pathName,-1));
		break;
	    }
	}
    } else if ((c == 'w') && (strncmp(argv[1], "width", length) == 0)) {
      char buf[128];
	SETUP("width");
	sprintf(buf, "%d", winPtr->width);
	Tcl_SetObjResult( interp, Tcl_NewStringObj(buf,-1));
    } else if ((c == 'x') && (argv[1][1] == '\0')) {
      char buf[128];
	SETUP("x");
	sprintf(buf, "%d", winPtr->x);
	Tcl_SetObjResult( interp, Tcl_NewStringObj(buf,-1));
    } else if ((c == 'y') && (argv[1][1] == '\0')) {
      char buf[128];
	SETUP("y");
	sprintf(buf, "%d", winPtr->y);
	Tcl_SetObjResult( interp, Tcl_NewStringObj(buf,-1));
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be children, class, containing, depth ",
		"exists, geometry, height, ",
		"ismapped, manager, name, parent, ",
		"reqheight, reqwidth, rootx, rooty, ",
		"screenheight, screenwidth, ",
		"toplevel, width, x, or y", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;

    wrongArgs:
    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
	    argv[0], " ", argName, " window\"", (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_WinfoCmdObj --
 *
 *	This procedure is invoked to process the "winfo" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Ck_WinfoCmdObj(clientData, interp, objc, objv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj* CONST objv[];      /* Tcl_Obj* array of arguments. */
{
  static char *commands[] =
    {
     "children",
     "containing",
     "depth",
     "exists",
     "geometry",
     "height",
     "ismapped",
     "manager",
     "name",
     "class",
     "parent",
     "reqheight",
     "reqwidth",
     "rootx",
     "rooty",
     "screenheight",
     "screenwidth",
     "toplevel",
     "width",
     "x",
     "y",
     NULL
    };
  enum
  {
   CMD_CHILDREN,
   CMD_CONTAINING,
   CMD_DEPTH,
   CMD_EXISTS,
   CMD_GEOMETRY,
   CMD_HEIGHT,
   CMD_ISMAPPED,
   CMD_MANAGER,
   CMD_NAME,
   CMD_CLASS,
   CMD_PARENT,
   CMD_REQHEIGHT,
   CMD_REQWIDTH,
   CMD_ROOTX,
   CMD_ROOTY,
   CMD_SCREENHEIGHT,
   CMD_SCREENWIDTH,
   CMD_TOPLEVEL,
   CMD_WIDTH,
   CMD_X,
   CMD_Y
  };
  
  CkWindow *mainPtr = (CkWindow *) clientData;
  int index;
  char *argName;
  CkWindow *winPtr;
  
#ifdef SETUP
#undef SETUP
#endif
#define SETUP(name)							\
  if (objc != 3) {							\
    Tcl_WrongNumArgs( interp, 2, objv, "window");                       \
    return TCL_ERROR;                                                   \
  }									\
  winPtr = Ck_NameToWindow(interp, Tcl_GetString(objv[2]), mainPtr);	\
  if (winPtr == NULL) {							\
    return TCL_ERROR;							\
  }


  if (objc < 2) {
    Tcl_WrongNumArgs( interp, 1, objv, "option ?arg?");
    return TCL_ERROR;
  }
  if (Tcl_GetIndexFromObj( interp, objv[1], commands, "option", TCL_EXACT, &index) != TCL_OK) {
    return TCL_ERROR;
  }

  switch (index) {
  case CMD_CHILDREN:
    {
      SETUP("children");
      for (winPtr = winPtr->childList; winPtr != NULL;
	   winPtr = winPtr->nextPtr) {
	Tcl_AppendElement(interp, winPtr->pathName);
      }
    }
    break;
  case CMD_CONTAINING:
    {
      int x, y;

      if (objc != 4) {
	Tcl_WrongNumArgs( interp, 2, objv, "x y");
	return TCL_ERROR;
      }
      if (Tcl_GetIntFromObj(interp, objv[2], &x) != TCL_OK ||
	  Tcl_GetIntFromObj(interp, objv[3], &y) != TCL_OK) {
	return TCL_ERROR;
      }
      winPtr = Ck_GetWindowXY(mainPtr->mainPtr, &x, &y, 0);
      if (winPtr != NULL) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(winPtr->pathName, -1));
      }
    }
    break;
  case CMD_DEPTH:
    {
      SETUP("depth");
      Tcl_SetObjResult(interp, Tcl_NewIntObj((winPtr->mainPtr->flags & CK_HAS_COLOR) ? 3 : 1));
    }
    break;
  case CMD_EXISTS:
    {
      if (objc != 3) {
	Tcl_WrongNumArgs( interp, 2, objv, "window");
	return TCL_ERROR;
      }
      if (Ck_NameToWindow(interp, Tcl_GetString(objv[2]), mainPtr) == NULL) {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
      } else {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(1));
      }
    }
    break;
  case CMD_GEOMETRY:
    {
      char buf[128];
      SETUP("geometry");
      sprintf(buf, "%dx%d+%d+%d", winPtr->width, winPtr->height, winPtr->x, winPtr->y);
      Tcl_SetObjResult( interp, Tcl_NewStringObj(buf,-1));
    }
    break;
  case CMD_HEIGHT:
    {
      SETUP("height");
      Tcl_SetObjResult( interp, Tcl_NewIntObj(winPtr->height));
    }
    break;
  case CMD_ISMAPPED:
    {
      SETUP("ismapped");
      Tcl_SetObjResult(interp, Tcl_NewIntObj(!!(winPtr->flags & CK_MAPPED)));
    }
    break;
  case CMD_MANAGER:
    {
      SETUP("manager");
      if (winPtr->geomMgrPtr != NULL)
	Tcl_SetObjResult(interp, Tcl_NewStringObj(winPtr->geomMgrPtr->name,-1));
    }
    break;
  case CMD_NAME:
    {
      SETUP("name");
      Tcl_SetObjResult(interp, Tcl_NewStringObj((char *) winPtr->nameUid,-1));
    }
    break;
  case CMD_CLASS:
    {
      SETUP("class");
      Tcl_SetObjResult(interp, Tcl_NewStringObj((char *) winPtr->classUid,-1));
    }
    break;
  case CMD_PARENT:
    {
      SETUP("parent");
      if (winPtr->parentPtr != NULL)
	Tcl_SetObjResult(interp, Tcl_NewStringObj(winPtr->parentPtr->pathName,-1));
    }
    break;
  case CMD_REQHEIGHT:
    {
      SETUP("reqheight");
      Tcl_SetObjResult( interp, Tcl_NewIntObj(winPtr->reqHeight));
    }
    break;
  case CMD_REQWIDTH:
    {
      SETUP("reqwidth");
      Tcl_SetObjResult( interp, Tcl_NewIntObj(winPtr->reqWidth));
    }
    break;
  case CMD_ROOTX:
    {
      int x;
      
      SETUP("rootx");
      Ck_GetRootGeometry(winPtr, &x, NULL, NULL, NULL);
      Tcl_SetObjResult( interp, Tcl_NewIntObj(x));
    }
    break;
  case CMD_ROOTY:
    {
      int y;

      SETUP("rooty");
      Ck_GetRootGeometry(winPtr, NULL, &y, NULL, NULL);
      Tcl_SetObjResult( interp, Tcl_NewIntObj(y));
    }
    break;
  case CMD_SCREENHEIGHT:
    {
      SETUP("screenheight");
      Tcl_SetObjResult( interp, Tcl_NewIntObj(winPtr->mainPtr->winPtr->height));
    }
    break;
  case CMD_SCREENWIDTH:
    {
      SETUP("screenwidth");
      Tcl_SetObjResult( interp, Tcl_NewIntObj(winPtr->mainPtr->winPtr->width));
    }
    break;
  case CMD_TOPLEVEL:
    {
      SETUP("toplevel");
      for (; winPtr != NULL; winPtr = winPtr->parentPtr) {
	if (winPtr->flags & CK_TOPLEVEL) {
	  Tcl_SetObjResult( interp, Tcl_NewStringObj(winPtr->pathName,-1));
	  break;
	}
      }
    }
    break;
  case CMD_WIDTH:
    {
      SETUP("width");
      Tcl_SetObjResult( interp, Tcl_NewIntObj(winPtr->width));
    }
    break;
  case CMD_X:
    {
      SETUP("x");
      Tcl_SetObjResult( interp, Tcl_NewIntObj(winPtr->x));
    }
    break;
  case CMD_Y:
    {
      SETUP("y");
      Tcl_SetObjResult( interp, Tcl_NewIntObj(winPtr->y));
    }
    break;
  default:
    {
      /* -- should never be reached -- */
      Tcl_AppendResult(interp, "bad option \"", Tcl_GetString(objv[1]),
		       "\": must be children, class, containing, depth ",
		       "exists, geometry, height, ",
		       "ismapped, manager, name, parent, ",
		       "reqheight, reqwidth, rootx, rooty, ",
		       "screenheight, screenwidth, ",
		       "toplevel, width, x, or y", (char *) NULL);
      return TCL_ERROR;
    }
  }
  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_BindCmdObj --
 *
 *	This procedure is invoked to process the "bind" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Ck_BindCmdObj(clientData, interp, objc, objv)
    ClientData clientData;	/* Main window associated with interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj* CONST objv[];      /* Tcl_Obj* array of arguments. */
{
    CkWindow *mainWin = (CkWindow *) clientData;
    CkWindow *winPtr;
    ClientData object;
    char *argv1;

    if ((objc < 2) || (objc > 4)) {
      Tcl_WrongNumArgs( interp, 1, objv, "window ?pattern? ?command?");
      return TCL_ERROR;
    }

    argv1 = Tcl_GetString(objv[1]);
    if (argv1[0] == '.') {
      winPtr = (CkWindow *) Ck_NameToWindow(interp, argv1, mainWin);
      if (winPtr == NULL) {
	return TCL_ERROR;
      }
      object = (ClientData) winPtr->pathName;
	
    }
    else {
      winPtr = (CkWindow *) clientData;
      object = (ClientData) Ck_GetUid(argv1);
    }

    if (objc == 4) {
      char *argv2, *argv3;
      int append = 0;

      argv2 = Tcl_GetString(objv[2]);
      argv3 = Tcl_GetString(objv[3]);
      if (argv3[0] == 0) {
	return Ck_DeleteBinding(interp, winPtr->mainPtr->bindingTable,
				object, argv2);
      }
      if (argv3[0] == '+') {
	argv3++;
	append = 1;
      }
      if (Ck_CreateBinding(interp, winPtr->mainPtr->bindingTable, object,
			   argv2, argv3, append) != TCL_OK) {
	return TCL_ERROR;
      }
    }
    else if (objc == 3) {
      char *argv2, *argv3;
      char *command;

      argv2 = Tcl_GetString(objv[2]);
      command = Ck_GetBinding(interp, winPtr->mainPtr->bindingTable,
			      object, argv2);
      if (command == NULL) {
	Tcl_ResetResult(interp);
	return TCL_OK;
      }
      Tcl_SetObjResult( interp, Tcl_NewStringObj(command,-1));
    }
    else {
      Ck_GetAllBindings(interp, winPtr->mainPtr->bindingTable, object);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * CkBindEventProc --
 *
 *	This procedure is invoked by Ck_HandleEvent for each event;  it
 *	causes any appropriate bindings for that event to be invoked.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Depends on what bindings have been established with the "bind"
 *	command.
 *
 *----------------------------------------------------------------------
 */

void
CkBindEventProc(winPtr, eventPtr)
    CkWindow *winPtr;			/* Pointer to info about window. */
    CkEvent *eventPtr;			/* Information about event. */
{
#define MAX_OBJS 20
    ClientData objects[MAX_OBJS], *objPtr;
    static Ck_Uid allUid = NULL;
    int i, count;
    char *p;
    Tcl_HashEntry *hPtr;
    CkWindow *topLevPtr;

    if ((winPtr->mainPtr == NULL) || (winPtr->mainPtr->bindingTable == NULL)) {
	return;
    }

    objPtr = objects;
    if (winPtr->numTags != 0) {
	/*
	 * Make a copy of the tags for the window, replacing window names
	 * with pointers to the pathName from the appropriate window.
	 */

	if (winPtr->numTags > MAX_OBJS) {
	    objPtr = (ClientData *) ckalloc(winPtr->numTags *
	        sizeof (ClientData));
	}
	for (i = 0; i < winPtr->numTags; i++) {
	    p = (char *) winPtr->tagPtr[i];
	    if (*p == '.') {
		hPtr = Tcl_FindHashEntry(&winPtr->mainPtr->nameTable, p);
		if (hPtr != NULL) {
		    p = ((CkWindow *) Tcl_GetHashValue(hPtr))->pathName;
		} else {
		    p = NULL;
		}
	    }
	    objPtr[i] = (ClientData) p;
	}
	count = winPtr->numTags;
    } else {
	objPtr[0] = (ClientData) winPtr->pathName;
	objPtr[1] = (ClientData) winPtr->classUid;
	for (topLevPtr = winPtr; topLevPtr != NULL && 
	     !(topLevPtr->flags & CK_TOPLEVEL);
	     topLevPtr = topLevPtr->parentPtr) {
	     /* Empty loop body. */
	}
	if (winPtr != topLevPtr && topLevPtr != NULL) {
	    objPtr[2] = (ClientData) topLevPtr->pathName;
	    count = 4;
	} else
	    count = 3;
	if (allUid == NULL) {
	    allUid = Ck_GetUid("all");
	}
	objPtr[count - 1] = (ClientData) allUid;
    }
    Ck_BindEvent(winPtr->mainPtr->bindingTable, eventPtr, winPtr,
	    count, objPtr);
    if (objPtr != objects) {
	ckfree((char *) objPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_BindtagsCmdObj --
 *
 *	This procedure is invoked to process the "bindtags" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Ck_BindtagsCmdObj(clientData, interp, objc, objv)
    ClientData clientData;	/* Main window associated with interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj* CONST objv[];      /* Tcl_Obj* array of arguments. */
{
    CkWindow *mainWin = (CkWindow *) clientData;
    CkWindow *winPtr, *winPtr2;
    int i, tagArgc;
    char *argv2, *p, **tagArgv;

    if ((objc < 2) || (objc > 3)) {
      Tcl_WrongNumArgs( interp, 1, objv, "window ?tags?");
      return TCL_ERROR;
    }
    winPtr = (CkWindow *) Ck_NameToWindow(interp, Tcl_GetString(objv[1]), mainWin);
    if (winPtr == NULL) {
	return TCL_ERROR;
    }
    if (objc == 2) {
      if (winPtr->numTags == 0) {
	Tcl_AppendElement(interp, winPtr->pathName);
	Tcl_AppendElement(interp, winPtr->classUid);
	for (winPtr2 = winPtr; winPtr2 != NULL && 
	       !(winPtr2->flags & CK_TOPLEVEL);
	     winPtr2 = winPtr2->parentPtr) {
	  /* Empty loop body. */
	}
	if (winPtr != winPtr2 && winPtr2 != NULL)
	  Tcl_AppendElement(interp, winPtr2->pathName);
	Tcl_AppendElement(interp, "all");
      } else {
	for (i = 0; i < winPtr->numTags; i++) {
	  Tcl_AppendElement(interp, (char *) winPtr->tagPtr[i]);
	}
      }
      return TCL_OK;
    }
    if (winPtr->tagPtr != NULL) {
	CkFreeBindingTags(winPtr);
    }
    argv2 = Tcl_GetString(objv[2]);
    if (argv2[0] == 0) {
	return TCL_OK;
    }
    if (Tcl_SplitList(interp, argv2, &tagArgc, &tagArgv) != TCL_OK) {
	return TCL_ERROR;
    }
    winPtr->numTags = tagArgc;
    winPtr->tagPtr = (ClientData *) ckalloc(tagArgc * sizeof(ClientData));
    for (i = 0; i < tagArgc; i++) {
      p = tagArgv[i];
      if (p[0] == '.') {
	char *copy;

	/*
	 * Handle names starting with "." specially: store a malloc'ed
	 * string, rather than a Uid;  at event time we'll look up the
	 * name in the window table and use the corresponding window,
	 * if there is one.
	 */

	copy = (char *) ckalloc((unsigned) (strlen(p) + 1));
	strcpy(copy, p);
	winPtr->tagPtr[i] = (ClientData) copy;
      } else {
	winPtr->tagPtr[i] = (ClientData) Ck_GetUid(p);
      }
    }
    ckfree((char *) tagArgv);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * CkFreeBindingTags --
 *
 *	This procedure is called to free all of the binding tags
 *	associated with a window;  typically it is only invoked where
 *	there are window-specific tags.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Any binding tags for winPtr are freed.
 *
 *----------------------------------------------------------------------
 */

void
CkFreeBindingTags(winPtr)
    CkWindow *winPtr;		/* Window whose tags are to be released. */
{
    int i;
    char *p;

    for (i = 0; i < winPtr->numTags; i++) {
	p = (char *) (winPtr->tagPtr[i]);
	if (*p == '.') {
	    /*
	     * Names starting with "." are malloced rather than Uids, so
	     * they have to be freed.
	     */
    
	    ckfree(p);
	}
    }
    ckfree((char *) winPtr->tagPtr);
    winPtr->numTags = 0;
    winPtr->tagPtr = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_TkwaitCmdObj --
 *
 *	This procedure is invoked to process the "tkwait" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Ck_TkwaitCmdObj(clientData, interp, objc, objv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj* CONST objv[];      /* Tcl_Obj* array of arguments. */
{
  static char *commands[] =
    {
     "variable",
     "visibility",
     "window",
     NULL
    };
  enum
  {
   CMD_VARIABLE,
   CMD_VISIBILITY,
   CMD_WINDOW
  };
  
  CkWindow *mainPtr = (CkWindow *) clientData;
  int index, done;
  char *argv2;
  
  if (objc != 3) {
    Tcl_WrongNumArgs( interp, 1, objv, "variable|visible|window name");
    return TCL_ERROR;
  }
  if (Tcl_GetIndexFromObj( interp, objv[1], commands, "option", TCL_EXACT, &index) != TCL_OK) {
    return TCL_ERROR;
  }

  argv2 = Tcl_GetString(objv[2]);
  switch (index) {
  case CMD_VARIABLE:
    {
      if (Tcl_TraceVar(interp, argv2,
		       TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		       WaitVariableProc, (ClientData) &done) != TCL_OK) {
	return TCL_ERROR;
      }
      done = 0;
      while (!done) {
	Tk_DoOneEvent(0);
      }
      Tcl_UntraceVar(interp, argv2,
		     TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		     WaitVariableProc, (ClientData) &done);
    }
    break;
  case CMD_VISIBILITY:
    {
      CkWindow *winPtr;

      winPtr = Ck_NameToWindow(interp, argv2, mainPtr);
      if (winPtr == NULL) {
	return TCL_ERROR;
      }
      Ck_CreateEventHandler(winPtr,
			    CK_EV_MAP | CK_EV_UNMAP | CK_EV_EXPOSE | CK_EV_DESTROY,
			    WaitVisibilityProc, (ClientData) &done);
      done = 0;
      while (!done) {
	Tk_DoOneEvent(0);
      }
      Ck_DeleteEventHandler(winPtr,
			    CK_EV_MAP | CK_EV_UNMAP | CK_EV_EXPOSE | CK_EV_DESTROY,
			    WaitVisibilityProc, (ClientData) &done);
    }
    break;
  case CMD_WINDOW:
    {
      CkWindow *winPtr;

      winPtr = Ck_NameToWindow(interp, argv2, mainPtr);
      if (winPtr == NULL) {
	return TCL_ERROR;
      }
      Ck_CreateEventHandler(winPtr, CK_EV_DESTROY,
			    WaitWindowProc, (ClientData) &done);
      done = 0;
      while (!done) {
	Tk_DoOneEvent(0);
      }
      /*
       * Note:  there's no need to delete the event handler.  It was
       * deleted automatically when the window was destroyed.
       */
    }
    break;
  default:
    {
      /* -- should never be reached -- */
      Tcl_AppendResult(interp, "bad option \"", Tcl_GetString(objv[1]),
		       "\": must be variable, visibility, or window", (char *) NULL);
      return TCL_ERROR;
    }
  }

  /*
   * Clear out the interpreter's result, since it may have been set
   * by event handlers.
   */
  
  Tcl_ResetResult(interp);
  return TCL_OK;
}

static char *
WaitVariableProc(clientData, interp, name1, name2, flags)
    ClientData clientData;	/* Pointer to integer to set to 1. */
    Tcl_Interp *interp;		/* Interpreter containing variable. */
    char *name1;		/* Name of variable. */
    char *name2;		/* Second part of variable name. */
    int flags;			/* Information about what happened. */
{
    int *donePtr = (int *) clientData;

    *donePtr = 1;
    return (char *) NULL;
}

static void
WaitVisibilityProc(clientData, eventPtr)
    ClientData clientData;	/* Pointer to integer to set to 1. */
    CkEvent *eventPtr;		/* Information about event (not used). */
{
    int *donePtr = (int *) clientData;

    *donePtr = 1;
}

static void
WaitWindowProc(clientData, eventPtr)
    ClientData clientData;	/* Pointer to integer to set to 1. */
    CkEvent *eventPtr;		/* Information about event. */
{
    int *donePtr = (int *) clientData;

    if (eventPtr->type == CK_EV_DESTROY) {
	*donePtr = 1;
    }
}
