/*
 * ckPort.h --
 *
 *	This file is included by all of the curses wish C files.
 *	It contains information that may be configuration-dependent,
 *	such as #includes for system include files and a few other things.
 *
 * Copyright (c) 1991-1993 The Regents of the University of California.
 * Copyright (c) 1994 Sun Microsystems, Inc.
 * Copyright (c) 1995 Christian Werner
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#ifndef _CKPORT
#define _CKPORT

#if defined(_WIN32) || defined(WIN32)
#   include <windows.h>
#else
#   define _XOPEN_SOURCE
#endif

/*
 * Macro to use instead of "void" for arguments that must have
 * type "void *" in ANSI C;  maps them to type "char *" in
 * non-ANSI systems.  This macro may be used in some of the include
 * files below, which is why it is defined here.
 */

#ifndef VOID
#   ifdef __STDC__
#       define VOID void
#   else
#       define VOID char
#   endif
#endif

#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#if defined(_WIN32) || defined(WIN32)
#   include <limits.h>
#else
#   ifdef HAVE_LIMITS_H
#      include <limits.h>
#   else
#      include "compat/limits.h"
#   endif
#endif
#include <math.h>
#if !defined(_WIN32) && !defined(WIN32)
#   include <pwd.h>
#endif
#ifdef NO_STDLIB_H
#   include "compat/stdlib.h"
#else
#   include <stdlib.h>
#endif
#include <string.h>
#include <sys/types.h>
#if !defined(_WIN32) && !defined(WIN32)
#   include <sys/file.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#   include <sys/select.h>
#endif
#include <sys/stat.h>
#if !defined(_WIN32) && !defined(WIN32)
#   include <sys/time.h>
#endif
#ifndef _TCL
#   include <tcl.h>
#endif
#if !defined(_WIN32) && !defined(WIN32)
#   ifdef HAVE_UNISTD_H
#      include <unistd.h>
#   else
#      include "compat/unistd.h"
#   endif
#endif

#if (TCL_MAJOR_VERSION < 7)
#error Tcl major version must be 7 or greater
#endif

/*
 * Not all systems declare the errno variable in errno.h. so this
 * file does it explicitly.
 */

#if !defined(_WIN32) && !defined(WIN32)
extern int errno;
#endif

/*
 * Provide some defines to get some Tk functionality which was in
 * tkEvent.c prior to Tcl version 7.5.
 */

#define Tk_BackgroundError Tcl_BackgroundError
#define Tk_DoWhenIdle Tcl_DoWhenIdle
#define Tk_DoWhenIdle2 Tcl_DoWhenIdle
#define Tk_CancelIdleCall Tcl_CancelIdleCall
#define Tk_CreateTimerHandler Tcl_CreateTimerHandler
#define Tk_DeleteTimerHandler Tcl_DeleteTimerHandler
#define Tk_AfterCmd Tcl_AfterCmd
#define Tk_FileeventCmd Tcl_FileEventCmd
#define Tk_DoOneEvent Tcl_DoOneEvent


/*
 * Declarations for various library procedures that may not be declared
 * in any other header file.
 */

#ifndef panic
extern void		panic();
#endif

/*
 * Return type for signal(), this taken from TclX.
 */

#ifndef RETSIGTYPE
#   define RETSIGTYPE void
#endif

typedef RETSIGTYPE (*Ck_SignalProc) _ANSI_ARGS_((int));


#endif /* _CKPORT */
