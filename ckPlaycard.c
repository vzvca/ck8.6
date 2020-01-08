/* 
 * ckPlayCard.c --
 *
 *	This module implements "playcard" widget for the
 *	toolkit.  Playcard are windows with a background color and foreground
 *	color for both front and back. They have a border and a default width
 *      and height. Their intended use is for implementing card games.
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

#include <locale.h>

/*
 * A data structure of the following type is kept for each
 * playcard that currently exists for this process:
 */

typedef struct {
  CkWindow *winPtr;		/* Window that embodies the playcard.  NULL
				 * means that the window has been destroyed
				 * but the data structures haven't yet been
				 * cleaned up.*/
  Tcl_Interp *interp;		/* Interpreter associated with widget.
    				 * Used to delete widget command.  */
  Tcl_Command widgetCmd;      	/* Token for playcard's widget command. */
  CkBorder *borderPtr;		/* Structure used to draw border. */
  int fg, bg;			/* Foreground/background colors. */
  int attr;			/* Video attributes. */
  int width;			/* Width to request for window.  <= 0 means
				 * don't request any size. */
  int height;			/* Height to request for window.  <= 0 means
				 * don't request any size. */
  char *takeFocus;		/* Value of -takefocus option. */
  int flags;			/* Various flags;  see below for
				 * definitions. */
  Ck_Uid suit;			/* spade, heart, diamond or club */
  Ck_Uid rank;			/* rank of the card 2..10, jake, queen, king, ace */
  int side;                     /* current side shown back or front */
} Playcard;

/*
 * Flag bits for playcards:
 *
 * REDRAW_PENDING:		Non-zero means a DoWhenIdle handler
 *				has already been queued to redraw
 *				this window.
 */

#define REDRAW_PENDING		1


/*
 * Custom option "-rank"
 */
static int RankParseProc(ClientData clientData,
			 Tcl_Interp *interp, CkWindow *winPtr,
			 char *value, char *widgRec, int offset);

static char *RankPrintProc(ClientData clientData,
			   CkWindow *winPtr, char *widgRec, int offset,
			   Tcl_FreeProc **freeProcPtr);

static Ck_CustomOption RankCustomOption =
  { RankParseProc, RankPrintProc, NULL };

/*
 * Custom option "-suit"
 */
static int SuitParseProc(ClientData clientData,
			 Tcl_Interp *interp, CkWindow *winPtr,
			 char *value, char *widgRec, int offset);

static char *SuitPrintProc(ClientData clientData,
			   CkWindow *winPtr, char *widgRec, int offset,
			   Tcl_FreeProc **freeProcPtr);

static Ck_CustomOption SuitCustomOption =
  { SuitParseProc, SuitPrintProc, NULL };

/*
 * Custom option "-side"
 */
static int SideParseProc(ClientData clientData,
			 Tcl_Interp *interp, CkWindow *winPtr,
			 char *value, char *widgRec, int offset);

static char *SidePrintProc(ClientData clientData,
			   CkWindow *winPtr, char *widgRec, int offset,
			   Tcl_FreeProc **freeProcPtr);

static Ck_CustomOption SideCustomOption =
  { SideParseProc, SidePrintProc, NULL };


static Ck_ConfigSpec configSpecs[] = {
    {CK_CONFIG_ATTR, "-attributes", "attributes", "Attributes",
	DEF_PLAYCARD_ATTRIB, Ck_Offset(Playcard, attr), 0},
    {CK_CONFIG_COLOR, "-background", "background", "Background",
	DEF_PLAYCARD_BG_COLOR, Ck_Offset(Playcard, bg), CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-background", "background", "Background",
	DEF_PLAYCARD_BG_MONO, Ck_Offset(Playcard, bg), CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_SYNONYM, "-bg", "background", (char *) NULL,
	(char *) NULL, 0, 0},
    {CK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_PLAYCARD_FG_COLOR, Ck_Offset(Playcard, fg), CK_CONFIG_COLOR_ONLY},
    {CK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_PLAYCARD_FG_MONO, Ck_Offset(Playcard, fg), CK_CONFIG_MONO_ONLY},
    {CK_CONFIG_SYNONYM, "-fg", "foreground", (char *) NULL,
	(char *) NULL, 0, 0},
    {CK_CONFIG_COORD, "-height", "height", "Height",
	DEF_PLAYCARD_HEIGHT, Ck_Offset(Playcard, height), 0},
    {CK_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_PLAYCARD_TAKE_FOCUS, Ck_Offset(Playcard, takeFocus),
	CK_CONFIG_NULL_OK},
    {CK_CONFIG_COORD, "-width", "width", "Width",
	DEF_PLAYCARD_WIDTH, Ck_Offset(Playcard, width), 0},
    {CK_CONFIG_CUSTOM, "-side", "side", "Side",
     	DEF_PLAYCARD_SIDE, Ck_Offset(Playcard, side),
        CK_CONFIG_NULL_OK, &SideCustomOption },
    {CK_CONFIG_CUSTOM, "-suit", "suit", "Suit",
     	DEF_PLAYCARD_SUIT, Ck_Offset(Playcard, suit),
     	0, &SuitCustomOption },
    {CK_CONFIG_CUSTOM, "-rank", "rank", "Rank",
        DEF_PLAYCARD_RANK, Ck_Offset(Playcard, rank),
     	0, &RankCustomOption},
    {CK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * Constants used later in this file:
 */
#define PLAYCARD_BORDER \
  "ulcorner hline urcorner vline lrcorner hline llcorner vline"

#define PLAYCARD_FRONT  0
#define PLAYCARD_BACK   1

#define PLAYCARD_SPADE   L"\u2660"
#define PLAYCARD_HEART   L"\u2665"
#define PLAYCARD_DIAMOND L"\u2666"
#define PLAYCARD_CLUB    L"\u2663"

#define PLAYCARD_ACE     "ace"
#define PLAYCARD_KING    "king"
#define PLAYCARD_QUEEN   "queen"
#define PLAYCARD_JAKE    "jake"
#define PLAYCARD_10      "10"
#define PLAYCARD_9       "9"
#define PLAYCARD_8       "8"
#define PLAYCARD_7       "7"
#define PLAYCARD_6       "6"
#define PLAYCARD_5       "5"
#define PLAYCARD_4       "4"
#define PLAYCARD_3       "3"
#define PLAYCARD_2       "2"


/*
 * The following UIDs are here to speed up comparisons
 */
static Ck_Uid card_spade   = NULL;
static Ck_Uid card_heart   = NULL;
static Ck_Uid card_diamond = NULL;
static Ck_Uid card_club    = NULL;
static Ck_Uid card_ace   = NULL;
static Ck_Uid card_king  = NULL;
static Ck_Uid card_queen = NULL;
static Ck_Uid card_jake  = NULL;
static Ck_Uid card_10    = NULL;
static Ck_Uid card_9     = NULL;
static Ck_Uid card_8     = NULL;
static Ck_Uid card_7     = NULL;
static Ck_Uid card_6     = NULL;
static Ck_Uid card_5     = NULL;
static Ck_Uid card_4     = NULL;
static Ck_Uid card_3     = NULL;
static Ck_Uid card_2     = NULL;

/*
 * Forward declarations for procedures defined later in this file:
 */

static int	ConfigurePlaycard _ANSI_ARGS_((Tcl_Interp *interp,
		    Playcard *playcardPtr, int argc, char **argv, int flags));
static void	DestroyPlaycard _ANSI_ARGS_((ClientData clientData));
static void     PlaycardCmdDeletedProc _ANSI_ARGS_((ClientData clientData));
static void	DisplayPlaycard _ANSI_ARGS_((ClientData clientData));
static void	PlaycardEventProc _ANSI_ARGS_((ClientData clientData,
		    CkEvent *eventPtr));
static int	PlaycardWidgetCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));
static void     PlaycardInitUids _ANSI_ARGS_((void));
static int      PlaycardInit _ANSI_ARGS_((Tcl_Interp *interp,
		    CkWindow *winPtr, int argc, char *argv[]));


/*
 *--------------------------------------------------------------
 *
 * Ck_PlaycardCmd --
 *
 *	This procedure is invoked to process the "playcard"
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
Ck_PlaycardCmd(clientData, interp, argc, argv)
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

    /* 
     * Initialize Uids
     */
    PlaycardInitUids();

    /*
     * Check number of args
     */
    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " pathName ?options?\"", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * The code below is a special hack that extracts a few key
     * options from the argument list now, rather than letting
     * ConfigurePlaycard do it.  This is necessary because we have
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
            className = "Playcard";
        }
    }
    Ck_SetClass(new, className);
    return PlaycardInit(interp, new, argc-2, argv+2);
}

/*
 *----------------------------------------------------------------------
 *
 * PlaycardInit --
 *
 *	This procedure initializes a playcard widget.  It's
 *	separate from Ck_PlaycardCmd so that it can be used for the
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
PlaycardInit(interp, winPtr, argc, argv)
    Tcl_Interp *interp;			/* Interpreter associated with the
					 * application. */
    CkWindow *winPtr;			/* Window to use for playcard.
					 * Caller must already
					 * have set window's class. */
    int argc;				/* Number of configuration arguments
					 * (not including class command and
					 * window name). */
    char *argv[];			/* Configuration arguments. */
{
    Playcard *playcardPtr;

    playcardPtr = (Playcard *) ckalloc(sizeof (Playcard));
    playcardPtr->winPtr = winPtr;
    playcardPtr->interp = interp;
    playcardPtr->widgetCmd = Tcl_CreateCommand(interp,
        playcardPtr->winPtr->pathName, PlaycardWidgetCmd,
	    (ClientData) playcardPtr, PlaycardCmdDeletedProc);
    playcardPtr->borderPtr = Ck_GetBorder(interp, PLAYCARD_BORDER);
    playcardPtr->fg = 0;
    playcardPtr->bg = 0;
    playcardPtr->attr = 0;
    playcardPtr->width = 1;
    playcardPtr->height = 1;
    playcardPtr->takeFocus = NULL;
    playcardPtr->flags = 0;
    playcardPtr->side = 0;
    playcardPtr->suit = NULL;
    playcardPtr->rank = NULL;
    Ck_CreateEventHandler(playcardPtr->winPtr,
    	    CK_EV_MAP | CK_EV_EXPOSE | CK_EV_DESTROY,
	    PlaycardEventProc, (ClientData) playcardPtr);
    if (ConfigurePlaycard(interp, playcardPtr, argc, argv, 0) != TCL_OK) {
	Ck_DestroyWindow(playcardPtr->winPtr);
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, Tcl_NewStringObj(playcardPtr->winPtr->pathName,-1));
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * PlaycardWidgetCmd --
 *
 *	This procedure is invoked to process the Tcl command
 *	that corresponds to a playcard widget.  See the user
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
PlaycardWidgetCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Information about playcard widget. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Playcard *playcardPtr = (Playcard *) clientData;
    int result = TCL_OK;
    int length;
    char c;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " option ?arg arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    Ck_Preserve((ClientData) playcardPtr);
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
        result = Ck_ConfigureValue(interp, playcardPtr->winPtr, configSpecs,
                (char *) playcardPtr, argv[2], 0);
    } else if ((c == 'c') && (strncmp(argv[1], "configure", length) == 0)) {
	if (argc == 2) {
	    result = Ck_ConfigureInfo(interp, playcardPtr->winPtr, configSpecs,
		    (char *) playcardPtr, (char *) NULL, 0);
	} else if (argc == 3) {
	    result = Ck_ConfigureInfo(interp, playcardPtr->winPtr, configSpecs,
		    (char *) playcardPtr, argv[2], 0);
	} else {
	    result = ConfigurePlaycard(interp, playcardPtr, argc-2, argv+2,
		    CK_CONFIG_ARGV_ONLY);
	}
    } else if ((c == 's') && (strncmp(argv[1], "show", length) == 0)) {
      
    } else if ((c == 'f') && (strncmp(argv[1], "flip", length) == 0)) {
      
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\":  must be cget, configure, flip or show", (char *) NULL);
	goto error;
    }
    Ck_Release((ClientData) playcardPtr);
    return result;

error:
    Ck_Release((ClientData) playcardPtr);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyPlaycard --
 *
 *	This procedure is invoked by Ck_EventuallyFree or Ck_Release
 *	to clean up the internal structure of a playcard at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the playcard is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
DestroyPlaycard(clientData)
    ClientData clientData;	/* Info about playcard widget. */
{
    Playcard *playcardPtr = (Playcard *) clientData;

    Ck_FreeOptions(configSpecs, (char *) playcardPtr, 0);
    ckfree((char *) playcardPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * PlaycardCmdDeletedProc --
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
PlaycardCmdDeletedProc(clientData)
    ClientData clientData;      /* Pointer to widget record for widget. */
{
    Playcard *playcardPtr = (Playcard *) clientData;
    CkWindow *winPtr = playcardPtr->winPtr;

    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case tkwin
     * is NULL) or because the command was deleted, and then this procedure
     * destroys the widget.
     */

    if (winPtr != NULL) {
        playcardPtr->winPtr = NULL;
        Ck_DestroyWindow(winPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigurePlaycard --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the option database, in order to configure (or
 *	reconfigure) a playcard widget.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for playcardPtr;  old resources get freed, if there
 *	were any.
 *
 *----------------------------------------------------------------------
 */

static int
ConfigurePlaycard(interp, playcardPtr, argc, argv, flags)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Playcard *playcardPtr;		/* Information about widget;  may or may
				 * not already have values for some fields. */
    int argc;			/* Number of valid entries in argv. */
    char **argv;		/* Arguments. */
    int flags;			/* Flags to pass to Tk_ConfigureWidget. */
{
    if (Ck_ConfigureWidget(interp, playcardPtr->winPtr, configSpecs,
	    argc, argv, (char *) playcardPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }
    
    Ck_SetInternalBorder(playcardPtr->winPtr, 1);

    if ((playcardPtr->width > 0) || (playcardPtr->height > 0)) {
	Ck_GeometryRequest(playcardPtr->winPtr, playcardPtr->width,
	    playcardPtr->height);
    }
    
    if ((playcardPtr->winPtr->flags & CK_MAPPED)
	    && !(playcardPtr->flags & REDRAW_PENDING)) {
	Tk_DoWhenIdle(DisplayPlaycard, (ClientData) playcardPtr);
	playcardPtr->flags |= REDRAW_PENDING;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DisplayPlaycard --
 *
 *	This procedure is invoked to display a playcard widget.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Commands are output to display the playcard in its
 *	current mode.
 *
 *----------------------------------------------------------------------
 */

static void
DisplayPlaycard(clientData)
    ClientData clientData;	/* Information about widget. */
{
    Playcard *playcardPtr = (Playcard *) clientData;
    CkWindow *winPtr = playcardPtr->winPtr;
    int off, fg;

    playcardPtr->flags &= ~REDRAW_PENDING;
    if ((playcardPtr->winPtr == NULL) || !(winPtr->flags & CK_MAPPED)) {
	return;
    }
    

    if ( playcardPtr->side == PLAYCARD_FRONT ) {
      cchar_t c = { winPtr->attr };
      
      /* draw suit symbol */
      switch( playcardPtr->suit[0] ) {
      case 's':
	memcpy( c.chars, PLAYCARD_SPADE, sizeof(PLAYCARD_SPADE));
	fg = COLOR_BLACK;
	break;
      case 'h':
	memcpy( c.chars, PLAYCARD_HEART, sizeof(PLAYCARD_HEART));
	fg = COLOR_RED;
	break;
      case 'd':
	memcpy( c.chars, PLAYCARD_DIAMOND, sizeof(PLAYCARD_HEART));
	fg = COLOR_RED;
	break;
      case 'c':
	memcpy( c.chars, PLAYCARD_CLUB, sizeof(PLAYCARD_CLUB));
	fg = COLOR_BLACK;
	break;
      }
      
      Ck_SetWindowAttr(winPtr, fg, COLOR_WHITE, playcardPtr->attr);
      Ck_ClearToBot(winPtr, 0, 0);
      Ck_DrawBorder(winPtr, playcardPtr->borderPtr, 0, 0,
		    winPtr->width, winPtr->height);

      wmove( playcardPtr->winPtr->window, 0, playcardPtr->winPtr->width - 2);
      wadd_wch( playcardPtr->winPtr->window, &c );
      wmove( playcardPtr->winPtr->window, playcardPtr->winPtr->height - 1, 1);
      wadd_wch( playcardPtr->winPtr->window, &c );

      /* draw rank */
      off = 3;
      if (isdigit( playcardPtr->rank[0])) {
	if ( playcardPtr->rank == card_10 ) {
	  off = 4;
	}
      }
    
      wmove( playcardPtr->winPtr->window, 0, playcardPtr->winPtr->width - off);
      waddch( playcardPtr->winPtr->window, toupper(playcardPtr->rank[0]));
      if ( off == 4 ) {
	waddch( playcardPtr->winPtr->window, playcardPtr->rank[1]);
      }
      wmove( playcardPtr->winPtr->window, playcardPtr->winPtr->height - 1, 2);
      waddch( playcardPtr->winPtr->window, toupper(playcardPtr->rank[0]) );      
      if ( off == 4 ) {
	waddch( playcardPtr->winPtr->window, playcardPtr->rank[1]);
      }
    }

    if ( playcardPtr->side == PLAYCARD_BACK ) {
      Ck_SetWindowAttr(winPtr, playcardPtr->fg, playcardPtr->bg,
		       playcardPtr->attr);
      Ck_ClearToBot(winPtr, 0, 0);
      Ck_DrawBorder(winPtr, playcardPtr->borderPtr, 0, 0,
		    winPtr->width, winPtr->height);
    }
    
    /*
     * Now display the symbols for the card
     */

    Ck_EventuallyRefresh(winPtr);
}

/*
 *--------------------------------------------------------------
 *
 * PlaycardEventProc --
 *
 *	This procedure is invoked by the dispatcher on
 *	structure changes to a playcard.
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
PlaycardEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window. */
    CkEvent *eventPtr;		/* Information about event. */
{
    Playcard *playcardPtr = (Playcard *) clientData;

    if (eventPtr->type == CK_EV_EXPOSE && playcardPtr->winPtr != NULL &&
	!(playcardPtr->flags & REDRAW_PENDING)) {
	Tk_DoWhenIdle(DisplayPlaycard, (ClientData) playcardPtr);
	playcardPtr->flags |= REDRAW_PENDING;
    } else if (eventPtr->type == CK_EV_DESTROY) {
        if (playcardPtr->winPtr != NULL) {
            playcardPtr->winPtr = NULL;
            Tcl_DeleteCommand(playcardPtr->interp,
                    Tcl_GetCommandName(playcardPtr->interp, playcardPtr->widgetCmd));
        }
	if (playcardPtr->flags & REDRAW_PENDING)
	    Tk_CancelIdleCall(DisplayPlaycard, (ClientData) playcardPtr);
	Ck_EventuallyFree((ClientData) playcardPtr, (Ck_FreeProc *) DestroyPlaycard);
    }
}

/*
 *----------------------------------------------------------------------
 *
 *  RankPrintProc --
 *
 *      This procedure returns the value of the "-rank" configuration
 *      option.
 *
 *----------------------------------------------------------------------
 */
static char*
RankPrintProc(clientData, winPtr, widgRec, offset, freeProcPtr)
     ClientData clientData;
     CkWindow *winPtr;
     char *widgRec;
     int offset;
     Tcl_FreeProc **freeProcPtr;
{
  Playcard *playcardPtr = (Playcard *) widgRec;

  if ( freeProcPtr != NULL ) {
    *freeProcPtr = NULL;
  }
  return (char*) playcardPtr->rank;
}

/*
 *----------------------------------------------------------------------
 *
 *  RankParseProc --
 *
 *----------------------------------------------------------------------
 */
static int
RankParseProc( clientData, interp, winPtr, value, widgetRec, offset)
     ClientData clientData; 
     Tcl_Interp *interp;    /* Used for error reporting */
     CkWindow *winPtr;
     char *value;
     char *widgetRec;
     int offset;
{
  Playcard *playcardPtr = (Playcard *) widgetRec;
  int ivalue, len;
  char c;

  if ( value == NULL ) {
    value = DEF_PLAYCARD_RANK;
    return RankParseProc( clientData, interp, winPtr,
			  value, widgetRec, offset );
  }

  c = value[0]; len = strlen(value);
  if (isdigit(c)) {
    if ( TCL_OK != Tcl_GetInt( interp, value, &ivalue )) {
      return TCL_ERROR;
    }
    if ( ivalue < 2 || ivalue > 10) {
      Tcl_AppendResult( interp, "playcard rank '", value, "' out of range", NULL );
      return TCL_ERROR;
    }
    playcardPtr->rank = Ck_GetUid(value);
  }
  else if ( (c == 'a') && (strncmp(value, PLAYCARD_ACE, len) == 0)) {
    playcardPtr->rank = Ck_GetUid(PLAYCARD_ACE);
  }
  else if ( (c == 'k') && (strncmp(value, PLAYCARD_KING, len) == 0)) {
    playcardPtr->rank = Ck_GetUid(PLAYCARD_KING);
  }
  else if ( (c == 'q') && (strncmp(value, PLAYCARD_QUEEN, len) == 0)) {
    playcardPtr->rank = Ck_GetUid(PLAYCARD_QUEEN);
  }
  else if ( (c == 'j') && (strncmp(value, PLAYCARD_JAKE, len) == 0)) {
    playcardPtr->rank = Ck_GetUid(PLAYCARD_JAKE);
  }
  else {
    Tcl_AppendResult( interp, "playcard value '", value, "' out of range", NULL );
    return TCL_ERROR;
  }

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 *  SuitPrintProc --
 *
 *      This procedure returns the value of the "-suit" configuration
 *      option.
 *
 *----------------------------------------------------------------------
 */
static char*
SuitPrintProc(clientData, winPtr, widgRec, offset, freeProcPtr)
     ClientData clientData;
     CkWindow *winPtr;
     char *widgRec;
     int offset;
     Tcl_FreeProc **freeProcPtr;
{
  Playcard *playcardPtr = (Playcard *) widgRec;

  if ( freeProcPtr != NULL ) {
    *freeProcPtr = NULL;
  }
  return (char*) playcardPtr->suit;
}

/*
 *----------------------------------------------------------------------
 *
 *  SuitParseProc --
 *
 *----------------------------------------------------------------------
 */
static int
SuitParseProc( clientData, interp, winPtr, value, widgetRec, offset)
     ClientData clientData; 
     Tcl_Interp *interp;    /* Used for error reporting */
     CkWindow *winPtr;
     char *value;
     char *widgetRec;
     int offset;
{
  Playcard *playcardPtr = (Playcard *) widgetRec;
  int ivalue, len;
  char c;

  if ( value == NULL ) {
    value = DEF_PLAYCARD_SUIT;
    return SuitParseProc( clientData, interp, winPtr,
			  value, widgetRec, offset );
  }

  c = value[0]; len = strlen(value);
  if ( (c == 's') && (strncmp(value, "spade", len) == 0)) {
    playcardPtr->suit = Ck_GetUid("spade");
  }
  else if ( (c == 'h') && (strncmp(value, "heart", len) == 0)) {
    playcardPtr->suit = Ck_GetUid("heart");
  }
  else if ( (c == 'd') && (strncmp(value, "diamond", len) == 0)) {
    playcardPtr->suit = Ck_GetUid("diamond");
  }
  else if ( (c == 'c') && (strncmp(value, "club", len) == 0)) {
    playcardPtr->suit = Ck_GetUid("club");
  }
  else {
    Tcl_AppendResult( interp, "invalid playcard suit '", value, "'", NULL );
    return TCL_ERROR;
  }

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 *  SidePrintProc --
 *
 *      This procedure returns the value of the "-side" configuration
 *      option.
 *
 *----------------------------------------------------------------------
 */
static char*
SidePrintProc(clientData, winPtr, widgRec, offset, freeProcPtr)
     ClientData clientData;
     CkWindow *winPtr;
     char *widgRec;
     int offset;
     Tcl_FreeProc **freeProcPtr;
{
  Playcard *playcardPtr = (Playcard *) widgRec;

  if ( freeProcPtr != NULL ) {
    *freeProcPtr = NULL;
  }
  return (playcardPtr->side == PLAYCARD_FRONT) ? "front" : "back";
}

/*
 *----------------------------------------------------------------------
 *
 *  SuitParseProc --
 *
 *----------------------------------------------------------------------
 */
static int
SideParseProc( clientData, interp, winPtr, value, widgetRec, offset)
     ClientData clientData; 
     Tcl_Interp *interp;    /* Used for error reporting */
     CkWindow *winPtr;
     char *value;
     char *widgetRec;
     int offset;
{
  Playcard *playcardPtr = (Playcard *) widgetRec;
  int ivalue, len;
  char c;

  if ( value == NULL ) {
    value = DEF_PLAYCARD_SIDE;
    return SideParseProc( clientData, interp, winPtr,
			  value, widgetRec, offset );
  }

  c = value[0]; len = strlen(value);
  if ( (c == 'f') && (strncmp(value, "front", len) == 0)) {
    playcardPtr->side = PLAYCARD_FRONT;
  }
  else if ( (c == 'b') && (strncmp(value, "back", len) == 0)) {
    playcardPtr->side = PLAYCARD_BACK;
  }
  else {
    Tcl_AppendResult( interp, "invalid playcard side '", value, "'", NULL );
    return TCL_ERROR;
  }

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * PlaycardInitUids --
 *
 *	This procedure is invoked to initialize Uids used by Playcard
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	New UIDs are created.
 *
 *----------------------------------------------------------------------
 */

static void
PlaycardInitUids()
{
  if ( card_spade != NULL ) {
    return;
  }
  
  card_spade   = Ck_GetUid("spade");
  card_heart   = Ck_GetUid("heart");
  card_diamond = Ck_GetUid("diamond");
  card_club    = Ck_GetUid("club");
  card_ace     = Ck_GetUid("ace");
  card_king    = Ck_GetUid("king");
  card_queen   = Ck_GetUid("queen");
  card_jake    = Ck_GetUid("jake");
  card_10      = Ck_GetUid("10");
  card_9       = Ck_GetUid("9");
  card_8       = Ck_GetUid("8");
  card_7       = Ck_GetUid("7");
  card_6       = Ck_GetUid("6");
  card_5       = Ck_GetUid("5");
  card_4       = Ck_GetUid("4");
  card_3       = Ck_GetUid("3");
  card_2       = Ck_GetUid("2");
}
