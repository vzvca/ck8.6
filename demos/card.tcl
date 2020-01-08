# --------------------------------------------------------------------------
#   Small demo of playcard
# --------------------------------------------------------------------------

playcard .c1 -fg yellow -bg red -suit heart -rank 10
playcard .c2 -fg yellow -bg blue -suit spade -rank 9 -attr blink
playcard .c3 -width 10 -height 8 -fg yellow -bg red -suit diamond -rank king
playcard .c4 -width 13 -height 10 -fg black -bg cyan -suit c -rank queen

pack .c1 .c2 .c3 .c4 -padx 1 -side left

bind Playcard <Button-1> { %W configure -side back ; after 1500 { %W configure -side front } }

after 10000 {exit 0}
