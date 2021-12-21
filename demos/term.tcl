
label .l -text "Demonstration of terminal widget. Click here to quit." -bg blue
pack .l -side top -fill x

button .l.b1 -text " Term1 " -command {
    pack forget .t1 .t2
    pack .t1 -side left -anchor w -expand yes -fill both
}
button .l.b2 -text " Term2 " -command {
    pack forget .t1 .t2
    pack .t2 -side left -anchor e -expand yes -fill both
}
button .l.b3 -text " Both  " -command {
    pack forget .t1 .t2
    pack .t1 -side left -anchor w -expand yes -fill both
    pack .t2 -side left -anchor e -expand yes -fill both
}
pack .l.b1 .l.b2 .l.b3 -side left

terminal .t1 -exec /bin/bash -term xterm-256color
pack .t1 -side left -anchor w -expand yes -fill both

terminal .t2 -exec /bin/bash -term xterm-256color
pack .t2 -side left -anchor e -expand yes -fill both

bind .t1 <Button-1> {focus %W}
bind .t2 <Button-1> {focus %W}

bind .l <Button-1> exit

