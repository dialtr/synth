/**
 * Copyright (C) 2018 Thomas R. Dial
 * All Rights Reserved
 * 
 * Table mapping MIDI note numbers to note frequencies in hertz.
 */
#ifndef MIDI_NOTES_H_INCLUDED_
#define MIDI_NOTES_H_INCLUDED_

// A table mapping MIDI notes to whole-number frequencies.
extern const int MIDI_NOTE_FREQUENCY_TABLE[];

// Safely access midi note.
// TODO(tdial): This could easily be a macro.
int midi_note_frequency_for_note(char note) {
    return MIDI_NOTE_FREQUENCY_TABLE[note & 0x7f];
}

#endif  // MIDI_NOTES_H_INCLUDED_
