/* 
 * ckSpinbox.c --
 *
 *	This module implements "spinbox" and "toplevel" widgets for the
 *	toolkit.  Spinboxs are windows with a background color
 *	and possibly border, but no other attributes.
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
#include "default.h"

/*
 * A data structure of the following type is kept for each
 * spinbox that currently exists for this process:
 */

typedef struct {
    CkWindow *winPtr;		/* Window that embodies the spinbox.  NULL
				 * means that the window has been destroyed
				 * but the data structures haven't yet been
				 * cleaned up.*/
    Tcl_Interp *interp;		/* Interpreter associated with widget.
    				 * Used to delete widget command.  */
    Tcl_Command widgetCmd;      /* Token for spinbox's widget command. */
    CkBorder *borderPtr;	/* Structure used to draw border. */
    int fg, bg;			/* Foreground/background colors. */
    int attr;			/* Video attributes. */
    int width;			/* Width to request for window.  <= 0 means
				 * don't request any size. */
    int height;			/* Height to request for window.  <= 0 means
				 * don't request any size. */
    char *takeFocus;		/* Value of -takefocus option. */
    int flags;			/* Various flags;  see below for
				 * definitions. */
} Spinbox;

/*
 * Flag bits for spinboxs:
 *
 * REDRAW_PENDING:		Non-zero means a DoWhenIdle handler
 *				has already been queued to redraw
 *				this window.
 */

#define REDRAW_PENDING		1

static Ck_ConfigSpec configSpecs[] = {
    {CK_CONFIG_ATTR, "-attributes", "attributes", "Attributes",
	DEF_SPINBOX_ATTRIB, Ck_Offset(Spinbox, attr), 0},
    {CK_CONFIG_COLOR, "-background", "background", "Background",
	DEF_SPINBOX_BG_COLOR, Ck_Offset(Spinbox, bg), CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-background", "background", "Background",
	DEF_SPINBOX_BG_MONO, Ck_Offset(Spinbox, bg), CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_SYNONYM, "-bg", "background", (char *) NULL,
	(char *) NULL, 0, 0},
    {CK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_SPINBOX_FG_COLOR, Ck_Offset(Spinbox, fg), CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_SPINBOX_FG_MONO, Ck_Offset(Spinbox, fg), CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_SYNONYM, "-fg", "foreground", (char *) NULL,
	(char *) NULL, 0, 0},
    {CK_CONFIG_BORDER, "-border", "border", "Border",
	DEF_SPINBOX_BORDER, Ck_Offset(Spinbox, borderPtr), CK_CONFIG_NULL_OK},
    {CK_CONFIG_COORD, "-height", "height", "Height",
	DEF_SPINBOX_HEIGHT, Ck_Offset(Spinbox, height), 0},
    {CK_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_SPINBOX_TAKE_FOCUS, Ck_Offset(Spinbox, takeFocus),
	CK_CONFIG_NULL_OK},
    {CK_CONFIG_COORD, "-width", "width", "Width",
	DEF_SPINBOX_WIDTH, Ck_Offset(Spinbox, width), 0},
    {CK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * Forward declarations for procedures defined later in this file:
 */

static int	ConfigureSpinbox _ANSI_ARGS_((Tcl_Interp *interp,
		    Spinbox *spinboxPtr, int argc, char **argv, int flags));
static void	DestroySpinbox _ANSI_ARGS_((ClientData clientData));
static void     SpinboxCmdDeletedProc _ANSI_ARGS_((ClientData clientData));
static void	DisplaySpinbox _ANSI_ARGS_((ClientData clientData));
static void	SpinboxEventProc _ANSI_ARGS_((ClientData clientData,
		    CkEvent *eventPtr));
static int	SpinboxWidgetCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));

/*
 *--------------------------------------------------------------
 *
 * Ck_SpinboxCmd --
 *
 *	This procedure is invoked to process the "spinbox" and
 *	"toplevel" Tcl commands.  See the user documentation for
 *	details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */

int
Ck_SpinboxCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    CkWindow *winPtr = (CkWindow *) clientData;
    CkWindow *new;
    char *className;
    int src, dst, toplevel;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " pathName ?options?\"", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * The code below is a special hack that extracts a few key
     * options from the argument list now, rather than letting
     * ConfigureSpinbox do it.  This is necessary because we have
     * to know the window's class before creating the window.
     */

    toplevel = *argv[0] == 't';
    className = NULL;
    for (src = 2, dst = 2; src < argc;  src += 2) {
	char c;

	c = argv[src][1];
	if ((c == 'c')
		&& (strncmp(argv[src], "-class", strlen(argv[src])) == 0)) {
	    className = argv[src+1];
	} else {
	    argv[dst] = argv[src];
	    argv[dst+1] = argv[src+1];
	    dst += 2;
	}
    }
    argc -= src-dst;

    /*
     * Create the window and initialize our structures and event handlers.
     */

    new = Ck_CreateWindowFromPath(interp, winPtr, argv[1], toplevel);
    if (new == NULL)
	return TCL_ERROR;
    if (className == NULL) {
        className = Ck_GetOption(new, "class", "Class");
        if (className == NULL) {
            className = (toplevel) ? "Toplevel" : "Spinbox";
        }
    }
    Ck_SetClass(new, className);
    return CkInitSpinbox(interp, new, argc-2, argv+2);
}

/*
 *----------------------------------------------------------------------
 *
 * CkInitSpinbox --
 *
 *	This procedure initializes a spinbox widget.  It's
 *	separate from Ck_SpinboxCmd so that it can be used for the
 *	main window, which has already been created elsewhere.
 *
 * Results:
 *	A standard Tcl completion code.
 *
 * Side effects:
 *	A widget record gets allocated, handlers get set up, etc..
 *
 *----------------------------------------------------------------------
 */

int
CkInitSpinbox(interp, winPtr, argc, argv)
    Tcl_Interp *interp;			/* Interpreter associated with the
					 * application. */
    CkWindow *winPtr;			/* Window to use for spinbox or
					 * top-level. Caller must already
					 * have set window's class. */
    int argc;				/* Number of configuration arguments
					 * (not including class command and
					 * window name). */
    char *argv[];			/* Configuration arguments. */
{
    Spinbox *spinboxPtr;

    spinboxPtr = (Spinbox *) ckalloc(sizeof (Spinbox));
    spinboxPtr->winPtr = winPtr;
    spinboxPtr->interp = interp;
    spinboxPtr->widgetCmd = Tcl_CreateCommand(interp,
        spinboxPtr->winPtr->pathName, SpinboxWidgetCmd,
	    (ClientData) spinboxPtr, SpinboxCmdDeletedProc);
    spinboxPtr->borderPtr = NULL;
    spinboxPtr->fg = 0;
    spinboxPtr->bg = 0;
    spinboxPtr->attr = 0;
    spinboxPtr->width = 1;
    spinboxPtr->height = 1;
    spinboxPtr->takeFocus = NULL;
    spinboxPtr->flags = 0;
    Ck_CreateEventHandler(spinboxPtr->winPtr,
    	    CK_EV_MAP | CK_EV_EXPOSE | CK_EV_DESTROY,
	    SpinboxEventProc, (ClientData) spinboxPtr);
    if (ConfigureSpinbox(interp, spinboxPtr, argc, argv, 0) != TCL_OK) {
	Ck_DestroyWindow(spinboxPtr->winPtr);
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, Tcl_NewStringObj(spinboxPtr->winPtr->pathName,-1));
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * SpinboxWidgetCmd --
 *
 *	This procedure is invoked to process the Tcl command
 *	that corresponds to a spinbox widget.  See the user
 *	documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */

static int
SpinboxWidgetCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Information about spinbox widget. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Spinbox *spinboxPtr = (Spinbox *) clientData;
    int result = TCL_OK;
    int length;
    char c;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " option ?arg arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    Ck_Preserve((ClientData) spinboxPtr);
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'c') && (strncmp(argv[1], "cget", length) == 0)
	&& (length >= 2)) {
        if (argc != 3) {
            Tcl_AppendResult(interp, "wrong # args: should be \"",
                    argv[0], " cget option\"",
                    (char *) NULL);
            goto error;
        }
        result = Ck_ConfigureValue(interp, spinboxPtr->winPtr, configSpecs,
                (char *) spinboxPtr, argv[2], 0);
    } else if ((c == 'c') && (strncmp(argv[1], "configure", length) == 0)) {
	if (argc == 2) {
	    result = Ck_ConfigureInfo(interp, spinboxPtr->winPtr, configSpecs,
		    (char *) spinboxPtr, (char *) NULL, 0);
	} else if (argc == 3) {
	    result = Ck_ConfigureInfo(interp, spinboxPtr->winPtr, configSpecs,
		    (char *) spinboxPtr, argv[2], 0);
	} else {
	    result = ConfigureSpinbox(interp, spinboxPtr, argc-2, argv+2,
		    CK_CONFIG_ARGV_ONLY);
	}
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\":  must be cget or configure", (char *) NULL);
	goto error;
    }
    Ck_Release((ClientData) spinboxPtr);
    return result;

error:
    Ck_Release((ClientData) spinboxPtr);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * DestroySpinbox --
 *
 *	This procedure is invoked by Ck_EventuallyFree or Ck_Release
 *	to clean up the internal structure of a spinbox at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the spinbox is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
DestroySpinbox(clientData)
    ClientData clientData;	/* Info about spinbox widget. */
{
    Spinbox *spinboxPtr = (Spinbox *) clientData;

    Ck_FreeOptions(configSpecs, (char *) spinboxPtr, 0);
    ckfree((char *) spinboxPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * SpinboxCmdDeletedProc --
 *
 *      This procedure is invoked when a widget command is deleted.  If
 *      the widget isn't already in the process of being destroyed,
 *      this command destroys it.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The widget is destroyed.
 *
 *----------------------------------------------------------------------
 */

static void
SpinboxCmdDeletedProc(clientData)
    ClientData clientData;      /* Pointer to widget record for widget. */
{
    Spinbox *spinboxPtr = (Spinbox *) clientData;
    CkWindow *winPtr = spinboxPtr->winPtr;

    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case tkwin
     * is NULL) or because the command was deleted, and then this procedure
     * destroys the widget.
     */

    if (winPtr != NULL) {
        spinboxPtr->winPtr = NULL;
        Ck_DestroyWindow(winPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureSpinbox --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the option database, in order to configure (or
 *	reconfigure) a spinbox widget.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for spinboxPtr;  old resources get freed, if there
 *	were any.
 *
 *----------------------------------------------------------------------
 */

static int
ConfigureSpinbox(interp, spinboxPtr, argc, argv, flags)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Spinbox *spinboxPtr;		/* Information about widget;  may or may
				 * not already have values for some fields. */
    int argc;			/* Number of valid entries in argv. */
    char **argv;		/* Arguments. */
    int flags;			/* Flags to pass to Tk_ConfigureWidget. */
{
    if (Ck_ConfigureWidget(interp, spinboxPtr->winPtr, configSpecs,
	    argc, argv, (char *) spinboxPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }

    Ck_SetWindowAttr(spinboxPtr->winPtr, spinboxPtr->fg, spinboxPtr->bg,
    	spinboxPtr->attr);
    Ck_SetInternalBorder(spinboxPtr->winPtr, spinboxPtr->borderPtr != NULL);
    if ((spinboxPtr->width > 0) || (spinboxPtr->height > 0))
	Ck_GeometryRequest(spinboxPtr->winPtr, spinboxPtr->width,
	    spinboxPtr->height);
    if ((spinboxPtr->winPtr->flags & CK_MAPPED)
	    && !(spinboxPtr->flags & REDRAW_PENDING)) {
	Tk_DoWhenIdle(DisplaySpinbox, (ClientData) spinboxPtr);
	spinboxPtr->flags |= REDRAW_PENDING;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DisplaySpinbox --
 *
 *	This procedure is invoked to display a spinbox widget.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Commands are output to display the spinbox in its
 *	current mode.
 *
 *----------------------------------------------------------------------
 */

static void
DisplaySpinbox(clientData)
    ClientData clientData;	/* Information about widget. */
{
    Spinbox *spinboxPtr = (Spinbox *) clientData;
    CkWindow *winPtr = spinboxPtr->winPtr;

    spinboxPtr->flags &= ~REDRAW_PENDING;
    if ((spinboxPtr->winPtr == NULL) || !(winPtr->flags & CK_MAPPED)) {
	return;
    }
    Ck_ClearToBot(winPtr, 0, 0);
    if (spinboxPtr->borderPtr != NULL)
	Ck_DrawBorder(winPtr, spinboxPtr->borderPtr, 0, 0,
	    winPtr->width, winPtr->height);
    Ck_EventuallyRefresh(winPtr);
}

/*
 *--------------------------------------------------------------
 *
 * SpinboxEventProc --
 *
 *	This procedure is invoked by the dispatcher on
 *	structure changes to a spinbox.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When the window gets deleted, internal structures get
 *	cleaned up.  When it gets exposed, it is redisplayed.
 *
 *--------------------------------------------------------------
 */

static void
SpinboxEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window. */
    CkEvent *eventPtr;		/* Information about event. */
{
    Spinbox *spinboxPtr = (Spinbox *) clientData;

    if (eventPtr->type == CK_EV_EXPOSE && spinboxPtr->winPtr != NULL &&
	!(spinboxPtr->flags & REDRAW_PENDING)) {
	Tk_DoWhenIdle(DisplaySpinbox, (ClientData) spinboxPtr);
	spinboxPtr->flags |= REDRAW_PENDING;
    } else if (eventPtr->type == CK_EV_DESTROY) {
        if (spinboxPtr->winPtr != NULL) {
            spinboxPtr->winPtr = NULL;
            Tcl_DeleteCommand(spinboxPtr->interp,
                    Tcl_GetCommandName(spinboxPtr->interp, spinboxPtr->widgetCmd));
        }
	if (spinboxPtr->flags & REDRAW_PENDING)
	    Tk_CancelIdleCall(DisplaySpinbox, (ClientData) spinboxPtr);
	Ck_EventuallyFree((ClientData) spinboxPtr, (Ck_FreeProc *) DestroySpinbox);
    }
}
