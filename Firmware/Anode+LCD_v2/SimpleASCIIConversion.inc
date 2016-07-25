;-----------------------------------------------
; HEX TO ASCII
;-----------------------------------------------
;I think, this was the smallest (only 10 words). 
                                    
;input: R16 = 8 bit value 0 ... 255 
;output: R18, R17, R16 = digits 
;bytes: 20 
; 
HEX_2_ASCII:

bcd: 
	ldi r18, -1 + '0' 
_bcd1: 
	inc r18 
	subi r16, 100 
	brcc _bcd1 
	ldi r17, 10 + '0' 
_bcd2: 
	dec r17 
	subi r16, -10 
	brcs _bcd2 
	sbci r16, -'0' 
ret  
