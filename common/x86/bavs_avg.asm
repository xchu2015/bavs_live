;*****************************************************************************
;* avg.asm: cavs decoder library
;*****************************************************************************

%include "bavs_x86inc.asm"
%include "bavs_x86util.asm"

SECTION .text

;-------------------------------------------------------------------------------------
; avg
;-------------------------------------------------------------------------------------

; cavs_avg_pixels8_mmxext(uint8_t *block, const uint8_t *pixels, int line_size, int h)
%macro AVG_PIXELS8 0
cglobal avg_pixels8_mmxext, 4,5
    ;lea          r4, [r2*2]
    lea          r4, [r2+r2]
.loop:
    mova         m0, [r0]
    mova         m1, [r0+r2]
    PAVGB        m0, [r1]
    PAVGB        m1, [r1+r2]
    mova       [r0], m0
    mova    [r0+r2], m1
    add          r1, r4
    add          r0, r4
    mova         m0, [r0]
    mova         m1, [r0+r2]
    PAVGB        m0, [r1]
    PAVGB        m1, [r1+r2]
    add          r1, r4
    mova       [r0], m0
    mova    [r0+r2], m1
    add          r0, r4
    sub         r3d, 4
    jne .loop
    REP_RET
%endmacro

INIT_MMX
AVG_PIXELS8


INIT_XMM
; void cavs_avg_pixels16_sse2(uint8_t *block, const uint8_t *pixels, int line_size, int h)
cglobal avg_pixels16_sse2, 4,5
    lea          r4, [r2*3]
.loop:
    movu         m0, [r1]
    movu         m1, [r1+r2]
    ;movu         m2, [r1+r2*2]
    movu         m2, [r1+r2+r2]
    
    movu         m3, [r1+r4]
    lea          r1, [r1+r2*4]
    pavgb        m0, [r0]
    pavgb        m1, [r0+r2]
    ;pavgb        m2, [r0+r2*2]
    pavgb        m2, [r0+r2+r2]
    
    pavgb        m3, [r0+r4]
    mova       [r0], m0
    mova    [r0+r2], m1
    ;mova  [r0+r2*2], m2
    mova  [r0+r2+r2], m2
    
    mova    [r0+r4], m3
	lea          r0, [r0+r2*4]
    sub         r3d, 4
    jne       .loop
    REP_RET


;---------------------------------------------------------------------------------------------
; put
;---------------------------------------------------------------------------------------------
INIT_MMX

; cavs_put_pixels8_mmxext(uint8_t *block, const uint8_t *pixels, int line_size, int h)
cglobal put_pixels8_mmxext, 4,5
    ;lea          r4, [r2*2]; 2*line_size
    lea          r4, [r2+r2]; 2*line_size
.loop:
    mova         m0, [r1]    ;pixels
    mova         m1, [r1+r2] ;pixels+line_size
    mova         [r0],  m0
    mova         [r0+r2], m1
    add          r1, r4
    add          r0, r4

    mova         m0, [r1]
    mova         m1, [r1+r2]
    add          r1, r4
    mova         [r0], m0
    mova         [r0+r2], m1
    add          r0, r4
    sub          r3d, 4
    jne .loop
    REP_RET


; cavs_put_pixels16_mmxext(uint8_t *block, const uint8_t *pixels, int line_size, int h)
cglobal put_pixels16_mmxext, 4,5
    ;lea          r4, [r2*2] ; 2*line_size
     lea          r4, [r2+r2] ; 2*line_size

.loop:
    mova         m0, [r1]
    mova         m1, [r1+r2]
	mova         m2, [r1+8]
	mova         m3, [r1+r2+8]

	mova         [r0], m0
	mova         [r0+r2], m1
	mova         [r0+8], m2
	mova         [r0+r2+8], m3

	add          r0, r4
	add          r1, r4

	mova         m0, [r1]
    mova         m1, [r1+r2]
	mova         m2, [r1+8]
	mova         m3, [r1+r2+8]

	mova         [r0], m0
	mova         [r0+r2], m1
	mova         [r0+8], m2
	mova         [r0+r2+8], m3

    lea          r1, [r1+r4]
    sub          r3d, 4
    lea          r0, [r0+r4]
    jnz       .loop
    REP_RET
 
 ; These functions are not general-use; not only do the SSE ones require aligned input,
; but they also will fail if given a non-mod16 size.
; memzero SSE will fail for non-mod128.

;-----------------------------------------------------------------------------
; void *memcpy_aligned( void *dst, const void *src, size_t n );
;-----------------------------------------------------------------------------
%macro MEMCPY 1
cglobal memcpy_aligned, 3,3
%if %1 == 16
    test r2d, 16
    jz .copy2
    mova  m0, [r1+r2-16]
    mova [r0+r2-16], m0
    sub  r2d, 16
.copy2:
%endif
    test r2d, 2*%1
    jz .copy4start
    mova  m0, [r1+r2-1*%1]
    mova  m1, [r1+r2-2*%1]
    mova [r0+r2-1*%1], m0
    mova [r0+r2-2*%1], m1
    sub  r2d, 2*%1
.copy4start:
    test r2d, r2d
    jz .ret
.copy4:
    mova  m0, [r1+r2-1*%1]
    mova  m1, [r1+r2-2*%1]
    mova  m2, [r1+r2-3*%1]
    mova  m3, [r1+r2-4*%1]
    mova [r0+r2-1*%1], m0
    mova [r0+r2-2*%1], m1
    mova [r0+r2-3*%1], m2
    mova [r0+r2-4*%1], m3
    sub  r2d, 4*%1
    jg .copy4
.ret:
    REP_RET
%endmacro

INIT_MMX mmx
MEMCPY 8
INIT_XMM sse
MEMCPY 16