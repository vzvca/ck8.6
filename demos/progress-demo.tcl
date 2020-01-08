# --------------------------------------------------------------------------
#  Demo of progress widget
# --------------------------------------------------------------------------

progress .p1 -height 3 -fg green  -bg blue -border {ulcorner hline urcorner vline lrcorner hline llcorner vline}
progress .p2 -height 5 -fg yellow -border {ulcorner hline urcorner vline lrcorner hline llcorner vline}
progress .p3 -height 4 -fg blue   -variable valp3

frame .f -bg @6
progress .f.p4 -bg yellow -fg black -orient vertical -width 5 -border {ulcorner hline urcorner vline lrcorner hline llcorner vline}

frame .bar
button .bar.l3 -textvariable valp3
button .bar.bstart -text " Start " -command {
    .p1 start 500
    .p2 start 250
    .p3 start 300
    .f.p4 start 50
}
button .bar.bstop  -text " Stop "  -command {
    .p1 stop
    .p2 stop
    .p3 stop
    .f.p4 stop
}
button .bar.bquit  -text " Quit "  -command {
    exit 0
}
button .bar.breset -text " Reset " -command {
    .p1 configure -value 0
    .p2 configure -value 0
    .p3 configure -value 0
    .f.p4 configure -value 0
}

pack .bar -side bottom -fill x
pack .bar.bstart .bar.bstop .bar.breset .bar.bquit -side left
pack .bar.l3 -side right

pack .p1 -fill x -side top -anchor nw
pack .p2 -fill x -side top -anchor nw
pack .p3 -fill x -side top -anchor nw
pack .f -fill both -expand yes 
pack .f.p4 -fill y -side left -anchor nw

