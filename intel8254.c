/**
 * Copyright (C) 2018 Thomas R. Dial
 * All Rights Reserved
 * 
 * Routines for initialization and communicating with an Intel 8254 Timer.
 */

#include "intel8254.h"
#include <xc.h>
#include <delays.h>
#include "config.h"

#define INTEL_8254_A0 PORTDbits.RD4
#define INTEL_8254_A1 PORTDbits.RD5
#define INTEL_8254_CS PORTDbits.RD6
#define INTEL_8254_WR PORTDbits.RD7


// Initialize the Intel 8254 timer to a known state.
status_t intel_8254_init() {
    // TODO(tdial): I should not be hard-coding things to use PORTB here;
    // and port initialization should be taking place outside of this
    // routine. The way to go probably is to have a configuration header
    // that defines the data port to use as well as the control lines. 
    // This could be put into a configuration header file.
    // Port B is output
    TRISB = 0;

    // Initial Port B lines are zero.
    PORTB = 0;
    
    // Port D is output
    TRISD = 0;
    
    // Initialize the 8254 to a known state.  CS, low, WR high,
    // meaning that the chip is selected but not write enabled.
    // A0, A1 are high, meaning that if a word was written it
    // would go to the control register.
    INTEL_8254_CS = 0; 
    INTEL_8254_WR = 1; 
    INTEL_8254_A0 = 1; 
    INTEL_8254_A1 = 1; 
     
    // Write control word
    for (char timer = 0; timer < 3; ++timer) {
        const unsigned char control_word = (timer << 6) |
                                           (0x3 << 4) |
                                           (0b00000110);
        PORTB = control_word;
        INTEL_8254_A0 = 1; // RD4 on the PIC -> A0 on the 8254
        INTEL_8254_A1 = 1; // RD5 on the PIC -> A1 on the 8254

        // Pulse Write
        INTEL_8254_WR = 0;
        NOPWAIT();
        INTEL_8254_WR = 1;
        
        // Delay before sending next command.
       __delay_ms(10);
    }
      
    return 0;
}

inline void intel_write_timer(unsigned char timer,
                              unsigned char lsb,
                              unsigned char msb) {
    // The timer must be 0, 1 or 2.
    timer &= 0x3;
    if (timer == 3) {
        return;
    }
    
    // Set up address for data words
    switch (timer) {
        case 0:
            INTEL_8254_A0 = 0;
            INTEL_8254_A1 = 0;
            break;
        case 1:
            INTEL_8254_A0 = 1;
            INTEL_8254_A1 = 0;
            break;
        case 2:
            INTEL_8254_A0 = 0;
            INTEL_8254_A1 = 1;
            break;
    }
    
    // Write LSB
    PORTB = lsb;
    INTEL_8254_WR = 0;
    NOPWAIT();
    // TODO(tdial): Figure out actual delays needed.s
    INTEL_8254_WR = 1;
    
    // Write MSB
    PORTB = msb;
    INTEL_8254_WR = 0;
    NOPWAIT();
    INTEL_8254_WR = 1;
    NOPWAIT();
}
