;*****************************************************************************
;* deblock-a.asm: x86 deblocking
;*****************************************************************************

%include "bavs_x86inc.asm"
%include "bavs_x86util.asm"

SECTION_RODATA

pw_3:	times 8 dw 3
transpose_shuf: db 0,4,8,12,1,5,9,13,2,6,10,14,3,7,11,15

SECTION .text

cextern pb_0
cextern pb_1
cextern pb_3
cextern pb_a1
cextern pw_2
cextern pw_4
cextern pw_00ff
cextern pw_pixel_max

;======================================================================
;
; deblock asm
;
;======================================================================

%if HIGH_BIT_DEPTH == 0
; expands to [base],...,[base+7*stride]
%define PASS8ROWS(base, base3, stride, stride3) \
    [base], [base+stride], [base+stride*2], [base3], \
    [base3+stride], [base3+stride*2], [base3+stride3], [base3+stride*4]

%define PASS8ROWS(base, base3, stride, stride3, offset) \
    PASS8ROWS(base+offset, base3+offset, stride, stride3)

; in: 8 rows of 4 bytes in %4..%11
; out: 4 rows of 8 bytes in m0..m3
%macro TRANSPOSE4x8_LOAD 11
    movh       m0, %4
    movh       m2, %5
    movh       m1, %6
    movh       m3, %7
    punpckl%1  m0, m2
    punpckl%1  m1, m3
    mova       m2, m0
    punpckl%2  m0, m1
    punpckh%2  m2, m1

    movh       m4, %8
    movh       m6, %9
    movh       m5, %10
    movh       m7, %11
    punpckl%1  m4, m6
    punpckl%1  m5, m7
    mova       m6, m4
    punpckl%2  m4, m5
    punpckh%2  m6, m5

    punpckh%3  m1, m0, m4
    punpckh%3  m3, m2, m6
    punpckl%3  m0, m4
    punpckl%3  m2, m6
%endmacro

; in: 4 rows of 8 bytes in m0..m3
; out: 8 rows of 4 bytes in %1..%8
%macro TRANSPOSE8x4B_STORE 8
    punpckhdq  m4, m0, m0
    punpckhdq  m5, m1, m1
    punpckhdq  m6, m2, m2

    punpcklbw  m0, m1
    punpcklbw  m2, m3
    punpcklwd  m1, m0, m2
    punpckhwd  m0, m2
    movh       %1, m1
    punpckhdq  m1, m1
    movh       %2, m1
    movh       %3, m0
    punpckhdq  m0, m0
    movh       %4, m0

    punpckhdq  m3, m3
    punpcklbw  m4, m5
    punpcklbw  m6, m3
    punpcklwd  m5, m4, m6
    punpckhwd  m4, m6
    movh       %5, m5
    punpckhdq  m5, m5
    movh       %6, m5
    movh       %7, m4
    punpckhdq  m4, m4
    movh       %8, m4
%endmacro

;%macro TRANSPOSE4x8B_LOAD 8
;    TRANSPOSE4x8_LOAD bw, wd, dq, %1, %2, %3, %4, %5, %6, %7, %8
;%endmacro

%macro TRANSPOSE4x8W_LOAD 8
%if mmsize==16
    TRANSPOSE4x8_LOAD wd, dq, qdq, %1, %2, %3, %4, %5, %6, %7, %8
%else
    SWAP  1, 4, 2, 3
    mova  m0, [t5]
    mova  m1, [t5+r1]
    ;mova  m2, [t5+r1*2]
    mova  m2, [t5+r1+r1]
    mova  m3, [t5+t6]
    TRANSPOSE4x4W 0, 1, 2, 3, 4
%endif
%endmacro

%macro TRANSPOSE8x2W_STORE 8
    punpckhwd  m0, m1, m2 ;intel X5650
    ;mova m0, m1;
    ;punpckhwd m0, m2;
    
    punpcklwd  m1, m2
%if mmsize==8
    movd       %3, m0
    movd       %1, m1
    psrlq      m1, 32
    psrlq      m0, 32
    movd       %2, m1
    movd       %4, m0
%else
    movd       %5, m0
    movd       %1, m1
    psrldq     m1, 4
    psrldq     m0, 4
    movd       %2, m1
    movd       %6, m0
    psrldq     m1, 4
    psrldq     m0, 4
    movd       %3, m1
    movd       %7, m0
    psrldq     m1, 4
    psrldq     m0, 4
    movd       %4, m1
    movd       %8, m0
%endif
%endmacro

%macro SBUTTERFLY3 4
    punpckh%1  %4, %2, %3
    punpckl%1  %2, %3
%endmacro

; in: 8 rows of 8 (only the middle 6 pels are used) in %1..%8
; out: 6 rows of 8 in [%9+0*16] .. [%9+5*16]
%macro TRANSPOSE6x8_MEM 9
    RESET_MM_PERMUTATION
    movq  m0, %1
    movq  m1, %2
    movq  m2, %3
    movq  m3, %4
    movq  m4, %5
    movq  m5, %6
    movq  m6, %7
    SBUTTERFLY bw, 0, 1, 7
    SBUTTERFLY bw, 2, 3, 7
    SBUTTERFLY bw, 4, 5, 7
    movq  [%9+0x10], m3
    SBUTTERFLY3 bw, m6, %8, m7
    SBUTTERFLY wd, 0, 2, 3
    SBUTTERFLY wd, 4, 6, 3
    punpckhdq m0, m4
    movq  [%9+0x00], m0
    SBUTTERFLY3 wd, m1, [%9+0x10], m3
    SBUTTERFLY wd, 5, 7, 0
    SBUTTERFLY dq, 1, 5, 0
    SBUTTERFLY dq, 2, 6, 0
    punpckldq m3, m7
    movq  [%9+0x10], m2
    movq  [%9+0x20], m6
    movq  [%9+0x30], m1
    movq  [%9+0x40], m5
    movq  [%9+0x50], m3
    RESET_MM_PERMUTATION
%endmacro

; in: 8 rows of 8 in %1..%8
; out: 8 rows of 8 in %9..%16
%macro TRANSPOSE8x8_MEM 16
    RESET_MM_PERMUTATION
    movq  m0, %1
    movq  m1, %2
    movq  m2, %3
    movq  m3, %4
    movq  m4, %5
    movq  m5, %6
    movq  m6, %7
    SBUTTERFLY bw, 0, 1, 7
    SBUTTERFLY bw, 2, 3, 7
    SBUTTERFLY bw, 4, 5, 7
    SBUTTERFLY3 bw, m6, %8, m7
    movq  %9,  m5
    SBUTTERFLY wd, 0, 2, 5
    SBUTTERFLY wd, 4, 6, 5
    SBUTTERFLY wd, 1, 3, 5
    movq  %11, m6
    movq  m6,  %9
    SBUTTERFLY wd, 6, 7, 5
    SBUTTERFLY dq, 0, 4, 5
    SBUTTERFLY dq, 1, 6, 5
    movq  %9,  m0
    movq  %10, m4
    movq  %13, m1
    movq  %14, m6
    SBUTTERFLY3 dq, m2, %11, m0
    SBUTTERFLY dq, 3, 7, 4
    movq  %11, m2
    movq  %12, m0
    movq  %15, m3
    movq  %16, m7
    RESET_MM_PERMUTATION
%endmacro

%macro TRANSPOSE8x8W_UNALIGNED 9-11

%if %0<11
    MOV_ALIGN16 %9, m%7
%endif
    SBUTTERFLY wd,  %1, %2, %7
    MOV_ALIGN16 %10, m%2
    MOV_ALIGN16 m%7, %9
    SBUTTERFLY wd,  %3, %4, %2
    SBUTTERFLY wd,  %5, %6, %2
    SBUTTERFLY wd,  %7, %8, %2
    SBUTTERFLY dq,  %1, %3, %2
    MOV_ALIGN16 %9, m%3
    MOV_ALIGN16 m%2, %10
    SBUTTERFLY dq,  %2, %4, %3
    SBUTTERFLY dq,  %5, %7, %3
    SBUTTERFLY dq,  %6, %8, %3
    SBUTTERFLY qdq, %1, %5, %3
    SBUTTERFLY qdq, %2, %6, %3
    MOV_ALIGN16 %10, m%2
    MOV_ALIGN16 m%3, %9
    SBUTTERFLY qdq, %3, %7, %2
    SBUTTERFLY qdq, %4, %8, %2
    SWAP %2, %5
    SWAP %4, %7
%if %0<11
    MOV_ALIGN16 m%5, %10
%endif

%endmacro

; out: %4 = |%1-%2|>%3
; clobbers: %5
%macro DIFF_GT 5
%if avx_enabled == 0
    mova    %5, %2
    mova    %4, %1
    psubusb %5, %1
    psubusb %4, %2
%else
    psubusb %5, %2, %1
    psubusb %4, %1, %2
%endif
    por     %4, %5
    psubusb %4, %3
%endmacro

; out: %4 = |%1-%2|>%3
; clobbers: %5
%macro DIFF_GT2 5

    mova    %5, %2
    mova    %4, %1
    ;movu %5, %2
    ;movu %4, %1
    psubusb %5, %1
    psubusb %4, %2

    psubusb %5, %3
    psubusb %4, %3
    pcmpeqb %4, %5
%endmacro

; out: %4 = |%1-%2|>%3
; clobbers: %5
; note: %2 is unalgined
%macro DIFF_GT2_UNALIGNED 5

    movu    %5, %2
    mova    %4, %1
    psubusb %4, %5
    psubusb %5, %1

    psubusb %5, %3
    psubusb %4, %3
    pcmpeqb %4, %5
%endmacro

; in: m0=p1 m1=p0 m2=q0 m3=q1 %1=alpha-1 %2=beta-1
; out: m5=beta-1, m7=mask, %3=alpha-1
; clobbers: m4,m6
%macro LOAD_MASK 2-3
    movd     m4, %1
    movd     m5, %2
    SPLATW   m4, m4
    SPLATW   m5, m5
    packuswb m4, m4  ; 16x alpha-1
    packuswb m5, m5  ; 16x beta-1
%if %0>2
    ;mova     %3, m4
    MOV_ALIGN16 %3, m4
%endif
    DIFF_GT  m1, m2, m4, m7, m6 ; |p0-q0| > alpha-1
    DIFF_GT  m0, m1, m5, m4, m6 ; |p1-p0| > beta-1
    por      m7, m4
    DIFF_GT  m3, m2, m5, m4, m6 ; |q1-q0| > beta-1
    por      m7, m4
    pxor     m6, m6
    pcmpeqb  m7, m6
%endmacro

;m0=p1, m1=p0, m2=q0, m5=q1, m4=(tc&mask)
;tmp: m6, m7
;dst: m1 = p0', m2 = q0'
%macro XAVS_DEBLOCK_P0_Q0 1
	pxor		m6, m6
	punpcklbw	m7, m1, m6
	punpcklbw	m2, m6
	psubw		m2, m7
	pmullw		m2, [pw_3]
	punpcklbw	m7, m0, m6
	paddw		m2, m7
	punpcklbw	m7, m5, m6
	psubw		m2, m7
	paddw		m2, [pw_4]
	psraw		m2, 3
	
	movh		m7, [r0+8]
	punpcklbw	m7, m6
	punpckhbw	m1, m6
	psubw		m7, m1
	pmullw		m7, [pw_3]
	punpckhbw	m0, m6
	paddw		m7, m0
	punpckhbw	m5, m6
	psubw		m7, m5
	paddw		m7, [pw_4]
	psraw		m7, 3

	packsswb	m2, m7
	psubsb		m6, m2
	pxor		m5, m5
	pcmpgtb	m7, m6, m5
	pand		m6, m7
	pcmpgtb	m7, m2, m5
	pand		m7, m2
	pminub		m7, m4
	pminub		m6, m4
%ifidn	%1, 1
	;movu		m1, [r4+2*r1] ; p0
	movu		m1, [r4+r1+r1] ; p0
	movu		m2, [r0]      ; q0
%else
	;mova		m1, [r4+2*r1] ; p0
	movu		m1, [r4+r1+r1] ; p0
	mova		m2, [r0]      ; q0
%endif
	psubusb	m1, m6
	psubusb	m2, m7
	paddusb	m1, m7
	paddusb	m2, m6
%endmacro

;m5=q1, m3=q2, m1=p0', m2=q0', m4=(tc&mask)
;tmp: m0, m6, m7
;dst: m6=q1'
%macro XAVS_LUMA_Q1 1
	pxor		m0, m0
	punpcklbw	m6, m5, m0
	punpcklbw	m7, m2, m0
	psubw		m6, m7
	pmullw		m6, [pw_3]
	punpcklbw	m7, m1, m0
	paddw		m6, m7
	punpcklbw	m7, m3, m0
	psubw		m6, m7
	paddw		m6, [pw_4]
	psraw		m6, 3

	punpckhbw	m5, m0
	punpckhbw	m7, m2, m0
	psubw		m5, m7
	pmullw		m5, [pw_3]
	punpckhbw	m7, m1, m0
	paddw		m5, m7
	punpckhbw	m3, m0
	psubw		m5, m3
	paddw		m5, [pw_4]
	psraw		m5, 3

	packsswb	m6, m5
	psubsb		m0, m6
	pxor		m7, m7
	pcmpgtb	m3, m6, m7
	pand		m3, m6
	pcmpgtb	m5, m0, m7
	pand		m5, m0
	pminub		m3, m4
	pminub		m5, m4
%ifidn %1, 1
	movu		m6, [r0+r1] ;q1
%else
	mova		m6, [r0+r1] ;q1
%endif
	psubusb	m6, m3
	paddusb	m6, m5
%endmacro

;m5=p1, m3=p2, m1=p0', m2=q0', m4=(tc&mask)
;tmp: m0, m6, m7
;dst: m5=p1'
%macro XAVS_LUMA_P1 0
	pxor		m0, m0
	punpcklbw	m6, m1, m0
	punpcklbw	m7, m5, m0
	psubw		m6, m7
	pmullw		m6, [pw_3]
	punpcklbw	m7, m3, m0
	paddw		m6, m7
	punpcklbw	m7, m2, m0
	psubw		m6, m7
	paddw		m6, [pw_4]
	psraw		m6, 3

	punpckhbw	m1, m0
	punpckhbw	m7, m5, m0
	psubw		m1, m7
	pmullw		m1, [pw_3]
	punpckhbw	m3, m0
	paddw		m1, m3
	punpckhbw	m2, m0
	psubw		m1, m2
	paddw		m1, [pw_4]
	psraw		m1, 3

	packsswb	m6, m1
	psubsb		m0, m6
	pxor		m7, m7
	pcmpgtb	m3, m6, m7
	pand		m3, m6
	pcmpgtb	m2, m0, m7
	pand		m2, m0
	pminub		m3, m4
	pminub		m2, m4
	psubusb	m5, m2
	paddusb	m5, m3
%endmacro

%if ARCH_X86_64==1
INIT_XMM sse2
;-----------------------------------------------------------------------------
; void deblock_v_luma( uint8_t *pix, int stride, int alpha, int beta, int8_t *tc0 )
;-----------------------------------------------------------------------------
;%macro DEBLOCK_LUMA 0
cglobal deblock_v_luma, 5, 11
%if UNIX64
    %define param4  r10

    mov   param4, r4mp
%endif

    lea     r4, [r1*3]
    dec     r2     ; alpha-1
    neg     r4
    dec     r3     ; beta-1
    add     r4, r0 ; pix-3*stride

    %assign pad 2*16+12-(stack_offset&15) ;%2 =16

    SUB     rsp, pad

    mova    m0, [r4+r1]   ; p1
    ;mova    m1, [r4+2*r1] ; p0
    mova    m1, [r4+r1+r1] ; p0

    mova    m2, [r0]      ; q0
    mova    m3, [r0+r1]   ; q1
    LOAD_MASK r2, r3

%if UNIX64
    mov     r3, param4
%else
    mov     r3, r4mp
%endif    
    movd    m4, [r3] ; tc0
    punpcklbw m4, m4
    punpcklbw m4, m4
    punpcklbw m4, m4 ; tc = 8x tc0[1], 8x tc0[0]
    pcmpeqb m6, m6
    pcmpgtb m3, m4, m6
    pand	 m3, m7
    pand    m4, m3

    mova    m3, [r4] ; p2
    DIFF_GT2 m1, m3, m5, m6, m7 ; |p2-p0| > beta-1
    pand    m6, m4
    MOV_ALIGN16 [rsp+16], m6 ; tc ;%2 = 16

    ;mova    m3, [r0+2*r1] ; q2
    mova    m3, [r0+r1+r1] ; q2

    DIFF_GT2 m2, m3, m5, m6, m7 ; |q2-q0| > beta-1
    pand    m6, m4
    MOV_ALIGN16 [rsp], m6 ; tc

    mova    m5, [r0+r1]   ; q1
    XAVS_DEBLOCK_P0_Q0 0
    ;mova	[r4+2*r1], m1
    mova	[r4+r1+r1], m1
    mova	[r0], m2

    mova	m5, [r0+r1]	  ; q1
    MOV_ALIGN16 m4, [rsp] ; tc
    XAVS_LUMA_Q1 0
    mova	[r0+r1], m6	  ;q1'

    mova	m5, [r4+r1]	  ; p1
    mova	m3, [r4]	  ; p2
    MOV_ALIGN16 m4, [rsp+16] ; tc ;%2 =16
    XAVS_LUMA_P1
    mova	[r4+r1], m5	 ; p1'

    ADD     rsp, pad
    RET

;-----------------------------------------------------------------------------
; void deblock_h_luma( uint8_t *pix, int stride, int alpha, int beta, int8_t *tc0 )
;-----------------------------------------------------------------------------
INIT_MMX cpuname
cglobal deblock_h_luma, 5,9
    movsxd r7, r1d
    lea    r8, [r7*3]
    lea    r6, [r0-4]
    lea    r5, [r0-4+r8]
%if WIN64
    sub   rsp, 0x98
    %define pix_tmp rsp+0x30
%elif UNIX64
    sub   rsp, 0x98
    %define pix_tmp rsp+0x30
%else
    sub   rsp, 0x68
    %define pix_tmp rsp
%endif

    ; transpose 6x16 -> tmp space
    TRANSPOSE6x8_MEM  PASS8ROWS(r6, r5, r7, r8), pix_tmp
    lea    r6, [r6+r7*8]
    lea    r5, [r5+r7*8]
    TRANSPOSE6x8_MEM  PASS8ROWS(r6, r5, r7, r8), pix_tmp+8

    ; vertical filter
    ; alpha, beta, tc0 are still in r2d, r3d, r4
    ; don't backup r6, r5, r7, r8 because deblock_v_luma_sse2 doesn't use them
    lea    r0, [pix_tmp+0x30]
    mov    r1d, 0x10
%if WIN64
    mov    [rsp+0x20], r4
%elif UNIX64
    mov    [rsp+0x20], r4
%endif
    call   deblock_v_luma

    ; transpose 16x4 -> original space  (only the middle 4 rows were changed by the filter)
    add    r6, 2
    add    r5, 2
    movq   m0, [pix_tmp+0x18]
    movq   m1, [pix_tmp+0x28]
    movq   m2, [pix_tmp+0x38]
    movq   m3, [pix_tmp+0x48]
    TRANSPOSE8x4B_STORE  PASS8ROWS(r6, r5, r7, r8)

    shl    r7, 3
    sub    r6, r7
    sub    r5, r7
    shr    r7, 3
    movq   m0, [pix_tmp+0x10]
    movq   m1, [pix_tmp+0x20]
    movq   m2, [pix_tmp+0x30]
    movq   m3, [pix_tmp+0x40]
    TRANSPOSE8x4B_STORE  PASS8ROWS(r6, r5, r7, r8)

%if WIN64
    add    rsp, 0x98
%elif UNIX64
    add    rsp, 0x98
%else
    add    rsp, 0x68
%endif
    RET
;%endmacro

;INIT_XMM sse2
;DEBLOCK_LUMA
;INIT_XMM avx
;DEBLOCK_LUMA

%else ;!ARCH_X86_64

;%macro DEBLOCK_LUMA 2
INIT_XMM sse2
;-----------------------------------------------------------------------------
; void deblock_v_luma( uint8_t *pix, int stride, int alpha, int beta, int8_t *tc0 )
;-----------------------------------------------------------------------------
cglobal deblock_v_luma, 5, 5 ;%1 =v

    lea     r4, [r1*3]
    dec     r2     ; alpha-1
    neg     r4
    dec     r3     ; beta-1
    add     r4, r0 ; pix-3*stride

    %assign pad 2*16+12-(stack_offset&15) ;%2 =16

    SUB     rsp, pad

    mova    m0, [r4+r1]   ; p1
    ;mova    m1, [r4+2*r1] ; p0
    mova    m1, [r4+r1+r1] ; p0

    mova    m2, [r0]      ; q0
    mova    m3, [r0+r1]   ; q1
    LOAD_MASK r2, r3

    mov     r3, r4mp
    movd    m4, [r3] ; tc0
    punpcklbw m4, m4
    punpcklbw m4, m4
    punpcklbw m4, m4 ; tc = 8x tc0[1], 8x tc0[0]
    pcmpeqb m6, m6
    pcmpgtb m3, m4, m6
    pand	 m3, m7
    pand    m4, m3

    mova    m3, [r4] ; p2
    DIFF_GT2 m1, m3, m5, m6, m7 ; |p2-p0| > beta-1
    pand    m6, m4
    MOV_ALIGN16 [rsp+16], m6 ; tc ;%2 = 16

    ;mova    m3, [r0+2*r1] ; q2
     mova    m3, [r0+r1+r1] ; q2
    DIFF_GT2 m2, m3, m5, m6, m7 ; |q2-q0| > beta-1
    pand    m6, m4
    MOV_ALIGN16 [rsp], m6 ; tc

    mova    m5, [r0+r1]   ; q1
    XAVS_DEBLOCK_P0_Q0 0
    ;mova	[r4+2*r1], m1
    mova	[r4+r1+r1], m1
    mova	[r0], m2

    mova	m5, [r0+r1]	  ; q1
    MOV_ALIGN16 m4, [rsp] ; tc
    XAVS_LUMA_Q1 0
    mova	[r0+r1], m6	  ;q1'

    mova	m5, [r4+r1]	  ; p1
    mova	m3, [r4]	  ; p2
    MOV_ALIGN16 m4, [rsp+16] ; tc ;%2 =16
    XAVS_LUMA_P1
    mova	[r4+r1], m5	 ; p1'

    ADD     rsp, pad
    RET

cglobal deblock_v_luma_unaligned, 5,5 ;%1 = v
    lea     r4, [r1*3]
    dec     r2     ; alpha-1
    neg     r4
    dec     r3     ; beta-1
    add     r4, r0 ; pix-3*stride
    
    %assign pad 2*16+12-(stack_offset&15) ;%2 =16

    SUB     rsp, pad

    MOV_ALIGN16 m0, [r4+r1]   ; p1
    ;MOV_ALIGN16 m1, [r4+2*r1] ; p0
    MOV_ALIGN16 m1, [r4+r1+r1] ; p0
    MOV_ALIGN16 m2, [r0]      ; q0
    MOV_ALIGN16 m3, [r0+r1]   ; q1
    LOAD_MASK r2, r3

    mov     r3, r4mp
    movd    m4, [r3] ; tc0
    punpcklbw m4, m4
    punpcklbw m4, m4
    punpcklbw m4, m4 ; tc = 8x tc0[1], 8x tc0[0]
    pcmpeqb m6, m6
    pcmpgtb m3, m4, m6
    pand	 m3, m7
    pand    m4, m3

    MOV_ALIGN16 m3, [r4] ; p2
    DIFF_GT2 m1, m3, m5, m6, m7 ; |p2-p0| > beta-1
    pand    m6, m4
    MOV_ALIGN16 [rsp+16], m6 ; tc ; %2 = 16

    ;MOV_ALIGN16 m3, [r0+2*r1] ; q2
    MOV_ALIGN16 m3, [r0+r1+r1] ; q2
    DIFF_GT2 m2, m3, m5, m6, m7 ; |q2-q0| > beta-1
    pand    m6, m4
    MOV_ALIGN16 [rsp], m6 ; tc

    MOV_ALIGN16 m5, [r0+r1]   ; q1
    XAVS_DEBLOCK_P0_Q0 1
    ;MOV_ALIGN16	[r4+2*r1], m1
    MOV_ALIGN16	[r4+r1+r1], m1
    MOV_ALIGN16	[r0], m2

    MOV_ALIGN16	m5, [r0+r1]	  ; q1
    MOV_ALIGN16 m4, [rsp] ; tc
    XAVS_LUMA_Q1 1
    MOV_ALIGN16	[r0+r1], m6	  ;q1'

    MOV_ALIGN16	m5, [r4+r1]	  ; p1
    MOV_ALIGN16	m3, [r4]	  ; p2
    MOV_ALIGN16    m4, [rsp+16] ; tc ; %2=16

    XAVS_LUMA_P1
    MOV_ALIGN16	[r4+r1], m5	 ; p1'

    ADD     rsp, pad
    RET

;-----------------------------------------------------------------------------
; void deblock_h_luma( uint8_t *pix, int stride, int alpha, int beta, int8_t *tc0 )
;-----------------------------------------------------------------------------
INIT_MMX cpuname
cglobal deblock_h_luma, 0,6
    mov    r0, r0mp
%if ARCH_X86_64==0
    mov    r3, r1m
%else
    mov    r3, r1mp
%endif    
    lea    r4, [r3*3]
    sub    r0, 4
    lea    r1, [r0+r4]
    %assign pad 0x78-(stack_offset&15)
    SUB    rsp, pad
%define pix_tmp rsp+12

    ; transpose 6x16 -> tmp space
    TRANSPOSE6x8_MEM  PASS8ROWS(r0, r1, r3, r4), pix_tmp
    lea    r0, [r0+r3*8]
    lea    r1, [r1+r3*8]
    TRANSPOSE6x8_MEM  PASS8ROWS(r0, r1, r3, r4), pix_tmp+8

    ; vertical filter
    lea    r0, [pix_tmp+0x30]
    
%if ARCH_X86_64 == 0
    PUSH   dword r4m
    PUSH   dword r3m
    PUSH   dword r2m
    PUSH   dword 16
    PUSH   dword r0
%else
    
%endif    

    call   deblock_v_luma_unaligned ; %1 = v

%if ARCH_X86_64==0
    ADD    rsp, 20
%else

    
%endif

    ; transpose 16x4 -> original space  (only the middle 4 rows were changed by the filter)
    mov    r0, r0mp
    sub    r0, 2
    lea    r1, [r0+r4]

    movq   m0, [pix_tmp+0x10]
    movq   m1, [pix_tmp+0x20]
    movq   m2, [pix_tmp+0x30]
    movq   m3, [pix_tmp+0x40]
    TRANSPOSE8x4B_STORE  PASS8ROWS(r0, r1, r3, r4)

    lea    r0, [r0+r3*8]
    lea    r1, [r1+r3*8]
    movq   m0, [pix_tmp+0x18]
    movq   m1, [pix_tmp+0x28]
    movq   m2, [pix_tmp+0x38]
    movq   m3, [pix_tmp+0x48]
    TRANSPOSE8x4B_STORE  PASS8ROWS(r0, r1, r3, r4)

    ADD    rsp, pad
    RET
;%endmacro ; DEBLOCK_LUMA


INIT_XMM sse2
;DEBLOCK_LUMA v, 16
INIT_XMM avx
;DEBLOCK_LUMA v, 16

%endif ; ARCH_X86_64

;in: m4 = (ap & k), %6: (ap & k), %7: 1 if aligned
;tmp: %3, %4, %5
;out: %1(p0'), %2(p1') in memory
%macro LUMA_INTRA_P012 7
	pxor	%3, p1, q0
	pavgb	%4, p1, q0
	pand	%3, mpb_1
	psubb	%4, %3
	pavgb	%4, p0	;p0'a = (p1 + 2*p0 + q0 + 2) >> 2
	pand	%4, %6	; %4 = p0'a & (ap & k)

	pxor	%3, p0, q0
	pavgb	%5, p0, q0
	pand	%3, mpb_1
	psubb	%5, %3
	pavgb	%5, p1		;p1' = p0'b = (2*p1 + p0 + q0 + 2) >> 2
	pand	%3, %5, %6	; %3 = p1' & (ap & k)

	pcmpeqb	%6, mpb_0
	pand	%5, %6	; %5 = p0'b & !(ap & k)
	pand	%6, p1	; %6 = p1'  &  !(ap & k)
	por		%4, %5
	por		%3, %6

	pxor	%4, p0
	pxor	%3, p1
%ifdef NON_MOD16_STACK
	movu	%5, mask0
	pand	%4, %5
	pand	%3, %5
%else
	pand	%4, mask0
	pand	%3, mask0
%endif
	pxor	%4, p0
	pxor	%3, p1
%ifidn %7, 1
	mova	%1, %4
	mova	%2, %3
%else
	MOV_ALIGN16	%1, %4
	MOV_ALIGN16	%2, %3
%endif
%endmacro

%macro DEBLOCK_INTRA_INIT 0
    %define p1 m0
    %define p0 m1
    %define q0 m2
    %define q1 m3
    %define t0 m4
    %define t1 m5
    %define t2 m6
    %define t3 m7

    %define spill(x) [rsp+16*x+((stack_offset+4)&15)]
    %define p2 [r4+r1]
    ;%define q2 [r0+2*r1]
    %define q2 [r0+r1+r1]
    %define t4 spill(0)
    %define t5 spill(1)
    %define mask0 spill(2)
    %define mask1p spill(3)
    %define mask1q spill(4)
    %define mpb_0 [pb_0]
    %define mpb_1 [pb_1]

%endmacro

%macro LUMA_INTRA_SWAP_PQ 0
    %define q0 m1
    %define p0 m2
%endmacro

%macro DEBLOCK_LUMA_INTRA 1
	
DEBLOCK_INTRA_INIT

;-----------------------------------------------------------------------------
; void deblock_v_luma_intra( uint8_t *pix, int stride, int alpha, int beta )
;-----------------------------------------------------------------------------
cglobal deblock_%1_luma_intra, 4, 7
    
    SUB     rsp, 0x60

    lea     r4, [r1*4]
    lea     r5, [r1*3] ; 3*stride
    dec     r2d        ; alpha-1
    jl .end
    neg     r4
    dec     r3d        ; beta-1
    jl .end
    add     r4, r0     ; pix-4*stride
    ;mova    p1, [r4+2*r1]	;m0
    mova    p1, [r4+r1+r1]	;m0
    mova    p0, [r4+r5]		;m1
    mova    q0, [r0]		;m2
    mova    q1, [r0+r1]		;m3
    ;movu   m0, p2
    ;mova   t4, m0

    ;movu  m0, q2
    ;mova  t5, m0
    
    ;movu    p1, [r4+2*r1]	;m0
    movu    p1, [r4+r1+r1]	;m0
    movu    p0, [r4+r5]		;m1
    movu    q0, [r0]		       ;m2
    movu    q1, [r0+r1]		;m3

    LOAD_MASK r2d, r3d, t5 ; m5=beta-1, t5=alpha-1, m7=mask0
    MOV_ALIGN16	m4, t5

    MOV_ALIGN16 mask0, m7
    pavgb   m4, [pb_0]
    pavgb   m4, [pb_1] ; alpha/4+1
    DIFF_GT2 p0, q0, m4, m6, m7 ; m6 = |p0-q0| > alpha/4+1    
    DIFF_GT2 p0, p2, m5, m4, m7 ; m4 = |p2-p0| > beta-1

    pand    m4, m6
    DIFF_GT2 q0, q2, m5, q1, m7 ; m4 = |q2-q0| > beta-1
    pand    q1, m6
    
    ;LUMA_INTRA_P012 [r4+r5], [r4+2*r1], m5, m6, m7, m4, 0;align
    LUMA_INTRA_P012 [r4+r5], [r4+r1+r1], m5, m6, m7, m4, 0;align
     
    LUMA_INTRA_SWAP_PQ
    mova	p1, [r0+r1]		;actually it's q1
    ;movu p1, [r0+r1]
    LUMA_INTRA_P012 [r0], [r0+r1], m5, m6, m7, q1, 0;align
    
.end:

    ADD rsp ,0x60
    
    RET

DEBLOCK_INTRA_INIT

;an unalign version of xavs_deblock_v_luma_intra_xxx, for
;invoking in xavs_deblock_h_luma_intra_xxx
cglobal deblock_%1_luma_intra_unaligned, 4,6,16

    SUB     rsp, 0x60

    lea     r4, [r1*4]
    lea     r5, [r1*3] ; 3*stride
    dec     r2d        ; alpha-1
    jl .end
    neg     r4
    dec     r3d        ; beta-1
    jl .end
    add     r4, r0     ; pix-4*stride
    
    ;MOV_ALIGN16	p1, [r4+2*r1]	;m0
    MOV_ALIGN16	p1, [r4+r1+r1]	;m0
    MOV_ALIGN16	p0, [r4+r5]		;m1
    MOV_ALIGN16	q0, [r0]		;m2
    MOV_ALIGN16	q1, [r0+r1]		;m3

    LOAD_MASK r2d, r3d, t5 ; m5=beta-1, t5=alpha-1, m7=mask0
    MOV_ALIGN16	m4, t5

    MOV_ALIGN16 mask0, m7
    pavgb   m4, [pb_0]
    pavgb   m4, [pb_1] ; alpha/4+1
    DIFF_GT2 p0, q0, m4, m6, m7 ; m6 = |p0-q0| > alpha/4+1
    
    MOV_ALIGN16	q1, p2
    DIFF_GT2 p0, q1, m5, m4, m7 ; m4 = |p2-p0| > beta-1
    pand    m4, m6
    DIFF_GT2_UNALIGNED q0, q2, m5, q1, m7 ; m4 = |q2-q0| > beta-1
    pand    q1, m6
   
    ;LUMA_INTRA_P012 [r4+r5], [r4+2*r1], m5, m6, m7, m4, 0
    LUMA_INTRA_P012 [r4+r5], [r4+r1+r1], m5, m6, m7, m4, 0
    
    LUMA_INTRA_SWAP_PQ
    MOV_ALIGN16	p1, [r0+r1]		;actually it's q1
    LUMA_INTRA_P012 [r0], [r0+r1], m5, m6, m7, q1, 0

.end:

    ADD     rsp, 0x60

    RET

INIT_MMX cpuname
%if ARCH_X86_64==1
;-----------------------------------------------------------------------------
; void deblock_h_luma_intra( uint8_t *pix, int stride, int alpha, int beta )
;-----------------------------------------------------------------------------
cglobal deblock_h_luma_intra, 4,9
    movsxd r7, r1d
    lea    r8, [r7*3]
    lea    r6, [r0-4]
    lea    r5, [r0-4+r8]
    sub   rsp, 0x88
    %define pix_tmp rsp

    ; transpose 8x16 -> tmp space
    TRANSPOSE8x8_MEM  PASS8ROWS(r6, r5, r7, r8), PASS8ROWS(pix_tmp, pix_tmp+0x30, 0x10, 0x30)
    lea    r6, [r6+r7*8]
    lea    r5, [r5+r7*8]
    TRANSPOSE8x8_MEM  PASS8ROWS(r6, r5, r7, r8), PASS8ROWS(pix_tmp+8, pix_tmp+0x38, 0x10, 0x30)

    lea    r0,  [pix_tmp+0x40]
    mov    r1,  0x10
    call   deblock_v_luma_intra

    ; transpose 16x6 -> original space (but we can't write only 6 pixels, so really 16x8)
    lea    r5, [r6+r8]
    TRANSPOSE8x8_MEM  PASS8ROWS(pix_tmp+8, pix_tmp+0x38, 0x10, 0x30), PASS8ROWS(r6, r5, r7, r8)
    shl    r7, 3
    sub    r6, r7
    sub    r5, r7
    shr    r7, 3
    TRANSPOSE8x8_MEM  PASS8ROWS(pix_tmp, pix_tmp+0x30, 0x10, 0x30), PASS8ROWS(r6, r5, r7, r8)
    add   rsp, 0x88
    RET
%else ;!ARCH_X86_64
cglobal deblock_h_luma_intra, 2,4
    lea    r3,  [r1*3]
    sub    r0,  4
    lea    r2,  [r0+r3]
%assign pad 0x8c-(stack_offset&15)
    SUB    rsp, pad
    %define pix_tmp rsp

    ; transpose 8x16 -> tmp space
    TRANSPOSE8x8_MEM  PASS8ROWS(r0, r2, r1, r3), PASS8ROWS(pix_tmp, pix_tmp+0x30, 0x10, 0x30)
    lea    r0,  [r0+r1*8]
    lea    r2,  [r2+r1*8]
    TRANSPOSE8x8_MEM  PASS8ROWS(r0, r2, r1, r3), PASS8ROWS(pix_tmp+8, pix_tmp+0x38, 0x10, 0x30)

    lea    r0,  [pix_tmp+0x40]
    PUSH   dword r3m
    PUSH   dword r2m
    PUSH   dword 16
    PUSH   r0
    call   deblock_%1_luma_intra_unaligned
%ifidn %1, v8
    add    dword [rsp], 8 ; pix_tmp+8
    call   deblock_%1_luma_intra
%endif
    ADD    rsp, 16

    mov    r1,  r1m
    mov    r0,  r0mp
    lea    r3,  [r1*3]
    sub    r0,  4
    lea    r2,  [r0+r3]
    ; transpose 16x6 -> original space (but we can't write only 6 pixels, so really 16x8)
    TRANSPOSE8x8_MEM  PASS8ROWS(pix_tmp, pix_tmp+0x30, 0x10, 0x30), PASS8ROWS(r0, r2, r1, r3)
    lea    r0,  [r0+r1*8]
    lea    r2,  [r2+r1*8]
    TRANSPOSE8x8_MEM  PASS8ROWS(pix_tmp+8, pix_tmp+0x38, 0x10, 0x30), PASS8ROWS(r0, r2, r1, r3)
    ADD    rsp, pad
    RET
%endif ; ARCH_X86_64
%endmacro ; DEBLOCK_LUMA_INTRA

INIT_XMM sse2
DEBLOCK_LUMA_INTRA v
INIT_XMM avx
DEBLOCK_LUMA_INTRA v
%endif ; !HIGH_BIT_DEPTH
