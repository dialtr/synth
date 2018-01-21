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
#define CHAN_STATUS_MASK     0x80  // Leading bit pattern for channel status

#define CHAN_TYPE_MASK       0xf0  // Mask for extracting channel msg type
#define CHAN_MASK            0x0f  // Mask for extracting channel number


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
 * Define channel voice message types. These
 */
#define CHAN_NOTE_OFF
#define CHAN_NOTE_ON
#define CHAN_POLY_KEY
#define CHAN_CONTROL_CHANGE


/**
 * Globals that hold current MIDI state.
 */

static char g_status_byte = 0;
static char g_data_byte_one = 0;
static char g_data_byte_two = 0;
static char g_data_byte_counter = 0;

static unsigned long g_event_counter = 0;


/*
 * Callback table
 */

// The null event callback is used by default for all events.
static void null_event_cb(char a, char b) {
    // Just implement the event counter.
    ++g_event_counter;
}


static midi_event_callback_t invoke_sys_realtime_timing_clock = null_event_cb;
static midi_event_callback_t invoke_sys_realtime_reserved_f9 = null_event_cb;
static midi_event_callback_t invoke_sys_realtime_seq_start = null_event_cb;
static midi_event_callback_t invoke_sys_realtime_seq_continue = null_event_cb;
static midi_event_callback_t invoke_sys_realtime_seq_stop = null_event_cb;
static midi_event_callback_t invoke_sys_realtime_reserved_fd = null_event_cb;
static midi_event_callback_t invoke_sys_realtime_active_sense = null_event_cb;
static midi_event_callback_t invoke_sys_realtime_reset = null_event_cb;


/*
 * Internal APIs
 */

// Process a "system real-time" message, which has no data bytes.
static status_t rx_status_sys_realtime_byte(char byte) {
    g_status_byte = byte;
    switch (byte) {
        case SYS_REALTIME_TIMING_CLOCK:
            invoke_sys_realtime_timing_clock(0, 0);
            break;
            
        case SYS_REALTIME_RESERVED_F9:
            invoke_sys_realtime_reserved_f9(0, 0);
            break;
            
        case SYS_REALTIME_SEQ_START:
            invoke_sys_realtime_seq_start(0 ,0);
            break;
            
        case SYS_REALTIME_SEQ_CONTINUE:
            invoke_sys_realtime_seq_continue(0, 0);
            break;
            
        case SYS_REALTIME_SEQ_STOP:
            invoke_sys_realtime_seq_stop(0, 0);
            break;
            
        case SYS_REALTIME_RESERVED_FD:
            invoke_sys_realtime_reserved_fd(0, 0);
            break;
            
        case SYS_REALTIME_ACTIVE_SENSE:
            invoke_sys_realtime_active_sense(0, 0);
            break;
            
        case SYS_REALTIME_RESET:
            invoke_sys_realtime_reset(0, 0);
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
    const char type = (byte & CHAN_TYPE_MASK);
    g_status_byte = byte;    
    
    switch (type) {
        
    }
    
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
    // In most cases, we will want to return 0 indicating success.
    status_t status = 0;
    
    // To clear a callback, the callers specifies NULL for the callback
    // pointer. However, we really want to restore the null_event_cb
    // function so that dummy events are
    if (cb == 0) {
        // Restore the handler in the table to the null event callback.
        cb = null_event_cb;
    }
    
    switch (evt) {
        case EVT_SYS_REALTIME_TIMING_CLOCK:
            invoke_sys_realtime_timing_clock = cb;
            break;
            
        case EVT_SYS_REALTIME_RESERVED_F9:
            invoke_sys_realtime_reserved_f9 = cb;
            break;
            
        case EVT_SYS_REALTIME_SEQ_START:
            invoke_sys_realtime_seq_start = cb;
            break;
            
        case EVT_SYS_REALTIME_SEQ_CONTINUE:
            invoke_sys_realtime_seq_continue = cb;
            break;
            
        case EVT_SYS_REALTIME_SEQ_STOP:
            invoke_sys_realtime_seq_stop = cb;
            break;
            
        case EVT_SYS_REALTIME_RESERVED_FD:
            invoke_sys_realtime_reserved_fd = cb;
            break;
            
        case EVT_SYS_REALTIME_ACTIVE_SENSE:
            invoke_sys_realtime_active_sense = cb;
            break;
            
        case EVT_SYS_REALTIME_RESET:
            invoke_sys_realtime_reset = cb;
            break;
            
        default:
            return E_MIDI_BAD_EVENT_HANDLER;
    }
    
    return status;    
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
    } else if (byte & CHAN_STATUS_MASK) {
        // The byte is a channel voice or channel mode status byte.
        return rx_status_channel_byte(byte);
    } else {
        // The byte is a regular data byte.
        return rx_data_byte(byte);
    }
}
