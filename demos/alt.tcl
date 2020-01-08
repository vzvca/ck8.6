frame .f -bg yellow 
pack .f -fill both -expand yes

bind .f <Alt-r> {.f configure -bg red}
bind .f <Alt-g> {.f configure -bg green}
bind .f <Alt-b> {.f configure -bg blue}
bind .f <Alt-y> {.f configure -bg yellow}

bind .f <Control-x> {.f configure -bg cyan}

focus .f

after 10000 { exit 0 }
