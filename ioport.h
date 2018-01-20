/**
 * Copyright (C) 2018 Thomas R. Dial
 * All Rights Reserved
 * 
 * Routines for initializing and reading/writing serial I/O.
 */
#ifndef IOPORT_H_INCLUDED_
#define IOPORT_H_INCLUDED_

#include "status.h"

typedef enum ioport_errors {
    E_IOPORT_INVALID_BAUDRATE = -1
};

/**
 * Initialize the I/O port.
 * 
 * @param baudrate The baud rate for communication.
 * @return Return zero on success, negative error code otherwise.
 */
status_t ioport_init(unsigned long int baudrate);

/**
 * Return nonzero when there is data available, zero otherwise.
 * 
 * @return Return byte indicating whether data is available.
 */
char ioport_data_ready();


/**
 * Read a byte from the I/O port. Note: this function spins if necessary
 * until a byte is ready. To poll the port to test if there is a byte
 * available, call ioport_data_ready(), above.
 * 
 * @return Return a byte received from USART.
 */
char ioport_read();

#endif  // IOPORT_H_INCLUDED_