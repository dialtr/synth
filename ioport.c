/**
 * Copyright (C) 2018 Thomas R. Dial
 * All Rights Reserved
 * 
 * Routines for initializing and reading/writing serial I/O.
 */
#include "ioport.h"
#include <xc.h>
#include "config.h"


status_t ioport_init(unsigned long int baudrate) {
    unsigned long int x = 0;
    x = (_XTAL_FREQ - (baudrate * 64)) / (baudrate * 64);
    if (x > 255) {
        x = (_XTAL_FREQ - (baudrate * 16)) / (baudrate * 16);
        BRGH = 1;
    }
    
    if (x < 256) {
        SPBRG = x;   
        SYNC = 0;    // Asynchronous mode (clear sync bit to zero.)
        SPEN = 1;    // Enable serial communication.
        TRISC7 = 1;  // Configure as RX pin (serial data receive)
        TRISC6 = 1;  // Configure as TX pin (serial data transmit)
        CREN = 1;    // Enable reception
        TXEN = 1;    // Enable transmission
        return 0;    // Return success
    }
    
    return E_IOPORT_INVALID_BAUDRATE;  // Return failure
}


char ioport_data_ready() {
    return RCIF;
}


char ioport_read() {
    while (!RCIF);
    return RCREG;
}