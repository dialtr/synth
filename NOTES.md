Notes:

1. I have modified the XLCD library by adding the source files to the project;
the modifications were made because I needed to use different pins to get
things done on the Hitachi 44780 compatible display. It defaults to use PORTB
but I need it use PORTD.

This will necessitate that the xlcd.h header be modified. Remember to copy this
modified file to the project and change the xlcd source files to include the
local copy rather than the system copy. Then, restore the system copy to its
original default.




