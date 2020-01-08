/* 
 * ckPalette.c --
 *
 *	This module implements "palette" widget for the toolkit.
 *      A "palette" is a window showing the available colors.
 *      It is currently limited to 256 colors.
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 * Copyright (c) 2019 VCA
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "ckPort.h"
#include "ck.h"
#include "default.h"

/*
 * A data structure of the following type is kept for each
 * palette that currently exists for this process:
 */

typedef struct {
    CkWindow *winPtr;		/* Window that embodies the palette.  NULL
				 * means that the window has been destroyed
				 * but the data structures haven't yet been
				 * cleaned up.*/
    Tcl_Interp *interp;		/* Interpreter associated with widget.
    				 * Used to delete widget command.  */
    Tcl_Command widgetCmd;      /* Token for palette's widget command. */
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
} Palette;

/*
 * Flag bits for palettes:
 *
 * REDRAW_PENDING:		Non-zero means a DoWhenIdle handler
 *				has already been queued to redraw
 *				this window.
 */

#define REDRAW_PENDING		1

static Ck_ConfigSpec configSpecs[] = {
    {CK_CONFIG_ATTR, "-attributes", "attributes", "Attributes",
	DEF_PALETTE_ATTRIB, Ck_Offset(Palette, attr), 0},
    {CK_CONFIG_COLOR, "-background", "background", "Background",
	DEF_PALETTE_BG_COLOR, Ck_Offset(Palette, bg), CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-background", "background", "Background",
	DEF_PALETTE_BG_MONO, Ck_Offset(Palette, bg), CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_SYNONYM, "-bg", "background", (char *) NULL,
	(char *) NULL, 0, 0},
    {CK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_PALETTE_FG_COLOR, Ck_Offset(Palette, fg), CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_PALETTE_FG_MONO, Ck_Offset(Palette, fg), CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_SYNONYM, "-fg", "foreground", (char *) NULL,
	(char *) NULL, 0, 0},
    {CK_CONFIG_BORDER, "-border", "border", "Border",
	DEF_PALETTE_BORDER, Ck_Offset(Palette, borderPtr), CK_CONFIG_NULL_OK},
    {CK_CONFIG_COORD, "-height", "height", "Height",
	DEF_PALETTE_HEIGHT, Ck_Offset(Palette, height), 0},
    {CK_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_PALETTE_TAKE_FOCUS, Ck_Offset(Palette, takeFocus),
	CK_CONFIG_NULL_OK},
    {CK_CONFIG_COORD, "-width", "width", "Width",
	DEF_PALETTE_WIDTH, Ck_Offset(Palette, width), 0},
    {CK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * Forward declarations for procedures defined later in this file:
 */

static int	ConfigurePalette _ANSI_ARGS_((Tcl_Interp *interp,
		    Palette *palettePtr, int argc, char **argv, int flags));
static void	DestroyPalette _ANSI_ARGS_((ClientData clientData));
static void     PaletteCmdDeletedProc _ANSI_ARGS_((ClientData clientData));
static void	DisplayPalette _ANSI_ARGS_((ClientData clientData));
static void	PaletteEventProc _ANSI_ARGS_((ClientData clientData,
		    CkEvent *eventPtr));
static int	PaletteWidgetCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));

/*
 *--------------------------------------------------------------
 *
 * Ck_PaletteCmd --
 *
 *	This procedure is invoked to process the "palette" and
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
Ck_PaletteCmd(clientData, interp, argc, argv)
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
     * ConfigurePalette do it.  This is necessary because we have
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
            className = (toplevel) ? "Toplevel" : "Palette";
        }
    }
    Ck_SetClass(new, className);
    return CkInitPalette(interp, new, argc-2, argv+2);
}

/*
 *----------------------------------------------------------------------
 *
 * CkInitPalette --
 *
 *	This procedure initializes a palette widget.  It's
 *	separate from Ck_PaletteCmd so that it can be used for the
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
CkInitPalette(interp, winPtr, argc, argv)
    Tcl_Interp *interp;			/* Interpreter associated with the
					 * application. */
    CkWindow *winPtr;			/* Window to use for palette or
					 * top-level. Caller must already
					 * have set window's class. */
    int argc;				/* Number of configuration arguments
					 * (not including class command and
					 * window name). */
    char *argv[];			/* Configuration arguments. */
{
    Palette *palettePtr;

    palettePtr = (Palette *) ckalloc(sizeof (Palette));
    palettePtr->winPtr = winPtr;
    palettePtr->interp = interp;
    palettePtr->widgetCmd = Tcl_CreateCommand(interp,
        palettePtr->winPtr->pathName, PaletteWidgetCmd,
	    (ClientData) palettePtr, PaletteCmdDeletedProc);
    palettePtr->borderPtr = NULL;
    palettePtr->fg = 0;
    palettePtr->bg = 0;
    palettePtr->attr = 0;
    palettePtr->width = 1;
    palettePtr->height = 1;
    palettePtr->takeFocus = NULL;
    palettePtr->flags = 0;
    Ck_CreateEventHandler(palettePtr->winPtr,
    	    CK_EV_MAP | CK_EV_EXPOSE | CK_EV_DESTROY,
	    PaletteEventProc, (ClientData) palettePtr);
    if (ConfigurePalette(interp, palettePtr, argc, argv, 0) != TCL_OK) {
	Ck_DestroyWindow(palettePtr->winPtr);
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, Tcl_NewStringObj(palettePtr->winPtr->pathName,-1));
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * PaletteWidgetCmd --
 *
 *	This procedure is invoked to process the Tcl command
 *	that corresponds to a palette widget.  See the user
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
PaletteWidgetCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Information about palette widget. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Palette *palettePtr = (Palette *) clientData;
    int result = TCL_OK;
    int length;
    char c;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " option ?arg arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    Ck_Preserve((ClientData) palettePtr);
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
        result = Ck_ConfigureValue(interp, palettePtr->winPtr, configSpecs,
                (char *) palettePtr, argv[2], 0);
    } else if ((c == 'c') && (strncmp(argv[1], "configure", length) == 0)) {
	if (argc == 2) {
	    result = Ck_ConfigureInfo(interp, palettePtr->winPtr, configSpecs,
		    (char *) palettePtr, (char *) NULL, 0);
	} else if (argc == 3) {
	    result = Ck_ConfigureInfo(interp, palettePtr->winPtr, configSpecs,
		    (char *) palettePtr, argv[2], 0);
	} else {
	    result = ConfigurePalette(interp, palettePtr, argc-2, argv+2,
		    CK_CONFIG_ARGV_ONLY);
	}
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\":  must be cget or configure", (char *) NULL);
	goto error;
    }
    Ck_Release((ClientData) palettePtr);
    return result;

error:
    Ck_Release((ClientData) palettePtr);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyPalette --
 *
 *	This procedure is invoked by Ck_EventuallyFree or Ck_Release
 *	to clean up the internal structure of a palette at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the palette is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
DestroyPalette(clientData)
    ClientData clientData;	/* Info about palette widget. */
{
    Palette *palettePtr = (Palette *) clientData;

    Ck_FreeOptions(configSpecs, (char *) palettePtr, 0);
    ckfree((char *) palettePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * PaletteCmdDeletedProc --
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
PaletteCmdDeletedProc(clientData)
    ClientData clientData;      /* Pointer to widget record for widget. */
{
    Palette *palettePtr = (Palette *) clientData;
    CkWindow *winPtr = palettePtr->winPtr;

    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case tkwin
     * is NULL) or because the command was deleted, and then this procedure
     * destroys the widget.
     */

    if (winPtr != NULL) {
        palettePtr->winPtr = NULL;
        Ck_DestroyWindow(winPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigurePalette --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the option database, in order to configure (or
 *	reconfigure) a palette widget.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for palettePtr;  old resources get freed, if there
 *	were any.
 *
 *----------------------------------------------------------------------
 */

static int
ConfigurePalette(interp, palettePtr, argc, argv, flags)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Palette *palettePtr;		/* Information about widget;  may or may
				 * not already have values for some fields. */
    int argc;			/* Number of valid entries in argv. */
    char **argv;		/* Arguments. */
    int flags;			/* Flags to pass to Tk_ConfigureWidget. */
{
    if (Ck_ConfigureWidget(interp, palettePtr->winPtr, configSpecs,
	    argc, argv, (char *) palettePtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }

    Ck_SetWindowAttr(palettePtr->winPtr, palettePtr->fg, palettePtr->bg,
    	palettePtr->attr);
    Ck_SetInternalBorder(palettePtr->winPtr, palettePtr->borderPtr != NULL);
    if ((palettePtr->width > 0) || (palettePtr->height > 0))
	Ck_GeometryRequest(palettePtr->winPtr, palettePtr->width,
	    palettePtr->height);
    if ((palettePtr->winPtr->flags & CK_MAPPED)
	    && !(palettePtr->flags & REDRAW_PENDING)) {
	Tk_DoWhenIdle(DisplayPalette, (ClientData) palettePtr);
	palettePtr->flags |= REDRAW_PENDING;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DisplayPalette --
 *
 *	This procedure is invoked to display a palette widget.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Commands are output to display the palette in its
 *	current mode.
 *
 *----------------------------------------------------------------------
 */

static void
DisplayPalette(clientData)
    ClientData clientData;	/* Information about widget. */
{
    Palette *palettePtr = (Palette *) clientData;
    CkWindow *winPtr = palettePtr->winPtr;
    int i;

    palettePtr->flags &= ~REDRAW_PENDING;
    if ((palettePtr->winPtr == NULL) || !(winPtr->flags & CK_MAPPED)) {
	return;
    }
    Ck_ClearToBot(winPtr, 0, 0);
    if (palettePtr->borderPtr != NULL) {
	Ck_DrawBorder(winPtr, palettePtr->borderPtr, 0, 0,
	    winPtr->width, winPtr->height);
    }

    /* display all colors */
    Ck_SetWindowAttr(winPtr, winPtr->fg, winPtr->bg, winPtr->attr);
    
    for ( i = 0 ; i < 8; ++i ) {
      Ck_SetWindowAttr(winPtr, COLOR_WHITE, winPtr->bg, winPtr->attr);
    }
    
    Ck_EventuallyRefresh(winPtr);
}

/*
 *--------------------------------------------------------------
 *
 * PaletteEventProc --
 *
 *	This procedure is invoked by the dispatcher on
 *	structure changes to a palette.
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
PaletteEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window. */
    CkEvent *eventPtr;		/* Information about event. */
{
    Palette *palettePtr = (Palette *) clientData;

    if (eventPtr->type == CK_EV_EXPOSE && palettePtr->winPtr != NULL &&
	!(palettePtr->flags & REDRAW_PENDING)) {
	Tk_DoWhenIdle(DisplayPalette, (ClientData) palettePtr);
	palettePtr->flags |= REDRAW_PENDING;
    } else if (eventPtr->type == CK_EV_DESTROY) {
        if (palettePtr->winPtr != NULL) {
            palettePtr->winPtr = NULL;
            Tcl_DeleteCommand(palettePtr->interp,
                    Tcl_GetCommandName(palettePtr->interp, palettePtr->widgetCmd));
        }
	if (palettePtr->flags & REDRAW_PENDING)
	    Tk_CancelIdleCall(DisplayPalette, (ClientData) palettePtr);
	Ck_EventuallyFree((ClientData) palettePtr, (Ck_FreeProc *) DestroyPalette);
    }
}
