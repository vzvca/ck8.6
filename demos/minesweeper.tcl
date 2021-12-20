# 
# minesweeper --
#
#	Well known game implemented for Ck demo.
#
# Copyright (c) 2019 VCA
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#

# --------------------------------------------------------------------------
#  -- globals
# --------------------------------------------------------------------------
# -- used to draw gui
set line0 "+---+---+---+---+---+---+---+---+---+---+"
set line1 "${line0}\n"
set line2 "|   |   |   |   |   |   |   |   |   |   |\n"
set cell  "+---+\n|   |\n+---+"
append brd $line1 $line2 $line1 $line2 $line1 $line2 $line1 $line2 $line1 $line2
append brd $line1 $line2 $line1 $line2 $line1 $line2 $line1 $line2 $line1 $line2
append brd $line0

# -- hold current position
set xcell 0
set ycell 0

# -- game status variables
set remaining 100
set elapsed   "00:00"
set start     0
set status    ""
set nbombs    10

# --------------------------------------------------------------------------
#  -- place 'nb' bombs on the grid
# --------------------------------------------------------------------------
proc init-bombs { {nb $::nbombs} } {
    for { set i 0 } { $i < 10 } { incr i } {
	for { set j 0 } { $j < 10 } { incr j } {
	    set ::bombs(${i}${j}) 0
	    set ::flags(${i}${j}) 0
	    set ::xpand(${i}${j}) 0
	}
    }
    for { set ib 0 } { $ib < $nb } { incr ib } {
	set i [expr {int(rand()*9.999999)}]
	set j [expr {int(rand()*9.999999)}]
	# -- do not use the same place twice !
	if { $::bombs(${i}${j}) == 0 } {
	    set ::bombs(${i}${j}) 1
	} else {
	    incr ib -1
	}
    }
}

# --------------------------------------------------------------------------
#   -- init
# --------------------------------------------------------------------------
proc init-board {} {
    for { set i 0 } { $i < 10 } { incr i } {
	for { set j 0 } { $j < 10 } { incr j } {
	    set im1 [expr {$i - 1}]
	    set jm1 [expr {$j - 1}]
	    set ip1 [expr {$i + 1}]
	    set jp1 [expr {$j + 1}]
	    if {($im1 >= 0) && ($jm1 >= 0)} {
		lappend ::neighbours(${i}${j}) [list $im1 $jm1]
	    }
	    if {($im1 >= 0)} {
		lappend ::neighbours(${i}${j}) [list $im1 $j]
	    }
	    if {($im1 >= 0) && ($jp1 <= 9)} {
		lappend ::neighbours(${i}${j}) [list $im1 $jp1]
	    }
	    if {($jm1 >= 0)} {
		lappend ::neighbours(${i}${j}) [list $i $jm1]
	    }
	    if {($jp1 <= 9)} {
		lappend ::neighbours(${i}${j}) [list $i $jp1]
	    }
	    if {($ip1 <= 9) && ($jm1 >= 0)} {
		lappend ::neighbours(${i}${j}) [list $ip1 $jm1]
	    }
	    if {($ip1 <= 9)} {
		lappend ::neighbours(${i}${j}) [list $ip1 $j]
	    }
	    if {($ip1 <= 9) && ($jp1 <= 9)} {
		lappend ::neighbours(${i}${j}) [list $ip1 $jp1]
	    }
	}
    }
}

# --------------------------------------------------------------------------
#   -- Second stage minesweep
# --------------------------------------------------------------------------
proc xpandmore { key } {
    incr ::xpand($key)
    if { $::xpand($key) != 3 } return
    foreach rc $::neighbours($key) {
	lassign $rc i j
	if { $::flags(${i}${j}) > 0 } {
	    continue
	}
	if { $::bombs(${i}${j}) } {
	    return [loose]
	}
    }
    foreach rc $::neighbours($key) {
	lassign $rc r c
	xpand $r $c 1
    }
}

# --------------------------------------------------------------------------
#   -- Mine sweep
# --------------------------------------------------------------------------
proc xpand { {row -1} {col -1} {auto 0} } {
    if { $::start == 0 } { return }
    if { $row == -1 } { set row $::ycell }
    if { $col == -1 } { set col $::xcell }
    set key ${row}${col}

    if { $::xpand($key) > 0 } {
	if { $auto == 0 } { xpandmore $key }
	return
    }
    if { $::flags($key) > 0 } {
	return
    }
    if { $::bombs($key) > 0 } {
	return [loose]
    }
    
    incr ::xpand($key)
    incr ::remaining -1
    
    set nbomb 0
    foreach rc $::neighbours($key) {
	lassign $rc i j
	if { $::bombs(${i}${j}) } {
	    incr nbomb
	}
    }
    if { $nbomb == 0 } {
	.c.l${key} configure -text " "
	foreach rc $::neighbours($key) {
	    lassign $rc r c
	    xpand $r $c 1
	}
    } else {
	array set color {1 blue 2 green 3 yellow 4 red 5 red 6 red 7 red 8 red}
	.c.l${key} configure -text "$nbomb" -fg $color($nbomb)
    }

    check
}

# --------------------------------------------------------------------------
#  -- Sets a flag
# --------------------------------------------------------------------------
proc flag { {row -1} {col -1} } {
    if { $::start == 0 } { return }
    if { $row == -1 } { set row $::ycell }
    if { $col == -1 } { set col $::xcell }
    set key ${row}${col}

    if { $::xpand($key) > 0 } {
	return
    }
    
    set ::flags($key) [expr {1 - $::flags($key)}]
    if { $::flags($key) > 0 } {
	.c.l${key} configure -text "*"
    } else {
	.c.l${key} configure -text "."
    }
}

# --------------------------------------------------------------------------
#  -- Checks if game is finished
# --------------------------------------------------------------------------
proc check {} {
    if { $::remaining <= $::nbombs } {
	# -- stop counter
	set ::start 0
	set ::status "YOU WON !!"
	for { set i 0 } { $i < 10 } { incr i } {
	    for { set j 0 } { $j < 10 } { incr j } {
		if { $::bombs(${i}${j}) > 0 } {
		    	.c.l${i}${j} configure -text "o"
		}
		.c.l${i}${j} configure -bg green
	    }
	}
	.c configure -bg green
	.c.cell configure -bg green
    }
}

# --------------------------------------------------------------------------
#  -- Lost game !
# --------------------------------------------------------------------------
proc loose {} {
    # -- stop counter
    set ::start 0
    set ::status "YOU LOOSE !"
    for { set i 0 } { $i < 10 } { incr i } {
	for { set j 0 } { $j < 10 } { incr j } {
	    set txt "x"
	    if { $::bombs(${i}${j}) > 0 } {set txt "o"}
	    .c.l${i}${j} configure -text $txt -bg red -fg black
	}
	.c configure -bg red
	.c.cell configure -bg red
    }
}

# --------------------------------------------------------------------------
#  -- Draws the active cell
# --------------------------------------------------------------------------
proc drawcell {} {
    place .c.cell -x [expr 4*$::xcell] -y [expr 2*$::ycell] -width 5 -height 3
}

# --------------------------------------------------------------------------
#  -- Initialize GUI
# --------------------------------------------------------------------------
proc init-gui {} {
    frame .f -width [string length $::line0]
    label .f.t -textvariable ::elapsed
    label .f.r -textvariable ::remaining
    pack  .f.t -side left -anchor w
    pack  .f.r -side right -anchor e
    
    message .c -text $::brd -attr dim
    message .c.cell -text $::cell -attr bold

    label .s -textvariable ::status

    pack .f -side top
    pack .c -side top
    pack .s -side top
    
    for { set i 0 } { $i < 10 } { incr i } {
	for { set j 0 } { $j < 10 } { incr j } {
	    label .c.l${i}${j} -text "."
	    place .c.l${i}${j} -x [expr 2+4*$j] -y [expr 1+2*$i] -w 1 -h 1
	    raise .c.l${i}${j}
	}
    }
    
    drawcell
}

# --------------------------------------------------------------------------
#  -- Clean board to replay
# --------------------------------------------------------------------------
proc clean-gui {} {
    for { set i 0 } { $i < 10 } { incr i } {
	for { set j 0 } { $j < 10 } { incr j } {
	    .c.l${i}${j} configure -text "." -fg white -bg black
	}
    }
    .c configure -bg black
    .c.cell configure -bg black
}

# --------------------------------------------------------------------------
#  -- Restart game
# --------------------------------------------------------------------------
proc replay {} {
    clean-gui
    init-bombs $::nbombs
    set ::remaining 100
    set ::elapsed "00:00     "
    set ::start [clock seconds]
    set ::status  ""
}

# --------------------------------------------------------------------------
#  -- Timer
# --------------------------------------------------------------------------
proc timer {} {
    if { $::start > 0 } {
	set t [expr {[clock seconds] - $::start}]
	set ::elapsed [clock format $t -format "%M:%S     "]
    }
    after 1000 timer
}

proc help {} {
    catch {destroy .help}
    toplevel .help -bg black -fg white \
	-border {ulcorner hline urcorner vline lrcorner hline llcorner vline}

    set W [winfo width  .]
    set H [winfo height .]
    incr W -1
    incr H -1
    
    place .help -x 1 -width $W -y 1 -height $H
    raise .help
 
    set t .help.t
    text $t -wrap word -width 70 -height 30 -yscrollcommand {.help.sb set}
    scrollbar .help.sb -orient vertical -command [list $t yview]
    button .help.dismiss -text Close -command {destroy .help}
    pack .help.dismiss -side bottom -pady 1
    pack .help.sb -side right -fill y
    pack $t -side top -expand 1 -fill both
 
    $t tag config title -justify center -foregr red -attr bold
    $t tag configure title2 -justify center -attr bold
    $t tag configure header -attr bold
    $t tag configure n -lmargin1 10 -lmargin2 10
    $t tag configure bullet -lmargin1 10 -lmargin2 12
 
    $t insert end "Minesweeper\n" title
    $t insert end "by vzvca\n\n" title2
 
    $t insert end "Minesweeper is a simple solitaire game.\n\n"
    
    $t insert end "Each Minesweeper game starts out with a grid of unmarked squares. "
    $t insert end "After clicking one of these squares, some of the squares will disappear, "
    $t insert end "some will remain blank, and some will have numbers on them. "
    $t insert end "It's your job to use the numbers to figure out which of "
    $t insert end "the blank squares have mines and which are safe to click. "
    $t insert end "There are 10 mines hidden when the game begins.\n\n"

    $t insert end "A number on a square refers to the number of mines that are currently "
    $t insert end "touching that square. For example, if there are two squares touching "
    $t insert end "each other and one of the squares has '1' on it, "
    $t insert end "you know that the square next to it has a mine beneath it.\n\n"
    
    $t insert end "Keys\n" header
    $t insert end "o Use arrows to move the cursor on the grid.\n" bullet
    $t insert end "o Use 'f' to flag a place as (maybe) containing a bomb.\n" bullet
    $t insert end "o Use 'space' on a square without mine. All the surrounding squares will be revealed. If a mine is header in the square you loose.\n" bullet
    $t insert end "o Use 'r' to replay current game.\n" bullet
    $t insert end "o Use 'q' to quit.\n" bullet
    $t insert end "o Use 'enter' to close this help box.\n" bullet
    $t insert end "\n\n"

    $t insert end "Mouse\n" header
    $t insert end "o Left button will act like 'space' key.\n" bullet
    $t insert end "o Right button will act like 'f' key.\n" bullet
    
    $t config -state disabled
}

# --------------------------------------------------------------------------
#  -- Set up key bindings
# --------------------------------------------------------------------------
bind all <Key-Right> {
    incr xcell
    if { $xcell >= 10 } {
	set xcell 9
    } else {
	drawcell
    }
}

bind all <Key-Left> {
    incr xcell -1
    if { $xcell < 0 } {
	set xcell 0
    } else {
	drawcell
    }
}

bind all <Key-Up> {
    incr ycell -1
    if { $ycell < 0 } {
	set ycell 0
    } else {
	drawcell
    }
}

bind all <Key-Down> {
    incr ycell 1
    if { $ycell >= 10 } {
	set ycell 9
    } else {
	drawcell
    }
}

bind all <Key-space> {
    xpand
}

bind all <Key-f> {
    flag
}

bind all <Key-h> {
    help
}

bind all <Key-r> {
    replay
}

bind all <Key-q> {
    exit
}

bind all <Key-Return> {
    catch {destroy .help}
}

bind all <Button-1> {
    if { [string match ".c.*" %W] } {
	set xcell [expr {(%X - [winfo rootx .c]) / 4}]
	set ycell [expr {(%Y - [winfo rooty .c]) / 2}]
	drawcell
	xpand
    }
}

bind all <Button-3> {
    if { [string match ".c.*" %W] } {
	set xcell [expr {(%X - [winfo rootx .c]) / 4}]
	set ycell [expr {(%Y - [winfo rooty .c]) / 2}]
	drawcell
	flag
    }
}

init-gui
init-board
timer

replay


