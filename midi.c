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
 * Define channel voice message types. Before the type can be compared with a
 * MIDI channel status byte, the CHAN_TYPE_MASK must be used to mask off the
 * channel bits.
 */
#define CHAN_NOTE_OFF              0x80
#define CHAN_NOTE_ON               0x90
#define CHAN_POLY_AFTER_TOUCH      0xa0
#define CHAN_CONTROL_CHANGE        0xb0  // Could be a CHANNEL MODE message
#define CHAN_PROGRAM_CHANGE        0xc0
#define CHAN_AFTER_TOUCH           0xd0
#define CHAN_PITCH_BEND            0xe0

/**
 * Enumeration that respresents the states that the MIDI protocol
 * state machine can be in.
 */
enum PROTOCOL_STATE {
    // The machine is waiting for a status byte.
    STATE_WAITING_FOR_STATUS = 0,
            
    // Note Off: waiting for key and velocity data bytes, respectively.
    STATE_WAITING_CHAN_NOTE_OFF_KEY = 1,
    STATE_WAITING_CHAN_NOTE_OFF_VELOCITY = 2,
    
    // Note On: waiting for key and velocity data bytes, respectively.
    STATE_WAITING_CHAN_NOTE_ON_KEY = 3,      
    STATE_WAITING_CHAN_NOTE_ON_VELOCITY = 4,
    
    // Poly After-touch: waiting for key and pressure value, respectively.
    STATE_WAITING_CHAN_POLY_AFTER_TOUCH_KEY = 5,
    STATE_WAITING_CHAN_POLY_AFTER_TOUCH_PRESSURE = 6,
    
    // Channel control change: waiting for control and value data bytes.
    // Note: depending on these values, messages of this type may be
    // considered to be "channel mode" messages.
    STATE_WAITING_CHAN_CONTROL_CHANGE_CONTROL = 7,       
    STATE_WAITING_CHAN_CONTROL_CHANGE_VALUE = 8,       
    
    // Program change: waiting for the program data byte.
    STATE_WAITING_CHAN_PROGRAM_CHANGE_PROGRAM = 9,  
            
    // Channel After-touch - waiting for the pressure data byte.
    STATE_WAITING_CHAN_AFTER_TOUCH_PRESSURE = 10,          
            
    // Pitch Bend: waiting for two bytes representing the bend amount.
    // The least-significant 7 bits are sent first. The most significant
    // bits are set second.
    STATE_WAITING_CHAN_PITCH_BEND_LSBITS = 11,
    STATE_WAITING_CHAN_PITCH_BEND_MSBITS = 12

};

/**
 * Globals that hold current MIDI state.
 */

static char g_debug_last_status_byte = 0;
static char g_debug_last_data_byte = 0;

static char g_data_byte_one = 0;
static char g_data_byte_two = 0;
static char g_data_byte_counter = 0;
static unsigned long g_event_counter = 0;

// State of the "MIDI protocol state machine"
static char g_state = STATE_WAITING_FOR_STATUS;

// The MIDI channel associated with the current state.
static char g_current_channel = 0;

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
// Real-time status bytes may arrive at any time. In fact, according to the
// MIDI spec, real-time status bytes can arrive *between* the bytes that
// comprise other 

/**
 * Process a "system real-time" message, which has no data bytes.
 * 
 * Real-time status bytes may arrive at any time. In fact, according to the
 * MIDI specification, real-time status bytes can be interleaved with bytes
 * that comprise other, lower priority messages.
 * 
 * For this reason, the rx_status_sys_realtime_byte() handler does NOT update
 * much state; messages of this type should conceptually just be handled
 * and then processing of the MIDI byte stream should continue as though the
 * real-time byte was never received.
 *  
 */
static status_t rx_status_sys_realtime_byte(char byte) {
    // The only state that we update is the global event counter. This is
    // something that might be useful to record for debugging purposes or
    // perhaps even unit testing.
    ++g_event_counter;
    
    switch (byte) {
        case SYS_REALTIME_TIMING_CLOCK:
            invoke_sys_realtime_timing_clock(0, 0);
            break;
            
        case SYS_REALTIME_RESERVED_F9:
            invoke_sys_realtime_reserved_f9(0, 0);
            break;
            
        case SYS_REALTIME_SEQ_START:
            invoke_sys_realtime_seq_start(0, 0);
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
    
    return 0;
}


// Process a "channel" status byte. (1 or 2 data bytes follow.)
static status_t rx_status_channel_byte(char byte) {
    // Mask of the channel bits, leaving only the message type.
    const char type = (byte & CHAN_TYPE_MASK);
    
    // Update the state machine with the MIDI channel of the message that
    // we are now processing. This is held in a global.
    g_current_channel = (byte & CHAN_MASK);
    
    switch (type) {
        case CHAN_NOTE_OFF:
            break;
        case CHAN_NOTE_ON:
            break;
        case CHAN_POLY_AFTER_TOUCH:
            break;
        case CHAN_CONTROL_CHANGE:
            break;
        case CHAN_PROGRAM_CHANGE:
            break;
        case CHAN_AFTER_TOUCH:
            break;
        case CHAN_PITCH_BEND:
            break;
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
        g_debug_last_status_byte = byte;
        return rx_status_sys_realtime_byte(byte);
    } else if ((byte & SYS_COMMON_MASK) == SYS_COMMON_MASK) {
        // The byte is a system common status byte.
        g_debug_last_status_byte = byte;
        return rx_status_sys_common_byte(byte);
    } else if (byte & CHAN_STATUS_MASK) {
        // The byte is a channel voice or channel mode status byte.
        g_debug_last_status_byte = byte;
        return rx_status_channel_byte(byte);
    } else {
        // The byte is a regular data byte.
        g_debug_last_data_byte = byte;
        return rx_data_byte(byte);
    }
}
