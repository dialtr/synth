/**
 * Copyright (C) 2018 Thomas R. Dial
 * All Rights Reserved
 * 
 * A MIDI processing library designed for use in embedded environments.
 */
#include "midi.h"
#include <xc.h>

/*
 * Masks / patterns that define leading high bits; these are used to 
 * determine what type of status message has been received.
 */

#define SYS_REALTIME_MASK    0xf8
#define SYS_COMMON_MASK      0xf0
#define CHAN_MASK            0x80


/**
 * System real-time status message types.
 */

#define SYS_REALTIME_TIMING_CLOCK  0xf8  // Timing clock sent 24 times/quarter.
#define SYS_REALTIME_RESERVED_F9   0xf9  // Undefined (Reserved.)
#define SYS_REALTIME_SEQ_START     0xfa  // Start the current sequence.
#define SYS_REALTIME_SEQ_CONTINUE  0xfb  // Continue the current sequence.
#define SYS_REALTIME_SEQ_STOP      0xfc  // Stop the current sequence.
#define SYS_REALTIME_RESERVED_FD   0xfd  // Undefined (Reserved.)
#define SYS_REALTIME_ACTIVE_SENSE  0xfe  // Active sensing message (each 300ms.)
#define SYS_REALTIME_RESET         0xff  // Reset all receivers to power-up.


/**
 * Globals that hold current MIDI state.
 */

static char g_status_byte = 0;
static char g_data_byte_one = 0;
static char g_data_byte_two = 0;
static char g_data_byte_counter = 0;


/*
 * Internal APIs
 */

// Process a "system real-time" message, which has no data bytes.
static status_t rx_status_sys_realtime_byte(char byte) {
    g_status_byte = byte;
    switch (byte) {
        case SYS_REALTIME_TIMING_CLOCK:
            break;
        case SYS_REALTIME_RESERVED_F9:            
            break;
        case SYS_REALTIME_SEQ_START:
            break;
        case SYS_REALTIME_SEQ_CONTINUE:
            break;
        case SYS_REALTIME_SEQ_STOP:
            break;
        case SYS_REALTIME_RESERVED_FD:
            break;
        case SYS_REALTIME_ACTIVE_SENSE:
            break;
        case SYS_REALTIME_RESET:
            break;
    }
    return 1;
}

// Process a "system common" status byte (0 or more data bytes follow.)
static status_t rx_status_sys_common_byte(char byte) {
    g_status_byte = byte;    
    return 0;
}

// Process a "channel" status byte. (1 or 2 data bytes follow.)
static status_t rx_status_channel_byte(char byte) {
    g_status_byte = byte;    
    return 0;
}

// Process a trailing data byte.
static status_t rx_data_byte(char byte) {
    return 0;
}

/*
 * Public APIs 
 */

status_t midi_init() {
    return 0;
}

status_t midi_register_event_handler(event_type evt, midi_event_callback_t cb) {
    return 0;    
}

status_t midi_receive_byte(char byte) {
    /*
     * The statements below, which are performed in deliberate order, determine
     * which type of byte has arrived on the input. First, we test the lead
     * bits to see if they match the expected mask for a system real-time
     * status byte. Next, we check for a system common status byte, and then
     * for a channel status byte, which could either be a voice or mode type.
     * 
     * If it is determined that the byte is not any type of status byte, then
     * by process of elimination, it must be a data byte.
     */
    
    if ((byte & SYS_REALTIME_MASK) == SYS_REALTIME_MASK) {
        // The byte is a system real-time status byte.
        return rx_status_sys_realtime_byte(byte);
    } else if ((byte & SYS_COMMON_MASK) == SYS_COMMON_MASK) {
        // The byte is a system common status byte.
        return rx_status_sys_common_byte(byte);
    } else if (byte & CHAN_MASK) {
        // The byte is a channel voice or channel mode status byte.
        return rx_status_channel_byte(byte);
    } else {
        // The byte is a regular data byte.
        return rx_data_byte(byte);
    }
} 