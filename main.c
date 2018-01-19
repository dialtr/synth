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

// Perform initial system initialization.
char init() {
    // TODO(tdial): Implement
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
    return;
}
