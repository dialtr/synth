 title  "Programmable Voltage-Controlled Envelope Generator V7B"
;
;  This program provides a versatile envelope generator on a single chip.
;  It is designed as a modern version of the CEM3312 or SSM2056 ICs.
;  Analogue output is provided as a PWM output, which requires LP
;  filtering to be useable.
;
;  Hardware Notes:
;   PIC16F684 running at 20 MHz using external crystal
; Six analogue inputs:
;   RA1/AN1: 0-5V Attack Time CV
;   RA2/AN2: 0-5V Decay Time CV
;   RC0/AN4: 0-5V Sustain Level CV
;   RC1/AN5: 0-5V Release Time CV
;   RC2/AN6: 0-5V Time Adjust CV (Keyboard CV or velocity, for example)
;   RC3/AN7: 0-5V Output Level CV
; Three digital inputs:
;   RA0: Trigger Input
;   RA3: Gate Input
;   RC4: Exp/Lin Input (High is Linear)
; One Digital Output:
;   RC5: PWM Envelope out
;
;  This version started as (ENVGEN4LIN.ASM), a test version without any of
;  the complications of the exponential output. Instead it passes
;  the linear PHASE value directly to the PWM output.
;  This should allow me to test the rest of the code, before trying to
;  add the complex (for a PIC) interpolation and lookp maths required for the 
;  exponential curve output 
;
;  29th Aug 06 - got this basically working.
;  2nd Sept 06 - ENVGEN5.ASM - Added exponential output.
; Still have 278 bytes left!
;
;   ENVGEN6 is a sidetrack to add pulse outputs for Tim Parkhurst
;
;   10th Nov 06 - ENVGEN7
;   Reduced the TIME CV range to 1:100, rather than 1:10000. Unfortunately
;   this leads to a loss of resolution (only 7 bits) on the TIME CV input
;   I've also rearranged the inputs a bit, and added an INVERTED input to
;   flip the envelope.
;
;   1 Aug 2007 - ENVGEN 7.1
;   Fixed a small bug which lead to the LSB of the output not being sent correctly.
;
;   14th May 2008 - ENVGEN 7B
;   Changed TIME CV to be unipolar not bipolar. This means it can simply be tied to
;   0V if not used.
;   Added Trigger input in place of GATE OUT, so envelope has full GATE & TRIGGER
;   operation. This caused problems using IOCA-driven GATE and TRIGGER pins, since
;   repeated bounces on the TRIGGER pin cause unwanted AD transients.
;   The solution was to move to a debounced, polled reading of the GATE & TRIGGER
;   pins. This is done at the sample rate, so delays are minimised.

  LIST R=DEC
 INCLUDE "p16f684.inc"

 __CONFIG _FCMEN_OFF & _IESO_OFF & _BOD_OFF & _CPD_OFF & _CP_OFF & _MCLRE_OFF & _PWRTE_ON & _WDT_OFF & _HS_OSC


;------------------------------
;	Variables
;------------------------------

 CBLOCK 0x020
	; Registers for context saving during interrupts
	W_TEMP 
	STATUS_TEMP
	PCLATH_TEMP
	FSR_TEMP
	; General working storage
	TEMP		; Used for the ADC delay
	LOOKUPTEMP	; The page boundary code needs a temp register
	PHASE_INC_INDEX	; Used by the PHASE_INC lookup
	TIME_TMP	; Used by the TIME_CV adjustments
	; The working storage for the interpolation subroutine
	INTERP_A
	INTERP_B
	INTERP_OUT_HI
	INTERP_OUT_LO
	; Working storage for the 8x8bit multiply subroutine
	MULT_IN
	MULT_OUT_HI
	MULT_OUT_LO

	; The current A/D channel and value
	ADC_CHANNEL
	ADC_VALUE
	; The current stage (0=Wait, 1=Attack, 2=Decay, 3=Sustain, 4=Release, 0=Wait again)
	STAGE
	; The currrent output level (used by RELEASE)
	CURRENT_LEVEL
	; The current control voltage(CV) values (8 bit)
	ATTACK_CV	; These first four aren't actually used any more
	DECAY_CV	; Instead we find a PHASE INC value and use that
	SUSTAIN_CV
	RELEASE_CV
	TIME_CV		; TIME and LEVEL are used directly
	LEVEL_CV
	; The debounce counters for GATE and TRIGGER
	DEBOUNCE_HI
	DEBOUNCE_LO
	STATES			; The output state from the debounce
	CHANGES			; The bits that have altered
	; The 24 bit phase accumulator
	PHASE_HI
	PHASE_MID
	PHASE_LO
	; The 20 bit frequency increments
	; These are stored separately for Attack, Decay, & Release
	; Note that these increments have been adjusted to reflect
	; changes due to TIME_CV, whereas the raw CVs haven't
	ATTACK_INC_LO
	ATTACK_INC_MID
	ATTACK_INC_HI
	DECAY_INC_LO
	DECAY_INC_MID
	DECAY_INC_HI
	RELEASE_INC_LO
	RELEASE_INC_MID
	RELEASE_INC_HI	
	; The 10 bit output level from the envelope
	OUTPUT_HI
	OUTPUT_LO
	; The 16 bit final output level, after scaling by LEVEL_CV
	FINAL_HI
	FINAL_LO
 ENDC

;-------------------------------------
;	DEFINE STATEMENTS
;-------------------------------------

; Useful bit definitions for clarity	
#define ZEROBIT		STATUS,Z	; Zero Flag
#define CARRY		STATUS,C	; Carry Flag
#define BORROW		STATUS,C	; Borrow is the same as Carry
; Input/Output bit definitions
#define EXPO_OR_LIN	PORTC, 4	; Exponential/Linear Input (1=Lin)

;----------------------------------------------------------------------
; Begin Executable Code Segment
;----------------------------------------------------------------------
	org     0x000		; processor reset vector
        nop			; for ICD use
        goto    Main		; Go to the main program

	org     0x004       	;Interrupt vector location
InterruptEnter
        movwf   W_TEMP              ; save W register
        swapf   STATUS, W           ; swap status to be saved into W
        bcf     STATUS, RP0         ; ---- Select Bank 0 -----
        movwf   STATUS_TEMP         ; save STATUS register
        movfw   PCLATH
        movwf   PCLATH_TEMP         ; save PCLATH_TEMP register
        movfw   FSR
        movwf   FSR_TEMP            ; save FSR_TEMP register

;----------------------------------------
; Interrupt Service Routine (ISR) (Section 12.4)
; This deals with the DDS and PWM output
;----------------------------------------
	
Timer2ISR
	btfss	PIR1, TMR2IF	; check if TMR2 interrupt
	goto	InterruptExit
	bcf	PIR1, TMR2IF	; clear TMR2 interrupt flag

; Test the GATE and TRIGGER Pins for changes
; Although there are two pins to monitor (TRIGGER and GATE) there
; are only actually two edges we're interested in. The envelope goes to
; the ATTACK stage when the TRIGGER goes high, and goes to the RELEASE
; stage when the GATE goes low. Other changes are ignored.

	; First, increment the debounce counters
	movfw	DEBOUNCE_LO	
	xorwf	DEBOUNCE_HI, f	; HI+ = HI XOR LO
	comf	DEBOUNCE_LO, f	; LO+ = ~LO

	; See if any changes occured
	movfw	PORTA		; Get current data from switches
	xorwf	STATES, w	; Find the changes
	; Reset counters where no change occured
	andwf	DEBOUNCE_LO, f
	andwf	DEBOUNCE_HI, f
	; If there is a pending change and the count has rolled over,
	; then the key has been debounced
	xorlw	D'255'		; Invert the changes
	iorwf	DEBOUNCE_HI, w	; If count is 0, both
	iorwf	DEBOUNCE_LO, w	; HI and LO are 0
	; Any bit in W that is clear at this point means that the
	; input has changed and the count rolled over.
	xorlw	D'255'
	; Now a 1 in W represents a key that has officially changed state
	movwf	CHANGES		; Store the changes (1 represents 'switch just changed')
	; Update the changes to the keyboard state
	xorwf	STATES, f

TestTrigger
	; Test Bit 0 of CHANGES (Has TRIGGER changed?)
	btfss	CHANGES, 0
	goto	TestGate
	; TRIGGER has changed, but has it gone high?
	btfss	STATES, 0
	goto	TestGate

TriggerGoneHigh
; If it's gone high, change to ATTACK stage
	; Zero the accumulator
	clrf	PHASE_HI
	clrf	PHASE_MID
	clrf	PHASE_LO
	; What's the current output level?
	movf	OUTPUT_HI, w
	movwf	CURRENT_LEVEL
	; Move to ATTACK stage
	movlw	D'1'
	movwf	STAGE
	goto	GenerateEnvelope

TestGate
	; Test Bit 3 of CHANGES (Has GATE changed?)
	btfss	CHANGES, 3
	goto	GenerateEnvelope
	; GATE has changed, but has it gone low?
	btfsc	STATES, 3
	goto	GenerateEnvelope

GateGoneLow
; If it's gone low, change to RELEASE stage
	; Zero the accumulator
	clrf	PHASE_HI
	clrf	PHASE_MID
	clrf	PHASE_LO
	; What's the current output level?
	movf	OUTPUT_HI, w
	movwf	CURRENT_LEVEL
	; Move to RELEASE stage
	movlw	D'4'
	movwf	STAGE

GenerateEnvelope
; Do we need to increment the phase accumulator?
; If so, what FSR offset should we use?
	movlw	HIGH IncrementPhaseBranch
	movwf	PCLATH		; Set up high bits of PC
	movf	STAGE, w	; Get current stage
	addwf	PCL, f		; Increment PC with STAGE value
IncrementPhaseBranch
	goto	Wait		; No PHASE_INC required for WAIT
	goto	GetAttackOffset
	goto	GetDecayOffset
	goto	Sustain		; No PHASE_INC required for SUSTAIN
	goto	GetReleaseOffset	
; THERE MIGHT BE A SHORTER WAY TO DO THIS BIT -
; THIS SEEMS LIKE A LOT FOR JUST THREE LITERALS
GetAttackOffset
	movlw	H'40'	; Offset for ATTACK_INC_LO
	goto	IncrementPhase

GetDecayOffset
	movlw	H'43'	; Offset for DECAY_INC_LO
	goto	IncrementPhase

GetReleaseOffset
	movlw	H'46'	; Offset for RELEASE_INC_LO


; Increment the phase accumulator PHASE (24+20 bit addition)
IncrementPhase
	; Which set of increments are we using? Set the FSR
	movwf	FSR
	; Add the increment to the phase accumulator, PHASE
	movf	INDF, w		; Add INC_LO to PHASE_LO
	addwf	PHASE_LO, f
	btfss	CARRY		; Do we carry one to the middle byte?
	goto	IncMiddleBit
	incfsz	PHASE_MID, f	; Has that carry caused a carry to the high byte?		
	goto	IncMiddleBit
	incf	PHASE_HI, f	; Has that carry caused an overflow?
	btfsc	ZEROBIT
	goto	NextStage
IncMiddleBit
	incf	FSR, f		; Move to the INC_MID
	movf	INDF, w		; Add INC_MID to PHASE_MID
	addwf	PHASE_MID, f
	btfss	CARRY		; Do we carry one to the high byte?
	goto	IncTopBit
	incf	PHASE_HI, f	; Has that carry caused an overflow?
	btfsc	ZEROBIT
	goto	NextStage
IncTopBit
	incf	FSR, f		; Move to the INC_HI
	movf	INDF, w		; Add INC_HI to PHASE_HI
	addwf	PHASE_HI, f
	btfss	CARRY		; Has it overflowed?
	goto	SelectStage	; No, so continue directly

; Accumulator has overflowed, so move to the next stage
NextStage
	; First zero the accumulator..
	clrf	PHASE_HI
	clrf	PHASE_MID
	clrf	PHASE_LO
	; ..then increment the STAGE
	incf	STAGE, f
	movf	STAGE, w
	xorlw	D'5'
	btfsc	ZEROBIT		; Is STAGE==5 yet?
	clrf	STAGE		; Yes, so reset it to zero


; We need to produce different output values depending on which stage we're at
SelectStage
	movlw	HIGH SelectStageBranch
	movwf	PCLATH		; Set up high bits of PC
	movf	STAGE, w	; Get current stage
	addwf	PCL, f	; Increment program counter with STAGE value
SelectStageBranch
	goto	Wait	; (Only gets called after'NextStage')
	goto	Attack
	goto	Decay
	goto	Sustain	; (Only gets called after'NextStage')
	goto	Release

Wait
	; Do nothing. GATE is low, and release stage has finished
	clrf	FINAL_HI	; Ensure we output zero when waiting
	clrf	FINAL_LO
	goto	PWMOutput

Attack
	; Attack needs scaling by 1-CURRENT_LEVEL,
	; then needs CURRENT_LEVEL adding to it
	comf	CURRENT_LEVEL, w
	movwf	MULT_IN
	movf	PHASE_HI, w	; This is the linear value
	; Do we use the linear value directly, or do an expo lookup?
	btfsc	EXPO_OR_LIN
	goto	AttackScaling

ExponentialAttack
	; Get the required value for an exponential attack curve
	; Lookup the first value
	call	GetAttackCurve	; We use the linear PHASE_HI value as an index
	movwf	INTERP_A
	; Lookup the next value
	incf	PHASE_HI, w
	call	GetAttackCurve
	movwf	INTERP_B
	; Do the interpolation
	call	Interpolation

AttackScaling
	call	Multiply8x8	; Do the scaling
	; Add CURRENT_LEVEL
	movf	CURRENT_LEVEL, w
	addwf	MULT_OUT_HI, w
	movwf	OUTPUT_HI
	movf	MULT_OUT_LO, w
	movwf	OUTPUT_LO
	goto	MultiplyByLevelCV

Decay
	; Decay needs scaling by 1-SUSTAIN, then inverting
	comf	SUSTAIN_CV, w	; Invert the SUSTAIN level	
	movwf	MULT_IN
	movf	PHASE_HI, w
	; Do we use the linear value directly, or do an expo lookup?
	btfsc	EXPO_OR_LIN
	goto	DecayScaling

ExponentialDecay
	; Get the required value for an exponential Decay curve
	; Lookup the first value
	call	GetDecayCurve
	movwf	INTERP_A
	; Lookup the next value
	incf	PHASE_HI, w
	call	GetDecayCurve
	movwf	INTERP_B
	; Do the interpolation
	call	Interpolation

DecayScaling
	call	Multiply8x8	; Do the scaling
	; Invert the result
	comf	MULT_OUT_HI, w
	movwf	OUTPUT_HI
	comf	MULT_OUT_LO, w
	movwf	OUTPUT_LO
	goto	MultiplyByLevelCV


Sustain
	; Do nothing. Gate is high, and decay stage has finished
	movf	SUSTAIN_CV, w	; Ensure that we output the sustain level
	movwf	OUTPUT_HI
	clrf	OUTPUT_LO
	goto	MultiplyByLevelCV


Release
	; Release needs scaling by CURRENT_LEVEL, then
	; 1-CURRENT_LEVEL level adding, then inverting
	movf	CURRENT_LEVEL, w
	movwf	MULT_IN
	movf	PHASE_HI, w
	; Do we use the linear value directly, or do an expo lookup?
	btfsc	EXPO_OR_LIN
	goto	ReleaseScaling

ExponentialRelease
	; Get the required value for an exponential Decay curve
	; Lookup the first value
	call	GetDecayCurve
	movwf	INTERP_A
	; Lookup the next value
	incf	PHASE_HI, w
	call	GetDecayCurve
	movwf	INTERP_B
	; Do the interpolation
	call	Interpolation

ReleaseScaling
	call	Multiply8x8	; Do the scaling
	; Add 1-CURRENT_LEVEL
	comf	CURRENT_LEVEL, w
	addwf	MULT_OUT_HI, w
	; Invert the result
	xorlw	D'255'
	movwf	OUTPUT_HI
	comf	MULT_OUT_LO, w
	movwf	OUTPUT_LO

; Modify the Output level
;-------------------------
; (Basic multiply routine from Microchip App Note 26)
; This involves multiplying the envelope output by
; the LEVEL_CV value - an 10 bit x 8 bit multiplication
; Of the result, I only need the top 10 bits
; Expects number to be multiplied by LEVEL_CV in OUTPUT

MultiplyByLevelCV

	clrf	FINAL_HI
	clrf	FINAL_LO
	movf	LEVEL_CV, w
	clrc	; Clear carry? Why didn't I know about this?!
	btfsc	OUTPUT_HI, 0
	addwf	FINAL_HI, f
	rrf	FINAL_HI, f
	rrf	FINAL_LO, f
	btfsc	OUTPUT_HI, 1
	addwf	FINAL_HI, f
	rrf	FINAL_HI, f
	rrf	FINAL_LO, f
	btfsc	OUTPUT_HI, 2
	addwf	FINAL_HI, f
	rrf	FINAL_HI, f
	rrf	FINAL_LO, f
	btfsc	OUTPUT_HI, 3
	addwf	FINAL_HI, f
	rrf	FINAL_HI, f
	rrf	FINAL_LO, f
	btfsc	OUTPUT_HI, 4
	addwf	FINAL_HI, f
	rrf	FINAL_HI, f
	rrf	FINAL_LO, f
	btfsc	OUTPUT_HI, 5
	addwf	FINAL_HI, f
	rrf	FINAL_HI, f
	rrf	FINAL_LO, f
	btfsc	OUTPUT_HI, 6
	addwf	FINAL_HI, f
	rrf	FINAL_HI, f
	rrf	FINAL_LO, f
	btfsc	OUTPUT_HI, 7
	addwf	FINAL_HI, f
	rrf	FINAL_HI, f
	rrf	FINAL_LO, f
; Extra bits
;	btfsc	OUTPUT_HI, 6
;	addwf	FINAL_HI, f
;	bcf	CARRY	; If the carry isn't cleared, it might wrap around
;	rrf	FINAL_HI, f
;	rrf	FINAL_LO, f
;	btfsc	OUTPUT_HI, 7
;	addwf	FINAL_HI, f
;	bcf	CARRY
;	rrf	FINAL_HI, f
;	rrf	FINAL_LO, f

	
; Set PWM duty cycle
;---------------------------------------
; This puts the value of PWM_DUTY_CYCLE into the appropriate
; registers.
; Note: we can set the duty cycle in registers
; CCP1CON and CCPR1L because they are double
; buffered and the changes will not take affect
; until the next PWM period starts (TMR2 resets).
; Note that FINAL_LO is not preserved
PWMOutput
	; Put the 2 MSBs of OUTPUT_LO into CCP1CON
	rlf	FINAL_LO,f	; rotate bit 7 into the carry bit
	bsf	CCP1CON, DC1B1	; set or clear bit 5 of the CCP1CON register
	btfss	CARRY
	bcf	CCP1CON, DC1B1

	rlf     FINAL_LO, f	; rotate bit 6 into the carry bit
	bsf	CCP1CON, DC1B0	; set or clear bit 4 of the CCP1CON register
	btfss	CARRY
	bcf	CCP1CON, DC1B0

	; Put the high byte into CCPR1L
	movf	FINAL_HI, w
	movwf   CCPR1L

;----------------------------------------
InterruptExit
        movfw   PCLATH_TEMP         ; restore PCLATH_TEMP register
        movwf   PCLATH
        movfw   FSR_TEMP            ; restore FSR_TEMP register
        movwf   FSR
        swapf   STATUS_TEMP, w      ; swap status_temp into W, sets bank to original state
        movwf   STATUS              ; restore STATUS register
        swapf   W_TEMP, f
        swapf   W_TEMP, w           ; restore W register

        retfie

;------------------------------------------------------
;	8 bit x 8 bit Multiply Subroutine
; This is used by the Decay and Release stages to
; scale their output to ensure it meets up with the
; SUSTAIN level.
; The value in W is multipled by MULT_IN.
; 16 bit Output is in MULT_OUT
;------------------------------------------------------
Multiply8x8
	clrf	MULT_OUT_HI
	clrf	MULT_OUT_LO
	clrc	; Clear carry? Why didn't I know about this?!
	btfsc	MULT_IN,0
	addwf	MULT_OUT_HI, f
	rrf	MULT_OUT_HI, f
	rrf	MULT_OUT_LO, f
	btfsc	MULT_IN,1
	addwf	MULT_OUT_HI, f
	rrf	MULT_OUT_HI, f
	rrf	MULT_OUT_LO, f
	btfsc	MULT_IN,2
	addwf	MULT_OUT_HI, f
	rrf	MULT_OUT_HI, f
	rrf	MULT_OUT_LO, f
	btfsc	MULT_IN,3
	addwf	MULT_OUT_HI, f
	rrf	MULT_OUT_HI, f
	rrf	MULT_OUT_LO, f
	btfsc	MULT_IN,4
	addwf	MULT_OUT_HI, f
	rrf	MULT_OUT_HI, f
	rrf	MULT_OUT_LO, f
	btfsc	MULT_IN,5
	addwf	MULT_OUT_HI, f
	rrf	MULT_OUT_HI, f
	rrf	MULT_OUT_LO, f
	btfsc	MULT_IN,6
	addwf	MULT_OUT_HI, f
	rrf	MULT_OUT_HI, f
	rrf	MULT_OUT_LO, f
	btfsc	MULT_IN,7
	addwf	MULT_OUT_HI, f
	rrf	MULT_OUT_HI, f
	rrf	MULT_OUT_LO, f
	return


;------------------------------------------------------
;	Linear Interpolation Subroutine
; This is used by the Attack, Decay, and Release stages.
; The routine expects to be given two 8-bit values to
; interpolate between: INTERP_A and INTERP_B
; It also assumes that PHASE_MID is to be used as
; interpolation index.
;------------------------------------------------------

Interpolation
	; Work out the DELTA value
	movf	INTERP_A, w
	subwf	INTERP_B, f	; Should have DELTA in INTERP_B

	; Calculate DELTA * INTERP_INDEX(PHASE_MID)
	; This is a 8 bit x 8 bit multiplication but I only need the 8 MSBs
	; The interpolation result goes into INTERP_OUT
	clrf	INTERP_OUT_HI
	clrf	INTERP_OUT_LO

	movf	INTERP_B, w	
	clrc	; Clear carry? Why didn't I know about this?!
	btfsc	PHASE_MID, 0	; PHASE_MID is used as the interp index
	addwf	INTERP_OUT_HI, f
	rrf	INTERP_OUT_HI, f
	rrf	INTERP_OUT_LO, f
	btfsc	PHASE_MID, 1
	addwf	INTERP_OUT_HI, f
	rrf	INTERP_OUT_HI, f
	rrf	INTERP_OUT_LO, f
	btfsc	PHASE_MID, 2
	addwf	INTERP_OUT_HI, f
	rrf	INTERP_OUT_HI, f
	rrf	INTERP_OUT_LO, f
	btfsc	PHASE_MID, 3
	addwf	INTERP_OUT_HI, f
	rrf	INTERP_OUT_HI, f
	rrf	INTERP_OUT_LO, f
	btfsc	PHASE_MID, 4
	addwf	INTERP_OUT_HI, f
	rrf	INTERP_OUT_HI, f
	rrf	INTERP_OUT_LO, f
	btfsc	PHASE_MID, 5
	addwf	INTERP_OUT_HI, f
	rrf	INTERP_OUT_HI, f
	rrf	INTERP_OUT_LO, f
	btfsc	PHASE_MID, 6
	addwf	INTERP_OUT_HI, f
	rrf	INTERP_OUT_HI, f
	rrf	INTERP_OUT_LO, f
	btfsc	PHASE_MID, 7
	addwf	INTERP_OUT_HI, f
	rrf	INTERP_OUT_HI, f
	rrf	INTERP_OUT_LO, f
	; We should have a 16 bit DELTA x INDEX in INTERP_OUT

	; Add the interpolated value and INTERP_A to get the output
	movf	INTERP_A, w
	addwf	INTERP_OUT_HI, w	; Required value is in W
	
	return


;----------------------------------------
;	Analogue to Digital conversion subroutine
; This is used by the main code loop
;----------------------------------------

DoADConversion
	; Short delay whilst the channel settles
	movlw   D'6'                ; At 4 MHz, a 22 us delay
        movwf   TEMP                ; (22us = 2us + 6 * 3us + 1us)
        decfsz  TEMP, F
        goto    $-1

	; Start the conversion
	bsf     ADCON0, GO
	; Wait for it to finish
	btfsc   ADCON0, GO            ;  Is it done?
	goto   $ - 1

	;  Read the ADC Value and store it
	movf    ADRESH, w
	movwf   ADC_VALUE
	return

;----------------------------------------
;	The main program
; This reads the A/D channels and provides
; values for the DDS
;----------------------------------------
Main
	clrf    PORTC
	movlw   7                     ;  Turn off Comparators
	movwf   CMCON0

	clrf    TMR2                  ;  Using TMR2 as a PWM Generator
	movlw   b'00000100'           ;  Enable TMR2
	movwf   T2CON
	clrf    CCPR1L                ;  Nothing Moving (Yet)
	
	bsf	STATUS, RP0	; Bank 1

	; Peripheral Interrupt Enable Register (PIE1) (Section 2.2.2.4)
	movlw	b'00000010'	; TMR2 Overflow Interrupt: ENABLED
	movwf	PIE1 ^ 0x80

	movlw   b'11110110'	; ADC Input on AN1, AN2 and AN4, AN5, AN6, AN7
	movwf   ANSEL ^ 0x80

	movlw   b'00100000'	; Select ADC Clock as Fosc/32
	movwf   ADCON1 ^ 0x80	; to ensure TAD of 1.6uS

	movlw   D'255'		; Setup the Limit for the PWM
	movwf   PR2 ^ 0x80	; Maximum 19.5 KHz Frequency

	movlw   b'011111'	; RC5 Output, all others inputs
	movwf   TRISC ^ 0x80

	movlw   b'111111'	; All PORT A inputs
	movwf   TRISA ^ 0x80

	clrf	IOCA ^ 0x80	; No Interrupt-on-Change

	bcf	STATUS, RP0	; Bank 0

	; Interrupt Control Register (INTCON) (Section 2.2.2.3)
	movlw	b'00000000'	; Clear all peripheral interrupts
	movwf	PIR1
	movlw	b'11000000'	; Peripheral Interrupts: ENABLED
	movwf	INTCON	; (EEI, ADI, CCP1I, C2I, C1I, OSFI, TMR2I, TMR1I)

	movlw   b'00001100'           ;  Enable single channel PWM
	movwf   CCP1CON

	; Set up initial values of the variables
	clrf	ATTACK_CV	; Default to minimum time (1mS)
	clrf	DECAY_CV
	clrf	SUSTAIN_CV
	clrf	RELEASE_CV
	movlw	D'128'
	movwf	TIME_CV		; Default to no time modulation
	movlw	D'255'
	movwf	LEVEL_CV	; Default to max output level
	clrf	STAGE
	; Clear the Phase Accumulator
	clrf	PHASE_LO
	clrf	PHASE_MID
	clrf	PHASE_HI
	; Clear the increments too
	clrf	ATTACK_INC_LO
	clrf	ATTACK_INC_MID
	clrf	ATTACK_INC_HI
	clrf	DECAY_INC_LO
	clrf	DECAY_INC_MID
	clrf	DECAY_INC_HI
	clrf	RELEASE_INC_LO
	clrf	RELEASE_INC_MID
	clrf	RELEASE_INC_HI
	; Clear the output buffers
	clrf	OUTPUT_HI	; Pre-scaling by LEVEL_CV
	clrf	OUTPUT_LO
	clrf	FINAL_HI	; Post-scaling
	clrf	FINAL_LO

	movlw	D'7'
	movwf	ADC_CHANNEL	; Start with ATTACK_CV

	clrf	CURRENT_LEVEL	; Start with this at zero
	; Set up the GATE & TRIGGER debounce
	clrf	DEBOUNCE_HI
	clrf	DEBOUNCE_LO
	clrf	STATES
	clrf	CHANGES

MainLoop
	; Change to next A/D channel
	incf	ADC_CHANNEL, f

; We need to do different things depending on which value we're reading:
SelectADCChannel
	movlw	HIGH ADCChannelJump
	movwf	PCLATH	; Set the top part of the PC
	movf	ADC_CHANNEL, w	; Get current channel
	andlw	D'7'		; Only need three LSBs
	addwf	PCL, f	; Increment program counter with channel value
ADCChannelJump	
	goto	AttackCV
	goto	DecayCV
	goto	SustainCV
	goto	ReleaseCV
	goto	TimeCV
	goto	LevelCV
ScannedAllChannels
	; If we jump to here, ADC_CHANNEL is either 6 or 7
	; Either way, it'll be 7 when it goes back to MainLoop
	incf	ADC_CHANNEL, f
	goto	MainLoop


; Update the Attack CV
AttackCV
	movlw	b'00000101'	; AN1, ADC On
	movwf	ADCON0
	call	DoADConversion
	movwf	ATTACK_CV

	; Subtract the TIME_CV (Increasing TIME_CV shortens the Env)
	movf	TIME_CV, w
	subwf	ATTACK_CV, w
	btfss	BORROW
	movlw	D'0'	; If value is less than 0, use minimum

; WHAT HAPPENS IF AN INTERRUPT OCCURS WHILST THIS INCREMENT
; IS BEING UPDATED? IT'S GOING TO GLITCH, SURELY?
	; Get the new phase increment for the ATTACK stage
	movwf	PHASE_INC_INDEX
	call	GetPhaseIncHi
	movwf	ATTACK_INC_HI
	call	GetPhaseIncMid
	movwf	ATTACK_INC_MID
	call	GetPhaseIncLo
	movwf	ATTACK_INC_LO
	goto	MainLoop

; Update the Decay CV
DecayCV
	movlw	b'00001001'	; AN2, ADC On
	movwf	ADCON0
	call	DoADConversion
	movwf	DECAY_CV

	; Subtract the TIME_CV (Increasing TIME_CV shortens the Env)
	movfw	TIME_CV
	subwf	DECAY_CV, w
	btfss	BORROW
	movlw	D'0'	; If value is less than 0, use minimum

	; Get the new phase increment for the DECAY stage
	movwf	PHASE_INC_INDEX
	call	GetPhaseIncHi
	movwf	DECAY_INC_HI
	call	GetPhaseIncMid
	movwf	DECAY_INC_MID
	call	GetPhaseIncLo
	movwf	DECAY_INC_LO
	goto	MainLoop


; Update the Sustain CV
SustainCV
	movlw	b'00010001'	; AN4, ADC On
	movwf	ADCON0
	call	DoADConversion
	movwf	SUSTAIN_CV	; Simply store this one- easy!
	goto	MainLoop


; Update the Release CV
ReleaseCV
	movlw	b'00010101'	; AN5, ADC On
	movwf	ADCON0
	call	DoADConversion
	movwf	RELEASE_CV

	; Subtract the TIME_CV (Increasing TIME_CV shortens the Env)
	movfw	TIME_CV
	subwf	RELEASE_CV, w
	btfss	BORROW
	movlw	D'0'	; If value is less than 0, use minimum

	; Get the new phase increment for the RELEASE stage
	movwf	PHASE_INC_INDEX
	call	GetPhaseIncHi
	movwf	RELEASE_INC_HI
	call	GetPhaseIncMid
	movwf	RELEASE_INC_MID
	call	GetPhaseIncLo
	movwf	RELEASE_INC_LO
	goto	MainLoop


; Update the Time CV
TimeCV
	movlw	b'00011001'	; AN6, ADC On
	movwf	ADCON0
	call	DoADConversion
	; Put the value into TIME_CV where we can work on it
	movwf	TIME_CV	
	; Reduce the resolution of the TIME CV so it isn't so extreme
	clrc
	rrf	TIME_CV, f
	; Note that there is no glitch problem here, since the
	; value can be updated in a single instruction.
	; This allows much faster modulation on the TIME_CV input.
	goto	MainLoop


; Update the Output Level CV
LevelCV
	movlw	b'00011101'	; AN7, ADC On
	movwf	ADCON0
	call	DoADConversion
	movwf	LEVEL_CV	; Simply store this one- easy!
	; Again,no glitch problem, therefore LEVEL_CV input
	; can cope with fast modulation too.
	goto	MainLoop


;--------------------------------------------------------------------
;	Curve Lookup Tables
; The Attack and Decay/Release curves are stored separately.
; This is because the Attack curve heads towards 6.5V but stops at 5V,
; whereas the Decay/Release curves actually arrive at their destination value.
; Note that because the linear count goes UP in all cases and
; hence needs inverting for Decay/Release, the Decay/Release curve is the same
; way up as the Attack. This way it gets the same inversion as the linear count,
; which makes life marginally simpler.
;--------------------------------------------------------------------

GetAttackCurve
        movwf   LOOKUPTEMP
        movlw   HIGH AttackCurveLookup
        movwf   PCLATH
        movfw   LOOKUPTEMP
        addlw   LOW AttackCurveLookup
        btfsc	CARRY
        incf	PCLATH, F
        movwf   PCL

AttackCurveLookup
	dt	D'0', D'1', D'3', D'5', D'7', D'9', D'11', D'13', D'14', D'16', D'18', D'20', D'22', D'23', D'25', D'27'
	dt	D'29', D'30', D'32', D'34', D'35', D'37', D'39', D'41', D'42', D'44', D'45', D'47', D'49', D'50', D'52', D'54'
	dt	D'55', D'57', D'58', D'60', D'61', D'63', D'65', D'66', D'68', D'69', D'71', D'72', D'74', D'75', D'76', D'78'
	dt	D'79', D'81', D'82', D'84', D'85', D'87', D'88', D'89', D'91', D'92', D'93', D'95', D'96', D'98', D'99', D'100'
	dt	D'102', D'103', D'104', D'105', D'107', D'108', D'109', D'111', D'112', D'113', D'114', D'116', D'117', D'118', D'119', D'121'
	dt	D'122', D'123', D'124', D'125', D'126', D'128', D'129', D'130', D'131', D'132', D'133', D'135', D'136', D'137', D'138', D'139'
	dt	D'140', D'141', D'142', D'143', D'144', D'146', D'147', D'148', D'149', D'150', D'151', D'152', D'153', D'154', D'155', D'156'
	dt	D'157', D'158', D'159', D'160', D'161', D'162', D'163', D'164', D'165', D'166', D'167', D'168', D'169', D'170', D'170', D'171'
	dt	D'172', D'173', D'174', D'175', D'176', D'177', D'178', D'179', D'179', D'180', D'181', D'182', D'183', D'184', D'185', D'185'
	dt	D'186', D'187', D'188', D'189', D'190', D'190', D'191', D'192', D'193', D'194', D'194', D'195', D'196', D'197', D'198', D'198'
	dt	D'199', D'200', D'201', D'201', D'202', D'203', D'204', D'204', D'205', D'206', D'206', D'207', D'208', D'209', D'209', D'210'
	dt	D'211', D'211', D'212', D'213', D'213', D'214', D'215', D'216', D'216', D'217', D'218', D'218', D'219', D'219', D'220', D'221'
	dt	D'221', D'222', D'223', D'223', D'224', D'225', D'225', D'226', D'226', D'227', D'228', D'228', D'229', D'229', D'230', D'231'
	dt	D'231', D'232', D'232', D'233', D'233', D'234', D'235', D'235', D'236', D'236', D'237', D'237', D'238', D'238', D'239', D'239'
	dt	D'240', D'240', D'241', D'242', D'242', D'243', D'243', D'244', D'244', D'245', D'245', D'246', D'246', D'247', D'247', D'248'
	dt	D'248', D'249', D'249', D'249', D'250', D'250', D'251', D'251', D'252', D'252', D'253', D'253', D'254', D'254', D'255', D'255'

GetDecayCurve
        movwf   LOOKUPTEMP
        movlw   HIGH DecayCurveLookup
        movwf   PCLATH
        movfw   LOOKUPTEMP
        addlw   LOW DecayCurveLookup
        btfsc	CARRY
        incf	PCLATH, F
        movwf   PCL

DecayCurveLookup
	dt	D'0', D'7', D'14', D'20', D'27', D'33', D'40', D'46', D'52', D'57', D'63', D'68', D'73', D'79', D'83', D'88'
	dt	D'93', D'98', D'102', D'106', D'110', D'114', D'118', D'122', D'126', D'130', D'133', D'137', D'140', D'143', D'146', D'149'
	dt	D'152', D'155', D'158', D'161', D'163', D'166', D'168', D'171', D'173', D'176', D'178', D'180', D'182', D'184', D'186', D'188'
	dt	D'190', D'192', D'194', D'195', D'197', D'199', D'200', D'202', D'203', D'205', D'206', D'208', D'209', D'210', D'211', D'213'
	dt	D'214', D'215', D'216', D'217', D'218', D'219', D'220', D'221', D'222', D'223', D'224', D'225', D'226', D'227', D'228', D'228'
	dt	D'229', D'230', D'231', D'231', D'232', D'233', D'233', D'234', D'234', D'235', D'236', D'236', D'237', D'237', D'238', D'238'
	dt	D'239', D'239', D'240', D'240', D'241', D'241', D'241', D'242', D'242', D'243', D'243', D'243', D'244', D'244', D'244', D'245'
	dt	D'245', D'245', D'245', D'246', D'246', D'246', D'247', D'247', D'247', D'247', D'247', D'248', D'248', D'248', D'248', D'249'
	dt	D'249', D'249', D'249', D'249', D'249', D'250', D'250', D'250', D'250', D'250', D'250', D'251', D'251', D'251', D'251', D'251'
	dt	D'251', D'251', D'251', D'252', D'252', D'252', D'252', D'252', D'252', D'252', D'252', D'252', D'252', D'253', D'253', D'253'
	dt	D'253', D'253', D'253', D'253', D'253', D'253', D'253', D'253', D'253', D'253', D'253', D'254', D'254', D'254', D'254', D'254'
	dt	D'254', D'254', D'254', D'254', D'254', D'254', D'254', D'254', D'254', D'254', D'254', D'254', D'254', D'254', D'254', D'254'
	dt	D'254', D'254', D'254', D'254', D'255', D'255', D'255', D'255', D'255', D'255', D'255', D'255', D'255', D'255', D'255', D'255'
	dt	D'255', D'255', D'255', D'255', D'255', D'255', D'255', D'255', D'255', D'255', D'255', D'255', D'255', D'255', D'255', D'255'
	dt	D'255', D'255', D'255', D'255', D'255', D'255', D'255', D'255', D'255', D'255', D'255', D'255', D'255', D'255', D'255', D'255'
	dt	D'255', D'255', D'255', D'255', D'255', D'255', D'255', D'255', D'255', D'255', D'255', D'255', D'255', D'255', D'255', D'255'


;-----------------------------------------
;	Control Lookup Table
; Converts from 0-255 CV input to 20 bit
; PHASE_INC value
; The tables should be called with the
; index in W, and will return the required
; value
;-----------------------------------------

GetPhaseIncHi
	movlw	HIGH PhaseLookupHi
	movwf	PCLATH
	movf	PHASE_INC_INDEX, w
	addlw	LOW PhaseLookupHi
	btfsc	CARRY
	incf	PCLATH, f
	movwf	PCL

PhaseLookupHi
	dt	0xc, 0xc, 0xb, 0xb, 0xb, 0xa, 0xa, 0x9, 0x9, 0x9, 0x8, 0x8, 0x8, 0x8, 0x7, 0x7
	dt	0x7, 0x6, 0x6, 0x6, 0x6, 0x6, 0x5, 0x5, 0x5, 0x5, 0x5, 0x4, 0x4, 0x4, 0x4, 0x4
	dt	0x4, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2
	dt	0x2, 0x2, 0x2, 0x2, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1
	dt	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0
	dt	0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0
	dt	0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0
	dt	0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0
	dt	0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0
	dt	0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0
	dt	0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0
	dt	0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0
	dt	0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0
	dt	0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0
	dt	0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0
	dt	0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0

GetPhaseIncMid
	movlw	HIGH PhaseLookupMid
	movwf	PCLATH
	movf	PHASE_INC_INDEX, w
	addlw	LOW PhaseLookupMid
	btfsc	CARRY
	incf	PCLATH, f
	movwf	PCL

PhaseLookupMid
	dt	0xcc, 0x59, 0xe9, 0x7d, 0x15, 0xb1, 0x50, 0xf3, 0x99, 0x42, 0xee, 0x9d, 0x4f, 0x4, 0xbc, 0x76
	dt	0x32, 0xf1, 0xb2, 0x76, 0x3b, 0x3, 0xcc, 0x98, 0x65, 0x34, 0x5, 0xd8, 0xac, 0x82, 0x59, 0x32
	dt	0xc, 0xe7, 0xc4, 0xa2, 0x81, 0x61, 0x43, 0x25, 0x9, 0xed, 0xd3, 0xb9, 0xa0, 0x89, 0x72, 0x5c
	dt	0x46, 0x32, 0x1e, 0xb, 0xf8, 0xe6, 0xd5, 0xc4, 0xb4, 0xa5, 0x96, 0x88, 0x7a, 0x6d, 0x60, 0x53
	dt	0x47, 0x3c, 0x30, 0x26, 0x1b, 0x11, 0x8, 0xfe, 0xf5, 0xed, 0xe4, 0xdc, 0xd4, 0xcd, 0xc6, 0xbf
	dt	0xb8, 0xb1, 0xab, 0xa5, 0x9f, 0x99, 0x94, 0x8f, 0x8a, 0x85, 0x80, 0x7c, 0x77, 0x73, 0x6f, 0x6b
	dt	0x67, 0x63, 0x60, 0x5d, 0x59, 0x56, 0x53, 0x50, 0x4d, 0x4a, 0x48, 0x45, 0x43, 0x40, 0x3e, 0x3c
	dt	0x3a, 0x38, 0x36, 0x34, 0x32, 0x30, 0x2e, 0x2d, 0x2b, 0x2a, 0x28, 0x27, 0x25, 0x24, 0x23, 0x21
	dt	0x20, 0x1f, 0x1e, 0x1d, 0x1c, 0x1b, 0x1a, 0x19, 0x18, 0x17, 0x16, 0x16, 0x15, 0x14, 0x13, 0x13
	dt	0x12, 0x11, 0x11, 0x10, 0xf, 0xf, 0xe, 0xe, 0xd, 0xd, 0xc, 0xc, 0xb, 0xb, 0xb, 0xa
	dt	0xa, 0x9, 0x9, 0x9, 0x8, 0x8, 0x8, 0x8, 0x7, 0x7, 0x7, 0x6, 0x6, 0x6, 0x6, 0x6
	dt	0x5, 0x5, 0x5, 0x5, 0x5, 0x4, 0x4, 0x4, 0x4, 0x4, 0x4, 0x3, 0x3, 0x3, 0x3, 0x3
	dt	0x3, 0x3, 0x3, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x1, 0x1
	dt	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1
	dt	0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0
	dt	0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0

GetPhaseIncLo
	movlw	HIGH PhaseLookupLo
	movwf	PCLATH
	movf	PHASE_INC_INDEX, w
	addlw	LOW PhaseLookupLo
	btfsc	CARRY
	incf	PCLATH, f
	movwf	PCL

PhaseLookupLo
	dt	0xcc, 0x0, 0x4c, 0x8a, 0x97, 0x50, 0x95, 0x45, 0x40, 0x6b, 0xa6, 0xd8, 0xe4, 0xb2, 0x28, 0x2e
	dt	0xae, 0x90, 0xbf, 0x26, 0xb1, 0x4e, 0xe8, 0x6f, 0xd0, 0xfb, 0xe0, 0x70, 0x9a, 0x51, 0x86, 0x2c
	dt	0x37, 0x98, 0x45, 0x32, 0x53, 0x9d, 0x6, 0x84, 0xd, 0x97, 0x1a, 0x8c, 0xe6, 0x1e, 0x2e, 0xd
	dt	0xb4, 0x1d, 0x40, 0x16, 0x9a, 0xc5, 0x91, 0xf9, 0xf7, 0x86, 0xa1, 0x42, 0x66, 0x6, 0x20, 0xaf
	dt	0xae, 0x19, 0xee, 0x27, 0xc2, 0xbb, 0xe, 0xba, 0xb9, 0xa, 0xaa, 0x95, 0xca, 0x45, 0x4, 0x4
	dt	0x44, 0xc1, 0x79, 0x6a, 0x91, 0xee, 0x7d, 0x3e, 0x2e, 0x4c, 0x96, 0xb, 0xa9, 0x6e, 0x5a, 0x6a
	dt	0x9f, 0xf5, 0x6d, 0x5, 0xbb, 0x8f, 0x80, 0x8d, 0xb4, 0xf5, 0x4f, 0xc1, 0x4a, 0xe9, 0x9e, 0x67
	dt	0x45, 0x36, 0x39, 0x4f, 0x75, 0xad, 0xf4, 0x4c, 0xb2, 0x27, 0xa9, 0x39, 0xd7, 0x80, 0x36, 0xf7
	dt	0xc4, 0x9c, 0x7e, 0x6a, 0x60, 0x5f, 0x67, 0x79, 0x92, 0xb4, 0xdd, 0xe, 0x47, 0x86, 0xcd, 0x1a
	dt	0x6d, 0xc6, 0x25, 0x8a, 0xf4, 0x64, 0xd9, 0x53, 0xd1, 0x54, 0xdb, 0x67, 0xf7, 0x8b, 0x22, 0xbd
	dt	0x5c, 0xfe, 0xa4, 0x4d, 0xf9, 0xa7, 0x59, 0xe, 0xc5, 0x7e, 0x3b, 0xf9, 0xba, 0x7d, 0x43, 0xa
	dt	0xd3, 0x9f, 0x6c, 0x3b, 0xb, 0xde, 0xb2, 0x87, 0x5e, 0x37, 0x10, 0xec, 0xc8, 0xa6, 0x85, 0x65
	dt	0x46, 0x29, 0xc, 0xf1, 0xd6, 0xbc, 0xa3, 0x8c, 0x75, 0x5e, 0x49, 0x34, 0x20, 0xd, 0xfa, 0xe9
	dt	0xd7, 0xc7, 0xb6, 0xa7, 0x98, 0x8a, 0x7c, 0x6e, 0x61, 0x55, 0x49, 0x3d, 0x32, 0x27, 0x1d, 0x12
	dt	0x9, 0xff, 0xf6, 0xee, 0xe5, 0xdd, 0xd5, 0xce, 0xc6, 0xbf, 0xb9, 0xb2, 0xac, 0xa6, 0xa0, 0x9a
	dt	0x95, 0x8f, 0x8a, 0x85, 0x81, 0x7c, 0x78, 0x73, 0x6f, 0x6b, 0x68, 0x64, 0x60, 0x5d, 0x5a, 0x56

; We never reach here
	end 
