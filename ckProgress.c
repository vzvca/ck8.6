/* 
 * ckProgress.c --
 *
 *	This module implements "progress" widget for the
 *	toolkit.  Progress bar are windows with a background color
 *	and possibly border. They can be oriented vertically
 *      or horizontally.
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
 * progress that currently exists for this process:
 */

typedef struct {
  CkWindow *winPtr;		/* Window that embodies the progress.  NULL
				 * means that the window has been destroyed
				 * but the data structures haven't yet been
				 * cleaned up.*/
  Tcl_Interp *interp;		/* Interpreter associated with widget.
    				 * Used to delete widget command.  */
  Tcl_Command widgetCmd;      	/* Token for progress's widget command. */
  CkBorder *borderPtr;		/* Structure used to draw border. */
  int fg, bg;			/* Foreground/background colors. */
  int attr;			/* Video attributes. */
  int width;			/* Width to request for window.  <= 0 means
				 * don't request any size. */
  int height;			/* Height to request for window.  <= 0 means
				 * don't request any size. */
  char *takeFocus;		/* Value of -takefocus option. */
  Ck_Uid orientUid;		/* Orientation for window ("vertical" or
				 * "horizontal"). */
  char *varName;		/* Name of variable used to control selected
				 * state of button.  Malloc'ed (if
				 * not NULL). */
  int maximum;			/* maximum value (defaults to 100) */
  int flags;			/* Various flags;  see below for
				 * definitions. */
  int step;                     /* current step value */
  int value;                    /* current value */
  Tk_TimerToken timer;          /* used in auto-increment mode */
  int timerRunning;		/* 1 if the timer is active, 0 otherwise */
  int timerInterval;            /* interval in msec of timer */
  char *mode;                   /* "determinate" or "undeterminate" */
  
} Progress;

/*
 * Flag bits for progress bars:
 *
 * REDRAW_PENDING:		Non-zero means a DoWhenIdle handler
 *				has already been queued to redraw
 *				this window.
 */

#define REDRAW_PENDING		1



static int ValueParseProc(ClientData clientData,
			  Tcl_Interp *interp, CkWindow *winPtr,
			  char *value, char *widgRec, int offset);

static char *ValuePrintProc(ClientData clientData,
			   CkWindow *winPtr, char *widgRec, int offset,
			   Tcl_FreeProc **freeProcPtr);

static Ck_CustomOption ValueCustomOption =
  { ValueParseProc, ValuePrintProc, NULL };


static Ck_ConfigSpec configSpecs[] = {
    {CK_CONFIG_ATTR, "-attributes", "attributes", "Attributes",
	DEF_PROGRESS_ATTRIB, Ck_Offset(Progress, attr), 0},
    {CK_CONFIG_COLOR, "-background", "background", "Background",
	DEF_PROGRESS_BG_COLOR, Ck_Offset(Progress, bg), CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-background", "background", "Background",
	DEF_PROGRESS_BG_MONO, Ck_Offset(Progress, bg), CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_SYNONYM, "-bg", "background", (char *) NULL,
	(char *) NULL, 0, 0},
    {CK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_PROGRESS_FG_COLOR, Ck_Offset(Progress, fg), CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_PROGRESS_FG_MONO, Ck_Offset(Progress, fg), CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_SYNONYM, "-fg", "foreground", (char *) NULL,
	(char *) NULL, 0, 0},
    {CK_CONFIG_BORDER, "-border", "border", "Border",
	DEF_PROGRESS_BORDER, Ck_Offset(Progress, borderPtr), CK_CONFIG_NULL_OK},
    {CK_CONFIG_COORD, "-height", "height", "Height",
	DEF_PROGRESS_HEIGHT, Ck_Offset(Progress, height), 0},
    {CK_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_PROGRESS_TAKE_FOCUS, Ck_Offset(Progress, takeFocus),
	CK_CONFIG_NULL_OK},
    {CK_CONFIG_COORD, "-width", "width", "Width",
	DEF_PROGRESS_WIDTH, Ck_Offset(Progress, width), 0},
    {CK_CONFIG_UID, "-orient", "orient", "Orient",
	DEF_PROGRESS_ORIENT, Ck_Offset(Progress, orientUid), 0},
    {CK_CONFIG_STRING, "-variable", "variable", "Variable",
	DEF_PROGRESS_VARIABLE, Ck_Offset(Progress, varName),
	CK_CONFIG_NULL_OK},
    {CK_CONFIG_STRING, "-mode", "mode", "Mode",
	DEF_PROGRESS_MODE, Ck_Offset(Progress, mode),
	CK_CONFIG_NULL_OK},
    {CK_CONFIG_INT, "-maximum", "maximum", "Maximum",
	DEF_PROGRESS_MAXIMUM, Ck_Offset(Progress,maximum), 
	CK_CONFIG_NULL_OK },
    {CK_CONFIG_CUSTOM, "-value", "value", "Value",
	DEF_PROGRESS_VALUE, Ck_Offset(Progress,value), 
     	0, &ValueCustomOption },
    {CK_CONFIG_INT, "-step", "step", "Step",
	DEF_PROGRESS_STEP, Ck_Offset(Progress,value), 
	CK_CONFIG_NULL_OK },
    {CK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * Constants used later
 */
#define PROGRESS_HORIZONTAL     "horizontal"
#define PROGRESS_VERTICAL       "vertical"
#define PROGRESS_DETERMINATE    "determinate"
#define PROGRESS_UNDERTERMINATE "undeterminate"

/*
 * Forward declarations for procedures defined later in this file:
 */

static int	ConfigureProgress _ANSI_ARGS_((Tcl_Interp *interp,
		    Progress *progressPtr, int argc, char **argv, int flags));
static void	DestroyProgress _ANSI_ARGS_((ClientData clientData));
static void     ProgressCmdDeletedProc _ANSI_ARGS_((ClientData clientData));
static void	DisplayProgress _ANSI_ARGS_((ClientData clientData));
static void	ProgressEventProc _ANSI_ARGS_((ClientData clientData,
		    CkEvent *eventPtr));
static int	ProgressWidgetCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
static void	ProgressTimer _ANSI_ARGS_((ClientData clientData));
static void	ProgressPostRedisplay _ANSI_ARGS_((Progress *progressPtr));
static int	ProgressInit _ANSI_ARGS_((Tcl_Interp *interp, CkWindow *winPtr,
		    int argc, char **argv));
static char *   ProgressVarProc _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, char *name1, char *name2, int flags));

/*
 *--------------------------------------------------------------
 *
 * Ck_ProgressCmd --
 *
 *	This procedure is invoked to process the "progress"
 *	Tcl commands.  See the user documentation for
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
Ck_ProgressCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    CkWindow *winPtr = (CkWindow *) clientData;
    CkWindow *new;
    char *className;
    int src, dst;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " pathName ?options?\"", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * The code below is a special hack that extracts a few key
     * options from the argument list now, rather than letting
     * ConfigureProgress do it.  This is necessary because we have
     * to know the window's class before creating the window.
     */

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

    new = Ck_CreateWindowFromPath(interp, winPtr, argv[1], 0);
    if (new == NULL)
	return TCL_ERROR;
    if (className == NULL) {
        className = Ck_GetOption(new, "class", "Class");
        if (className == NULL) {
            className = "Progress";
        }
    }
    Ck_SetClass(new, className);
    return ProgressInit(interp, new, argc-2, argv+2);
}

/*
 *----------------------------------------------------------------------
 *
 * ProgressInit --
 *
 *	This procedure initializes a progress widget.  It's
 *	separate from Ck_ProgressCmd so that it can be used for the
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

static int
ProgressInit(interp, winPtr, argc, argv)
    Tcl_Interp *interp;			/* Interpreter associated with the
					 * application. */
    CkWindow *winPtr;			/* Window to use for progress.
					 * Caller must already
					 * have set window's class. */
    int argc;				/* Number of configuration arguments
					 * (not including class command and
					 * window name). */
    char *argv[];			/* Configuration arguments. */
{
    Progress *progressPtr;

    progressPtr = (Progress *) ckalloc(sizeof (Progress));
    progressPtr->winPtr = winPtr;
    progressPtr->interp = interp;
    progressPtr->widgetCmd =
      Tcl_CreateCommand(interp, progressPtr->winPtr->pathName, ProgressWidgetCmd,
			(ClientData) progressPtr, ProgressCmdDeletedProc);
    progressPtr->borderPtr = NULL;
    progressPtr->fg = 0;
    progressPtr->bg = 0;
    progressPtr->attr = 0;
    progressPtr->width = 1;
    progressPtr->height = 1;
    progressPtr->takeFocus = NULL;
    progressPtr->orientUid = Ck_GetUid(DEF_PROGRESS_ORIENT);
    progressPtr->varName = NULL;
    progressPtr->mode = NULL;
    progressPtr->flags = 0;
    progressPtr->value = atoi(DEF_PROGRESS_VALUE);
    progressPtr->step = atoi(DEF_PROGRESS_STEP);
    progressPtr->timer = NULL;
    progressPtr->timerRunning = 0;
    progressPtr->timerInterval = atoi(DEF_PROGRESS_INTERVAL);
    Ck_CreateEventHandler(progressPtr->winPtr,
    	    CK_EV_MAP | CK_EV_EXPOSE | CK_EV_DESTROY,
	    ProgressEventProc, (ClientData) progressPtr);
    if (ConfigureProgress(interp, progressPtr, argc, argv, 0) != TCL_OK) {
	Ck_DestroyWindow(progressPtr->winPtr);
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, Tcl_NewStringObj(progressPtr->winPtr->pathName,-1));
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * ProgressWidgetCmd --
 *
 *	This procedure is invoked to process the Tcl command
 *	that corresponds to a progress widget.  See the user
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
ProgressWidgetCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Information about progress widget. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Progress *progressPtr = (Progress *) clientData;
    int result = TCL_OK;
    int length;
    char c;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " option ?arg arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    Ck_Preserve((ClientData) progressPtr);
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
        result = Ck_ConfigureValue(interp, progressPtr->winPtr, configSpecs,
                (char *) progressPtr, argv[2], 0);
	
    } else if ((c == 'c') && (strncmp(argv[1], "configure", length) == 0)) {
	if (argc == 2) {
	    result = Ck_ConfigureInfo(interp, progressPtr->winPtr, configSpecs,
		    (char *) progressPtr, (char *) NULL, 0);
	} else if (argc == 3) {
	    result = Ck_ConfigureInfo(interp, progressPtr->winPtr, configSpecs,
		    (char *) progressPtr, argv[2], 0);
	} else {
	    result = ConfigureProgress(interp, progressPtr, argc-2, argv+2,
		    CK_CONFIG_ARGV_ONLY);
	}
	
    } else if ((c == 's') && (strncmp(argv[1], "start", length) == 0)) {
	char *sinterval;
	int interval;
	if (argc == 2) {
	  sinterval = DEF_PROGRESS_INTERVAL;
	} else if (argc == 3) {
	  sinterval = argv[2];
	} else {
	  Tcl_AppendResult(interp, "wrong # args: should be \"",
			   argv[0], " start ?interval?\"",
			   (char *) NULL);
	  goto error;
	}
	if (TCL_OK != Tcl_GetInt( interp, sinterval, &interval)) {
	  goto error;
	}
	if (interval <= 0) {
	  Tcl_AppendResult(interp, "invalid timer interval value \"",
			   sinterval, "\"", NULL);
	}
	
	if ( progressPtr->timerRunning != 0 ) {
	  Tk_DeleteTimerHandler(progressPtr->timer);
	  progressPtr->timerRunning = 0;
	}

	progressPtr->timerInterval = interval;
	progressPtr->timerRunning = 1;
	Tk_DoWhenIdle(ProgressTimer, (ClientData) progressPtr);

    } else if ((c == 's') && (strncmp(argv[1], "step", length) == 0)) {
	char *sstep;
	int step;
	if (argc == 2) {
	  sstep = DEF_PROGRESS_STEP;
	} else if (argc == 3) {
	  sstep = argv[2];
	} else {
	  Tcl_AppendResult(interp, "wrong # args: should be \"",
			   argv[0], " stop ?value?\"",
			   (char *) NULL);
	  goto error;
	}
	if (TCL_OK != Tcl_GetInt( interp, sstep, &step)) {
	  goto error;
	}

	progressPtr->step = step;
	
	progressPtr->value += progressPtr->step;
	ProgressPostRedisplay( progressPtr );
	
	/* set bound variable if defined - don't care about errors */
	if ( progressPtr->varName != NULL ) {
	  char svalue[16];
	  sprintf( svalue, "%d", progressPtr->value );
	  Tcl_SetVar( progressPtr->interp, progressPtr->varName,
		      svalue, TCL_GLOBAL_ONLY);
	}
	
    } else if ((c == 's') && (strncmp(argv[1], "stop", length) == 0)) {
      if (argc != 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
			 argv[0], " stop", (char *) NULL);
	return TCL_ERROR;
      }

      if ( progressPtr->timerRunning != 0 ) {
	Tk_DeleteTimerHandler(progressPtr->timer);
	progressPtr->timerRunning = 0;
      }

    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\":  must be cget or configure", (char *) NULL);
	goto error;
    }
    Ck_Release((ClientData) progressPtr);
    return result;

error:
    Ck_Release((ClientData) progressPtr);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyProgress --
 *
 *	This procedure is invoked by Ck_EventuallyFree or Ck_Release
 *	to clean up the internal structure of a progress at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the progress is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
DestroyProgress(clientData)
    ClientData clientData;	/* Info about progress widget. */
{
    Progress *progressPtr = (Progress *) clientData;

    /* kill running timer */
    if (progressPtr->timerRunning) {
      Tk_DeleteTimerHandler(progressPtr->timer);
      progressPtr->timerRunning = 0;
    }

    /* remove trace on variable */
    if (progressPtr->varName != NULL) {
      Tcl_UntraceVar(progressPtr->interp, progressPtr->varName,
		     TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		     ProgressVarProc, (ClientData) progressPtr);
      progressPtr->varName = NULL;
    }
    
    Ck_FreeOptions(configSpecs, (char *) progressPtr, 0);
    ckfree((char *) progressPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ProgressCmdDeletedProc --
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
ProgressCmdDeletedProc(clientData)
    ClientData clientData;      /* Pointer to widget record for widget. */
{
    Progress *progressPtr = (Progress *) clientData;
    CkWindow *winPtr = progressPtr->winPtr;

    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case tkwin
     * is NULL) or because the command was deleted, and then this procedure
     * destroys the widget.
     */

    if (winPtr != NULL) {
        progressPtr->winPtr = NULL;
        Ck_DestroyWindow(winPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureProgress --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the option database, in order to configure (or
 *	reconfigure) a progress widget.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for progressPtr;  old resources get freed, if there
 *	were any.
 *
 *----------------------------------------------------------------------
 */

static int
ConfigureProgress(interp, progressPtr, argc, argv, flags)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Progress *progressPtr;	/* Information about widget;  may or may
				 * not already have values for some fields. */
    int argc;			/* Number of valid entries in argv. */
    char **argv;		/* Arguments. */
    int flags;			/* Flags to pass to Tk_ConfigureWidget. */
{
  /* 
   * Remove the trace otherwise it will be added again and again 
   */
  if (progressPtr->varName != NULL ) {
    Tcl_UntraceVar(progressPtr->interp, progressPtr->varName,
		   TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		   ProgressVarProc, (ClientData) progressPtr);
  }
  
  if (Ck_ConfigureWidget(interp, progressPtr->winPtr, configSpecs,
			 argc, argv, (char *) progressPtr, flags) != TCL_OK) {
    return TCL_ERROR;
  }

  Ck_SetWindowAttr(progressPtr->winPtr, progressPtr->fg, progressPtr->bg,
		   progressPtr->attr);
  Ck_SetInternalBorder(progressPtr->winPtr, progressPtr->borderPtr != NULL);

  /*
   * If the progress bar is to display the value of a variable, then set up
   * a trace on the variable's value, create the variable if it doesn't
   * exist, and fetch its current value.
   */

  if (progressPtr->varName != NULL) {
    char *value;
	
    value = Tcl_GetVar(interp, progressPtr->varName, TCL_GLOBAL_ONLY);
    if (value == NULL) {
      char svalue[16];
      sprintf( svalue, "%d", progressPtr->value);
      Tcl_SetVar(interp, progressPtr->varName, svalue, TCL_GLOBAL_ONLY);
    } else {
      int ivalue;
      if (TCL_OK != Tcl_GetInt( interp, value, &ivalue)) {
	Tk_BackgroundError(progressPtr->interp);
      }
      else {
	progressPtr->value = ivalue;
      }
    }
    Tcl_TraceVar(interp, progressPtr->varName,
		 TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		 ProgressVarProc, (ClientData) progressPtr);
  }


  if ((progressPtr->width > 0) || (progressPtr->height > 0))
    Ck_GeometryRequest(progressPtr->winPtr, progressPtr->width,
		       progressPtr->height);

  ProgressPostRedisplay( progressPtr );
  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DisplayProgress --
 *
 *	This procedure is invoked to display a progress widget.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Commands are output to display the progress in its
 *	current mode.
 *
 *----------------------------------------------------------------------
 */

static void
DisplayProgress(clientData)
    ClientData clientData;	/* Information about widget. */
{
    static Ck_Uid horizontalUid = NULL;

    Progress *progressPtr = (Progress *) clientData;
    CkWindow *winPtr = progressPtr->winPtr;

    int row, col, nw, nh, off, value;
    
    progressPtr->flags &= ~REDRAW_PENDING;
    if ((progressPtr->winPtr == NULL) || !(winPtr->flags & CK_MAPPED)) {
	return;
    }
    Ck_ClearToBot(winPtr, 0, 0);
    if (progressPtr->borderPtr != NULL) {
	Ck_DrawBorder(winPtr, progressPtr->borderPtr,
		      0, 0, winPtr->width, winPtr->height);
    }

    if ( horizontalUid == NULL ) {
      horizontalUid = Ck_GetUid(PROGRESS_HORIZONTAL);
    }

    value = progressPtr->value;

    /* in determinate mode use value */
    if ( value > progressPtr->maximum ) {
      value = progressPtr->maximum;
    }
    if ( value < 0 ) {
      value = 0;
    }

    /* in undeterminate mode */
    if ( 0 ) {
      value = value % progressPtr->maximum;
    }

    /* drawing depends on border presence */
    off = (progressPtr->borderPtr != NULL) ? 1 : 0;

    if ( progressPtr->orientUid == horizontalUid ) {
      nw  = (value * (progressPtr->winPtr->width-off)) / progressPtr->maximum;
      nh  = progressPtr->height - off; 
      for (row = off; row < nh; row++) {
	wmove( progressPtr->winPtr->window, row, off );
	for (col = off; col < nw; col++) {
	  waddch( progressPtr->winPtr->window, '|');
	}
      }
    }
    else {
      // @todo : le dessin va etre a l'envers
      nw  = progressPtr->width - off; 
      nh  = (value * (progressPtr->winPtr->height-off)) / progressPtr->maximum;
      for (row = off; row < nh; row++) {
	wmove( progressPtr->winPtr->window, row, off );
	for (col = off; col < nw; col++) {
	  waddch( progressPtr->winPtr->window, '-');
	}
      }
    }
    
    Ck_EventuallyRefresh(winPtr);
}

/*
 *--------------------------------------------------------------
 *
 * ProgressEventProc --
 *
 *	This procedure is invoked by the dispatcher on
 *	structure changes to a progress.
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
ProgressEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window. */
    CkEvent *eventPtr;		/* Information about event. */
{
    Progress *progressPtr = (Progress *) clientData;

    if (eventPtr->type == CK_EV_EXPOSE && progressPtr->winPtr != NULL &&
	!(progressPtr->flags & REDRAW_PENDING)) {
	Tk_DoWhenIdle(DisplayProgress, (ClientData) progressPtr);
	progressPtr->flags |= REDRAW_PENDING;
    } else if (eventPtr->type == CK_EV_DESTROY) {
        if (progressPtr->winPtr != NULL) {
            progressPtr->winPtr = NULL;
            Tcl_DeleteCommand(progressPtr->interp,
                    Tcl_GetCommandName(progressPtr->interp, progressPtr->widgetCmd));
        }
	if (progressPtr->flags & REDRAW_PENDING)
	    Tk_CancelIdleCall(DisplayProgress, (ClientData) progressPtr);
	Ck_EventuallyFree((ClientData) progressPtr, (Ck_FreeProc *) DestroyProgress);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ProgressPostRedisplay --
 *
 *      This procedure is invoked to ask for a redisplay.
 *      If a redisplay is already pending, this procedure does nothing.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Push idle events to event queue.
 *
 *----------------------------------------------------------------------
 */

static void
ProgressPostRedisplay(progressPtr)
     Progress *progressPtr;      /* Info about widget. */
{
  if ((progressPtr->winPtr->flags & CK_MAPPED)
      && !(progressPtr->flags & REDRAW_PENDING)) {
    Tk_DoWhenIdle(DisplayProgress, (ClientData) progressPtr);
    progressPtr->flags |= REDRAW_PENDING;
  }
}

/*
 *----------------------------------------------------------------------
 *
 * ProgressTimer --
 *
 *      This procedure is invoked to auto-increment the current value
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Reschedule a call to itself.
 *
 *----------------------------------------------------------------------
 */

static void
ProgressTimer(clientData)
     ClientData clientData;	/* information about widget */
{
  Progress *progressPtr = (Progress *) clientData;
  
  progressPtr->value += progressPtr->step;
  ProgressPostRedisplay( progressPtr );

  /* set bound variable if defined - don't care about errors */
  if ( progressPtr->varName != NULL ) {
    char svalue[16];
    sprintf( svalue, "%d", progressPtr->value );
    Tcl_SetVar( progressPtr->interp, progressPtr->varName,
		svalue, TCL_GLOBAL_ONLY);
  }

  /* reschedule next call if timer is active */
  if ( progressPtr->timerRunning ) {
    progressPtr->timer =
      Tk_CreateTimerHandler(progressPtr->timerInterval, ProgressTimer,
			    (ClientData) progressPtr);
  }
}

/*
 *----------------------------------------------------------------------
 *
 *  ValuePrintProc --
 *
 *      This procedure returns the value of the "-value" configuration
 *      option.
 *
 *----------------------------------------------------------------------
 */
static char*
ValuePrintProc(clientData, winPtr, widgRec, offset, freeProcPtr)
     ClientData clientData;
     CkWindow *winPtr;
     char *widgRec;
     int offset;
     Tcl_FreeProc **freeProcPtr;
{
  Progress *progressPtr = (Progress *) widgRec;
  char *buffer = (char*) Tcl_Alloc(16);

  if ( buffer == NULL ) {
    Tcl_Panic("memory allocation error at %s:%d", __FILE__, __LINE__);
  }

  if ( freeProcPtr != NULL ) {
    *freeProcPtr = Tcl_Free;
  }
  sprintf( buffer, "%d", progressPtr->value );
  return buffer;
}

/*
 *----------------------------------------------------------------------
 *
 *  ValueParseProc --
 *
 *----------------------------------------------------------------------
 */
static int
ValueParseProc( clientData, interp, winPtr, value, widgetRec, offset)
     ClientData clientData; 
     Tcl_Interp *interp;    /* Used for error reporting */
     CkWindow *winPtr;
     char *value;
     char *widgetRec;
     int offset;
{
  Progress *progressPtr = (Progress *) widgetRec;
  int ivalue;

  if ( value == NULL ) {
    value = DEF_PROGRESS_VALUE;
    return ValueParseProc( clientData, interp, winPtr,
			   value, widgetRec, offset );
  }

  if ( TCL_OK != Tcl_GetInt( interp, value, &ivalue )) {
    return TCL_ERROR;
  }
  if ( ivalue < 0 ) {
    Tcl_AppendResult( interp, "value '", value, "' out of range", NULL );
    return TCL_ERROR;
  }

  if ( ivalue != progressPtr->value ) {
    progressPtr->value = ivalue;
    ProgressPostRedisplay( progressPtr );

    /* set bound variable if defined - don't care about errors */
    if ( progressPtr->varName != NULL ) {
      char svalue[16];
      sprintf( svalue, "%d", progressPtr->value);
      Tcl_SetVar( progressPtr->interp, progressPtr->varName,
		  svalue, TCL_GLOBAL_ONLY);
    }
  }

  return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * ProgressVarProc --
 *
 *	This procedure is invoked when someone changes the variable
 *	whose contents are to be displayed in a progress bar.
 *
 * Results:
 *	NULL is always returned.
 *
 * Side effects:
 *	The progress bar will change to match the variable.
 *      A background error might be raised if the value
 *      of the variable is not an integer.
 *
 *--------------------------------------------------------------
 */

static char *
ProgressVarProc(clientData, interp, name1, name2, flags)
     ClientData clientData;	/* Information about button. */
     Tcl_Interp *interp;	/* Interpreter containing variable. */
     char *name1;		/* Name of variable. */
     char *name2;		/* Second part of variable name. */
     int flags;			/* Information about what happened. */
{
  Progress *progressPtr = (Progress *) clientData;
  char *value;
  int ivalue;

  /*
   * If the variable is unset, then immediately recreate it unless
   * the whole interpreter is going away.
   */

  if (flags & TCL_TRACE_UNSETS) {
    if ((flags & TCL_TRACE_DESTROYED) && !(flags & TCL_INTERP_DESTROYED)) {
      char svalue[16];
      sprintf(svalue, "%d", progressPtr->value );
      Tcl_SetVar2(interp, name1, name2, svalue, flags & TCL_GLOBAL_ONLY);
      Tcl_TraceVar2(interp, name1, name2,
		    TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		    ProgressVarProc, clientData);
    }
    return (char *) NULL;
  }
    
  value = Tcl_GetVar2(interp, name1, name2, flags & TCL_GLOBAL_ONLY);
  if ((value == NULL) || (TCL_OK != Tcl_GetInt(interp, value, &ivalue))) {
    Tk_BackgroundError(interp);
    return (char *) NULL;
  }

  if ( progressPtr->value != ivalue ) {
    progressPtr->value = ivalue;
    ProgressPostRedisplay( progressPtr );
  }
  
  return (char *) NULL;
}
