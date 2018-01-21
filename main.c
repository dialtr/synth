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


// Report error state using a system peripheral
void error(status_t c) {
    PORTB = c;
    PORTDbits.RD0 = 1;
    // TODO(tdial): Implement
    for (;;);
}


void on_midi_active_sensing(char, char) {
    PORTDbits.RD1 = 1;
    for (int i = 0; i < 32; ++i) {
        Nop();
    }
    
    PORTDbits.RD1 = 0;
}


// Configure I/O pins used in the system, set them to their initial state.
status_t iopins_init() {
    TRISB = 0;
    PORTB = 0;
    
    TRISD = 0;
    PORTD = 0;
    
    return 0;
}

// Perform initial system initialization.
status_t system_init() {
    status_t status = 0;
    
    // Configure and initialize I/O pins used in the system.
    status = iopins_init();
    if (status) {
        return status;
    }
    
    // Initialize the USART for receive / transmit.
    status = ioport_init(MIDI_BAUD_RATE);
    if (status) {
        return status;
    }
    
    // Initialize MIDI library.
    status = midi_init();
    if (status) {
        return status;
    }

    status = midi_register_event_handler(EVT_SYS_REALTIME_ACTIVE_SENSE,
                                         on_midi_active_sensing);
    if (status) {
        return status;
    }
    
    return 0;
}



// Main event handler and dispatcher; runs continuously.
void loop() {
    char byte = 0;
    if (ioport_data_ready()) {
        byte = ioport_read();
        midi_receive_byte(byte);
    }
}

// Entry Point
void main(void) {
    status_t status = system_init();
    if (status) {
        error(status);
    } 
    
    for (;;) {
        loop();
    }    
}
