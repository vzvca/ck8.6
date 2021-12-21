# ck8.6
This work is based on earlier work by Christian Werner. His original library has been ported to Tcl 8.6 and a few widgets, commands and demos were added.

Ck is a Tk port to curses. A subset of Tk widgets have been ported to curses allowing to develop textual user interface nearly the same way Tk interfaces are coded.

## Changes compared to original version

 * Port to Tcl 8.6
 * Added a progress bar widget
 * Added a terminal emulator widget
 * Added a spinbox widget
 * A play card widget, useful to code card games
 * More colors and a way to define custom colors
 * Support for `<alt-key>` form of bindings

## Building

The following dependencies are required:

 * libtcl8.6-dev
 * libncursesw5-dev
 * [libgpm-dev] : required for mouse support in text console.

Debian packaging is provided (tested on debian stretch and debian buster).

~~~~
$ ./configure
$ make && sudo make install
~~~~

## Mouse support

Mouse is supported out of the box in X11 terminals windows. In true text console, mouse is supported through `gpm` which needs to be installed on the system. On debian based system, as `root` :

~~~~
$ apt-get install gpm
$ systemctl enable gpm.service
$ systemctl start gpm.service
~~~~

## Resizing

True text consoles are not resized but X11 terminals are often resized.

It might be necessary to run `cwsh` with environment variable `CK_USE_GPM=1` set. Sometimes ncurses does not support gpm and `cwsh` has to support it by itself.


## Documentation

The original documentation is included (in the doc subdirectory) in man page format. The man pages were converted to HTML and can be found in the html subdirectory.

## Demos

A few games have been coded / ported form Tk to Ck. Most of the games come from the Tcl/Tk wiki. I would like to thanks here the original authors of these games :
 * Keith Wetter : I stole its montana solitaire.
 * BHE : I copied TinyTetris and changed it.

### Puzzle

~~~~
$ cwsh demos/puzzle.tcl
~~~~

![alt text](screenshots/puzzle.PNG)

### Tetris

Classic tetris game. Play using arrows, type `q` to quit and `F2` to restart game.

~~~~
$ cwsh demos/tetris.tcl
~~~~

![alt text](screenshots/tetris.png)

### Minesweeper

Classic minesweeper game with 10 bombs on a 10x10 grid. Can be played using keyboard or mouse.
Press key 'h' once game is started.

~~~~
$ cwsh demos/minesweeper.tcl
~~~~

![alt text](screenshots/minesweeper-1.png)

### Montana

~~~~
$ cwsh demos/montana.tcl
~~~~

![alt text](screenshots/montana-1.png)
