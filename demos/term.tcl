# --------------------------------------------------------------------------
#
#  Terminal multiplexer program
#
#  This program was written to demonstrate the use of the terminal
#  widget and demonstrates other Ck features.
#
#  It provides some of the facilities of a terminal multiplexer
#  program like "tmux" or "screen".
#
#  It offers less functions, the main limitation compared to others
#  is that it lacks session de/re-connection which is the `killer'
#  feature of 'tmux' and 'screen'. It is planned to add this !
#
# --------------------------------------------------------------------------

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
#  global terminal counter
# --------------------------------------------------------------------------
set TX 0

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
#  -- Terminal multiplexer commands --
#
#
# --------------------------------------------------------------------------
set COMMANDS {
    {
	name "close"
	desc "Close current pane, others will expand."
	cmd  pane-kill
	shortcut "C-b C-k"
    }
    {
	name "eXchange"
	desc "Change pane content."
	cmd  pane-switch
	shortcut "C-b C-x"
    }
    {
	name "next"
	desc "Focus next pane."
	cmd  pane-next
	shortcut "C-b C-n"
    }
    {
	name "prev"
	desc "Focus previous pane."
	cmd  pane-prev
	shortcut "C-b C-p"
    }
    {
	name "hexpand"
	desc "Expand pane horizontally."
	cmd  pane-hexpand
	shortcut "C-b C-h"
    }
    {
	name "vexpand"
	desc "Expand pane vertically."
	cmd  pane-vexpand
	shortcut "C-b C-v"
    }
    {
	name "hsplit"
	desc "Split pane horizontally."
	cmd  pane-hsplit
	shortcut "C-b exclam"
    }
    {
	name "vsplit"
	desc "Split pane vertically."
	cmd  pane-vsplit
	shortcut "C-b slash"
    }
    {
	name "hdecr"
	desc "Decrease pane width."
	cmd  pane-hdecr
	shortcut "C-b left"
    }
    {
	name "hincr"
	desc "Increase pane width."
	cmd  pane-hincr
	shortcut "C-b right"
    }
    {
	name "vdecr"
	desc "Decrease pane height."
	cmd  pane-vdecr
	shortcut "C-b down"
    }
    {
	name "vincr"
	desc "Increase pane height."
	cmd  pane-vincr
	shortcut "C-b up"
    }
    {
	name "fullscreen"
	desc "Close all panes except focused one."
	cmd  pane-fullscreen
	shortcut "C-b C-f"
    }
    {
	name "help"
	desc "Display help screen."
	cmd  help
	shortcut "C-b C-h"
    }
    {
	name "quit"
	desc "Close terminal multiplexer."
	cmd  exit
	shortcut "C-b C-q"
    }
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

set PREFS [join {
    "#-------------------------------------------------------------------------------"
    "#  tmuck user settings"
    "#-------------------------------------------------------------------------------"
    "# do some clever customisation here ..."
} "\n"]

# --------------------------------------------------------------------------
#  Helper - debug
# --------------------------------------------------------------------------
proc debug msg {
    .l.state configure -text $msg
}

# --------------------------------------------------------------------------
#  Helper - does nothing
# --------------------------------------------------------------------------
proc noop {} {}

# --------------------------------------------------------------------------
#  custom focus command
#  We orverload the default focus command to make sure
#  that the "focused" dictionary field of global variable LAYOUT
#  keeps updated while moving the focus around.
# --------------------------------------------------------------------------
rename focus _focus
proc focus { w } {
    # -- call toolkit implementation
    _focus $w

    # -- update focus 
    set wp $w
    catch {set wp [winfo parent $w]}
    global LAYOUT CHOOSER
    dict with LAYOUT {
	if { ($w eq $focused) || ($wp eq $focused)  } {
	    return
	}
	set n 1
	if {$layout > 1} { incr n }
	if {$layout > 3} { incr n }
	if {$layout > 7} { incr n }
	for {set i 1} {$i <= $n} {incr i} {
	    set t [set t$i]
	    if { $t eq $w } {
		set focused $w
	    }
	    if { $t eq $wp } {
		set focused $wp
	    }
	}
	debug "layout #$layout $w $h $focused"
    }
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
    debug "set-widget $w1 $w2"
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
	if {$i > 4} return
	
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
    debug "display-selected-term $wp $idx"
    if { $idx == 0 } {
	display-term $wp
    } else {
	incr idx -1
	set term [lindex $TERM $idx]
	set term [dict get $term "wid"]
	display-term $wp $term
    }
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
	bind ${w} <1> "%W activate @%x,%y ; display-selected-term %W"

	pack  ${w}
    }
    catch { ${w} delete 0 end }

    ${w} insert end "New terminal"
    foreach term $TERM {
	dict with term {
	    ${w} insert end "Term # ${idx} : ${name}"
	}
    }
    ${w} activate 0
    ${w} selection set 0
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
		# -- when reusing an already created choose
		# -- make sure to reset the active index
		$w.termlist activate 0
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
    message ${f}.msg -width 50 -text "\nCreate a new terminal or choose an existing one.\n"

    pack ${f}.msg
    list-term $f
    
    lappend CHOOSER ${f}
    return ${f}
}

# --------------------------------------------------------------------------
#  command-update-list
#  Updates the list of commands. The list is updating by selecting
#  commands whose name match the content of the entry.
# --------------------------------------------------------------------------
proc command-update-list {} {
    global COMMANDS
    set w .cmd
    set filter [string trim [$w.entry get]]
    debug "filter: $filter"
    
    $w.lst delete 0 end
    foreach command $COMMANDS {
	dict with command {
	    if {[string length $filter]} {
		if {[string first $filter $name] == 0} {
		    $w.lst insert end [format "%-12s%s " $name $desc]
		}
	    } else {
		$w.lst insert end [format "%-12s%s " $name $desc]
	    }
	}
    }
}

# --------------------------------------------------------------------------
#  command-execute
# --------------------------------------------------------------------------
proc command-execute {{what ""}} {
    global COMMANDS
    set w .cmd
    set sz [$w.lst size]

    # -- no argument give, try to find one
    if {![string length $what]} {
	if {$sz == 1} {
	    set what [$w.lst get 0]
	} elseif {$sz > 1} {
	    set what [$w.lst get active]
	} else {
	    return
	}
    }

    # -- scan commands
    foreach command $COMMANDS {
	dict with command {
	    debug "scan $name"
	    if { ($sz == 1) && ([string first $what $name] == 0) } {
		debug "run: $cmd"
		catch $cmd
		break
	    }
	    if { $name eq $what } {
		debug "run: $cmd"
		catch $cmd
		break
	    }
	    set txt [format "%-12s%s " $name $desc]
	    if { $txt eq $what } {
		debug "run: $cmd"
		catch $cmd
		break
	    }
	}
    }
}

# --------------------------------------------------------------------------
#  command-dialog
#  Shows a command dialog on top of current selected term
#  The window allows to enter a command or choose a command in a list
# --------------------------------------------------------------------------
proc command-dialog {} {
    set w .cmd
    if { ![winfo exists $w] } {
	toplevel $w -border { ulcorner hline urcorner vline lrcorner hline llcorner vline }

	label $w.title -text "Type Command"
	place $w.title -y 0 -relx 0.5 -bordermode ignore -anchor center

	entry $w.entry
	frame $w.sep0 -border hline -height 1
	scrollbar $w.scroll -command "$w.lst yview" -takefocus 0
	listbox $w.lst -yscrollcommand "$w.scroll set"
#	frame $w.sep1 -border hline -height 1
#	button $w.close -command "lower $w" -text "Close"

#	pack $w.close -side bottom -ipadx 1
#	pack $w.sep1 -side bottom -fill x
	pack $w.entry -side bottom -fill x
	pack $w.sep0 -side bottom -fill x
	pack $w.lst -side left -fill both -expand 1
	pack $w.scroll -side right -fill y

	# bindings
	bind $w.entry <KeyPress> command-update-list
	bind $w.entry <Return> {command-execute [%W get]}
	bind $w.lst   <Return> {command-execute}
	bind $w.lst   <1>  {%W activate @%x,%y ; command-execute}

	# we change the default binding order
	# to have the typed characters already inserted
	# before calling "command-update-list"
	bindtags $w.entry [list Entry $w.entry $w all]
    }
    
    $w.entry delete 0 end
    command-update-list
    focus $w.entry
    place $w -relx 0.5 -rely 0.5 -relwidth 0.5 -relheight 0.5 -anchor center
    raise $w
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
		    set focusid 1
		    set focused $t1
		}
	    }
	    2 - 3 {
		if { $focusid > 2 } {
		    error "invalid focusid $focusid for layout $layout"
		    set focusid 1
		    set focused $t1
		}
	    }
	    4 - 5 - 6 - 7 {
		if { $focusid > 3 } {
		    error "invalid focusid $focusid for layout $layout"
		    set focusid 1
		    set focused $t1
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
#  display-layout
#  Display a small toplevel window that shows the current layout and
#  the currently focused widget in the layout
# --------------------------------------------------------------------------
proc display-layout {} {
    toplevel .layout \
	-border {ulcorner hline urcorner vline lrcorner hline llcorner vline} \
	-width 3 -height 3
    #@ todo
}

# --------------------------------------------------------------------------
#  apply-layout --
#  Apply directives in layout
# --------------------------------------------------------------------------
proc apply-layout {} {
    global LAYOUT CHOOSER
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
	# -- note that if a "terminal chooser" is displayed the focus
	# -- is transfered to the listbox widget it contains.
	if {[lsearch $CHOOSER $focused] == -1} {
	    focus $focused
	} else {
	    focus $focused.termlist
	}
	debug "layout #$layout $w $h $focused"
    }
}

# -------------------------------------------------------------------------
#  pane-switch
#  Replace the content of the focused pane with a terminal chooser
#  dialog box.
# -------------------------------------------------------------------------
proc pane-switch {} {
    global LAYOUT CHOOSER
    dict with LAYOUT {
	# -- if the pane content is already a terminal chooser
	# -- there is nothing to do
	if {[lsearch $CHOOSER $focused] != -1} return
	set idx [get-focused-idx]
	set focused [choose-term]
	set t${idx} $focused
    }
    apply-layout
}

# -------------------------------------------------------------------------
#  pane-next
#  Transfer focus to the next pane in layout cycling if needed.
# -------------------------------------------------------------------------
proc pane-next {{inc 1}} {
    global LAYOUT
    set focusid [get-focused-idx]
    incr focusid $inc
    dict with LAYOUT {
	switch -exact -- $layout {
	    2 - 3 {
		set focused [set t$focusid]
		if {$focusid == 3} {
		    set focused $t1
		}
		if {$focusid == 0} {
		    set focused $t2
		}
	    }
	    4 - 5 - 6 - 7 {
		set focused [set t$focusid]
		if {$focusid == 4} {
		    set focused $t1
		}
		if {$focusid == 0} {
		    set focused $t3
		}
	    }
	    8 {
		set focused [set t$focusid]
		if {$focusid == 5} {
		    set focused $t1
		}
		if {$focusid == 0} {
		    set focused $t4
		}
	    }
	}
    }
    apply-layout
}

# -------------------------------------------------------------------------
#  pane-next
#  Transfer focus to the previous pane in layout cycling if needed.
# -------------------------------------------------------------------------
proc pane-prev {} {
    pane-next -1
}

# -------------------------------------------------------------------------
#  pane-hincr
#
#  Increment pane width when possible. The increment might be negative.
#  For some layouts the change is not possible, in this case the function
#  does nothing.
#
#  If once incremented the width reaches 1 or 0, the layout is changed.
# -------------------------------------------------------------------------
proc pane-hincr {{inc 0.1001}} {
    global LAYOUT
    set focusid [get-focused-idx]
    dict with LAYOUT {
	switch -exact -- $layout {
	    1 - 2 { return }
	    3 {
		if {$focusid == 1} {
		    set w [expr {$w + $inc}]
		} else {
		    set w [expr {$w - $inc}]
		}
		if {$w >= 1.0} {
		    set layout 1
		    set focused $t1
		}
		if {$w <= 0.0} {
		    set layout 1
		    swap-vars t1 t2
		    set focused $t1
		}
	    }
	    4 {
		if {$focusid == 1} {
		    set w [expr {$w + $inc}]
		} else {
		    set w [expr {$w - $inc}]
		}
		if {$w >= 1.0} {
		    set layout 1
		    set focused $t1
		}
		if {$w <= 0.0} {
		    set layout 2
		    swap-vars t1 t2
		    swap-vars t2 t3
		    if {$focusid == 3} {
			set focused $t2
		    } else {
			set focused $t1
		    }
		}
	    }
	    5 {
		if {$focusid == 3} {
		    set w [expr {$w - $inc}]
		} else {
		    set w [expr {$w + $inc}]
		}
		if {$w >= 1.0} {
		    set layout 2
		    if {$focusid == 3} {
			set focused $t1
		    }
		}
		if {$w <= 0.0} {
		    set layout 1
		    swap-vars t1 t3
		    set focused $t1
		}
	    }
	    6 {
		if {$focusid == 1} return
		if {$focusid == 2} {
		    set w [expr {$w + $inc}]
		} else {
		    set w [expr {$w - $inc}]
		}
		if {$w >= 1.0} {
		    set layout 2
		    set focused $t2
		}
		if {$w <= 0.0} {
		    set layout 2
		    if {$focusid == 2} {
			swap-vars t2 t3
		    }
		    set focused $t2 
		}
	    }
	    7 {
		if {$focusid == 3} return
		if {$focusid == 1} {
		    set w [expr {$w + $inc}]
		} else {
		    set w [expr {$w - $inc}]
		}
		if {$w >= 1.0} {
		    set layout 2
		    swap-vars t2 t3
		}
		if {$w <= 0.0} {
		    set layout 2
		    swap-vars t1 t2
		    swap-vars t2 t3
		    set focused $t1
		}
	    }
	    8 {
		if {$focusid == 1 || $focusid == 3} {
		    set w [expr {$w + $inc}]
		} else {
		    set w [expr {$w - $inc}]
		}
		if { $w >= 1.0 } {
		    set layout 2
		    swap-vars t2 t3
		    if {$focusid == 3} {
			set focused $t2
		    }
		}
		if {$w <= 0.0} {
		    set layout 2
		    swap-vars t1 t2
		    swap-vars t2 t4
		    set focused $t1
		    if {$focusid == 4} {
			set focused $t2
		    }
		}
	    }
	}
    }
    apply-layout
}

# -------------------------------------------------------------------------
#  pane-hdecr
#  Decrement pane width when possible.
# -------------------------------------------------------------------------
proc pane-hdecr {} {
    pane-hincr -0.1001
}

# -------------------------------------------------------------------------
#  pane-vincr
#
#  Increment pane height when possible. The increment might be negative.
#  For some layouts the change is not possible, in this case the function
#  does nothing.
#
#  If once incremented the height reaches 1 or 0, the layout is changed.
# -------------------------------------------------------------------------
proc pane-vincr {{inc 0.1001}} {
    global LAYOUT
    set focusid [get-focused-idx]
    dict with LAYOUT {
	switch -exact -- $layout {
	    1 { return }
	    2 {
		if {$focusid == 1} {
		    set h [expr {$h + $inc}]
		} else {
		    set h [expr {$h - $inc}]
		}
		if {$h >= 1.0} {
		    set layout 1
		    set focused $t1
		}
		if {$h <= 0.0} {
		    set layout 1
		    swap-vars t1 t2
		    set focused $t1
		}
	    }
	    3 { return }
	    4 {
		if {$focusid == 1} return
		if {$focusid == 2} {
		    set h [expr {$h + $inc}]
		} else {
		    set h [expr {$h - $inc}]
		}
		if {$h >= 1.0} {
		    set layout 3
		    if {$focusid == 3} {
			set focused $t2
		    }
		}
		if {$h <= 0.0} {
		    set layout 3
		    swap-vars t2 t3
		    set focused $t2
		}
	    }
	    5 {
		if {$focusid == 3} return
		if {$focusid == 1} {
		    set h [expr {$h + $inc}]
		} else {
		    set h [expr {$h - $inc}]
		}
		if {$h >= 1.0} {
		    set layout 3
		    if {$focusid == 1} {
			swap-vars t2 t3
		    }
		    if {$focusid == 2} {
			swap-vars t1 t2
			swap-vars t2 t3
		    }
		    set focused $t1
		}
		if {$h <= 0.0} {
		    set layout 3
		    if {$focusid == 1} {
			swap-vars t1 t2
			swap-vars t2 t3
		    }
		    if {$focusid == 2} {
			swap-vars t2 t3
		    }
		    set focused $t1
		}
	    }
	    6 {
		if {$focusid == 1} {
		    set h [expr {$h + $inc}]
		} else {
		    set h [expr {$h - $inc}]
		}
		if {$h >= 1.0} {
		    if {$focusid == 1} {
			set layout 1
		    }
		    if {$focusid == 2} {
			set layout 3
			swap-vars t1 t2
			swap-vars t2 t3
			set focused $t1
		    }
		    if {$focusid == 3} {
			set layout 3
			swap-vars t1 t2
			swap-vars t2 t3
			set focused $t2			
		    }
		}
		if {$h <= 0.0} {
		    if {$focusid == 1} {
			set layout 3
			swap-vars t1 t2
			swap-vars t2 t3
		    }
		    if {$focusid == 2 || $focusid == 3} {
			set layout 1
		    }
		    set focused $t1
		}
	    }
	    7 {
		if {$focusid == 3} {
		    set h [expr {$h - $inc}]
		} else {
		    set h [expr {$h + $inc}]
		}
		if {$h >= 1.0} {
		    if {$focusid == 1 || $focusid == 2} {
			set layout 3
		    }
		    if {$focusid == 3} {
			set layout 1
			swap-vars t1 t3
			set focused $t1
		    }
		}
		if {$h <= 0.0} {
		    if {$focusid == 1 || $focusid == 2} {
			set layout 1
			swap-vars t1 t3
		    }
		    if {$focusid == 3} {
			set layout 3
		    }
		    set focused $t1
		}
	    }
	    8 {
		if {$focusid == 1 || $focusid == 2} {
		    set h [expr {$h + $inc}]
		} else {
		    set h [expr {$h - $inc}]
		}
		if {$h >= 1.0} {
		    set layout 3
		    if {$focusid == 3} {
			swap-vars t1 t3
			swap-vars t2 t4
			set focused $t1
		    }
		    if {$focusid == 4} {
			swap-vars t1 t3
			swap-vars t2 t4
			set focused $t2
		    }
		}
		if {$h <= 0.0} {
		    set layout 3
		    if {$focusid == 1} {
			swap-vars t1 t3
			swap-vars t2 t4
			set focused $t1
		    }
		    if {$focusid == 2} {
			swap-vars t1 t3
			swap-vars t2 t4
			set focused $t2
		    }
		}
	    }
	}
    }
    apply-layout
}

# -------------------------------------------------------------------------
#  pane-vdecr
#  Decrement pane height when possible.
# -------------------------------------------------------------------------
proc pane-vdecr {} {
    # -- avoid numerical artifacts
    pane-vincr -0.1001
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
#  mk-button
#  Helper function to create a menubar button and display it.
# --------------------------------------------------------------------------
proc mk-button { name text command {pack left} } {
    button .l.${name} -text " ${text} " -command $command
    bind .l.${name} <Left>  {focus [ck_focusPrev %W]}
    bind .l.${name} <Right> {focus [ck_focusNext %W]}
    pack .l.${name} -side ${pack}
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
    
    $t config -state disabled -takefocus 0
    focus .help.dismiss
}

# --------------------------------------------------------------------------
#  create-gui
#  Creates user interface
# --------------------------------------------------------------------------
proc create-gui {} {
    global LAYOUT
    
    label .l -text "Terminal multiplexer" -bg blue
    pack .l -side top -fill x
    frame .f
    pack .f -side top -fill both -expand yes
    
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

    #mk-button q "Quit" exit right
    #mk-button h "Help" help right
    mk-button c "Commands" command-dialog right

    mk-button state "layout" noop
    
    apply-layout
}

# --------------------------------------------------------------------------
#  new-term
#  Creates a new terminal widget
# --------------------------------------------------------------------------
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

    do-bindings .f.t${TX}
    
    return .f.t${TX}
}

# --------------------------------------------------------------------------
#  create-pref
#  Creates a user specific file for storing preferences
# --------------------------------------------------------------------------
proc create-pref-file {} {
    global PREFS
    catch {
	if {[file exists "~/.tmuckrc"]} return
	set fout [open "~/.tmuckrc" "w"]
	puts $fout $PREFS
	close $fout
    }
}

# --------------------------------------------------------------------------
#  load-pref-file
#
#  Load preference file searching in various places.
#   - first, tries to source file "/etc/tmuck/profile",
#   - then :
#       - if TMUCKRC environment variable is set. Its value is interpreted
#         as a file name and its content is sourced.
#       - otherwise, tries to source file "~/.tmuckrc".
#
#  As a result the preferences sourced is a combination of system wide
#  settings and user specific settings. The user specific settings
#  take precedence over system wide settings.
# --------------------------------------------------------------------------
proc load-pref-file {} {
    if {[info exists ::env(TMUCKRC)]} {
	lappend lst $::env(TMUCKRC)
    } else {
	lappend lst "~/.tmuckrc"
    }
    lappend lst "/etc/tmuck/profile"
    foreach f $lst {
	catch {source $f}
    }
}


# --------------------------------------------------------------------------
# --------------------------------------------------------------------------
proc do-bindings {{w all}} {
    global COMMANDS
    set m {
	"left"     "Left"
	"right"    "Right"
	"down"     "Down"
	"up"       "Up"
    }
    foreach command $COMMANDS {
	dict with command {
	    foreach k $shortcut {
		set k [string map $m $k]
		if {[string first "C-" $k] == 0} {
		    set k "<[string map {C- Control-} [string toupper $k]]> "
		}
		append key $k
	    }
	    bind $w [string trim $key] $cmd
	}
    }
}

# --------------------------------------------------------------------------
create-pref-file
load-pref-file
create-gui
do-bindings
# --------------------------------------------------------------------------

