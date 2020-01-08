/* 
 * ckGet.c --
 *
 *	This file contains a number of "Ck_GetXXX" procedures, which
 *	parse text strings into useful forms for Ck.
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

typedef struct {
    short fg, bg;
} CPair;

static CPair *cPairs = NULL;
static int numPairs, newPair;

/*
 * The hash table below is used to keep track of all the Ck_Uids created
 * so far.
 */

static Tcl_HashTable uidTable;
static int initialized = 0;

/*
 * Maximum color distance threshold
 * We tried to map X11 colors to the 256-colors terminal colors
 * In most case the mapping is not exact. The color difference
 * between the X11 color and its terminal approximation is computed.
 * If the distance is over the threshold defined here, the color name
 * will not be usable.
 *
 * The threshold might be changed at runtime.
 * See the ctab array below for explanation.
 *
 */
static int color_threshold = 50;

static Tcl_HashTable colorTable;

/*
 * Definition os X11 colors
 *
 */
static struct {
  char *name;
  unsigned char r;
  unsigned char g;
  unsigned char b;
  
} x11ctab[] =
  {
   /*
    *  Curses colors - all systems have these 8 colors
    */
   {  "black"   ,0      ,0      ,0 },
   {  "red"	,0x80	,0 	,0 },
   {  "green"	,0	,0x80 	,0 },
   {  "yellow"	,0x80	,0x80 	,0 },
   {  "blue"	,0	,0 	,0x80 },
   {  "magenta"	,0x80	,0 	,0x80 },
   {  "cyan"	,0	,0x80 	,0x80 },
   {  "white"	,0xc0	,0xc0 	,0xc0 },

   /*
    *  Curses colors - allmost all systems have these 8 colors
    */
   {  "high-black"      ,0x80   ,0x80   ,0x80 },
   {  "high-red"        ,0xff	,0 	,0 },
   {  "high-green"	,0	,0xff 	,0 },
   {  "high-yellow"	,0xff	,0xff 	,0 },
   {  "high-blue"	,0	,0 	,0xff },
   {  "high-magenta"	,0xff	,0 	,0xff },
   {  "high-cyan"	,0	,0xff 	,0xff },
   {  "high-white"	,0xff	,0xff 	,0xff },

   /* extra colors are kept in separate file */
#ifdef X11COLOR
#undef X11COLOR
#endif
#define X11COLOR(name,r,g,b) { "x11 " name , r,g,b },
#include "x11_colors.h"
#endif
#undef X11COLOR

  };

/*
 * Curses terminal colors
 * Up to 256 colors supported
 */
struct termcolor_t {
  short r,g,b;
};
static struct termcolor_t termcolors[256];

/*
 *  Definition of a color
 */
struct color_t {
  char *name;            /* color name */
  int   value;           /* index of closest color in termcolors array */
  short r,g,b;
  short x11r,x11g,x11b;
  int   dist;            /* distance to cell */
};

static struct {
    char *name;
    int value;
} atab[] = {
    { "blink", A_BLINK },
    { "bold", A_BOLD },
    { "dim", A_DIM },
    { "normal", A_NORMAL },
    { "reverse", A_REVERSE },
    { "standout", A_STANDOUT },
    { "underline", A_UNDERLINE }
};

static inline int square(int x) { return x*x; }



/*
 *----------------------------------------------------------------------
 *
 * findBestCell --
 *
 *	Find the color cell in ncurses allocated colors that matches
 *      the given color.
 *
 * Results:
 *      None
 *
 * Side effects:
 *	colorPtr is updated, its value and dist fields are updated.
 *
 *----------------------------------------------------------------------
 */

static void
findBestCell( colorPtr )
     struct color_t *colorPtr;
{
  int j, ibest = -1, dbest = INT_MAX;

  colorPtr->value = -1;
  colorPtr->dist = INT_MAX;
  
  for( j = 0; (j < COLORS) && (j < 256); j++ ) {
    dist  = square(colorPtr->x11r - termcolors[j].r);
    dist += square(colorPtr->x11g - termcolors[j].g);
    dist += square(colorPtr->x11b - termcolors[j].b);
    if ( dist < dbest ) {
      dbest = dist;
      ibest = j;
    }
  }
  if ( ibest >= 0 ) {
    dist = (int) sqrt(dist);
    colorPtr->value = ibest;
    colorPtr->dist = (int) sqrt(dist);
    colorPtr->r = termcolors[ibest].r;
    colorPtr->g = termcolors[ibest].g;
    colorPtr->b = termcolors[ibest].b;
  }
}


/*
 *----------------------------------------------------------------------
 *
 * findBestCells --
 *
 *	Scan the whole color hash table updating the best matching
 *      cell in the terminal ncurses color table.
 *
 * Results:
 *      None
 *
 * Side effects:
 *	colorPtr is updated, its value and dist fields are updated.
 *
 *----------------------------------------------------------------------
 */
static void findBestCells()
{
  struct Tcl_HashSearch search;
  struct Tcl_HashEntry *entryPtr;
  struct color_t *colorPtr;

  for( entryPtr = Tcl_FirstHashEntry( colorTable, &search );
       entryPtr != NULL;
       entryPtr = Tcl_NextHashEntry( &search ) )
    {
      colorPtr = (struct color_t*) Tcl_GetHashValue( entryPtr );
      findBestCell( colorPtr );
    }
}


/*
 *----------------------------------------------------------------------
 *
 * setColor --
 *
 *	Sets an entry of the global color hash table
 *
 * Results:
 *      None
 *
 * Side effects:
 *	Update or create a new entry
 *
 *----------------------------------------------------------------------
 */

static void
setColor( colorPtr )
     struct color_t *colorPtr;
{
  struct color_t *colorEntryPtr = NULL;
  Tcl_HashEntry *entryPtr = NULL;
  int isnew = 0;

  /* create or update hash table entry */
  entryPtr = Tcl_CreateHashEntry( &colorTable, colorPtr->name, &isnew);
  if ( isnew ) {
    colorEntryPtr = (struct color_t *) Tcl_Alloc(sizeof(*colorEntryPtr));
  }
  else {
    colorEntryPtr = (struct color_t *) Tcl_GetHashValue( entryPtr );
  }
  memcpy( colorEntryPtr, colorPtr, sizeof(*colorPtr) );
  Tcl_SetHashValue( entryPtr, colorEntryPtr );

  /* find closest matching color */
  findBestCell( colorPtr );
}



/*
 *----------------------------------------------------------------------
 *
 * setCell --
 *
 *	Sets a terminal color cell. Parameter 'idx' gives its index
 *      which must be between 0 and max(COLORS,256)-1. Values outside
 *      of range are ignored, the function just returns doing nothing.
 *
 *      The color is defined by its red, green and blue components are
 *      exptected to be in the range 0-255. If a value outside of range
 *      is detected the function does nothing.
 *
 *      The function calls 'init_color' if color change is supported
 *      by the terminal. AFterwards it calls 'color_content to get
 *      the actual value of the color.
 *
 * Results:
 *      None
 *
 * Side effects:
 *	Updates the terminal color table
 *
 *----------------------------------------------------------------------
 */

static void
setCell( idx, red, green, blue )
     int idx;		/* index of cell to update */
     short red;		/* red component of color */
     short green;	/* green component of color */
     short blue;	/* blue component of color */
{
  if ( idx < 0 || idx >= COLORS || idx >= 256 ||
       red < 0 || red >= 256 ||
       green <= 0 || green >= 256 ||
       blue < 0 || blue >= 256 ) {
    return;
  }
  if (can_change_color()) {
    init_color( i, (1000*red)/255,(1000*green)/255,(1000*blue)/255);
  }
  color_content( i, &red, &green, &blue );
  termcolors[i].r = (255*red)/1000;
  termcolors[i].g = (255*green)/1000;
  termcolors[i].b = (255*blue)/1000;
}


/*
 *----------------------------------------------------------------------
 *
 * Ck_InitColor --
 *
 *	Initialize the color table. The initialisation depends
 *      on the number of available colors reported by ncurses.
 *
 * Results:
 *      None
 *
 * Side effects:
 *	Initialize color hashtable
 *
 *----------------------------------------------------------------------
 */

void
Ck_InitColor()
{
  int dist, i = 0, j = 0;

  /* retreive the color definition actually used 
   * by ncurses. */
  for ( i = 0; (i < COLORS) && (i < 256); ++i ) {
    struct termcolor_t *col = termcolors + i;
    color_content( i, &col->r, &col->g, &col->b );
    col->r = (short)((int)col->r*255)/1000;
    col->g = (short)((int)col->g*255)/1000;
    col->b = (short)((int)col->b*255)/1000;
    
    if ( (i >= 8) && (i < 16) ) {
      /* bright colors */
      col->r = col->g = col->b = 0x55;
      switch( i-8 ) {
      case COLOR_BLACK: break;
      case COLOR_RED:     col->r = 0xFF; break;
      case COLOR_GREEN:   col->g = 0xFF; break;
      case COLOR_YELLOW:  col->r = col->g = 0xFF; break;
      case COLOR_BLUE:    col->b = 0xFF; break;
      case COLOR_MAGENTA: col->r = col->b = 0xFF; break;
      case COLOR_CYAN:    col->g = col->b = 0xFF; break;
      case COLOR_WHITE:   col->r = col->g = col->b = 0xFF; break;
      }
      setCell( i, col->r, col->g, col->b );
    }
    if ( (i >= 16) && (i < 232) ) {
      short scale[6] = {0, 51, 102, 153, 204, 255};
      /* 6x6x6 color cube */
      col->r = scale[(i-16)/36];
      col->g = scale[((i-16)%36)/6];
      col->b = scale[(i-16)%6];
      setCell( i, col->r, col->g, col->b );
    }
    if ( i >= 232 ) {
      /* gray ramp */
      col->r = col->g = col->b = 8 + 10*(i-232);
      setCell( i, col->r, col->g, col->b );
    }
  }

  /* start creation of color hashtable */
  Tcl_InitHashTable( &colorTable, TCL_STRING_KEYS );

  /* force system entries */
  for ( i = 0; i < 16; ++i ) {
    Tcl_HashEntry *entryPtr = NULL;
    int isnew = 0;
    entryPtr = Tcl_CreateHashEntry( &colorTable, x11ctab[i].name, &isnew);
    if ( isnew ) {
      struct color_t *colorPtr = NULL;
      colorPtr = (struct color_t *) Tcl_Alloc(sizeof(*colorPtr));
      colorPtr->x11r = colorPtr->r = x11ctab[i].r;
      colorPtr->x11g = colorPtr->g = x11ctab[i].g;
      colorPtr->x11b = colorPtr->b = x11ctab[i].b;
      colorPtr->name = x11ctab[i].name;
      colorPtr->dist = 0;
      colorPtr->value = i;
      Tcl_SetHashValue( entryPtr, colorPtr );
    }
  }

  /* scan all X11 colors and find the best matching terminal colors */
  for ( /* nothing */; i < sizeof(x11ctab)/sizeof(x11ctab[0]); ++i) {
    Tcl_HashEntry *entryPtr = NULL;
    int isnew = 0;
    entryPtr = Tcl_CreateHashEntry( &colorTable, x11ctab[i].name, &isnew);
    if ( isnew ) {
      struct color_t *colorPtr = NULL;
      colorPtr = (struct color_t *) Tcl_Alloc(sizeof(*colorPtr));
      colorPtr->x11r = x11ctab[i].r;
      colorPtr->x11g = x11ctab[i].g;
      colorPtr->x11b = x11ctab[i].b;
      colorPtr->name = x11ctab[i].name;
      findBestCell( colorPtr );      
    }
  }
}


/*
 *----------------------------------------------------------------------
 *
 * Ck_GetUid --
 *
 *	Given a string, this procedure returns a unique identifier
 *	for the string.
 *
 * Results:
 *	This procedure returns a Ck_Uid corresponding to the "string"
 *	argument.  The Ck_Uid has a string value identical to string
 *	(strcmp will return 0), but it's guaranteed that any other
 *	calls to this procedure with a string equal to "string" will
 *	return exactly the same result (i.e. can compare Ck_Uid
 *	*values* directly, without having to call strcmp on what they
 *	point to).
 *
 * Side effects:
 *	New information may be entered into the identifier table.
 *
 *----------------------------------------------------------------------
 */

Ck_Uid
Ck_GetUid(string)
    char *string;		/* String to convert. */
{
    int dummy;

    if (!initialized) {
	Tcl_InitHashTable(&uidTable, TCL_STRING_KEYS);
	initialized = 1;
    }
    return (Ck_Uid) Tcl_GetHashKey(&uidTable,
	    Tcl_CreateHashEntry(&uidTable, string, &dummy));
}

/*
 *------------------------------------------------------------------------
 *
 * Ck_GetColor --
 *
 *	Given a color specification, return curses color value.
 *      Color specification can take 3 forms :
 *      - "@<index>" index gives the name of curses color
 *      - "#<rgb>", "#<rrggbb>" where r, g, b are 1 ou 2-digits hex values :
 *        in this case the closest known color is returned.
 *      - "<name>" : it is color name which is search in color array
 *
 * Results:
 *	TCL_OK if color found, curses color in *colorPtr.
 *	TCL_ERROR if color not found; interp->result contains an
 *	error message.
 *
 * Side effects:
 *	None.
 *
 *------------------------------------------------------------------------
 */

int
Ck_GetColor(interp, name, colorPtr)
    Tcl_Interp *interp;
    char *name;
    int *colorPtr;
{
    int i, len;

    len = strlen(name);
    if (len > 0) {
      if ( name[0] == '@' ) {
	if ( TCL_OK != Tcl_GetInt(interp, name+1, &i) ) {
	  return TCL_ERROR;
	}
	if ( i < 0 || i >= COLORS ) {
	  Tcl_AppendResult( interp, "color index ", name+1 , " out of range.", NULL);
	  return TCL_ERROR;
	}
	if (colorPtr != NULL) {
	  *colorPtr = i;
	}
	return TCL_OK;
      }
      else if ( name[0] == '#' ) {
	Tcl_HashEntry *entryPtr;
	Tcl_HashSearch search;
	int ibest = -1, dbest = INT_MAX, dist;
	int r, g, b;
	
	switch( strlen(name+1) ) {
	case 3:
	  sscanf( name+1, "%01x%01x%01x", &r, &g, &b);
	  r *= 16; g *= 16; b *= 16;
	  break;
	case 6:
	  sscanf( name+1, "%02x%02x%02x", &r, &g, &b);
	  break;
	default:
	  Tcl_AppendResult( interp, "invalid color RGB specification : ", name+1, NULL);
	  return TCL_ERROR;
	}

	for( entryPtr = Tcl_FirstHashEntry( &colorTable, &search );
	     entryPtr != NULL;
	     entryPtr = Tcl_NextHashEntry( &search )) {
	  struct color_t *colPtr;
	  colPtr = (struct color_t*) Tcl_GetHashValue( entryPtr );
	  dist  = square(colPtr->x11r - r);
	  dist += square(colPtr->x11g - g);
	  dist += square(colPtr->x11b - b);
	  if ( dist < dbest ) {
	    dbest = dist;
	    ibest = colPtr->value;
	  }
	}
	
	if ( colorPtr != NULL ) {
	  *colorPtr = ibest;
	}
	return TCL_OK;
      }
      else {
	Tcl_HashEntry *entryPtr;
	entryPtr = Tcl_FindHashEntry( &colorTable, name);
	if ( entryPtr != NULL ) {
	  struct color_t *colPtr;
	  colPtr = (struct color_t *) Tcl_GetHashValue( entryPtr );
	  if ( (colPtr->dist < color_threshold) ) {
	    if (colorPtr != NULL) {
	      *colorPtr = colPtr->value;
	    }
	    return TCL_OK;
	  }
	}
      }
    }
    Tcl_AppendResult(interp, "bad color \"", name, "\"", (char *) NULL);
    return TCL_ERROR;
}

/*
 *------------------------------------------------------------------------
 *
 * Ck_NameOfColor --
 *
 *	Given a curses color, return its name.
 *
 * Results:
 *	String: name of color, or NULL if no valid color.
 *
 * Side effects:
 *	None.
 *
 *------------------------------------------------------------------------
 */

char *
Ck_NameOfColor(color)
    int color;		/* Curses color to get name for */
{
  Tcl_HashEntry *entryPtr;
  Tcl_HashSearch search;

  for ( entryPtr = Tcl_FirstHashEntry( &colorTable, &search);
	entryPtr != NULL;
	entryPtr = Tcl_NextHashEntry( &search )) {
    struct color_t *colorPtr;
    colorPtr = (struct color_t *) Tcl_GetHashValue( entryPtr );
    if ( (colorPtr->value == color) && (colorPtr->dist < color_threshold) ) {
      return colorPtr->name;
    }
  }
  
  return NULL;
}

/*
 *------------------------------------------------------------------------
 *
 * Ck_GetAttr --
 *
 *	Given an attribute specification, return attribute value.
 *
 * Results:
 *	TCL_OK if color found, curses color in *colorPtr.
 *	TCL_ERROR if color not found; interp->result contains an
 *	error message.
 *
 * Side effects:
 *	None.
 *
 *------------------------------------------------------------------------
 */

int
Ck_GetAttr(interp, name, attrPtr)
    Tcl_Interp *interp;
    char *name;
    int *attrPtr;
{
    int i, k, len, largc;
    char **largv;

    if (Tcl_SplitList(interp, name, &largc, &largv) != TCL_OK)
	return TCL_ERROR;
    if (attrPtr != NULL)
	*attrPtr = A_NORMAL;
    if (largc > 1 || (largc == 1 && largv[0][0] != '\0')) {
	for (i = 0; i < largc; i++) {
	    len = strlen(largv[i]);
	    if (len > 0) {
		for (k = 0; k < sizeof (atab) / sizeof (atab[0]); k++)
		    if (strncmp(largv[i], atab[k].name, len) == 0) {
			if (attrPtr != NULL)
		            *attrPtr |= atab[k].value;
			break;
		    }
		if (k >= sizeof (atab) / sizeof (atab[0])) {
		    Tcl_AppendResult(interp, "bad attribute \"",
			name, "\"", (char *) NULL);
		    ckfree((char *) largv);
		    return TCL_ERROR;
		}
	    }
	}
    }
    ckfree((char *) largv);
    return TCL_OK;
}

/*
 *------------------------------------------------------------------------
 *
 * Ck_NameOfAttr --
 *
 *	Given an attribute value, return its textual specification.
 *
 * Results:
 *	interp->result contains result or message.
 *
 * Side effects:
 *	None.
 *
 *------------------------------------------------------------------------
 */

char *
Ck_NameOfAttr(attr)
    int attr;
{
    int i;
    char *result;
    Tcl_DString list;

    Tcl_DStringInit(&list);
    if (attr == -1 || attr == A_NORMAL)
        Tcl_DStringAppendElement(&list, "normal");
    else {
	for (i = 0; i < sizeof (atab) / sizeof (atab[0]); i++)
	    if (attr & atab[i].value)
                Tcl_DStringAppendElement(&list, atab[i].name);
    }
    result = ckalloc(Tcl_DStringLength(&list) + 1);
    strcpy(result, Tcl_DStringValue(&list));
    Tcl_DStringFree(&list);
    return result;
}
/*
 *------------------------------------------------------------------------
 *
 * Ck_GetColorPair --
 *
 *	Given background/foreground curses colors, a color pair
 *	is allocated and returned.
 *
 * Results:
 *	TCL_OK if color found, curses color in *colorPtr.
 *	TCL_ERROR if color not found; interp->result contains an
 *	error message.
 *
 * Side effects:
 *	None.
 *
 *------------------------------------------------------------------------
 */

int
Ck_GetPair(winPtr, fg, bg)
    CkWindow *winPtr;
    int fg, bg;
{
    int i;

    if (!(winPtr->mainPtr->flags & CK_HAS_COLOR))
	return COLOR_PAIR(0);
    if (cPairs == NULL) {
	cPairs = (CPair *) ckalloc(sizeof (CPair) * (COLOR_PAIRS + 2));
	numPairs = 0;
	newPair = 1;
    }
    for (i = 1; i < numPairs; i++)
	if (cPairs[i].fg == fg && cPairs[i].bg == bg)
	    return COLOR_PAIR(i);
    i = newPair;
    cPairs[i].fg = fg;
    cPairs[i].bg = bg;
    init_pair((short) i, (short) fg, (short) bg);
    if (++newPair >= COLOR_PAIRS)
	newPair = 1;
    else
	numPairs = newPair;
    return COLOR_PAIR(i);
}

/*
 *--------------------------------------------------------------
 *
 * Ck_GetAnchor --
 *
 *	Given a string, return the corresponding Ck_Anchor.
 *
 * Results:
 *	The return value is a standard Tcl return result.  If
 *	TCL_OK is returned, then everything went well and the
 *	position is stored at *anchorPtr;  otherwise TCL_ERROR
 *	is returned and an error message is left in
 *	interp->result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
Ck_GetAnchor(interp, string, anchorPtr)
    Tcl_Interp *interp;		/* Use this for error reporting. */
    char *string;		/* String describing a direction. */
    Ck_Anchor *anchorPtr;	/* Where to store Ck_Anchor corresponding
				 * to string. */
{
    switch (string[0]) {
	case 'n':
	    if (string[1] == 0) {
		*anchorPtr = CK_ANCHOR_N;
		return TCL_OK;
	    } else if ((string[1] == 'e') && (string[2] == 0)) {
		*anchorPtr = CK_ANCHOR_NE;
		return TCL_OK;
	    } else if ((string[1] == 'w') && (string[2] == 0)) {
		*anchorPtr = CK_ANCHOR_NW;
		return TCL_OK;
	    }
	    goto error;
	case 's':
	    if (string[1] == 0) {
		*anchorPtr = CK_ANCHOR_S;
		return TCL_OK;
	    } else if ((string[1] == 'e') && (string[2] == 0)) {
		*anchorPtr = CK_ANCHOR_SE;
		return TCL_OK;
	    } else if ((string[1] == 'w') && (string[2] == 0)) {
		*anchorPtr = CK_ANCHOR_SW;
		return TCL_OK;
	    } else {
		goto error;
	    }
	case 'e':
	    if (string[1] == 0) {
		*anchorPtr = CK_ANCHOR_E;
		return TCL_OK;
	    }
	    goto error;
	case 'w':
	    if (string[1] == 0) {
		*anchorPtr = CK_ANCHOR_W;
		return TCL_OK;
	    }
	    goto error;
	case 'c':
	    if (strncmp(string, "center", strlen(string)) == 0) {
		*anchorPtr = CK_ANCHOR_CENTER;
		return TCL_OK;
	    }
	    goto error;
    }

    error:
    Tcl_AppendResult(interp, "bad anchor position \"", string,
	    "\": must be n, ne, e, se, s, sw, w, nw, or center",
	    (char *) NULL);
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * Ck_NameOfAnchor --
 *
 *	Given a Ck_Anchor, return the string that corresponds
 *	to it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

char *
Ck_NameOfAnchor(anchor)
    Ck_Anchor anchor;		/* Anchor for which identifying string
				 * is desired. */
{
    switch (anchor) {
	case CK_ANCHOR_N: return "n";
	case CK_ANCHOR_NE: return "ne";
	case CK_ANCHOR_E: return "e";
	case CK_ANCHOR_SE: return "se";
	case CK_ANCHOR_S: return "s";
	case CK_ANCHOR_SW: return "sw";
	case CK_ANCHOR_W: return "w";
	case CK_ANCHOR_NW: return "nw";
	case CK_ANCHOR_CENTER: return "center";
    }
    return "unknown anchor position";
}

/*
 *--------------------------------------------------------------
 *
 * Ck_GetJustify --
 *
 *	Given a string, return the corresponding Ck_Justify.
 *
 * Results:
 *	The return value is a standard Tcl return result.  If
 *	TCL_OK is returned, then everything went well and the
 *	justification is stored at *justifyPtr;  otherwise
 *	TCL_ERROR is returned and an error message is left in
 *	interp->result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
Ck_GetJustify(interp, string, justifyPtr)
    Tcl_Interp *interp;		/* Use this for error reporting. */
    char *string;		/* String describing a justification style. */
    Ck_Justify *justifyPtr;	/* Where to store Ck_Justify corresponding
				 * to string. */
{
    int c, length;

    c = string[0];
    length = strlen(string);

    if ((c == 'l') && (strncmp(string, "left", length) == 0)) {
	*justifyPtr = CK_JUSTIFY_LEFT;
	return TCL_OK;
    }
    if ((c == 'r') && (strncmp(string, "right", length) == 0)) {
	*justifyPtr = CK_JUSTIFY_RIGHT;
	return TCL_OK;
    }
    if ((c == 'c') && (strncmp(string, "center", length) == 0)) {
	*justifyPtr = CK_JUSTIFY_CENTER;
	return TCL_OK;
    }
    if ((c == 'f') && (strncmp(string, "fill", length) == 0)) {
	*justifyPtr = CK_JUSTIFY_FILL;
	return TCL_OK;
    }

    Tcl_AppendResult(interp, "bad justification \"", string,
	    "\": must be left, right, center, or fill",
	    (char *) NULL);
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * Ck_NameOfJustify --
 *
 *	Given a Ck_Justify, return the string that corresponds
 *	to it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

char *
Ck_NameOfJustify(justify)
    Ck_Justify justify;		/* Justification style for which
				 * identifying string is desired. */
{
    switch (justify) {
	case CK_JUSTIFY_LEFT: return "left";
	case CK_JUSTIFY_RIGHT: return "right";
	case CK_JUSTIFY_CENTER: return "center";
	case CK_JUSTIFY_FILL: return "fill";
    }
    return "unknown justification style";
}

/*
 *--------------------------------------------------------------
 *
 * Ck_GetCoord --
 *
 *	Given a string, return the coordinate that corresponds
 *	to it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
Ck_GetCoord(interp, winPtr, string, intPtr)
    Tcl_Interp *interp;        /* Use this for error reporting. */
    CkWindow *winPtr;          /* Window (not used). */
    char *string;              /* String to convert. */
    int *intPtr;               /* Place to store converted result. */
{
    int value;

    if (Tcl_GetInt(interp, string, &value) != TCL_OK)
	return TCL_ERROR;
    if (value < 0) {
        Tcl_AppendResult(interp, "coordinate may not be negative",
	    (char *) NULL);
        return TCL_ERROR;
    }
    *intPtr = value;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Ck_ColorCmd --
 *
 *	This procedure is invoked to process the "color" command.
 *	This command allows to query / update the color database
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *      Definition of colors can be changed by this command
 *
 *----------------------------------------------------------------------
 */

int
Ck_ColorCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
  char buffer[64];
  int res = TCL_OK;
  char c;
  int len;
  int i;

  if ( argc < 2 ) {
    Tcl_AppendResult( interp, "wrong # args: should be \"",
		      argv[0], " subcommand ?arg? ...\"", (char *) NULL);
    return TCL_ERROR;
  }
  
  len = strlen(argv[1]);
  c = argv[1][0];
  
  switch(c) {
  case 'c':
    if (strncmp(argv[1], "cells", len) == 0) {
      int maxcell = COLORS;
      if ( maxcell > 256 ) {
	maxcell = 256;
      }
      
      if ( argc == 2 ) {
	/* returns a list of all color cells */
	Tcl_AppendResult( interp, "{ ", NULL );
	for ( i = 0; i < maxcell; ++i ) {
	  struct termcolor_t *cellPtr = termcolors + i;
	  sprintf( buffer, "{ red %3d green %3d blue %3d }",
		   cellPtr->red, cellPtr->green, cellPtr->blue );
	  Tcl_AppendResult( interp, buffer, NULL );
	}
	Tcl_AppendResult( interp, " }", NULL );
	return TCL_OK;
      }
      else if (argc == 3) {
	struct termcolor_t *cellPtr;
	if ( TCL_OK != Tcl_GetInt( interp, argv[2], &i)) {
	  return TCL_ERROR;
	}
	if ( (i < 0) || (i >= maxcell) ) {
	  sprintf(buffer,"value out of range (expected between 0 and %6d)",
		  maxcell-1);
	  Tcl_AppendResult( interp, buffer, NULL );
	  return TCL_ERROR;
	}
	cellPtr = termcolors + i;
	sprintf( buffer, "{ red %3d green %3d blue %3d }",
		 cellPtr->red, cellPtr->green, cellPtr->blue );
	Tcl_AppendResult( interp, buffer, NULL );
	return TCL_OK;
      }
      else if (argc == 4) {
	int largc, icell, j, idx, rgb[3] = { -1, -1, -1 };
	const char **largv;
	if ( TCL_OK != Tcl_GetInt( interp, argv[2], &icell)) {
	  return TCL_ERROR;
	}
	if ( (i < 0) || (i >= maxcell) ) {
	  sprintf(buffer,"value out of range (expected between 0 and %6d)",
		  maxcell-1);
	  Tcl_AppendResult( interp, buffer, NULL );
	  return TCL_ERROR;
	}
	if ( TCL_OK != TCL_SplitList( interp, argv[3], &largc, &largv) ) {
	  return TCL_ERROR;
	}
	if ( largc != 6 ) {
	  return TCL_ERROR;
	}
	for( j = 0; j < 6; j += 2 ) {
	  idx = -1;
	  if (!strcmp( largv[j], "red")) idx = 0;
	  if (!strcmp( largv[j], "green")) idx = 1;
	  if (!strcmp( largv[j], "blue")) idx = 2;

	  if ( idx == -1 ) {
	    Tcl_AppendResult( interp, (char*) NULL);
	    res = TCL_ERROR;
	    break;
	  }
	  if ( rgb[idx] != -1 ) {
	    Tcl_AppendResult( interp, (char*) NULL);
	    res = TCL_ERROR;
	    break;
	  }
	  if ( TCL_OK != Tcl_GetInt( interp, largv[j+1], &rgb[idx]) ) {
	    res = TCL_ERROR;
	    break;
	  }
	  if ( rgb[idx] < 0 || rgb[idx] > 255 ) {
	    sprintf(buffer,"color component %d out of range (0-255)", rgb[idx]);
	    Tcl_AppendResult( interp, buffer, (char*) NULL);
	    return TCL_ERROR;
	  }
	}
	if ( res == TCL_OK ) {
	  setCell( i, rgb[0], rgb[1], rgb[2] );
	  findBestCells();
	}
	Tcl_Free(largv);
	return res;
      failed:
      }
      else {
	return TCL_ERROR;
      }
    }
    else {
      goto error;
    }
    
  case 'i':
    if (strncmp(argv[1], "info", len) == 0) {
      if (argc == 2) {
	int maxnames = 0;
	int maxcell = COLORS;
	if ( maxcell > 256 ) {
	  maxcell = 256;
	}
	sprintf( buffer, "{ threshold %6d ", color_threshold );
	Tcl_AppendResult( interp, buffer, (char*) NULL);
	sprintf( buffer, "cells %6d ", maxcell );
	Tcl_AppendResult( interp, buffer, (char*) NULL);	  
	sprintf( buffer, "names %6d }", maxnames );
	Tcl_AppendResult( interp, buffer, (char*) NULL);	  
      }
      else if (argc == 3) {
	if ( !strcmp(argv[2], "threshold")) {
	  sprintf( buffer, "{ threshold %6d }", color_threshold );
	  Tcl_AppendResult( interp, buffer, (char*) NULL);
	}
	else if ( !strcmp(argv[2], "cells")) {
	  int maxcell = COLORS;
	  if ( maxcell > 256 ) {
	    maxcell = 256;
	  }
	  sprintf( buffer, "{ cells %6d }", maxcell );
	  Tcl_AppendResult( interp, buffer, (char*) NULL);	  
	}
	else if ( !strcmp(argv[2], "names")) {
	  int maxnames = 0;
	  sprintf( buffer, "{ names %6d }", maxnames );
	  Tcl_AppendResult( interp, buffer, (char*) NULL);	  
	}
	else {
	  Tcl_AppendResult( interp, "invalid option \"", argv[2],
			    "\" expecting one of \"cells\", \"names\" ",
			    "or	\"threshold\"", (char*) NULL);
	  return TCL_ERROR;
	}
      }
      else {
	Tcl_AppendResult( interp, "wrong # args: should be \"",
	   argv[0], " info ?option?\"", (char *) NULL);
	return TCL_ERROR;
      }
    } else {
      goto error;
    }
    
  case 'n':
    if (strncmp(argv[1], "names", len) == 0) {
      if ( argc == 2 ) {
      }
      else if ( argc == 3 ) {
      }
    } else {
      goto error;
    }

  case 'r':
    if (strncmp(argv[1], "reset", len) == 0) {
      if ( argc == 2 ) {
	/* reinit the curses COLORS to their ANSI default values */
	/* @todo */
      }
      else {
	/* Bad number of arguments */
	Tcl_AppendResult( interp, "wrong # args: should be \"",
	     argv[0], " reset\"", (char *) NULL);
	return TCL_ERROR;
      }
    else {
      goto error;
    }

  case 't':
    if (strncmp(argv[1], "threshold", len) == 0) {
      if ( argc == 2 ) {
	/* returns the color threshold which is used
	 * to associate X11 colors to the 'closest' curses match */
	Tcl_SetObjResult( interp, Tcl_NewIntObj(color_threshold));
      }
      else if ( argc == 3 ) {
	int value;
	if (TCL_OK != Tcl_GetInt( interp, argv[2], &value)) {
	  return TCL_ERROR;
	}
	if ((value < 0) && (value > 100)) {
	  Tcl_AppendResult( interp, "value out of range (must be in interval 0-100)");
	  return TCL_ERROR;
	}
	color_threshold = value;
      }
      else {
	/* Bad number of arguments */
	Tcl_AppendResult( interp, "wrong # args: should be \"",
	     argv[0], " threshold ?value?\"", (char *) NULL);
	return TCL_ERROR;
      }
    }
    else {
      goto error;
    }

  default:
  error:
    Tcl_AppendResult( interp, "unknown subcommand \"",  argv[1],"\" :",
		      "must be one of \"info\", \"cell\", \"cells\", \"name\", ",
		      "\"names\" or \"threshold\"",
		      (char *) NULL);
    return TCL_ERROR;
  }
  
  return res;
}
