message .m
color threshold 10
color cell 2 {red 0 green 0xaa blue 0x10}
set msg [color names]
.m configure -text $msg

pack .m
bind all <Key-q> exit

puts $msg
after 2000



