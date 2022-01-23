# --------------------------------------------------------------------------
#  Terminal multiplexer program
#
#  Demonstrates the terminal widget
# --------------------------------------------------------------------------

set TX  0
set CTX 0

# --------------------------------------------------------------------------
#  -- Terminals --
#
#  Terminals are kept in a list.
#
#  A terminal is described by :
#  {
#     name  string    <-- the name of the terminal
#     title string    <-- the title of the terminal as retreived from OCS
#                         escape sequence
#     idx integer     <-- terminal index
#     wid widget      <-- widget identifier
#     layout interger <-- index in layout
#  }
# --------------------------------------------------------------------------
set TERM [list]

# --------------------------------------------------------------------------
#  -- Screen Layouts --
#
#  There are 8 possible layouts, there are described by
#     Layout 1      Layout 2      Layout 3      Layout 4
#   -----------   -----------   -----------   -----------
#   |         |   |         |   |    |    |   |    |    |
#   |         |   |    1    |   |    |    |   |    | 2  |
#   |    1    |   |---------|   | 1  | 2  |   |  1 |----|
#   |         |   |         |   |    |    |   |    |    |
#   |         |   |    2    |   |    |    |   |    | 3  |
#   -----------   -----------   -----------   -----------
#
#     Layout 5      Layout 6      Layout 7      Layout 8
#   -----------   -----------   -----------   -----------
#   |    |    |   |         |   |    |    |   |    |    |
#   | 1  |    |   |    1    |   |  1 | 2  |   |  1 | 2  |
#   |----| 3  |   |---------|   |---------|   |---------|
#   |    |    |   |    |    |   |         |   |    |    |
#   | 2  |    |   |  2 | 3  |   |    3    |   |  3 | 4  |
#   -----------   -----------   -----------   -----------
#
#  A layout is described by a dictionnary
#  {
#     layout 1..8    <-- layout identifier
#     t1 .widget     <-- widget in slot 1 of layout (pane 1)
#     t2 .widget     <-- widget in slot 2 of layout (pane 2)
#     t3 .widget     <-- widget in slot 3 of layout (pane 3)
#     t4 .widget     <-- widget in slot 4 of layout (pane 4)
#     focus widget   <-- currently focused widget
#     w relwidth     <-- between 0.0 and 1.0, distance form left
#                        to vertical split
#     h relheight    <-- between 0.0 and 1.0, distance form top
#                        to vertical split.
#  }
#
#  Depending on the layout some fields are not relevant.
#  For example for layout 1, fields t2 to t4, h and w are not used.
#  For layout 6, field t4 is not used.
#
# --------------------------------------------------------------------------
set LAYOUT [dict create]

# --------------------------------------------------------------------------
#  -- Terminal chooser widgets --
#
#  The chooser variable holds a list of available widgets that
#  can be used to help the user to display a terminal in an empty pane.
#
#  Using a 'chooser' widget, the user can decide :
#    - to create a new terminal
#    - to use an already created terminal, picking it in a list
#
#  Once the user had chosen the terminal to display (new or old)
#  the widget disappears and get replaced by the terminal in the pane.
#
#  There are a maximum of 4 chooser widgets because the layouts
#  will display at most 4 panes.
# --------------------------------------------------------------------------
set CHOOSER [list]

# --------------------------------------------------------------------------
#  Helper
# --------------------------------------------------------------------------
proc report args {
    return -code return
}


# --------------------------------------------------------------------------
#  set-widget
#
#  Display widget 'w2' in pane where 'w1' is displayed.
#  This function is used when the user choose a terminal to display
#
#  @todo: check if w2 was already displayed elsewhere
# --------------------------------------------------------------------------
proc set-widget {w1 w2} {
    # -- check if there is something to do
    if { $w1 eq $w2 } return
    
    global LAYOUT
    dict with LAYOUT {
	for { set i 1 } { $i <= 4 } { incr i } {
	    set slave [set t${i}]
	    if { $slave eq ${w1} } {
		set tw1 t${i}
		break
	    }
	}
	if {$i == 4} return
	
	# -- check if w2 is displayed
	# -- in this case replace it by a chooser
	for { set i 1 } { $i <= 4 } { incr i } {
	    set slave [set t${i}]
	    if { $slave eq ${w2} } {
		set t${i} [choose-term]
		break
	    }	    
	}
	set $tw1    $w2
	set focused $w2
    }
    apply-layout
}

# --------------------------------------------------------------------------
#  display-term
#
#  Display terminal 't' (a new terminal might be created) in pane
#  occupied by by 'w'.
#
#  @todo: add a control on the number of already created terminals
# --------------------------------------------------------------------------
proc display-term {w {t "new-term"}} {
    catch { set t [eval $t] }
    set-widget $w $t
}

# --------------------------------------------------------------------------
#  display-selected-term
#
#  Display the terminal which was selected in listbox
# --------------------------------------------------------------------------
proc display-selected-term { w } {
    global TERM
    set idx  [$w index active]
    set wp   [winfo parent $w]
    set term [lindex $TERM $idx]
    set term [dict get $term "wid"]
    display-term $wp $term
}

# --------------------------------------------------------------------------
#  list-term
#
#  Computes the list of terminals and displays it in supplied widget.
#  This list is presented to the user to allow him to choose the terminal
#  to display in a pane.
# --------------------------------------------------------------------------
proc list-term { widget } {
    global TERM

    set w ${widget}.termlist

    if { ![winfo exists ${w}] } {
	listbox ${w}
	bind ${w} <Return> "display-selected-term %W"
	bind ${w} <1>      "display-selected-term %W"

	pack  ${w}
    }
    catch { ${w} delete 0 end }
    
    foreach term $TERM {
	dict with term {
	    ${w} insert end "Term # ${idx} : ${name}"
	}
    }
    #focus ${w}
}

# --------------------------------------------------------------------------
#  choose-term
#
#  This function will return a widget that allows to create
#  a new terminal or to choose one in the list of currently
#  opened terminals.
# --------------------------------------------------------------------------
proc choose-term {{create 0}} {
    global CHOOSER
    global TERM
    global MXTERM
    
    # -- if not asked to create a new chooser
    # -- try to reuse an unmapped one
    if {$create == 0} {
	foreach w $CHOOSER {
	    if {![winfo ismapped $w]} {
		return $w
	    }
	}
    }
    # -- need to create a new one
    set len [llength $CHOOSER]
    if { $len == 4} {
	error "Too many terminal chooser widgets"
    }

    set f [frame .f.c${len}]
    message ${f}.msg -width 60 -text "\nCreate a new terminal or choose an existing one.\n"
    button ${f}.bnew -text "New Term"

    ${f}.bnew    configure -command "display-term $f"

    pack ${f}.msg ${f}.bnew
    list-term $f
    
    lappend CHOOSER ${f}
    return ${f}
    
#    set w [button .f.b${len} -text "New Term"]
#    $w configure -command "display-term $w"
#    lappend CHOOSER $w
#    return $w
}

# --------------------------------------------------------------------------
#  get-focused-idx
#  Retrieve index of focused window
# --------------------------------------------------------------------------
proc get-focused-idx {} {
    global LAYOUT
    dict with LAYOUT {
	lappend lst $t1 $t2 $t3 $t4
	set focusid [expr {1+[lsearch $lst $focused]}]
	if { $focusid == 0 } {
	    error "unable to find focused window"
	}
	switch -exact -- $layout {
	    1 {
		if { $focusid > 1 } {
		    error "invalid focusid $focusid for layout $layout"
		}
	    }
	    2 - 3 {
		if { $focusid > 2 } {
		    error "invalid focusid $focusid for layout $layout"
		}
	    }
	    4 - 5 - 6 - 7 {
		if { $focusid > 3 } {
		    error "invalid focusid $focusid for layout $layout"
		}
	    }
	}
	
	return $focusid
    }
}

# --------------------------------------------------------------------------
#  swap-vars
#  Swap content on variable given by their names
# --------------------------------------------------------------------------
proc swap-vars {v1 v2} {
    upvar $v1 n1
    upvar $v2 n2
    set t $n1
    set n1 $n2
    set n2 $t
}

# --------------------------------------------------------------------------
#  apply-layout --
#  Apply directives in layout
# --------------------------------------------------------------------------
proc apply-layout {} {
    global LAYOUT
    dict with LAYOUT {
	# -- remove widgets
	foreach slave [place slaves .f] {
	    catch {place forget $slave}
	}
	# -- compute sizes
	if { $layout == 1 } {
	    set w 1.0
	    set h 1.0
	}
	if { $layout == 2 } {
	    set w 1.0
	}
	if { $layout == 3 } {
	    set h 1.0
	}
	# -- place widgets
	set 1h [expr {1-$h}]
	set 1w [expr {1-$w}]
	switch -exact -- $layout {
	    1 - 2 - 3 - 5 - 7 - 8   {
		place $t1 -relx 0.0 -rely 0.0 -relwidth $w  -relheight $h
	    }
	    4 {
		place $t1 -relx 0.0 -rely 0.0 -relwidth $w  -relheight 1.0
	    }
	    6 {
		place $t1 -relx 0.0 -rely 0.0 -relwidth 1.0  -relheight $h
	    }
	}
	if { $layout > 1 } {
	    switch -exact -- $layout {
		2 - 5 - 6 {
		    place $t2 -relx 0.0  -rely $h -relwidth $w -relheight $1h
		}
		3 - 4 - 7 - 8 {
		    place $t2 -relx $w  -rely 0.0 -relwidth $1w -relheight $h
		}
	    }
	}
	if { $layout > 3 } {
	    switch -exact -- $layout {
		4 - 6 {
		    place $t3 -relx $w -rely $h  -relwidth $1w  -relheight $1h
		}
		5 {
		    place $t3 -relx $w -rely 0.0  -relwidth $1w  -relheight 1.0
		}
		7 {
		    place $t3 -relx 0.0 -rely $h  -relwidth 1.0  -relheight $1h
		}
		8 {
		    place $t3 -relx 0.0 -rely $h  -relwidth $w  -relheight $1h
		}
	    }
	}
	if { $layout == 8 } {
	    place $t4 -relx $w  -rely $h  -relwidth $1w  -relheight $1h
	}
	focus $focused
	.l.state configure -text "layout #$layout $w $h"
    }
}

# -------------------------------------------------------------------------
#   fullscreen
#   Enlarge focused pane (ie set layout 1)
# -------------------------------------------------------------------------
proc pane-fullscreen {} {
    global LAYOUT
    set focusid [get-focused-idx]
    dict with LAYOUT {
	.l.state configure -text "$LAYOUT"
	set layout 1
	if {$focusid > 1} {
	    swap-vars t1 t${focusid}
	    set focused $t1
	}
    }
    apply-layout
}

# --------------------------------------------------------------------------
#  This proc is invoked to remove focused pane from layout
#
#  Here are the layout transitions :
#  Layout 1 -> choose-term
#  Layout 2 -> Layout 1
#  Layout 3 -> Layout 1
#  Layout 4 -> Layout 2 if focused = 1, layout 3 if focused = 2 or 3
#  Layout 5 -> Layout 2 if focused = 3, layout 3 if focused = 1 or 2
#  Layout 6 -> Layout 3 if focused = 1, layout 2 if focused = 2 or 3
#  Layout 7 -> Layout 3 if focused = 3, layout 2 if focused = 1 or 2
#  Layout 8 -> Layout 6 if focused = 1 or 2, layout 7 if focused = 2 or 3
# --------------------------------------------------------------------------
proc pane-kill {} {
    global LAYOUT
    set focusid [get-focused-idx]
    dict with LAYOUT {
	switch -exact -- $layout {
	    1 {
		set t1 [choose-term]
		set focused $t1
	    }
	    2 - 3 {
		set layout 1
		if { $focusid == 1 } {
		    swap-vars t1 t2
		    set focused $t1
		}
	    }
	    4 {
		if { $focusid == 1 } {
		    set layout 2
		    swap-vars t1 t2
		    swap-vars t2 t3
		    set focused $t1 ;# <-- will focus the widget which was t2
		}
		if { $focusid == 2 } {
		    set layout 3
		    swap-vars t2 t3
		    set focused $t2 ;# <-- will focus the widget which was t3
		}
		if { $focusid == 3 } {
		    set layout 3
		    set focused $t2
		}
	    }
	    5 {
		if { $focusid == 3 } {
		    set layout 2
		    set focused $t1
		}
		if { $focusid == 1 } {
		    set layout 3
		    swap-vars t1 t2
		    swap-vars t2 t3
		    set focused $t1 ;# <-- will focus the widget which was t2
		}
		if { $focusid == 2 } {
		    set layout 3
		    swap-vars t2 t3
		    set focused $t2 ;# <-- will focus the widget which was t3
		}
	    }
	    6 {
		if { $focusid == 1 } {
		    set layout 3
		    swap-vars t1 t2
		    swap-vars t2 t3
		    set focused $t1 ;# <-- will focus the widget which was t2
		}
		if { $focusid == 2 } {
		    set layout 2
		    swap-vars t2 t3
		    set focused $t2 ;# <-- will focus the widget which was t3
		}
		if { $focusid == 3 } {
		    set layout 2
		    set focused $t2
		}
	    }
	    7 {
		if { $focusid == 1 } {
		    set layout 2
		    swap-vars t1 t2
		    set focused $t1 ;# <-- will focus the widget which was t2
		}
		if { $focusid == 2 } {
		    set layout 2
		    swap-vars t2 t3
		    set focused $t2 ;# <-- will focus the widget which was t3
		}
		if { $focusid == 3 } {
		    set layout 3
		    set focused $t1
		}
	    }
	    8 {
		if { $focusid == 1 } {
		    set layout 6
		    swap-vars t1 t2
		    swap-vars t2 t3
		    swap-vars t3 t4
		    set focused $t1 ;# <-- will focus the widget which was t2
		}
		if { $focusid == 2 } {
		    set layout 6
		    swap-vars t2 t3
		    swap-vars t3 t4
		    set focused $t1
		}
		if { $focusid == 3 } {
		    set layout 7
		    swap-vars t3 t4
		    set focused $t3 ;# <-- will focus the widget which was t4
		}
		if { $focusid == 4 } {
		    set layout 7
		    set focused $t3
		}
	    }
	}
    }
    apply-layout
}

# --------------------------------------------------------------------------
#   This proc is invoked to expand vertically
#
#   Here are the layout transitions :
#   Layout 1 -> Nothing done
#   Layout 2 -> Layout 1, focused window becomes 1
#   Layout 3 -> Nothing done
#   Layout 4 -> Layout 3 if focus(2/3), focused window becomes 2
#   Layout 5 -> Layout 3 if focus(1/2), focused window becomes 1
#   Layout 6 -> Layout 1 if focus(1), layout 4 if f = 2, layout 5 if 3
#   Layout 7 -> Layout 1 if focus(3), layout 3 if focused = 2 or 3
#   Layout 8 -> Layout 4 if focus(1/3), layout 5 if focused = 2 or 4
# --------------------------------------------------------------------------
proc pane-vexpand {} {
    global LAYOUT
    set focusid [get-focused-idx]
    dict with LAYOUT {
	switch -exact -- $layout {
	    2 {
		set layout 1
		if { $focusid == 2 } {
		    swap-vars t1 t2
		}
		set focused $t1
	    }
	    4 {
		if { $focusid == 2 } {
		    set layout 3
		}
		if { $focusid == 3 } {
		    set layout 3
		    swap-vars t2 t3
		    set focused $t2 ;# <-- will focus the widget which was t3
		}
	    }
	    5 {
		if { $focusid == 1 } {
		    set layout 3
		    swap-vars t2 t3
		}
		if { $focusid == 2 } {
		    set layout 3
		    swap-vars t1 t2
		    swap-vars t2 t3
		    set focused $t1 ;# <-- will focus the widget which was t2
		}
	    }
	    6 {
		if { $focusid == 1 } {
		    set layout 1
		}
		if { $focusid == 2 } {
		    set layout 4
		    swap-vars t1 t2
		    set focused $t1 ;# <-- will focus the widget which was t2
		}
		if { $focusid == 3 } {
		    set layout 5
		}
	    }
	    7 {
		if { $focusid == 1 } {
		    set layout 4
		}
		if { $focusid == 2 } {
		    set layout 5
		    swap-vars t2 t3
		    set focused $t3 ;# <-- will focus the widget which was t2
		}
		if { $focusid == 3 } {
		    set layout 1
		    swap-vars t1 t3
		    set focused $t1 ;# <-- will focus the widget which was t3
		}
	    }
	    8 {
		if { $focusid == 1 } {
		    set layout 4
		    swap-vars t3 t4
		}
		if { $focusid == 2 } {
		    set layout 5
		    swap-vars t2 t3
		    set focused $t3 ;# <-- will focus the widget which was t2
		}
		if { $focusid == 3 } {
		    set layout 4
		    swap-vars t1 t3
		    swap-vars t3 t4
		    set focused $t1 ;# <-- will focus the widget which was t3
		}
		if { $focusid == 4 } {
		    set layout 5
		    swap-vars t2 t3
		    swap-vars t3 t4
		    set focused $t3 ;# <-- will focus the widget which was t4		    
		}
	    }
	}
    }
    apply-layout
}

# --------------------------------------------------------------------------
#   This proc is invoked to expand horizontally
#
#   Here are the layout transitions :
#   Layout 1 -> Nothing done
#   Layout 2 -> Nothing done
#   Layout 3 -> Layout 1
#   Layout 4 -> Layout 1 if focused = 1, layout 2 if focused = 2 or 3
#   Layout 5 -> Layout 1 if focused = 3, layout 2 if focused = 1 or 2
#   Layout 6 -> Layout 2 if focused = 2 or 3
#   Layout 7 -> Layout 2 if focused = 1 or 2
#   Layout 8 -> Layout 6 if focused = 1 or 2, layout 7 if focused = 3 or 4
# --------------------------------------------------------------------------
proc pane-hexpand {} {
    global LAYOUT
    set focusid [get-focused-idx]
    dict with LAYOUT {
	switch -exact -- $layout {
	    3 {
		set layout 1
		if { $focusid == 2 } {
		    swap-vars t1 t2
		}
		set focused $t1
	    }
	    4 {
		if { $focusid == 1 } {
		    set layout 1
		}
		if { $focusid == 2 } {
		    set layout 6
		    swap-vars t1 t2
		    set focused $t1
		}
		if { $focusid == 3 } {
		    set layout 7
		}
	    }
	    5 {
		if { $focusid == 1 } {
		    set layout 6
		}
		if { $focusid == 2 } {
		    set layout 7
		    swap-vars t2 t3
		    set focused $t3
		}
		if { $focusid == 3 } {
		    set layout 1
		    swap-vars t1 t3
		    set focused $t1
		}
	    }
	    6 {
		if { $focusid == 2 } {
		    set layout 2
		}
		if { $focusid == 3 } {
		    set layout 2
		    swap-vars t2 t3
		    set focused $t2
		}
	    }
	    7 {
		if { $focusid == 1 } {
		    set layout 2
		    swap-vars t2 t3
		}
		if { $focusid == 2 } {
		    set layout 2
		    swap-vars t1 t2
		    swap-vars t2 t3
		    set focused $t1
		}
	    }
	    8 {
		if { $focusid == 1 } {
		    set layout 6
		    swap-vars t2 t3
		    swap-vars t3 t4
		}
		if { $focusid == 2 } {
		    set layout 6
		    swap-vars t1 t2
		    swap-vars t2 t3
		    swap-vars t3 t4
		    set focused $t1
		}
		if { $focusid == 3 } {
		    set layout 7
		}
		if { $focusid == 4 } {
		    set layout 7
		    swap-vars t3 t4
		    set focused $t3
		}
	    }
	}
    }
    apply-layout
}

# --------------------------------------------------------------------------
#   This proc is invoked to split vertically
#
#   Here are the layout transitions :
#   Layout 1 -> Layout 2
#   Layout 2 -> Nothing done
#   Layout 3 -> Layout 5 if pane 1 focused, layout 4 if pane 2 focused
#   Layout 4 -> Layout 8 if pane 1 focused, nothing done otherwise
#   Layout 5 -> Layout 8 if pane 3 focused, nothing done otherwise
#   Layout 6,7,8 -> Nothing done
# --------------------------------------------------------------------------
proc pane-vsplit {} {
    global LAYOUT
    set focusid [get-focused-idx]
    dict with LAYOUT {
	switch -exact -- $layout {
	    1 {
		set layout 2
		set h 0.5
	    }
	    3 {
		if { $focusid == 1 } {
		    set layout 5
		    set h 0.5
		    swap-vars t2 t3
		}
		if { $focusid == 2 } {
		    set layout 4
		    set h 0.5
		}
	    }
	    4 {
		if { $focusid == 1 } {
		    set layout 8
		    swap-vars t3 t4
		}
	    }
	    5 {
		if { $focusid == 3 } {
		    set layout 8
		}
	    }
	}
    }
    apply-layout
}

# --------------------------------------------------------------------------
#   This proc is invoked to split horizontally
#
#   Here are the layout transitions :
#   Layout 1 -> Layout 3
#   Layout 2 -> Layout 7 if pane 1 focused, Layout 6 if pane 2 focused.
#   Layout 3,4,5 -> Nothing done
#   Layout 6 -> Layout 8 if pane 1 focused, nothing done otherwise
#   Layout 7 -> Layout 8 if pane 3 focused, nothing done otherwise
#   Layout 8 -> Nothing done
# --------------------------------------------------------------------------
proc pane-hsplit {} {
    global LAYOUT
    set focusid [get-focused-idx]
    dict with LAYOUT {
	switch -exact -- $layout {
	    1 {
		set layout 3
		set w 0.5
	    }
	    2 {
		if { $focusid == 1 } {
		    set layout 7
		    set w 0.5
		    swap-vars t2 t3
		}
		if { $focusid == 2 } {
		    set layout 6
		    set w 0.5
		}
	    }
	    6 {
		if {$focusid == 1} {
		    set layout 8
		    swap-vars t3 t4
		    swap-vars t2 t3
		}
	    }
	    7 {
		if {$focusid == 3} {
		    set layout 8
		}
	    }
	}
    }
    apply-layout
}



# --------------------------------------------------------------------------
#  Banner that will get displayed in each terminal opened
#  It gives some infos about control sequences to use
# --------------------------------------------------------------------------
set BANNER [join {
    ""
    "-------------------------------------------------------------------------------"
    ""
    "               *** Welcome to cktermux ***"
    ""
    "   This program demonstrates the terminal widget of ck8.6."
    "   It implements a small terminal multiplexer program."
    ""
    "   Your current shell is launched in a terminal."
    ""
    "   The control key is set to <Control-%1$s> and starts control sequences."
    "     * <Control-%1$s>-o        : gives focus to control buttons"
    "     * <Control-%1$s>-<PageUp> : Starts scrolling in terminal history."
    ""    
    "-------------------------------------------------------------------------------"
    ""
    ""
} "\r\n"]

proc next-term { t } {
    global LAYOUT
    dict with LAYOUT {
    }
}

proc prev-term { t } {
    global LAYOUT
    dict with LAYOUT {
    }
}

proc mk-button { name text command {pack left} } {
    button .l.${name} -text " ${text} " -command $command
    bind .l.${name} <Left>  {focus [ck_focusPrev %W]}
    bind .l.${name} <Right> {focus [ck_focusNext %W]}
    pack .l.${name} -side ${pack}
}

proc set-term { t } {
    global TX
    for { set i 1 } { $i <= $TX } { incr i } {
	if { [winfo exists .t${i}] } {
	    lappend lst .t${i}
	    .l.t${i} configure -foreground white
	}
    }
    pack forget {*}$lst
    pack $t -side left -anchor w -expand yes -fill both
    .l$t configure -foreground red
    focus $t
}

proc new-term { {ckey "b"} } {
    global TX BANNER
    incr TX
    terminal .t${TX} -exec /bin/bash -term xterm-256color \
	-commandkey ${ckey} -banner [format ${BANNER} ${ckey}] 
#	-border {ulcorner hline urcorner vline lrcorner hline llcorner vline}

    bind .t${TX} <Button-1> {focus %W}

    # -- binding to virtual events
    bind .t${TX} <<Close>> {close-term %W}
    bind .t${TX} <<New>>   new-term
    #bind .t${TX} <Control-b><Control-g> exit
    
    mk-button t${TX} "Term #${TX}" "set-term .t${TX}"
    bind .l.t${TX} <Control-q> "close-term .t${TX}"
    bind .l.t${TX} <Control-o> "new-term"

    bindtags .t${TX} .t${TX}

    return .t${TX}
}

proc close-term { t } {
    global TX
    destroy $t
    destroy .l$t
    for { set i 1 } { $i <= $TX } { incr i } {
	if { [winfo exists .t${i}] } {
	    set-term .t${i}
	}
    }
}

# --------------------------------------------------------------------------
#  Help
#  Display help box
# --------------------------------------------------------------------------
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
 
    $t insert end "Terminal multiplexer\n" title
    $t insert end "by vzvca\n\n" title2
 
    $t insert end "This is crude terminal multiplexer.\n\n"
    
    $t insert end "Type <Control-b><Key-o> to access to control buttons. "
    
    $t insert end "Keys on terminal buttons\n" header
    $t insert end "o <Control-q> closes the terminal.\n" bullet
    $t insert end "o <Control-n> opens a new terminal.\n" bullet
    $t insert end "o Use <Left> and <Right> to navigate between buttons.\n" bullet
    $t insert end "\n\n"

    $t insert end "Mouse\n" header
    $t insert end "o Left button clicked in terminal will transfer focus to it.\n" bullet
    
    $t config -state disabled
}

label .l -text "Terminal multiplexer" -bg blue
pack .l -side top -fill x
frame .f
pack .f -side top -fill both -expand yes

#new-term
#new-term
#new-term
#new-term

proc new-term { {ckey "b"} } {
    global TX BANNER TERM CHOOSER
    
    incr TX
    array set colors {0 blue 1 red 2 green 3 cyan 4 magenta 5 yellow}
    set col $colors([expr {$TX%6}])
    message .f.t${TX} -text "Frame #${TX}" -bg $col
    bind .f.t${TX} <1> {dict set LAYOUT focused %W}

    # -- compute terminal description
    set desc [dict create]
    dict set desc "name"   "unamed"
    dict set desc "title"  ""
    dict set desc "idx"    $TX
    dict set desc "wid"    .f.t${TX}
    dict set desc "layout" 0
    lappend TERM $desc

    # -- update terminal chooser content
    foreach chooser $CHOOSER {
	list-term $chooser
    }
    
    return .f.t${TX}
}


set LAYOUT [dict create]
dict set LAYOUT layout 4
dict set LAYOUT t1 [choose-term 1]
dict set LAYOUT t2 [choose-term 1]
dict set LAYOUT t3 [choose-term 1]
dict set LAYOUT t4 [choose-term 1]
dict set LAYOUT w 0.7
dict set LAYOUT h 0.4
dict set LAYOUT focused [dict get $LAYOUT t1]

#set-term .t1

mk-button q "Quit" exit right
mk-button h "Help" help right

mk-button k  "kill" pane-kill right
mk-button ve "v-expand"    pane-vexpand right
mk-button he "h-expand"    pane-hexpand right
mk-button vs "v-split"     pane-vsplit right
mk-button hs "h-split"     pane-hsplit right
mk-button fs "fullscreen"  pane-fullscreen right

proc noop {} {}
mk-button state "layout" noop

apply-layout
