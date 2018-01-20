/**
 * Copyright (C) 2018 Thomas R. Dial
 * All Rights Reserved
 * 
 * This file defines the entry point to the program. Like most embedded
 * software, the control flow of the program involves performing one-
 * time system initialization followed by executing an endless loop that
 * manages incoming events.
 * 
 */
#include <xc.h>
#include "config.h"
#include "ioport.h"
#include "midi.h"
#include "status.h"

// Here, we are configuring various settings on the PIC18. The most important
// setting to note here is 'OSC', which we set to 'HS'. This configures the
// PIC18 to use an external, high-speed crystal oscillator. The frequency of
// the oscillator used in this system is 8 MHZ, and we must define this value
// below in _XTAL_FREQ as this symbol is used in other calculations.

#pragma config OSC = HS
#pragma config MCLRE = OFF
#pragma config PWRT = OFF
#pragma config PBADEN = OFF
#pragma config WDT = OFF
#pragma config DEBUG = OFF
#pragma config LVP = OFF
#pragma config BOREN = 0



// Perform initial system initialization.
status_t init() {
    // TODO(tdial): Implement
    return 0;
}

// Report error state using a system peripheral
void error() {
    // TODO(tdial): Implement
    for (;;);
}

// Main event handler and dispatcher; runs continuously.
void loop() {
    // TODO(tdial): Implement
}

// Entry Point
void main(void) {
    char status = init();
    if (status) {
        error();
    } 
    
    for (;;) {
        loop();
    }    
}
