/**
 * Copyright (C) 2018 Thomas R. Dial
 * All Rights Reserved
 * 
 * A MIDI processing library designed for use in embedded environments.
 */
#ifndef MIDI_H_INCLUDED_
#define MIDI_H_INCLUDED_

#include "status.h"

// The baud rate for MIDI data
#define MIDI_BAUD_RATE  31250

#define SYS_REALTIME_TIMING_CLOCK  0xf8  // Timing clock sent 24 times/quarter.
#define SYS_REALTIME_RESERVED_F9   0xf9  // Undefined (Reserved.)
#define SYS_REALTIME_SEQ_START     0xfa  // Start the current sequence.
#define SYS_REALTIME_SEQ_CONTINUE  0xfb  // Continue the current sequence.
#define SYS_REALTIME_SEQ_STOP      0xfc  // Stop the current sequence.
#define SYS_REALTIME_RESERVED_FD   0xfd  // Undefined (Reserved.)
#define SYS_REALTIME_ACTIVE_SENSE  0xfe  // Active sensing message (each 300ms.)
#define SYS_REALTIME_RESET         0xff  // Reset all receivers to power-up.


/*
 * The midi_event_callback_t type defines a type for event callback functions
 * to be invoked on the receipt of MIDI messages. The caller only needs to
 * register functions for those events for which there is interest.
 * Unhandled events will be dispatched to a null handler implemented within
 * the library.
 */
typedef void (*midi_event_callback_t)(char, char);

typedef enum midi_errors {
    E_MIDI_BAD_EVENT_HANDLER = -1
};

typedef enum event_type {
    EVT_SYS_REALTIME_TIMING_CLOCK = 1,
    EVT_SYS_REALTIME_RESERVED_F9 = 2,
    EVT_SYS_REALTIME_SEQ_START = 3,
    EVT_SYS_REALTIME_SEQ_CONTINUE = 4,
    EVT_SYS_REALTIME_SEQ_STOP = 5,
    EVT_SYS_REALTIME_RESERVED_FD = 6,
    EVT_SYS_REALTIME_ACTIVE_SENSE = 7,
    EVT_SYS_REALTIME_RESET = 8
} event_type;


/**
 * Initialize the MIDI library. This routine must be called prior to using
 * library functions or unpredictable behavior may result.
 * 
 * @return Zero on success; nonzero status otherwise.
 */
status_t midi_init();


/**
 * Register an event handler for the specified event. To clear an event
 * handle, simply pass a NULL pointer for the callback argument.
 * 
 * Returns  0 if a callback was registered or cleare successfully.
 * Returns -1 if the event type was invalid.
 * 
 * @param evt Event to register. See the EVT_xxx enumeration above.
 * @param cb Function to invoke upon receipt of the MIDI event.
 * @return Status code indicating (see above for comments.)
 */
status_t midi_register_event_handler(event_type evt, midi_event_callback_t cb);


/**
 * Processes a byte arriving via the MIDI input of the device. This function
 * *must* be invoked for every byte that arrives so that the internal state
 * machine maintained by the library is updated properly. Calling this function
 * may result in callback invocations. On success, the function returns the
 * number of callbacks that resulted from the new byte's arrival. In the event
 * that the received byte is invalid, a negative code is returned (see the
 * section above that outlines status codes for this library.)
 * 
 * @param byte Byte of data received from an input port.
 * 
 * @return Number of callback invocations. Returns negative status on error.
 */
status_t midi_receive_byte(char byte);


#endif  // MIDI_H_INCLUDED_