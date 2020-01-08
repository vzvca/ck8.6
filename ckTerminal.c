/* 

 *
 *      This module implements "terminal" widgets for the
 *      toolkit.  Terminals are windows with a background color
 *      and possibly border, but no other attributes.
 *
 *      A terminal forks a subprocess bound to a fresh pty
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 * Copyright (c) 1995 Christian Werner
 * Copyright (c) 2019 VCA
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "ckPort.h"
#include "ck.h"
#include "default.h"

extern int setenv (const char *__string, const char *__value, int __overwrite);;

/*
 * Code below is taken form MTM
 */

/* Copyright 2017 - 2019 Rob King <jking@deadpixi.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <pwd.h>
#include <signal.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

/**** CONFIGURATION
 * VTPARSER_BAD_CHAR is the character that will be displayed when
 * an application sends an invalid multibyte sequence to the terminal.
 */
#ifndef VTPARSER_BAD_CHAR
#ifdef __STDC_ISO_10646__
#define VTPARSER_BAD_CHAR ((wchar_t)0xfffd)
#else
#define VTPARSER_BAD_CHAR L'?'
#endif
#endif

/**** DATA TYPES */
#define MAXPARAM    16
#define MAXCALLBACK 128
#define MAXOSC      100
#define MAXBUF      100

typedef struct VTPARSER VTPARSER;
typedef struct STATE STATE;
typedef void (*VTCALLBACK)(VTPARSER *v, void *p, wchar_t w, wchar_t iw,
                           int argc, int *argv, const wchar_t *osc);

struct VTPARSER {
  STATE *s;
  int narg;
  int nosc;
  int args[MAXPARAM];
  int inter;
  wchar_t oscbuf[MAXOSC + 1];
  mbstate_t ms;
  void *p;
  VTCALLBACK print;
  VTCALLBACK osc;
  VTCALLBACK cons[MAXCALLBACK];
  VTCALLBACK escs[MAXCALLBACK];
  VTCALLBACK csis[MAXCALLBACK];
};

typedef enum
  {
   VTPARSER_CONTROL,
   VTPARSER_ESCAPE,
   VTPARSER_CSI,
   VTPARSER_OSC,
   VTPARSER_PRINT
  } VtEvent;

/**** FUNCTIONS */
VTCALLBACK
vtonevent(VTPARSER *vp, VtEvent t, wchar_t w, VTCALLBACK cb);

void
vtwrite(VTPARSER *vp, const char *s, size_t n);


/*** CONFIGURATION */
/* mtm by default will advertise itself as a "screen-bce" terminal.
 * This is the terminal type advertised by such programs as
 * screen(1) and tmux(1) and is a widely-supported terminal type.
 * mtm supports emulating the "screen-bce" terminal very well, and this
 * is a reasonable default.
 *
 * However, you can change the default terminal that mtm will
 * advertise itself as. There's the "mtm" terminal type that is
 * recommended for use if you know it will be available in all the
 * environments in which mtm will be used. It advertises a few
 * features that mtm has that the default "screen-bce" terminfo doesn't
 * list, meaning that terminfo-aware programs may get a small
 * speed boost.
 */
#define DEFAULT_TERMINAL "screen-bce"
#define DEFAULT_256_COLOR_TERMINAL "screen-256color-bce"

/* mtm supports a scrollback buffer, allowing users to scroll back
 * through the output history of a virtual terminal. The SCROLLBACK
 * knob controls how many lines are saved (minus however many are
 * currently displayed). 1000 seems like a good number.
 *
 * Note that every virtual terminal is sized to be at least this big,
 * so setting a huge number here might waste memory. It is recommended
 * that this number be at least as large as the largest terminal you
 * expect to use is tall.
 */
#define SCROLLBACK 1000

/* The default command prefix key, when modified by cntrl.
 * This can be changed at runtime using the '-c' flag.
 */
#define COMMAND_KEY 'b'

/* The change focus keys. */
#define MOVE_UP         CODE(KEY_UP)
#define MOVE_DOWN       CODE(KEY_DOWN)
#define MOVE_RIGHT      CODE(KEY_RIGHT)
#define MOVE_LEFT       CODE(KEY_LEFT)
#define MOVE_OTHER      KEY(L'o')

/* The split terminal keys. */
#define HSPLIT KEY(L'h')
#define VSPLIT KEY(L'v')

/* The delete terminal key. */
#define DELETE_NODE KEY(L'w')

/* The force redraw key. */
#define REDRAW KEY(L'l')

/* The scrollback keys. */
#define SCROLLUP CODE(KEY_PPAGE)
#define SCROLLDOWN CODE(KEY_NPAGE)
#define RECENTER CODE(KEY_END)

/* The path for the wide-character curses library. */
#ifndef NCURSESW_INCLUDE_H
#if defined(__APPLE__) || !defined(__linux__) || defined(__FreeBSD__)
#define NCURSESW_INCLUDE_H <curses.h>
#else
#define NCURSESW_INCLUDE_H <ncursesw/curses.h>
#endif
#endif
#include NCURSESW_INCLUDE_H

/* Includes needed to make forkpty(3) work. */
#ifndef FORKPTY_INCLUDE_H
#if defined(__APPLE__)
#define FORKPTY_INCLUDE_H <util.h>
#elif defined(__FreeBSD__)
#define FORKPTY_INCLUDE_H <libutil.h>
#else
#define FORKPTY_INCLUDE_H <pty.h>
#endif
#endif
#include FORKPTY_INCLUDE_H

/* You probably don't need to alter these much, but if you do,
 * here is where you can define alternate character sets.
 *
 * Note that if your system's wide-character implementation
 * maps directly to Unicode, the preferred Unicode characters
 * will be used automatically if your system declares such
 * support. If it doesn't declare it, define WCHAR_IS_UNICODE to
 * force Unicode to be used.
 */
#define MAXMAP 0x7f
  static wchar_t CSET_US[MAXMAP]; /* "USASCII"...really just the null table */

#if defined(__STDC_ISO_10646__) || defined(WCHAR_IS_UNICODE)
static wchar_t CSET_UK[MAXMAP] ={ /* "United Kingdom"...really just Pound Sterling */
				 [L'#'] = 0x00a3
};

static wchar_t CSET_GRAPH[MAXMAP] =
  { /* Graphics Set One */
   [L'-'] = 0x2191,
   [L'}'] = 0x00a3,
   [L'~'] = 0x00b7,
   [L'{'] = 0x03c0,
   [L','] = 0x2190,
   [L'+'] = 0x2192,
   [L'.'] = 0x2193,
   [L'|'] = 0x2260,
   [L'>'] = 0x2265,
   [L'`'] = 0x25c6,
   [L'a'] = 0x2592,
   [L'b'] = 0x2409,
   [L'c'] = 0x240c,
   [L'd'] = 0x240d,
   [L'e'] = 0x240a,
   [L'f'] = 0x00b0,
   [L'g'] = 0x00b1,
   [L'h'] = 0x2592,
   [L'i'] = 0x2603,
   [L'j'] = 0x2518,
   [L'k'] = 0x2510,
   [L'l'] = 0x250c,
   [L'm'] = 0x2514,
   [L'n'] = 0x253c,
   [L'o'] = 0x23ba,
   [L'p'] = 0x23bb,
   [L'q'] = 0x2500,
   [L'r'] = 0x23bc,
   [L's'] = 0x23bd,
   [L't'] = 0x251c,
   [L'u'] = 0x2524,
   [L'v'] = 0x2534,
   [L'w'] = 0x252c,
   [L'x'] = 0x2502,
   [L'y'] = 0x2264,
   [L'z'] = 0x2265,
   [L'_'] = L' ',
   [L'0'] = 0x25ae
  };

#else /* wchar_t doesn't map to Unicode... */

static wchar_t CSET_UK[] =
  { /* "United Kingdom"...really just Pound Sterling */
   [L'#'] = L'&'
  };

static wchar_t CSET_GRAPH[] =
  { /* Graphics Set One */
   [L'-'] = '^',
   [L'}'] = L'&',
   [L'~'] = L'o',
   [L'{'] = L'p',
   [L','] = L'<',
   [L'+'] = L'>',
   [L'.'] = L'v',
   [L'|'] = L'!',
   [L'>'] = L'>',
   [L'`'] = L'+',
   [L'a'] = L':',
   [L'b'] = L' ',
   [L'c'] = L' ',
   [L'd'] = L' ',
   [L'e'] = L' ',
   [L'f'] = L'\'',
   [L'g'] = L'#',
   [L'h'] = L'#',
   [L'i'] = L'i',
   [L'j'] = L'+',
   [L'k'] = L'+',
   [L'l'] = L'+',
   [L'm'] = L'+',
   [L'n'] = '+',
   [L'o'] = L'-',
   [L'p'] = L'-',
   [L'q'] = L'-',
   [L'r'] = L'-',
   [L's'] = L'_',
   [L't'] = L'+',
   [L'u'] = L'+',
   [L'v'] = L'+',
   [L'w'] = L'+',
   [L'x'] = L'|',
   [L'y'] = L'<',
   [L'z'] = L'>',
   [L'_'] = L' ',
   [L'0'] = L'#',
  };
#endif

#define MIN(x, y) ((x) < (y)? (x) : (y))
#define MAX(x, y) ((x) > (y)? (x) : (y))
#define CTL(x) ((x) & 0x1f)

/*** DATA TYPES */
typedef struct SCRN SCRN;
struct SCRN {
  int sy;        /* saved cursor y position */
  int sx;        /* saved cursor x position */
  int vis;
  int tos;        /* Top os screen */
  int off;        /* Offset */
  short wbg;      /* window background */
  short wfg;      /* window foreground */
  short fg;       /* current foreground color */
  short bg;       /* current background color */
  short sfg;      /* saved foreground */
  short sbg;      /* saved background */
  short sp;
  bool insert;
  bool oxenl;     /* saved xenl */
  bool xenl;
  bool saved;     /* if true saved cursor done */
  attr_t sattr;   /* saved attributes */
  WINDOW *win;    /* curses window - it is a pad */
};

typedef struct NODE NODE;
struct NODE {
  int h;        /* height of terminal */
  int w;        /* width of terminal */
  int pt;       /* Pseudo-TTY bound to subprocess (i.e. shell) */
  int ntabs;    /* number of tabs */
  bool *tabs;   /* dynamically allocated tab stops array */
  bool pnm;
  bool decom;
  bool am;
  bool lnm;
  wchar_t repc;  /* char to repeat */
  SCRN pri;      /* primary screen */
  SCRN alt;      /* alternate screen */
  SCRN *s;       /* active screen , points to pri or alt member */
  wchar_t *g0, *g1, *g2, *g3, *gc, *gs, *sgc, *sgs;
  VTPARSER vp;   /* used to parse input stream from bound pseudo-TTY */

  char iobuf[BUFSIZ];  /* buffer for character input */
  bool cmd;            /* true if in command mode */
  
  void *clientData;    /* pointer to extra data */
};

// --------------------------------------------------------------------------

/*
 * A data structure of the following type is kept for each
 * terminal that currently exists for this process:
 */

typedef struct Terminal_s {
  CkWindow *winPtr;           /* Window that embodies the terminal.  NULL
			       * means that the window has been destroyed
			       * but the data structures haven't yet been
			       * cleaned up.*/
  Tcl_Interp *interp;         /* Interpreter associated with widget.
			       * Used to delete widget command.  */
  Tcl_Command widgetCmd;      /* Token for terminal's widget command. */
  CkBorder *borderPtr;        /* Structure used to draw border. */
  int fg, bg;                 /* Foreground/background colors. */
  int attr;                   /* Video attributes. */
  int width;                  /* Width to request for window.  <= 0 means
			       * don't request any size. */
  int height;                 /* Height to request for window.  <= 0 means
			       * don't request any size. */
  char *takeFocus;            /* Value of -takefocus option. */
  int flags;                  /* Various flags;  see below for
			       * definitions. */

  char *term;                 /* terminal type a default value is computed */
  char *exec;                 /* command to execute - default to user $SHELL 
				 environment variable */
  int    nexec;               /* number of elements in splitted version of exec */
  char **aexec;               /* splitted version of exec - used for process fork */
  int scrollback;             /* size of terminal scrollback buffer */
  char *yscrollcommand;       /* y-scroll command */
  int redisplayPolicy;        /* update display policy
                               * POLICY_NONE : widget will be updated once 
                               *               all the pending input from pty 
			       *               has been preocessed.
                               * POLICY_LINE : widget will be updated 
			       *               each time a newline is received.
			       * number > 0  : widget is updated each time
                               *               this amount of bytes has been read
                               *               from the pty channel. */

  int count;                  /* received characters counter used for redisplay */
  int yview;                  /* used while scrolling through the buffer */
  Tcl_Channel tee;            /* channel where pty input stream gets duplicated */
  
  NODE *node;                 /* pointer to terminal object */
  
} Terminal;

/*
 * Flag bits for terminals:
 *
 * REDRAW_PENDING:              Non-zero means a DoWhenIdle handler
 *                              has already been queued to redraw
 *                              this window.
 *
 * DISCONNECTED:                Bound subprocess (shell) is finished
 *
 * MODE_INTERACT:               The user interacts with the terminal
 *                              using mouse and keyboard. Keys and mouse events
 *                              are sent to the subprocess (through opened pty)
 * 
 * MODE_MOVE:                   The user interacts with the terminal
 *                              using mouse and keyboard. BUT Keys and mouse events
 *                              are not sent to the forked subprocess.
 *                              They are interpreted to move around in the buffer.
 *
 * MODE_EXPECT:                 The user doesn't interact with the terminal.
 *                              the interaction is delegated to a TCL script
 *                              or C function that automates the dialog.
 *
 */

#define REDRAW_PENDING          1
#define DISCONNECTED            2
#define MODE_INTERACT           4
#define MODE_MOVE               8
#define MODE_EXPECT             16

static int ExecParseProc(ClientData clientData,
			 Tcl_Interp *interp, CkWindow *winPtr,
			 char *value, char *widgRec, int offset);

static char *ExecPrintProc(ClientData clientData,
			   CkWindow *winPtr, char *widgRec, int offset,
			   Tcl_FreeProc **freeProcPtr);

static Ck_CustomOption ExecCustomOption =
  { ExecParseProc, ExecPrintProc, NULL };

static int TermParseProc(ClientData clientData,
			 Tcl_Interp *interp, CkWindow *winPtr,
			 char *value, char *widgRec, int offset);

static char *TermPrintProc(ClientData clientData,
			   CkWindow *winPtr, char *widgRec, int offset,
			   Tcl_FreeProc **freeProcPtr);

static Ck_CustomOption TermCustomOption =
  { TermParseProc, TermPrintProc, NULL };

static int RedisplayPolicyParseProc(ClientData clientData,
				    Tcl_Interp *interp, CkWindow *winPtr,
				    char *value, char *widgRec, int offset);

static char *RedisplayPolicyPrintProc(ClientData clientData,
				      CkWindow *winPtr, char *widgRec, int offset,
				      Tcl_FreeProc **freeProcPtr);

static Ck_CustomOption RedisplayPolicyCustomOption =
  { RedisplayPolicyParseProc, RedisplayPolicyPrintProc, NULL };

static Ck_ConfigSpec configSpecs[] = {
    {CK_CONFIG_ATTR, "-attributes", "attributes", "Attributes",
     DEF_TERMINAL_ATTRIB, Ck_Offset(Terminal, attr), 0},
    
    {CK_CONFIG_COLOR, "-background", "background", "Background",
     DEF_TERMINAL_BG_COLOR, Ck_Offset(Terminal, bg), CK_CONFIG_COLOR_ONLY},
    
    {CK_CONFIG_COLOR, "-background", "background", "Background",
     DEF_TERMINAL_BG_MONO, Ck_Offset(Terminal, bg), CK_CONFIG_MONO_ONLY},
    
    {CK_CONFIG_SYNONYM, "-bg", "background", (char *) NULL,
     (char *) NULL, 0, 0},
    
    {CK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
     DEF_TERMINAL_FG_COLOR, Ck_Offset(Terminal, fg), CK_CONFIG_COLOR_ONLY},
    
    {CK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
     DEF_TERMINAL_FG_MONO, Ck_Offset(Terminal, fg), CK_CONFIG_MONO_ONLY},
    
    {CK_CONFIG_SYNONYM, "-fg", "foreground", (char *) NULL,
     (char *) NULL, 0, 0},
    
    {CK_CONFIG_BORDER, "-border", "border", "Border",
     DEF_TERMINAL_BORDER, Ck_Offset(Terminal, borderPtr), CK_CONFIG_NULL_OK},
    
    {CK_CONFIG_COORD, "-height", "height", "Height",
     DEF_TERMINAL_HEIGHT, Ck_Offset(Terminal, height), 0},
    
    {CK_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
     DEF_TERMINAL_TAKE_FOCUS, Ck_Offset(Terminal, takeFocus),
     CK_CONFIG_NULL_OK},
    
    {CK_CONFIG_COORD, "-width", "width", "Width",
     DEF_TERMINAL_WIDTH, Ck_Offset(Terminal, width), 0},
    
    {CK_CONFIG_CUSTOM, "-exec", "exec", "Exec",
     DEF_TERMINAL_EXEC, Ck_Offset(Terminal, exec),
     CK_CONFIG_NULL_OK, &ExecCustomOption },
    
    {CK_CONFIG_CUSTOM, "-term", "term", "Term",
     DEF_TERMINAL_TERM, Ck_Offset(Terminal, term),
     CK_CONFIG_NULL_OK, &TermCustomOption },
    
    {CK_CONFIG_CUSTOM, "-redisplay", "redisplay", "Redisplay",
     DEF_TERMINAL_REDISPLAY, Ck_Offset(Terminal, redisplayPolicy),
     CK_CONFIG_NULL_OK, &RedisplayPolicyCustomOption },
    
    {CK_CONFIG_INT, "-scrollback", "scrollback", "Scrollback",
     DEF_TERMINAL_SCROLLBACK, Ck_Offset(Terminal, scrollback), 0},
    
    {CK_CONFIG_STRING, "-yscrollcommand", "yscrollcommand", "YScrollCommand",
     (char*) NULL, Ck_Offset(Terminal, yscrollcommand), CK_CONFIG_NULL_OK},
    
    {CK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
        (char *) NULL, 0, 0}
};

/*
 * Forward declarations for procedures defined later in this file:
 */

static int      ConfigureTerminal _ANSI_ARGS_((Tcl_Interp *interp,
                    Terminal *terminalPtr, int argc, char **argv, int flags));
static void     DestroyTerminal _ANSI_ARGS_((ClientData clientData));
static void     TerminalCmdDeletedProc _ANSI_ARGS_((ClientData clientData));
static void     DisplayTerminal _ANSI_ARGS_((ClientData clientData));
static void     TerminalEventProc _ANSI_ARGS_((ClientData clientData,
                    CkEvent *eventPtr));
static int      TerminalWidgetCmd _ANSI_ARGS_((ClientData clientData,
                    Tcl_Interp *interp, int argc, char **argv));

static void     TerminalKeyEventProc _ANSI_ARGS_((ClientData clientData,
						  CkEvent *eventPtr));
static void     TerminalPtyProc _ANSI_ARGS_((ClientData clientData, int flags));
static void     SendToTerminal  _ANSI_ARGS_((Terminal *terminalPtr, char *text));
static void     TerminalGiveFocus  _ANSI_ARGS_((Terminal *terminalPtr));
static void     TerminalPostRedisplay  _ANSI_ARGS_((Terminal *terminalPtr));
static void     TerminalYScrollCommand  _ANSI_ARGS_((ClientData clientData));
static int      TerminalYView _ANSI_ARGS_((Terminal *terminalPtr,
					   int argc, char **argv));
static int      TerminalTee _ANSI_ARGS_((Terminal *terminalPtr,
					 int argc, char **argv));
// --------------------------------------------------------------------------

/*** GLOBALS AND PROTOTYPES */
static int commandkey = CTL(COMMAND_KEY);

static void setupevents(NODE *n);
static void reshape(NODE *n, int h, int w);
static void draw(NODE *n);
static void freenode(NODE *n);
static void fixcursor(NODE *n);

static void
safewrite(int fd, const char *b, size_t n) /* Write, checking for errors. */
{
  size_t w = 0;
  while (w < n){
    ssize_t s = write(fd, b + w, n - w);
    if (s < 0 && errno != EINTR)
      return;
    else if (s < 0)
      s = 0;
    w += (size_t)s;
  }
}


/*** TERMINAL EMULATION HANDLERS
 * These functions implement the various terminal commands activated by
 * escape sequences and printing to the terminal. Large amounts of boilerplate
 * code is shared among all these functions, and is factored out into the
 * macros below:
 *      PD(n, d)       - Parameter n, with default d.
 *      P0(n)          - Parameter n, default 0.
 *      P1(n)          - Parameter n, default 1.
 *      CALL(h)        - Call handler h with no arguments.
 *      SENDN(n, s, c) - Write string c bytes of s to n.
 *      SEND(n, s)     - Write string s to node n's host.
 *      (END)HANDLER   - Declare/end a handler function
 *      COMMONVARS     - All of the common variables for a handler.
 *                       x, y     - cursor position
 *                       mx, my   - max possible values for x and y
 *                       px, py   - physical cursor position in scrollback
 *                       n        - the current node
 *                       win      - the current window
 *                       top, bot - the scrolling region
 *                       tos      - top of the screen in the pad
 *                       s        - the current SCRN buffer
 * The funny names for handlers are from their ANSI/ECMA/DEC mnemonics.
 */
#define PD(x, d) (argc < (x) || !argv? (d) : argv[(x)])
#define P0(x) PD(x, 0)
#define P1(x) (!P0(x)? 1 : P0(x))
#define CALL(x) (x)(v, n, 0, 0, 0, NULL, NULL)
#define SENDN(n, s, c) safewrite(n->pt, s, c)
#define SEND(n, s) SENDN(n, s, strlen(s))
#define COMMONVARS						\
  NODE *n = (NODE *)p;						\
  SCRN *s = n->s;						\
  WINDOW *win = s->win;						\
  Terminal *term = (Terminal *) n->clientData;			\
  WINDOW *winterm = term->winPtr->window;			\
  int py, px, y, x, my, mx, top = 0, bot = 0, tos = s->tos;	\
  (void)v; (void)p; (void)w; (void)iw; (void)argc; (void)argv;	\
  (void)win; (void)y; (void)x; (void)my; (void)mx; (void)osc;	\
  (void)tos;							\
  getyx(win, py, px); y = py - s->tos; x = px;			\
  getmaxyx(win, my, mx); my -= s->tos;				\
  wgetscrreg(win, &top, &bot);					\
  bot++; bot -= s->tos;						\
  top = top <= tos? 0 : top - tos;				\

#define HANDLER(name)                                   \
  static void						\
  name (VTPARSER *v, void *p, wchar_t w, wchar_t iw,	\
	int argc, int *argv, const wchar_t *osc)	\
  { COMMONVARS
#define ENDHANDLER n->repc = 0; } /* control sequences aren't repeated */

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(bell) { /* Terminal bell. */
  beep();
  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(numkp) { /* Application/Numeric Keypad Mode */
  n->pnm = (w == L'=');
  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(vis) { /* Cursor visibility */
  s->vis = iw == L'6'? 0 : 1;
  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(cup) { /* CUP - Cursor Position */
  s->xenl = false;
  wmove(win, tos + (n->decom? top : 0) + P1(0) - 1, P1(1) - 1);
  fixcursor(n);
  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(dch) { /* DCH - Delete Character */
  for (int i = 0; i < P1(0); i++) {
    wdelch(win);
  }
  fixcursor(n);
  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(ich) { /* ICH - Insert Character */
  for (int i = 0; i < P1(0); i++) {
    wins_nwstr(win, L" ", 1);
  }
  fixcursor(n);
  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(cuu) { /* CUU - Cursor Up */
  wmove(win, MAX(py - P1(0), tos + top), x);
  fixcursor(n);
  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(cud) { /* CUD - Cursor Down */
  wmove(win, MIN(py + P1(0), tos + bot - 1), x);
  fixcursor(n);
  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(cuf) { /* CUF - Cursor Forward */
  wmove(win, py, MIN(x + P1(0), mx - 1));
  fixcursor(n);
  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(ack) { /* ACK - Acknowledge Enquiry */
  SEND(n, "\006");
  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(hts) { /* HTS - Horizontal Tab Set */
  if (x < n->ntabs && x > 0)
    n->tabs[x] = true;
  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(ri) { /* RI - Reverse Index */
  int otop = 0, obot = 0;
  wgetscrreg(win, &otop, &obot);
  wsetscrreg(win, otop >= tos? otop : tos, obot);
  y == top? wscrl(win, -1) : wmove(win, MAX(tos, py - 1), x);
  wsetscrreg(win, otop, obot);
  fixcursor(n);
  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(decid) { /* DECID - Send Terminal Identification */
  if (w == L'c') {
    SEND(n, iw == L'>'? "\033[>1;10;0c" : "\033[?1;2c");
  } else if (w == L'Z') {
    SEND(n, "\033[?6c");
  }
  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(hpa) { /* HPA - Cursor Horizontal Absolute */
  wmove(win, py, MIN(P1(0) - 1, mx - 1));
  fixcursor(n);
  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(hpr) { /* HPR - Cursor Horizontal Relative */
  wmove(win, py, MIN(px + P1(0), mx - 1));
  fixcursor(n);
  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(vpa) { /* VPA - Cursor Vertical Absolute */
  wmove(win, MIN(tos + bot - 1, MAX(tos + top, tos + P1(0) - 1)), x);
  fixcursor(n);
  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(vpr) { /* VPR - Cursor Vertical Relative */
  wmove(win, MIN(tos + bot - 1, MAX(tos + top, py + P1(0))), x);
  fixcursor(n);
  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(cbt) { /* CBT - Cursor Backwards Tab */
  for (int i = x - 1; i < n->ntabs && i >= 0; i--) {
    if (n->tabs[i]) {
      wmove(win, py, i);
      fixcursor(n);
      return;
    }
  }
  wmove(win, py, 0);
  fixcursor(n);
  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(ht) { /* HT - Horizontal Tab */
  for (int i = x + 1; i < n->w && i < n->ntabs; i++) {
    if (n->tabs[i]) {
      wmove(win, py, i);
      fixcursor(n);
      return;
    }
  }
  wmove(win, py, mx - 1);
  fixcursor(n);
  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(tab) { /* Tab forwards or backwards */
  for (int i = 0; i < P1(0); i++) {
    switch (w) {
    case L'I':  CALL(ht);  break;
    case L'\t': CALL(ht);  break;
    case L'Z':  CALL(cbt); break;
    }
  }
  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(decaln) { /* DECALN - Screen Alignment Test */
  chtype e[] = {COLOR_PAIR(0) | 'E', 0};
  for (int r = 0; r < my; r++) {
    for (int c = 0; c <= mx; c++) {
      mvwaddchnstr(win, tos + r, c, e, 1);
    }
  }
  wmove(win, py, px);
  fixcursor(n);
  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(su) { /* SU - Scroll Up/Down */
  wscrl(win, (w == L'T' || w == L'^')? -P1(0) : P1(0));
  fixcursor(n);
  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(sc) { /* SC - Save Cursor */
  s->sx = px;                              /* save X position            */
  s->sy = py;                              /* save Y position            */
  wattr_get(win, &s->sattr, &s->sp, NULL); /* save attrs and color pair  */
  s->sfg = s->fg;                          /* save foreground color      */
  s->sbg = s->bg;                          /* save background color      */
  s->oxenl = s->xenl;                      /* save xenl state            */
  s->saved = true;                         /* save data is valid         */
  n->sgc = n->gc; n->sgs = n->gs;          /* save character sets        */
  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(rc) { /* RC - Restore Cursor */
  if (iw == L'#'){
    CALL(decaln);
    return;
  }
  if (!s->saved)
    return;
  wmove(win, s->sy, s->sx);                /* get old position          */
  wattr_set(win, s->sattr, s->sp, NULL);   /* get attrs and color pair  */
  s->fg = s->sfg;                          /* get foreground color      */
  s->bg = s->sbg;                          /* get background color      */
  s->xenl = s->oxenl;                      /* get xenl state            */
  n->gc = n->sgc; n->gs = n->sgs;          /* save character sets        */

  /* restore colors */
  int cp = Ck_GetPair(term->winPtr, s->fg, s->bg); cp = PAIR_NUMBER(cp);
  wcolor_set(win, cp, NULL);
  cchar_t c;
  setcchar(&c, L" ", A_NORMAL, cp, NULL);
  wbkgrndset(win, &c);

  fixcursor(n);
  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(tbc) { /* TBC - Tabulation Clear */
  switch (P0(0)){
  case 0: n->tabs[x < n->ntabs? x : 0] = false;          break;
  case 3: memset(n->tabs, 0, sizeof(bool) * (n->ntabs)); break;
  }
  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(cub) { /* CUB - Cursor Backward */
  s->xenl = false;
  wmove(win, py, MAX(x - P1(0), 0));
  fixcursor(n);
  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(el) { /* EL - Erase in Line */
  cchar_t b;
  int p = Ck_GetPair(term->winPtr, s->fg, s->bg); p = PAIR_NUMBER(p);
  setcchar(&b, L" ", A_NORMAL, p, NULL);
  switch (P0(0)){
  case 0: wclrtoeol(win);                                                 break;
  case 1: for (int i = 0; i <= x; i++) mvwadd_wchnstr(win, py, i, &b, 1); break;
  case 2: wmove(win, py, 0); wclrtoeol(win);                              break;
  }
  wmove(win, py, x);
  fixcursor(n);
  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(ed) { /* ED - Erase in Display */
  int o = 1;
  switch (P0(0)){
  case 0: wclrtobot(win);                     break;
  case 3: werase(win);                        break;
  case 2: wmove(win, tos, 0); wclrtobot(win); break;
  case 1:
    for (int i = tos; i < py; i++){
      wmove(win, i, 0);
      wclrtoeol(win);
    }
    wmove(win, py, x);
    el(v, p, w, iw, 1, &o, NULL);
    break;
  }
  wmove(win, py, px);
  fixcursor(n);
  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(ech) { /* ECH - Erase Character */
  cchar_t c;
  int p = Ck_GetPair(term->winPtr, s->fg, s->bg); p = PAIR_NUMBER(p);
  setcchar(&c, L" ", A_NORMAL, p, NULL);
  for (int i = 0; i < P1(0); i++)
    mvwadd_wchnstr(win, py, x + i, &c, 1);
  wmove(win, py, px);
  fixcursor(n);
  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(dsr) { /* DSR - Device Status Report */
  char buf[100] = {0};
  if (P0(0) == 6)
    snprintf(buf, sizeof(buf) - 1, "\033[%d;%dR",
	     (n->decom? y - top : y) + 1, x + 1);
  else
    snprintf(buf, sizeof(buf) - 1, "\033[0n");
  SEND(n, buf);
  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(idl) { /* IL or DL - Insert/Delete Line */
  /* we don't use insdelln here because it inserts above and not below,
   * and has a few other edge cases... */
  int otop = 0, obot = 0, p1 = MIN(P1(0), (my - 1) - y);
  wgetscrreg(win, &otop, &obot);
  wsetscrreg(win, py, obot);
  wscrl(win, w == L'L'? -p1 : p1);
  wsetscrreg(win, otop, obot);
  wmove(win, py, 0);
  fixcursor(n);
  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(csr) { /* CSR - Change Scrolling Region */
  if (wsetscrreg(win, tos + P1(0) - 1, tos + PD(1, my) - 1) == OK)
    CALL(cup);
  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(decreqtparm) { /* DECREQTPARM - Request Device Parameters */
  SEND(n, P0(0)? "\033[3;1;2;120;1;0x" : "\033[2;1;2;120;128;1;0x");
  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(sgr0) { /* Reset SGR to default */
#if 0
  wcolor_set(win, 0, NULL);
  s->fg = s->bg = -1;
  wbkgdset(win, COLOR_PAIR(0) | ' ');
#endif
  
  //@vca : essai de regler le fond
  int p;
  wattrset(win, A_NORMAL);
  p = Ck_GetPair(term->winPtr, s->wfg, s->wbg); p = PAIR_NUMBER(p);
  wcolor_set(win, p, NULL);
  wbkgdset(win, COLOR_PAIR(p) | ' ' );
  s->fg = s->bg = -1;

  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(cls) { /* Clear screen */
  CALL(cup);
  wclrtobot(win);
  CALL(cup);
  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(ris) { /* RIS - Reset to Initial State */
  n->gs = n->gc = n->g0 = CSET_US; n->g1 = CSET_GRAPH;
  n->g2 = CSET_US; n->g3 = CSET_GRAPH;
  n->decom = s->insert = s->oxenl = s->xenl = n->lnm = false;
  CALL(sgr0);
  CALL(cls);
  n->am = n->pnm = true;
  n->pri.vis = n->alt.vis = 1;
  n->s = &n->pri;
  wsetscrreg(n->pri.win, 0, MAX(term->scrollback, n->h) - 1);
  wsetscrreg(n->alt.win, 0, n->h - 1);
  for (int i = 0; i < n->ntabs; i++)
    n->tabs[i] = (i % 8 == 0);
  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 *
 *--------------------------------------------------------------------------
 */
HANDLER(mode) { /* Set or Reset Mode */
  bool set = (w == L'h');
  for (int i = 0; i < argc; i++) switch (P0(i)){
    case  1: n->pnm = set;              break;
    case  3: CALL(cls);                 break;
    case  4: s->insert = set;           break;
    case  6: n->decom = set; CALL(cup); break;
    case  7: n->am = set;               break;
    case 20: n->lnm = set;              break;
    case 25: s->vis = set? 1 : 0;       break;
    case 34: s->vis = set? 1 : 2;       break;
    case 1048: CALL((set? sc : rc));    break;
    case 1049:
      CALL((set? sc : rc)); /* fall-through */
    case 47: case 1047: if (set && n->s != &n->alt){
	n->s = &n->alt;
	CALL(cls);
      } else if (!set && n->s != &n->pri)
	n->s = &n->pri;
      break;
    }
  ENDHANDLER;
}

/* 
 *--------------------------------------------------------------------------
 * @todo a reprendre car change le background de la fenetre
 *       ce nest pas ce quil faut faire
 *--------------------------------------------------------------------------
 */
HANDLER(sgr) { /* SGR - Select Graphic Rendition */
  bool doc = false, do8 = COLORS >= 8, do16 = COLORS >= 16, do256 = COLORS >= 256;
  if (!argc)
    CALL(sgr0);

  short bg = s->wbg, fg = s->wfg;
  
  for (int i = 0; i < argc; i++) switch (P0(i)){
    case  0:  CALL(sgr0);                                              break;
    case  1:  wattron(win,  A_BOLD);                                   break;
    case  2:  wattron(win,  A_DIM);                                    break;
    case  4:  wattron(win,  A_UNDERLINE);                              break;
    case  5:  wattron(win,  A_BLINK);                                  break;
    case  7:  wattron(win,  A_REVERSE);                                break;
    case  8:  wattron(win,  A_INVIS);                                  break;
    case 21:  wattroff(win, A_BOLD);                                   break;
    case 22:  wattroff(win, A_DIM); wattroff(win, A_BOLD);             break;
    case 24:  wattroff(win, A_UNDERLINE);                              break;
    case 25:  wattroff(win, A_BLINK);                                  break;
    case 27:  wattroff(win, A_REVERSE);                                break;
    case 30:  fg = COLOR_BLACK;                           doc = do8;   break;
    case 31:  fg = COLOR_RED;                             doc = do8;   break;
    case 32:  fg = COLOR_GREEN;                           doc = do8;   break;
    case 33:  fg = COLOR_YELLOW;                          doc = do8;   break;
    case 34:  fg = COLOR_BLUE;                            doc = do8;   break;
    case 35:  fg = COLOR_MAGENTA;                         doc = do8;   break;
    case 36:  fg = COLOR_CYAN;                            doc = do8;   break;
    case 37:  fg = COLOR_WHITE;                           doc = do8;   break;
    case 38:  fg = P0(i+1) == 5? P0(i+2) : s->fg; i += 2; doc = do256; break;
    case 39:  fg = -1;                                    doc = true;  break;
    case 40:  bg = COLOR_BLACK;                           doc = do8;   break;
    case 41:  bg = COLOR_RED;                             doc = do8;   break;
    case 42:  bg = COLOR_GREEN;                           doc = do8;   break;
    case 43:  bg = COLOR_YELLOW;                          doc = do8;   break;
    case 44:  bg = COLOR_BLUE;                            doc = do8;   break;
    case 45:  bg = COLOR_MAGENTA;                         doc = do8;   break;
    case 46:  bg = COLOR_CYAN;                            doc = do8;   break;
    case 47:  bg = COLOR_WHITE;                           doc = do8;   break;
    case 48:  bg = P0(i+1) == 5? P0(i+2) : s->bg; i += 2; doc = do256; break;
    case 49:  bg = -1;                                    doc = true;  break;
    case 90:  fg = COLOR_BLACK;                           doc = do16;  break;
    case 91:  fg = COLOR_RED;                             doc = do16;  break;
    case 92:  fg = COLOR_GREEN;                           doc = do16;  break;
    case 93:  fg = COLOR_YELLOW;                          doc = do16;  break;
    case 94:  fg = COLOR_BLUE;                            doc = do16;  break;
    case 95:  fg = COLOR_MAGENTA;                         doc = do16;  break;
    case 96:  fg = COLOR_CYAN;                            doc = do16;  break;
    case 97:  fg = COLOR_WHITE;                           doc = do16;  break;
    case 100: bg = COLOR_BLACK;                           doc = do16;  break;
    case 101: bg = COLOR_RED;                             doc = do16;  break;
    case 102: bg = COLOR_GREEN;                           doc = do16;  break;
    case 103: bg = COLOR_YELLOW;                          doc = do16;  break;
    case 104: bg = COLOR_BLUE;                            doc = do16;  break;
    case 105: bg = COLOR_MAGENTA;                         doc = do16;  break;
    case 106: bg = COLOR_CYAN;                            doc = do16;  break;
    case 107: bg = COLOR_WHITE;                           doc = do16;  break;
#if defined(A_ITALIC) && !defined(NO_ITALICS )
    case  3:  wattron(win,  A_ITALIC);                    break;
    case 23:  wattroff(win, A_ITALIC);                    break;
#endif
    }
  if (doc){
    int p = Ck_GetPair(term->winPtr, s->fg = fg, s->bg = bg); p = PAIR_NUMBER(p);
    wcolor_set(win, p, NULL);
    cchar_t c;
    setcchar(&c, L" ", A_NORMAL, p, NULL);
    wbkgrndset(win, &c);
  }
#if 0
  if (doc) {
    attr_t attr; short pair;
    wattr_get(win, &attr, &pair, NULL);
    pair = Ck_GetPair(term->winPtr, s->fg = fg, s->bg = bg);
    wattrset(win, attr | pair);
    pair = PAIR_NUMBER(pair);
    wcolor_set(win, pair, NULL);
  }
#endif
}}

HANDLER(cr) { /* CR - Carriage Return */
  s->xenl = false;
  wmove(win, py, 0);
  fixcursor(n);
  ENDHANDLER;
}

HANDLER(ind) { /* IND - Index */
  y == (bot - 1)? scroll(win) : wmove(win, py + 1, x);
  fixcursor(n);
  ENDHANDLER;
}

HANDLER(nel) { /* NEL - Next Line */
  CALL(cr); CALL(ind);
  ENDHANDLER;
}

HANDLER(pnl) { /* NL - Newline */
  CALL((n->lnm? nel : ind));
  ENDHANDLER;
}

HANDLER(cpl) { /* CPL - Cursor Previous Line */
  wmove(win, MAX(tos + top, py - P1(0)), 0);
  fixcursor(n);
  ENDHANDLER;
}

HANDLER(cnl) { /* CNL - Cursor Next Line */
  wmove(win, MIN(tos + bot - 1, py + P1(0)), 0);
  fixcursor(n);
  ENDHANDLER;
}

HANDLER(print) { /* Print a character to the terminal */
  if (wcwidth(w) < 0)
    return;

  if (s->insert)
    CALL(ich);

  if (s->xenl){
    s->xenl = false;
    if (n->am)
      CALL(nel);
    getyx(win, y, x);
    y -= tos;
  }

  if (w < MAXMAP && n->gc[w])
    w = n->gc[w];
  n->repc = w;

  if (x == mx - wcwidth(w)){
    s->xenl = true;
    wins_nwstr(win, &w, 1);
  } else {
    waddnwstr(win, &w, 1);
  }
  n->gc = n->gs;
  fixcursor(n);
}} /* no ENDHANDLER because we don't want to reset repc */

HANDLER(rep) { /* REP - Repeat Character */
  for (int i = 0; i < P1(0) && n->repc; i++)
    print(v, p, n->repc, 0, 0, NULL, NULL);
  fixcursor(n);
  ENDHANDLER;
}

HANDLER(scs) { /* Select Character Set */
  wchar_t **t = NULL;
  switch (iw){
  case L'(': t = &n->g0;  break;
  case L')': t = &n->g1;  break;
  case L'*': t = &n->g2;  break;
  case L'+': t = &n->g3;  break;
  default: return;        break;
  }
  switch (w){
  case L'A': *t = CSET_UK;    break;
  case L'B': *t = CSET_US;    break;
  case L'0': *t = CSET_GRAPH; break;
  case L'1': *t = CSET_US;    break;
  case L'2': *t = CSET_GRAPH; break;
  }
  ENDHANDLER;
}

HANDLER(so) { /* Switch Out/In Character Set */
  if (w == 0x0e)
    n->gs = n->gc = n->g1; /* locking shift */
  else if (w == 0xf)
    n->gs = n->gc = n->g0; /* locking shift */
  else if (w == L'n')
    n->gs = n->gc = n->g2; /* locking shift */
  else if (w == L'o')
    n->gs = n->gc = n->g3; /* locking shift */
  else if (w == L'N'){
    n->gs = n->gc; /* non-locking shift */
    n->gc = n->g2;
  } else if (w == L'O'){
    n->gs = n->gc; /* non-locking shift */
    n->gc = n->g3;
  }
  ENDHANDLER;
}

static void
setupevents(NODE *n)
{
  n->vp.p = n;
  vtonevent(&n->vp, VTPARSER_CONTROL, 0x05, ack);
  vtonevent(&n->vp, VTPARSER_CONTROL, 0x07, bell);
  vtonevent(&n->vp, VTPARSER_CONTROL, 0x08, cub);
  vtonevent(&n->vp, VTPARSER_CONTROL, 0x09, tab);
  vtonevent(&n->vp, VTPARSER_CONTROL, 0x0a, pnl);
  vtonevent(&n->vp, VTPARSER_CONTROL, 0x0b, pnl);
  vtonevent(&n->vp, VTPARSER_CONTROL, 0x0c, pnl);
  vtonevent(&n->vp, VTPARSER_CONTROL, 0x0d, cr);
  vtonevent(&n->vp, VTPARSER_CONTROL, 0x0e, so);
  vtonevent(&n->vp, VTPARSER_CONTROL, 0x0f, so);
  vtonevent(&n->vp, VTPARSER_CSI,     L'A', cuu);
  vtonevent(&n->vp, VTPARSER_CSI,     L'B', cud);
  vtonevent(&n->vp, VTPARSER_CSI,     L'C', cuf);
  vtonevent(&n->vp, VTPARSER_CSI,     L'D', cub);
  vtonevent(&n->vp, VTPARSER_CSI,     L'E', cnl);
  vtonevent(&n->vp, VTPARSER_CSI,     L'F', cpl);
  vtonevent(&n->vp, VTPARSER_CSI,     L'G', hpa);
  vtonevent(&n->vp, VTPARSER_CSI,     L'H', cup);
  vtonevent(&n->vp, VTPARSER_CSI,     L'I', tab);
  vtonevent(&n->vp, VTPARSER_CSI,     L'J', ed);
  vtonevent(&n->vp, VTPARSER_CSI,     L'K', el);
  vtonevent(&n->vp, VTPARSER_CSI,     L'L', idl);
  vtonevent(&n->vp, VTPARSER_CSI,     L'M', idl);
  vtonevent(&n->vp, VTPARSER_CSI,     L'P', dch);
  vtonevent(&n->vp, VTPARSER_CSI,     L'S', su);
  vtonevent(&n->vp, VTPARSER_CSI,     L'T', su);
  vtonevent(&n->vp, VTPARSER_CSI,     L'X', ech);
  vtonevent(&n->vp, VTPARSER_CSI,     L'Z', tab);
  vtonevent(&n->vp, VTPARSER_CSI,     L'`', hpa);
  vtonevent(&n->vp, VTPARSER_CSI,     L'^', su);
  vtonevent(&n->vp, VTPARSER_CSI,     L'@', ich);
  vtonevent(&n->vp, VTPARSER_CSI,     L'a', hpr);
  vtonevent(&n->vp, VTPARSER_CSI,     L'b', rep);
  vtonevent(&n->vp, VTPARSER_CSI,     L'c', decid);
  vtonevent(&n->vp, VTPARSER_CSI,     L'd', vpa);
  vtonevent(&n->vp, VTPARSER_CSI,     L'e', vpr);
  vtonevent(&n->vp, VTPARSER_CSI,     L'f', cup);
  vtonevent(&n->vp, VTPARSER_CSI,     L'g', tbc);
  vtonevent(&n->vp, VTPARSER_CSI,     L'h', mode);
  vtonevent(&n->vp, VTPARSER_CSI,     L'l', mode);
  vtonevent(&n->vp, VTPARSER_CSI,     L'm', sgr);
  vtonevent(&n->vp, VTPARSER_CSI,     L'n', dsr);
  vtonevent(&n->vp, VTPARSER_CSI,     L'r', csr);
  vtonevent(&n->vp, VTPARSER_CSI,     L's', sc);
  vtonevent(&n->vp, VTPARSER_CSI,     L'u', rc);
  vtonevent(&n->vp, VTPARSER_CSI,     L'x', decreqtparm);
  vtonevent(&n->vp, VTPARSER_ESCAPE,  L'0', scs);
  vtonevent(&n->vp, VTPARSER_ESCAPE,  L'1', scs);
  vtonevent(&n->vp, VTPARSER_ESCAPE,  L'2', scs);
  vtonevent(&n->vp, VTPARSER_ESCAPE,  L'7', sc);
  vtonevent(&n->vp, VTPARSER_ESCAPE,  L'8', rc);
  vtonevent(&n->vp, VTPARSER_ESCAPE,  L'A', scs);
  vtonevent(&n->vp, VTPARSER_ESCAPE,  L'B', scs);
  vtonevent(&n->vp, VTPARSER_ESCAPE,  L'D', ind);
  vtonevent(&n->vp, VTPARSER_ESCAPE,  L'E', nel);
  vtonevent(&n->vp, VTPARSER_ESCAPE,  L'H', hts);
  vtonevent(&n->vp, VTPARSER_ESCAPE,  L'M', ri);
  vtonevent(&n->vp, VTPARSER_ESCAPE,  L'Z', decid);
  vtonevent(&n->vp, VTPARSER_ESCAPE,  L'c', ris);
  vtonevent(&n->vp, VTPARSER_ESCAPE,  L'p', vis);
  vtonevent(&n->vp, VTPARSER_ESCAPE,  L'=', numkp);
  vtonevent(&n->vp, VTPARSER_ESCAPE,  L'>', numkp);
  vtonevent(&n->vp, VTPARSER_PRINT,   0,    print);
}

/*** MTM FUNCTIONS
 * These functions do the user-visible work of MTM: creating nodes in the
 * tree, updating the display, and so on.
 */
static bool *
newtabs(int w, int ow, bool *oldtabs) /* Initialize default tabstops. */
{
  bool *tabs = calloc(w, sizeof(bool));
  if (!tabs)
    return NULL;
  for (int i = 0; i < w; i++) /* keep old overlapping tabs */
    tabs[i] = i < ow? oldtabs[i] : (i % 8 == 0);
  return tabs;
}

static NODE *
newnode(int h, int w) /* Create a new node. */
{
  NODE *n = calloc(1, sizeof(NODE));
  bool *tabs = newtabs(w, 0, NULL);
  if (!n || h < 2 || w < 2 || !tabs) {
    return free(n), free(tabs), NULL;
  }

  n->pt = -1;
  n->h = h;
  n->w = w;
  n->tabs = tabs;
  n->ntabs = w;

  n->clientData = NULL;
  memset(n->iobuf, 0, sizeof(n->iobuf));
  
  return n;
}

static void
freenode(NODE *n) /* Free a node. */
{
  if (n){
    if (n->pri.win)
      delwin(n->pri.win);
    if (n->alt.win)
      delwin(n->alt.win);
    if (n->pt >= 0){
      close(n->pt);
    }
    free(n->tabs);
    free(n);
  }
}

static void
fixcursor(NODE *n) /* Move the terminal cursor to the active view. */
{
  Terminal *terminalPtr = (Terminal*) n->clientData;
  int y, x;
  
  getyx(n->s->win, y, x);
  y = MIN(MAX(y, n->s->tos), n->s->tos + n->h - 1);
  wmove(n->s->win, y, x);

  if ( (terminalPtr != NULL) && (terminalPtr->winPtr->window != NULL) ) {
    int offset = (terminalPtr->borderPtr != NULL) ? 1 : 0;
    wmove(terminalPtr->winPtr->window, y - n->s->tos + offset, x + offset);
    if (n->s->off != n->s->tos? 0 : n->s->vis) {
      terminalPtr->winPtr->flags |= CK_SHOW_CURSOR;
    }
    else {
      terminalPtr->winPtr->flags &= ~CK_SHOW_CURSOR;
    }
  }
}

static NODE *
newview(Terminal *term, int scrollback, int h, int w, int fg, int bg) /* Open a new view. */
{
  struct winsize ws = {.ws_row = h, .ws_col = w};
  NODE *n = newnode(h, w);
  if (!n) {
    return NULL;
  }
  n->clientData = term;

  SCRN *pri = &n->pri, *alt = &n->alt;
  pri->win = newpad(MAX(h, scrollback), w);
  alt->win = newpad(h, w);
  if (!pri->win || !alt->win) {
    return freenode(n), NULL;
  }
  pri->tos = pri->off = MAX(0, scrollback - h);
  n->s = pri;

  pri->wfg = alt->wfg = fg;
  pri->wbg = alt->wbg = bg;

  nodelay(pri->win, TRUE); nodelay(alt->win, TRUE);
  scrollok(pri->win, TRUE); scrollok(alt->win, TRUE);
  keypad(pri->win, TRUE); keypad(alt->win, TRUE);

  setupevents(n);
  ris(&n->vp, n, L'c', 0, 0, NULL, NULL);

  pid_t pid = forkpty(&n->pt, NULL, NULL, &ws);
  if (pid < 0) {
    // @todo: erreur handling a la Tk a mettre en place
    perror("forkpty");
    return freenode(n), NULL;
    
  } else if (pid == 0) {
    char *argv[term->nexec + 1];
    char buf[100] = {0};
    snprintf(buf, sizeof(buf) - 1, "%lu", (unsigned long)getppid());
    setsid();
    setenv("MTM", buf, 1);

    if ( term->term == NULL ) {
      int res;
      res = TermParseProc( NULL, term->interp, term->winPtr,
			   NULL, (char*) term, Ck_Offset(Terminal, attr) );
    }
    
    setenv("TERM", term->term, 1);
    signal(SIGCHLD, SIG_DFL);
    memcpy( argv, term->aexec, term->nexec * sizeof(char*));
    argv[term->nexec] = NULL;
    execv( argv[0], argv );
    return NULL;
  }

  fcntl(n->pt, F_SETFL, O_NONBLOCK);
  
  return n;
}

static void
deletenode(NODE *n) /* Delete a node. */
{
  freenode(n);
}

static void
reshapeview(NODE *n, int d, int ow) /* Reshape a view. */
{
  int oy, ox;
  Terminal *term = (Terminal *) n->clientData;
  bool *tabs = newtabs(n->w, ow, n->tabs);
  struct winsize ws = {.ws_row = n->h, .ws_col = n->w};

  if (tabs) {
    free(n->tabs);
    n->tabs = tabs;
    n->ntabs = n->w;
  }

  getyx(n->s->win, oy, ox);
  wresize(n->pri.win, MAX(n->h, term->scrollback), MAX(n->w, 2));
  wresize(n->alt.win, MAX(n->h, 2), MAX(n->w, 2));
  n->pri.tos = n->pri.off = MAX(0, term->scrollback - n->h);
  n->alt.tos = n->alt.off = 0;
  wsetscrreg(n->pri.win, 0, MAX(term->scrollback, n->h) - 1);
  wsetscrreg(n->alt.win, 0, n->h - 1);
  if (d > 0){ /* make sure the new top line syncs up after reshape */
    wmove(n->s->win, oy + d, ox);
    wscrl(n->s->win, -d);
  }
  fixcursor(n);
  ioctl(n->pt, TIOCSWINSZ, &ws);
}


static void
reshape(NODE *n, int h, int w) /* Reshape a node. */
{
  if (n->h == h && n->w == w)
    return;

  int d = n->h - h;
  int ow = n->w;
  n->h = MAX(h, 1);
  n->w = MAX(w, 1);

  reshapeview(n, d, ow);
  draw(n);
}

static void
draw(NODE *n) /* Draw a node. */
{
  Terminal *terminalPtr = (Terminal*) n->clientData;
  
  if ( (terminalPtr != NULL) &&  (terminalPtr->winPtr->window != NULL)) {
    CkWindow *winPtr = terminalPtr->winPtr;
    int height = (n->h < winPtr->height) ? n->h : winPtr->height;
    int width  = (n->w < winPtr->width)  ? n->w : winPtr->width;
    int offset = (terminalPtr->borderPtr != NULL) ? 1 : 0;
    int y, x;

    /* save cursor position */
    getyx(winPtr->window, y, x);

    copywin( n->s->win, winPtr->window,
	     n->s->off, 0, offset, offset, height+offset-1, width+offset-1, 0);

    Ck_SetWindowAttr(winPtr, COLOR_RED, COLOR_BLACK, A_NORMAL);
    if ( terminalPtr->flags & MODE_MOVE ) {
      mvwprintw( winPtr->window, 0, width - 6, "MOVING" );
    }
    else if ( terminalPtr->flags & MODE_EXPECT ) {
      mvwprintw( winPtr->window, 0, width - 6, "EXPECT" );
    }
    else {
      mvwprintw( winPtr->window, 0, width - 6, "EXPECT" );
    }

    /* restore cursor position */
    wmove(winPtr->window, y, x);
  }
}

static void
scrollback(NODE *n)
{
  n->s->off = MAX(0, n->s->off - n->h / 2);
  Tk_DoWhenIdle(TerminalYScrollCommand, n->clientData);
}

static void
scrollforward(NODE *n)
{
  n->s->off = MIN(n->s->tos, n->s->off + n->h / 2);
  Tk_DoWhenIdle(TerminalYScrollCommand, n->clientData);
}

static void
scrollbottom(NODE *n)
{
  n->s->off = n->s->tos;
  Tk_DoWhenIdle(TerminalYScrollCommand, n->clientData);
}

static void
sendarrow(const NODE *n, const char *k)
{
  char buf[100] = {0};
  snprintf(buf, sizeof(buf) - 1, "\033%s%s", n->pnm? "O" : "[", k);
  SEND(n, buf);
}

static bool
handlechar(NODE *n, int r, int k) /* Handle a single input character. */
{
  const char cmdstr[] = {commandkey, 0};
  Terminal *terminalPtr = (Terminal*) n->clientData;
  
#define KERR(i) (r == ERR && (i) == k)
#define KEY(i)  (r == OK  && (i) == k)
#define CODE(i) (r == KEY_CODE_YES && (i) == k)
#define INSCR (n->s->tos != n->s->off)
#define SB scrollbottom(n)
#define DO(s, t, a)						\
  if (s == n->cmd && (t)) { a ; n->cmd = false; return true; }

  DO(n->cmd,KERR(k),             return false);
  DO(false, KEY(commandkey),     return n->cmd = true);
  DO(false, KEY(0),              SENDN(n, "\000", 1); SB);
  DO(false, KEY(L'\n'),          SEND(n, "\n"); SB);
  DO(false, KEY(L'\r'),          SEND(n, n->lnm? "\r\n" : "\r"); SB);
  DO(false, SCROLLUP && INSCR,   scrollback(n));
  DO(false, SCROLLDOWN && INSCR, scrollforward(n));
  DO(false, RECENTER && INSCR,   scrollbottom(n));
  DO(false, CODE(KEY_ENTER),     SEND(n, n->lnm? "\r\n" : "\r"); SB);
  DO(false, CODE(KEY_UP),        sendarrow(n, "A"); SB);
  DO(false, CODE(KEY_DOWN),      sendarrow(n, "B"); SB);
  DO(false, CODE(KEY_RIGHT),     sendarrow(n, "C"); SB);
  DO(false, CODE(KEY_LEFT),      sendarrow(n, "D"); SB);
  DO(false, CODE(KEY_HOME),      SEND(n, "\033[1~"); SB);
  DO(false, CODE(KEY_END),       SEND(n, "\033[4~"); SB);
  DO(false, CODE(KEY_PPAGE),     SEND(n, "\033[5~"); SB);
  DO(false, CODE(KEY_NPAGE),     SEND(n, "\033[6~"); SB);
  DO(false, CODE(KEY_BACKSPACE), SEND(n, "\177"); SB);
  DO(false, CODE(KEY_DC),        SEND(n, "\033[3~"); SB);
  DO(false, CODE(KEY_IC),        SEND(n, "\033[2~"); SB);
  DO(false, CODE(KEY_BTAB),      SEND(n, "\033[Z"); SB);
  DO(false, CODE(KEY_F(1)),      SEND(n, "\033OP"); SB);
  DO(false, CODE(KEY_F(2)),      SEND(n, "\033OQ"); SB);
  DO(false, CODE(KEY_F(3)),      SEND(n, "\033OR"); SB);
  DO(false, CODE(KEY_F(4)),      SEND(n, "\033OS"); SB);
  DO(false, CODE(KEY_F(5)),      SEND(n, "\033[15~"); SB);
  DO(false, CODE(KEY_F(6)),      SEND(n, "\033[17~"); SB);
  DO(false, CODE(KEY_F(7)),      SEND(n, "\033[18~"); SB);
  DO(false, CODE(KEY_F(8)),      SEND(n, "\033[19~"); SB);
  DO(false, CODE(KEY_F(9)),      SEND(n, "\033[20~"); SB);
  DO(false, CODE(KEY_F(10)),     SEND(n, "\033[21~"); SB);
  DO(false, CODE(KEY_F(11)),     SEND(n, "\033[23~"); SB);
  DO(false, CODE(KEY_F(12)),     SEND(n, "\033[24~"); SB);

  /* add extra keys bindings - keys are just forwarded to pty */
#define CK_NEW_KEY(name, seq, val) DO(false, CODE(name), SEND(n, seq); SB);
#include "ckKeys.h"
#undef CK_NEW_KEY
  
#if 0
  //@todo : reprendre les bindings
  DO(true,  MOVE_UP,             focus(findnode(root, ABOVE(n))));
  DO(true,  MOVE_DOWN,           focus(findnode(root, BELOW(n))));
  DO(true,  MOVE_LEFT,           focus(findnode(root, LEFT(n))));
  DO(true,  MOVE_RIGHT,          focus(findnode(root, RIGHT(n))));
  DO(true,  HSPLIT,              split(n, HORIZONTAL));
  DO(true,  VSPLIT,              split(n, VERTICAL));
#endif
  DO(true,  CODE(KEY(CTL('i'))), TerminalGiveFocus(terminalPtr));
  DO(true,  MOVE_OTHER,          TerminalGiveFocus(terminalPtr));
  DO(true,  REDRAW,              TerminalPostRedisplay(terminalPtr));
  DO(true,  SCROLLUP,            scrollback(n));
  DO(true,  SCROLLDOWN,          scrollforward(n));
  DO(true,  RECENTER,            scrollbottom(n));
  DO(true,  KEY(commandkey),     SENDN(n, cmdstr, 1));
  
  char c[MB_LEN_MAX + 1] = {0};
  if (wctomb(c, k) > 0){
    scrollbottom(n);
    SEND(n, c);
  }
  return n->cmd = false, true;
}


// vtparser.c

/**** DATA TYPES */
#define MAXACTIONS  128

typedef struct ACTION ACTION;
struct ACTION {
  wchar_t lo, hi;
  void (*cb)(VTPARSER *p, wchar_t w);
  STATE *next;
};

struct STATE{
  void (*entry)(VTPARSER *v);
  ACTION actions[MAXACTIONS];
};

/**** GLOBALS */
static STATE ground, escape, escape_intermediate, csi_entry,
  csi_ignore, csi_param, csi_intermediate, osc_string;

/**** ACTION FUNCTIONS */
static void
reset(VTPARSER *v)
{
  v->inter = v->narg = v->nosc = 0;
  memset(v->args, 0, sizeof(v->args));
  memset(v->oscbuf, 0, sizeof(v->oscbuf));
}

static void
ignore(VTPARSER *v, wchar_t w)
{
  (void)v; (void)w; /* avoid warnings */
}

static void
collect(VTPARSER *v, wchar_t w)
{
  v->inter = v->inter? v->inter : (int)w;
}

static void
collectosc(VTPARSER *v, wchar_t w)
{
  if (v->nosc < MAXOSC)
    v->oscbuf[v->nosc++] = w;
}

static void
param(VTPARSER *v, wchar_t w)
{
  v->narg = v->narg? v->narg : 1;

  if (w == L';')
    v->args[v->narg++] = 0;
  else if (v->narg < MAXPARAM && v->args[v->narg - 1] < 9999)
    v->args[v->narg - 1] = v->args[v->narg - 1] * 10 + (w - 0x30);
}

#define VTDO(k, t, f, n, a)                             \
  static void						\
  do ## k (VTPARSER *v, wchar_t w)			\
       {						\
	 if (t)						\
	   f (v, v->p, w, v->inter, n, a, v->oscbuf);	\
       }

VTDO(control, w < MAXCALLBACK && v->cons[w], v->cons[w], 0, NULL);
VTDO(escape,  w < MAXCALLBACK && v->escs[w], v->escs[w], v->inter > 0, &v->inter);
VTDO(csi,     w < MAXCALLBACK && v->csis[w], v->csis[w], v->narg, v->args);
VTDO(print,   v->print, v->print, 0, NULL);
VTDO(osc,     v->osc, v->osc, v->nosc, NULL);

/**** PUBLIC FUNCTIONS */
VTCALLBACK
vtonevent(VTPARSER *vp, VtEvent t, wchar_t w, VTCALLBACK cb)
{
  VTCALLBACK o = NULL;
  if (w < MAXCALLBACK) {
    switch (t) {
    case VTPARSER_CONTROL: o = vp->cons[w]; vp->cons[w] = cb; break;
    case VTPARSER_ESCAPE:  o = vp->escs[w]; vp->escs[w] = cb; break;
    case VTPARSER_CSI:     o = vp->csis[w]; vp->csis[w] = cb; break;
    case VTPARSER_PRINT:   o = vp->print;   vp->print   = cb; break;
    case VTPARSER_OSC:     o = vp->osc;     vp->osc     = cb; break;
    }
  }

  return o;
}

static void
vthandlechar(VTPARSER *vp, wchar_t w)
{
  vp->s = vp->s ? vp->s : &ground;
  for (ACTION *a = vp->s->actions; a->cb; a++) {
    if (w >= a->lo && w <= a->hi) {
      a->cb(vp, w);
      if (a->next) {
	vp->s = a->next;
	if (a->next->entry) {
	  a->next->entry(vp);
	}
      }
      return;
    }
  }
}

void
vtwrite(VTPARSER *vp, const char *s, size_t n)
{
  wchar_t w = 0;
  while (n){
    size_t r = mbrtowc(&w, s, n, &vp->ms);
    switch (r){
    case -2: /* incomplete character, try again */
      return;

    case -1: /* invalid character, skip it */
      w = VTPARSER_BAD_CHAR;
      r = 1;
      break;

    case 0: /* literal zero, write it but advance */
      r = 1;
      break;
    }

    n -= r;
    s += r;
    vthandlechar(vp, w);
  }
}

/**** STATE DEFINITIONS
 * This was built by consulting the excellent state chart created by
 * Paul Flo Williams: http://vt100.net/emu/dec_ansi_parser
 * Please note that Williams does not (AFAIK) endorse this work.
 */
#define MAKESTATE(name, onentry, ...)				\
  static STATE name ={						\
		      onentry ,					\
		      {						\
		       {0x00, 0x00, ignore,    NULL},		\
		       {0x7f, 0x7f, ignore,    NULL},		\
		       {0x18, 0x18, docontrol, &ground},	\
		       {0x1a, 0x1a, docontrol, &ground},	\
		       {0x1b, 0x1b, ignore,    &escape},	\
		       {0x01, 0x06, docontrol, NULL},		\
		       {0x08, 0x17, docontrol, NULL},		\
		       {0x19, 0x19, docontrol, NULL},		\
		       {0x1c, 0x1f, docontrol, NULL},		\
		       __VA_ARGS__ ,				\
		       {0x07, 0x07, docontrol, NULL},		\
		       {0x00, 0x00, NULL,      NULL}		\
		      }						\
  };

MAKESTATE(ground, NULL,
	  {0x20, WCHAR_MAX, doprint, NULL}
	  );

MAKESTATE(escape, reset,
	  {0x21, 0x21, ignore,   &osc_string},
	  {0x20, 0x2f, collect,  &escape_intermediate},
	  {0x30, 0x4f, doescape, &ground},
	  {0x51, 0x57, doescape, &ground},
	  {0x59, 0x59, doescape, &ground},
	  {0x5a, 0x5a, doescape, &ground},
	  {0x5c, 0x5c, doescape, &ground},
	  {0x6b, 0x6b, ignore,   &osc_string},
	  {0x60, 0x7e, doescape, &ground},
	  {0x5b, 0x5b, ignore,   &csi_entry},
	  {0x5d, 0x5d, ignore,   &osc_string},
	  {0x5e, 0x5e, ignore,   &osc_string},
	  {0x50, 0x50, ignore,   &osc_string},
	  {0x5f, 0x5f, ignore,   &osc_string}
	  );

MAKESTATE(escape_intermediate, NULL,
	  {0x20, 0x2f, collect,  NULL},
	  {0x30, 0x7e, doescape, &ground}
	  );

MAKESTATE(csi_entry, reset,
	  {0x20, 0x2f, collect, &csi_intermediate},
	  {0x3a, 0x3a, ignore,  &csi_ignore},
	  {0x30, 0x39, param,   &csi_param},
	  {0x3b, 0x3b, param,   &csi_param},
	  {0x3c, 0x3f, collect, &csi_param},
	  {0x40, 0x7e, docsi,   &ground}
	  );

MAKESTATE(csi_ignore, NULL,
	  {0x20, 0x3f, ignore, NULL},
	  {0x40, 0x7e, ignore, &ground}
	  );

MAKESTATE(csi_param, NULL,
	  {0x30, 0x39, param,   NULL},
	  {0x3b, 0x3b, param,   NULL},
	  {0x3a, 0x3a, ignore,  &csi_ignore},
	  {0x3c, 0x3f, ignore,  &csi_ignore},
	  {0x20, 0x2f, collect, &csi_intermediate},
	  {0x40, 0x7e, docsi,   &ground}
	  );

MAKESTATE(csi_intermediate, NULL,
	  {0x20, 0x2f, collect, NULL},
	  {0x30, 0x3f, ignore,  &csi_ignore},
	  {0x40, 0x7e, docsi,   &ground}
	  );

MAKESTATE(osc_string, reset,
	  {0x07, 0x07, doosc, &ground},
	  {0x20, 0x7f, collectosc, NULL}
	  );



/*
 * These constants define the update policy of terminal
 */
#define POLICY_LINE (-1)
#define POLICY_NONE (0)


/*
 *----------------------------------------------------------------------
 *
 * CkInitTerminal --
 *
 *      This procedure initializes a terminal widget.  It's
 *      separate from Ck_TerminalCmd so that it can be used for the
 *      main window, which has already been created elsewhere.
 *
 * Results:
 *      A standard Tcl completion code.
 *
 * Side effects:
 *      A widget record gets allocated, handlers get set up, etc..
 *
 *----------------------------------------------------------------------
 */

int
CkInitTerminal(interp, winPtr, argc, argv)
    Tcl_Interp *interp;                 /* Interpreter associated with the
                                         * application. */
    CkWindow *winPtr;                   /* Window to use for terminal or
                                         * top-level. Caller must already
                                         * have set window's class. */
    int argc;                           /* Number of configuration arguments
                                         * (not including class command and
                                         * window name). */
    char *argv[];                       /* Configuration arguments. */
{
    Terminal *terminalPtr;

    terminalPtr = (Terminal *) ckalloc(sizeof (Terminal));
    terminalPtr->winPtr = winPtr;
    terminalPtr->interp = interp;
    terminalPtr->widgetCmd = Tcl_CreateCommand(interp,
        terminalPtr->winPtr->pathName, TerminalWidgetCmd,
            (ClientData) terminalPtr, TerminalCmdDeletedProc);
    terminalPtr->borderPtr = NULL;
    terminalPtr->fg = 0;
    terminalPtr->bg = 0;
    terminalPtr->attr = 0;
    terminalPtr->width = 1;
    terminalPtr->height = 1;
    terminalPtr->takeFocus = NULL;
    terminalPtr->flags = 0; 
    terminalPtr->node = NULL;
    terminalPtr->scrollback = 0;
    terminalPtr->yscrollcommand = NULL;
    terminalPtr->redisplayPolicy = POLICY_LINE;
    terminalPtr->count = 0;
    terminalPtr->exec = NULL;
    terminalPtr->aexec = NULL;
    terminalPtr->tee = NULL;
    
    Ck_CreateEventHandler(terminalPtr->winPtr,
            CK_EV_MAP | CK_EV_EXPOSE | CK_EV_DESTROY,
            TerminalEventProc, (ClientData) terminalPtr);
    Ck_CreateEventHandler(terminalPtr->winPtr,
            CK_EV_KEYPRESS,
            TerminalKeyEventProc, (ClientData) terminalPtr);

    if (ConfigureTerminal(interp, terminalPtr, argc, argv, 0) != TCL_OK) {
        Ck_DestroyWindow(terminalPtr->winPtr);
        return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, Tcl_NewStringObj(terminalPtr->winPtr->pathName,-1));

    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * Ck_TerminalCmd --
 *
 *      This procedure is invoked to process the "terminal" and
 *      "toplevel" Tcl commands.  See the user documentation for
 *      details on what it does.
 *
 * Results:
 *      A standard Tcl result.
 *
 * Side effects:
 *      See the user documentation.
 *
 *--------------------------------------------------------------
 */

int
Ck_TerminalCmd(clientData, interp, argc, argv)
    ClientData clientData;      /* Main window associated with
                                 * interpreter. */
    Tcl_Interp *interp;         /* Current interpreter. */
    int argc;                   /* Number of arguments. */
    char **argv;                /* Argument strings. */
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
     * ConfigureTerminal do it.  This is necessary because we have
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
            className = "Terminal";
        }
    }
    Ck_SetClass(new, className);
    return CkInitTerminal(interp, new, argc-2, argv+2);
}

/*
 *--------------------------------------------------------------
 *
 * TerminalWidgetCmd --
 *
 *      This procedure is invoked to process the Tcl command
 *      that corresponds to a terminal widget.  See the user
 *      documentation for details on what it does.
 *
 * Results:
 *      A standard Tcl result.
 *
 * Side effects:
 *      See the user documentation.
 *
 *--------------------------------------------------------------
 */

static int
TerminalWidgetCmd(clientData, interp, argc, argv)
    ClientData clientData;      /* Information about terminal widget. */
    Tcl_Interp *interp;         /* Current interpreter. */
    int argc;                   /* Number of arguments. */
    char **argv;                /* Argument strings. */
{
    Terminal *terminalPtr = (Terminal *) clientData;
    int result = TCL_OK;
    int length;
    char c;

    if (argc < 2) {
        Tcl_AppendResult(interp, "wrong # args: should be \"",
                argv[0], " option ?arg arg ...?\"", (char *) NULL);
        return TCL_ERROR;
    }
    Ck_Preserve((ClientData) terminalPtr);
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
        result = Ck_ConfigureValue(interp, terminalPtr->winPtr, configSpecs,
                (char *) terminalPtr, argv[2], 0);
    } else if ((c == 'c') && (strncmp(argv[1], "configure", length) == 0)) {
        if (argc == 2) {
            result = Ck_ConfigureInfo(interp, terminalPtr->winPtr, configSpecs,
                    (char *) terminalPtr, (char *) NULL, 0);
        } else if (argc == 3) {
            result = Ck_ConfigureInfo(interp, terminalPtr->winPtr, configSpecs,
                    (char *) terminalPtr, argv[2], 0);
        } else {
            result = ConfigureTerminal(interp, terminalPtr, argc-2, argv+2,
                    CK_CONFIG_ARGV_ONLY);
        }
    } else if ((c == 'e') && (strncmp(argv[1], "expect", length) == 0)) {
      
      
    } else if ((c == 'i') && (strncmp(argv[1], "interact", length) == 0)) {
      
      
    } else if ((c == 's') && (strncmp(argv[1], "send", length) == 0)) {
      if (argc == 3) {
	SendToTerminal( terminalPtr, argv[2] );
      } else {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
			 argv[0], " send text\"",
			 (char *) NULL);
	goto error;
      }
    } else if ((c == 't') && (strncmp(argv[1], "tee", length) == 0)) {
      return TerminalTee( terminalPtr, argc, argv);
      
    } else if ((c == 'y') && (strncmp(argv[1], "yview", length) == 0)) {
      return TerminalYView( terminalPtr, argc, argv);

    } else {
        Tcl_AppendResult(interp, "bad option \"", argv[1],
                "\":  must be cget or configure", (char *) NULL);
        goto error;
    }
    Ck_Release((ClientData) terminalPtr);
    return result;

error:
    Ck_Release((ClientData) terminalPtr);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyTerminal --
 *
 *      This procedure is invoked by Ck_EventuallyFree or Ck_Release
 *      to clean up the internal structure of a terminal at a safe time
 *      (when no-one is using it anymore).
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Everything associated with the terminal is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
DestroyTerminal(clientData)
    ClientData clientData;      /* Info about terminal widget. */
{
    Terminal *terminalPtr = (Terminal *) clientData;

    Ck_FreeOptions(configSpecs, (char *) terminalPtr, 0);

    terminalPtr->flags &= ~REDRAW_PENDING;
    if ( (terminalPtr->flags & DISCONNECTED) == 0 ) {
      Tcl_DeleteFileHandler( terminalPtr->node->pt );
      close ( terminalPtr->node->pt );
      terminalPtr->flags |= DISCONNECTED;
    }

    if ( terminalPtr->exec != NULL ) {
      Tcl_Free( terminalPtr->exec );
      terminalPtr->exec = NULL;
    }

    if ( terminalPtr->aexec != NULL ) {
      Tcl_Free( (char*) terminalPtr->aexec );
      terminalPtr->aexec = NULL;
    }

    if ( terminalPtr->node != NULL ) {
      deletenode( terminalPtr->node );
      terminalPtr->node = NULL;
    }
    
    ckfree((char *) terminalPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TerminalCmdDeletedProc --
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
TerminalCmdDeletedProc(clientData)
    ClientData clientData;      /* Pointer to widget record for widget. */
{
    Terminal *terminalPtr = (Terminal *) clientData;
    CkWindow *winPtr = terminalPtr->winPtr;

    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case tkwin
     * is NULL) or because the command was deleted, and then this procedure
     * destroys the widget.
     */

    if (winPtr != NULL) {
        terminalPtr->winPtr = NULL;
        Ck_DestroyWindow(winPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 *  ExecPrintProc --
 *
 *      This procedure returns the value of the "-exec" configuration
 *      option.
 *
 *----------------------------------------------------------------------
 */
static char*
ExecPrintProc(clientData, winPtr, widgRec, offset, freeProcPtr)
     ClientData clientData;
     CkWindow *winPtr;
     char *widgRec;
     int offset;
     Tcl_FreeProc **freeProcPtr;
{
  Terminal *terminalPtr = (Terminal*) widgRec;
  *freeProcPtr = NULL;
  return (terminalPtr->exec == NULL) ? "" : terminalPtr->exec;
}

/*
 *----------------------------------------------------------------------
 *
 *  ExecParseProc --
 *
 *----------------------------------------------------------------------
 */
static int
ExecParseProc( clientData, interp, winPtr, value, widgetRec, offset )
     ClientData clientData; /* Terminal widget associated */
     Tcl_Interp *interp;    /* Used for error reporting */
     CkWindow *winPtr;
     char *value;
     char *widgetRec;
     int offset;
{
  Terminal *terminalPtr = (Terminal*) widgetRec;
  int res;

  terminalPtr->nexec = 0;

  /* if the supplied value is not NULL */
  if ( value != NULL ) {
    res = Tcl_SplitList( terminalPtr->interp, value,  
			 &terminalPtr->nexec, &terminalPtr->aexec);
    if ( TCL_OK != res ) {
      terminalPtr->exec = NULL;
      terminalPtr->aexec = NULL;
      terminalPtr->nexec = 0;
      return TCL_ERROR;
    }
  }

  /* when it is empty - use defaut shell */
  if ( terminalPtr->nexec == 0 ) {
    if (getenv("SHELL")) {
      value = getenv("SHELL");
    }
    else {
      struct passwd *pwd = getpwuid(getuid());
      if (pwd) {
	value =  pwd->pw_shell;
      }
      else {
	value = "/bin/sh";
      }
    }

    return ExecParseProc( clientData, interp, winPtr, value, widgetRec, offset );
  }

  /* duplicate string value */
  terminalPtr->exec = Tcl_Alloc( strlen(value) + 1 );
  strcpy( terminalPtr->exec, value );

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 *  TermPrintProc --
 *
 *      This procedure returns the value of the "-exec" configuration
 *      option.
 *
 *----------------------------------------------------------------------
 */
static char*
TermPrintProc(clientData, winPtr, widgRec, offset, freeProcPtr)
     ClientData clientData;
     CkWindow *winPtr;
     char *widgRec;
     int offset;
     Tcl_FreeProc **freeProcPtr;
{
  Terminal *terminalPtr = (Terminal*) widgRec;
  *freeProcPtr = NULL;
  return (terminalPtr->term == NULL) ? "" : terminalPtr->term;
}

/*
 *----------------------------------------------------------------------
 *
 *  TermParseProc --
 *
 *----------------------------------------------------------------------
 */
static int
TermParseProc( clientData, interp, winPtr, value, widgetRec, offset )
     ClientData clientData; /* Terminal widget associated */
     Tcl_Interp *interp;    /* Used for error reporting */
     CkWindow *winPtr;
     char *value;
     char *widgetRec;
     int offset;
{
  Terminal *terminalPtr = (Terminal*) widgetRec;

  // @todo retourner une erreur si deja configure
  
  if ( value == NULL ) {
      const char *envterm = getenv("TERM");
      if (envterm && COLORS >= 256 && !strstr(DEFAULT_TERMINAL, "-256color")) {
	value = DEFAULT_256_COLOR_TERMINAL;
      } else {
	value = DEFAULT_TERMINAL;
      }
      return TermParseProc( clientData, interp, winPtr, value, widgetRec, offset );
  }

  /* duplicate string value */
  terminalPtr->term = Tcl_Alloc( strlen(value) + 1 );
  strcpy( terminalPtr->term, value );

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 *  RedisplayPolicyPrintProc --
 *
 *      This procedure returns the value of the "-exec" configuration
 *      option.
 *
 *----------------------------------------------------------------------
 */
static char*
RedisplayPolicyPrintProc(clientData, winPtr, widgRec, offset, freeProcPtr)
     ClientData clientData;
     CkWindow *winPtr;
     char *widgRec;
     int offset;
     Tcl_FreeProc **freeProcPtr;
{
  Terminal *terminalPtr = (Terminal*) widgRec;
  char *buffer = (char*) Tcl_Alloc(16);
  
  *freeProcPtr = Tcl_Free;
  
  switch( terminalPtr->redisplayPolicy ) {
  case POLICY_NONE:
    strcpy( buffer, "none" );
    break;
  case POLICY_LINE:
    strcpy( buffer, "line" );
    break;
  default:
    sprintf( buffer, "%d", terminalPtr->redisplayPolicy );
    break;
  }

  return buffer;
}

/*
 *----------------------------------------------------------------------
 *
 *  RedisplayPolicyParseProc --
 *
 *----------------------------------------------------------------------
 */
static int
RedisplayPolicyParseProc( clientData, interp, winPtr, value, widgetRec, offset)
     ClientData clientData; /* Terminal widget associated */
     Tcl_Interp *interp;    /* Used for error reporting */
     CkWindow *winPtr;
     char *value;
     char *widgetRec;
     int offset;
{
  Terminal *terminalPtr = (Terminal*) widgetRec;

  if ( value == NULL ) {
    value = "line";
    return RedisplayPolicyParseProc( clientData, interp, winPtr,
				     value, widgetRec, offset );
  }

  if ( !strcmp( value, "line" ) ) {
    terminalPtr->redisplayPolicy = POLICY_LINE;
  }
  else if ( !strcmp( value, "none" ) ) {
    terminalPtr->redisplayPolicy = POLICY_NONE;
  }
  else {
    int policy;
    if ( TCL_OK != Tcl_GetInt( interp, value, &policy )) {
      return TCL_ERROR;
    }
    if ( policy <= 0 || policy > 65536 ) {
      Tcl_AppendResult( interp, "policy value '", value,
			"' out of range", NULL );
      return TCL_ERROR;
    }
    terminalPtr->redisplayPolicy = policy;
  }

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureTerminal --
 *
 *      This procedure is called to process an argv/argc list, plus
 *      the option database, in order to configure (or
 *      reconfigure) a terminal widget.
 *
 * Results:
 *      The return value is a standard Tcl result.  If TCL_ERROR is
 *      returned, then interp->result contains an error message.
 *
 * Side effects:
 *      Configuration information, such as text string, colors, font,
 *      etc. get set for terminalPtr;  old resources get freed, if there
 *      were any.
 *
 *----------------------------------------------------------------------
 */

static int
ConfigureTerminal(interp, terminalPtr, argc, argv, flags)
     Tcl_Interp *interp;         /* Used for error reporting. */
     Terminal *terminalPtr;            /* Information about widget;  may or may
                                 * not already have values for some fields. */
     int argc;                   /* Number of valid entries in argv. */
     char **argv;                /* Arguments. */
     int flags;                  /* Flags to pass to Tk_ConfigureWidget. */
{
    if (Ck_ConfigureWidget(interp, terminalPtr->winPtr, configSpecs,
            argc, argv, (char *) terminalPtr, flags) != TCL_OK) {
        return TCL_ERROR;
    }

    Ck_SetWindowAttr(terminalPtr->winPtr, terminalPtr->fg, terminalPtr->bg,
		     terminalPtr->attr);
    
    Ck_SetInternalBorder(terminalPtr->winPtr, terminalPtr->borderPtr != NULL);
    
    if ((terminalPtr->width > 0) || (terminalPtr->height > 0)) {
        Ck_GeometryRequest(terminalPtr->winPtr, terminalPtr->width, terminalPtr->height);
    }
    if ((terminalPtr->winPtr->flags & CK_MAPPED)
            && !(terminalPtr->flags & REDRAW_PENDING)) {
        Tk_DoWhenIdle(DisplayTerminal, (ClientData) terminalPtr);
        terminalPtr->flags |= REDRAW_PENDING;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DisplayTerminal --
 *
 *      This procedure is invoked to display a terminal widget.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Commands are output to display the terminal in its
 *      current mode.
 *
 *----------------------------------------------------------------------
 */

static void
DisplayTerminal(clientData)
    ClientData clientData;      /* Information about widget. */
{
    Terminal *terminalPtr = (Terminal *) clientData;
    CkWindow *winPtr = terminalPtr->winPtr;

    terminalPtr->flags &= ~REDRAW_PENDING;
    if ((winPtr == NULL) || !(winPtr->flags & CK_MAPPED)) {
        return;
    }

    Ck_ClearToBot(winPtr, 0, 0);

    if (terminalPtr->borderPtr != NULL) {
      int y, x;
      getyx(winPtr->window, y, x);
      Ck_DrawBorder(winPtr, terminalPtr->borderPtr, 0, 0,
		    winPtr->width, winPtr->height);
      wmove(winPtr->window, y, x);
    }

    draw( terminalPtr->node );
    fixcursor( terminalPtr->node );

    Ck_EventuallyRefresh(winPtr);
}

/*
 *--------------------------------------------------------------
 *
 * TerminalEventProc --
 *
 *      This procedure is invoked by the dispatcher on
 *      structure changes to a terminal.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      When the window gets deleted, internal structures get
 *      cleaned up.  When it gets exposed, it is redisplayed.
 *
 *--------------------------------------------------------------
 */

static void
TerminalEventProc(clientData, eventPtr)
    ClientData clientData;      /* Information about window. */
    CkEvent *eventPtr;          /* Information about event. */
{
    Terminal *terminalPtr = (Terminal *) clientData;

    if (eventPtr->type == CK_EV_EXPOSE && terminalPtr->winPtr != NULL &&
        !(terminalPtr->flags & REDRAW_PENDING)) {
      int width  = terminalPtr->winPtr->width;
      int height = terminalPtr->winPtr->height;
	
      if ( terminalPtr->borderPtr != NULL ) {
	width = width - 2;
	height = height - 2;
      }

      if ( terminalPtr->node == NULL ) {
	
	terminalPtr->node =
	  newview( terminalPtr, terminalPtr->scrollback,
		   height, width,
		   terminalPtr->fg, terminalPtr->bg);
	if (terminalPtr->node == NULL ) {
	  Ck_DestroyWindow(terminalPtr->winPtr);
	  return;
	}
	Tcl_CreateFileHandler( terminalPtr->node->pt, TCL_READABLE, TerminalPtyProc,
			       (ClientData) terminalPtr);

	terminalPtr->winPtr->flags |= CK_MAPPED;
	
      } else {
	reshape( terminalPtr->node, height, width);
      }
      
      Tk_DoWhenIdle(DisplayTerminal, (ClientData) terminalPtr);
      terminalPtr->flags |= REDRAW_PENDING;
	
    } else if (eventPtr->type == CK_EV_DESTROY) {
        if (terminalPtr->winPtr != NULL) {
            terminalPtr->winPtr = NULL;
            Tcl_DeleteCommand(terminalPtr->interp,
                    Tcl_GetCommandName(terminalPtr->interp, terminalPtr->widgetCmd));
        }
        if (terminalPtr->flags & REDRAW_PENDING)
            Tk_CancelIdleCall(DisplayTerminal, (ClientData) terminalPtr);
        Ck_EventuallyFree((ClientData) terminalPtr, (Ck_FreeProc *) DestroyTerminal);
    }
}

/*
 *--------------------------------------------------------------
 *
 * TerminalKeyEventProc --
 *
 *      This procedure is invoked by the dispatcher on
 *      key input on the terminal. It handles mouse events too.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      When the window gets deleted, internal structures get
 *      cleaned up.  When it gets exposed, it is redisplayed.
 *
 *--------------------------------------------------------------
 */

static void
TerminalKeyEventProc(clientData, eventPtr)
    ClientData clientData;      /* Information about window. */
    CkEvent *eventPtr;          /* Information about event. */
{
    Terminal *terminalPtr = (Terminal *) clientData;

    /* do not handle more keypress
     * if the terminal is disconnected */
    if ( terminalPtr->flags & DISCONNECTED != 0 ) {
      return;
    }
    
    if (eventPtr->type == CK_EV_KEYPRESS ) {
      CkKeyEvent *keyEventPtr = (CkKeyEvent*) eventPtr;
      
      handlechar( terminalPtr->node, keyEventPtr->curses_rc, keyEventPtr->curses_w);
      
      /* schedule redisplay if needed */
      if ( !(terminalPtr->flags & REDRAW_PENDING)) {
        Tk_DoWhenIdle(DisplayTerminal, (ClientData) terminalPtr);
        terminalPtr->flags |= REDRAW_PENDING;
      }
    }

    if (eventPtr->type = CK_EV_MOUSE_UP ) {
      // @todo: if mouse reporting was requested
      //        send the mouse events to the pty
    }
}

/*
 *--------------------------------------------------------------
 *
 * TerminalPtyProc --
 *
 *      This procedure is invoked when characters arrive
 *      from the pty bound
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      When the window gets deleted, internal structures get
 *      cleaned up.  When it gets exposed, it is redisplayed.
 *
 *--------------------------------------------------------------
 */

static void
TerminalPtyProc( clientData, flags)
     ClientData clientData;       /* Information about window */
     int flags;                   /* Flags descibing what happened to file */
{
  if ( flags & TCL_READABLE ) {
    Terminal *terminalPtr = (Terminal *) clientData;
    NODE *nodePtr = terminalPtr->node;
    ssize_t r;
    int needupdate = 0;
    int ocounter = terminalPtr->count;
    int chunck;
    
    r = read( nodePtr->pt, nodePtr->iobuf, sizeof(nodePtr->iobuf));
    if (r > 0) {
      
      /* forward input to tee channel */
      if ( terminalPtr->tee != NULL ) {
	int w = Tcl_WriteChars( terminalPtr->tee, nodePtr->iobuf, r);
	if ( w != r ) {
	  /* @todo : background error ? */
	}
      }

      vtwrite(&nodePtr->vp, nodePtr->iobuf, r);
      
      /* update character counter */
      /* we must be more clever and ignore escape sequences */
      /* and treat multibytes characters */
      terminalPtr->count += r;
      
      switch( terminalPtr->redisplayPolicy ) {
      case POLICY_LINE:
	chunck = terminalPtr->winPtr->width;
	goto compute;
      case POLICY_NONE:
	/* do nothing */
	break;
      default:
	chunck = terminalPtr->redisplayPolicy;
      compute:
	/* check if we have read anough bytes to trigger a display update */
	if ((ocounter / terminalPtr->redisplayPolicy) != (terminalPtr->count / terminalPtr->redisplayPolicy)) {
	  needupdate = 1;
	}
      }

      /* force a synchronous redisplay */
      if ( needupdate && (terminalPtr->winPtr->flags & CK_MAPPED)) {
	DisplayTerminal( terminalPtr );
	wnoutrefresh( terminalPtr->winPtr->window );
	doupdate();

	/* cancel pending IDLE call to DisplayTerminal */
	Tk_CancelIdleCall(DisplayTerminal, (ClientData) terminalPtr);
      }
      /* schedule redisplay */
      else if ( !(terminalPtr->flags & REDRAW_PENDING)) {
        Tk_DoWhenIdle(DisplayTerminal, (ClientData) terminalPtr);
        terminalPtr->flags |= REDRAW_PENDING;
      }

    }

    /* disconnection */
    if (r <= 0 && errno != EINTR && errno != EWOULDBLOCK) {
      char *argv[] = {"-takeFocus", "false"};
      char  cmd[256];
      
      Tcl_DeleteFileHandler( nodePtr->pt );
      close ( nodePtr->pt );
      terminalPtr->flags &= ~REDRAW_PENDING;
      terminalPtr->flags |= DISCONNECTED;
      ConfigureTerminal(terminalPtr->interp, terminalPtr, 2, argv, CK_CONFIG_ARGV_ONLY);
      TerminalGiveFocus(terminalPtr);
    }
  }
}

/*
 *----------------------------------------------------------------------
 *
 * SendToTerminal --
 *
 *      This procedure is invoked to send text to the terminal
 *      exactly as it was typed. It generates key events.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Puash events to event queue.
 *
 *----------------------------------------------------------------------
 */

static void
SendToTerminal(terminalPtr, text)
    Terminal *terminalPtr;      /* Info about terminal widget. */
    char *text;                 /* Texte */
{
  if ( (terminalPtr->flags & DISCONNECTED) == 0 ) {
    int i;
    for (i = 0; text[i]; ++i ) {
      handlechar(terminalPtr->node, OK, text[i]);
    }
  }
}

/*
 *----------------------------------------------------------------------
 *
 * TerminalGiveFocus --
 *
 *      This procedure is invoked to give back the focus to the next
 *      window in the focus chain
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
TerminalGiveFocus(terminalPtr)
     Terminal *terminalPtr;      /* Info about terminal widget. */
{
  char cmd[256];
  sprintf(cmd, "after idle {focus [ck_focusNext %s]}", terminalPtr->winPtr->pathName );
  Tcl_Eval(terminalPtr->interp, cmd);
}

/*
 *----------------------------------------------------------------------
 *
 * TerminalPostRedisplay --
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
TerminalPostRedisplay(terminalPtr)
     Terminal *terminalPtr;      /* Info about terminal widget. */
{
      if ((terminalPtr->winPtr->flags & CK_MAPPED)
            && !(terminalPtr->flags & REDRAW_PENDING)) {
        Tk_DoWhenIdle(DisplayTerminal, (ClientData) terminalPtr);
        terminalPtr->flags |= REDRAW_PENDING;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TerminalYView --
 *
 *      This procedure handles the yview widget subcommand.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Scrolls the terminal view.
 *
 *----------------------------------------------------------------------
 */

static int
TerminalYView(terminalPtr, argc, argv)
     Terminal *terminalPtr;      /* Info about terminal widget. */
     int argc;                   /* Number of arguments */
     char **argv;                /* arguments */
{
  struct NODE *nodePtr = terminalPtr->node;
  int offset = 0;
  char c;
  
  switch(argc) {
  case 2:
    /* we are in a call like ".t yview" */
    /* we need to return the visible fraction */
    {
      double  limit;
      Tcl_Obj *objv[2];

      limit = (double)(nodePtr->s->off) / (double)(terminalPtr->scrollback);
      objv[0] = Tcl_NewDoubleObj(limit);
      limit = limit + (double)(nodePtr->h) / (double)(terminalPtr->scrollback);
      objv[1] = Tcl_NewDoubleObj(limit);
      
      Tcl_SetObjResult( terminalPtr->interp, Tcl_NewListObj(2, objv));
      return TCL_OK;
    }
  case 3:
    if ( TCL_OK != Tcl_GetInt( terminalPtr->interp, argv[3], &offset) ) {
      return TCL_ERROR;
    }
  moveto:
    /* move offset into allowed range, do not throw an error */
    if ( offset < 0 ) { offset = 0; }
    if ( offset > terminalPtr->scrollback ) { offset = terminalPtr->scrollback; }
    if ( offset > nodePtr->s->tos ) { offset = nodePtr->s->tos; }

    if ( offset != nodePtr->s->off ) {
      nodePtr->s->off = offset;
      nodePtr->cmd = (nodePtr->s->off != nodePtr->s->tos);
      Tk_DoWhenIdle( DisplayTerminal, (ClientData) terminalPtr);
      Tk_DoWhenIdle( TerminalYScrollCommand, (ClientData) terminalPtr );
    }
    
    break;
  case 4:
    c = argv[2][0];
    if ( (c == 'm') && !strcmp(argv[2], "moveto") ) {
      double d;
      if ( TCL_OK != Tcl_GetDouble( terminalPtr->interp, argv[3], &d) ) {
	return TCL_ERROR;
      }
      offset = (int) ( d * (double) terminalPtr->scrollback );
    }
    goto moveto;
  case 5:
    c = argv[2][0];
    if ( (c == 's') && !strcmp(argv[2], "scroll" )) {
      if ( strcmp( argv[4], "units") && strcmp( argv[4], "pages") ) {
	Tcl_AppendResult(terminalPtr->interp,
			 "expecting units or pages instead of \"",
			 argv[4], "\"", (char *) NULL);
	return TCL_ERROR;
      }
      if ( TCL_OK != Tcl_GetInt( terminalPtr->interp, argv[3], &offset) ) {
	return TCL_ERROR;
      }
      if ( !strcmp( argv[4], "pages") ) {
	offset = offset * nodePtr->h;
      }
    }
    goto moveto;
  default:
    Tcl_AppendResult(terminalPtr->interp, "wrong # args: should be \"",
		     argv[0], " yview",
		     "| yview number ",
		     "| yview moveto fraction",
		     "| yview scroll number pages|units"
		     "\"", (char *) NULL);
    return TCL_ERROR;
  }
}

/*
 *----------------------------------------------------------------------
 *
 * TerminalTee --
 *
 *      This procedure handles the tee widget subcommand.
 *      This subcommand is useful to implement an 'expect' like
 *      facility on the top of the terminal, say for supplying
 *      passwords.
 *
 *      With no arguments it returns the TCL channel where pty input
 *      stream is copied.
 *
 *      With one argument which is the empty list. It removes
 *      any previously connected TCL channel. The channel is not closed
 *      but won't receive any more input.
 *
 *      If the command receives the name of a valid TCL channel.
 *      The channel will start receiving all bytes received on the pty.
 *      If another channel was previously connected it gets disconnected,
 *      as "<terminal window> tee {}" does. The channel is not configured
 *      by this command, it must be set to non-blocking binary mode
 *      if required by the client code.
 *
 * Results:
 *      The name of the channel.
 *
 * Side effects:
 *      
 *
 *----------------------------------------------------------------------
 */

static int
TerminalTee(terminalPtr, argc, argv)
     Terminal *terminalPtr;      /* Info about terminal widget. */
     int argc;                   /* Number of arguments */
     char **argv;                /* arguments */
{
  Tcl_Channel channel;
  int mode;
  
  switch(argc) {
  case 2:
  getresult:
    {
      Tcl_Obj *objPtr;
      /* we are in a call like ".t tee" */
      if ( terminalPtr->tee != NULL ) {
	objPtr = Tcl_NewStringObj( Tcl_GetChannelName(terminalPtr->tee), -1);
      }
      else {
	objPtr = Tcl_NewListObj(0, NULL);
      }
      Tcl_SetObjResult( terminalPtr->interp, objPtr );
      return TCL_OK;
    }
    break;
    
  case 3:
    /* we are in a call like ".t tee {}" or ".t tee chan" */
    channel = Tcl_GetChannel( terminalPtr->interp, argv[2], &mode );
    if ( channel != NULL ) {
      if ( (mode & TCL_WRITABLE) == 0 ) {
	Tcl_AppendResult(terminalPtr->interp, "channel \"",
			 argv[2], "\" is not writable", (char *) NULL);
	return TCL_ERROR;
      }

      terminalPtr->tee = channel;
      goto getresult;
    }
    else {
      /* it is not a channel, check if it is the empty list */
      int i;
      for ( i = 0; (argv[2][i]) && isspace(argv[2][i]); ++i );
      if ( argv[2][i] != '{' ) goto noSuchChannel;
      for ( i++;  (argv[2][i]) && isspace(argv[2][i]); ++i );
      if ( argv[2][i] != '}' ) goto noSuchChannel;      
      for ( i++;  (argv[2][i]) && isspace(argv[2][i]); ++i );
      if ( argv[2][i] != '\0' ) goto noSuchChannel;

      terminalPtr->tee = NULL;
      goto getresult;
      
    noSuchChannel:
      {
	Tcl_AppendResult(terminalPtr->interp, "can't find channel \"",
			 argv[2], "\"", (char *) NULL);
	return TCL_ERROR;
      }
    }
  default:
    {
      Tcl_AppendResult(terminalPtr->interp, "wrong # args: should be \"",
		       argv[0], " tee ?channel|{}?\"", (char *) NULL);
    }
  }
}

/*
 *----------------------------------------------------------------------
 *
 * TerminalYScrollCommand --
 *
 *      This procedure is invoked to evaluate yscrollcommand
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
TerminalYScrollCommand(clientData)
     ClientData clientData;
{
  Terminal *terminalPtr = (Terminal*) clientData;
  if ( terminalPtr->yscrollcommand != NULL ) {
    struct NODE *nodePtr = terminalPtr->node;
    char stdbl[TCL_DOUBLE_SPACE];
    Tcl_DString ds;
    double start, end;
    char *script;
    
    start = (double)(nodePtr->s->off) / (double)(terminalPtr->scrollback);
    end = start + (double)(nodePtr->h) / (double)(terminalPtr->scrollback);

    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, terminalPtr->yscrollcommand, -1 );
    Tcl_PrintDouble( terminalPtr->interp, start, stdbl);
    Tcl_DStringAppendElement(&ds, stdbl );
    Tcl_PrintDouble( terminalPtr->interp, end, stdbl);
    Tcl_DStringAppendElement(&ds, stdbl );

    script = Tcl_DStringValue(&ds);
    Tcl_Eval( terminalPtr->interp, script );

    Tcl_DStringFree(&ds);
  }
}
