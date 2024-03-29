/* 
 * ckConfig.c --
 *
 *	This file contains the Ck_ConfigureWidget procedure.
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
 * Values for "flags" field of Ck_ConfigSpec structures.  Be sure
 * to coordinate these values with those defined in ck.h
 * (CK_CONFIG_*).  There must not be overlap!
 *
 * INIT -		Non-zero means (char *) things have been
 *			converted to Ck_Uid's.
 */

#define INIT		0x20

/*
 * Forward declarations for procedures defined later in this file:
 */

static int		DoConfig _ANSI_ARGS_((Tcl_Interp *interp,
			    CkWindow *winPtr, Ck_ConfigSpec *specPtr,
			    Ck_Uid value, int valueIsUid, char *widgRec));
static Ck_ConfigSpec *	FindConfigSpec _ANSI_ARGS_ ((Tcl_Interp *interp,
			    Ck_ConfigSpec *specs, char *argvName,
			    int needFlags, int hateFlags));
static char *		FormatConfigInfo _ANSI_ARGS_ ((Tcl_Interp *interp,
			    CkWindow *winPtr, Ck_ConfigSpec *specPtr,
			    char *widgRec));
static char *           FormatConfigValue _ANSI_ARGS_((Tcl_Interp *interp,
                            CkWindow *tkwin, Ck_ConfigSpec *specPtr,
                            char *widgRec, char *buffer));

/*
 *--------------------------------------------------------------
 *
 * Ck_ConfigureWidget --
 *
 *	Process command-line options to fill in fields of a
 *	widget record with resources and other parameters.
 *
 * Results:
 *	A standard Tcl return value.  In case of an error,
 *	interp->result will hold an error message.
 *
 * Side effects:
 *	The fields of widgRec get filled in with information
 *	from argc/argv.  Old information in widgRec's fields
 *	gets recycled.
 *
 *--------------------------------------------------------------
 */

int
Ck_ConfigureWidget(interp, winPtr, specs, argc, argv, widgRec, flags)
    Tcl_Interp *interp;		/* Interpreter for error reporting. */
    CkWindow *winPtr;		/* Window containing widget. */
    Ck_ConfigSpec *specs;	/* Describes legal options. */
    int argc;			/* Number of elements in argv. */
    char **argv;		/* Command-line options. */
    char *widgRec;		/* Record whose fields are to be
				 * modified.  Values must be properly
				 * initialized. */
    int flags;			/* Used to specify additional flags
				 * that must be present in config specs
				 * for them to be considered.  Also,
				 * may have CK_CONFIG_ARGV_ONLY set. */
{
    Ck_ConfigSpec *specPtr;
    Ck_Uid value;		/* Value of option from database. */
    int needFlags;		/* Specs must contain this set of flags
				 * or else they are not considered. */
    int hateFlags;		/* If a spec contains any bits here, it's
				 * not considered. */

    needFlags = flags & ~(CK_CONFIG_USER_BIT - 1);
    if (!(winPtr->mainPtr->flags & CK_HAS_COLOR)) {
	hateFlags = CK_CONFIG_COLOR_ONLY;
    } else {
	hateFlags = CK_CONFIG_MONO_ONLY;
    }

    /*
     * Pass one:  scan through all the option specs, replacing strings
     * with Ck_Uids (if this hasn't been done already) and clearing
     * the CK_CONFIG_OPTION_SPECIFIED flags.
     */

    for (specPtr = specs; specPtr->type != CK_CONFIG_END; specPtr++) {
	if (!(specPtr->specFlags & INIT) && (specPtr->argvName != NULL)) {
	    if (specPtr->dbName != NULL) {
		specPtr->dbName = Ck_GetUid(specPtr->dbName);
	    }
	    if (specPtr->dbClass != NULL) {
		specPtr->dbClass = Ck_GetUid(specPtr->dbClass);
	    }
	    if (specPtr->defValue != NULL) {
		specPtr->defValue = Ck_GetUid(specPtr->defValue);
	    }
	}
	specPtr->specFlags = (specPtr->specFlags & ~CK_CONFIG_OPTION_SPECIFIED)
		| INIT;
    }

    /*
     * Pass two:  scan through all of the arguments, processing those
     * that match entries in the specs.
     */

    for ( ; argc > 0; argc -= 2, argv += 2) {
	specPtr = FindConfigSpec(interp, specs, *argv, needFlags, hateFlags);
	if (specPtr == NULL) {
	    return TCL_ERROR;
	}

	/*
	 * Process the entry.
	 */

	if (argc < 2) {
	    Tcl_AppendResult(interp, "value for \"", *argv,
		    "\" missing", (char *) NULL);
	    return TCL_ERROR;
	}
	if (DoConfig(interp, winPtr, specPtr, argv[1], 0, widgRec) != TCL_OK) {
	    char msg[100];

	    sprintf(msg, "\n    (processing \"%.40s\" option)",
		    specPtr->argvName);
	    Tcl_AddErrorInfo(interp, msg);
	    return TCL_ERROR;
	}
	specPtr->specFlags |= CK_CONFIG_OPTION_SPECIFIED;
    }

    /*
     * Pass three:  scan through all of the specs again;  if no
     * command-line argument matched a spec, then check for info
     * in the option database.  If there was nothing in the
     * database, then use the default.
     */

    if (!(flags & CK_CONFIG_ARGV_ONLY)) {
	for (specPtr = specs; specPtr->type != CK_CONFIG_END; specPtr++) {
	    if ((specPtr->specFlags & CK_CONFIG_OPTION_SPECIFIED)
		    || (specPtr->argvName == NULL)
		    || (specPtr->type == CK_CONFIG_SYNONYM)) {
		continue;
	    }
	    if (((specPtr->specFlags & needFlags) != needFlags)
		    || (specPtr->specFlags & hateFlags)) {
		continue;
	    }
	    value = NULL;
	    if (specPtr->dbName != NULL) {
		value = Ck_GetOption(winPtr, specPtr->dbName,
		    specPtr->dbClass);
	    }
	    if (value != NULL) {
		if (DoConfig(interp, winPtr, specPtr, value, 1, widgRec) !=
			TCL_OK) {
		    char msg[200];
    
		    sprintf(msg, "\n    (%s \"%.50s\" in widget \"%.50s\")",
			    "database entry for",
			    specPtr->dbName, winPtr->pathName);
		    Tcl_AddErrorInfo(interp, msg);
		    return TCL_ERROR;
		}
	    } else {
		value = specPtr->defValue;
		if ((value != NULL) && !(specPtr->specFlags
			& CK_CONFIG_DONT_SET_DEFAULT)) {
		    if (DoConfig(interp, winPtr, specPtr, value, 1, widgRec) !=
			    TCL_OK) {
			char msg[200];
	
			sprintf(msg,
				"\n    (%s \"%.50s\" in widget \"%.50s\")",
				"default value for",
				specPtr->dbName, winPtr->pathName);
			Tcl_AddErrorInfo(interp, msg);
			return TCL_ERROR;
		    }
		}
	    }
	}
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * FindConfigSpec --
 *
 *	Search through a table of configuration specs, looking for
 *	one that matches a given argvName.
 *
 * Results:
 *	The return value is a pointer to the matching entry, or NULL
 *	if nothing matched.  In that case an error message is left
 *	in interp->result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static Ck_ConfigSpec *
FindConfigSpec(interp, specs, argvName, needFlags, hateFlags)
    Tcl_Interp *interp;		/* Used for reporting errors. */
    Ck_ConfigSpec *specs;	/* Pointer to table of configuration
				 * specifications for a widget. */
    char *argvName;		/* Name (suitable for use in a "config"
				 * command) identifying particular option. */
    int needFlags;		/* Flags that must be present in matching
				 * entry. */
    int hateFlags;		/* Flags that must NOT be present in
				 * matching entry. */
{
    Ck_ConfigSpec *specPtr;
    char c;			/* First character of current argument. */
    Ck_ConfigSpec *matchPtr;	/* Matching spec, or NULL. */
    int length;

    c = argvName[1];
    length = strlen(argvName);
    matchPtr = NULL;
    for (specPtr = specs; specPtr->type != CK_CONFIG_END; specPtr++) {
	if (specPtr->argvName == NULL) {
	    continue;
	}
	if ((specPtr->argvName[1] != c)
		|| (strncmp(specPtr->argvName, argvName, length) != 0)) {
	    continue;
	}
	if (((specPtr->specFlags & needFlags) != needFlags)
		|| (specPtr->specFlags & hateFlags)) {
	    continue;
	}
	if (specPtr->argvName[length] == 0) {
	    matchPtr = specPtr;
	    goto gotMatch;
	}
	if (matchPtr != NULL) {
	    Tcl_AppendResult(interp, "ambiguous option \"", argvName,
		    "\"", (char *) NULL);
	    return (Ck_ConfigSpec *) NULL;
	}
	matchPtr = specPtr;
    }

    if (matchPtr == NULL) {
	Tcl_AppendResult(interp, "unknown option \"", argvName,
		"\"", (char *) NULL);
	return (Ck_ConfigSpec *) NULL;
    }

    /*
     * Found a matching entry.  If it's a synonym, then find the
     * entry that it's a synonym for.
     */

    gotMatch:
    specPtr = matchPtr;
    if (specPtr->type == CK_CONFIG_SYNONYM) {
	for (specPtr = specs; ; specPtr++) {
	    if (specPtr->type == CK_CONFIG_END) {
		Tcl_AppendResult(interp,
			"couldn't find synonym for option \"",
			argvName, "\"", (char *) NULL);
		return (Ck_ConfigSpec *) NULL;
	    }
	    if ((specPtr->dbName == matchPtr->dbName) 
		    && (specPtr->type != CK_CONFIG_SYNONYM)
		    && ((specPtr->specFlags & needFlags) == needFlags)
		    && !(specPtr->specFlags & hateFlags)) {
		break;
	    }
	}
    }
    return specPtr;
}

/*
 *--------------------------------------------------------------
 *
 * DoConfig --
 *
 *	This procedure applies a single configuration option
 *	to a widget record.
 *
 * Results:
 *	A standard Tcl return value.
 *
 * Side effects:
 *	WidgRec is modified as indicated by specPtr and value.
 *	The old value is recycled, if that is appropriate for
 *	the value type.
 *
 *--------------------------------------------------------------
 */

static int
DoConfig(interp, winPtr, specPtr, value, valueIsUid, widgRec)
    Tcl_Interp *interp;		/* Interpreter for error reporting. */
    CkWindow *winPtr;		/* Window containing widget. */
    Ck_ConfigSpec *specPtr;	/* Specifier to apply. */
    char *value;		/* Value to use to fill in widgRec. */
    int valueIsUid;		/* Non-zero means value is a Tk_Uid;
				 * zero means it's an ordinary string. */
    char *widgRec;		/* Record whose fields are to be
				 * modified.  Values must be properly
				 * initialized. */
{
    char *ptr;
    Ck_Uid uid;
    int nullValue;

    nullValue = 0;
    if ((*value == 0) && (specPtr->specFlags & CK_CONFIG_NULL_OK)) {
	nullValue = 1;
    }

    do {
	ptr = widgRec + specPtr->offset;
	switch (specPtr->type) {
	    case CK_CONFIG_BOOLEAN:
		if (Tcl_GetBoolean(interp, value, (int *) ptr) != TCL_OK) {
		    return TCL_ERROR;
		}
		break;
	    case CK_CONFIG_INT:
		if (Tcl_GetInt(interp, value, (int *) ptr) != TCL_OK) {
		    return TCL_ERROR;
		}
		break;
	    case CK_CONFIG_DOUBLE:
		if (Tcl_GetDouble(interp, value, (double *) ptr) != TCL_OK) {
		    return TCL_ERROR;
		}
		break;
	    case CK_CONFIG_STRING: {
		char *old, *new;

		if (nullValue) {
		    new = NULL;
		} else {
		    new = (char *) ckalloc((unsigned) (strlen(value) + 1));
		    strcpy(new, value);
		}
		old = *((char **) ptr);
		if (old != NULL) {
		    ckfree(old);
		}
		*((char **) ptr) = new;
		break;
	    }
	    case CK_CONFIG_UID:
		if (nullValue) {
		    *((Ck_Uid *) ptr) = NULL;
		} else {
		    uid = valueIsUid ? (Ck_Uid) value : Ck_GetUid(value);
		    *((Ck_Uid *) ptr) = uid;
		}
		break;
	    case CK_CONFIG_COLOR: {
	    	int color;

		uid = valueIsUid ? (Ck_Uid) value : Ck_GetUid(value);
		if (Ck_GetColor(interp, (char *) value, &color) != TCL_OK)
		    return TCL_ERROR;
		*((int *) ptr) = color;
		break;
	    }
	    case CK_CONFIG_BORDER: {
		CkBorder *new, *old;

		if (nullValue) {
		    new = NULL;
		} else {
		    uid = valueIsUid ? (Ck_Uid) value : Ck_GetUid(value);
		    new = Ck_GetBorder(interp, uid);
		    if (new == NULL) {
			return TCL_ERROR;
		    }
		}
		old = *((CkBorder **) ptr);
		if (old != NULL) {
		    Ck_FreeBorder(old);
		}
		*((CkBorder **) ptr) = new;
		break;
	    }
	    case CK_CONFIG_JUSTIFY:
		uid = valueIsUid ? (Ck_Uid) value : Ck_GetUid(value);
		if (Ck_GetJustify(interp, uid, (Ck_Justify *) ptr) != TCL_OK) {
		    return TCL_ERROR;
		}
		break;
	    case CK_CONFIG_ANCHOR:
		uid = valueIsUid ? (Ck_Uid) value : Ck_GetUid(value);
		if (Ck_GetAnchor(interp, uid, (Ck_Anchor *) ptr) != TCL_OK) {
		    return TCL_ERROR;
		}
		break;
	    case CK_CONFIG_COORD:
		if (Ck_GetCoord(interp, winPtr, value, (int *) ptr) != TCL_OK)
		    return TCL_ERROR;
		break;
	    case CK_CONFIG_ATTR:
		if (Ck_GetAttr(interp, value, (int *) ptr) != TCL_OK)
		    return TCL_ERROR;
		break;
	    case CK_CONFIG_WINDOW: {
		CkWindow *winPtr2;

		if (nullValue) {
		    winPtr2 = NULL;
		} else {
		    winPtr2 = Ck_NameToWindow(interp, value, winPtr);
		    if (winPtr2 == NULL) {
			return TCL_ERROR;
		    }
		}
		*((CkWindow **) ptr) = winPtr2;
		break;
	    }
	    case CK_CONFIG_CUSTOM:
		if ((*specPtr->customPtr->parseProc)(
			specPtr->customPtr->clientData, interp, winPtr,
			value, widgRec, specPtr->offset) != TCL_OK) {
		    return TCL_ERROR;
		}
		break;
	    default: {
	      char buf[128];
		sprintf(buf, "bad config table: unknown type %d",specPtr->type);
		Tcl_SetObjResult(interp, Tcl_NewStringObj(buf,-1));
		return TCL_ERROR;
	    }
	}
	specPtr++;
    } while ((specPtr->argvName == NULL) && (specPtr->type != CK_CONFIG_END));
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * Ck_ConfigureInfo --
 *
 *	Return information about the configuration options
 *	for a window, and their current values.
 *
 * Results:
 *	Always returns TCL_OK.  Interp->result will be modified
 *	hold a description of either a single configuration option
 *	available for "widgRec" via "specs", or all the configuration
 *	options available.  In the "all" case, the result will
 *	available for "widgRec" via "specs".  The result will
 *	be a list, each of whose entries describes one option.
 *	Each entry will itself be a list containing the option's
 *	name for use on command lines, database name, database
 *	class, default value, and current value (empty string
 *	if none).  For options that are synonyms, the list will
 *	contain only two values:  name and synonym name.  If the
 *	"name" argument is non-NULL, then the only information
 *	returned is that for the named argument (i.e. the corresponding
 *	entry in the overall list is returned).
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
Ck_ConfigureInfo(interp, winPtr, specs, widgRec, argvName, flags)
    Tcl_Interp *interp;		/* Interpreter for error reporting. */
    CkWindow *winPtr;		/* Window corresponding to widgRec. */
    Ck_ConfigSpec *specs;	/* Describes legal options. */
    char *widgRec;		/* Record whose fields contain current
				 * values for options. */
    char *argvName;		/* If non-NULL, indicates a single option
				 * whose info is to be returned.  Otherwise
				 * info is returned for all options. */
    int flags;			/* Used to specify additional flags
				 * that must be present in config specs
				 * for them to be considered. */
{
    Ck_ConfigSpec *specPtr;
    int needFlags, hateFlags;
    char *list;
    char *leader = "{";

    needFlags = flags & ~(CK_CONFIG_USER_BIT - 1);
    if (!(winPtr->mainPtr->flags & CK_HAS_COLOR)) {
	hateFlags = CK_CONFIG_COLOR_ONLY;
    } else {
	hateFlags = CK_CONFIG_MONO_ONLY;
    }

    /*
     * If information is only wanted for a single configuration
     * spec, then handle that one spec specially.
     */

    Tcl_SetResult(interp, (char *) NULL, TCL_STATIC);
    if (argvName != NULL) {
      char *s;
	specPtr = FindConfigSpec(interp, specs, argvName, needFlags, hateFlags);
	if (specPtr == NULL) {
	    return TCL_ERROR;
	}
	s = FormatConfigInfo(interp, winPtr, specPtr, widgRec);
	Tcl_SetObjResult(interp, Tcl_NewStringObj(s,-1));
	Tcl_Free(s);
	return TCL_OK;
    }

    /*
     * Loop through all the specs, creating a big list with all
     * their information.
     */

    for (specPtr = specs; specPtr->type != CK_CONFIG_END; specPtr++) {
	if ((argvName != NULL) && (specPtr->argvName != argvName)) {
	    continue;
	}
	if (((specPtr->specFlags & needFlags) != needFlags)
		|| (specPtr->specFlags & hateFlags)) {
	    continue;
	}
	if (specPtr->argvName == NULL) {
	    continue;
	}
	list = FormatConfigInfo(interp, winPtr, specPtr, widgRec);
	Tcl_AppendResult(interp, leader, list, "}", (char *) NULL);
	Tcl_Free(list);
	leader = " {";
    }
    return TCL_OK;
}
/*
 *----------------------------------------------------------------------
 *
 * Ck_ConfigureValue --
 *
 *      This procedure returns the current value of a configuration
 *      option for a widget.
 *
 * Results:
 *      The return value is a standard Tcl completion code (TCL_OK or
 *      TCL_ERROR).  Interp->result will be set to hold either the value
 *      of the option given by argvName (if TCL_OK is returned) or
 *      an error message (if TCL_ERROR is returned).
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
Ck_ConfigureValue(interp, winPtr, specs, widgRec, argvName, flags)
    Tcl_Interp *interp;         /* Interpreter for error reporting. */
    CkWindow *winPtr;           /* Window corresponding to widgRec. */
    Ck_ConfigSpec *specs;       /* Describes legal options. */
    char *widgRec;              /* Record whose fields contain current
                                 * values for options. */
    char *argvName;             /* Gives the command-line name for the
                                 * option whose value is to be returned. */
    int flags;                  /* Used to specify additional flags
                                 * that must be present in config specs
                                 * for them to be considered. */
{
    Ck_ConfigSpec *specPtr;
    int needFlags, hateFlags;
    char buf[256];

    needFlags = flags & ~(CK_CONFIG_USER_BIT - 1);
    if (winPtr->mainPtr->flags & CK_HAS_COLOR)
        hateFlags = CK_CONFIG_MONO_ONLY;
    else
        hateFlags = CK_CONFIG_COLOR_ONLY;
    specPtr = FindConfigSpec(interp, specs, argvName, needFlags, hateFlags);
    if (specPtr == NULL) {
        return TCL_ERROR;
    }
    Tcl_SetObjResult( interp, Tcl_NewStringObj(FormatConfigValue(interp, winPtr, specPtr, widgRec, buf),-1));
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * FormatConfigInfo --
 *
 *	Create a valid Tcl list holding the configuration information
 *	for a single configuration option.
 *
 * Results:
 *	A Tcl list, dynamically allocated.  The caller is expected to
 *	arrange for this list to be freed eventually.
 *
 * Side effects:
 *	Memory is allocated.
 *
 *--------------------------------------------------------------
 */

static char *
FormatConfigInfo(interp, winPtr, specPtr, widgRec)
    Tcl_Interp *interp;			/* Interpreter to use for things
					 * like floating-point precision. */
    CkWindow *winPtr;			/* Window corresponding to widget. */
    Ck_ConfigSpec *specPtr;		/* Pointer to information describing
					 * option. */
    char *widgRec;			/* Pointer to record holding current
					 * values of info for widget. */
{
    char *argv[6], *result;
    char buffer[256] = "";

    argv[0] = specPtr->argvName;
    argv[1] = specPtr->dbName;
    argv[2] = specPtr->dbClass;
    argv[3] = specPtr->defValue;
    if (specPtr->type == CK_CONFIG_SYNONYM) {
	return Tcl_Merge(2, argv);
    }
    argv[4] = FormatConfigValue(interp, winPtr, specPtr, widgRec, buffer);
    if (argv[1] == NULL) {
	argv[1] = "";
    }
    if (argv[2] == NULL) {
	argv[2] = "";
    }
    if (argv[3] == NULL) {
	argv[3] = "";
    }
    if (argv[4] == NULL) {
      buffer[0] = '\0';
      argv[4] = buffer;
    }
    result = Tcl_Merge(5, argv);
    if ( argv[4] != buffer ) {
      ckfree(argv[4]);
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * FormatConfigValue --
 *
 *      This procedure formats the current value of a configuration
 *      option.
 *
 * Results:
 *      The return value is the formatted value of the option given
 *      by specPtr and widgRec.  If the value is static, so that it
 *      need not be freed, *freeProcPtr will be set to NULL;  otherwise
 *      *freeProcPtr will be set to the address of a procedure to
 *      free the result, and the caller must invoke this procedure
 *      when it is finished with the result.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static char *
FormatConfigValue(interp, winPtr, specPtr, widgRec, buffer)
    Tcl_Interp *interp;         /* Interpreter for use in real conversions. */
    CkWindow *winPtr;           /* Window corresponding to widget. */
    Ck_ConfigSpec *specPtr;     /* Pointer to information describing option.
                                 * Must not point to a synonym option. */
    char *widgRec;              /* Pointer to record holding current
                                 * values of info for widget. */
    char *buffer;               /* Static buffer to use for small values.
                                 * Must have at least 200 bytes of storage. */
{
    char *ptr, *result;
    Tcl_FreeProc *freeProcPtr = NULL;
    
    ptr = widgRec + specPtr->offset;
    buffer[0] = '\0';
    result = "";
    switch (specPtr->type) {
	case CK_CONFIG_BOOLEAN:
	    if (*((int *) ptr) == 0) {
	      sprintf( buffer, "%d", 0);
	      result = buffer;
	    } else {
	      sprintf( buffer, "%d", 1);
	      result = buffer;
	    }
	    break;
	case CK_CONFIG_INT:
        case CK_CONFIG_COORD:
	    sprintf(buffer, "%d", *((int *) ptr));
	    result = buffer;
	    break;
	case CK_CONFIG_DOUBLE:
	    Tcl_PrintDouble(interp, *((double *) ptr), buffer);
	    result = buffer;
	    break;
	case CK_CONFIG_STRING:
	    result = (*(char **) ptr);
            if (result == NULL) {
	      buffer[0] = '\0';
	      result = buffer;
	    }
	    break;
	case CK_CONFIG_UID: {
	    Ck_Uid uid = *((Ck_Uid *) ptr);
	    if (uid != NULL) {
		result = uid;
	    }
	    break;
	}
	case CK_CONFIG_COLOR: {
	    result = Ck_NameOfColor(*((int *) ptr));
	    break;
	}
	case CK_CONFIG_BORDER: {
	    CkBorder *borderPtr = *((CkBorder **) ptr);
	    if (borderPtr != NULL) {
		result = Ck_NameOfBorder(borderPtr);
	    }
	    break;
	}
	case CK_CONFIG_JUSTIFY:
	    result = Ck_NameOfJustify(*((Ck_Justify *) ptr));
	    break;
	case CK_CONFIG_ANCHOR:
	    result = Ck_NameOfAnchor(*((Ck_Anchor *) ptr));
	    break;
	case CK_CONFIG_ATTR:
	    result = Ck_NameOfAttr(*(int *) ptr);
	    break;
	case CK_CONFIG_WINDOW: {
	    CkWindow *winPtr2;

	    winPtr2 = *((CkWindow **) ptr);
	    if (winPtr2 != NULL) {
		result = winPtr2->pathName;
	    }
	    break;
	}
	case CK_CONFIG_CUSTOM:
	    result = (*specPtr->customPtr->printProc)(
		    specPtr->customPtr->clientData, winPtr, widgRec,
		    specPtr->offset, &freeProcPtr);
	    break;
	default: 
	    result = "?? unknown type ??";
    }
    if ( result != buffer) {
      strcat( buffer, result);
      if ( freeProcPtr != NULL ) {
	(*freeProcPtr)( result );
      }
    }
    return buffer;
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_FreeOptions --
 *
 *	Free up all resources associated with configuration options.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Any resource in widgRec that is controlled by a configuration
 *	option is freed in the appropriate fashion.
 *
 *----------------------------------------------------------------------
 */

void
Ck_FreeOptions(specs, widgRec, needFlags)
    Ck_ConfigSpec *specs;	/* Describes legal options. */
    char *widgRec;		/* Record whose fields contain current
				 * values for options. */
    int needFlags;		/* Used to specify additional flags
				 * that must be present in config specs
				 * for them to be considered. */
{
    Ck_ConfigSpec *specPtr;
    char *ptr;

    for (specPtr = specs; specPtr->type != CK_CONFIG_END; specPtr++) {
	if ((specPtr->specFlags & needFlags) != needFlags) {
	    continue;
	}
	ptr = widgRec + specPtr->offset;
	switch (specPtr->type) {
	    case CK_CONFIG_STRING:
		if (*((char **) ptr) != NULL) {
		    ckfree(*((char **) ptr));
		    *((char **) ptr) = NULL;
		}
		break;
	    case CK_CONFIG_BORDER:
		if (*((CkBorder **) ptr) != NULL) {
		    Ck_FreeBorder(*((CkBorder **) ptr));
		    *((CkBorder **) ptr) = NULL;
		}
		break;
	}
    }
}
