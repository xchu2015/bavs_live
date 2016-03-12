;*****************************************************************************
;* bavs_predict.asm: cavs decoder library
;*****************************************************************************


%include "bavs_x86inc.asm"
%include "bavs_x86util.asm"

SECTION_RODATA


pw_76543210:
pw_3210:    dw 0, 1, 2, 3, 4, 5, 6, 7
pb_0_1:		times 8 db 0
			times 8 db 1
pb_2_3:		times 8 db 2
			times 8 db 3
pb_4_5:		times 8 db 4
			times 8 db 5
pb_6_7:		times 8 db 6
			times 8 db 7
pb_shuf_00: db 0, 0, 1, 2, 3, 4, 5, 6, 0, 0, 0, 0, 0, 0, 0, 0
pb_shuf_77: db 1, 2, 3, 4, 5, 6, 7, 7, 0, 0, 0, 0, 0, 0, 0, 0
pb_shuf_4x4: times 4 db 3
			 times 4 db 2
			 times 4 db 1
			 times 4 db 0
pb_128:  times 8 db	128
pw_43210123: dw -3, -2, -1, 0, 1, 2, 3, 4
pw_m3:       times 8 dw -3
pw_m7:       times 8 dw -7
pb_00s_ff:   times 8 db 0
pb_0s_ff:    times 7 db 0
             db 0xff

shuf_fixtr:  db 0, 1, 2, 3, 4, 5, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7
shuf_nop:    db 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
shuf_hu:     db 7,6,5,4,3,2,1,0,0,0,0,0,0,0,0,0
shuf_vr:     db 2,4,6,8,9,10,11,12,13,14,15,0,1,3,5,7
pw_reverse:  db 14,15,12,13,10,11,8,9,6,7,4,5,2,3,0,1

SECTION .text

cextern pb_0
cextern pb_1
cextern pb_3
cextern pw_1
cextern pw_2
cextern pw_4
cextern pw_8
cextern pw_16
cextern pw_00ff

;-----------------------------------------------------------------------------------------------------------------
; void cavs_predict_8x8_h_mmxext( uint8_t *p_dest, uint8_t edge[33], int i_stride, uint8_t *p_top, uint8_t *p_left );
;-----------------------------------------------------------------------------------------------------------------
INIT_MMX
%macro	LOAD_B_FILL_ALL 2
%if WIN64
    mov		dl, %2		;rdx = r1
    mov		dh, %2
    movd	%1, r1d
    pshufw	%1, %1, 0x00
%elif UNIX64
    mov		dl, %2		;rdx = r2
    mov		dh, %2
    movd	%1, r2d
    pshufw	%1, %1, 0x00

%else
    mov		cl, %2		;ecx = r1
    mov		ch, %2
    movd	%1, ecx
    pshufw	%1, %1, 0x00

%endif
%endmacro

cglobal predict_8x8_h_mmxext, 5, 7
    mov r5, r2

%if WIN64	
    mov r6, r4
    LOAD_B_FILL_ALL m0, [r6+1]
    LOAD_B_FILL_ALL m1, [r6+2]
    LOAD_B_FILL_ALL m2, [r6+3]
    LOAD_B_FILL_ALL m3, [r6+4]
    LOAD_B_FILL_ALL m4, [r6+5]
    LOAD_B_FILL_ALL m5, [r6+6]
    LOAD_B_FILL_ALL m6, [r6+7]
    LOAD_B_FILL_ALL m7, [r6+8]
%elif UNIX64
    mov r6, r4
    LOAD_B_FILL_ALL m0, [r6+1]
    LOAD_B_FILL_ALL m1, [r6+2]
    LOAD_B_FILL_ALL m2, [r6+3]
    LOAD_B_FILL_ALL m3, [r6+4]
    LOAD_B_FILL_ALL m4, [r6+5]
    LOAD_B_FILL_ALL m5, [r6+6]
    LOAD_B_FILL_ALL m6, [r6+7]
    LOAD_B_FILL_ALL m7, [r6+8]
%else
    LOAD_B_FILL_ALL m0, [r4+1]
    LOAD_B_FILL_ALL m1, [r4+2]
    LOAD_B_FILL_ALL m2, [r4+3]
    LOAD_B_FILL_ALL m3, [r4+4]
    LOAD_B_FILL_ALL m4, [r4+5]
    LOAD_B_FILL_ALL m5, [r4+6]
    LOAD_B_FILL_ALL m6, [r4+7]
    LOAD_B_FILL_ALL m7, [r4+8]
%endif

    mova        [r0 + 0*r5], m0
    mova        [r0 + 1*r5], m1

    mov         r6, r5 ;i_stride
    add          r6, r5 ;2*i_stride 	
    mova        [r0 + r6], m2

    add          r6, r5 ;3*i_stride 	
    mova        [r0 + r6], m3

    add          r6, r5 ;4*i_stride 	
    mova        [r0 + r6], m4

    add          r6, r5 ;5*i_stride 	
    mova        [r0 + r6], m5

    add          r6, r5 ;6*i_stride 	
    mova        [r0 + r6], m6

    add          r6, r5 ;7*i_stride 	
    mova        [r0 + r6], m7
    RET

;-------------------------------------------------------------------------------------------------------------------
; void cavs_predict_8x8_v_mmxext( uint8_t *p_dest, uint8_t edge[33], int i_stride, uint8_t *p_top, uint8_t *p_left );
;-------------------------------------------------------------------------------------------------------------------
cglobal predict_8x8_v_mmxext, 5,7
    mova        m0, [r3+1];[r0 - FDEC_STRIDE] change to [p_top + 1]
    mova        [r0 + 0*r2], m0
    mova        [r0 + 1*r2], m0

    mov         r5, r2 ;i_stride
    add         r5, r2 ;2*i_stride 	
    mova        [r0 + r5], m0

    add         r5, r2 ;3*i_stride 
    mova        [r0 + r5], m0

    add         r5, r2 ;4*i_stride 	
    mova        [r0 + r5], m0

    add         r5, r2 ;5*i_stride 
    mova        [r0 + r5], m0

    add         r5, r2 ;6*i_stride 
    mova        [r0 + r5], m0

    add         r5, r2 ;7*i_stride 
    mova        [r0 + r5], m0
    RET

;------------------------------------------------------------------------------------------------------
; void cavs_predict_8x8_dc_128_mmxext( uint8_t *p_dest, uint8_t edge[33], int i_stride, uint8_t *p_top, uint8_t *p_left )
;------------------------------------------------------------------------------------------------------
cglobal	predict_8x8_dc_128_mmxext, 5, 7
    mova		m0, [pb_128]

    mova        [r0 + 0*r2], m0
    mova        [r0 + 1*r2], m0

    mov         r5, r2 ;i_stride
    add         r5, r2 ;2*i_stride 	
    mova        [r0 + r5], m0

    add         r5, r2 ;3*i_stride 	
    mova        [r0 + r5], m0

    add         r5, r2 ;4*i_stride 	
    mova        [r0 + r5], m0

    add         r5, r2 ;5*i_stride 	
    mova        [r0 + r5], m0

    add         r5, r2 ;6*i_stride 	
    mova        [r0 + r5], m0

    add         r5, r2 ;7*i_stride 	
    mova        [r0 + r5], m0
    RET

;-------------------------------------------------------------------------------------------------------------------------
; void cavs_predict_8x8_dc_top_mmxext( uint8_t *p_dest, uint8_t edge[33], int i_stride, uint8_t *p_top, uint8_t *p_left );
;-------------------------------------------------------------------------------------------------------------------------
cglobal	predict_8x8_dc_top_mmxext, 5, 7
    mova		m0, [r1+16+1]	

    mova        [r0 + 0*r2], m0
    mova        [r0 + 1*r2], m0

    mov			r5, r2 
    add			r5, r2;2*i_stride
    mova        [r0 + r5], m0

    add         r5, r2;3*i_stride
    mova        [r0 + r5], m0

    add         r5, r2;4*i_stride
    mova        [r0 + r5], m0

    add         r5, r2;5*i_stride
    mova        [r0 + r5], m0

    add         r5, r2;6*i_stride
    mova        [r0 + r5], m0

    add         r5, r2;7*i_stride
    mova        [r0 + r5], m0
    RET

;--------------------------------------------------------------------------------------------------------------------------
; void cavs_predict_8x8_dc_left_mmxext( uint8_t *p_dest, uint8_t edge[33], int i_stride, uint8_t *p_top, uint8_t *p_left );
;--------------------------------------------------------------------------------------------------------------------------

%macro	LOAD_B_FILL_ALL2 2
%if WIN64
    mov		al, %2		;rax = r6
    mov		ah, %2
    movd	       %1, r6d
    pshufw	%1, %1, 0x00
%elif UNIX64
    mov		al, %2		;rax = r6
    mov		ah, %2
    movd	       %1, r6d
    pshufw	%1, %1, 0x00
%else
    mov		dl, %2		;edx = r2
    mov		dh, %2
    movd	       %1, edx
    pshufw	%1, %1, 0x00
%endif    
%endmacro

cglobal	predict_8x8_dc_left_mmxext, 5,7
    mov  r5, r2

    LOAD_B_FILL_ALL2	m0, [r1+16-1]
    LOAD_B_FILL_ALL2	m1, [r1+16-2]
    LOAD_B_FILL_ALL2	m2, [r1+16-3]
    LOAD_B_FILL_ALL2	m3, [r1+16-4]
    LOAD_B_FILL_ALL2	m4, [r1+16-5]
    LOAD_B_FILL_ALL2	m5, [r1+16-6]
    LOAD_B_FILL_ALL2	m6, [r1+16-7]
    LOAD_B_FILL_ALL2	m7, [r1+16-8]

    mova        [r0 + 0*r5], m0
    mova        [r0 + 1*r5], m1

	mov         r6, r5
	add         r6, r5; 2*i_stride
    mova        [r0 + r6], m2

	add         r6, r5; 3*i_stride
    mova        [r0 + r6], m3

	add         r6, r5; 4*i_stride
    mova        [r0 + r6], m4

	add         r6, r5; 5*i_stride
    mova        [r0 + r6], m5

	add         r6, r5; 6*i_stride
    mova        [r0 + r6], m6

	add         r6, r5; 7*i_stride
    mova        [r0 + r6], m7
    RET

INIT_XMM
%macro  LOAD_B_AS_W 2
	movh		%1, %2
	punpcklbw	%1, m7
%endmacro
%macro	LOAD_B_AS_W_R 3
	LOAD_B_AS_W	%1, %3
	pshuflw	%2, %1, 00011011b
	pshufhw	%1, %1, 00011011b
	movhlps	%1, %1
	movlhps	%1, %2
%endmacro
%macro	LOAD_B_AS_W_FILL_ALL 3
	mov			%3b, %2
	movd		%1, %3
	pshuflw	%1, %1, 0
	movlhps	%1, %1
%endmacro
%macro	DO_8X8_DC 2
	LOAD_B_AS_W_FILL_ALL	m1, %2, r2
	paddw		m1, m0
	psrlw		m1, 1
	packuswb 	m1, m1
	movh		%1, m1
%endmacro

;---------------------------------------------------------------------------------------------------------------------
; void cavs_predict_8x8_dc_mmxext( uint8_t *p_dest, uint8_t edge[33], int i_stride, uint8_t *p_top, uint8_t *p_left );
;---------------------------------------------------------------------------------------------------------------------
cglobal	predict_8x8_dc_mmxext, 5, 7
    mov			r5, r2

	pxor		m7, m7
	LOAD_B_AS_W	m0, [r1+16+1]
	xor			r2d, r2d
	DO_8X8_DC	[r0 + 0*r5], [r1+16-1]
	DO_8X8_DC	[r0 + 1*r5], [r1+16-2]
	
	mov			r6, r5
	add			r6, r5;2*i_stride
	DO_8X8_DC	[r0 + r6], [r1+16-3]
	
	add			r6, r5;3*i_stride
	DO_8X8_DC	[r0 + r6], [r1+16-4]
	
	add			r6, r5;4*i_stride
	DO_8X8_DC	[r0 + r6], [r1+16-5]
	
	add			r6, r5;5*i_stride
	DO_8X8_DC	[r0 + r6], [r1+16-6]
	
	add			r6, r5;6*i_stride
	DO_8X8_DC	[r0 + r6], [r1+16-7]
	
	add			r6, r5;7*i_stride
	DO_8X8_DC	[r0 + r6], [r1+16-8]
	RET

%macro DO_8X8_DDL 3
	LOAD_B_AS_W	m0, %2
	LOAD_B_AS_W_R m1, m2, %3
	paddw		m0, m1
	psrlw		m0, 1
	packuswb	m0, m0
	movh		%1, m0
%endmacro

;----------------------------------------------------------------------------------------------------------------------
; void cavs_predict_8x8_ddl_mmxext( uint8_t *p_dest, uint8_t edge[33], int i_stride, uint8_t *p_top, uint8_t *p_left );
;----------------------------------------------------------------------------------------------------------------------
cglobal	predict_8x8_ddl_mmxext, 5, 7
	pxor		m7, m7
	
	DO_8X8_DDL	[r0 + 0*r2], [r1+16+2], [r1+16-2-7]
	DO_8X8_DDL	[r0 + 1*r2], [r1+16+3], [r1+16-3-7]

	mov			r5, r2
	add         r5, r2;2*i_stride
	DO_8X8_DDL	[r0 + r5], [r1+16+4], [r1+16-4-7]

	add         r5, r2;3*i_stride
	DO_8X8_DDL	[r0 + r5], [r1+16+5], [r1+16-5-7]

	add         r5, r2;4*i_stride
	DO_8X8_DDL	[r0 + r5], [r1+16+6], [r1+16-6-7]

	add         r5, r2;5*i_stride
	DO_8X8_DDL	[r0 + r5], [r1+16+7], [r1+16-7-7]
	
	add         r5, r2;6*i_stride
	DO_8X8_DDL	[r0 + r5], [r1+16+8], [r1+16-8-7]
	
	add         r5, r2;7*i_stride
	DO_8X8_DDL	[r0 + r5], [r1+16+9], [r1+16-9-7]
	RET

;-------------------------------------------------------------------------------------------------------------------
; void cavs_predict_8x8_ddr_mmxext( uint8_t *src, uint8_t edge[33], int i_stride, uint8_t *p_top, uint8_t *p_left );
;-------------------------------------------------------------------------------------------------------------------
INIT_MMX
cglobal	predict_8x8_ddr_mmxext, 5, 7
    mova	m0, [r1+16]
    mova	m1, [r1+16-1]
    mova	m2, [r1+16-2]
    mova	m3, [r1+16-3]
    mova	m4, [r1+16-4]
    mova	m5, [r1+16-5]
    mova	m6, [r1+16-6]
    mova	m7, [r1+16-7]

    mova        [r0 + 0*r2], m0
    mova        [r0 + 1*r2], m1

    mov         r5, r2
    add         r5, r2;2*i_stride
    mova        [r0 + r5], m2

    add         r5, r2;3*i_stride
    mova        [r0 + r5], m3

    add         r5, r2;4*i_stride
    mova        [r0 + r5], m4

    add         r5, r2;5*i_stride
    mova        [r0 + r5], m5

    add         r5, r2;6*i_stride
    mova        [r0 + r5], m6

    add         r5, r2;7*i_stride
    mova        [r0 + r5], m7
    RET

;-----------------------------------------------------------------------------
;chroma intra
;-----------------------------------------------------------------------------
;-----------------------------------------------------------------------------------------------------------------
; void cavs_predict_8x8c_h_mmxext( uint8_t *p_dest, int neighbor, int i_stride, uint8_t *p_top, uint8_t *p_left );
;-----------------------------------------------------------------------------------------------------------------
cglobal predict_8x8c_h_mmxext, 5, 7
    mov r5, r2

%if WIN64	
    mov r6, r4
    LOAD_B_FILL_ALL m0, [r6+1]
    LOAD_B_FILL_ALL m1, [r6+2]
    LOAD_B_FILL_ALL m2, [r6+3]
    LOAD_B_FILL_ALL m3, [r6+4]
    LOAD_B_FILL_ALL m4, [r6+5]
    LOAD_B_FILL_ALL m5, [r6+6]
    LOAD_B_FILL_ALL m6, [r6+7]
    LOAD_B_FILL_ALL m7, [r6+8]
%elif UNIX64
    mov r6, r4
    LOAD_B_FILL_ALL m0, [r6+1]
    LOAD_B_FILL_ALL m1, [r6+2]
    LOAD_B_FILL_ALL m2, [r6+3]
    LOAD_B_FILL_ALL m3, [r6+4]
    LOAD_B_FILL_ALL m4, [r6+5]
    LOAD_B_FILL_ALL m5, [r6+6]
    LOAD_B_FILL_ALL m6, [r6+7]
    LOAD_B_FILL_ALL m7, [r6+8]
%else
    LOAD_B_FILL_ALL m0, [r4+1]
    LOAD_B_FILL_ALL m1, [r4+2]
    LOAD_B_FILL_ALL m2, [r4+3]
    LOAD_B_FILL_ALL m3, [r4+4]
    LOAD_B_FILL_ALL m4, [r4+5]
    LOAD_B_FILL_ALL m5, [r4+6]
    LOAD_B_FILL_ALL m6, [r4+7]
    LOAD_B_FILL_ALL m7, [r4+8]
%endif

    mova        [r0 + 0*r5], m0
    mova        [r0 + 1*r5], m1

	mov         r6, r5;i_stride
	add         r6, r5;2*i_stride
    mova        [r0 + r6], m2

	add         r6, r5;3*i_stride
    mova        [r0 + r6], m3

	add         r6, r5;4*i_stride
    mova        [r0 + r6], m4

	add         r6, r5;5*i_stride
    mova        [r0 + r6], m5

	add         r6, r5;6*i_stride
    mova        [r0 + r6], m6

	add         r6, r5;7*i_stride
    mova        [r0 + r6], m7
    RET

;-----------------------------------------------------------------------------------------------------------------
; void cavs_predict_8x8c_v_mmxext( uint8_t *p_dest, int neighbor, int i_stride, uint8_t *p_top, uint8_t *p_left );
;-----------------------------------------------------------------------------------------------------------------
cglobal predict_8x8c_v_mmxext, 5, 7
    mova        m0, [r3+1];[r0 - FDEC_STRIDE] change to [p_top + 1]
    mova        [r0 + 0*r2], m0
    mova        [r0 + 1*r2], m0

    mov         r5, r2 ;i_stride
    add         r5, r2 ;2*i_stride 	
    mova        [r0 + r5], m0

    add         r5, r2 ;3*i_stride 
    mova        [r0 + r5], m0

    add         r5, r2 ;4*i_stride 	
    mova        [r0 + r5], m0

    add         r5, r2 ;5*i_stride 
    mova        [r0 + r5], m0

    add         r5, r2 ;6*i_stride 
    mova        [r0 + r5], m0

    add         r5, r2 ;7*i_stride 
    mova        [r0 + r5], m0
    RET

;----------------------------------------------------------------------------------------------------------------------
; void cavs_predict_8x8c_dc_128_mmxext( uint8_t *p_dest, int neighbor, int i_stride, uint8_t *p_top, uint8_t *p_left );
;----------------------------------------------------------------------------------------------------------------------
cglobal predict_8x8c_dc_128_mmxext, 5, 7
    mova		m0, [pb_128]

    mova        [r0 + 0*r2], m0
    mova        [r0 + 1*r2], m0

    mov         r5, r2 ;i_stride
    add         r5, r2 ;2*i_stride 	
    mova        [r0 + r5], m0

    add         r5, r2 ;3*i_stride 	
    mova        [r0 + r5], m0

    add         r5, r2 ;4*i_stride 	
    mova        [r0 + r5], m0

    add         r5, r2 ;5*i_stride 	
    mova        [r0 + r5], m0

    add         r5, r2 ;6*i_stride 	
    mova        [r0 + r5], m0

    add         r5, r2 ;7*i_stride 	
    mova        [r0 + r5], m0
    RET

;--------------------------------------------------------------------------------------------------------------------------
; void cavs_predict_8x8c_dc_top_sse4( uint8_t *p_dest, int neighbor, int i_stride, uint8_t *p_top, uint8_t *p_left );
; void cavs_predict_8x8c_dc_left_core_sse4( uint8_t *p_dest, uint8_t left[8], int i_stride, uint8_t *p_top, uint8_t *p_left );
; void cavs_predict_8x8c_dc_core_sse4( uint8_t *p_dest, int neighbor, uint8_t left[8], int i_stride, uint8_t *p_top, uint8_t *p_left );
;--------------------------------------------------------------------------------------------------------------------------

%assign MB_LEFT		1
%assign MB_TOP		2
%assign MB_TOPRIGHT 4
%assign MB_TOPLEFT	8
%assign MB_DOWNLEFT 16

%macro DC_TWO_LINE 3
	mova		m6, m1
	pshufb		m6, %3
	
	mova             m7, m6;
	pxor             m7, m0;
	;pxor		m7, m6, m0; NOTE : not right on Intel(R) Xeon(R) CPU X5650 
	pand		m7, [pb_1]
	pavgb		m6, m0
	psubusb	      m6, m7
	movq		%1, m6
	psrldq		m6, 8
	movq		%2, m6
%endmacro

%macro PREDICT_8x8_LOAD_DC_TOP 0
	movq	m0, [r3+1];[r0 - FDEC_STRIDE] ;top
	test	r1, MB_TOPLEFT
	jnz		.have_tl
	mova	m1, m0
	pshufb	m1, [pb_shuf_00]
	jmp		.go_ahead
.have_tl:
	movq	m1, [r3];[r0 - FDEC_STRIDE - 1] ; top_left
.go_ahead:
	test	r1, MB_TOPRIGHT
	jnz		.have_tr
	mova	m2, m0
	pshufb	m2, [pb_shuf_77]
	jmp		.go_ahead2
.have_tr:
	movq	m2, [r3+2];[r0 - FDEC_STRIDE + 1] ;top_right
.go_ahead2:
	pxor		m3, m1, m2
	pand		m3, [pb_1]
	pavgb		m1, m2
	psubusb	m1, m3
	pavgb		m0, m1
%endmacro

%macro PREDICT_8x8_LOAD_DC 0
	movq	m0, [r4+1];[r0 - FDEC_STRIDE] ;top
	test	r1, MB_TOPLEFT
	jnz		.have_tl
	mova	m1, m0
	pshufb	m1, [pb_shuf_00]
	jmp		.go_ahead
.have_tl:
	movq	m1, [r4];[r0 - FDEC_STRIDE - 1] ; top_left
.go_ahead:
	test	r1, MB_TOPRIGHT
	jnz		.have_tr
	mova	m2, m0
	pshufb	m2, [pb_shuf_77]
	jmp		.go_ahead2
.have_tr:
	movq	m2, [r4+2];[r0 - FDEC_STRIDE + 1] ;top_right
.go_ahead2:
	pxor		m3, m1, m2
	pand		m3, [pb_1]
	pavgb		m1, m2
	psubusb	m1, m3
	pavgb		m0, m1
%endmacro

%macro PREDICT_8X8C_DC 0
cglobal	predict_8x8c_dc_top, 5, 7
	PREDICT_8x8_LOAD_DC_TOP
	;%assign x 0
	;%rep 8
	;	movq	[r0 + x*FDEC_STRIDE], m0
	;	%assign x x+1
	;%endrep

	movq	[r0 + 0*r2], m0
	movq	[r0 + 1*r2], m0

	mov     r5, r2 ;i_stride
       add     r5, r2 ;2*i_stride 	
	movq	[r0 + r5], m0

	add     r5, r2 ;3*i_stride 
	movq	[r0 + r5], m0

	add     r5, r2 ;4*i_stride 
	movq	[r0 + r5], m0

	add     r5, r2 ;5*i_stride 
	movq	[r0 + r5], m0

	add     r5, r2 ;6*i_stride 
	movq	[r0 + r5], m0

	add     r5, r2 ;7*i_stride 
	movq	[r0 + r5], m0
	REP_RET

cglobal	predict_8x8c_dc_left_core, 5, 7
	movq		m7, [r1]
	pshufb		m6, m7, [pb_0_1]

	movq		[r0 + 0*r2], m6
	psrldq		m6, 8
	movq		[r0 + 1*r2], m6
	pshufb		m6, m7, [pb_2_3]
	
	mov         r5, r2 ;i_stride
    add         r5, r2 ;2*i_stride 	
	movq		[r0 + r5], m6
	psrldq		m6, 8
	
	add         r5, r2 ;3*i_stride 
	movq		[r0 + r5], m6
	pshufb		m6, m7, [pb_4_5]
	
	add         r5, r2 ;4*i_stride 
	movq		[r0 + r5], m6
	psrldq		m6, 8
	
	add         r5, r2 ;5*i_stride 
	movq		[r0 + r5], m6
	pshufb		m6, m7, [pb_6_7]
	
	add         r5, r2 ;6*i_stride 
	movq		[r0 + r5], m6
	psrldq		m6, 8
	
	add         r5, r2 ;7*i_stride 
	movq		[r0 + r5], m6
	REP_RET


cglobal	predict_8x8c_dc_core, 6, 7
	PREDICT_8x8_LOAD_DC
	movlhps	m0, m0
	movq	m1, [r2]
	
	DC_TWO_LINE	[r0 + 0*r3], [r0 + 1*r3], [pb_0_1]
	;DC_TWO_LINE	[r0 + 2*FDEC_STRIDE], [r0 + 3*FDEC_STRIDE], [pb_2_3]
	;DC_TWO_LINE	[r0 + 4*FDEC_STRIDE], [r0 + 5*FDEC_STRIDE], [pb_4_5]
	;DC_TWO_LINE	[r0 + 6*FDEC_STRIDE], [r0 + 7*FDEC_STRIDE], [pb_6_7]

	mova		m6, m1
	pshufb		m6, [pb_2_3]	
	mova        m7, m6;
	pxor        m7, m0;
	;pxor		m7, m6, m0; NOTE : not right on Intel(R) Xeon(R) CPU X5650 
	pand		m7, [pb_1]
	pavgb		m6, m0
	psubusb	    m6, m7
	mov         r6, r3
    add         r6, r3;2*i_stride
	movq		[r0 + r6], m6
	psrldq		m6, 8
    add      r6, r3;3*i_stride
	movq		[r0 + r6], m6

    mova		m6, m1
	pshufb		m6, [pb_4_5]	
	mova        m7, m6;
	pxor        m7, m0;
	;pxor		m7, m6, m0; NOTE : not right on Intel(R) Xeon(R) CPU X5650 
	pand		m7, [pb_1]
	pavgb		m6, m0
	psubusb	    m6, m7
    add         r6, r3;4*i_stride
	movq		[r0 + r6], m6
	psrldq		m6, 8
    add         r6, r3;5*i_stride
	movq		[r0 + r6], m6

    mova		m6, m1
	pshufb		m6, [pb_6_7]	
	mova        m7, m6;
	pxor        m7, m0;
	;pxor		m7, m6, m0; NOTE : not right on Intel(R) Xeon(R) CPU X5650 
	pand		m7, [pb_1]
	pavgb		m6, m0
	psubusb	    m6, m7
    add         r6, r3;6*i_stride
	movq		[r0 + r6], m6
	psrldq		m6, 8
    add         r6, r3;7*i_stride
	movq		[r0 + r6], m6

	REP_RET
%endmacro


INIT_XMM sse4
PREDICT_8X8C_DC

INIT_MMX
;----------------------------------------------------------------------------------------------
; void cavs_predict_8x8c_p_core_mmxext( uint8_t *p_dest, int i00, int b, int c, int i_stride );
;----------------------------------------------------------------------------------------------
%macro LOAD_PLANE_ARGS 0
%if ARCH_X86_64==1
    movd        mm0, r1d
    movd        mm2, r2d
    movd        mm4, r3d
    pshufw      mm0, mm0, 0
    pshufw      mm2, mm2, 0
    pshufw      mm4, mm4, 0

	movsxd      r4, r4d;i_stride
%else
    pshufw      mm0, r1m, 0
    pshufw      mm2, r2m, 0
    pshufw      mm4, r3m, 0
%endif
%endmacro


cglobal predict_8x8c_p_core_mmxext, 5,7
    LOAD_PLANE_ARGS
    movq        mm1, mm2
    pmullw      mm2, [pw_3210]
    psllw       mm1, 2
    paddsw      mm0, mm2        ; mm0 = {i+0*b, i+1*b, i+2*b, i+3*b}
    paddsw      mm1, mm0        ; mm1 = {i+4*b, i+5*b, i+6*b, i+7*b}

    mov         r1d, 8
ALIGN 4
.loop:
    movq        mm5, mm0
    movq        mm6, mm1
    psraw       mm5, 5
    psraw       mm6, 5
    packuswb    mm5, mm6
    movq        [r0], mm5

    paddsw      mm0, mm4
    paddsw      mm1, mm4
    ;add         r0, FDEC_STRIDE
%if ARCH_X86_64==1    
	add         r0, r4
%else
    add         r0, r4d 
%endif
	dec         r1d
    jg          .loop
    REP_RET