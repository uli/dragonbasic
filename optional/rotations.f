{ -- MF/TIN ROTATION library
  -- Copyright (c) 2003 by Jeff Massung }

\ confine an angle to within 0-359
:n (wrap-hi) ( n1 -- n2 )
	dup 360 # >= if 180 # - 180 # - then ;

\ confine an angle to within 0-359
:n (wrap-lo) ( n1 -- n2 )
	dup 0< if 180 # + 180 # + then ;

\ confine an angle to within 0-359
: wrapangle ( n1 -- n2 ) (wrap-lo) (wrap-hi) ;

\ sine and cosine lookup table... each index is a 
\ 24.8 fixed-point value where the upper 2 bytes
\ are the sine and lower 2 bytes are the cosine

label __sincos

$00000100 , $000400FF , $000800FF , $000D00FF , $001100FF , $001600FF , 
$001A00FE , $001F00FE , $002300FD , $002800FC , $002C00FC , $003000FB , 
$003500FA , $003900F9 , $003D00F8 , $004200F7 , $004600F6 , $004A00F4 , 
$004F00F3 , $005300F2 , $005700F0 , $005B00EE , $005F00ED , $006400EB , 
$006800E9 , $006C00E8 , $007000E6 , $007400E4 , $007800E2 , $007C00DF , 
$007F00DD , $008300DB , $008700D9 , $008B00D6 , $008F00D4 , $009200D1 , 
$009600CF , $009A00CC , $009D00C9 , $00A100C6 , $00A400C4 , $00A700C1 , 
$00AB00BE , $00AE00BB , $00B100B8 , $00B500B5 , $00B800B1 , $00BB00AE , 
$00BE00AB , $00C100A7 , $00C400A4 , $00C600A1 , $00C9009D , $00CC009A , 
$00CF0096 , $00D10092 , $00D4008F , $00D6008B , $00D90087 , $00DB0083 , 
$00DD007F , $00DF007C , $00E20078 , $00E40074 , $00E60070 , $00E8006C , 
$00E90068 , $00EB0064 , $00ED005F , $00EE005B , $00F00057 , $00F20053 , 
$00F3004F , $00F4004A , $00F60046 , $00F70042 , $00F8003D , $00F90039 , 
$00FA0035 , $00FB0030 , $00FC002C , $00FC0028 , $00FD0023 , $00FE001F , 
$00FE001A , $00FF0016 , $00FF0011 , $00FF000D , $00FF0008 , $00FF0004 , 
$01000000 , $00FFFFFC , $00FFFFF8 , $00FFFFF3 , $00FFFFEF , $00FFFFEA , 
$00FEFFE6 , $00FEFFE1 , $00FDFFDD , $00FCFFD8 , $00FCFFD4 , $00FBFFD0 , 
$00FAFFCB , $00F9FFC7 , $00F8FFC3 , $00F7FFBE , $00F6FFBA , $00F4FFB6 , 
$00F3FFB1 , $00F2FFAD , $00F0FFA9 , $00EEFFA5 , $00EDFFA1 , $00EBFF9C , 
$00E9FF98 , $00E8FF94 , $00E6FF90 , $00E4FF8C , $00E2FF88 , $00DFFF84 , 
$00DDFF80 , $00DBFF7D , $00D9FF79 , $00D6FF75 , $00D4FF71 , $00D1FF6E , 
$00CFFF6A , $00CCFF66 , $00C9FF63 , $00C6FF5F , $00C4FF5C , $00C1FF59 , 
$00BEFF55 , $00BBFF52 , $00B8FF4F , $00B5FF4B , $00B1FF48 , $00AEFF45 , 
$00ABFF42 , $00A7FF3F , $00A4FF3C , $00A1FF3A , $009DFF37 , $009AFF34 , 
$0096FF31 , $0092FF2F , $008FFF2C , $008BFF2A , $0087FF27 , $0083FF25 , 
$007FFF23 , $007CFF21 , $0078FF1E , $0074FF1C , $0070FF1A , $006CFF18 , 
$0068FF17 , $0064FF15 , $005FFF13 , $005BFF12 , $0057FF10 , $0053FF0E , 
$004FFF0D , $004AFF0C , $0046FF0A , $0042FF09 , $003DFF08 , $0039FF07 , 
$0035FF06 , $0030FF05 , $002CFF04 , $0028FF04 , $0023FF03 , $001FFF02 , 
$001AFF02 , $0016FF01 , $0011FF01 , $000DFF01 , $0008FF01 , $0004FF01 , 
$0000FF00 , $FFFCFF01 , $FFF8FF01 , $FFF3FF01 , $FFEFFF01 , $FFEAFF01 , 
$FFE6FF02 , $FFE1FF02 , $FFDDFF03 , $FFD8FF04 , $FFD4FF04 , $FFD0FF05 , 
$FFCBFF06 , $FFC7FF07 , $FFC3FF08 , $FFBEFF09 , $FFBAFF0A , $FFB6FF0C , 
$FFB1FF0D , $FFADFF0E , $FFA9FF10 , $FFA5FF12 , $FFA1FF13 , $FF9CFF15 , 
$FF98FF17 , $FF94FF18 , $FF90FF1A , $FF8CFF1C , $FF88FF1E , $FF84FF21 , 
$FF80FF23 , $FF7DFF25 , $FF79FF27 , $FF75FF2A , $FF71FF2C , $FF6EFF2F , 
$FF6AFF31 , $FF66FF34 , $FF63FF37 , $FF5FFF3A , $FF5CFF3C , $FF59FF3F , 
$FF55FF42 , $FF52FF45 , $FF4FFF48 , $FF4BFF4B , $FF48FF4F , $FF45FF52 , 
$FF42FF55 , $FF3FFF59 , $FF3CFF5C , $FF3AFF5F , $FF37FF63 , $FF34FF66 , 
$FF31FF6A , $FF2FFF6E , $FF2CFF71 , $FF2AFF75 , $FF27FF79 , $FF25FF7D , 
$FF23FF81 , $FF21FF84 , $FF1EFF88 , $FF1CFF8C , $FF1AFF90 , $FF18FF94 , 
$FF17FF98 , $FF15FF9C , $FF13FFA1 , $FF12FFA5 , $FF10FFA9 , $FF0EFFAD , 
$FF0DFFB1 , $FF0CFFB6 , $FF0AFFBA , $FF09FFBE , $FF08FFC3 , $FF07FFC7 , 
$FF06FFCB , $FF05FFD0 , $FF04FFD4 , $FF04FFD8 , $FF03FFDD , $FF02FFE1 , 
$FF02FFE6 , $FF01FFEA , $FF01FFEF , $FF01FFF3 , $FF01FFF8 , $FF01FFFC , 
$FF000000 , $FF010004 , $FF010008 , $FF01000D , $FF010011 , $FF010016 , 
$FF02001A , $FF02001F , $FF030023 , $FF040028 , $FF04002C , $FF050030 , 
$FF060035 , $FF070039 , $FF08003D , $FF090042 , $FF0A0046 , $FF0C004A , 
$FF0D004F , $FF0E0053 , $FF100057 , $FF12005B , $FF13005F , $FF150064 , 
$FF170068 , $FF18006C , $FF1A0070 , $FF1C0074 , $FF1E0078 , $FF21007C , 
$FF230080 , $FF250083 , $FF270087 , $FF2A008B , $FF2C008F , $FF2F0092 , 
$FF310096 , $FF34009A , $FF37009D , $FF3A00A1 , $FF3C00A4 , $FF3F00A7 , 
$FF4200AB , $FF4500AE , $FF4800B1 , $FF4B00B5 , $FF4F00B8 , $FF5200BB , 
$FF5500BE , $FF5900C1 , $FF5C00C4 , $FF5F00C6 , $FF6300C9 , $FF6600CC , 
$FF6A00CF , $FF6E00D1 , $FF7100D4 , $FF7500D6 , $FF7900D9 , $FF7D00DB , 
$FF8100DD , $FF8400DF , $FF8800E2 , $FF8C00E4 , $FF9000E6 , $FF9400E8 , 
$FF9800E9 , $FF9C00EB , $FFA100ED , $FFA500EE , $FFA900F0 , $FFAD00F2 , 
$FFB100F3 , $FFB600F4 , $FFBA00F6 , $FFBE00F7 , $FFC300F8 , $FFC700F9 , 
$FFCB00FA , $FFD000FB , $FFD400FC , $FFD800FC , $FFDD00FD , $FFE100FE , 
$FFE600FE , $FFEA00FF , $FFEF00FF , $FFF300FF , $FFF800FF , $FFFC00FF , 

\ lookup a 16.16 fixed point cosine
code-thumb cos# ( degrees -- f )
	__sincos w literal
	2 ## tos tos lsl,
	w tos +( tos ldr,
	16 ## w mov,
	w tos ror,
	16 ## tos tos asr,
	ret
end-code

\ lookup a 16.16 fixed point sine
code-thumb sin# ( degrees -- f )
	__sincos w literal
	2 ## tos tos lsl,
	w tos +( tos ldr,
	16 ## tos tos asr,
	ret
end-code

\ tangent is a function of sin and cos
code-thumb tan# ( degrees -- f )
	__sincos w literal
	2 ## tos tos lsl,
	w tos +( tos ldr,
	
	\ cos -> a
	16 ## tos a lsl,
	16 ## a a asr,
	
	\ sin -> tos and divide
	8 ## tos tos asr,
	6 swi,
	ret
end-code

\ create a sprite rotation matrix
code-thumb makerotation ( matrix sx sy degrees -- )
	v0 v1 v2 pop

	5 ## v2 v2 lsl,
	$30 ## a mov,
	20 ## a a lsl,	\ IWRAM
	a v2 v2 add,
	v2 r6 r7 push

	
	\ set the table address
	__sincos a literal 
	2 ## tos tos lsl,
	a tos +( r6 ldr, 
	16 ## r6 a asr,		\ sin
	16 ## r6 r6 lsl,
	16 ## r6 r6 asr,	\ cos

	\ calculate rotation parameters
	v1 r7 mov,
	r6 r7 mul,		\ pa = (x*cos)
	v0 v2 mov,
	a v2 mul,		\ pb = (y*sin)
	a a neg,		\ -sin
	v1 tos mov,
	a tos mul,		\ pc = (x*-sin)
	v0 w mov,
	r6 w mul,		\ pd = (y*cos)
	
	\ shift back down
	8 ## r7 r7 asr,
	8 ## v2 v0 asr,
	8 ## tos tos asr,
	8 ## w w asr,

	\ write
	v2 pop
	v2 $06 #( r7 strh,
	v2 $0e #( v0 strh,
	v2 $16 #( tos strh,
	v2 $1e #( w strh,

	\ done
	r6 r7 pop
	tos pop
	ret
end-code

\ set the rotation matrix and bit for a sprite
code-thumb rotatesprite ( sprite matrix -- )
	v2 pop
	
	\ offset to sprite address
	3 ## v2 v2 lsl,
	$30 ## a mov,
	20 ## a a lsl,	\ IWRAM
	a v2 v2 add,
	
	\ set rotation bit
	v2 0@ v1 ldrh,
	1 ## a mov,
	8 ## a a lsl,	\ $100
	a v1 orr,
	
	\ check turn off and store
	0 ## tos cmp,
	4 #offset pl? b,
	a v1 eor,
	v2 0@ v1 strh,
	2 ## v2 add,
	
	\ set matrix index
	v2 0@ v1 ldrh,
	$e ## a mov,
	8 ## a a lsl,	\ $e00
	a v1 bic,
	0 ## tos cmp,
	6 #offset mi? b,
	9 ## tos tos lsl,
	tos v1 orr,
	v2 0@ v1 strh,
	
	\ done
	tos pop
	ret
end-code
