;*****************************************************************************
;* idct.asm: cavs decoder library
;*****************************************************************************

%include "bavs_x86inc.asm"
%include "bavs_x86util.asm"

SECTION_RODATA

const pd_4,             times 4 dd 4
const pd_64,            times 4 dd 64
const pw_10,            times 8 dw 10


SECTION .text

; ----------------------------------------------------------------------------
; transpose 8x8 block (16bit data)
%macro TRANSPOSE 9-11

; in:  m0..m7, unless %11 in which case m6 is in %9
; out: m0..m7, unless %11 in which case m4 is in %10
; spills into %9 and %10
;   IN: m%1: 1817161514131211                OUT: m%1: 1121314151617181
;       m%2: 2827262524232221                     m%2: 1222324252627282
;       m%3: 3837363534333231                     m%3: 1323334353637383
;       m%4: 4847464544434241        ---\         m%4: 1424344454647484 (%11)
;       m%5: 5857565554535251        ---/         m%5: 1525354555657585
;       m%6: 6867666564636261 (%9)                m%6: 1626364656667686
;       m%7: 7877767574737271                     m%7: 1727374757677787
;       m%8: 8887868584838281                     m%8: 1828384858687888
%if %0 < 11
    movdqa      %9,  m%7        ; %9  <-- m%7
%endif
    SBUTTERFLY  wd,  %1, %2, %7 ; m%1: 1424132312221121 m%2: 1828172716261525
    movdqa      %10, m%2        ; %10 <-- m%2
    movdqa      m%7, %9         ; restore m%7
    SBUTTERFLY  wd,  %3, %4, %2 ; m%3: 3444334332423141 m%4: 3848374736463545
    SBUTTERFLY  wd,  %5, %6, %2 ; m%5: 5464536352625161 m%6: 5868576756665565
    SBUTTERFLY  wd,  %7, %8, %2 ; m%7: 7484738372827181 m%8: 7888778776867585
    SBUTTERFLY  dq,  %1, %3, %2 ; m%1: 1222324211213141 m%3: 1424344413233343
    movdqa      %9,  m%3        ; %9 <-- m%3
    movdqa      m%2, %10        ; restore m%2
    SBUTTERFLY  dq,  %2, %4, %3 ; m%2: 1626364615253545  m%4: 1828384817273747
    SBUTTERFLY  dq,  %5, %7, %3 ; m%5: 5262728251617181  m%7: 5464748453637383
    SBUTTERFLY  dq,  %6, %8, %3 ; m%6: 5666768655657585  m%8: 5868788857677787
    SBUTTERFLY  qdq, %1, %5, %3 ; m%1: 1121314151617181  m%5: 1222324252627282
    SBUTTERFLY  qdq, %2, %6, %3 ; m%2: 1525354555657585  m%6: 1626364656667686
    movdqa      %10, m%2        ; %10 <-- m%2
    movdqa      m%3, %9         ; restore m%3
    SBUTTERFLY  qdq, %3, %7, %2 ; m%3: 1323334353637383  m%7: 1424344454647484
    SBUTTERFLY  qdq, %4, %8, %2 ; m%4: 1727374757677787  m%8: 1828384858687888
    SWAP        %2,  %5         ; m%2 <--> m%5
    SWAP        %4,  %7         ; m%4 <--> m%7
%if %0 < 11
    movdqa      m%5, %10        ; restore m%5
%endif

%endmacro




; ----------------------------------------------------------------------------
; IN:  %1: dct row 0
;      %2: dct row 1
;      %3: temp reg
;      %4: temp reg (zero)
;      %5: dst row 0
;      %6: dst row 1
; OUT: %2, the result
%macro X_DIFFx2 6-7
    movh        %3, %5                ; %3 <-- load 8 pixel from %5 (8 bytes)
    punpcklbw   %3, %4                ; %3 (8 words)
    paddsw      %1, %3                ; %1 <-- (dct row 0) + (dst row 0)
    movh        %3, %6                ; %3 <-- load 8 pixel from %6 (8 bytes)
    punpcklbw   %3, %4                ; %3 (8 words)
    paddsw      %2, %3                ; %2 <-- (dct row 1) + (dst row 1)
    packuswb    %2, %1                ; %2 <-- result (pack dst row 0,1)
%endmacro


; ----------------------------------------------------------------------------
; store: regs --> mem
%macro SPILL_SHUFFLE 3-*        ; ptr, list of regs, list of memory offsets
    %xdefine    %%base %1
    %rep        %0/2
    %xdefine    %%tmp m%2
    %rotate     %0/2
    mova        [%%base + %2*16], %%tmp     ; regs --> mem
    %rotate     1-%0/2
    %endrep
%endmacro


; ----------------------------------------------------------------------------
; store: regs --> mem
%macro SPILL 2+                 ; assume offsets are the same as reg numbers
    SPILL_SHUFFLE %1, %2, %2
%endmacro


; ----------------------------------------------------------------------------
; load: mem --> regs
%macro UNSPILL_SHUFFLE 3-*      ; ptr, list of regs, list of memory offsets
    %xdefine    %%base %1
    %rep        %0/2
    %xdefine    %%tmp m%2
    %rotate     %0/2
    mova        %%tmp, [%%base + %2*16]     ; mem --> regs
    %rotate     1-%0/2
    %endrep
%endmacro


; ----------------------------------------------------------------------------
; load: mem --> regs
%macro UNSPILL 2+               ; assume offsets are the same as reg numbers
    UNSPILL_SHUFFLE %1, %2, %2
%endmacro


; ----------------------------------------------------------------------------
; in : dct row 0,2,4,6 in mem, row 1,3,5,7 in regs
;      %1 ~ %8: regs
;           %9: point to dct
;          %10: bias
;          %11: r_shift
; out: all result are in regs
%macro IDCT8_1D  11
    ; --- dct data in regs -----------------------------------------
    ; m%2 <-- a4 = SRC(1)
    ; m%4 <-- a5 = SRC(3)
    ; m%6 <-- a6 = SRC(5)
    ; m%8 <-- a7 = SRC(7)

    ; --- downleft butterfly ---------------------------------------
    mova        m%3, m%4              ; copy,  m%3 = m%4 = a5
    mova        m%5, m%4              ; copy,  m%5 = m%4 = a5
    mova        m%7, m%2              ; copy,  m%7 = m%2 = a4
    mova        m%1, m%2              ; copy,  m%1 = m%2 = a4
    paddsw      m%4, m%6              ;        a5 + a6
    psubsw      m%3, m%6              ;        a5 - a6
    paddsw      m%2, m%8              ;        a4 + a7
    psubsw      m%7, m%8              ;        a4 - a7
    psllw       m%4, 1                ;       (a5 + a6) << 1
    psllw       m%3, 1                ;       (a5 - a6) << 1
    psllw       m%2, 1                ;       (a4 + a7) << 1
    psllw       m%7, 1                ;       (a4 - a7) << 1
    paddsw      m%4, m%5              ; b1 = ((a5 + a6) << 1) + a5
    psubsw      m%3, m%6              ; b2 = ((a5 - a6) << 1) - a6
    paddsw      m%2, m%8              ; b3 = ((a4 + a7) << 1) + a7
    paddsw      m%7, m%1              ; b0 = ((a4 - a7) << 1) + a4
    mova        m%5, m%7              ; copy,  m%5 = m%7 = b0
    mova        m%6, m%7              ; copy,  m%6 = m%7 = b0
    mova        m%1, m%7              ; copy,  m%1 = m%7 = b0
    mova        m%8, m%2              ; copy,  m%8 = m%2 = b3
    paddsw      m%7, m%4              ;        b0 + b1
    psubsw      m%5, m%4              ;        b0 - b1
    psubsw      m%2, m%4              ;        b3 - b1
    psubsw      m%6, m%3              ;        b0 - b2
    paddsw      m%7, m%8              ;        b0 + b1 + b3
    paddsw      m%5, m%3              ;        b0 - b1 + b2
    psubsw      m%2, m%3              ;        b3 - b1 - b2
    psubsw      m%6, m%8              ;        b0 - b2 - b3
    psllw       m%7, 1                ;       (b0 + b1 + b3) << 1
    psllw       m%5, 1                ;       (b0 - b1 + b2) << 1
    psllw       m%2, 1                ;       (b3 - b1 - b2) << 1
    psllw       m%6, 1                ;       (b0 - b2 - b3) << 1
    paddsw      m%7, m%4              ; b4 = ((b0 + b1 + b3) << 1) + b1
    paddsw      m%5, m%1              ; b5 = ((b0 - b1 + b2) << 1) + b0
    paddsw      m%2, m%8              ; b6 = ((b3 - b1 - b2) << 1) + b3
    psubsw      m%6, m%3              ; b7 = ((b0 - b2 - b3) << 1) - b2

    ; extern b4/b5/b6/b7 from 16bit to 32bit and store in stack
    pxor        m%4, m%4              ; clear
    pxor        m%1, m%1              ; clear
    mova        m%8, m%7              ; copy b4
    mova        m%3, m%5              ; copy b5
    pcmpgtw     m%4, m%7              ; if m7[i]<0, then m4[i]=0xFFFF
    pcmpgtw     m%1, m%5              ; if m5[i]<0, then m1[i]=0xFFFF
    punpckhwd   m%8, m%4              ; b4                              (H)
    punpcklwd   m%7, m%4              ; b4                              (L)
    punpckhwd   m%5, m%1              ; b5                              (H)
    punpcklwd   m%3, m%1              ; b5                              (L)
    mova        [rsp + 0*16], m%8     ; save b4                         (H)
    mova        [rsp + 1*16], m%7     ; save b4                         (L)
    mova        [rsp + 2*16], m%5     ; save b5                         (H)
    mova        [rsp + 3*16], m%3     ; save b5                         (L)

    pxor        m%4, m%4              ; clear
    pxor        m%1, m%1              ; clear
    mova        m%8, m%2              ; copy b6
    mova        m%3, m%6              ; copy b7
    pcmpgtw     m%4, m%2              ; if m7[i]<0, then m4[i]=0xFFFF
    pcmpgtw     m%1, m%6              ; if m5[i]<0, then m1[i]=0xFFFF
    punpckhwd   m%8, m%4              ; b6                              (H)
    punpcklwd   m%2, m%4              ; b6                              (L)
    punpckhwd   m%6, m%1              ; b7                              (H)
    punpcklwd   m%3, m%1              ; b7                              (L)
    mova        [rsp + 4*16], m%8     ; save b6                         (H)
    mova        [rsp + 5*16], m%2     ; save b6                         (L)
    mova        [rsp + 6*16], m%6     ; save b7                         (H)
    mova        [rsp + 7*16], m%3     ; save b7                         (L)

    ; --- upleft butterfly -----------------------------------------
    ; load a2/a3 from memory
    mova        m%3, [%9 + 2*16]      ; m%3 <-- a2 = SRC(2)
    mova        m%4, [%9 + 6*16]      ; m%4 <-- a3 = SRC(6)

    ; cal t2/t3
    mova        m%7, m%3              ; copy,  m%7 = m%3 = a2
    mova        m%8, m%4              ; copy,  m%8 = m%4 = a3
    pmullw      m%3, [pw_10]          ;        a2 * 10
    pmullw      m%4, [pw_10]          ;        a3 * 10
    psllw       m%7, 2                ;        a2 << 2
    psllw       m%8, 2                ;        a3 << 2

    pxor        m%1, m%1              ; clear
    pxor        m%2, m%2              ; clear
    mova        m%5, m%3              ; copy,  m%5 = m%3 = a2 * 10
    mova        m%6, m%8              ; copy,  m%6 = m%8 = a3 << 2
    pcmpgtw     m%1, m%3              ; if m3[i]<0, then m1[i]=0xFFFF
    pcmpgtw     m%2, m%8              ; if m8[i]<0, then m2[i]=0xFFFF
    punpckhwd   m%3, m%1              ;        a2 * 10                  (H)
    punpcklwd   m%5, m%1              ;        a2 * 10                  (L)
    punpckhwd   m%8, m%2              ;        a3 << 2                  (H)
    punpcklwd   m%6, m%2              ;        a3 << 2                  (L)
    paddd       m%3, m%8              ; t2 = (a2 * 10) + (a3 << 2)      (H)
    paddd       m%5, m%6              ; t2 = (a2 * 10) + (a3 << 2)      (L)

    pxor        m%1, m%1              ; clear
    pxor        m%2, m%2              ; clear
    mova        m%8, m%4              ; copy,  m%8 = m%4 = a3 * 10
    mova        m%6, m%7              ; copy,  m%6 = m%7 = a2 << 2
    pcmpgtw     m%1, m%4              ; if m4[i]<0, then m1[i]=0xFFFF
    pcmpgtw     m%2, m%7              ; if m7[i]<0, then m2[i]=0xFFFF
    punpckhwd   m%4, m%1              ;        a3 * 10                  (H)
    punpcklwd   m%8, m%1              ;        a3 * 10                  (L)
    punpckhwd   m%6, m%2              ;        a2 << 2                  (H)
    punpcklwd   m%7, m%2              ;        a2 << 2                  (L)
    psubd       m%6, m%4              ; t3 = (a2 << 2) - (a3 * 10)      (H)
    psubd       m%7, m%8              ; t3 = (a2 << 2) - (a3 * 10)      (L)

    ; store t2/t3 to release the registers
    mova        [%9 + 1*16], m%3      ; store t2                        (H)
    mova        [%9 + 3*16], m%5      ; store t2                        (L)
    mova        [%9 + 5*16], m%6      ; store t3                        (H)
    mova        [%9 + 7*16], m%7      ; store t3                        (L)

    ; load a0/a1 from memory
    mova        m%1, [%9 + 0*16]      ; m%1 <-- a0 = SRC(0)
    mova        m%3, [%9 + 4*16]      ; m%3 <-- a1 = SRC(4)

    ; cal t0/t1
    pxor        m%8, m%8              ; clear
    pxor        m%7, m%7              ; clear
    mova        m%2, m%1              ; copy,  m%2 = m%1 = a0
    paddsw      m%1, m%3              ;        a0 + a1
    psubsw      m%2, m%3              ;        a0 - a1

    pcmpgtw     m%8, m%1              ; if m1[i]<0, then m8[i]=0xFFFF
    pcmpgtw     m%7, m%2              ; if m2[i]<0, then m7[i]=0xFFFF
    mova        m%5, m%1              ; copy,  m%5 = m%1 = a0 + a1
    mova        m%6, m%2              ; copy,  m%6 = m%2 = a0 - a1
    punpckhwd   m%1, m%8              ;        a0 + a1                  (H)
    punpcklwd   m%5, m%8              ;        a0 + a1                  (L)
    punpckhwd   m%2, m%7              ;        a0 - a1                  (H)
    punpcklwd   m%6, m%7              ;        a0 - a1                  (L)
    pslld       m%1, 3                ;       (a0 + a1) << 3            (H)
    pslld       m%5, 3                ;       (a0 + a1) << 3            (L)
    pslld       m%2, 3                ;       (a0 - a1) << 3            (H)
    pslld       m%6, 3                ;       (a0 - a1) << 3            (H)
    paddd       m%1, [%10]            ; t0 = ((a0 + a1) << 3) + (bias)  (H)
    paddd       m%5, [%10]            ; t0 = ((a0 + a1) << 3) + (bias)  (L)
    paddd       m%2, [%10]            ; t1 = ((a0 - a1) << 3) + (bias)  (H)
    paddd       m%6, [%10]            ; t1 = ((a0 - a1) << 3) + (bias)  (L)

    ; cal b0/b1/b2/b3
    mova        m%3, m%1              ; copy,  m%3 = m%1 = t0           (H)
    mova        m%4, m%5              ; copy,  m%4 = m%5 = t0           (L)
    mova        m%7, m%2              ; copy,  m%7 = m%2 = t1           (H)
    mova        m%8, m%6              ; copy,  m%8 = m%6 = t1           (L)
    paddd       m%1, [%9 + 1*16]      ; b0 = t0 + t2                    (H)
    paddd       m%5, [%9 + 3*16]      ; b0 = t0 + t2                    (L)
    paddd       m%2, [%9 + 5*16]      ; b1 = t1 + t3                    (H)
    paddd       m%6, [%9 + 7*16]      ; b1 = t1 + t3                    (L)
    psubd       m%7, [%9 + 5*16]      ; b2 = t1 - t3                    (H)
    psubd       m%8, [%9 + 7*16]      ; b2 = t1 - t3                    (L)
    psubd       m%3, [%9 + 1*16]      ; b3 = t0 - t2                    (H)
    psubd       m%4, [%9 + 3*16]      ; b3 = t0 - t2                    (L)

    ; --- last butterfly -------------------------------------------
    mova        [%9 + 0*16], m%1      ; store b0                        (H)
    mova        [%9 + 1*16], m%5      ; store b0                        (L)
    mova        [%9 + 2*16], m%2      ; store b1                        (H)
    mova        [%9 + 3*16], m%6      ; store b1                        (L)
    mova        [%9 + 4*16], m%7      ; store b2                        (H)
    mova        [%9 + 5*16], m%8      ; store b2                        (L)
    mova        [%9 + 6*16], m%3      ; store b3                        (H)
    mova        [%9 + 7*16], m%4      ; store b3                        (L)

    ; cal a0/a1/a2/a3
    paddd       m%1, [rsp + 0*16]     ; a0 = b0 + b4                    (H)
    paddd       m%5, [rsp + 1*16]     ; a0 = b0 + b4                    (L)
    paddd       m%2, [rsp + 2*16]     ; a1 = b1 + b5                    (H)
    paddd       m%6, [rsp + 3*16]     ; a1 = b1 + b5                    (L)
    paddd       m%7, [rsp + 4*16]     ; a2 = b2 + b6                    (H)
    paddd       m%8, [rsp + 5*16]     ; a2 = b2 + b6                    (L)
    paddd       m%3, [rsp + 6*16]     ; a3 = b3 + b7                    (H)
    paddd       m%4, [rsp + 7*16]     ; a3 = b3 + b7                    (L)

    ; pack a0/a1/a2/a3 from 32bit to 16bit and store in stack
    packssdw    m%5, m%1              ; a0
    packssdw    m%6, m%2              ; a1
    packssdw    m%8, m%7              ; a2
    packssdw    m%4, m%3              ; a3

    psraw       m%5, %11              ; a0 >> r_shift
    psraw       m%6, %11              ; a1 >> r_shift
    psraw       m%8, %11              ; a2 >> r_shift
    psraw       m%4, %11              ; a3 >> r_shift

    mova        [rsp +  9*16], m%5    ; store a0
    mova        [rsp + 10*16], m%6    ; store a1
    mova        [rsp + 11*16], m%8    ; store a2
    mova        [rsp + 12*16], m%4    ; store a3

    ; cal a4/a5/a6/a7
    mova        m%1, [%9 + 6*16]      ; load b3                         (H)
    mova        m%5, [%9 + 7*16]      ; load b3                         (L)
    mova        m%2, [%9 + 4*16]      ; load b2                         (H)
    mova        m%6, [%9 + 5*16]      ; load b2                         (L)
    mova        m%3, [%9 + 2*16]      ; load b1                         (H)
    mova        m%7, [%9 + 3*16]      ; load b1                         (L)
    mova        m%4, [%9 + 0*16]      ; load b0                         (H)
    mova        m%8, [%9 + 1*16]      ; load b0                         (L)

    psubd       m%1, [rsp + 6*16]     ; a4 = b3 - b7                    (H)
    psubd       m%5, [rsp + 7*16]     ; a4 = b3 - b7                    (L)
    psubd       m%2, [rsp + 4*16]     ; a5 = b2 - b6                    (H)
    psubd       m%6, [rsp + 5*16]     ; a5 = b2 - b6                    (L)
    psubd       m%3, [rsp + 2*16]     ; a6 = b1 - b5                    (H)
    psubd       m%7, [rsp + 3*16]     ; a6 = b1 - b5                    (L)
    psubd       m%4, [rsp + 0*16]     ; a7 = b0 - b4                    (H)
    psubd       m%8, [rsp + 1*16]     ; a7 = b0 - b4                    (L)

    ; pack a4/a5/a6/a7 from 32bit to 16bit
    packssdw    m%5, m%1              ; a4
    packssdw    m%6, m%2              ; a5
    packssdw    m%7, m%3              ; a6
    packssdw    m%8, m%4              ; a7

    psraw       m%5, %11              ; a4 >> r_shift
    psraw       m%6, %11              ; a5 >> r_shift
    psraw       m%7, %11              ; a6 >> r_shift
    psraw       m%8, %11              ; a7 >> r_shift

    mova        m%1, [rsp +  9*16]    ; load a0
    mova        m%2, [rsp + 10*16]    ; load a1
    mova        m%3, [rsp + 11*16]    ; load a2
    mova        m%4, [rsp + 12*16]    ; load a3

    ; --------------------------------------------------------------
    ; NOW: a0/a1.../a7 are in reg m%1/m%2.../m%8
    ; --------------------------------------------------------------
%endmacro


INIT_XMM
;-----------------------------------------------------------------------------
; void add8x8_idct8(pixel *dst, int16_t* block,int stride )
;-----------------------------------------------------------------------------

cglobal add8x8_idct8_sse2, 3,7
    
%assign stack_size  ((8+8)*8)*2
    mov             r3,  rsp                                ; save rsp
    and             rsp, ~(mmsize-1)                        ; align 16
    sub             rsp, stack_size                         ; make stack space
                                                            ;
    UNSPILL     r1, 0,1,2,3,4,5,6,7                         ; load  data from mem to reg
    TRANSPOSE       0,1,2,3,4,5,6,7,[r1+0x60],[r1+0x40]     ;
    SPILL       r1, 0,2,4,6                                 ; store data from reg to mem
    IDCT8_1D        0,1,2,3,4,5,6,7,r1,pd_4,3               ;
    TRANSPOSE       0,1,2,3,4,5,6,7,[r1+0x60],[r1+0x40]     ;
    SPILL       r1, 0,2,4,6                                 ; store data from reg to mem
    IDCT8_1D        0,1,2,3,4,5,6,7,r1,pd_64,7              ;

    SPILL       r1, 6,7
    pxor        m7, m7

    X_DIFFx2          m0, m1, m6, m7, [r0], [r0+r2]
    mov r4, r2  ;r2
    add r4, r2	;2*r2
    mov r5, r4
    add r5, r2	;3*r2
    X_DIFFx2          m2, m3, m6, m7, [r0+r4], [r0+r5]

    UNSPILL_SHUFFLE r1, 0,2, 6,7

    add r4, r4  ;4*r2
    mov r5, r4
    add r5, r2 	;5*r2
    X_DIFFx2          m4, m5, m6, m7, [r0+r4], [r0+r5]

    add r4,r2
    add r4,r2 	;6*r2
    mov r5, r4
    add r5, r2 	;7*r2
    X_DIFFx2          m0, m2, m6, m7, [r0+r4], [r0+r5]

    movhps      [r0], m1
    movh        [r0+r2], m1

    ;2*r2
    mov r4, r2
    add r4, r2
    ;3*r2
    mov r5, r4
    add r5, r2
    movhps      [r0+r4], m3
    movh        [r0+r5], m3

    ;4*r2
    add r4, r4
    ;5*r2
    mov r5, r4
    add r5, r2
    movhps      [r0+r4], m5
    movh        [r0+r5], m5

    ;6*r2
    add r4,r2
    add r4,r2
    ;7*r2
    mov r5, r4
    add r5, r2
    movhps      [r0+r4], m2
    movh        [r0+r5], m2

    ; restore the stack and quit
    mov             rsp, r3                                 ; restore rsp
    RET



