##+##########################################################################
#
# Montana -- plays Montana solitaire
# by Keith Vetter, April 2006
#
# Adaptation to Ck
# by VCA, Dec 2019
 
 
array set S {title "Montana Solitaire" lm 30 bm 30 tm 100 padx 5 pady 5
    color green gcolor cyan }

 
proc DoDisplay {} {
    global S
 
    eval destroy [winfo child .]
    DoMenus
    
    frame .c  -width $S(w) -height $S(h) -bg $S(color) -attr dim
   
    frame .bottom 
    label .bottom.lmoves -text "Moves:" -anchor e
 
    label .bottom.vmoves -textvariable ::STATS(moves) -anchor e
    label .bottom.lgood -text "Good:" -anchor e
    label .bottom.vgood -textvariable ::STATS(good) -anchor e
    label .bottom.lredeals -text "Redeals:" -anchor e
    label .bottom.vredeals -textvariable ::STATS(redeals) -anchor e
    grid .bottom.lgood    .bottom.vgood -sticky ew
    grid .bottom.lmoves   .bottom.vmoves -sticky ew
    grid .bottom.lredeals .bottom.vredeals -sticky ew
    grid columnconfigure .bottom 2 -weight 1
 
    pack .c -side top -fill both -expand 1
    pack .bottom -side top -fill x
    bind all <Key-F2> StartGame
    bind all <Control-z> Undo
 
    GetCardPlacement
}
 
##+##########################################################################
#
# DoMenus -- isn't installing menus really verbose and clunky?
#
proc DoMenus {} {
    option add *Menu.tearOff 0

    frame .menu
    menubutton .menu.game -text "Game " -underline 0
    menu .menu.game.m 
    .menu.game configure -menu .menu.game.m
    .menu.game.m add command -label "New Game" -command StartGame -underline 0 \
        -accelerator "F2"
    .menu.game.m add command -label "Restart" -command [list StartGame 1] \
        -underline 0
    .menu.game.m add separator
    .menu.game.m add command -label "Undo" -command Undo -underline 0 \
        -accelerator "Ctrl-Z"
    .menu.game.m add separator
    .menu.game.m add command -label "Exit" -underline 1 -command exit
 
    menubutton .menu.help -text "Help " -underline 0
    menu .menu.help.m 
    .menu.help configure -menu .menu.help.m
    .menu.help.m add command -label "Help" -underline 0 -command Help
    .menu.help.m add command -label "About" -underline 0 -command About

    pack .menu.game .menu.help -side left
    pack .menu -side top -fill x
}
##+##########################################################################
#
# GetCardPlacement -- sets up board with lots of empty tagged items
#
proc GetCardPlacement {} {
    global S
 
    for {set idx 0} {$idx < 52} {incr idx} {
        set row [expr {$idx / 13}]
        set col [expr {$idx % 13}]
        set x [expr {$S(lm) + $col * ($S(cw)+$S(padx))}]
        set y [expr {$S(tm) + $row * ($S(ch)+$S(pady))}]
 
        set x1 [expr {$x+$S(cw)}]
        set y1 [expr {$y+$S(ch)}]

	frame .c.g_${row}_${col} -bg $::S(color) -fg $::S(color)
	grid .c.g_${row}_${col} -column $col -row $row -sticky nsew -ipady 1
	
	playcard .c.g_${row}_${col}.card -fg $::S(color) -bg $::S(color) -width 7 -height 5
	grid .c.g_${row}_${col}.card -column 1 -row 1 -sticky nsew
	bind .c.g_${row}_${col}.card <Button-1> [list CardB1 $row $col]
	bind .c.g_${row}_${col}.card <Button-3> [list CardB3 $row $col]
    }
    
    bind all <ButtonPress-2> [list Hint3 down]
    bind all <ButtonRelease-2> [list Hint3 up]
}
proc CardB1 { row col } {
    set forb [.c.g_${row}_${col}.card cget -side]
    if { $forb eq "back" } {
	Hint2 $row $col double
    }
    if { $forb eq "front" } {
	Click $row $col
    }
}
proc CardB3 { row col } {
    set forb [.c.g_${row}_${col}.card cget -side]
    if { $forb eq "back" } {
	Hint2 $row $col
    }
    if { $forb eq "front" } {
	Hint $row $col
    }
}

##+##########################################################################
#
# Click -- handles moving a card after clicking on it
#
proc Click {row col} {
    global B
    
    set card $B($row,$col)
    if {$card eq "gap"} return                  ;# Be safe, shouldn't happen
    if {$card eq "X"} return                    ;# Be safe, shouldn't happen
 
    set pred [CardPredecessor $card]
    if {! [string match "2?" $card]} {
        foreach {r c} $B(r,$pred) break
        incr c
    } else {
        set c 0
        for {set r 0} {$r < 3} {incr r} {
            if {$B($r,$c) eq "gap"} break
        }
    }
 
    if {$B($r,$c) eq "gap"} {
        lappend B(undo) [list $row $col $r $c]
        .menu.game.m entryconfig "Undo" -state normal
        incr ::STATS(moves)
        MoveCardToGap $row $col $r $c
    } else {
        Flash bad $row $col
    }
}
##+##########################################################################
#
# Flash -- temporarily highlights a card for either bad move or hint
#
proc Flash {how args} {
    array set delays {bad 300 good 1000 all 15000}
    array set clr {bad red good magenta all yellow}
 
    foreach aid [after info] { after cancel $aid }

    foreach {row col} $args {
	.c.g_${row}_${col} configure -bg $clr($how) -fg $clr($how)
	after $delays($how) \
	    [list .c.g_${row}_${col} configure -bg $::S(color) -fg $::S(color)]
    }
}
##+##########################################################################
#
# CanMove -- returns true if a valid move still exists
#
proc CanMove {} {
    global B
 
    foreach gap $B(gaps) {
        foreach {row col} $gap break
        if {$col == 0} { return 1 }
        set left $B($row,[expr {$col-1}])
        if {$left eq "gap"} continue
        if {! [string match "k?" $left]} { return 1 }
    }
    return 0
}
##+##########################################################################
#
# MoveCardToGap -- moves card from row/col to the gap at r/c
#
proc MoveCardToGap {row col r c} {
    global B
 
    set card $B($row,$col)
    set B($row,$col) "gap"
    set B($r,$c) $card
    set B(r,$card) [list $r $c]
    set n [lsearch $B(gaps) [list $r $c]]
    lset B(gaps) $n [list $row $col]

    .c.g_${row}_${col}.card configure -side back
    .c.g_${r}_${c}.card configure -suit [Suit $card] -rank [Pip $card] -side front
    
    EndTurn
}
##+##########################################################################
#
# EndTurn -- Handles end-of-turn logic
#
proc EndTurn {} {
    HighlightGood
    set ::STATS(good) [llength [FindGood]]
    if {[CanMove]} return
    if {$::STATS(good) == 48} {
        ck_messageBox -title $::S(title) -message "You Won!"
    } else {
        ck_messageBox -title $::S(title) -message "No more moves.\n\nRedeal"
        Redeal
    }
}
##+##########################################################################
#
# HighlightGood -- highlight all cards in their proper position
#
proc HighlightGood {} {
    global B
    foreach card [FindGood] {
        foreach {row col} $B(r,$card) break
	.c.g_${row}_${col} configure -bg $::S(gcolor)
    }
}
##+##########################################################################
#
# FindGood -- finds all cards that are in their proper position
#
proc FindGood {} {
    global B
    set pos {2 3 4 5 6 7 8 9 t j q k}
 
    set good {}
    for {set row 0} {$row < 4} {incr row} {
        set head $B($row,0)
        if {! [string match "2?" $head]} continue
        set hsuit [string index $head 1]
        for {set col 0} {$col < 13} {incr col} {
            foreach {pip suit} [split $B($row,$col) ""] break
            if {$suit ne $hsuit} break
            if {$pip ne [lindex $pos $col]} break
 
            lappend good $B($row,$col)
        }
    }
    return $good
}
##+##########################################################################
#
# About -- tell something about us
#
proc About {} {
    set txt "$::S(title)\n\nby Keith Vetter\nApril, 2006"
    append txt "\n\n\nAdaptation to CK\n"
    append txt "by VCA\nDecember, 2019"
    ck_messageBox -icon info -message $txt -title "About $::S(title)"
}
##+##########################################################################
#
# Help -- a simple help screen
#
proc Help {} {
    catch {destroy .help}
    toplevel .help -bg black -fg white \
	-border {ulcorner hline urcorner vline lrcorner hline llcorner vline}

    set W [winfo width  .]
    set H [winfo height .]
    incr W -1
    incr H -1
    
    place .help -x 1 -width $W -y 1 -height $H
    raise .help
 
    set t .help.t
    text $t -wrap word -width 70 -height 30 -yscrollcommand {.help.sb set}
    scrollbar .help.sb -orient vertical -command [list $t yview]
    button .help.dismiss -text Dismiss -command {destroy .help}
    pack .help.dismiss -side bottom -pady 1
    pack .help.sb -side right -fill y
    pack $t -side top -expand 1 -fill both
 
    $t tag config title -justify center -foregr red -attr bold
    $t tag configure title2 -justify center -attr bold
    $t tag configure header -attr bold
    $t tag configure n -lmargin1 10 -lmargin2 10
    $t tag configure bullet -lmargin1 20 -lmargin2 30
 
    $t insert end "$::S(title)\n" title
    $t insert end "by Keith Vetter\n\n" title2
 
    $t insert end "$::S(title) is a simple solitaire game that goes by "
    $t insert end "a variety of names including \x22Gaps\x22, \x22Rangoon\x22, "
    $t insert end "\"BlueMoon\", \x22Station\x22 and \x22Montana Aces\x22.\n\n"
 
    $t insert end "Tableau\n" header
    $t insert end "At the start of the game, all 52 cards are shuffled and "
    $t insert end "dealt face up in four rows of thirteen cards. The four aces "
    $t insert end "are removed creating four gaps.\n\n"
 
    $t insert end "Object\n" header
 
    $t insert end "The objective is to rearrange the cards so that each row "
    $t insert end "contains the cards of a single suit ordered from deuce to "
    $t insert end "king. (The last column in each row will contain a gap "
    $t insert end "instead of the ace.)\n\n"
 
    $t insert end "The Play\n" header
 
    $t insert end "If a gap appears in the first column, you can move any "
    $t insert end "deuce to that position. If a gap appears elsewhere, you "
    $t insert end "move there only the card with same suit and one higher "
    $t insert end "rank than the card to the left of the gap. For example, "
    $t insert end "if the 5 of Hearts appears to the left of a gap, you "
    $t insert end "can move the 6 of Hearts to that gap. If the card to "
    $t insert end "the left of the gap is a King or another gap, you cannot "
    $t insert end "move any card to that gap.\n\n"
 
    $t insert end "Whenever you move a card, you'll fill one gap, but create "
    $t insert end "a new one.\n\n"
 
    $t insert end "Mechanics\n" header
    $t insert end "o Click on a card to move it to a gap (if legal)\n" bullet
    $t insert end "o Right-click on card to highlight its predecessor\n" bullet
    $t insert end "o Right-click on a gap to highlight legal move\n" bullet
    $t insert end "o Double-click on a gap to fill gap\n" bullet
    $t insert end "o Hold middle-button down to highlight all legal moves\n" bullet
    $t insert end "\n"
 
    $t insert end "Redeal\n" header
    $t insert end "If no move is possible, a redeal occurs automatically. "
    $t insert end "All cards which are not in their correct positions are "
    $t insert end "picked up, shuffled and redealt. Again the four aces are "
    $t insert end "removed creating four gaps.\n\n"
 
    $t insert end "See Also\n" header
    $t insert end "For more details about all the different variants in "
    $t insert end "family of solitaire games, see "
    $t insert end "http://web.inter.nl.net/hcc/Rudy.Muller/ranrules.html\n\n"
    $t config -state disabled
}
##+##########################################################################
#
# MakeCards -- makes are deck and cards
#
proc MakeCards {} {
    global S
 
    set S(deck) {}
    foreach suit {s d c h} {
        foreach pip {a 2 3 4 5 6 7 8 9 t j q k} {
            lappend S(deck) "$pip$suit"
        }
    }
 
    set S(cw) 7
    set S(ch) 9
    set S(w) [expr {2*$S(lm) + 13*$S(cw) + 12*$S(padx)}]
    set S(h) [expr {$S(tm) + 4*$S(ch) + 3*$S(pady) + $S(bm)}]
}
proc Suit { card } {
    array set suits {
	s spade d diamond c club h heart
    }
    return $suits([string index $card 1])
}
proc Pip { card } {
    array set pips {
	a ace 2 2 3 3 4 4 5 5 6 6 7 7 8 8 9 9 \
	t 10 j jake q queen k king
    }
    return $pips([string index $card 0])
}
##+##########################################################################
#
# Shuffle -- Shuffles a list
#
proc Shuffle { l } {
    set len [llength $l]
    set len2 $len
    for {set i 0} {$i < $len-1} {incr i} {
        set n [expr {int($i + $len2 * rand())}]
        incr len2 -1
 
        # Swap elements at i & n
        set temp [lindex $l $i]
        lset l $i [lindex $l $n]
        lset l $n $temp
    }
    return $l
}
##+##########################################################################
#
# StartGame -- starts a new game
#
proc StartGame {{noShuffle 0}} {
    global S B STATS
 
    array unset STATS
    array set STATS {moves 0 redeals 0 good 0}
 
    array unset B
    array set B {0,13 X 1,13 X 2,13 X 3,13 X 4,0 X} ;# Sentinels
    .menu.game.m entryconfig "Undo" -state disabled
    if {! $noShuffle} {
        set S(cards) [Shuffle $S(deck)]
    }

    # Deal all the cards
    for {set idx 0} {$idx < 52} {incr idx} {
        set row [expr {$idx / 13}]
        set col [expr {$idx % 13}]
        set card [lindex $S(cards) $idx]
        if {[string match "a?" $card]} {        ;# Ace, leave a gap
            set B($row,$col) "gap"
            lappend B(gaps) [list $row $col]
	    .c.g_${row}_${col}.card configure -side back
        } else {
            set B($row,$col) $card
            set B(r,$card) [list $row $col]
	    
	    .c.g_${row}_${col}.card configure -side front \
		-suit [Suit $card] -rank [Pip $card]
        }
    }
    EndTurn
}
##+##########################################################################
#
# CardPredecessor -- returns previous card in sequence
#
proc CardPredecessor {card} {
    set n [lsearch $::S(deck) $card]
    return [lindex $::S(deck) [expr {$n-1}]]
}
##+##########################################################################
#
# CardSuccessor -- returns next card in sequence
#
proc CardSuccessor {card} {
    set n [lsearch $::S(deck) $card]
    return [lindex $::S(deck) [expr {$n+1}]]
}
##+##########################################################################
#
# Redeal -- deals out all cards that are not in their proper position
#
proc Redeal {} {
    global S B
 
    incr ::STATS(redeals)
    set good [FindGood]
    set bad {}                                  ;# All the cards to deal
    set cells $B(gaps)                          ;# Where to deal to
    foreach card $S(deck) {
        if {[lsearch $good $card] > -1} continue
        lappend bad $card
        catch {lappend cells $B(r,$card)}
    }
 
    set B(undo) {}
    .menu.game.m entryconfig "Undo" -state disabled
    while {1} {
        set B(gaps) {}
        set cards [Shuffle $bad]
 
        foreach card $cards cell $cells {
            foreach {row col} $cell break
            if {[string match "a?" $card]} {    ;# Ace, leave a gap
                set B($row,$col) "gap"
                lappend B(gaps) [list $row $col]
		.c.g_${row}_${col}.card configure -side back
            } else {
                set B($row,$col) $card
                set B(r,$card) [list $row $col]
		.c.g_${row}_${col}.card configure -side front \
		    -suit [Suit $card] -rank [Pip $card]
            }
        }
        if {[CanMove]} break
    }
    EndTurn
}
proc Undo {} {
    global B
 
    if {$B(undo) eq {}} return
    foreach {r c row col} [lindex $B(undo) end] break
    set B(undo) [lrange $B(undo) 0 end-1]
    MoveCardToGap $row $col $r $c
    incr ::STATS(moves)
    if {$B(undo) eq {}} {
        .menu.game.m entryconfig "Undo" -state disabled
    }
 
}
##+##########################################################################
#
# Hint -- shows predecessor for a given card
#
proc Hint {row col} {
    global B
 
    set pred [CardPredecessor $B($row,$col)]
    if {! [info exists B(r,$pred)]} return
    foreach {r c} $B(r,$pred) break
    Flash good $r $c
}
##+##########################################################################
#
# Hint2 -- shows which card goes into a gap
#
proc Hint2 {row col {how single}} {
    global B
 
    if {$B($row,$col) ne "gap"} return
    incr col -1
    if {$col < 0} return
    set card $B($row,$col)
    if {$card eq "gap"} return
    if {[string match "k?" $card]} return
 
    set succ [CardSuccessor $card]
    if {! [info exists B(r,$succ)]} return
    if {$how eq "single"} {
        eval Flash good $B(r,$succ)
    } else {                                    ;# Double click--do actual move
        eval Click $B(r,$succ)
    }
}
 
proc Hint3 {updown} {
    global B
 
    if {$updown eq "up"} {
        Flash all                               ;# Turn off highlighting
        return
    }
    
    set moves {}
    foreach pos $B(gaps) {
        foreach {row col} $pos break
        if {$col == 0} {                        ;# Empty in left column
            foreach card {2h 2c 2d 2s} {
                foreach {row col} $B(r,$card) break
                if {$col > 0} {
                    lappend moves $row $col
                }
            }
            continue
        }
        incr col -1
        if {$col < 0} continue
        
        set card $B($row,$col)
        if {$card eq "gap"} continue            ;# Left is gap
        if {[string match "k?" $card]} continue ;# Left is a king
 
        set succ [CardSuccessor $card]
        if {! [info exists B(r,$succ)]} continue;# Shouldn't happen
        eval lappend moves $B(r,$succ)
    }
    eval Flash all $moves
}

################################################################
DoMenus
MakeCards
DoDisplay
StartGame
return


