/**
 * Copyright (C) 2018 Thomas R. Dial
 * All Rights Reserved
 * 
 * This file defines the entry point to the program. Like most embedded
 * software, the control flow of the program involves performing one-
 * time system initialization followed by executing an endless loop that
 * manages incoming events.
 * 
 */
#include <xc.h>
#include "config.h"
#include "intel8254.h"
#include "ioport.h"
#include "midi.h"
#include "midi_notes.h"
#include "status.h"

// Here, we are configuring various settings on the PIC18. The most important
// setting to note here is 'OSC', which we set to 'HS'. This configures the
// PIC18 to use an external, high-speed crystal oscillator. The frequency of
// the oscillator used in this system is 8 MHZ, and we must define this value
// below in _XTAL_FREQ as this symbol is used in other calculations.

#pragma config OSC = HS
#pragma config MCLRE = OFF
#pragma config PWRT = OFF
#pragma config PBADEN = OFF
#pragma config WDT = OFF
#pragma config DEBUG = OFF
#pragma config LVP = OFF
#pragma config BOREN = 0

static int g_notes_on = 0;

static long int g_base_freq = 20000;
static long int g_pitch_bend = 8192; 
static char g_update_frequency = 0;

void on_note_off() {
    --g_notes_on;
    if (g_notes_on <= 0) {
        intel_write_timer(0, 1, 0);
        g_notes_on = 0;
    }
}

// Report error state using a system peripheral
void error(status_t c) {
    PORTDbits.RD0 = 1;
    // TODO(tdial): Implement
    for (;;);
}


void on_midi_active_sensing(char, char, char) {
    // TODO(tdial): Update event watchdog timer; if we don't get any
    // event or active sensing message in some period of time, we will
    // want to silence all oscillators. 
}


void on_midi_note_off(char chan, char key, char val) {
    on_note_off();
    PORTDbits.RD0 = 0;
}

// This is really OSC / 65535
#define MIN_FREQ 64

long calc_oscillator_frequency(long base_freq, long bender) {
    long new_freq = 0;
    if (bender < (8192 - 20)) {
        new_freq = (base_freq * (bender + 8192)) / 16384;
        if (new_freq < MIN_FREQ) {
            new_freq = MIN_FREQ;
        }
    } else if (bender > (8192 + 20)) {
        new_freq = (base_freq * bender) / 8192;
        if (new_freq > 20000) {
            new_freq = 20000;
        }
    } else {
        return base_freq;
    }
    return new_freq;
}


void set_oscillator_frequency(long freq) {
    const unsigned long clock = 2000000;
    const unsigned long divisor = clock / freq;
    const char lsb = (unsigned char) (divisor & 0xff);
    const char msb = (unsigned char) ((divisor >> 8) & 0xff);
    intel_write_timer(0, lsb, msb);
}


void on_midi_note_on(char chan, char key, char vel) {
    // Instruments sometimes send "note off" messages as note on messages
    // with a velocity of zero. Check here for that condition and delegate
    // to the handler for note off.
    if (vel == 0) {
        on_midi_note_off(chan, key, vel);
        return;
    }
    
    ++g_notes_on;
    
    char lsb = 0;
    char msb = 0;
    
    // Given the MIDI note, which frequency should we be playing?
    // Calculate the new global frequency and store it there.
    g_base_freq = midi_note_frequency_for_note(key);
    
    const long freq = calc_oscillator_frequency(g_base_freq, g_pitch_bend);
    
    // Now, set the oscillator frequency, which uses the global frequency.
    // as well as the current pitch bend value.
    set_oscillator_frequency(freq);
    
    PORTDbits.RD0 = 0;
}

// Configure I/O pins used in the system, set them to their initial state.
status_t iopins_init() {
    TRISB = 0;
    PORTB = 0;
    
    TRISD = 0;
    PORTD = 0;
    
    return 0;
}


void on_pitch_bend(char chan, char lsb, char msb) {
    lsb &= 0x7f;
    msb &= 0x7f;
    long pitch_bend = msb;
    pitch_bend <<= 7;
    pitch_bend |= lsb;
    g_pitch_bend = pitch_bend;
    g_update_frequency = 1;
}

// Perform initial system initialization.
status_t system_init() {
    status_t status = 0;
    
    // Configure and initialize I/O pins used in the system.
    status = iopins_init();
    if (status) {
        return status;
    }
    
    // Initialize the USART for receive / transmit.
    status = ioport_init(MIDI_BAUD_RATE);
    if (status) {
        return status;
    }
    
    // Initialize the Intel 8254 Timer 
    status = intel_8254_init();
    if (status) {
        return status;
    }
    
    // Initialize MIDI library.
    status = midi_init();
    if (status) {
        return status;
    }
    
    status = midi_register_event_handler(EVT_SYS_REALTIME_ACTIVE_SENSE,
                                         on_midi_active_sensing);

    status = midi_register_event_handler(EVT_CHAN_NOTE_OFF,
                                         on_midi_note_off);

    status = midi_register_event_handler(EVT_CHAN_NOTE_ON,
                                         on_midi_note_on);
    
    status = midi_register_event_handler(EVT_CHAN_PITCH_BEND,
                                         on_pitch_bend);
    
    // TODO(tdial): Eliminate
    on_midi_note_off(0, 0, 0);
    
    if (status) {
        return status;
    }
    
    return 0;
}


// Main event handler and dispatcher; runs continuously.
void loop() {
    char byte = 0;
    if (ioport_data_ready()) {
        byte = ioport_read();
        midi_receive_byte(byte);
    }
    if (g_update_frequency && g_notes_on) {
        const long freq = calc_oscillator_frequency(g_base_freq, g_pitch_bend);
        set_oscillator_frequency(freq);
        g_update_frequency = 0;
    }
}

// Entry Point
void main(void) {
    status_t status = system_init();
    if (status) {
        error(status);
    } 
    
    for (;;) {
        loop();
    }    
}
