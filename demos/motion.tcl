message .msg -justify center -width 80 -text "Demonstrate mouse motion events.\nMove the mouse in empty area below.\nPress <Q> to quit."

pack .msg -side top -fill x

frame .f -bg white
pack .f -fill both -expand yes

bind .f <Key-q> {exit 0}

bind .f <Motion> {.msg configure -text "Demonstrate mouse motion events.\nMouse @ %y %x\nPress <Q> to quit."}

focus .f
