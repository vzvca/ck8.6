# --------------------------------------------------------------------------
#  Small terminal multiplexer program
#  Demonstrates the terminal widget
# --------------------------------------------------------------------------

set TX  0
set CTX 0

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
    global TX
}

proc prev-term { t } {
    global TX
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
#  -- Help
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

new-term

set-term .t1

mk-button q "Quit" exit right
mk-button h "Help" help right

