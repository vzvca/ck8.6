/*
 * --------------------------------------------------------------------------
 *  When keypad() is TRUE.
 *  ncurses reports arrows with modifiers with weird undocumented KEY_.
 *  get_wch(&w) will return KEY_CODE_YES and will get one of the numeric
 *  constant below.
 * --------------------------------------------------------------------------
 */

CK_ARROW(CTRL_UP,    566, "\033[1;5A")
CK_ARROW(CTRL_DOWN,  525, "\033[1;5B")
CK_ARROW(CTRL_RIGHT, 560, "\033[1;5C")
CK_ARROW(CTRL_LEFT,  545, "\033[1;5D")

CK_ARROW(ALT_UP,     564, "\033[1;3A")
CK_ARROW(ALT_DOWN,   523, "\033[1;3B")
CK_ARROW(ALT_RIGHT,  558, "\033[1;3C")
CK_ARROW(ALT_LEFT,   543, "\033[1;3D")

CK_ARROW(SHIFT_UP,   337, "\033[1;2A")
CK_ARROW(SHIFT_DOWN, 336, "\033[1;2B")
CK_ARROW(SHIFT_RIGHT, 402, "\033[1;2C")
CK_ARROW(SHIFT_LEFT, 393, "\033[1;2D")

CK_ARROW(CTRL_SHIFT_UP,   567, "\033[1;6A")
CK_ARROW(CTRL_SHIFT_DOWN, 526, "\033[1;6B")
CK_ARROW(CTRL_SHIFT_RIGHT,561, "\033[1;6C")
CK_ARROW(CTRL_SHIFT_LEFT, 546, "\033[1;6D")


