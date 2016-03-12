;*****************************************************************************
;* bavs_deblock_inter.asm: x86 deblocking
;*****************************************************************************

%include "bavs_x86inc.asm"

SECTION_RODATA
dw_4:  times 8 dw 4


SECTION .text
;cglobal cavs_deblock_v_chroma_sse2
;cglobal cavs_deblock_h_chroma_sse2

; out: %4 = |%1-%2|>%3
; clobbers: %5
%macro DIFF_GT_SSE 5
    movaps    %5, %2
    movaps    %4, %1
    psubusw   %5, %1
    psubusw   %4, %2
    por       %4, %5
    psubusw   %4, %3
%endmacro

; expands to [base],...,[base+7*stride]
%define PASS8ROWS(base, base3, stride, stride3) \
    [base], [base+stride], [base+stride*2], [base3], \
    [base3+stride], [base3+stride*2], [base3+stride3], [base3+stride*4]

; in: 8 rows of 4 bytes in %1..%8
; out: 4 rows of 8 bytes in mm0..mm3
%macro TRANSPOSE4x8_LOAD 8
    movd       xmm0, %1
    movd       xmm2, %2
    movd       xmm1, %3
    movd       xmm3, %4
    punpcklbw  xmm0, xmm2
    punpcklbw  xmm1, xmm3
    punpcklwd  xmm0, xmm1

    movd       xmm4, %5
    movd       xmm6, %6
    movd       xmm5, %7
    movd       xmm7, %8
    punpcklbw  xmm4, xmm6
    punpcklbw  xmm5, xmm7
    punpcklwd  xmm4, xmm5
   

    movaps     xmm2, xmm0 
    punpckldq  xmm0, xmm4
    punpckhdq  xmm2, xmm4
    movhlps    xmm1, xmm0
    movhlps    xmm3, xmm2 

%endmacro

; in: 4 rows of 8 bytes in mm0..mm3
; out: 8 rows of 4 bytes in %1..%8
%macro TRANSPOSE8x4_STORE 8
    movq       mm4, mm0
    movq       mm5, mm1
    movq       mm6, mm2
    punpckhdq  mm4, mm4
    punpckhdq  mm5, mm5
    punpckhdq  mm6, mm6

    punpcklbw  mm0, mm1
    punpcklbw  mm2, mm3
    movq       mm1, mm0
    punpcklwd  mm0, mm2
    punpckhwd  mm1, mm2
    movd       %1,  mm0
    punpckhdq  mm0, mm0
    movd       %2,  mm0
    movd       %3,  mm1
    punpckhdq  mm1, mm1
    movd       %4,  mm1

    punpckhdq  mm3, mm3
    punpcklbw  mm4, mm5
    punpcklbw  mm6, mm3
    movq       mm5, mm4
    punpcklwd  mm4, mm6
    punpckhwd  mm5, mm6
    movd       %5,  mm4
    punpckhdq  mm4, mm4
    movd       %6,  mm4
    movd       %7,  mm5
    punpckhdq  mm5, mm5
    movd       %8,  mm5
%endmacro

INIT_XMM
;-----------------------------------------------------------------------------
;   void cavs_deblock_v_chroma_sse2( uint8_t *pix, int stride, int alpha, int beta, int8_t *tc0 )
;-----------------------------------------------------------------------------
cglobal deblock_v_chroma_sse2, 5, 7 

    dec r2 ;alpha - 1
    dec r3 ;beta - 1

    mov r5, r0; backup pix
    sub r0, r1 ; pix - stride
    sub r0, r1 ; pix - 2*stride    
    
    pxor   xmm4, xmm4
    movq  xmm0, [r0]    ; pix[-2*stride]----p1
    movq  xmm1, [r0+r1] ; pix[-stride]------p0
    movq  xmm2, [r5]    ; pix[0]------------q0
    movq  xmm3, [r5+r1] ; pix[stride]-------q1

    punpcklbw  xmm0, xmm4 ; low 8 byte to word
    punpcklbw  xmm1, xmm4 
    punpcklbw  xmm2, xmm4
    punpcklbw  xmm3, xmm4

%if WIN64==1|| UNIX64==1    
    movd     xmm4, r2d
    movd     xmm5, r3d
%else
    movd     xmm4, r2 ;alpha-1
    movd     xmm5, r3 ;beta-1
%endif		

    pshuflw  xmm4, xmm4, 0 ; 4x alpha-1
    pshuflw  xmm5, xmm5, 0 ; 4x beta-1
    movlhps  xmm4, xmm4  ; 8x alpha-1
    movlhps  xmm5, xmm5  ; 8x beta-1

    DIFF_GT_SSE  xmm1, xmm2, xmm4, xmm7, xmm6 ; |p0-q0| > alpha-1
    DIFF_GT_SSE  xmm0, xmm1, xmm5, xmm4, xmm6 ; |p1-p0| > beta-1
    por      xmm7, xmm4
    DIFF_GT_SSE  xmm3, xmm2, xmm5, xmm4, xmm6 ; |q1-q0| > beta-1
    por      xmm7, xmm4
    pxor     xmm6, xmm6
    pcmpeqw  xmm7, xmm6 ;outer condition
    
    movd       xmm6, [r4] ; tc0[1] tc0[0]
    
    punpcklbw  xmm6, xmm6 ; tc0[1] tc0[1] tc0[0] tc0[0]
    punpcklbw  xmm6, xmm6 ; tc0[1] tc0[1] tc0[1] tc0[1] tc0[0] tc0[0] tc0[0] tc0[0]
    pxor       xmm5, xmm5
    movaps     xmm4, xmm6 
    pcmpgtb    xmm4, xmm5 ; tc > 0
    punpcklbw  xmm4, xmm4 ; byte to word
    punpcklbw  xmm6, xmm5 ; 0x00 tc0[0] 0x00 tc0[0] 0x00 tc0[0] 0x00 tc0[0]
    pand       xmm6, xmm4 ; save non-zero tc     
    pand       xmm6, xmm7 ; into loop
     
    ; in: xmm0=p1 xmm1=p0 xmm2=q0 xmm3=q1
    movaps     xmm4, xmm2 ; q0
    psubw      xmm4, xmm1 ; q0-p0
    movdqa     xmm5, xmm4
    paddw      xmm4, xmm5 ;2*(q0-p0)
    paddw      xmm4, xmm5 ; 3*(q0-p0)
    
    psubw      xmm0, xmm3 ; p1-q1
    paddw      xmm0, [dw_4] ;(p1-q1)+4
    paddw      xmm4,xmm0 ; 3*(q0-p0)+(p1-q1)+4
      
    psraw      xmm4, 3 ; (3*(q0-p0)+(p1-q1)+4)>>3
    movaps     xmm5, xmm4 ;backup
    pxor       xmm7, xmm7
    pcmpgtw    xmm7, xmm4 ;packed signed word compare
    pxor       xmm4, xmm7 ; remove sign
    psubw      xmm4, xmm7 ;output absolute value
   
    pcmpgtw    xmm4, xmm6 ; ((3*(q0-p0)+(p1-q1)+4)>>3) > tc
    
    pxor       xmm6, xmm7
    psubw      xmm6, xmm7 ; |tc |
    
    pand       xmm6, xmm4 ; ((3*(q0-p0)+(p1-q1)+4)>>3)  beyond tc, limit to tc
    pandn      xmm4, xmm5 ;((3*(q0-p0)+(p1-q1)+4)>>3) not beyond tc
    por        xmm4, xmm6 ; delta
    paddw      xmm1, xmm4 ;p0' = p0 + delta
    psubw      xmm2, xmm4 ;q0' = q0 -delta
    
    packuswb   xmm1,xmm2
    movlps     [r0+r1],xmm1
    movhps     [r5],xmm1
    
    RET


;-----------------------------------------------------------------------------
;   void cavs_deblock_h_chroma_sse2( uint8_t *pix, int stride, int alpha, int beta, int8_t *tc0 )
;-----------------------------------------------------------------------------
cglobal deblock_h_chroma_sse2, 5, 7 
   	
    dec r2 ; alpha -1
    dec r3 ; beta - 1

    sub r0, 2; pix - 2

    mov r5, r1; stride
    add r5, r1; 2*stride
    add r5, r1; 3*stride
    
    mov r6, r0; pix - 2
    add r0, r5; pix -2 + 3*stride

%if ARCH_X86_64==0    
    sub   rsp, 16
%endif

    TRANSPOSE4x8_LOAD  PASS8ROWS(r6, r0, r1, r5) 

%if ARCH_X86_64==0
    movq  [rsp+8], xmm0
    movq  [rsp+0], xmm3
%else
    movdqa xmm8, xmm0
    movdqa xmm9, xmm3
%endif

    pxor       xmm4, xmm4
    punpcklbw  xmm0, xmm4
    punpcklbw  xmm1, xmm4
    punpcklbw  xmm2, xmm4
    punpcklbw  xmm3, xmm4

%if WIN64==1 || UNIX64==1
    movd      xmm4, r2d
    movd      xmm5, r3d
%else
    movd      xmm4, r2
    movd      xmm5, r3
%endif
    
    pshuflw  xmm4, xmm4, 0
    pshuflw  xmm5, xmm5, 0
    movlhps  xmm4, xmm4  ; 8x alpha-1
    movlhps  xmm5, xmm5  ; 8x beta-1

    DIFF_GT_SSE  xmm1, xmm2, xmm4, xmm7, xmm6 ; |p0-q0| > alpha-1
    DIFF_GT_SSE  xmm0, xmm1, xmm5, xmm4, xmm6 ; |p1-p0| > beta-1
    por      xmm7, xmm4
    DIFF_GT_SSE  xmm3, xmm2, xmm5, xmm4, xmm6 ; |q1-q0| > beta-1
    por      xmm7, xmm4
    pxor     xmm6, xmm6
    pcmpeqw  xmm7, xmm6

    movd         xmm6, [r4]
    
    punpcklbw  xmm6, xmm6
    punpcklbw  xmm6, xmm6
    pxor       xmm5, xmm5
    movaps     xmm4, xmm6 
    pcmpgtb    xmm4, xmm5
    punpcklbw  xmm4, xmm4
    punpcklbw  xmm6, xmm5
    pand       xmm6, xmm4      
    pand       xmm6, xmm7
    
     
    ; in: xmm0=p1 xmm1=p0 xmm2=q0 xmm3=q1
    movaps     xmm4, xmm2
    psubw      xmm4, xmm1
    movdqa     xmm5, xmm4
    paddw      xmm4, xmm5
    paddw      xmm4, xmm5
    psubw      xmm0, xmm3
    paddw      xmm0, [dw_4]
    paddw      xmm4,xmm0
    psraw      xmm4, 3
    movaps     xmm5, xmm4
    pxor       xmm7, xmm7
    pcmpgtw    xmm7, xmm4
    pxor       xmm4, xmm7
    psubw      xmm4, xmm7
    pcmpgtw    xmm4, xmm6
    pxor       xmm6, xmm7
    psubw      xmm6, xmm7
    pand       xmm6, xmm4
    pandn      xmm4, xmm5
    por        xmm4, xmm6
    paddw      xmm1, xmm4
    psubw      xmm2, xmm4
    packuswb   xmm1,xmm2
   
    movdq2q    mm1, xmm1
    psrldq     xmm1,8
    movdq2q    mm2,xmm1

%if ARCH_X86_64==0    
    movq  mm0, [rsp+8]
    movq  mm3, [rsp+0]
%else
    movdq2q  mm0, xmm8
    movdq2q  mm3, xmm9
%endif

    TRANSPOSE8x4_STORE  PASS8ROWS(r6, r0, r1, r5) 

%if ARCH_X86_64==0 
    add  rsp, 16
%endif

    RET