message .msg -justify center -width 80 -text "Demonstrate <Alt-?> key bindings.\n? can be r,g,b or y, it will change window background color.\n<Ctrl-x> changes background to cyan.\nPress <Q> to quit."

pack .msg -side top -fill x

frame .f -bg white
pack .f -fill both -expand yes

bind .f <Alt-r> {.f configure -bg red}
bind .f <Alt-g> {.f configure -bg green}
bind .f <Alt-b> {.f configure -bg blue}
bind .f <Alt-y> {.f configure -bg #ffff00}

bind .f <Alt-B> {.f configure -bg black}

bind .f <Control-x> {.f configure -bg cyan}

bind .f <Key-q> {exit 0}

focus .f
