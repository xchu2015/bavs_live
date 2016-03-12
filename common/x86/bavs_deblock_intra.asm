;*****************************************************************************
;* bavs_deblock_intra.asm: x86 deblocking
;*****************************************************************************

%include "bavs_x86inc.asm"
%include "bavs_x86util.asm"

SECTION_RODATA 
pb_01: times  16 db 0x01

SECTION .text
;cglobal cavs_deblock_v_chroma_intra_mmxext
;cglobal cavs_deblock_h_chroma_intra_mmxext

%macro LOAD_MASK_MMX 2
    movd     mm4, %1
    movd     mm5, %2
    pshufw   mm4, mm4, 0
    pshufw   mm5, mm5, 0
    packuswb mm4, mm4  ; 8x alpha-1
    packuswb mm5, mm5  ; 8x beta-1
%endmacro
%macro CHROMA_INTRA_P0 4  
     movq    %4, %1 ;p0
     pxor    %4, %3 ; q0^p0
     pand    %4, [pb_01]  ; %4 = (p0^q0)&1 
     pavgb   %1, %3 ; (p0+q0+1)>>1
     psubusb %1, %4
     pavgb   %1, %2       ; dst = avg(p1, avg(p0,q0) - ((p0^q0)&1))

%endmacro
%macro DIFF_GT_MMX  5
    movq    %5, %2
    movq    %4, %1
    psubusb %5, %1
    psubusb %4, %2
    por     %4, %5
    psubusb %4, %3
%endmacro

%macro DIFF_GT_MMX1 5              ; |p0-q0| > ((alpha>>2)+1)
    movq      %5, %2
    movq      %4, %1
    psubusb   %5, %1
    psubusb   %4, %2
    por       %4, %5
    pxor      %5, %5 
    punpcklbw %3, %5
    psrlw     %3, 2 ;alpha>>2

%if WIN64==1|| UNIX64==1
    mov       r2d, 1
    pinsrw    %5,r2d,0
%else
    mov       dx, 1
    pinsrw    %5,r2,0 ; 1
%endif 

    pshufw    %5, %5,0 ;4x 1
    paddw     %3, %5 ; 4x ((alpha>>2) +1)
    packuswb  %3, %3;8x((alpha>>2) +1) 
    psubusb   %4, %3 
%endmacro

%define PASS8ROWS(base, base3, stride, stride3)  [base], [base+stride], [base+stride*2], [base3],[base3+stride], [base3+stride*2], [base3+stride3], [base3+stride*4]

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
 
%macro SBUTTERFLY_Q 4
    movq       %4, %2
    punpckl%1  %2, %3
    punpckh%1  %4, %3
%endmacro

; in: 8 rows of 8 (only the middle 6 pels are used) in %1..%8
; out: 6 rows of 8 in [%9+0*16] .. [%9+5*16]
%macro TRANSPOSE6x8_MEM 9
    movq  mm0, %1
    movq  mm1, %3
    movq  mm2, %5 
    movq  mm3, %7
    SBUTTERFLY_Q bw, mm0, %2, mm4
    SBUTTERFLY_Q bw, mm1, %4, mm5
    SBUTTERFLY_Q bw, mm2, %6, mm6
    movq  [%9+0x10], mm5
    SBUTTERFLY_Q bw, mm3, %8, mm7
    SBUTTERFLY_Q wd, mm0, mm1, mm5  
    SBUTTERFLY_Q wd, mm2, mm3, mm1
    punpckhdq mm0, mm2
    movq  [%9+0x00], mm0
    SBUTTERFLY_Q wd, mm4, [%9+0x10], mm3
    SBUTTERFLY_Q wd, mm6, mm7, mm2
    SBUTTERFLY_Q dq, mm4, mm6, mm0
    SBUTTERFLY_Q dq, mm5, mm1, mm7
    punpckldq mm3, mm2
    movq  [%9+0x10], mm5
    movq  [%9+0x20], mm7
    movq  [%9+0x30], mm4
    movq  [%9+0x40], mm0 
    movq  [%9+0x50], mm3
%endmacro

%if ARCH_X86_64==0
INIT_MMX
%else
INIT_XMM
%endif
;-----------------------------------------------------------------------------
;void cavs_deblock_v_chroma_intra_mmxext( uint8_t *pix, int stride, int alpha, int beta )  
;-----------------------------------------------------------------------------
cglobal deblock_v_chroma_intra_mmxext, 4, 7
    dec   r2 ; alpha-1
    dec   r3 ; beta-1
    mov  r4, r0 ; pix
    sub   r4, r1 ; pix-stride
    sub   r4, r1 ; pix-2*stride
    sub   r4, r1 ; pix-3*stride
    
    movq  mm0, [r4+r1]                          ; p1
    ;movq  mm1, [r4+2*r1]                       ; p0
    movq  mm1, [r4+r1+r1]                       ; p0
    movq  mm2, [r0]                               ; q0
    movq  mm3, [r0+r1]                          ; q1

%if WIN64==1|| UNIX64==1    
    LOAD_MASK_MMX r2d, r3d
%else
    LOAD_MASK_MMX r2, r3 ; 8x alpha-1 mm4    8x beta-1 mm5
%endif

    DIFF_GT_MMX  mm1, mm2, mm4, mm7, mm6           ; |p0-q0| >alpha-1
    DIFF_GT_MMX  mm0, mm1, mm5, mm2, mm6           ; |p1-p0| > beta-1
    por     mm7, mm2 ; (|p0-q0| >alpha-1) || (|p1-p0| > beta-1)
    movq    mm2, [r0] ; q0
    DIFF_GT_MMX  mm3, mm2, mm5, mm1, mm6           ;|q1-q0| > beta-1
    por     mm7, mm1 ; (|p0-q0| >alpha-1) || (|p1-p0| > beta-1) ||(|q1-q0| > beta-1)
    pxor    mm6, mm6
    pcmpeqb mm7, mm6 ; if equal 0, set 0xff;or, set 0x00.

%if ARCH_X86_64==0    
    movq    [rsp-16],mm7 ; save outer condition
%else
    movq2dq xmm2, mm7
%endif

    ;movq   mm1, [r4+2*r1] ; p0
    movq   mm1, [r4+r1+r1] ; p0

    add     r2, 1; alpha

%if WIN64==1|| UNIX64==1
    LOAD_MASK_MMX r2d, r3d
%else
    LOAD_MASK_MMX r2, r3 ; 8x alpha-> mm4    8x beta-1 -> mm5
%endif

    DIFF_GT_MMX1  mm1, mm2, mm4, mm0, mm6          ;|p0-q0| >((alpha>>2)+1)

%if ARCH_X86_64==0
    movq    [rsp-8],mm0 ; save inner condition for next use
%else
    movq2dq xmm1, mm0
%endif
    
    DIFF_GT_MMX  [r4], mm1, mm5, mm4, mm6         ;|p2-p0| >beta-1
    por     mm4,mm0 ; (|p0-q0| >((alpha>>2)+1)) || (|p2-p0| >beta-1)
    pxor    mm6,mm6
    pcmpeqb mm4,mm6 
    pand    mm4,mm7 ; full true
    movq    mm0, [r4+r1] ; p1
    
    CHROMA_INTRA_P0  mm1, mm0, mm2 ,mm6             ;p0 0 = (2*p1+p0+q0+2)>>2
    ;CHROMA_INTRA_P0  mm0,[r4+2*r1], mm2 ,mm6      ;p0 1 =(2*p0+p1+q0+2)>>2
    CHROMA_INTRA_P0  mm0,[r4+r1+r1], mm2 ,mm6      ;p0 1 =(2*p0+p1+q0+2)>>2

    pand   mm0, mm4 ; p0 1
    pandn  mm4, mm1 ; p0 0
    pand   mm4, mm7 ; full outer condition
    ;pandn  mm7,[r4+2*r1] ;p0, not full outer condition
    pandn  mm7,[r4+r1+r1] ;p0, not full outer condition
    
    por    mm4, mm7 ; original point merge modified point
    movq   mm1, mm4
    por    mm1, mm0 ; merge if and else switch , output p0'
    
    ;DIFF_GT_MMX  [r0+2*r1], mm2, mm5, mm4, mm6   ;|q2-q0| >beta-1
    DIFF_GT_MMX  [r0+r1+r1], mm2, mm5, mm4, mm6   ;|q2-q0| >beta-1

%if ARCH_X86_64==0 
    por     mm4,[rsp-8] ;inner
%else
    movdq2q mm6, xmm1
    por    mm4, mm6
%endif

    pxor    mm6,mm6
    pcmpeqb mm4,mm6
    
%if ARCH_X86_64==0
    pand    mm4,[rsp-16] ; outer
%else
    movdq2q mm7, xmm2
    pand    mm4, mm7
%endif

    ;CHROMA_INTRA_P0  mm3, mm2,[r4+2*r1],mm6       ;q0 1
    ;CHROMA_INTRA_P0  mm2,[r0+r1],[r4+2*r1],mm6  ;q0 0
    CHROMA_INTRA_P0  mm3, mm2,[r4+r1+r1],mm6       ;q0 1
    CHROMA_INTRA_P0  mm2,[r0+r1],[r4+r1+r1],mm6  ;q0 0
    
%if ARCH_X86_64==0        
    movq   mm7,[rsp-16] ;outer
%endif

    pand   mm3, mm4
    pandn  mm4, mm2
    pand   mm4, mm7 
    pandn  mm7,[r0]
    
    por    mm4, mm7
    movq   mm2, mm4
    por    mm2, mm3
 
    ;movq  [r4+2*r1], mm1 ;p0'
    movq  [r4+r1+r1], mm1 ;p0'
    movq  [r0], mm2;q0'
    
    RET
    

;-----------------------------------------------------------------------------
;void cavs_deblock_h_chroma_intra_mmxext( uint8_t *pix, int stride, int alpha, int beta )
;-----------------------------------------------------------------------------

cglobal deblock_h_chroma_intra_mmxext, 4, 7
    mov  r6, r0 ; backup pix
    sub   r0, 4 ; pix-4
    dec   r2 ; alpha-1 
    dec   r3 ;beta-1
    mov  r4, r1; stride
    add   r4, r1 ; 2*stride
    add   r4, r1 ; 3*stride
    mov  r5, r0 ; pix - 4
    add  r0, r4 ;pix-4+3*stride
    
    sub   rsp, 96
 %define pix_tmp rsp    
   
    TRANSPOSE6x8_MEM  PASS8ROWS(r5, r0, r1, r4),pix_tmp

    movq  mm0,[pix_tmp+0x10] ;p1
    movq  mm1,[pix_tmp+0x20] ;p0 
    movq  mm2,[pix_tmp+0x30] ;q0
    movq  mm3,[pix_tmp+0x40] ;q1
    
%if WIN64==1|| UNIX64==1
    LOAD_MASK_MMX r2d, r3d
%else     
    LOAD_MASK_MMX r2, r3
%endif    

    DIFF_GT_MMX  mm1, mm2, mm4, mm7, mm6           ;|p0-q0| >alpha-1
    DIFF_GT_MMX  mm0, mm1, mm5, mm2, mm6           ;|p1-p0| > beta-1
    por     mm7, mm2
    movq    mm2, [pix_tmp+0x30]
    DIFF_GT_MMX  mm3, mm2, mm5, mm1, mm6           ;|q1-q0| > beta-1
    por     mm7, mm1
    pxor    mm6, mm6
    pcmpeqb mm7, mm6

%if ARCH_X86_64==0    
    movq    [rsp-8],mm7
%else
    movq2dq xmm0, mm7
%endif

    movq    mm1, [pix_tmp+0x20]
    add     r2, 1

%if WIN64==1|| UNIX64==1
    LOAD_MASK_MMX r2d, r3d
%else
    LOAD_MASK_MMX r2, r3
%endif
    
    DIFF_GT_MMX1  mm1, mm2, mm4, mm0, mm6          ;|p0-q0| >(alpha>>2+1)

%if ARCH_X86_64==0
    movq    [rsp-16],mm0
%else
    movq2dq xmm1, mm0
%endif

    DIFF_GT_MMX  [pix_tmp+0x00], mm1, mm5, mm4, mm6         ;|p2-p0| >beta-1
    por     mm4,mm0
    pxor    mm6,mm6
    pcmpeqb mm4,mm6 
    pand    mm4,mm7
    movq    mm0,[pix_tmp+0x10]
    CHROMA_INTRA_P0  mm1, mm0, mm2 ,mm6                ;p0 0
    CHROMA_INTRA_P0  mm0,[pix_tmp+0x20], mm2 ,mm6      ;p0 1
    pand   mm0, mm4 
    pandn  mm4, mm1
    pand   mm4, mm7
    pandn  mm7,[pix_tmp+0x20] 
    por    mm4, mm7
    movq   mm1, mm4
    por    mm1, mm0
    DIFF_GT_MMX [pix_tmp+0x50], mm2, mm5, mm4, mm6   ;|q2-q0| >beta-1

%if ARCH_X86_64==0
    por     mm4,[rsp-16]
%else
    movdq2q mm6, xmm1
    por     mm4, mm6
%endif

    pxor    mm6,mm6
    pcmpeqb mm4,mm6 

%if ARCH_X86_64==0
    pand    mm4,[rsp-8]
%else
    movdq2q mm7, xmm0
    pand      mm4, mm7
%endif

    CHROMA_INTRA_P0  mm3, mm2,[pix_tmp+0x20],mm6            ;q0 1
    CHROMA_INTRA_P0  mm2,[pix_tmp+0x40],[pix_tmp+0x20],mm6  ;q0 0

%if ARCH_X86_64==0
    movq   mm7,[rsp-8]
%endif

    pand   mm3, mm4
    pandn  mm4, mm2
    pand   mm4, mm7
    pandn  mm7,[pix_tmp+0x30]
    por    mm4, mm7
    movq   mm2, mm4
    por    mm2, mm3
    
    movq  mm0,[pix_tmp+0x10]
    movq  mm3,[pix_tmp+0x40]
    
    add   rsp, 96
    mov   r0, r6; pix
    sub   r0, 2; pix-2, only two pixel will used each edge side    
    mov   r4, r1;stride
    add   r4, r1 ;2*stride
    add   r4, r1 ;3*stride
    mov   r5, r0 ;pix-2
    add   r0, r4 ;pix-2 + 3*stride
    TRANSPOSE8x4_STORE  PASS8ROWS(r5, r0, r1, r4)
    
    RET
