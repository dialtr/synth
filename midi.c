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
 * Enumeration that represents the states that the MIDI protocol
 * state machine can be in.
 */
enum PROTOCOL_STATE {
    // The machine is waiting for a status byte.
    STATE_WAITING_FOR_STATUS = 0,
    STATE_ERROR = 1,
            
    // Note Off: waiting for key and velocity data bytes, respectively.
    STATE_WAITING_CHAN_NOTE_OFF_KEY,
    STATE_WAITING_CHAN_NOTE_OFF_VELOCITY,
    
    // Note On: waiting for key and velocity data bytes, respectively.
    STATE_WAITING_CHAN_NOTE_ON_KEY,      
    STATE_WAITING_CHAN_NOTE_ON_VELOCITY,
    
    // Poly After-touch: waiting for key and pressure value, respectively.
    STATE_WAITING_CHAN_POLY_AFTERTOUCH_KEY,
    STATE_WAITING_CHAN_POLY_AFTERTOUCH_PRESSURE,
    
    // Channel control change: waiting for control and value data bytes.
    // Note: depending on these values, messages of this type may be
    // considered to be "channel mode" messages.
    STATE_WAITING_CHAN_CONTROL_CHANGE_CONTROL,       
    STATE_WAITING_CHAN_CONTROL_CHANGE_VALUE,       
    
    // Program change: waiting for the program data byte.
    STATE_WAITING_CHAN_PROGRAM_CHANGE_PROGRAM,  
            
    // Channel After-touch - waiting for the pressure data byte.
    STATE_WAITING_CHAN_AFTERTOUCH_PRESSURE,          
            
    // Pitch Bend: waiting for two bytes representing the bend amount.
    // The least-significant 7 bits are sent first. The most significant
    // bits are set second.
    STATE_WAITING_CHAN_PITCH_BEND_LSBITS,
    STATE_WAITING_CHAN_PITCH_BEND_MSBITS 
};

/**
 * Globals that hold current MIDI state.
 */

static char g_debug_last_status_byte = 0;
static char g_debug_last_data_byte = 0;

// The MIDI channel associated with the current state.
static char g_current_channel = 0;
static char g_data_byte_one = 0;
static char g_data_byte_two = 0;
static char g_data_byte_counter = 0;

static unsigned long g_event_counter = 0;

// State of the "MIDI protocol state machine"
static char g_state = STATE_WAITING_FOR_STATUS;


// The null event callback is used by default for all events.
static void null_event_cb(char channel, char a, char b) {
    // Just implement the event counter.
    ++g_event_counter;
}


// Callback table.
static midi_event_callback_t g_callbacks[EVT_MAX] = {0};


// Wrapper that invokes callback functions.
static inline void invoke_callback(int evt) {
    // Reject invalid events.
    if ((evt < 0) || (evt >= EVT_MAX)) {
        return;
    }
    
    // Increment the global event counter.
    ++g_event_counter;
    
    // Invoke the callback.
    (g_callbacks[evt])(g_current_channel, g_data_byte_one, g_data_byte_two);
    
    // Clear data state
    g_data_byte_one = 0;
    g_data_byte_two = 0;
}


/*
 * Internal APIs
 */

/**
 * Process a "system real-time" message, which has no data bytes.
 * 
 * Real-time status bytes may arrive at any time. In fact, according to the
 * MIDI specification, real-time status bytes can be interleaved with bytes
 * that comprise other, lower priority messages.
 * 
 * For this reason, the rx_status_sys_real-time_byte() handler does NOT update
 * much state; messages of this type should conceptually just be handled
 * and then processing of the MIDI byte stream should continue as though the
 * real-time byte was never received.
 */
static status_t rx_status_sys_realtime_byte(char byte) {
    switch (byte) {
        case SYS_REALTIME_TIMING_CLOCK:
            invoke_callback(EVT_SYS_REALTIME_TIMING_CLOCK);
            break;
            
        case SYS_REALTIME_RESERVED_F9:
            invoke_callback(EVT_SYS_REALTIME_RESERVED_F9);
            break;
            
        case SYS_REALTIME_SEQ_START:
            invoke_callback(EVT_SYS_REALTIME_SEQ_START);
            break;
            
        case SYS_REALTIME_SEQ_CONTINUE:
            invoke_callback(EVT_SYS_REALTIME_SEQ_CONTINUE);
            break;
            
        case SYS_REALTIME_SEQ_STOP:
            invoke_callback(EVT_SYS_REALTIME_SEQ_STOP);
            break;
            
        case SYS_REALTIME_RESERVED_FD:
            invoke_callback(EVT_SYS_REALTIME_RESERVED_FD);
            break;
            
        case SYS_REALTIME_ACTIVE_SENSE:
            invoke_callback(EVT_SYS_REALTIME_ACTIVE_SENSE);
            break;
            
        case SYS_REALTIME_RESET:
            invoke_callback(EVT_SYS_REALTIME_RESET);
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
            g_state = STATE_WAITING_CHAN_NOTE_OFF_KEY;
            break;
            
        case CHAN_NOTE_ON:
            g_state = STATE_WAITING_CHAN_NOTE_ON_KEY;
            break;
            
        case CHAN_POLY_AFTER_TOUCH:
            g_state = STATE_WAITING_CHAN_POLY_AFTERTOUCH_KEY;
            break;
            
        case CHAN_CONTROL_CHANGE:
            g_state = STATE_WAITING_CHAN_CONTROL_CHANGE_CONTROL;
            break;
            
        case CHAN_PROGRAM_CHANGE:
            g_state = STATE_WAITING_CHAN_PROGRAM_CHANGE_PROGRAM;
            break;
            
        case CHAN_AFTER_TOUCH:
            g_state = STATE_WAITING_CHAN_AFTERTOUCH_PRESSURE;
            break;
            
        case CHAN_PITCH_BEND:
            g_state = STATE_WAITING_CHAN_PITCH_BEND_LSBITS;
            break;
            
        default:
            g_state = STATE_ERROR;
            return E_MIDI_BAD_CHANNEL_STATE;
    }
    return 0;
}


// Process a trailing data byte.
static status_t rx_data_byte(char byte) {
    switch (g_state) {
        // Process first byte of a "note off" message.
        case STATE_WAITING_CHAN_NOTE_OFF_KEY:
            g_data_byte_one = byte;
            g_state = STATE_WAITING_CHAN_NOTE_OFF_VELOCITY;
            break;
            
        // Process second byte of a "note off" message, and invoke callback.
        case STATE_WAITING_CHAN_NOTE_OFF_VELOCITY:
            g_data_byte_two = byte;
            invoke_callback(EVT_CHAN_NOTE_OFF);
            // TODO(tdial): Update state? we could get another one of these..
            return 1;
            
        // Process first byte of a "note on" message.
        case STATE_WAITING_CHAN_NOTE_ON_KEY:
            g_data_byte_one = byte;
            g_state = STATE_WAITING_CHAN_NOTE_ON_VELOCITY;
            break;
            
        // Process second byte of a "note on" message, and invoke callback.
        case STATE_WAITING_CHAN_NOTE_ON_VELOCITY:
            g_data_byte_two = byte;
            invoke_callback(EVT_CHAN_NOTE_ON);
            // TODO(tdial): Update state? we could get another one of these..
            return 1;
            
        // Process first byte of a poly after-touch message.    
        case STATE_WAITING_CHAN_POLY_AFTERTOUCH_KEY:
            g_data_byte_one = byte;
            g_state = STATE_WAITING_CHAN_POLY_AFTERTOUCH_PRESSURE;
            break;
        
        // Process second byte of a poly after-touch message, invoke callback.
        case STATE_WAITING_CHAN_POLY_AFTERTOUCH_PRESSURE:
            g_data_byte_two = byte;
            invoke_callback(EVT_CHAN_POLY_AFTERTOUCH);
            return 1;
        
        // Process first byte of a channel control change message.
        case STATE_WAITING_CHAN_CONTROL_CHANGE_CONTROL:
            g_data_byte_one = byte;
            g_state = STATE_WAITING_CHAN_CONTROL_CHANGE_VALUE;
            break;
        
        // Process second byte of a channel control change, invoke callback.    
        case STATE_WAITING_CHAN_CONTROL_CHANGE_VALUE:    
            g_data_byte_two = byte;
            invoke_callback(EVT_CHAN_CONTROL_CHANGE);
            return 1;
            
        // Process program change, invoke callback.
        case STATE_WAITING_CHAN_PROGRAM_CHANGE_PROGRAM:
            g_data_byte_one = byte;
            g_data_byte_two = 0;
            invoke_callback(EVT_CHAN_PROGRAM_CHANGE);
            return 1;
        
        // Process channel after-touch message, invoke callback.     
        case STATE_WAITING_CHAN_AFTERTOUCH_PRESSURE:
            g_data_byte_one = byte;
            g_data_byte_two = 0;
            invoke_callback(EVT_CHAN_AFTERTOUCH);
            return 1;
        
        // Process first byte of pitch bend.    
        case STATE_WAITING_CHAN_PITCH_BEND_LSBITS:
            g_data_byte_one = byte;
            g_state = STATE_WAITING_CHAN_PITCH_BEND_MSBITS;
            break;
        
        // Process second byte of pitch bend.    
        case STATE_WAITING_CHAN_PITCH_BEND_MSBITS:
            g_data_byte_two = byte;
            invoke_callback(EVT_CHAN_PITCH_BEND);
            return 1;
        
        // Handle bad state.    
        default:
            g_data_byte_one = 0;
            g_data_byte_two = 0;
            // What to do?
            break;
    }
    
    // No messages processed; return 0.
    return 0;
}


/*
 * Public APIs 
 */


status_t midi_init() {
    // Initialize the callback table; all events to the null callback.
    for (int i = 0; i < EVT_MAX; ++i) {
        g_callbacks[i] = null_event_cb;
    }
    return 0;
}


status_t midi_register_event_handler(event_type evt, midi_event_callback_t cb) {
    if (cb) {
        g_callbacks[evt] = cb;
    } else {
        g_callbacks[evt] = null_event_cb;
    }
    
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
