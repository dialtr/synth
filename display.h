#ifndef DISPLAY_H_INCLUDED_
#define DISPLAY_H_INCLUDED_


void display_open(void);

void display_clear(void);

void display_move(unsigned char row, unsigned char col);

void display_enable(void);

void display_disable(void);

void display_write_string(const char* str);


#endif  // DISPLAY_H_INCLUDED_