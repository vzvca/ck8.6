# --------------------------------------------------------------------------
#  Stolen from the TCL wiki TinyTetris
#  Adaptation to CK by VCA dev 2020
# --------------------------------------------------------------------------

namespace eval tetris {
    variable grid
    variable run
    variable step  0
    variable level 0
    variable score 0
    variable lines 0
    variable piece ""
    variable stepdiv 10
    
    variable blocksizex 2
    variable blocksizey 1
    
    # counter for piece generation
    variable ID 0
    
    variable pieces
    array set pieces {
	0 {cyan      4 0 2 2 {1 1 1 1}}
	1 {yellow    4 0 4 3 {0 1 0 0 1 0 0 1 0 0 1 0}}
	2 {magenta   4 0 3 3 {0 1 0 1 1 0 1 0 0}}
	3 {green     4 0 3 3 {0 1 0 0 1 1 0 0 1}}
	4 {red       4 0 3 3 {0 1 0 1 1 1 0 0 0}}
	5 {blue      4 0 3 3 {0 0 1 1 1 1 0 0 0}}
	6 {white     4 0 3 3 {1 0 0 1 1 1 0 0 0}}
    }
    
    proc build {} {
	variable blocksizex
	variable blocksizey

	#@ wm title . "TinyTetris"
	
	# canvas should be 15x20 blocks
	# the game table is 10x20
	# the right 5 blocks are for the nextpiece
	frame .c -width [expr {$blocksizex*15 + 2}] -height [expr {$blocksizey*20 + 2}]

	label .l1 -text "POINTS"
	label .l2 -anchor n  -textvariable tetris::score
	label .l3 -text "LEVEL"
	label .l4 -anchor n -textvariable tetris::level
	label .l5 -text "LINES"
	label .l6 -anchor n -textvariable tetris::lines

	grid .c   -column 1 -row 0 -rowspan [expr {$blocksizey*20 + 2}]
	grid .l1  -column 0 -row 0 -sticky nw
	grid .l2  -column 0 -row 1 -sticky nw
	grid .l3  -column 0 -row 2 -sticky nw
	grid .l4  -column 0 -row 3 -sticky nw
	grid .l5  -column 0 -row 4 -sticky nw
	grid .l6  -column 0 -row 5 -sticky nw

	bind . <Up>    "tetris::rotateCW"
	bind . <Left>  "tetris::move left"
	bind . <Right> "tetris::move right"
	bind . <space> "tetris::drop"
	bind . <Down>  "tetris::nextStep"
	bind . <F2>    "tetris::restart"
	bind . <Key-q> "exit"

	restart
    }
    
    proc restart {} {
	clearGrid
	variable run 1
	after 1000 tetris::newPiece
    }
    
    proc endGame {} {
	variable run 0
    }
    
    proc clearGrid {} {
	
	variable lines 0
	variable score 0
	variable step  0
	variable level 0
	variable run   1
	variable nextpiece
	
	variable grid
	foreach i {0 1 2 3 4 5 6 7 8 9} {
	    for {set j 0} {$j < 20} {incr j} {
		set id ""
		if { [info exists grid($i,$j)] } {
		    set id $grid($i,$j)
		}
		set grid($i,$j) ""
		if { $id ne "" } {
		    destroy $id
		}
	    }
	}
	set nextpiece [randomPiece]
	cycle
    }
    
    proc cycle {} {
	
	variable level
	variable run
	
	if {!$run} { return }
	nextStep
	
	set speed [expr {100 - 8*$level}]
	if {$speed < 20} { set speed 20 }
	after $speed tetris::cycle
    }
    
    proc newPiece {} {
	variable piece
	variable nextpiece
	variable step 0
	
	variable blocksizex
	variable blocksizey

	set piece $nextpiece
	set nextpiece [randomPiece]
	
	if {[checkPiece] == 0} {
	    endGame
	    return
	}

	# -- draw 'new' current piece
	drawPiece

	# -- draw nextpiece on screen
	lassign $nextpiece color X Y h w piecegrid
	for {set j 0} {$j < $h} {incr j} {
	    for {set i 0} {$i < $w} {incr i} {
		set id [lindex $piecegrid [expr {$i + $j*$w}]]
		if {$id != 0} {
		    set x [expr {12*$blocksizex + $i*$blocksizex}]
		    set y [expr {3*$blocksizey + $j*$blocksizey}]
		    place $id -x $x -y $y -w $blocksizex -h $blocksizey
		}
	    }
	}
    }
    
    proc randomPiece {} {
	variable pieces
	variable ID
	
	set apiece $pieces([expr {int(rand()*7)}])
	lassign $apiece color X Y h w piecegrid
	for {set j 0} {$j < $h} {incr j} {
	    for {set i 0} {$i < $w} {incr i} {
		if {[lindex $piecegrid [expr {$i + $j*$w}]] != 0} {
		    set nid piece[incr ID]
		    frame .c.$nid -bg $color
		    lset apiece 5 [expr {$i + $j*$w}] .c.$nid
		}
	    }
	}
	return $apiece
    }
    
    proc checkPiece {} {
	variable piece
	variable grid
	
	lassign $piece color X Y h w piecegrid
	for {set j 0} {$j < $h} {incr j} {
	    for {set i 0} {$i < $w} {incr i} {
		if {[lindex $piecegrid [expr {$i + $j*$w}]] == 0} { continue }
		set x [expr {$X+$i}]
		set y [expr {$Y+$j}]
		if {$x < 0} { return 0 }
		if {$x > 9} { return 0 }
		if {$grid($x,$y) != ""} { return 0 }
	    }
	}
	return 1
    }
    
    proc drawPiece {} {
	
	variable grid
	variable piece
	variable step
	variable blocksizex
	variable blocksizey
	
	lassign $piece color X Y h w piecegrid
	for {set j 0} {$j < $h} {incr j} {
	    for {set i 0} {$i < $w} {incr i} {
		set id [lindex $piecegrid [expr {$i + $j*$w}]]
		if {$id != 0} {
		    set x [expr {($X+$i)*$blocksizex + 1}]
		    set y [expr {($Y+$j)*$blocksizey}]
		    place $id -x $x -y $y -w $blocksizex -h $blocksizey
		}
	    }
	}
    }
    
    proc rotateCCW {} {
	variable grid
	variable piece
	
	if {$piece == ""} { return }
	set oldpiece $piece
	lassign $piece color X Y h w piecegrid
	set newgrid $piecegrid
	set s [expr {$h-1}]
	for {set j 0} {$j < $h} {incr j} {
	    for {set i 0} {$i < $w} {incr i} {
		set id [lindex $piecegrid [expr {$i + $j*$w}]]
		set p $j
		set q [expr {$s-$i}]
		lset newgrid [expr {$p + $q*$h}] $id
	    }
	}
	set piece [list $color $X $Y $w $h $newgrid]
	if {[checkPiece]} {
	    drawPiece
	} else {
	    set piece $oldpiece
	}
    }
    
    proc rotateCW {} {
	variable grid
	variable piece
	
	if {$piece == ""} { return }
	set oldpiece $piece
	lassign $piece color X Y h w piecegrid
	set newgrid $piecegrid
	set s [expr {$h-1}]
	for {set j 0} {$j < $h} {incr j} {
	    for {set i 0} {$i < $w} {incr i} {
		set id [lindex $piecegrid [expr {$i + $j*$w}]]
		set p [expr {$s-$j}]
		set q $i
		lset newgrid [expr {$p + $q*$h}] $id
	    }
	}
	set piece [list $color $X $Y $w $h $newgrid]
	if {[checkPiece]} {
	    drawPiece
	} else {
	    set piece $oldpiece
	}
    }
    
    proc drop {} {
	variable piece
	while {$piece != ""} { nextStep }
    }
    
    proc move {dir} {
	variable piece
	
	if {$piece == ""} { return }
	foreach {color X Y h w piecegrid} $piece {}
	switch $dir {
	    left { set dx -1 }
	    right { set dx 1 }
	}
	lset piece 1 [expr {$X + $dx}]
	if {[checkPiece] == 0} {
	    lset piece 1 $X
	    return
	}
	drawPiece
    }
    
    proc nextStep {} {
	variable step
	variable piece
	variable grid
	variable stepdiv

	if {$piece == ""} { return }
	lassign $piece color X Y h w piecegrid

	incr step
	set offset [expr {$step%$stepdiv}]
	if {$offset == 1} {
	    lset piece 2 [incr Y]
	    drawPiece
	}
	if {$offset != 0} {
	    return
	}

	# foreach block in piece
	#  if Y+1 is a block, call finish
	#  else incr Y
	for {set i 0} {$i < $w} {incr i} {
	    for {set j [expr {$h-1}]} {$j >= 0} {incr j -1} {
		if {[lindex $piecegrid [expr {$i + $j*$w}]] != 0} {
		    set x [expr {$X+$i}]
		    set y [expr {$Y+$j+1}]
		    if {$y == 20 || $grid($x,$y) != ""} {
			finishPiece
			return
		    }
		    break
		}
	    }
	}
    }
    
    proc finishPiece {} {
	variable grid
	variable piece
	
	variable level
	variable score
	variable lines
	
	lassign $piece color X Y h w piecegrid
	for {set j 0} {$j < $h} {incr j} {
	    for {set i 0} {$i < $w} {incr i} {
		set id [lindex $piecegrid [expr {$i + $j*$w}]]
		if {$id != 0} {
		    set grid([expr {$X+$i}],[expr {$Y+$j}]) $id
		}
	    }
	}

	set nlines 0
	for {set y $Y} {$y < [expr {$Y+$h}]} {incr y} {
	    incr nlines [checkLine $y]
	}
	
	incr score [expr {($level+1)*[lindex {0 40 100 300 1200} $nlines]}]
	incr lines $nlines
	set level [expr {$lines/10}]
	set piece {}
	after 500 "tetris::newPiece"
    }
    
    proc checkLine {y} {
	variable grid
	
	variable blocksizex
	variable blocksizey
	
	if {$y == 20} { return 0 }
	set ids {}
	for {set x 0} {$x < 10} {incr x} {
	    lappend ids [list $x $grid($x,$y)]
	    if {$grid($x,$y) == ""} { return 0 }
	}
	
	foreach e $ids {
	    lassign $e x id
	    place forget $id
	    destroy $id
	    set grid($x,$y) ""
	}
	while {$y > 0} {
	    for {set x 0} {$x < 10} {incr x} {
		set id $grid($x,[expr {$y-1}])
		set grid($x,$y) $id
		if { $id != "" } {
		    set xx [expr {$x*$blocksizex + 1}]
		    set yy [expr {$y*$blocksizey}]
		    place $id -x $xx -y $yy -w $blocksizex -h $blocksizey
		}
	    }
	    incr y -1
	}
	for {set x 0} {$x < 10} {incr x} {
	    set grid($x,0) ""
	}
	
	return 1
    }
}

tetris::build

vwait forever
