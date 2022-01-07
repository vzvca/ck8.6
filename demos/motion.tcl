message .msg -justify center -width 80 -text "Demonstrate mouse motion events.\nMove the mouse in empty area below.\nPress <Q> to quit."

pack .msg -side top -fill x

proc settxt {msg} {
    .msg configure -text "Demonstrate mouse motion events.\n${msg}\nPress <Q> to quit."
}

frame .f -bg white
pack .f -fill both -expand yes

bind .f <Key-q> {exit 0}

bind .f <Motion> { settxt "Mouse @ %y %x" }
bind .f <1> { settxt "Ring !"; bell }

focus .f
