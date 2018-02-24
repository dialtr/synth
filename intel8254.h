/**
 * Copyright (C) 2018 Thomas R. Dial
 * All Rights Reserved
 * 
 * Routines for initialization and communicating with an Intel 8254 Timer.
 */
#ifndef INTEL8254_H_INCLUDED_
#define INTEL8254_H_INCLUDED_

#include "status.h"
#include <xc.h>

// Initialize the Intel 8254 timer to a known state.
status_t intel_8254_init();

// Load a new divisor into timer zero for square-wave clock generation.
//void intel_8254_set_timer0(unsigned char lsb, unsigned char msb);

//#define NOPWAIT() Nop(); Nop(); Nop(); Nop(); Nop();

// Below for 16mhz Master Clock
// TODO(tdial): Ensure delays are long enough for the 8254.)
// This defines the delay in cycles needed between operations on the 8254.
// I suspect that when I moved to 16mhz I didn't do the calculations correctly.
// This delay must be verified against the data sheet.
#define NOPWAIT() Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); Nop()


inline void intel_write_timer(unsigned char timer,
                              unsigned char lsb,
                              unsigned char msb);


#endif  // INTEL8254_H_INCLUDED_