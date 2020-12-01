# puzzle.tcl --
#
# This demonstration script creates a 15-puzzle game using a collection
# of buttons.

set border {ulcorner hline urcorner vline lrcorner hline llcorner vline}

# puzzleSwitch --
# This procedure is invoked when the user clicks on a particular button;
# if the button is next to the empty space, it moves the button into th
# empty space.

proc puzzleSwitch {w num} {
    global xpos ypos
    if {(($ypos($num) >= $ypos(space))
	 && ($ypos($num) <= $ypos(space))
	 && ($xpos($num) >= ($xpos(space) - 1))
	 && ($xpos($num) <= ($xpos(space) + 1)))
	|| (($xpos($num) >= $xpos(space))
	    && ($xpos($num) <= $xpos(space))
	    && ($ypos($num) >= ($ypos(space) - 1))
	    && ($ypos($num) <= ($ypos(space) + 1)))} {
	set tmp $xpos(space)
	set xpos(space) $xpos($num)
	set xpos($num) $tmp
	set tmp $ypos(space)
	set ypos(space) $ypos($num)
	set ypos($num) $tmp
	grid .frame.$num -column $xpos($num) -row $ypos($num) -sticky nsew
    }
}


message .msg -width 80 -text "A 15-puzzle appears below as a collection of buttons.  Click on any of the pieces next to the space, and that piece will slide over the space.  Continue this until the pieces are arranged in numerical order from upper-left to lower-right. Press <Q> to quit."
pack .msg -side top -expand yes -fill x

frame .frame -width 20 -height 20
pack .frame -side top -fill both -expand yes

bind . <Key-q> exit

after idle {
    grid columnconfigure .frame 0 -weight 0.25
    grid columnconfigure .frame 1 -weight 0.25
    grid columnconfigure .frame 2 -weight 0.25
    grid columnconfigure .frame 3 -weight 0.25

    grid rowconfigure .frame 0 -weight 0.25
    grid rowconfigure .frame 1 -weight 0.25
    grid rowconfigure .frame 2 -weight 0.25
    grid rowconfigure .frame 3 -weight 0.25

    set order {3 1 6 2 5 7 15 13 4 11 8 9 14 10 12}

    for {set i 0} {$i < 15} {incr i} {
	set num [lindex $order $i]
	set xpos($num) [expr {($i%4)}]
	set ypos($num) [expr {($i/4)}]
    
	frame .frame.$num -border $border
	grid .frame.$num -column $xpos($num) -row $ypos($num) -sticky nsew
	
	button .frame.${num}.b -text "$num" -command "puzzleSwitch .frame $num"
	pack .frame.$num.b -fill both -expand yes
    }
    set xpos(space) 3
    set ypos(space) 3
}

vwait forever

