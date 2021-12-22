# --------------------------------------------------------------------------
#  This script demonstrates a color animation using palette rotation.
#  Terminal colors are redefined.
# --------------------------------------------------------------------------

set MXCOL 48
set counter 0

# --------------------------------------------------------------------------
#  -- Save colors
# --------------------------------------------------------------------------
for { set i 16 } { $i < $MXCOL } { incr i } {
    set colors($i) [color cell $i]
}

# --------------------------------------------------------------------------
#  -- Restore colors
# --------------------------------------------------------------------------
proc bye {} {
    for { set i 16 } { $i < $::MXCOL } { incr i } {
	color cell $i $::colors($i)
    }
    after idle exit
}

# --------------------------------------------------------------------------
#  -- Timer
# --------------------------------------------------------------------------
proc timer {} {
    incr ::counter
    for { set i 16 } { $i < $::MXCOL } { incr i } {
	set j [expr {$i + $::counter - 16}]
	color cell $i $::colors([expr {16 + ($j % ($::MXCOL - 16))}])
	.f.f${i} configure -bg @${i}
    }
    after 1000 timer
}
after 3000 timer

# --------------------------------------------------------------------------
#  -- Draw GUI
# --------------------------------------------------------------------------
label .m -h 3 -text "This script demonstrates palette rotation effects. After a few seconds animation will start. Type <q> to quit."
frame .f
pack .m -side top -fill x
for { set i 16 } { $i < $::MXCOL } { incr i } {
    frame .f.f${i} -bg @${i}
    pack .f.f${i} -side left -fill both -expand y
}
pack .f -side bottom -fill both -expand yes
##.m configure -text [array get ::colors]
. configure -bg green
bind all <Key-q> bye


