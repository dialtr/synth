/**
 * Copyright (C) 2018 Thomas R. Dial
 * All Rights Reserved
 * 
 * Routines for initialization and communicating with an Intel 8254 Timer.
 */
#ifndef INTEL8254_H_INCLUDED_
#define INTEL8254_H_INCLUDED_

#include "status.h"

// Initialize the Intel 8254 timer to a known state.
status_t intel_8254_init();

// Load a new divisor into timer zero for square-wave clock generation.
//void intel_8254_set_timer0(unsigned char lsb, unsigned char msb);

#define NOPWAIT() Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); Nop();\
                  Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); Nop();\
                  Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); Nop()/*
                  Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); Nop();*/


#define INTEL_8254_TRANSFER_DATA()\
  PORTDbits.RD2 = 0;\
  NOPWAIT();\
  PORTDbits.RD3 = 0;\
  NOPWAIT();\
  NOPWAIT();\
  PORTDbits.RD3 = 1;\
  NOPWAIT();\
  PORTDbits.RD2 = 1;\
  NOPWAIT()



unsigned char g_last_lsbs[3] = {0, 0, 0};
unsigned char g_last_msbs[3] = {0, 0, 0};

#define INTEL_8254_WRITE_TIMER(t_id, t_lsb, t_msb)\
  do {\
    const unsigned char id_ = t_id & 0x3;\
    if (id_ == 3) break;\
    const unsigned char modebits_ = 0b00000110;\
    unsigned char rw_ = 0;\
    if (t_lsb != g_last_lsbs[id_]) { rw_ |= 0x1; g_last_lsbs[id_] = t_lsb; }\
    if (t_msb != g_last_msbs[id_]) { rw_ |= 0x2; g_last_msbs[id_] = t_msb; }\
    if (rw_ == 0) break;\
    unsigned ctrl_word_ = (id_ << 6) | (rw_ << 4) | modebits_;\
    PORTDbits.RD4 = 1;\
    PORTDbits.RD5 = 1;\
    PORTB = ctrl_word_;\
    INTEL_8254_TRANSFER_DATA();\
    switch (id_) {\
      case 0:\
        PORTDbits.RD4 = 0;\
        PORTDbits.RD5 = 0;\
        break;\
      case 1:\
        PORTDbits.RD4 = 0;\
        PORTDbits.RD5 = 1;\
        break;\
      case 2:\
        PORTDbits.RD4 = 1;\
        PORTDbits.RD5 = 0;\
        break;\
      }\
      if (rw_ & 0x1) {\
        PORTB = t_lsb;\
        INTEL_8254_TRANSFER_DATA();\
      }\
      if (rw_ & 0x2) {\
        PORTB = t_msb;\
        INTEL_8254_TRANSFER_DATA();\
      }\
  } while(0)

#define INTEL_8254_WRITE_TIMER0(t_id, t_lsb, t_msb)\
  INTEL_8254_WRITE_TIMER(0, t_lsb, t_msb)

#endif  // INTEL8254_H_INCLUDED_