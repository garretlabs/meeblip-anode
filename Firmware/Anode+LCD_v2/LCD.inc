; ***************************************************************************
;   LCD-AVR-4d.asm  - Use an HD44780U based LCD with an Atmel ATmega processor
;
;   Copyright (C) 2013 Donald Weiman    (weimandn@alfredstate.edu)
;
;   This program is free software: you can redistribute it and/or modify
;   it under the terms of the GNU General Public License as published by
;   the Free Software Foundation, either version 3 of the License, or
;   (at your option) any later version.
;
;   This program is distributed in the hope that it will be useful,
;   but WITHOUT ANY WARRANTY; without even the implied warranty of
;   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;   GNU General Public License for more details.
;
;   You should have received a copy of the GNU General Public License
;   along with this program.  If not, see <http://www.gnu.org/licenses/>.
;
; ***************************************************************************
;        File:    LCD-AVR-4d.asm
;        Date:    September 8, 2013
;
;      Target:    ATmega328
;   Assembler:    Atmel AvrAssembler2 (AVR Studio 6)
;      Author:    Donald Weiman
;	   Modded by: Marco Lastri (garretlabs.wordpress.com) to be used connected to MeeBlip Anode synthesizer
;
;    Hardware:    Arduino, Boarduino, or equivalent, LCD Module
;
;     Summary:    4-bit data interface, busy flag not implemented.
;                 Any LCD pin can be connected to any available I/O port.
;                 Includes a simple write string routine.
;
; ****************************** Program Notes ******************************
;
;           This program uses a 4-bit data interface but does not use the
;             busy flag to determine when the LCD controller is ready.  The
;             LCD RW line (pin 5) is not connected to the uP and it must be
;             connected to GND for the program to function.
;
;           All time delays are longer than those specified in most datasheets
;             in order to accommodate slower than normal LCD modules.  This
;             requirement is well documented but almost always ignored.  The
;             information is in a note at the bottom of the right hand
;             (Execution Time) column of the instruction set.
;
; ***************************************************************************
;
;           The four data lines as well as the two control lines may be
;             implemented on any available I/O pin of any port.  These are
;             the connections used for this program:
;
;                -----------                   ----------
;               | ATmega644 |                 |   LCD    |
;               |           |                 |          |
;               |        PB3|---------------->|D7        |
;               |        PB2|---------------->|D6        |
;               |        PB1|---------------->|D5        |
;               |        PB0|---------------->|D4        |
;               |           |                 |D3        |
;               |           |                 |D2        |
;               |           |                 |D1        |
;               |           |                 |D0        |
;               |           |                 |          |
;               |        PD6|---------------->|E         |
;               |           |         GND --->|RW        |
;               |        PD5|---------------->|RS        |
;                -----------                   ----------
;
; ***************************************************************************
 
 //ML: added to convert integer numbers in strings
 .include "SimpleAsciiConversion.inc"

 .equ    fclk                = 16000000      ; system clock frequency (for delays)
////ML
; register usage
.def    temp_lcd                = R24           ; temporary storage for LCD
.def	RAMtemp					= R25

; LCD interface (should agree with the diagram above)
;   make sure that the LCD RW pin is connected to GND
.equ    lcd_D7_port         = PORTB         ; lcd D7 connection
.equ    lcd_D7_bit          = PORTB3
.equ    lcd_D7_ddr          = DDRB

.equ    lcd_D6_port         = PORTB         ; lcd D6 connection
.equ    lcd_D6_bit          = PORTB2
.equ    lcd_D6_ddr          = DDRB

.equ    lcd_D5_port         = PORTB         ; lcd D5 connection
.equ    lcd_D5_bit          = PORTB1
.equ    lcd_D5_ddr          = DDRB

.equ    lcd_D4_port         = PORTB         ; lcd D4 connection
.equ    lcd_D4_bit          = PORTB0
.equ    lcd_D4_ddr          = DDRB

.equ    lcd_E_port          = PORTD         ; lcd Enable pin
.equ    lcd_E_bit           = PORTD6
.equ    lcd_E_ddr           = DDRD

.equ    lcd_RS_port         = PORTD         ; lcd Register Select pin
.equ    lcd_RS_bit          = PORTD5
.equ    lcd_RS_ddr          = DDRD

; LCD module information
.equ    lcd_LineOne         = 0x00          ; start of line 1
.equ    lcd_LineTwo         = 0x40          ; start of line 2
.equ   lcd_LineThree        = 0x14          ; start of line 3 (20x4)
.equ   lcd_lineFour         = 0x54          ; start of line 4 (20x4)
;.equ   lcd_LineThree        = 0x10          ; start of line 3 (16x4)
;.equ   lcd_lineFour         = 0x50          ; start of line 4 (16x4)

; LCD instructions
.equ    lcd_Clear           = 0b00000001    ; replace all characters with ASCII 'space'
.equ    lcd_Home            = 0b00000010    ; return cursor to first position on first line
.equ    lcd_EntryMode       = 0b00000110    ; shift cursor from left to right on read/write
.equ    lcd_DisplayOff      = 0b00001000    ; turn display off
.equ    lcd_DisplayOn       = 0b00001100    ; display on, cursor off, don't blink character
.equ    lcd_FunctionReset   = 0b00110000    ; reset the LCD
.equ    lcd_FunctionSet4bit = 0b00101000    ; 4-bit data, 2-line display, 5 x 7 font
.equ    lcd_SetCursor       = 0b10000000    ; set cursor position


///////////////////////////////////////////////////////////////////////////////////////

///// ML: Main procedure for LCD Update

///////////////////////////////////////////////////////////////////////////////////////
MYLCD_UPDATE:
			
			//time of update
			in	    r16, TCNT1L		    ;\
		    in	    r17, TCNT1H		    ;/ r17:r16 = t
		    lds	    r18, TPREV_KBD_L	;\
		    lds	    r19, TPREV_KBD_H	;/ r19:r18 = t0
		    sub	    r16, r18			;\
		    sbc	    r17, r19			;/ r17:r16 = t - t0
		    subi	r16, LOW(KBDSCAN)	;\
		    sbci	r17, HIGH(KBDSCAN)	;/ r17:r16 = (t-t0) - 100ms
		    brsh	MLP_LCDUPDATE		    ;\
		    rjmp	MLP_SKIPLCDUPDATE		;/ skip scanning if (t-t0) < 100ms
MLP_LCDUPDATE:
			//status of switches
			ldi		r19, 0
			lds		r18, PATCH_SWITCH1 //take the patch of switches	
			//swithc 0
			bst		r18, 0 //bit 0 to T (which can be 0 or 1)
			bld     r19, 0  //bit 0 of T in r19
			ldi		r20,0x30
			add		r19,r20 //r19 will be 0x30 or 0x31 (ascii "0" or "1")
			sts     my_LCDstring1, r19 
			ldi		r19, 0
			sts     my_LCDstring1+1, r19
			ldi     ZH, high(my_LCDstring1)        ; point to the information that is to be displayed
			ldi     ZL, low(my_LCDstring1)
			ldi     temp_lcd, lcd_LineOne+3         ; point to where the information should be displayed
			call    lcd_write_string_4d

			//swithc 1
			bst		r18, 1 //bit 1 to T (which can be 0 or 1)
			bld     r19, 1  //bit 1 of T in r19
			lsr		r19 //right shift--->bit 1 is now bit 0
			ldi		r20,0x30 //ascii ofsset for numbers
			add		r19,r20 //r19 will be 0x30 or 0x31 (ascii "0" or "1")
			sts     my_LCDstring1, r19 
			ldi		r19, 0
			sts     my_LCDstring1+1, r19
			ldi     ZH, high(my_LCDstring1)        ; point to the information that is to be displayed
			ldi     ZL, low(my_LCDstring1)
			ldi     temp_lcd, lcd_LineOne+8         ; point to where the information should be displayed
			call    lcd_write_string_4d

			//swithc 2
			bst		r18, 2 //bit 2 to T (which can be 0 or 1)
			bld     r19, 2  //bit 2 of T in r19
			lsr		r19
			lsr		r19 //twice right shift--->bit 2 is now bit 0
					//save the value of switch 3 in r24...it will be used to display the values of pot Wave/Sweep
					sts		temp_in_RAM,r19
					//
			ldi		r20,0x30
			add		r19,r20 //r19 will be 0x30 or 0x31 (ascii "0" or "1")
			sts     my_LCDstring1, r19 
			ldi		r19, 0
			sts     my_LCDstring1+1, r19
			ldi     ZH, high(my_LCDstring1)        ; point to the information that is to be displayed
			ldi     ZL, low(my_LCDstring1)
			ldi     temp_lcd, lcd_LineOne+13         ; point to where the information should be displayed
			call    lcd_write_string_4d

			//swithc 3
			bst		r18, 3 //bit 3 to T (which can be 0 or 1)
			bld     r19, 3  //bit 3 of T in r19
			lsr		r19
			lsr		r19
			lsr		r19 //three times right shift--->bit 3 is now bit 0
					
			ldi		r20,0x30
			add		r19,r20 //r19 will be 0x30 or 0x31 (ascii "0" or "1")
			sts     my_LCDstring1, r19 
			ldi		r19, 0
			sts     my_LCDstring1+1, r19
			ldi     ZH, high(my_LCDstring1)        ; point to the information that is to be displayed
			ldi     ZL, low(my_LCDstring1)
			ldi     temp_lcd, lcd_LineOne+18         ; point to where the information should be displayed
			call    lcd_write_string_4d

			

			/////status of pots...in this case, since the pots have a value from 0 to 255, we must convert the value to ascii
			//LFO Freq
			lds	r16,LFOFREQ //<<<<--value of 1st pot (0-255)
			rcall HEX_2_ASCII //this function report the values in r18=hundreds, r17=decimal, r16=units
			mov     temp_lcd, r18
			sts		my_LCDstring4,temp_lcd
			mov     temp_lcd, r17
			sts		my_LCDstring4+1,temp_lcd
			mov     temp_lcd, r16
			sts		my_LCDstring4+2,temp_lcd
			ldi		temp_lcd,0x0
			sts		my_LCDstring4+3,temp_lcd

			ldi     ZH, high(my_LCDstring4)        ; point to the information that is to be displayed
			ldi     ZL, low(my_LCDstring4)
			ldi     temp_lcd, lcd_LineFour+4         ; point to where the information should be displayed
			call    lcd_write_string_4d
			

			//LFO level
			lds	r16,LFOLEVEL//<<<<--value of 1st pot (0-255)
			rcall HEX_2_ASCII //this function report the values in r18=hundreds, r17=decimal, r16=units
			mov     temp_lcd, r18
			sts		my_LCDstring4,temp_lcd
			mov     temp_lcd, r17
			sts		my_LCDstring4+1,temp_lcd
			mov     temp_lcd, r16
			sts		my_LCDstring4+2,temp_lcd
			ldi		temp_lcd,0x0
			sts		my_LCDstring4+3,temp_lcd

			ldi     ZH, high(my_LCDstring4)        ; point to the information that is to be displayed
			ldi     ZL, low(my_LCDstring4)
			ldi     temp_lcd, lcd_LineFour+12         ; point to where the information should be displayed
			call    lcd_write_string_4d
			

			//Attack
			lds	r16,KNOB_AMP_ATTACK//<<<<--value of 1st pot (0-255)
			rcall HEX_2_ASCII //this function report the values in r18=hundreds, r17=decimal, r16=units
			mov     temp_lcd, r18
			sts		my_LCDstring2,temp_lcd
			mov     temp_lcd, r17
			sts		my_LCDstring2+1,temp_lcd
			mov     temp_lcd, r16
			sts		my_LCDstring2+2,temp_lcd
			ldi		temp_lcd,0x0
			sts		my_LCDstring2+3,temp_lcd

			ldi     ZH, high(my_LCDstring2)        ; point to the information that is to be displayed
			ldi     ZL, low(my_LCDstring2)
			ldi     temp_lcd, lcd_LineTwo+3         ; point to where the information should be displayed
			call    lcd_write_string_4d

			//Decay
			lds	r16,KNOB_AMP_DECAY//<<<<--value of 1st pot (0-255)
			rcall HEX_2_ASCII //this function report the values in r18=hundreds, r17=decimal, r16=units
			mov     temp_lcd, r18
			sts		my_LCDstring2,temp_lcd
			mov     temp_lcd, r17
			sts		my_LCDstring2+1,temp_lcd
			mov     temp_lcd, r16
			sts		my_LCDstring2+2,temp_lcd
			ldi		temp_lcd,0x0
			sts		my_LCDstring2+3,temp_lcd

			ldi     ZH, high(my_LCDstring2)        ; point to the information that is to be displayed
			ldi     ZL, low(my_LCDstring2)
			ldi     temp_lcd, lcd_LineTwo+10        ; point to where the information should be displayed
			call    lcd_write_string_4d

			//Cutoff
			lds	r16,CUTOFF//<<<<--value of 1st pot (0-255)
			rcall HEX_2_ASCII //this function report the values in r18=hundreds, r17=decimal, r16=units
			mov     temp_lcd, r18
			sts		my_LCDstring2,temp_lcd
			mov     temp_lcd, r17
			sts		my_LCDstring2+1,temp_lcd
			mov     temp_lcd, r16
			sts		my_LCDstring2+2,temp_lcd
			ldi		temp_lcd,0x0
			sts		my_LCDstring2+3,temp_lcd

			ldi     ZH, high(my_LCDstring2)        ; point to the information that is to be displayed
			ldi     ZL, low(my_LCDstring2)
			ldi     temp_lcd, lcd_LineTwo+17        ; point to where the information should be displayed
			call    lcd_write_string_4d

			//Wavetable or Width
			lds		r16,temp_in_RAM
			cpi		r16,1
			breq	DISPLAY_SWEEP_VAL		
DISPLAY_WAVETABLE_VAL:
			lds	r16,WAVETABLE//<<<<--value of 1st pot (0-255)			
			rcall HEX_2_ASCII //this function report the values in r18=hundreds, r17=decimal, r16=units
			mov     temp_lcd, r18
			sts		my_LCDstring3,temp_lcd
			mov     temp_lcd, r17
			sts		my_LCDstring3+1,temp_lcd
			mov     temp_lcd, r16
			sts		my_LCDstring3+2,temp_lcd
			ldi		temp_lcd,0x0
			sts		my_LCDstring3+3,temp_lcd

			ldi     ZH, high(my_LCDstring3)        ; point to the information that is to be displayed
			ldi     ZL, low(my_LCDstring3)
			ldi     temp_lcd, lcd_LineThree+3      ; point to where the information should be displayed
			call    lcd_write_string_4d
			jmp     DISPLAY_DETUNE

DISPLAY_SWEEP_VAL:
			lds	r16,PULSE_KNOB_LIMITED//<<<<--value of 1st pot (0-255)			
			rcall HEX_2_ASCII //this function report the values in r18=hundreds, r17=decimal, r16=units
			mov     temp_lcd, r18
			sts		my_LCDstring3,temp_lcd
			mov     temp_lcd, r17
			sts		my_LCDstring3+1,temp_lcd
			mov     temp_lcd, r16
			sts		my_LCDstring3+2,temp_lcd
			ldi		temp_lcd,0x0
			sts		my_LCDstring3+3,temp_lcd

			ldi     ZH, high(my_LCDstring3)        ; point to the information that is to be displayed
			ldi     ZL, low(my_LCDstring3)
			ldi     temp_lcd, lcd_LineThree+3      ; point to where the information should be displayed
			call    lcd_write_string_4d

DISPLAY_DETUNE:
			lds	r16,OSC_DETUNE//<<<<--value of 1st pot (0-255)			
			rcall HEX_2_ASCII //this function report the values in r18=hundreds, r17=decimal, r16=units
			mov     temp_lcd, r18
			sts		my_LCDstring3,temp_lcd
			mov     temp_lcd, r17
			sts		my_LCDstring3+1,temp_lcd
			mov     temp_lcd, r16
			sts		my_LCDstring3+2,temp_lcd
			ldi		temp_lcd,0x0
			sts		my_LCDstring3+3,temp_lcd

			ldi     ZH, high(my_LCDstring3)        ; point to the information that is to be displayed
			ldi     ZL, low(my_LCDstring3)
			ldi     temp_lcd, lcd_LineThree+10      ; point to where the information should be displayed
			call    lcd_write_string_4d


MLP_SKIPLCDUPDATE:

			ret

/////////////////////////////////////////////////////////////////////////////////////////
// ML: Procedure to prepare GUI
/////////////////////////////////////////////////////////////////////////////////////////
MYLCD_PREPARE_GUI:

		//first row (====>>> Su:x Oc:x WS:x Lf:x)
		ldi temp_lcd,0x53
		sts my_LCDstring1, temp_lcd
		ldi temp_lcd,0x75
		sts my_LCDstring1+1, temp_lcd
		ldi temp_lcd,0x3A
		sts my_LCDstring1+2, temp_lcd
		ldi temp_lcd,0x20
		sts my_LCDstring1+3, temp_lcd
		ldi temp_lcd,0x20
		sts my_LCDstring1+4, temp_lcd

		ldi temp_lcd,0x4F
		sts my_LCDstring1+5, temp_lcd
		ldi temp_lcd,0x63
		sts my_LCDstring1+6, temp_lcd
		ldi temp_lcd,0x3A
		sts my_LCDstring1+7, temp_lcd
		ldi temp_lcd,0x20
		sts my_LCDstring1+8, temp_lcd
		ldi temp_lcd,0x20
		sts my_LCDstring1+9, temp_lcd

		ldi temp_lcd,0x57
		sts my_LCDstring1+10, temp_lcd
		ldi temp_lcd,0x53
		sts my_LCDstring1+11, temp_lcd
		ldi temp_lcd,0x3A
		sts my_LCDstring1+12, temp_lcd
		ldi temp_lcd,0x20
		sts my_LCDstring1+13, temp_lcd
		ldi temp_lcd,0x20
		sts my_LCDstring1+14, temp_lcd

		ldi temp_lcd,0x4C
		sts my_LCDstring1+15, temp_lcd
		ldi temp_lcd,0x66
		sts my_LCDstring1+16, temp_lcd
		ldi temp_lcd,0x3A
		sts my_LCDstring1+17, temp_lcd
		ldi temp_lcd,0x20
		sts my_LCDstring1+18, temp_lcd
		
		ldi temp_lcd,0x0
		sts my_LCDstring1+19, temp_lcd

		//display the first line of information
		ldi     ZH, high(my_LCDstring1)        ; point to the information that is to be displayed
		ldi     ZL, low(my_LCDstring1)
		ldi     temp_lcd, lcd_LineOne          ; point to where the information should be displayed
		call    lcd_write_string_4d

		

		
		//Second row (====>>> At:xxx Dc:xxx Ct:xxx)
		ldi temp_lcd,0x41
		sts my_LCDstring2, temp_lcd
		ldi temp_lcd,0x74
		sts my_LCDstring2+1, temp_lcd
		ldi temp_lcd,0x3A
		sts my_LCDstring2+2, temp_lcd
		ldi temp_lcd,0x20
		sts my_LCDstring2+3, temp_lcd
		ldi temp_lcd,0x20
		sts my_LCDstring2+4, temp_lcd
		ldi temp_lcd,0x20
		sts my_LCDstring2+5, temp_lcd
		ldi temp_lcd,0x20
		sts my_LCDstring2+6, temp_lcd

		ldi temp_lcd,0x44
		sts my_LCDstring2+7, temp_lcd
		ldi temp_lcd,0x63
		sts my_LCDstring2+8, temp_lcd
		ldi temp_lcd,0x3A
		sts my_LCDstring2+9, temp_lcd
		ldi temp_lcd,0x20
		sts my_LCDstring2+10, temp_lcd
		ldi temp_lcd,0x20
		sts my_LCDstring2+11, temp_lcd
		ldi temp_lcd,0x20
		sts my_LCDstring2+12, temp_lcd
		ldi temp_lcd,0x20
		sts my_LCDstring2+13, temp_lcd

		ldi temp_lcd,0x43
		sts my_LCDstring2+14, temp_lcd
		ldi temp_lcd,0x74
		sts my_LCDstring2+15, temp_lcd
		ldi temp_lcd,0x3A
		sts my_LCDstring2+16, temp_lcd
		ldi temp_lcd,0x20
		sts my_LCDstring2+17, temp_lcd
		ldi temp_lcd,0x20
		sts my_LCDstring2+18, temp_lcd
		ldi temp_lcd,0x20
		sts my_LCDstring2+19, temp_lcd

		ldi temp_lcd,0x0
		sts my_LCDstring2+20, temp_lcd

		//display the second line of information
		ldi     ZH, high(my_LCDstring2)        ; point to the information that is to be displayed
		ldi     ZL, low(my_LCDstring2)
		ldi     temp_lcd, lcd_LineTwo          ; point to where the information should be displayed
		call    lcd_write_string_4d




		//Third row (====>>> Wv:xxx Dt:xxx) //Rz is not displayed since it is not managed by cpu (it is a simple resonance resistor)
		/* Rez:xxx
		ldi temp_lcd,0x52
		sts my_LCDstring3, temp_lcd
		ldi temp_lcd,0x7A
		sts my_LCDstring3+1, temp_lcd
		ldi temp_lcd,0x3A
		sts my_LCDstring3+2, temp_lcd
		ldi temp_lcd,0x20
		sts my_LCDstring3+3, temp_lcd
		ldi temp_lcd,0x20
		sts my_LCDstring3+4, temp_lcd
		ldi temp_lcd,0x20
		sts my_LCDstring3+5, temp_lcd
		ldi temp_lcd,0x20
		sts my_LCDstring3+6, temp_lcd
		*/

		ldi temp_lcd,0x57
		sts my_LCDstring3, temp_lcd
		ldi temp_lcd,0x76
		sts my_LCDstring3+1, temp_lcd
		ldi temp_lcd,0x3A
		sts my_LCDstring3+2, temp_lcd
		ldi temp_lcd,0x20
		sts my_LCDstring3+3, temp_lcd
		ldi temp_lcd,0x20
		sts my_LCDstring3+4, temp_lcd
		ldi temp_lcd,0x20
		sts my_LCDstring3+5, temp_lcd
		ldi temp_lcd,0x20
		sts my_LCDstring3+6, temp_lcd

		ldi temp_lcd,0x44
		sts my_LCDstring3+7, temp_lcd
		ldi temp_lcd,0x74
		sts my_LCDstring3+8, temp_lcd
		ldi temp_lcd,0x3A
		sts my_LCDstring3+9, temp_lcd
		ldi temp_lcd,0x20
		sts my_LCDstring3+10, temp_lcd
		ldi temp_lcd,0x20
		sts my_LCDstring3+111, temp_lcd
		ldi temp_lcd,0x20
		sts my_LCDstring3+12, temp_lcd

		ldi temp_lcd,0x0
		sts my_LCDstring3+13, temp_lcd

		//display the third line of information
		ldi     ZH, high(my_LCDstring3)        ; point to the information that is to be displayed
		ldi     ZL, low(my_LCDstring3)
		ldi     temp_lcd, lcd_LineThree          ; point to where the information should be displayed
		call    lcd_write_string_4d



		//Fourth row (====>>> LRt:xxx LDp:xxx)
		ldi temp_lcd,0x4C
		sts my_LCDstring4, temp_lcd
		ldi temp_lcd,0x52
		sts my_LCDstring4+1, temp_lcd
		ldi temp_lcd,0x74
		sts my_LCDstring4+2, temp_lcd
		ldi temp_lcd,0x3A
		sts my_LCDstring4+3, temp_lcd
		ldi temp_lcd,0x20
		sts my_LCDstring4+4, temp_lcd
		ldi temp_lcd,0x20
		sts my_LCDstring4+5, temp_lcd
		ldi temp_lcd,0x20
		sts my_LCDstring4+6, temp_lcd
		ldi temp_lcd,0x20
		sts my_LCDstring4+7, temp_lcd

		ldi temp_lcd,0x4C
		sts my_LCDstring4+8, temp_lcd
		ldi temp_lcd,0x44
		sts my_LCDstring4+9, temp_lcd
		ldi temp_lcd,0x70
		sts my_LCDstring4+10, temp_lcd
		ldi temp_lcd,0x3A
		sts my_LCDstring4+11, temp_lcd
		ldi temp_lcd,0x20
		sts my_LCDstring4+12, temp_lcd
		ldi temp_lcd,0x20
		sts my_LCDstring4+13, temp_lcd
		ldi temp_lcd,0x20
		sts my_LCDstring4+14, temp_lcd

		ldi temp_lcd,0x0
		sts my_LCDstring4+15, temp_lcd

		//display the fourth line of information
		ldi     ZH, high(my_LCDstring4)        ; point to the information that is to be displayed
		ldi     ZL, low(my_LCDstring4)
		ldi     temp_lcd, lcd_LineFour          ; point to where the information should be displayed
		call    lcd_write_string_4d

		ret

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

MYLCD_CLEAR:
; Clear Display instruction
    ldi     temp_lcd, lcd_Clear                 ; clear display RAM
    call    lcd_write_instruction_4d
    ldi     temp_lcd, 4                         ; 1.64 mS delay (min)
    call    delayTx1mS
	ret


LCD_START:
	program_author:
	.db         "M.Lastri-Garretlabs ",0,0,0,0

	program_version:
	.db         "MeeBlip Anode",0

	program_version2:
	.db         "(Italian Style v1)",0,0

	program_date:
	.db			"15 April 2016",0

	; configure the microprocessor pins for the data lines
    sbi     lcd_D7_ddr, lcd_D7_bit          ; 4 data lines - output
    sbi     lcd_D6_ddr, lcd_D6_bit
    sbi     lcd_D5_ddr, lcd_D5_bit
    sbi     lcd_D4_ddr, lcd_D4_bit

; configure the microprocessor pins for the control lines
    sbi     lcd_E_ddr,  lcd_E_bit           ; E line - output
    sbi     lcd_RS_ddr, lcd_RS_bit          ; RS line - output

; initialize the LCD controller as determined by the equates (LCD instructions)
    call    lcd_init_4d                     ; initialize the LCD display for a 4-bit interface

///ML: I want to read strings from RAM
//We initialize them... whith the infos: "Meeblip Anode Italian Way/ Marco Lastri 2016 /version etc. etc."

//first row 
ldi temp_lcd,0x4d
sts my_LCDstring1, temp_lcd
ldi temp_lcd,0x2e
sts my_LCDstring1+1, temp_lcd
ldi temp_lcd,0x4c
sts my_LCDstring1+2, temp_lcd
ldi temp_lcd,0x61
sts my_LCDstring1+3, temp_lcd
ldi temp_lcd,0x73
sts my_LCDstring1+4, temp_lcd
ldi temp_lcd,0x74
sts my_LCDstring1+5, temp_lcd
ldi temp_lcd,0x72
sts my_LCDstring1+6, temp_lcd
ldi temp_lcd,0x69
sts my_LCDstring1+7, temp_lcd
ldi temp_lcd,0x0
sts my_LCDstring1+8, temp_lcd


//second row
ldi temp_lcd,0x47
sts my_LCDstring2, temp_lcd
ldi temp_lcd,0x61
sts my_LCDstring2+1, temp_lcd
ldi temp_lcd,0x72
sts my_LCDstring2+2, temp_lcd
ldi temp_lcd,0x72
sts my_LCDstring2+3, temp_lcd
ldi temp_lcd,0x65
sts my_LCDstring2+4, temp_lcd
ldi temp_lcd,0x74
sts my_LCDstring2+5, temp_lcd
ldi temp_lcd,0x6c
sts my_LCDstring2+6, temp_lcd
ldi temp_lcd,0x61
sts my_LCDstring2+7, temp_lcd
ldi temp_lcd,0x62
sts my_LCDstring2+8, temp_lcd
ldi temp_lcd,0x73
sts my_LCDstring2+9, temp_lcd
ldi temp_lcd,0x0
sts my_LCDstring2+10, temp_lcd

//third row
ldi temp_lcd,0x4d
sts my_LCDstring3, temp_lcd
ldi temp_lcd,0x65
sts my_LCDstring3+1, temp_lcd
ldi temp_lcd,0x65
sts my_LCDstring3+2, temp_lcd
ldi temp_lcd,0x62
sts my_LCDstring3+3, temp_lcd
ldi temp_lcd,0x6c
sts my_LCDstring3+4, temp_lcd
ldi temp_lcd,0x69
sts my_LCDstring3+5, temp_lcd
ldi temp_lcd,0x70
sts my_LCDstring3+6, temp_lcd
ldi temp_lcd,0x20
sts my_LCDstring3+7, temp_lcd
ldi temp_lcd,0x41
sts my_LCDstring3+8, temp_lcd
ldi temp_lcd,0x6e
sts my_LCDstring3+9, temp_lcd
ldi temp_lcd,0x6f
sts my_LCDstring3+10, temp_lcd
ldi temp_lcd,0x64
sts my_LCDstring3+11, temp_lcd
ldi temp_lcd,0x65
sts my_LCDstring3+12, temp_lcd
ldi temp_lcd,0x0
sts my_LCDstring3+13, temp_lcd

//fourth row //----->SW Version
ldi temp_lcd,0x32
sts my_LCDstring4, temp_lcd
ldi temp_lcd,0x39
sts my_LCDstring4+1, temp_lcd
ldi temp_lcd,0x0
sts my_LCDstring4+2, temp_lcd
ldi temp_lcd,0x0
sts my_LCDstring4+3, temp_lcd



; display the first line of information
    ldi     ZH, high(my_LCDstring1)        ; point to the information that is to be displayed
    ldi     ZL, low(my_LCDstring1)
    ldi     temp_lcd, lcd_LineOne               ; point to where the information should be displayed
    call    lcd_write_string_4d

; display the second line of information
    ldi     ZH, high(my_LCDstring2)       ; point to the information that is to be displayed
    ldi     ZL, low(my_LCDstring2)
    ldi     temp_lcd, lcd_LineTwo               ; point to where the information should be displayed
    call    lcd_write_string_4d

; display the third line of information
    ldi     ZH, high(my_LCDstring3)       ; point to the information that is to be displayed
    ldi     ZL, low(my_LCDstring3)
    ldi     temp_lcd, lcd_LineThree               ; point to where the information should be displayed
    call    lcd_write_string_4d

////ML posizionamento di due cifre nella quarta riga, alla colonna 8
	ldi     ZH, high(my_LCDstring4)       ; point to the information that is to be displayed
    ldi     ZL, low(my_LCDstring4)
    ldi     temp_lcd,lcd_LineFour+8             ; point to where the information should be displayed
    call    lcd_write_string_4d
	
	ret

 ; ============================== 4-bit LCD Subroutines ======================
; Name:     lcd_init_4d
; Purpose:  initialize the LCD module for a 4-bit data interface
; Entry:    equates (LCD instructions) set up for the desired operation
; Exit:     no parameters
; Notes:    uses time delays instead of checking the busy flag

lcd_init_4d:
; Power-up delay
    ldi     temp_lcd, 100                       ; initial 40 mSec delay
    call    delayTx1mS

; IMPORTANT - At this point the LCD module is in the 8-bit mode and it is expecting to receive  
;    8 bits of data, one bit on each of its 8 data lines, each time the 'E' line is pulsed.
;
;  Since the LCD module is wired for the 4-bit mode, only the upper four data lines are connected to 
;    the microprocessor and the lower four data lines are typically left open.  Therefore, when 
;    the 'E' line is pulsed, the LCD controller will read whatever data has been set up on the upper 
;    four data lines and the lower four data lines will be high (due to internal pull-up circuitry).
;
;  Fortunately the 'FunctionReset' instruction does not care about what is on the lower four bits so  
;    this instruction can be sent on just the four available data lines and it will be interpreted 
;    properly by the LCD controller.  The 'lcd_write_4' subroutine will accomplish this if the 
;    control lines have previously been configured properly.

; Set up the RS and E lines for the 'lcd_write_4' subroutine.
    cbi     lcd_RS_port, lcd_RS_bit         ; select the Instruction Register (RS low)
    cbi     lcd_E_port, lcd_E_bit           ; make sure E is initially low

; Reset the LCD controller.
    ldi     temp_lcd, lcd_FunctionReset         ; first part of reset sequence
    call    lcd_write_4
    ldi     temp_lcd, 10                        ; 4.1 mS delay (min)
    call    delayTx1mS

    ldi     temp_lcd, lcd_FunctionReset         ; second part of reset sequence
    call    lcd_write_4
    ldi     temp_lcd, 200                       ; 100 uS delay (min)
    call    delayTx1uS

    ldi     temp_lcd, lcd_FunctionReset         ; third part of reset sequence
    call    lcd_write_4
    ldi     temp_lcd, 200                       ; this delay is omitted in the data sheet
    call    delayTx1uS

; Preliminary Function Set instruction - used only to set the 4-bit mode.
; The number of lines or the font cannot be set at this time since the controller is still in the 
;   8-bit mode, but the data transfer mode can be changed since this parameter is determined by one 
;   of the upper four bits of the instruction.
    ldi     temp_lcd, lcd_FunctionSet4bit       ; set 4-bit mode
    call    lcd_write_4
    ldi     temp_lcd, 80                        ; 40 uS delay (min)
    call    delayTx1uS

; Function Set instruction
    ldi     temp_lcd, lcd_FunctionSet4bit       ; set mode, lines, and font
    call    lcd_write_instruction_4d
    ldi     temp_lcd, 80                        ; 40 uS delay (min)
    call    delayTx1uS

; The next three instructions are specified in the data sheet as part of the initialization routine,
;   so it is a good idea (but probably not necessary) to do them just as specified and then redo them
;   later if the application requires a different configuration.

; Display On/Off Control instruction
    ldi     temp_lcd, lcd_DisplayOff            ; turn display OFF
    call    lcd_write_instruction_4d
    ldi     temp_lcd, 80                        ; 40 uS delay (min)
    call    delayTx1uS

; Clear Display instruction
    ldi     temp_lcd, lcd_Clear                 ; clear display RAM
    call    lcd_write_instruction_4d
    ldi     temp_lcd, 4                         ; 1.64 mS delay (min)
    call    delayTx1mS

; Entry Mode Set instruction
    ldi     temp_lcd, lcd_EntryMode             ; set desired shift characteristics
    call    lcd_write_instruction_4d
    ldi     temp_lcd, 80                        ; 40 uS delay (min)
    call    delayTx1uS

; This is the end of the LCD controller initialization as specified in the data sheet, but the display
;   has been left in the OFF condition.  This is a good time to turn the display back ON.

; Display On/Off Control instruction
    ldi     temp_lcd, lcd_DisplayOn             ; turn the display ON
    call    lcd_write_instruction_4d
    ldi     temp_lcd, 80                        ; 40 uS delay (min)
    call    delayTx1uS
    ret

; ---------------------------------------------------------------------------
; Name:     lcd_write_string_4d
; Purpose:  display a string of characters on the LCD
; Entry:    ZH and ZL pointing to the start of the string
;           (temp_lcd) contains the desired DDRAM address at which to start the display
; Exit:     no parameters
; Notes:    the string must end with a null (0)
;           uses time delays instead of checking the busy flag

lcd_write_string_4d:
; preserve registers
    push    ZH                              ; preserve pointer registers
    push    ZL

//ML: rimuovo visto che prendo dati dalla RAM
; fix up the pointers for use with the 'lpm' instruction
    ;lsl     ZL                              ; shift the pointer one bit left for the lpm instruction
    ;rol     ZH

; set up the initial DDRAM address
    ori     temp_lcd, lcd_SetCursor             ; convert the plain address to a set cursor instruction
    call   lcd_write_instruction_4d         ; set up the first DDRAM address
    ldi     temp_lcd, 80                        ; 40 uS delay (min)
    call    delayTx1uS

; write the string of characters
lcd_write_string_4d_01:
    ;lpm     temp_lcd, Z+                        ; ///ML: I remove this since i Read from RAM the data
	ld    temp_lcd, Z+							; ////ML ----> so get a character from RAM
    cpi     temp_lcd,  0                        ; check for end of string
    breq    lcd_write_string_4d_02          ; done

; arrive here if this is a valid character
    call    lcd_write_character_4d          ; display the character
    ldi     temp_lcd, 80                        ; 40 uS delay (min)
    call    delayTx1uS
    rjmp    lcd_write_string_4d_01          ; not done, send another character

; arrive here when all characters in the message have been sent to the LCD module
lcd_write_string_4d_02:
    pop     ZL                              ; restore pointer registers
    pop     ZH
    ret

; ---------------------------------------------------------------------------
; Name:     lcd_write_character_4d
; Purpose:  send a byte of information to the LCD data register
; Entry:    (temp_lcd) contains the data byte
; Exit:     no parameters
; Notes:    does not deal with RW (busy flag is not implemented)

lcd_write_character_4d:
    sbi     lcd_RS_port, lcd_RS_bit         ; select the Data Register (RS high)
    cbi     lcd_E_port, lcd_E_bit           ; make sure E is initially low
    call    lcd_write_4                     ; write the upper 4-bits of the data
    swap    temp_lcd                            ; swap high and low nibbles
    call    lcd_write_4                     ; write the lower 4-bits of the data
    ret

; ---------------------------------------------------------------------------
; Name:     lcd_write_instruction_4d
; Purpose:  send a byte of information to the LCD instruction register
; Entry:    (temp_lcd) contains the data byte
; Exit:     no parameters
; Notes:    does not deal with RW (busy flag is not implemented)

lcd_write_instruction_4d:
    cbi     lcd_RS_port, lcd_RS_bit         ; select the Instruction Register (RS low)
    cbi     lcd_E_port, lcd_E_bit           ; make sure E is initially low
    call    lcd_write_4                     ; write the upper 4-bits of the instruction
    swap    temp_lcd                            ; swap high and low nibbles
    call    lcd_write_4                     ; write the lower 4-bits of the instruction
    ret

; ---------------------------------------------------------------------------
; Name:     lcd_write_4
; Purpose:  send a nibble (4-bits) of information to the LCD module
; Entry:    (temp_lcd) contains a byte of data with the desired 4-bits in the upper nibble
;           (RS) is configured for the desired LCD register
;           (E) is low
;           (RW) is low
; Exit:     no parameters
; Notes:    use either time delays or the busy flag

lcd_write_4:
; set up D7
    sbi     lcd_D7_port, lcd_D7_bit         ; assume that the D7 data is '1'
    sbrs    temp_lcd, 7                         ; check the actual data value
    cbi     lcd_D7_port, lcd_D7_bit         ; arrive here only if the data was actually '0'

; set up D6
    sbi     lcd_D6_port, lcd_D6_bit         ; repeat for each data bit
    sbrs    temp_lcd, 6
    cbi     lcd_D6_port, lcd_D6_bit

; set up D5
    sbi     lcd_D5_port, lcd_D5_bit
    sbrs    temp_lcd, 5
    cbi     lcd_D5_port, lcd_D5_bit

; set up D4
    sbi     lcd_D4_port, lcd_D4_bit
    sbrs    temp_lcd, 4 
    cbi     lcd_D4_port, lcd_D4_bit

; write the data
                                            ; 'Address set-up time' (40 nS)
    sbi     lcd_E_port, lcd_E_bit           ; Enable pin high
    call    delay1uS                        ; implement 'Data set-up time' (80 nS) and 'Enable pulse width' (230 nS)
    cbi     lcd_E_port, lcd_E_bit           ; Enable pin low
    call    delay1uS                        ; implement 'Data hold time' (10 nS) and 'Enable cycle time' (500 nS)
    ret

; ============================== End of 4-bit LCD Subroutines ===============

; ============================== Time Delay Subroutines =====================
; Name:     delayYx1mS
; Purpose:  provide a delay of (YH:YL) x 1 mS
; Entry:    (YH:YL) = delay data
; Exit:     no parameters
; Notes:    the 16-bit register provides for a delay of up to 65.535 Seconds
;           requires delay1mS

delayYx1mS:
    call    delay1mS                        ; delay for 1 mS
    sbiw    YH:YL, 1                        ; update the the delay counter
    brne    delayYx1mS                      ; counter is not zero

; arrive here when delay counter is zero (total delay period is finished)
    ret

; ---------------------------------------------------------------------------
; Name:     delayTx1mS
; Purpose:  provide a delay of (temp_lcd) x 1 mS
; Entry:    (temp_lcd) = delay data
; Exit:     no parameters
; Notes:    the 8-bit register provides for a delay of up to 255 mS
;           requires delay1mS

delayTx1mS:
    call    delay1mS                        ; delay for 1 mS
    dec     temp_lcd                            ; update the delay counter
    brne    delayTx1mS                      ; counter is not zero

; arrive here when delay counter is zero (total delay period is finished)
    ret

; ---------------------------------------------------------------------------
; Name:     delay1mS
; Purpose:  provide a delay of 1 mS
; Entry:    no parameters
; Exit:     no parameters
; Notes:    chews up fclk/1000 clock cycles (including the 'call')

delay1mS:
    push    YL                              ; [2] preserve registers
    push    YH                              ; [2]
    ldi     YL, low (((fclk/1000)-18)/4)    ; [1] delay counter
    ldi     YH, high(((fclk/1000)-18)/4)    ; [1]

delay1mS_01:
    sbiw    YH:YL, 1                        ; [2] update the the delay counter
    brne    delay1mS_01                     ; [2] delay counter is not zero

; arrive here when delay counter is zero
    pop     YH                              ; [2] restore registers
    pop     YL                              ; [2]
    ret                                     ; [4]

; ---------------------------------------------------------------------------
; Name:     delayTx1uS
; Purpose:  provide a delay of (temp_lcd) x 1 uS with a 16 MHz clock frequency
; Entry:    (temp_lcd) = delay data
; Exit:     no parameters
; Notes:    the 8-bit register provides for a delay of up to 255 uS
;           requires delay1uS

delayTx1uS:
    call    delay1uS                        ; delay for 1 uS
    dec     temp_lcd                            ; decrement the delay counter
    brne    delayTx1uS                      ; counter is not zero

; arrive here when delay counter is zero (total delay period is finished)
    ret

; ---------------------------------------------------------------------------
; Name:     delay1uS
; Purpose:  provide a delay of 1 uS with a 16 MHz clock frequency
; Entry:    no parameters
; Exit:     no parameters
; Notes:    add another push/pop for 20 MHz clock frequency

delay1uS:
    push    temp_lcd                            ; [2] these instructions do nothing except consume clock cycles
    pop     temp_lcd                            ; [2]
    push    temp_lcd                            ; [2]
    pop     temp_lcd                            ; [2]
    ret                                     ; [4]

; ============================== End of Time Delay Subroutines ==============
