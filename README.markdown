Arduino Tetris
==============

Hardware
--------

* Arduino
* 128 x 96 LCD from Sparkfun: http://www.sparkfun.com/commerce/product_info.php?products_id=8844
* Four standard push buttons

Method
------

A 10x20 char grid represents the game grid. A struct that holds four two-byte Positions represents
the piece in play. The code then loops through, deciding whether to apply gravity each loop based
on how long it was since gravity was previously applied, then checking for a collision if so.
If a collision happened the current piece is saved to the grid and a new one is generated. The
grid is checked for newly completed lines and if any are found they're cleared and the player
gets a point. In the meantime, the four buttons are on the pin change interrupt, and drive the
move_left, move_right, rotate and drop functions. Interrupts are disabled each time one happens
and reenabled after drawing each frame to help stop accidental repeats.
At the end of the game, when a block is placed in the top row, the grid is filled with red and then
a number of squares are lit up green to represent the player's score. This is problematic with a
score of over 200, but that's a pretty unlikely edge case.

Bugs
----

The only known bug at the moment is that the random number generator keeps spitting out the same random
numbers and I have no idea what to do about it. This means you always get the same blocks, and some blocks
never come up. I know, right? weeeeird.