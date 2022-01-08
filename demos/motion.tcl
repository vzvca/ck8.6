message .msg -justify center -width 80 -text "Demonstrate mouse motion events.\nMove the mouse in empty area below.\nPress <Q> to quit."

pack .msg -side top -fill x

proc settxt {msg} {
    .msg configure -text "Demonstrate mouse motion events.\n${msg}\nPress <Q> to quit."
}

frame .f -bg white
pack .f -fill both -expand yes

bind .f <Key-q> {exit 0}

bind .f <Motion> { settxt "Mouse @ %x %y" }
bind .f <1> { settxt "Button-1 !" }
bind .f <ButtonRelease-3> { settxt "Release-3 !" }

bind .f <Button-4> { settxt "Button-4 !" }
bind .f <Button-5> { settxt "Button-5 !" }

focus .f
