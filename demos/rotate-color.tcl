# --------------------------------------------------------------------------
#  This script demonstrates a color animation using palette rotation.
#  Terminal colors are redefined.
# --------------------------------------------------------------------------


# --------------------------------------------------------------------------
#  -- Save colors
# --------------------------------------------------------------------------
for { set i 16 } { $i < 24 } { incr i } {
    set colors($i) [color cell $i]
}

# --------------------------------------------------------------------------
#  -- Restore colors
# --------------------------------------------------------------------------
proc bye {} {
    for { set i 16 } { $i < 24 } { incr i } {
	color cell $i $::colors($i)
    }
    after idle exit
}

# --------------------------------------------------------------------------
#  -- Timer
# --------------------------------------------------------------------------
proc timer {} {
    for { set i 16 } { $i < 24 } { incr i } {
	color cell $i [list "red" [expr int(rand()*255)] "green" 10 "blue" 20]
	.f${i} configure -bg @${i}
    }
    after 1000 timer
}
after 3000 timer

# --------------------------------------------------------------------------
#  -- Draw GUI
# --------------------------------------------------------------------------
message .m -text "This script demonstrates palette rotation effects."
for { set i 16 } { $i < 24 } { incr i } {
    frame .f${i} -bg @${i}
    pack .f${i} -side left -fill both -expand y
}
.m configure -text [array get ::colors]
pack .m -side bottom -fill x

bind all <Key-q> bye


