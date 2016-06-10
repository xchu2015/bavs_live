/*****************************************************************************
 * bavs_clearbit.c: 2d-VLC
 *****************************************************************************
*/

#include "bavs_clearbit.h"
#ifndef	_STRING_H
# include <string.h>
#endif	/* string.h  */

int cavs_init_bitstream( InputStream *p , unsigned char *rawstream, unsigned int i_len )
{
    p->f = rawstream;
    p->f_end = rawstream + i_len;
    p->uClearBits			= 0xffffffff;
    p->iBytePosition		= 0;
    p->iBufBytesNum		= 0;
    p->iClearBitsNum		= 0;
    p->iStuffBitsNum		= 0;
    p->iBitsCount			= 0;
    p->uPre3Bytes			= 0;

    return 0;
}

static int ClearNextByte(InputStream *p)
{
	int i,k,j;
	unsigned char temp[3];
	i = p->iBytePosition;
	k = p->iBufBytesNum - i;
	if(k < 3)
	{
		int last_len;

		for(j=0;j<k;j++) 
                 temp[j] = p->buf[i+j];
#if 0
             p->iBufBytesNum = fread(p->buf+k,1,SVA_STREAM_BUF_SIZE-k,p->f);
#else
			 if( p->f < p->f_end )
			 {
				last_len = p->f_end - p->f; 
				p->iBufBytesNum = MIN2(SVA_STREAM_BUF_SIZE - k, last_len);
				memcpy( p->buf+k, p->f, p->iBufBytesNum );
				p->f += p->iBufBytesNum;
			 }
			 else
				p->iBufBytesNum = 0;
#endif
		if(p->iBufBytesNum == 0)
		{
			if(k>0)
			{
                while(k>0)
                {
				    p->uPre3Bytes = ((p->uPre3Bytes<<8) | p->buf[i]) & 0x00ffffff;
				    if(p->uPre3Bytes < 4 && p->demulate_enable) //modified by X ZHENG, 20080515
				    {
					    p->uClearBits = (p->uClearBits << 6) | (p->buf[i] >> 2);
					    p->iClearBitsNum += 6;
					    //StatBitsPtr->emulate_bits += 2;	//rm52k_r2
				    }
				    else
				    {
					    p->uClearBits = (p->uClearBits << 8) | p->buf[i];
					    p->iClearBitsNum += 8;
				    }
				    p->iBytePosition++;
                    k--;
                    i++;
                }
				return 0;
			}
			else
			{
				return -1;//arrive at stream end
			}
		}
		else
		{
			for(j=0;j<k;j++) p->buf[j] = temp[j];
			p->iBufBytesNum += k;
			i = p->iBytePosition = 0;
		}
	}
	if(p->buf[i]==0 && p->buf[i+1]==0 && p->buf[i+2]==1)	return -2;// meet another start code
	p->uPre3Bytes = ((p->uPre3Bytes<<8) | p->buf[i]) & 0x00ffffff;
	if(p->uPre3Bytes < 4 && p->demulate_enable) //modified by XZHENG, 20080515
	{
		p->uClearBits = (p->uClearBits << 6) | (p->buf[i] >> 2);
		p->iClearBitsNum += 6;
		//StatBitsPtr->emulate_bits += 2;	//rm52k_r2
	}
	else
	{
		p->uClearBits = (p->uClearBits << 8) | p->buf[i];
		p->iClearBitsNum += 8;
	}
	p->iBytePosition++;
	
	return 0;
}

static int read_n_bit(InputStream *p,int n,int *v)
{
	int r;
	unsigned int t;
	while(n > p->iClearBitsNum)
	{
		r = ClearNextByte( p );
		if(r)
		{
			if(r==-1)
			{
				if(p->iBufBytesNum - p->iBytePosition > 0)
					break;
			}
			return r;
		}
	}
	t = p->uClearBits;
	r = 32 - p->iClearBitsNum;
	*v = (t << r) >> (32 - n);
	p->iClearBitsNum -= n;
	return 0;
}

static int read_n_bit8(InputStream *p, int *v)
{
	int r = ClearNextByte(p);
	if (r) return r;
	if(8 > p->iClearBitsNum){
		r = ClearNextByte(p);
		if(r) return r;
	};
	//t = p->uClearBits;
	//r = 32 - p->iClearBitsNum;
	//*v = (t << r) >> (24);
	p->iClearBitsNum -= 8;
	*v = (p->uClearBits) >> (p->iClearBitsNum);
	return 0;
}

int NextStartCode(InputStream *p)
{
	int i, m;
	unsigned char a = '\0', b = '\0';	// a b 0 1 2 3 4 ... M-3 M-2 M-1
	m=0;                // cjw 20060323 for linux envi
 
	while(1)
	{
		if(p->iBytePosition >= p->iBufBytesNum - 2)	//if all bytes in buffer has been searched
		{
			int last_len;

			m = p->iBufBytesNum - p->iBytePosition;
			if(m <0) 
			{
#if 0
				return -2;	// p->iBytePosition error
#else
				return 254; // current stream end // FIXIT
#endif
			}
			if(m==1)  b=p->buf[p->iBytePosition+1];
			if(m==2)
                    { 
                        b = p->buf[p->iBytePosition+1]; 
                        a = p->buf[p->iBytePosition];
                    }
#if 0
                    p->iBufBytesNum = fread(p->buf,1,SVA_STREAM_BUF_SIZE,p->f);
#else
			if( p->f < p->f_end )
			{
				last_len = p->f_end - p->f;
				p->iBufBytesNum = MIN2(SVA_STREAM_BUF_SIZE, last_len);
				memcpy( p->buf, p->f, p->iBufBytesNum );
				p->f += p->iBufBytesNum;
			}
			else
				p->iBufBytesNum = 0;

#endif
			p->iBytePosition = 0;
		}

		if(p->iBufBytesNum + m < 3) 
			return -1;  //arrive at stream end and start code is not found

		if(m==1 && b==0 && p->buf[0]==0 && p->buf[1]==1)
		{
			p->iBytePosition	= 2;
			p->iClearBitsNum	= 0;
			p->iStuffBitsNum	= 0;
			p->iBitsCount		+= 24;
			p->uPre3Bytes		= 1;
			return 0;
		}

		if(m==2 && b==0 && a==0 && p->buf[0]==1)
		{
			p->iBytePosition	= 1;
			p->iClearBitsNum	= 0;
			p->iStuffBitsNum	= 0;
			p->iBitsCount		+= 24;
			p->uPre3Bytes		= 1;
			return 0;
		}

		if(m==2 && b==0 && p->buf[0]==0 && p->buf[1]==1)
		{
			p->iBytePosition	= 2;
			p->iClearBitsNum	= 0;
			p->iStuffBitsNum	= 0;
			p->iBitsCount		+= 24;
			p->uPre3Bytes		= 1;
			return 0;
		}

		for(i = p->iBytePosition; i < p->iBufBytesNum - 2; i++)
		{
			if(p->buf[i]==0 && p->buf[i+1]==0 && p->buf[i+2]==1)
			{
				p->iBytePosition	= i+3;
				p->iClearBitsNum	= 0;
				p->iStuffBitsNum	= 0;
				p->iBitsCount		+= 24;
				p->uPre3Bytes		= 1;
				return 0;
			}
			p->iBitsCount += 8;
		}
		p->iBytePosition = i;
	}
}

void CheckType(InputStream* p, int startcode)
{
    startcode = startcode&0x000000ff;
    switch(startcode)
    {
        case 0xb0:
        case 0xb2:
        case 0xb5:
            p->demulate_enable = 0;
            break;  
        default:
            p->demulate_enable = 1;
            break;
    }
}

unsigned int cavs_get_one_nal (InputStream* p, unsigned char *buf, int *length, int buf_len)
{
    InputStream* pIRABS = p;
    int i,k, j = 0;
    unsigned int i_startcode;
    int i_tmp = 0;

    i = NextStartCode(pIRABS);
    i_tmp = i;
    if(i!=0)
    {
        i_tmp = i;
        if( i == -1 )
        {
            i_tmp = 254;
            printf("\n arrive at stream end and start code is not found! ");
        }
        if( i == -2 )
            printf("\n p->iBytePosition error! ");
        //exit(i);
    }
    buf[0] = 0;
    buf[1] = 0;
    buf[2] = 1;
    //*startcodepos = 3;
	i = read_n_bit8(pIRABS, &j); //read_n_bit( pIRABS, 8, &j );
    buf[3] = j;
    CheckType( pIRABS, buf[3] );  //X ZHENG, 20080515, for demulation

    /* get startcode */
    i_startcode = buf[0];
    i_startcode = (i_startcode << 8) | buf[1];
    i_startcode = (i_startcode << 8) | buf[2];
    i_startcode = (i_startcode << 8) | buf[3];
       
    if((unsigned char)buf[3]==SEQUENCE_END_CODE)
    {
    	*length = 4;
#if 0
              return -1;
#else
              return i_startcode;
#endif
    }
    k = 4;
    while(1)
    {
		i = read_n_bit8(pIRABS, &j);  //read_n_bit(pIRABS, 8, &j);
    	if(i<0) break;
		if (k >= buf_len) return -1; //bitstream error, exceed buffer limit
    	buf[k++] = /*(char)*/j;
    }
    if(pIRABS->iClearBitsNum>0)
    {
    	int shift = 8 - pIRABS->iClearBitsNum;
    	//i = read_n_bit(pIRABS,pIRABS->iClearBitsNum,&j);
		j = (pIRABS->uClearBits << shift)/* & 0x000000ff*/;
		pIRABS->iClearBitsNum = 0;

		if (j != 0){
			if (k >= buf_len) return -1; //bitstream error, exceed buffer limit
			buf[k++] = /*(char)*/(j /*<< shift*/);
		}
    }
    *length = k;
    
    if( i_tmp == 254 )
    {
        //printf(" length %d\n", k);
        return 0x000001fe;
    }
#if 0
       return k;
#else

      	return i_startcode;

#endif
}