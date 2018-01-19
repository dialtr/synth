/**
 * Copyright (C) 2018 Thomas R. Dial
 * All Rights Reserved
 * 
 * System Configuration
 */
#ifndef CONFIG_H_INCLUDED_
#define CONFIG_H_INCLUDED_

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

// The master clock runs at 8 MHZ, as described above.
#define _XTAL_FREQ 8000000

#endif  // CONFIG_H_INCLUDED_