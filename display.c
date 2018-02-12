#include "display.h"
#include <xc.h>
#include <plib/xlcd.h>
#include <plib/delays.h>
#include "config.h"


void DelayFor18TCY(void) {
    // This implementation assumes a 16mhz CPU clock.
    Nop(); Nop(); Nop(); Nop();
    Nop(); Nop(); Nop(); Nop();
    Nop(); Nop(); Nop(); Nop();
    Nop(); Nop();

    Nop(); Nop(); Nop(); Nop();
    Nop(); Nop(); Nop(); Nop();
    Nop(); Nop(); Nop(); Nop();
    Nop(); Nop();

    return;
}

void DelayPORXLCD(void) {
    Delay1KTCYx(30);
}

void DelayXLCD(void) {
    Delay1KTCYx(10);
}

void display_open(void) {
    // TODO(tdial): Make sure I'm setting the right options here.
    OpenXLCD(FOUR_BIT & LINES_5X7);
}

void display_clear(void) {
    while (BusyXLCD());
    WriteCmdXLCD(0x01);
}

void display_move(unsigned char row, unsigned char col) {
    // TODO(tdial): I don't think this is right. Need to read the data sheet
    // on the device.
    const char addr = (40 * row) + col;
    while (BusyXLCD());
    SetDDRamAddr(addr);
}

void display_enable(void) {
    while (BusyXLCD());
    WriteCmdXLCD(DON);
}

void display_disable(void) {
    while (BusyXLCD());
    WriteCmdXLCD(DOFF);
}

void display_write_string(const char* str) {
    while (BusyXLCD());
    putrsXLCD(str);
}