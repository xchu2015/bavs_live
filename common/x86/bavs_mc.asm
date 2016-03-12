;*****************************************************************************
;* bavs_mc.asm: cavs decoder library
;*****************************************************************************


%include "bavs_x86inc.asm"
%include "bavs_x86util.asm"

SECTION_RODATA 32

ff_pw_5:  times 8 dw  5
ff_pw_4:  times 8 dw  4
ff_pw_64: times 8 dw 64
ff_pw_96: times 8 dw 96
ff_pw_42: times 8 dw 42
ff_pw_7: times 8 dw 7
ff_pw_32:times 8 dw 32
ff_pw_21: times 8 dw 21
ff_pw_48:times 8 dw 48

pww_1_56: times 4 dw 1, 56
pww_56_1: times 4 dw 56, 1
pww_7_8:  times 4 dw 7, 8
pww_8_7:  times 4 dw 8, 7
pd_512:	times 4 dd 512
pd_64:	times 4 dd 64

SECTION .text

INIT_MMX
;-----------------------------------------------------------------------------------
; cavs_put_qpel8_h_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride);
;-----------------------------------------------------------------------------------
cglobal put_qpel8_h_mmxext, 4, 7
	pxor mm7, mm7
	movq mm6, [ff_pw_5]
	mov  r4, 8

.loop:
	movq mm0, [r1] ; src
	movq mm2, [r1+1] ; src+1
	movq mm1, mm0
	movq mm3, mm2

	punpcklbw mm0, mm7
	punpckhbw mm1, mm7
	punpcklbw mm2, mm7
	punpckhbw mm3, mm7

	paddw mm0, mm2
	paddw mm1, mm3
	pmullw mm0, mm6;[ff_pw_5]
	pmullw mm1, mm6

	movq mm2, [r1-1]
	movq mm4, [r1+2]
	movq mm3, mm2
	movq mm5, mm4

	punpcklbw mm2, mm7
	punpckhbw mm3, mm7
	punpcklbw mm4, mm7
	punpckhbw mm5, mm7

	paddw mm2, mm4
	paddw mm5, mm3
	psubw mm0, mm2
	psubw mm1, mm5

	movq mm5, [ff_pw_4]
	paddw mm0, mm5
	paddw mm1, mm5
	psraw mm0, 3
	psraw mm1, 3
	packuswb mm0,mm1

	;put
	movq [r0], mm0 

	add r1, r3
	add r0, r2

	dec r4d
	jnz .loop

	RET

;-----------------------------------------------------------------------------------
; cavs_avg_qpel8_h_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride);
;-----------------------------------------------------------------------------------
cglobal avg_qpel8_h_mmxext, 4, 7
	pxor mm7, mm7
	movq mm6, [ff_pw_5]
	mov  r4, 8

.loop:
	movq mm0, [r1] ; src
	movq mm2, [r1+1] ; src+1
	movq mm1, mm0
	movq mm3, mm2

	punpcklbw mm0, mm7
	punpckhbw mm1, mm7
	punpcklbw mm2, mm7
	punpckhbw mm3, mm7

	paddw mm0, mm2
	paddw mm1, mm3
	pmullw mm0, mm6;[ff_pw_5]
	pmullw mm1, mm6

	movq mm2, [r1-1]
	movq mm4, [r1+2]
	movq mm3, mm2
	movq mm5, mm4

	punpcklbw mm2, mm7
	punpckhbw mm3, mm7
	punpcklbw mm4, mm7
	punpckhbw mm5, mm7

	paddw mm2, mm4
	paddw mm5, mm3
	psubw mm0, mm2
	psubw mm1, mm5

	movq mm5, [ff_pw_4]
	paddw mm0, mm5
	paddw mm1, mm5
	psraw mm0, 3
	psraw mm1, 3
	packuswb mm0,mm1

	;avg
	movq mm5, [r0] 
	pavgb mm0, mm5
	movq  [r0], mm0

	add r1, r3
	add r0, r2

	dec r4d
	jnz .loop

	RET


;put
%macro PUT_OP 4 
 mov%4 [%2], %1
%endmacro

;avg
%macro AVG_MMXEXT_OP 4
mov%4   %3, [%2]
pavgb   %1, %3
mov%4   [%2], %1
%endmacro

;-------------------------------------------------------------------------------
; v1
;-------------------------------------------------------------------------------
%macro QPEL_CAVSV1 7
        movd %6, [r1]
        movq mm6, %3
        pmullw mm6, [ff_pw_96]
        movq mm7, %4
        pmullw mm7, [ff_pw_42]
        psllw %5, 3             
        psubw mm6, %5          
        psraw %5, 3             
        paddw mm6, mm7         
        paddw mm6, %5          
        paddw %2, %2           
        pxor mm7, mm7          
        add r1, r3                 
        punpcklbw %6, mm7      
        psubw mm6, %2          
        psraw %2, 1             
        psubw mm6, %1          
        paddw mm6, [ff_pw_64]            
        psraw mm6, 7
			            
        packuswb mm6, mm6      
        %7 mm6, r0, %1, d       
        add r0, r2                  
%endmacro

;-----------------------------------------------------------------------------------
;cavs_put_qpel8or16_v1_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride);
;-----------------------------------------------------------------------------------
cglobal put_qpel8or16_v1_mmxext, 4, 7
        pxor mm7, mm7
        movd mm0, [r1]
        add  r1, r3
        movd mm1, [r1]
        add  r1, r3
        movd mm2, [r1]
        add  r1, r3
        movd mm3, [r1]
        add r1, r3
        movd mm4, [r1]
        add r1, r3
        punpcklbw mm0, mm7     
        punpcklbw mm1, mm7    
        punpcklbw mm2, mm7    
        punpcklbw mm3, mm7    
        punpcklbw mm4, mm7    
        QPEL_CAVSV1 mm0, mm1, mm2, mm3, mm4, mm5, PUT_OP
        QPEL_CAVSV1 mm1, mm2, mm3, mm4, mm5, mm0, PUT_OP
        QPEL_CAVSV1 mm2, mm3, mm4, mm5, mm0, mm1, PUT_OP
        QPEL_CAVSV1 mm3, mm4, mm5, mm0, mm1, mm2, PUT_OP
        QPEL_CAVSV1 mm4, mm5, mm0, mm1, mm2, mm3, PUT_OP
        QPEL_CAVSV1 mm5, mm0, mm1, mm2, mm3, mm4, PUT_OP
        QPEL_CAVSV1 mm0, mm1, mm2, mm3, mm4, mm5, PUT_OP
        QPEL_CAVSV1 mm1, mm2, mm3, mm4, mm5, mm0, PUT_OP
        
		REP_RET

;-----------------------------------------------------------------------------------
;cavs_put_qpel8or16_v1_h16_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride);
;-----------------------------------------------------------------------------------
cglobal put_qpel8or16_v1_h16_mmxext, 4, 7
        QPEL_CAVSV1 mm2, mm3, mm4, mm5, mm0, mm1, PUT_OP
        QPEL_CAVSV1 mm3, mm4, mm5, mm0, mm1, mm2, PUT_OP
        QPEL_CAVSV1 mm4, mm5, mm0, mm1, mm2, mm3, PUT_OP
        QPEL_CAVSV1 mm5, mm0, mm1, mm2, mm3, mm4, PUT_OP
        QPEL_CAVSV1 mm0, mm1, mm2, mm3, mm4, mm5, PUT_OP
        QPEL_CAVSV1 mm1, mm2, mm3, mm4, mm5, mm0, PUT_OP
        QPEL_CAVSV1 mm2, mm3, mm4, mm5, mm0, mm1, PUT_OP
        QPEL_CAVSV1 mm3, mm4, mm5, mm0, mm1, mm2, PUT_OP

		REP_RET
		
;-----------------------------------------------------------------------------------
;cavs_avg_qpel8or16_v1_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride);
;-----------------------------------------------------------------------------------
cglobal avg_qpel8or16_v1_mmxext, 4, 7
        pxor mm7, mm7
        movd mm0, [r1]
        add  r1, r3
        movd mm1, [r1]
        add  r1, r3
        movd mm2, [r1]
        add  r1, r3
        movd mm3, [r1]
        add r1, r3
        movd mm4, [r1]
        add r1, r3
        punpcklbw mm0, mm7     
        punpcklbw mm1, mm7    
        punpcklbw mm2, mm7    
        punpcklbw mm3, mm7    
        punpcklbw mm4, mm7    
        QPEL_CAVSV1 mm0, mm1, mm2, mm3, mm4, mm5, AVG_MMXEXT_OP
        QPEL_CAVSV1 mm1, mm2, mm3, mm4, mm5, mm0, AVG_MMXEXT_OP
        QPEL_CAVSV1 mm2, mm3, mm4, mm5, mm0, mm1, AVG_MMXEXT_OP
        QPEL_CAVSV1 mm3, mm4, mm5, mm0, mm1, mm2, AVG_MMXEXT_OP
        QPEL_CAVSV1 mm4, mm5, mm0, mm1, mm2, mm3, AVG_MMXEXT_OP
        QPEL_CAVSV1 mm5, mm0, mm1, mm2, mm3, mm4, AVG_MMXEXT_OP
        QPEL_CAVSV1 mm0, mm1, mm2, mm3, mm4, mm5, AVG_MMXEXT_OP
        QPEL_CAVSV1 mm1, mm2, mm3, mm4, mm5, mm0, AVG_MMXEXT_OP
        
		REP_RET
		
;-----------------------------------------------------------------------------------
;cavs_avg_qpel8or16_v1_h16_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride);
;-----------------------------------------------------------------------------------
cglobal avg_qpel8or16_v1_h16_mmxext, 4, 7
        QPEL_CAVSV1 mm2, mm3, mm4, mm5, mm0, mm1, AVG_MMXEXT_OP
        QPEL_CAVSV1 mm3, mm4, mm5, mm0, mm1, mm2, AVG_MMXEXT_OP
        QPEL_CAVSV1 mm4, mm5, mm0, mm1, mm2, mm3, AVG_MMXEXT_OP
        QPEL_CAVSV1 mm5, mm0, mm1, mm2, mm3, mm4, AVG_MMXEXT_OP
        QPEL_CAVSV1 mm0, mm1, mm2, mm3, mm4, mm5, AVG_MMXEXT_OP
        QPEL_CAVSV1 mm1, mm2, mm3, mm4, mm5, mm0, AVG_MMXEXT_OP
        QPEL_CAVSV1 mm2, mm3, mm4, mm5, mm0, mm1, AVG_MMXEXT_OP
        QPEL_CAVSV1 mm3, mm4, mm5, mm0, mm1, mm2, AVG_MMXEXT_OP

		REP_RET					

;-------------------------------------------------------------------------------
; v2
;-------------------------------------------------------------------------------

%macro QPEL_CAVSV2 7
        movd %6, [r1]            
        movq mm6, %3          
        paddw mm6, %4         
        pmullw mm6, [ff_pw_5]          
        add r1, r3                
        punpcklbw %6, mm7     
        psubw mm6, %2         
        psubw mm6, %5         
        paddw mm6, [ff_pw_4]           
        psraw mm6, 3           
        packuswb mm6, mm6     
        %7 mm6, r0, %1, d      
        add r0, r2                
%endmacro

;-----------------------------------------------------------------------------------
;cavs_put_qpel8or16_v2_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride);
;-----------------------------------------------------------------------------------
cglobal put_qpel8or16_v2_mmxext, 4, 7
        pxor mm7, mm7
        movd mm0, [r1]
        add  r1, r3
        movd mm1, [r1]
        add  r1, r3
        movd mm2, [r1]
        add  r1, r3
        movd mm3, [r1]
        add r1, r3
        movd mm4, [r1]
        add r1, r3
        punpcklbw mm0, mm7     
        punpcklbw mm1, mm7    
        punpcklbw mm2, mm7    
        punpcklbw mm3, mm7    
        punpcklbw mm4, mm7    
        QPEL_CAVSV2 mm0, mm1, mm2, mm3, mm4, mm5, PUT_OP
        QPEL_CAVSV2 mm1, mm2, mm3, mm4, mm5, mm0, PUT_OP
        QPEL_CAVSV2 mm2, mm3, mm4, mm5, mm0, mm1, PUT_OP
        QPEL_CAVSV2 mm3, mm4, mm5, mm0, mm1, mm2, PUT_OP
        QPEL_CAVSV2 mm4, mm5, mm0, mm1, mm2, mm3, PUT_OP
        QPEL_CAVSV2 mm5, mm0, mm1, mm2, mm3, mm4, PUT_OP
        QPEL_CAVSV2 mm0, mm1, mm2, mm3, mm4, mm5, PUT_OP
        QPEL_CAVSV2 mm1, mm2, mm3, mm4, mm5, mm0, PUT_OP
        
		REP_RET

;-----------------------------------------------------------------------------------
;cavs_put_qpel8or16_v2_h16_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride);
;-----------------------------------------------------------------------------------
cglobal put_qpel8or16_v2_h16_mmxext, 4, 7
        QPEL_CAVSV2 mm2, mm3, mm4, mm5, mm0, mm1, PUT_OP
        QPEL_CAVSV2 mm3, mm4, mm5, mm0, mm1, mm2, PUT_OP
        QPEL_CAVSV2 mm4, mm5, mm0, mm1, mm2, mm3, PUT_OP
        QPEL_CAVSV2 mm5, mm0, mm1, mm2, mm3, mm4, PUT_OP
        QPEL_CAVSV2 mm0, mm1, mm2, mm3, mm4, mm5, PUT_OP
        QPEL_CAVSV2 mm1, mm2, mm3, mm4, mm5, mm0, PUT_OP
        QPEL_CAVSV2 mm2, mm3, mm4, mm5, mm0, mm1, PUT_OP
        QPEL_CAVSV2 mm3, mm4, mm5, mm0, mm1, mm2, PUT_OP

		REP_RET
		
;-----------------------------------------------------------------------------------
;cavs_avg_qpel8or16_v2_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride);
;-----------------------------------------------------------------------------------
cglobal avg_qpel8or16_v2_mmxext, 4, 7
        pxor mm7, mm7
        movd mm0, [r1]
        add  r1, r3
        movd mm1, [r1]
        add  r1, r3
        movd mm2, [r1]
        add  r1, r3
        movd mm3, [r1]
        add r1, r3
        movd mm4, [r1]
        add r1, r3
        punpcklbw mm0, mm7     
        punpcklbw mm1, mm7    
        punpcklbw mm2, mm7    
        punpcklbw mm3, mm7    
        punpcklbw mm4, mm7    
        QPEL_CAVSV2 mm0, mm1, mm2, mm3, mm4, mm5, AVG_MMXEXT_OP
        QPEL_CAVSV2 mm1, mm2, mm3, mm4, mm5, mm0, AVG_MMXEXT_OP
        QPEL_CAVSV2 mm2, mm3, mm4, mm5, mm0, mm1, AVG_MMXEXT_OP
        QPEL_CAVSV2 mm3, mm4, mm5, mm0, mm1, mm2, AVG_MMXEXT_OP
        QPEL_CAVSV2 mm4, mm5, mm0, mm1, mm2, mm3, AVG_MMXEXT_OP
        QPEL_CAVSV2 mm5, mm0, mm1, mm2, mm3, mm4, AVG_MMXEXT_OP
        QPEL_CAVSV2 mm0, mm1, mm2, mm3, mm4, mm5, AVG_MMXEXT_OP
        QPEL_CAVSV2 mm1, mm2, mm3, mm4, mm5, mm0, AVG_MMXEXT_OP
        
		REP_RET
		
;-----------------------------------------------------------------------------------
;cavs_avg_qpel8or16_v2_h16_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride);
;-----------------------------------------------------------------------------------
cglobal avg_qpel8or16_v2_h16_mmxext, 4, 7
        QPEL_CAVSV2 mm2, mm3, mm4, mm5, mm0, mm1, AVG_MMXEXT_OP
        QPEL_CAVSV2 mm3, mm4, mm5, mm0, mm1, mm2, AVG_MMXEXT_OP
        QPEL_CAVSV2 mm4, mm5, mm0, mm1, mm2, mm3, AVG_MMXEXT_OP
        QPEL_CAVSV2 mm5, mm0, mm1, mm2, mm3, mm4, AVG_MMXEXT_OP
        QPEL_CAVSV2 mm0, mm1, mm2, mm3, mm4, mm5, AVG_MMXEXT_OP
        QPEL_CAVSV2 mm1, mm2, mm3, mm4, mm5, mm0, AVG_MMXEXT_OP
        QPEL_CAVSV2 mm2, mm3, mm4, mm5, mm0, mm1, AVG_MMXEXT_OP
        QPEL_CAVSV2 mm3, mm4, mm5, mm0, mm1, mm2, AVG_MMXEXT_OP

		REP_RET				

;-------------------------------------------------------------------------------
; v3
;-------------------------------------------------------------------------------
%macro QPEL_CAVSV3 7
        movd %6, [r1]            
        movq mm6, %3          
        pmullw mm6, [ff_pw_42]
        movq mm7, %4           
        pmullw mm7,[ff_pw_96]           
        psllw %2, 3             
        psubw mm6, %2          
        psraw %2, 3             
        paddw mm6, mm7         
        paddw mm6, %2          
        paddw %5, %5           
        pxor mm7, mm7          
        add r1, r3                 
        punpcklbw %6, mm7      
        psubw mm6, %5          
        psraw %5, 1             
        psubw mm6, %6          
        paddw mm6, [ff_pw_64]            
        psraw mm6, 7            
        packuswb mm6, mm6      
        %7 mm6, r0, %1, d       
        add r0, r2                 
%endmacro

;-----------------------------------------------------------------------------------
;cavs_put_qpel8or16_v3_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride);
;-----------------------------------------------------------------------------------
cglobal put_qpel8or16_v3_mmxext, 4, 7
        pxor mm7, mm7
        movd mm0, [r1]
        add  r1, r3
        movd mm1, [r1]
        add  r1, r3
        movd mm2, [r1]
        add  r1, r3
        movd mm3, [r1]
        add r1, r3
        movd mm4, [r1]
        add r1, r3
        punpcklbw mm0, mm7     
        punpcklbw mm1, mm7    
        punpcklbw mm2, mm7    
        punpcklbw mm3, mm7    
        punpcklbw mm4, mm7    
        QPEL_CAVSV3 mm0, mm1, mm2, mm3, mm4, mm5, PUT_OP
        QPEL_CAVSV3 mm1, mm2, mm3, mm4, mm5, mm0, PUT_OP
        QPEL_CAVSV3 mm2, mm3, mm4, mm5, mm0, mm1, PUT_OP
        QPEL_CAVSV3 mm3, mm4, mm5, mm0, mm1, mm2, PUT_OP
        QPEL_CAVSV3 mm4, mm5, mm0, mm1, mm2, mm3, PUT_OP
        QPEL_CAVSV3 mm5, mm0, mm1, mm2, mm3, mm4, PUT_OP
        QPEL_CAVSV3 mm0, mm1, mm2, mm3, mm4, mm5, PUT_OP
        QPEL_CAVSV3 mm1, mm2, mm3, mm4, mm5, mm0, PUT_OP
        
		REP_RET

;-----------------------------------------------------------------------------------
;cavs_put_qpel8or16_v3_h16_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride);
;-----------------------------------------------------------------------------------
cglobal put_qpel8or16_v3_h16_mmxext, 4, 7
        QPEL_CAVSV3 mm2, mm3, mm4, mm5, mm0, mm1, PUT_OP
        QPEL_CAVSV3 mm3, mm4, mm5, mm0, mm1, mm2, PUT_OP
        QPEL_CAVSV3 mm4, mm5, mm0, mm1, mm2, mm3, PUT_OP
        QPEL_CAVSV3 mm5, mm0, mm1, mm2, mm3, mm4, PUT_OP
        QPEL_CAVSV3 mm0, mm1, mm2, mm3, mm4, mm5, PUT_OP
        QPEL_CAVSV3 mm1, mm2, mm3, mm4, mm5, mm0, PUT_OP
        QPEL_CAVSV3 mm2, mm3, mm4, mm5, mm0, mm1, PUT_OP
        QPEL_CAVSV3 mm3, mm4, mm5, mm0, mm1, mm2, PUT_OP

		REP_RET
		
;-----------------------------------------------------------------------------------
;cavs_avg_qpel8or16_v3_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride);
;-----------------------------------------------------------------------------------
cglobal avg_qpel8or16_v3_mmxext, 4, 7
        pxor mm7, mm7
        movd mm0, [r1]
        add  r1, r3
        movd mm1, [r1]
        add  r1, r3
        movd mm2, [r1]
        add  r1, r3
        movd mm3, [r1]
        add r1, r3
        movd mm4, [r1]
        add r1, r3
        punpcklbw mm0, mm7     
        punpcklbw mm1, mm7    
        punpcklbw mm2, mm7    
        punpcklbw mm3, mm7    
        punpcklbw mm4, mm7    
        QPEL_CAVSV3 mm0, mm1, mm2, mm3, mm4, mm5, AVG_MMXEXT_OP
        QPEL_CAVSV3 mm1, mm2, mm3, mm4, mm5, mm0, AVG_MMXEXT_OP
        QPEL_CAVSV3 mm2, mm3, mm4, mm5, mm0, mm1, AVG_MMXEXT_OP
        QPEL_CAVSV3 mm3, mm4, mm5, mm0, mm1, mm2, AVG_MMXEXT_OP
        QPEL_CAVSV3 mm4, mm5, mm0, mm1, mm2, mm3, AVG_MMXEXT_OP
        QPEL_CAVSV3 mm5, mm0, mm1, mm2, mm3, mm4, AVG_MMXEXT_OP
        QPEL_CAVSV3 mm0, mm1, mm2, mm3, mm4, mm5, AVG_MMXEXT_OP
        QPEL_CAVSV3 mm1, mm2, mm3, mm4, mm5, mm0, AVG_MMXEXT_OP
        
		REP_RET
		
;-----------------------------------------------------------------------------------
;cavs_avg_qpel8or16_v3_h16_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride);
;-----------------------------------------------------------------------------------
cglobal avg_qpel8or16_v3_h16_mmxext, 4, 7
        QPEL_CAVSV3 mm2, mm3, mm4, mm5, mm0, mm1, AVG_MMXEXT_OP
        QPEL_CAVSV3 mm3, mm4, mm5, mm0, mm1, mm2, AVG_MMXEXT_OP
        QPEL_CAVSV3 mm4, mm5, mm0, mm1, mm2, mm3, AVG_MMXEXT_OP
        QPEL_CAVSV3 mm5, mm0, mm1, mm2, mm3, mm4, AVG_MMXEXT_OP
        QPEL_CAVSV3 mm0, mm1, mm2, mm3, mm4, mm5, AVG_MMXEXT_OP
        QPEL_CAVSV3 mm1, mm2, mm3, mm4, mm5, mm0, AVG_MMXEXT_OP
        QPEL_CAVSV3 mm2, mm3, mm4, mm5, mm0, mm1, AVG_MMXEXT_OP
        QPEL_CAVSV3 mm3, mm4, mm5, mm0, mm1, mm2, AVG_MMXEXT_OP

		REP_RET				





INIT_XMM

%macro QPEL_CAVS_H1 1
	movq xmm0, [r1] ;src[0]
	movq xmm1, [r1+1] ;src[1]
	movq xmm2, [r1+2] ;src[2]

	punpcklbw xmm0, xmm7
	punpcklbw xmm1, xmm7
	punpcklbw xmm2, xmm7

	pmullw xmm0, [ff_pw_96] ;96*src[0]
	pmullw xmm1, [ff_pw_42] ;42*src[1]
	pmullw xmm2, [ff_pw_7] ; 7*src[2]

	paddw xmm0, xmm1; 96*src[0] + 42*src[1]
	psubw xmm0, xmm2; 96*src[0] + 42*src[1] - 7*src[2]

	movq xmm1, [r1-1] ; src[-1]
	movq xmm2, [r1-2] ; src[-2]
	
	punpcklbw xmm1, xmm7
	punpcklbw xmm2, xmm7

	psllw xmm1, 1;2*src[-1]
	paddw xmm1, xmm2;src[-2] + 2*src[-1]

	psubw xmm0, xmm1; -src[-2] - 2*src[-2] + 96*src[0] + 42*src[1] - 7*src[2]

	paddw xmm0, [ff_pw_64]
	psraw xmm0, 7

	packuswb xmm0, xmm0
	;movq [r0], xmm0 ; put

	%1 xmm0, r0, xmm5, q

	add r0, r2
	add r1, r3

%endmacro

;--------------------------------------------------------------------------------------
; cavs_put_qpel8or16_h1_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride);
;--------------------------------------------------------------------------------------
cglobal put_qpel8or16_h1_mmxext, 4, 7	
	pxor xmm7, xmm7
	
	QPEL_CAVS_H1 PUT_OP
	QPEL_CAVS_H1 PUT_OP
	QPEL_CAVS_H1 PUT_OP
	QPEL_CAVS_H1 PUT_OP

	QPEL_CAVS_H1 PUT_OP
	QPEL_CAVS_H1 PUT_OP
	QPEL_CAVS_H1 PUT_OP
	QPEL_CAVS_H1 PUT_OP

	REP_RET



;--------------------------------------------------------------------------------------
; cavs_avg_qpel8or16_h1_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride);
;--------------------------------------------------------------------------------------
cglobal avg_qpel8or16_h1_mmxext, 4, 7	
    
	pxor xmm7, xmm7
	
	QPEL_CAVS_H1 AVG_MMXEXT_OP
	QPEL_CAVS_H1 AVG_MMXEXT_OP
	QPEL_CAVS_H1 AVG_MMXEXT_OP
	QPEL_CAVS_H1 AVG_MMXEXT_OP

	QPEL_CAVS_H1 AVG_MMXEXT_OP
	QPEL_CAVS_H1 AVG_MMXEXT_OP
	QPEL_CAVS_H1 AVG_MMXEXT_OP
	QPEL_CAVS_H1 AVG_MMXEXT_OP

	REP_RET


INIT_XMM

%macro QPEL_CAVS_H2 1
	movq xmm1, [r1-1] ;src[-1]
	movq xmm0, [r1] ;src[0]
	movq xmm2, [r1+1] ;src[1]

	punpcklbw xmm0, xmm7
	punpcklbw xmm1, xmm7
	punpcklbw xmm2, xmm7

	pmullw xmm2, [ff_pw_96] ;96*src[1]
	pmullw xmm0, [ff_pw_42] ;42*src[0]
	pmullw xmm1, [ff_pw_7] ; 7*src[-1]

	paddw xmm0, xmm2; 96*src[1] + 42*src[0]
	psubw xmm0, xmm1; -7*src[-1] + 96*src[0] + 42*src[1]

	movq xmm1, [r1+2] ; src[2]
	movq xmm2, [r1+3] ; src[3]
	
	punpcklbw xmm1, xmm7
	punpcklbw xmm2, xmm7

	psllw xmm1, 1;2*src[2]
	paddw xmm1, xmm2;src[3] + 2*src[2]

	psubw xmm0, xmm1; -7*src[-1] + 42*src[0] + 96*src[1] - 2*src[2] - src[3]

	paddw xmm0, [ff_pw_64]
	psraw xmm0, 7

	packuswb xmm0, xmm0
	;movq [r0], xmm0 ; put

	%1 xmm0, r0, xmm5, q

	add r0, r2
	add r1, r3

%endmacro

;--------------------------------------------------------------------------------------
; cavs_put_qpel8or16_h2_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride);
;--------------------------------------------------------------------------------------
cglobal put_qpel8or16_h2_mmxext, 4, 7	
	pxor xmm7, xmm7
	
	QPEL_CAVS_H2 PUT_OP
	QPEL_CAVS_H2 PUT_OP
	QPEL_CAVS_H2 PUT_OP
	QPEL_CAVS_H2 PUT_OP

	QPEL_CAVS_H2 PUT_OP
	QPEL_CAVS_H2 PUT_OP
	QPEL_CAVS_H2 PUT_OP
	QPEL_CAVS_H2 PUT_OP

	REP_RET



;--------------------------------------------------------------------------------------
; cavs_avg_qpel8or16_h2_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride);
;--------------------------------------------------------------------------------------
cglobal avg_qpel8or16_h2_mmxext, 4, 7	
    
	pxor xmm7, xmm7
	
	QPEL_CAVS_H2 AVG_MMXEXT_OP
	QPEL_CAVS_H2 AVG_MMXEXT_OP
	QPEL_CAVS_H2 AVG_MMXEXT_OP
	QPEL_CAVS_H2 AVG_MMXEXT_OP

	QPEL_CAVS_H2 AVG_MMXEXT_OP
	QPEL_CAVS_H2 AVG_MMXEXT_OP
	QPEL_CAVS_H2 AVG_MMXEXT_OP
	QPEL_CAVS_H2 AVG_MMXEXT_OP

	REP_RET


;--------------------------------------------------------------------------------------
; cavs_put_qpel8or16_j_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride);
;--------------------------------------------------------------------------------------
;assume m7 is 0
%macro LOAD_B_AS_W 2
	movh %1, %2
	pxor m7, m7 ;
	punpcklbw %1, m7;
	;pmovzxbw %1, %1 ; NOTE : not right on Intel(R) Xeon(R) CPU X5650 
%endmacro

%macro FILTER3 7
	LOAD_B_AS_W %1, %4
	LOAD_B_AS_W %2, %5
	paddw	%1, %2 ; pix[0]+pix[1]
	;pmullw	%1, [pw_5]	
	mova      %7, %1
	psllw %1, 2 ; 4*(pix[0]+pix[1])
	paddw %1, %7 ;5*(pix[0]+pix[1])
	
	LOAD_B_AS_W %2, %3
	psubw	%1, %2
	LOAD_B_AS_W %2, %6
	psubw	%1, %2
%endmacro

;Filter: (-%2 + 5*%3 + 5*%4 - %5 + 32) >> 6
;out: %1
%macro FILTER4 6
	mova		%1, %3
	paddw		%1, %4
	;pmullw		%1, [pw_5]
    mova             %6, %1
    psllw             %1, 2
    paddw           %1, %6
       
	psubw		%1, %2
	psubw		%1, %5
%endmacro

cglobal put_qpel8or16_j_mmxext, 4, 7
	xor		r5, r5
	sub		r5, r3;-1*srcStride
	mov     r6d, 8

	FILTER3	m3, m4, [r1+r5-1], [r1+r5], [r1+r5+1], [r1+r5+2], m7
	FILTER3	m4, m5, [r1-1], [r1], [r1+1], [r1+2], m7
	FILTER3	m5, m6, [r1+r3-1], [r1+r3], [r1+r3+1], [r1+r3+2] , m7
.height_loop:
	;FILTER3	m6, m2, [r1+r3*2-1], [r1+r3*2], [r1+r3*2+1], [r1+r3*2+2], m7
	FILTER3	m6, m2, [r1+r3+r3-1], [r1+r3+r3], [r1+r3+r3+1], [r1+r3+r3+2], m7

	FILTER4 m0, m3, m4, m5, m6, m7
	paddw		m0, [ff_pw_32]
	psraw		m0, 6
	packuswb	m0, m0
	movh		[r0], m0 ;dst

	;SWAP	3, 4, 5, 6
	mova	m3, m4
	mova	m4, m5
	mova	m5, m6

	lea		r0, [r0+r2] ;dst
	lea		r1, [r1+r3]	;src
	dec		r6d
	jg		.height_loop	

	REP_RET

;--------------------------------------------------------------------------------------
; cavs_avg_qpel8or16_j_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride);
;--------------------------------------------------------------------------------------
cglobal avg_qpel8or16_j_mmxext, 4, 7
	xor		r5, r5
	sub		r5, r3;-1*srcStride
	mov     r6d, 8

	FILTER3	m3, m4, [r1+r5-1], [r1+r5], [r1+r5+1], [r1+r5+2], m7
	FILTER3	m4, m5, [r1-1], [r1], [r1+1], [r1+2], m7
	FILTER3	m5, m6, [r1+r3-1], [r1+r3], [r1+r3+1], [r1+r3+2] , m7
.height_loop:
	;FILTER3	m6, m2, [r1+r3*2-1], [r1+r3*2], [r1+r3*2+1], [r1+r3*2+2], m7
	FILTER3	m6, m2, [r1+r3+r3-1], [r1+r3+r3], [r1+r3+r3+1], [r1+r3+r3+2], m7

	FILTER4 m0, m3, m4, m5, m6, m7
	paddw		m0, [ff_pw_32]
	psraw		m0, 6
	packuswb	m0, m0
	;movh		[r0], m0 ;dst
	AVG_MMXEXT_OP m0, r0, m1, q

	;SWAP	3, 4, 5, 6
	mova	m3, m4
	mova	m4, m5
	mova	m5, m6

	lea		r0, [r0+r2] ;dst
	lea		r1, [r1+r3]	;src
	dec		r6d
	jg		.height_loop	

	REP_RET


;---------------------------------------------------------------------------------------------
; cavs_get_qpel8or16_c1_mmxext(uint16_t *mid_dst, uint8_t *src, int dstStride, int srcStride);
;
; mid_dst : uint16_t [16][16]
;---------------------------------------------------------------------------------------------
cglobal get_qpel8or16_c1_mmxext, 4, 7
	xor		r5, r5
	sub		r5, r3;-1*srcStride
	mov     r6d, 8

	FILTER3	m3, m4, [r1+r5-1], [r1+r5], [r1+r5+1], [r1+r5+2], m7
	FILTER3	m4, m5, [r1-1], [r1], [r1+1], [r1+2], m7
	FILTER3	m5, m6, [r1+r3-1], [r1+r3], [r1+r3+1], [r1+r3+2] , m7
.height_loop:
	;FILTER3	m6, m2, [r1+r3*2-1], [r1+r3*2], [r1+r3*2+1], [r1+r3*2+2], m7
	FILTER3	m6, m2, [r1+r3+r3-1], [r1+r3+r3], [r1+r3+r3+1], [r1+r3+r3+2], m7

	FILTER4 m0, m3, m4, m5, m6, m7
	mova    [r0], m0 ;mid_dst

	;SWAP	3, 4, 5, 6
	mova	m3, m4
	mova	m4, m5
	mova	m5, m6

	;lea		r0, [r0+2*16] ;dst, uint16_t = 2*(uint8_t)
	lea		r0, [r0+32] ;dst, uint16_t = 2*(uint8_t)
	lea		r1, [r1+r3]	;src
	dec		r6d
	jg		.height_loop	

	REP_RET

;-----------------------------------------------------------------------------------------------------------
; cavs_put_qpel8or16_c1_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride, uint16_t *mid_dst);
;
; mid_dst : uint16_t [16][16], only use 8x8 when every call
;-----------------------------------------------------------------------------------------------------------
cglobal put_qpel8or16_c1_mmxext, 5, 7
	mov     r6d, 8
.height_loop:
    LOAD_B_AS_W m0, [r1] ; src 
	psllw   m0, 5 ; 32
	movu    m1, [r4] ; mid_dst
	psraw   m1, 1
	paddw   m0, m1
	paddw   m0, [ff_pw_32]
	psraw   m0, 6
	packuswb m0, m0
	movh    [r0], m0

	lea		r0, [r0+r2] ;dst
	lea		r1, [r1+r3]	;src
	lea     r4, [r4+16*2]; mid_dst
	dec		r6d
	jg		.height_loop	

	REP_RET

;-----------------------------------------------------------------------------------------------------------
; cavs_avg_qpel8or16_c1_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride, uint16_t *mid_dst);
;
; mid_dst : uint16_t [16][16]
;-----------------------------------------------------------------------------------------------------------
cglobal avg_qpel8or16_c1_mmxext, 5, 7
	mov     r6d, 8
.height_loop:
    LOAD_B_AS_W m0, [r1] ; src 
	psllw   m0, 5 ; 32
	movu    m1, [r4] ; mid_dst
	psraw   m1, 1
	paddw   m0, m1
	paddw   m0, [ff_pw_32]
	psraw   m0, 6
	packuswb m0, m0
	;movh    [r0], m0
	AVG_MMXEXT_OP m0, r0, m1, q

	lea		r0, [r0+r2] ;dst
	lea		r1, [r1+r3]	;src
	lea     r4, [r4+16*2]; mid_dst
	dec		r6d
	jg		.height_loop	

	REP_RET

;---------------------------------------------------------------------------------------------
; cavs_get_qpel8or16_hh_mmxext(uint16_t *mid_dst, uint8_t *src, int dstStride, int srcStride);
;
; mid_dst : uint16_t [24][24] , only use 16x16 every call
;---------------------------------------------------------------------------------------------
cglobal get_qpel8or16_hh_mmxext, 4, 7
	mov     r6d, 16
.height_loop:
	FILTER3	m0, m1, [r1-1], [r1], [r1+1], [r1+2], m7
	mova [r0], m0
	FILTER3	m1, m2, [r1+8-1], [r1+8], [r1+8+1], [r1+8+2], m7
	mova [r0+16], m1	
	
	lea		r1, [r1+r3]
	;lea		r0, [r0+24*2]
	lea		r0, [r0+48]
	dec		r6d
	jg		.height_loop
	
	REP_RET

;---------------------------------------------------------------------------------------------
; cavs_get_qpel8or16_hv_mmxext(uint16_t *mid_dst, uint8_t *src, int dstStride, int srcStride);
;
; mid_dst : uint16_t [24][24]
;---------------------------------------------------------------------------------------------
cglobal get_qpel8or16_hv_mmxext, 4,7
	mov     r6d, 16
	xor		r5, r5
	sub		r5, r3
.height_loop:
	;FILTER3	m0, m1, [r1+r5], [r1], [r1+r3], [r1+2*r3], m7
	FILTER3	m0, m1, [r1+r5], [r1], [r1+r3], [r1+r3+r3], m7
	mova	[r0], m0
	;FILTER3	m1, m2, [r1+8+r5], [r1+8], [r1+8+r3], [r1+8+2*r3], m7
	FILTER3	m1, m2, [r1+8+r5], [r1+8], [r1+8+r3], [r1+8+r3+r3], m7
	mova	[r0+16], m1

	lea		r1, [r1+r3] ;src
	;lea		r0, [r0+24*2] ;mid_dst
	lea		r0, [r0+48] ;mid_dst

	dec		r6d
	jg		.height_loop
	
	REP_RET

;---------------------------------------------------------------------------------------------
; cavs_get_qpel8or16_hc_mmxext(uint16_t *mid_dst, uint8_t *src, int dstStride, int srcStride);
;
; mid_dst : uint16_t [16][16]
;---------------------------------------------------------------------------------------------
cglobal get_qpel8or16_hc_mmxext, 4,7
	xor		r5, r5
	sub		r5, r3;-1*srcStride
	mov     r6d, 16

	push r6
	push r1
	push r0

	FILTER3	m3, m4, [r1+r5-1], [r1+r5], [r1+r5+1], [r1+r5+2], m7
	FILTER3	m4, m5, [r1-1], [r1], [r1+1], [r1+2], m7
	FILTER3	m5, m6, [r1+r3-1], [r1+r3], [r1+r3+1], [r1+r3+2] , m7
.height_loop:
	;FILTER3	m6, m2, [r1+r3*2-1], [r1+r3*2], [r1+r3*2+1], [r1+r3*2+2], m7
	FILTER3	m6, m2, [r1+r3+r3-1], [r1+r3+r3], [r1+r3+r3+1], [r1+r3+r3+2], m7

	FILTER4 m0, m3, m4, m5, m6, m7
	mova    [r0], m0 ;mid_dst

	;SWAP	3, 4, 5, 6
	mova	m3, m4
	mova	m4, m5
	mova	m5, m6

	;lea		r0, [r0+2*24] ;dst, uint16_t = 2*(uint8_t)
	lea		r0, [r0+48] ;dst, uint16_t = 2*(uint8_t)
	lea		r1, [r1+r3]	;src
	dec		r6d
	jg		.height_loop
	
	pop r0
	pop r1
	pop r6

	add r0, 16
	add r1,8
	
	FILTER3	m3, m4, [r1+r5-1], [r1+r5], [r1+r5+1], [r1+r5+2], m7
	FILTER3	m4, m5, [r1-1], [r1], [r1+1], [r1+2], m7
	FILTER3	m5, m6, [r1+r3-1], [r1+r3], [r1+r3+1], [r1+r3+2] , m7
.height_loop2:
	;FILTER3	m6, m2, [r1+r3*2-1], [r1+r3*2], [r1+r3*2+1], [r1+r3*2+2], m7
	FILTER3	m6, m2, [r1+r3+r3-1], [r1+r3+r3], [r1+r3+r3+1], [r1+r3+r3+2], m7

	FILTER4 m0, m3, m4, m5, m6, m7
	mova    [r0], m0 ;mid_dst

	;SWAP	3, 4, 5, 6
	mova	m3, m4
	mova	m4, m5
	mova	m5, m6

	;lea		r0, [r0+2*24] ;dst, uint16_t = 2*(uint8_t)
	lea		r0, [r0+48] ;dst, uint16_t = 2*(uint8_t)
	lea		r1, [r1+r3]	;src
	dec		r6d
	jg		.height_loop2		

	REP_RET

;-----------------------------------------------------------------------------------------------------------------------------
; cavs_put_qpel8or16_i_mmxext(uint8_t *dst, int dstStride, uint16_t *mid_hv, uint16_t *mid_c );
;
; mid_hv : uint16_t [24][24]
; mid_c  : uint16_t [24][24]
;-----------------------------------------------------------------------------------------------------------------------------

%macro DO_LOAD_D 4
	mov%3		%1, [%2]
	%if %4 = 1
		pxor    xmm7, xmm7 
		punpcklwd	%1, xmm7
	%endif
%endmacro

%macro LOAD_ALL_AS_D 16
	DO_LOAD_D		%1, %2, %3, %4
	DO_LOAD_D		%5, %6, %7, %8
	DO_LOAD_D		%9, %10, %11, %12
	DO_LOAD_D		%13, %14, %15, %16
%endmacro

;dst: %1
%macro FILTER1_D 10-12
    mova %11, %1
    punpckhwd %11, %3
	
	punpcklwd	%1, %3
	pmaddwd		%11, [pww_%2_%4];pww_1_56
	pmaddwd		%1,  [pww_%2_%4];pww_1_56

    mova %3, %5
    punpckhwd %3, %7
	
	punpcklwd	%5, %7
	pmaddwd		%3, [pww_%6_%8];pww_7_8
	pmaddwd		%5, [pww_%6_%8];pww_7_8

	paddd		%11, %3
	paddd		%1, %5
	paddd		%11, [pd_%9];pd_512
	psrad		%11, %10
	paddd		%1,  [pd_%9];pd_512
	psrad		%1, %10
	packssdw	%1, %11
%endmacro

cglobal put_qpel8or16_i_mmxext, 4, 7
	mov     r6d, 8
.height_loop:
    LOAD_ALL_AS_D m0, r3-2, u, 0, m1, r2, u, 0, m2, r3, u, 0, m3, r2+2, u, 0
	FILTER1_D	m0, 1, m1, 56, m2, 7, m3, 8, 512, 10, m4, m5
	
	packuswb m0, m0
	movh	[r0], m0

	lea r0, [r0 + r1]
	;lea r2, [r2 + 24*2]
	;lea	r3, [r3 + 24*2]
	lea r2, [r2 + 48]
	lea	r3, [r3 + 48]
	
	dec r6d
	jg	.height_loop

	REP_RET


;-----------------------------------------------------------------------------------------------------------------------------
; cavs_avg_qpel8or16_i_mmxext(uint8_t *dst, int dstStride, uint16_t *mid_hv, uint16_t *mid_c );
;
; mid_hv : uint16_t [24][24]
; mid_c  : uint16_t [24][24]
;-----------------------------------------------------------------------------------------------------------------------------
cglobal avg_qpel8or16_i_mmxext, 4, 7
	mov     r6d, 8
.height_loop:
    LOAD_ALL_AS_D m0, r3-2, u, 0, m1, r2, u, 0, m2, r3, u, 0, m3, r2+2, u, 0
	FILTER1_D	m0, 1, m1, 56, m2, 7, m3, 8, 512, 10, m4, m5
	
	packuswb m0, m0
	;movh	[r0], m0
	AVG_MMXEXT_OP m0, r0, m1, q

	lea r0, [r0 + r1]
	;lea r2, [r2 + 24*2]
	;lea	r3, [r3 + 24*2]
	lea r2, [r2 + 48]
	lea	r3, [r3 + 48]
	
	dec r6d
	jg	.height_loop

	REP_RET


;-----------------------------------------------------------------------------------------------------------------------------
; cavs_put_qpel8or16_k_mmxext(uint8_t *dst, int dstStride, uint16_t *mid_hv, uint16_t *mid_c );
;
; mid_hv : uint16_t [24][24]
; mid_c  : uint16_t [24][24]
;-----------------------------------------------------------------------------------------------------------------------------
cglobal put_qpel8or16_k_mmxext, 4, 7
	mov     r6d, 8
.height_loop:
    LOAD_ALL_AS_D m0, r3+2, u, 0, m1, r2+2, u, 0, m2, r3, u, 0, m3, r2, u, 0
	FILTER1_D	m0, 1, m1, 56, m2, 7, m3, 8, 512, 10, m4, m5
	
	packuswb m0, m0
	movh	[r0], m0

	lea r0, [r0 + r1]
	;lea r2, [r2 + 24*2]
	;lea	r3, [r3 + 24*2]
	lea r2, [r2 + 48]
	lea	r3, [r3 + 48]
	
	dec r6d
	jg	.height_loop

	REP_RET


;-----------------------------------------------------------------------------------------------------------------------------
; cavs_avg_qpel8or16_k_mmxext(uint8_t *dst, int dstStride, uint16_t *mid_hv, uint16_t *mid_c );
;
; mid_hv : uint16_t [24][24]
; mid_c  : uint16_t [24][24]
;-----------------------------------------------------------------------------------------------------------------------------
cglobal avg_qpel8or16_k_mmxext, 4, 7
	mov     r6d, 8
.height_loop:
    LOAD_ALL_AS_D m0, r3+2, u, 0, m1, r2+2, u, 0, m2, r3, u, 0, m3, r2, u, 0
	FILTER1_D	m0, 1, m1, 56, m2, 7, m3, 8, 512, 10, m4, m5
	
	packuswb m0, m0
	;movh	[r0], m0
	AVG_MMXEXT_OP m0, r0, m1, q

	lea r0, [r0 + r1]
	;lea r2, [r2 + 24*2]
	;lea	r3, [r3 + 24*2]
	lea r2, [r2 + 48]
	lea	r3, [r3 + 48]
	
	dec r6d
	jg	.height_loop

	REP_RET


;-----------------------------------------------------------------------------------------------------------------------------
; cavs_put_qpel8or16_f_mmxext(uint8_t *dst, int dstStride, uint16_t *mid_hv, uint16_t *mid_c );
;
; mid_hv : uint16_t [24][24]
; mid_c  : uint16_t [24][24]
;-----------------------------------------------------------------------------------------------------------------------------
cglobal put_qpel8or16_f_mmxext, 4, 7
	mov     r6d, 8
.height_loop:
        ;LOAD_ALL_AS_D m0, r3 - 2*24, u, 0, m1, r2, u, 0, m2, r3, u, 0, m3, r2 + 2*24, u, 0
        LOAD_ALL_AS_D m0, r3 - 48, u, 0, m1, r2, u, 0, m2, r3, u, 0, m3, r2 + 48, u, 0
	FILTER1_D	m0, 1, m1, 56, m2, 7, m3, 8, 512, 10, m4, m5
	
	packuswb m0, m0
	movh	[r0], m0

	lea r0, [r0 + r1]
	;lea r2, [r2 + 24*2]
	;lea	r3, [r3 + 24*2]
	lea r2, [r2 + 48]
	lea	r3, [r3 + 48]
	
	dec r6d
	jg	.height_loop

	REP_RET


;-----------------------------------------------------------------------------------------------------------------------------
; cavs_avg_qpel8or16_f_mmxext(uint8_t *dst, int dstStride, uint16_t *mid_hv, uint16_t *mid_c );
;
; mid_hv : uint16_t [24][24]
; mid_c  : uint16_t [24][24]
;-----------------------------------------------------------------------------------------------------------------------------
cglobal avg_qpel8or16_f_mmxext, 4, 7
	mov     r6d, 8
.height_loop:
        ;LOAD_ALL_AS_D m0, r3 - 2*24, u, 0, m1, r2, u, 0, m2, r3, u, 0, m3, r2 + 2*24, u, 0
        LOAD_ALL_AS_D m0, r3 - 48, u, 0, m1, r2, u, 0, m2, r3, u, 0, m3, r2 + 48, u, 0
	FILTER1_D	m0, 1, m1, 56, m2, 7, m3, 8, 512, 10, m4, m5
	
	packuswb m0, m0
	;movh	[r0], m0
	AVG_MMXEXT_OP m0, r0, m1, q

	lea r0, [r0 + r1]
	;lea r2, [r2 + 24*2]
	;lea	r3, [r3 + 24*2]
	lea r2, [r2 + 48]
	lea	r3, [r3 + 48]
	
	dec r6d
	jg	.height_loop

	REP_RET


;-----------------------------------------------------------------------------------------------------------------------------
; cavs_put_qpel8or16_q_mmxext(uint8_t *dst, int dstStride, uint16_t *mid_hv, uint16_t *mid_c );
;
; mid_hv : uint16_t [24][24]
; mid_c  : uint16_t [24][24]
;-----------------------------------------------------------------------------------------------------------------------------
cglobal put_qpel8or16_q_mmxext, 4, 7
	mov     r6d, 8
.height_loop:
        ;LOAD_ALL_AS_D m0, r3 + 2*24, u, 0, m1, r2+2*24, u, 0, m2, r3, u, 0, m3, r2, u, 0
        LOAD_ALL_AS_D m0, r3 + 48, u, 0, m1, r2 + 48, u, 0, m2, r3, u, 0, m3, r2, u, 0
	FILTER1_D	m0, 1, m1, 56, m2, 7, m3, 8, 512, 10, m4, m5
	
	packuswb m0, m0
	movh	[r0], m0

	lea r0, [r0 + r1]
	;lea r2, [r2 + 24*2]
	;lea	r3, [r3 + 24*2]
	lea r2, [r2 + 48]
	lea	r3, [r3 + 48]
	
	dec r6d
	jg	.height_loop

	REP_RET

;-----------------------------------------------------------------------------------------------------------------------------
; cavs_avg_qpel8or16_q_mmxext(uint8_t *dst, int dstStride, uint16_t *mid_hv, uint16_t *mid_c );
;
; mid_hv : uint16_t [24][24]
; mid_c  : uint16_t [24][24]
;-----------------------------------------------------------------------------------------------------------------------------
cglobal avg_qpel8or16_q_mmxext, 4, 7
	mov     r6d, 8
.height_loop:
        ;LOAD_ALL_AS_D m0, r3 + 2*24, u, 0, m1, r2+2*24, u, 0, m2, r3, u, 0, m3, r2, u, 0
	LOAD_ALL_AS_D m0, r3 + 48, u, 0, m1, r2+ 48, u, 0, m2, r3, u, 0, m3, r2, u, 0
	FILTER1_D	m0, 1, m1, 56, m2, 7, m3, 8, 512, 10, m4, m5
	
	packuswb m0, m0
	;movh	[r0], m0
	AVG_MMXEXT_OP m0, r0, m1, q

	lea r0, [r0 + r1]
	;lea r2, [r2 + 24*2]
	;lea	r3, [r3 + 24*2]
	lea r2, [r2 + 48]
	lea	r3, [r3 + 48]
	
	dec r6d
	jg	.height_loop

	REP_RET


;-----------------------------------------------------------------------------------------------------------------------------
; cavs_put_qpel8or16_d_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride, uint16_t *mid_v );
;
; mid_v : uint16_t [24][24]
;-----------------------------------------------------------------------------------------------------------------------------
cglobal put_qpel8or16_d_mmxext, 5, 7
	mov     r6d, 8
.height_loop:
    ;LOAD_ALL_AS_D m0, r4 - 2*24, u, 0, m1, r1, u, 0, m2, r4, u, 0, m3, r1 + r3, u, 0
	;movu m0, [r4-2*24] ;mid_v - stride
	movu m0, [r4-48] ;mid_v - stride
	
	movu m1, [r1] ;src
	pxor m7, m7
	punpcklbw m1, m7 ; byte to word

	movu m2, [r4] ;mid_v

	movu m3, [r1+r3]; src+srcStride
	punpcklbw m3, m7 ; byte to word

	FILTER1_D	m0, 1, m1, 56, m2, 7, m3, 8, 64, 7, m4, m5
	
	packuswb m0, m0
	movh	[r0], m0

	lea r0, [r0 + r2] ;dst
	lea r1, [r1 + r3] ;src
	;lea	r4, [r4 + 24*2] ;mid_v
	lea	r4, [r4 + 48] ;mid_v

	dec r6d
	jg	.height_loop

	REP_RET

;-----------------------------------------------------------------------------------------------------------------------------
; cavs_avg_qpel8or16_d_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride, uint16_t *mid_v );
;
; mid_v : uint16_t [24][24]
;-----------------------------------------------------------------------------------------------------------------------------
cglobal avg_qpel8or16_d_mmxext, 5, 7
	mov     r6d, 8
.height_loop:
    ;LOAD_ALL_AS_D m0, r4 - 2*24, u, 0, m1, r1, u, 0, m2, r4, u, 0, m3, r1 + r3, u, 0
	;movu m0, [r4-2*24] ;mid_v - stride
	movu m0, [r4-48] ;mid_v - stride
	
	movu m1, [r1] ;src
	pxor m7, m7
	punpcklbw m1, m7 ; byte to word

	movu m2, [r4] ;mid_v

	movu m3, [r1+r3]; src+srcStride
	punpcklbw m3, m7 ; byte to word

	FILTER1_D	m0, 1, m1, 56, m2, 7, m3, 8, 64, 7, m4, m5
	
	packuswb m0, m0
	;movh	[r0], m0
	AVG_MMXEXT_OP m0, r0, m1, q

	lea r0, [r0 + r2] ;dst
	lea r1, [r1 + r3] ;src
	;lea	r4, [r4 + 24*2] ;mid_v
	lea	r4, [r4 + 48] ;mid_v

	dec r6d
	jg	.height_loop

	REP_RET


;-----------------------------------------------------------------------------------------------------------------------------
; cavs_put_qpel8or16_n_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride, uint16_t *mid_v );
;
; mid_v : uint16_t [24][24]
;-----------------------------------------------------------------------------------------------------------------------------
cglobal put_qpel8or16_n_mmxext, 5, 7
	mov     r6d, 8
.height_loop:
	;movu m0, [r4+2*24] ;mid_v + stride
	movu m0, [r4+48] ;mid_v + stride
	
	movu m1, [r1 + r3] ;src + stride
	pxor m7, m7
	punpcklbw m1, m7 ; byte to word

	movu m2, [r4] ;mid_v

	movu m3, [r1]; src
	punpcklbw m3, m7 ; byte to word

	FILTER1_D	m0, 1, m1, 56, m2, 7, m3, 8, 64, 7, m4, m5
	
	packuswb m0, m0
	movh	[r0], m0

	lea r0, [r0 + r2] ;dst
	lea r1, [r1 + r3] ;src
	;lea	r4, [r4 + 24*2] ;mid_v
	lea	r4, [r4 + 48] ;mid_v

	dec r6d
	jg	.height_loop

	REP_RET

;-----------------------------------------------------------------------------------------------------------------------------
; cavs_avg_qpel8or16_n_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride, uint16_t *mid_v );
;
; mid_v : uint16_t [24][24]
;-----------------------------------------------------------------------------------------------------------------------------
cglobal avg_qpel8or16_n_mmxext, 5, 7
	mov     r6d, 8
.height_loop:
	;movu m0, [r4+2*24] ;mid_v + stride
	movu m0, [r4 + 48] ;mid_v + stride
	
	movu m1, [r1 + r3] ;src + stride
	pxor m7, m7
	punpcklbw m1, m7 ; byte to word

	movu m2, [r4] ;mid_v

	movu m3, [r1]; src
	punpcklbw m3, m7 ; byte to word

	FILTER1_D	m0, 1, m1, 56, m2, 7, m3, 8, 64, 7, m4, m5
	
	packuswb m0, m0
	;movh	[r0], m0
	AVG_MMXEXT_OP m0, r0, m1, q

	lea r0, [r0 + r2] ;dst
	lea r1, [r1 + r3] ;src
	;lea	r4, [r4 + 24*2] ;mid_v
	lea	r4, [r4 + 48] ;mid_v

	dec r6d
	jg	.height_loop

	REP_RET

;-----------------------------------------------------------------------------------------------------------------------------
; cavs_put_qpel8or16_a_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride, uint16_t *mid_h );
;
; mid_h : uint16_t [24][24]
;-----------------------------------------------------------------------------------------------------------------------------
cglobal put_qpel8or16_a_mmxext, 5, 7
	mov     r6d, 8
.height_loop:
	movu m0, [r4-2] ;mid_h - 1
	
	movu m1, [r1] ;src
	pxor m7, m7
	punpcklbw m1, m7 ; byte to word

	movu m2, [r4] ;mid_h

	movu m3, [r1+1]; src+1
	punpcklbw m3, m7 ; byte to word

	FILTER1_D	m0, 1, m1, 56, m2, 7, m3, 8, 64, 7, m4, m5
	
	packuswb m0, m0
	movh	[r0], m0

	lea r0, [r0 + r2] ;dst
	lea r1, [r1 + r3] ;src
	;lea	r4, [r4 + 24*2] ;mid_h
	lea	r4, [r4 + 48] ;mid_h

	dec r6d
	jg	.height_loop

	REP_RET

;-----------------------------------------------------------------------------------------------------------------------------
; cavs_avg_qpel8or16_a_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride, uint16_t *mid_h );
;
; mid_h : uint16_t [24][24]
;-----------------------------------------------------------------------------------------------------------------------------
cglobal avg_qpel8or16_a_mmxext, 5, 7
	mov     r6d, 8
.height_loop:
	movu m0, [r4-2] ;mid_h - 1
	
	movu m1, [r1] ;src
	pxor m7, m7
	punpcklbw m1, m7 ; byte to word

	movu m2, [r4] ;mid_h

	movu m3, [r1+1]; src+1
	punpcklbw m3, m7 ; byte to word

	FILTER1_D	m0, 1, m1, 56, m2, 7, m3, 8, 64, 7, m4, m5
	
	packuswb m0, m0
	;movh	[r0], m0
	AVG_MMXEXT_OP m0, r0, m1, q

	lea r0, [r0 + r2] ;dst
	lea r1, [r1 + r3] ;src
	;lea	r4, [r4 + 24*2] ;mid_h
	lea	r4, [r4 + 48] ;mid_h

	dec r6d
	jg	.height_loop

	REP_RET

;-----------------------------------------------------------------------------------------------------------------------------
; cavs_put_qpel8or16_c_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride, uint16_t *mid_h );
;
; mid_h : uint16_t [24][24]
;-----------------------------------------------------------------------------------------------------------------------------
cglobal put_qpel8or16_c_mmxext, 5, 7
	mov     r6d, 8
.height_loop:
	movu m0, [r4+2] ;mid_h + 1
	
	movu m1, [r1+1] ;src + 1
	pxor m7, m7
	punpcklbw m1, m7 ; byte to word

	movu m2, [r4] ;mid_h

	movu m3, [r1]; src
	punpcklbw m3, m7 ; byte to word

	FILTER1_D	m0, 1, m1, 56, m2, 7, m3, 8, 64, 7, m4, m5
	
	packuswb m0, m0
	movh	[r0], m0

	lea r0, [r0 + r2] ;dst
	lea r1, [r1 + r3] ;src
	;lea	r4, [r4 + 24*2] ;mid_h
	lea	r4, [r4 + 48] ;mid_h

	dec r6d
	jg	.height_loop

	REP_RET

;-----------------------------------------------------------------------------------------------------------------------------
; cavs_avg_qpel8or16_c_mmxext(uint8_t *dst, uint8_t *src, int dstStride, int srcStride, uint16_t *mid_h );
;
; mid_h : uint16_t [24][24]
;-----------------------------------------------------------------------------------------------------------------------------
cglobal avg_qpel8or16_c_mmxext, 5, 7
	mov     r6d, 8
.height_loop:
	movu m0, [r4+2] ;mid_h + 1
	
	movu m1, [r1+1] ;src + 1
	pxor m7, m7
	punpcklbw m1, m7 ; byte to word

	movu m2, [r4] ;mid_h

	movu m3, [r1]; src
	punpcklbw m3, m7 ; byte to word

	FILTER1_D	m0, 1, m1, 56, m2, 7, m3, 8, 64, 7, m4, m5
	
	packuswb m0, m0
	;movh	[r0], m0
	AVG_MMXEXT_OP m0, r0, m1, q

	lea r0, [r0 + r2] ;dst
	lea r1, [r1 + r3] ;src
	;lea	r4, [r4 + 24*2] ;mid_h
	lea	r4, [r4 + 48] ;mid_h

	dec r6d
	jg	.height_loop

	REP_RET

;=============================================================================
; pixel avg
;=============================================================================

%if WIN64
    DECLARE_REG_TMP 0,1,2,3,4,5,4,5
    %macro AVG_START 0-1 0
        PROLOGUE 4,7,%1
    %endmacro
%elif UNIX64
    DECLARE_REG_TMP 0,1,2,3,4,5,7,8
    %macro AVG_START 0-1 0
        PROLOGUE 4,9,%1
    %endmacro
%else
    DECLARE_REG_TMP 1,2,3,4,5,6,1,2
    %macro AVG_START 0-1 0
        PROLOGUE 0,7,%1
        mov t0, r0m ;dst
        mov t1, r1m ;i_dst
        mov t2, r2m ;src
        mov t3, r3m ;i_src
    %endmacro
%endif

%macro AVG_END 0
    lea  t2, [t2+t3*2*SIZEOF_PIXEL] ; src + 2*i_src
    lea  t0, [t0+t1*2*SIZEOF_PIXEL] ; dst + 2*i_dst
    sub eax, 2
    jg .height_loop
    RET
%endmacro

;-----------------------------------------------------------------------------
; void pixel_avg_4x4( uint8_t *dst, int i_dst, uint8_t *src1, int i_src );
;-----------------------------------------------------------------------------
%macro AVGH 2
cglobal pixel_avg_%1x%2
    mov eax, %2
    ;cmp dword r6m, 32
    ;jne pixel_avg_weight_w%1 %+ SUFFIX
%if cpuflag(avx2) && %1 == 16 ; all AVX2 machines can do fast 16-byte unaligned loads
    jmp pixel_avg_w%1_avx2
%else
%if mmsize == 16 && %1 == 16
    test dword r4m, 15
    jz pixel_avg_w%1_sse2
%endif
    jmp pixel_avg_w%1_mmx2
%endif
%endmacro

;-----------------------------------------------------------------------------
; void pixel_avg_w4( uint8_t *dst, int i_dst, uint8_t *src, int i_src );
;-----------------------------------------------------------------------------

%macro AVG_FUNC 3
cglobal pixel_avg_w%1
    AVG_START
.height_loop:
%assign x 0
%rep (%1*SIZEOF_PIXEL+mmsize-1)/mmsize
    %2     m0, [t2+x] ; src
    %2     m1, [t2+x+SIZEOF_PIXEL*t3] ; src + i_src
%if HIGH_BIT_DEPTH
    pavgw  m0, [t0+x] ;(dst+src+1)>>1
    pavgw  m1, [t0+x+SIZEOF_PIXEL*t1]
%else ;!HIGH_BIT_DEPTH
    pavgb  m0, [t0+x]
    pavgb  m1, [t0+x+SIZEOF_PIXEL*t1]
%endif
    %3     [t0+x], m0 ;dst
    %3     [t0+x+SIZEOF_PIXEL*t1], m1 ;dst + i_dst
%assign x x+mmsize
%endrep
    AVG_END
%endmacro

%if HIGH_BIT_DEPTH

INIT_MMX mmx2
AVG_FUNC 4, movq, movq
AVGH 4, 4

AVG_FUNC 8, movq, movq
AVGH 8,  8

AVG_FUNC 16, movq, movq
AVGH 16, 16

INIT_XMM sse2
AVG_FUNC 4, movq, movq
AVGH  4, 4

AVG_FUNC 8, movdqu, movdqa
AVGH  8,  8

AVG_FUNC 16, movdqu, movdqa
AVGH  16, 16

%else ;!HIGH_BIT_DEPTH

INIT_MMX mmx2
AVG_FUNC 4, movd, movd
AVGH 4, 4

AVG_FUNC 8, movq, movq
AVGH 8,  8

AVG_FUNC 16, movq, movq
AVGH 16, 16

INIT_XMM sse2
AVG_FUNC 16, movdqu, movdqa
AVGH 16, 16
AVGH  8,  8

INIT_XMM ssse3
AVGH 16, 16
AVGH  8,  8

INIT_MMX ssse3
AVGH  4,  4

INIT_XMM avx2
AVG_FUNC 16, movdqu, movdqa
AVGH 16, 16
%endif ;HIGH_BIT_DEPTH