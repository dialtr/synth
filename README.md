# DIAL-1 Synthesizer

This project contains the source code for my first synthesizer design. An
overarching goal of this project is to learn more about electronics design
as well as microcontroller programming. Accordingly, a key goal of this
project is to document the design and implementation as clearly as possible
so that others may use it as a resource in their own designs.

## Hardware

The DIAL-1 employs a PIC18F4620 microcontroller to coordinate system
activities, which include:

* Receiving input from panel controls (potentiometers, switches, etc.)
* Receiving and transmitting MIDI data via the USART
* Programming the DCOs (digitally controlled oscillators)
* Computing control voltages for DACs to drive VCAs and the VCF, etc.
* Displaying data on the Hitachi 44780-compatible 16x2 Display
* Communicating with the SD-card interface for Save / Load
* Arpeggiator function

The sections below outline the major subsystems.

### Digitally Controlled Oscillators (DCOs)

The DIAL-1 DCO system uses an Intel 8254 programmable timer IC to generate
baseline waveforms (square waves) that are transformed via additional
circuitry to "more interesting" waveforms such as saw, triangle, and PWM.
Benefits of the DCO approach include unparalleled pitch stability as well
as the potential for the implementation of precise microtonal tuning
tables.

### Dedicated Envelope Generator for the VCA

Early MIDI-controlled analog synthesizers (such as the Roland Juno-106 and
the Oberheim Matrix 1000) computed envelopes entirely in software which
reduced part count and made rich modulation capabilities possible.
However, software-based envelopes tended to be sluggish due to the fact
that the microcontrollers used were saddled with other duties.

For this reason, in the prototype of the DIAL-1, a dedicated envelope
generator will be used to control the VCA. (The envelope generator is in
fact a PIC16-series microcontroller running a highly optimized envelope
generation routine that allows for fast (~1ms) response times.



### Noise Generator

The DIAL-1 features an analog noise generator that employs a typical
open-collector transistor, as found in many synthesizers.

### Voltage Controlled Filter

The filter used in the DIAL-1 is 24db/oct low-pass ladder filter circuit
with resonance and cutoff. The filter can be driven to self oscillation.

## Software

