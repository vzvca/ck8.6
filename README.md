# ck8.6
This work is based on earlier work by Christian Werner. His original library has been ported to Tcl 8.6 and a few widgets, commands and demos were added.

Ck is a Tk port to curses. A subset of Tk widgets have been ported to curses allowing to develop textual user interface nearly the same way Tk interfaces are coded.

## Changes compared to original version

 * Port to Tcl 8.6
 * Added a progress bar widget
 * Added a terminal emulator widget
 * Added a spinbox widget
 * A plya card widget, useful to code card games
 * More colors and a way to define custom colors
 * Support for <alt-key> form of bindings

## Documentation

The original documentation is included (in the doc subdirectory) in man page format. The man pages were converted to HTML and can be found in the html subdirectory.

## Demos

A few games have been coded / ported form Tk to Ck. Most of the games come from the Tcl/Tk wiki. I would like to thanks here the original authors of these games :
 * Keith Wetter : I stole its montana solitaire
 * XXX : I copied TinyTetris and changed it

![alt text](screenshots/tetris.png)
