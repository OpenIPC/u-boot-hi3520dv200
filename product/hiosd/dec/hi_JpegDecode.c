/******************************************************************************

  Copyright (C), 2001-2011, Hisilicon Tech. Co., Ltd.

 ******************************************************************************
  File Name     :
  Version       : Initial Draft
  Author        : Hisilicon multimedia software group
  Created       : 2005/08/25
  Last Modified :
  Description   :

  Function List :

  History       :
  1.Date        : 2005/08/25
    Author      : y43135
    Modification: Created file

******************************************************************************/

#include <common.h>
#include <command.h>
#include <malloc.h>
//#include <devices.h>
#include <version.h>
#include <net.h>
#include <asm/io.h>
#include <asm/arch/platform.h>
#include <asm/sizes.h>
#include <config.h>


#define MAKEWORD(a, b)      ((UINT16)((unsigned char)(a)) | ((UINT16)((unsigned char)(b)) << 8))

#define FUNC_OK  0
#define FUNC_MEMORY_ERROR  1
#define FUNC_FILE_ERROR   2
#define FUNC_FORMAT_ERROR  3

#define M_SOF0  0xc0
#define M_DHT   0xc4
#define M_EOI   0xd9
#define M_SOS   0xda
#define M_DQT   0xdb
#define M_DRI   0xdd
#define M_APP0  0xe0

#define W1  2841   /* 2048*sqrt(2)*cos(1*pi/16) */
#define W2  2676   /* 2048*sqrt(2)*cos(2*pi/16) */
#define W3  2408   /* 2048*sqrt(2)*cos(3*pi/16) */
#define W5  1609   /* 2048*sqrt(2)*cos(5*pi/16) */
#define W6  1108   /* 2048*sqrt(2)*cos(6*pi/16) */
#define W7  565    /* 2048*sqrt(2)*cos(7*pi/16) */

const int Zig_Zag[8][8]=
{
{0,1,5,6,14,15,27,28},
{2,4,7,13,16,26,29,42},
{3,8,12,17,25,30,41,43},
{9,11,18,24,31,40,44,53},
{10,19,23,32,39,45,52,54},
{20,22,33,38,46,51,55,60},
{21,34,37,47,50,56,59,61},
{35,36,48,49,57,58,62,63}
};
#define HI_OK 0
#define HI_ERROR 1
typedef unsigned int UINT32;
typedef unsigned char UINT8;
typedef unsigned short UINT16;


//UINT32 LoadJpegFile(void *pImg);
int InitTag(void);
void InitTable(void);
int Decode(void);
int DecodeMCUBlock(void);
int HufBlock(UINT8 dchufindex,UINT8 achufindex);
int DecodeElement(void);
void IQtIZzMCUComponent(int flag);
void IQtIZzBlock(int *s,int *d,int flag);
void GetYUV(int flag);
void StoreBuffer(void);
UINT8 ReadByte(void);
void Initialize_Fast_IDCT(void);
void Fast_IDCT(int * block);
void idctrow(int * blk);
void idctcol(int * blk);


UINT32 ImgWidth = 0,ImgHeight = 0;

int SampRate_Y_H,SampRate_Y_V,SampRate_U_H,SampRate_U_V,SampRate_V_H,SampRate_V_V;
int H_YtoU,V_YtoU,H_YtoV,V_YtoV,Y_in_MCU,U_in_MCU,V_in_MCU;
//UINT8 *lpJpegBuf=0x35000000,*lp,*lpPtr;
UINT8 *lpJpegBuf,*lp,*lpPtr;
int qt_table[3][64];
int comp_num;
UINT8 comp_index[3];
UINT8 YDcIndex,YAcIndex,UVDcIndex,UVAcIndex,HufTabIndex;
int *YQtTable,*UQtTable,*VQtTable;
UINT8 MyAnd[9]={0,1,3,7,0x0f,0x1f,0x3f,0x7f,0xff};
int code_pos_table[4][16],code_len_table[4][16];
UINT16 code_value_table[4][256];
UINT16 huf_max_value[4][16],huf_min_value[4][16];
int BitPos,CurByte,rrun,vvalue;
int MCUBuffer[640],BlockBuffer[64];
int QtZzMCUBuffer[640];
int ycoef,ucoef,vcoef,IntervalFlag,interval = 0;
int Y[256],U[256],V[256];
int sizei,sizej;
int restart;
int iclip[1024];
int LineBytes;
extern  unsigned char hilogo[];

UINT32 LoadJpegFile(void *pImg)
{
    UINT32 ImgSize;
    int funcret;
    lpJpegBuf = hilogo;

    InitTable();

    funcret = InitTag();
    if(funcret != FUNC_OK)    /* load the JPEEG file to DDR */
    {
    	printf("InitTag error\n");
        return HI_ERROR;
    }

#if 0
    LineBytes = ((ImgWidth * 16 + 31) >> 5) * 4;
#else
    if (1)
    {
        /* modified by x100808
         * hi35xx 16 bytes align, 8 pixel, stride use byte
         */
        int mod = ImgWidth%8;
        if (mod != 0)
        {
            LineBytes = (ImgWidth + (8-mod)) * 2;
        }
        else
        {
            LineBytes = ImgWidth * 2;
        }
    }
#endif

    ImgSize = LineBytes * ImgHeight;       
    lpPtr = pImg;   
    if((SampRate_Y_H == 0) || (SampRate_Y_V == 0))
    {
        return HI_ERROR;
    }

    printf("<<imgwidth=%d, imgheight=%d, linebytes=%d>>\n", ImgWidth, ImgHeight, LineBytes);

    funcret = Decode();
    if(funcret == FUNC_OK)      /* decode succeed */
    {
    	printf("decode success!!!!\n");
        return HI_OK;
    }
    else
    {
    	printf("JPEG Decode ERROR.\n");
        return HI_ERROR;
    }
}

int InitTag(void)
{
    UINT32 finish;
    UINT8 id;
    int llength,i,j,k,huftab1,huftab2,huftabindex,ccount;
    UINT8 hf_table_index,qt_table_index,comnum;
    UINT8 *lptemp;
    int  temp2;
    finish = HI_ERROR;
    lp = lpJpegBuf + 2;   /* jump over two bytes for SOI(0xFF，0xD8 Start of Image) */
    while(finish != HI_OK)
    {
        id = *(lp + 1);   /* take the low byte out. (high is in forward, low is in behind) */
        lp += 2;       /* jump over the byte which is taken out */
        switch(id)
        {
            case M_APP0:
                llength = MAKEWORD(*(lp + 1),*lp);  /* MAKEWORD transform to Motorlora and intel format */
                lp += llength;  /* Skip JFIF segment marker */
                break;
            case M_DQT:
                temp2 = 2;
                llength = MAKEWORD(*(lp + 1),*lp); /* (quantity table length) */
                qt_table_index = *(lp + 2) & 0x0f;/* quantity table information bit */
                lptemp = lp + 3;      
                if(llength < 80)
                {
                    for(i = 0;i < 64;i++)
                    {
                        qt_table[qt_table_index][i] = (int)(*(lptemp++));
                    }
                }
                else
                {
                    #if 1
                    while(temp2 < llength)
                    {
                 //       printf("qt_table_index  %d\n",qt_table_index);
                        for(i = 0;i < 64;i++)
                        {
                            qt_table[qt_table_index][i] = (int)(*(lptemp++));
                        }
                        qt_table_index = (*(lptemp++)) & 0x0f;
                        if(qt_table_index > 3)
                        {
                  //          printf("qt_table_index err %d\n",qt_table_index);
                            break;
                        }
                        temp2 += 65;
                    }
                    #else
       
                    for(i = 0;i < 64;i++)
                    {
                        qt_table[qt_table_index][i] = (int)(*(lptemp++));
                    }
                    qt_table_index = (*(lptemp++)) & 0x0f;
                    for(i = 0;i < 64;i++)
                    {
                        qt_table[qt_table_index][i] = (int)(*(lptemp++));
                    }
                    #endif
                }
  			    lp += llength; /* jump over the quantity table */
                break;
            case M_SOF0:      /* start of frame (baseline JPEG 0xFF,0xC0) */
	 		    llength = MAKEWORD(*(lp + 1),*lp);         /* length (high byte, low byte), 8+components*3 */
	 		    ImgHeight = MAKEWORD(*(lp + 4),*(lp + 3));/* image height(high byte, low byte), if not support DNL must be >0 */
	 		    ImgWidth = MAKEWORD(*(lp + 6),*(lp + 5)); /* image width(high byte, low byte), if not support DNL must be >0 */
                comp_num = *(lp + 7);                   /* components number(1 byte), ashy picture is 1, YCbCr/YIQ colour picture is 3, CMYK colour picture is 4 */
	            if((comp_num != 1) && (comp_num != 3))
	            {
	                return FUNC_FORMAT_ERROR;
	            }
	            if(comp_num == 3)
	            {
    				comp_index[0] = *(lp + 8);          /* component id (1 = Y, 2 = Cb, 3 = Cr, 4 = I, 5 = Q) */
    	  			SampRate_Y_H = (*(lp + 9)) >> 4;      /* coefficient of horizontal sampling  */
    				SampRate_Y_V = (*(lp + 9)) & 0x0f;    /* coefficient of vertical sampling */
    	  			YQtTable = (int *)qt_table[*(lp + 10)]; /* quantity table address */
                                  /*  ShowMessage(IntToStr(YQtTable^)); */

    				comp_index[1] = *(lp  + 11);         /* component id */
    				SampRate_U_H = (*(lp + 12)) >> 4;     /* coefficient of horizontal sampling  */
    	  			SampRate_U_V = (*(lp + 12)) & 0x0f;   /* coefficient of vertical sampling */
    	  			UQtTable = (int *)qt_table[*(lp + 13)];  /* quantity table address */

    	  			comp_index[2] = *(lp + 14);         /* component id */
    	  			SampRate_V_H = (*(lp  + 15)) >> 4;      /* coefficient of horizontal sampling  */
    	  			SampRate_V_V = (*(lp + 15)) & 0x0f;   /* coefficient of vertical sampling */
    				VQtTable = (int *)qt_table[*(lp + 16)];    /* quantity table address */
	            }
	            else
	            {
    	  			comp_index[0] = *(lp + 8);
    				SampRate_Y_H = (*(lp + 9)) >> 4;
    	  			SampRate_Y_V = (*(lp + 9)) & 0x0f;
    	  			YQtTable = (int *)qt_table[*(lp + 10)];  /* quantity table of ashy picture are the same */

    				comp_index[1] = *(lp + 8);
    	  			SampRate_U_H = 1;
    	  			SampRate_U_V = 1;
    	  			UQtTable = (int *)qt_table[*(lp + 10)];

    				comp_index[2] = *(lp + 8);
    				SampRate_V_H = 1;
    	  			SampRate_V_V = 1;
    	  			VQtTable = (int *)qt_table[*(lp + 10)];
	            }
	            lp += llength;
                break;
            case M_DHT:  /* define Huffman Table(0xFF,0xC4) */
    			llength = MAKEWORD(*(lp + 1),*lp);       /* length (high byte, low byte) */
				
				/* 
				   HSCP201407070001: one JPEG of custom decode error 
				   reason analyse: this is not rationality to consider only one huffman table 
				   when the length of huffman syntax is less than 0xd0. This confine is not 
				   appear in the protocol of JPEG. 
				   resolution: delete the "if" embranchment, into the "else" embranchment.
				*/
    			if (0)//(llength < 0xd0)                /* Huffman Table (1 byte) */
                {
    				huftab1 = (int)(*(lp + 2)) >> 4;     /* huftab1=0,1(HT type,0 = DC 1 = AC) */
    		 		huftab2 = (int)(*(lp + 2)) & 0x0f;   /* huftab2=0,1(HT number,0 = Y  1 = UV) */
    				huftabindex = huftab1 * 2 + huftab2;           /* 0 = YDC 1 = UVDC 2 = YAC 3 = UVAC */
    		 		lptemp = lp + 3;
    				for(i = 0;i < 16;i++)                     /* 16 bytes: length is 1..16 */
                    {
    					code_len_table[huftabindex][i] = (int)(*(lptemp++));
    					                                
                    }
    				j = 0;
    				for(i = 0;i < 16;i++)             
                    {
    					if(code_len_table[huftabindex][i] != 0)
                        {
    						k = 0;
    						while(k < code_len_table[huftabindex][i])
                            {
    							code_value_table[huftabindex][k + j] = (int)(*(lptemp++));
    							k++;
    						}
    						j += k;
    					}
    			    }
    				i = 0;
    				while(code_len_table[huftabindex][i] == 0)  /* throw off the zero number */
    		 		{
    		 			i++;
    		 	    }
    				for(j = 0;j < i;j++)
                    {
    					huf_min_value[huftabindex][j] = 0;            /* the min code which the code length is j */
    					huf_max_value[huftabindex][j] = 0;            /* the max code which the code length is j */
    				}
    				huf_min_value[huftabindex][i] = 0;
    				huf_max_value[huftabindex][i] = code_len_table[huftabindex][i] - 1;
    				for(j = i + 1;j < 16;j++)
                    {                                              /* the min code and the max code which the code length is j */
    					huf_min_value[huftabindex][j] = (huf_max_value[huftabindex][j - 1] + 1) << 1;
    					huf_max_value[huftabindex][j] = huf_min_value[huftabindex][j] + code_len_table[huftabindex][j] - 1;
    				}
    				code_pos_table[huftabindex][0] = 0; /*start of code  */
    				for(j = 1;j < 16;j++)
    				{
    		  			code_pos_table[huftabindex][j] = code_len_table[huftabindex][j - 1] + code_pos_table[huftabindex][j - 1];
    		  		}
    		  		lp += llength;
                }
    			else
    			{
    	 			hf_table_index = *(lp + 2);
    				lp += 2;
    				while(hf_table_index != 0xff)
                    {
    					huftab1 = (int)hf_table_index >> 4;     /* huftab1=0,1 */
    			 		huftab2 = (int)hf_table_index & 0x0f;   /* huftab2=0,1 */
    					huftabindex = huftab1 * 2 + huftab2;
    					lptemp = lp;
    					lptemp++;
    					ccount = 0;
    					for(i = 0;i < 16;i++)
                        {
                            code_len_table[huftabindex][i] = (int)(*(lptemp++));
    						ccount = ccount + code_len_table[huftabindex][i];
    					}
    					ccount = ccount + 17;
    					j = 0;
    					for(i = 0;i < 16;i++)
    					{
    						if(code_len_table[huftabindex][i] != 0)
                            {
    							k = 0;
    							while(k < code_len_table[huftabindex][i])
    							{
    								code_value_table[huftabindex][k + j] = (int)(*(lptemp++));
    								k++;
    							}
    							j = j + k;
    						}
    				    }
    					i = 0;
    					while(code_len_table[huftabindex][i] == 0)
    					{
    						i++;
    					}
    					for(j = 0;j < i;j++)
                        {
    						huf_min_value[huftabindex][j] = 0;
    						huf_max_value[huftabindex][j] = 0;
    					}
    					huf_min_value[huftabindex][i] = 0;
    					huf_max_value[huftabindex][i] = code_len_table[huftabindex][i] - 1;
    					for(j = i + 1;j < 16;j++)
                        {
    						huf_min_value[huftabindex][j] = (huf_max_value[huftabindex][j - 1] + 1) << 1;
    						huf_max_value[huftabindex][j] = huf_min_value[huftabindex][j] + code_len_table[huftabindex][j] - 1;
    					}
    					code_pos_table[huftabindex][0] = 0;
    					for(j = 1;j < 16;j++)
    					{
    						code_pos_table[huftabindex][j] = code_len_table[huftabindex][j - 1] + code_pos_table[huftabindex][j - 1];
    					}
    					lp += ccount;
    					hf_table_index = *lp;
    				}
    			}
                break;
            case M_DRI:
    			llength = MAKEWORD(*(lp + 1),*lp);
    			restart = MAKEWORD(*(lp + 3),*(lp + 2));
    			lp += llength;
                break;
            case M_SOS:    /* scan the beginning of row 0xff, 0xda (SOS) */
    		    llength = MAKEWORD(*(lp + 1),*lp);/* length (high byte, low byte) */
    			comnum = *(lp + 2);               
		       	if(comnum != comp_num)       
                {
			        return FUNC_FORMAT_ERROR;
                }
                lptemp = lp + 3;
    			for(i = 0;i < comp_num;i++)
                {                  
    				if(*lptemp == comp_index[0])
                    {   /* 1 byte :Component id */
    					YDcIndex = (*(lptemp + 1)) >> 4;   /* the Huffman table which is used by Y */
    					YAcIndex = ((*(lptemp + 1)) & 0x0f) + 2;
    				}
    				else
                    {
    					UVDcIndex = (*(lptemp + 1)) >> 4;   /* U,V */
    					UVAcIndex = ((*(lptemp + 1)) & 0x0f) + 2;
    				}
                    lptemp += 2;
    			}
    			lp += llength;
    			finish = HI_OK;
                break;
            case M_EOI:
                return FUNC_FORMAT_ERROR;
                break;
            default:
     			if((id & 0xf0) != 0xd0)
                {
    				llength = MAKEWORD(*(lp + 1),*lp);
    	 			lp += llength;
    			}
    			else
    			{
    			    lp += 2;
    			}
                break;
        }
    }
    return FUNC_OK;
}

void InitTable(void)
{
    int i,j;
	sizei = 0;
    sizej = 0;
	ImgWidth = 0;
    ImgHeight = 0;
	rrun = 0;
    vvalue = 0;
	BitPos = 0;
	CurByte = 0;
	IntervalFlag = HI_ERROR;
	restart = 0;
	for(i = 0;i < 3;i++)
	{
        for(j = 0;j < 64;j++)
        {
		    qt_table[i][j] = 0;           /* quantity table */
	    }
	}
	comp_num = 0;
	HufTabIndex = 0;
	for(i = 0;i < 3;i++)
	{
		comp_index[i] = 0;
	}
	for(i = 0;i < 4;i++)
	{
	    for(j = 0;j < 16;j++)
        {
	    	code_len_table[i][j] = 0;
	    	code_pos_table[i][j] = 0;
	    	huf_max_value[i][j] = 0;
	    	huf_min_value[i][j] = 0;
	    }
	}
	for(i = 0;i < 4;i++)
	{
	    for(j = 0;j < 256;j++)
	    {
	        code_value_table[i][j] = 0;
        }
    }
	for(i = 0;i< 640;i++)
    {
		MCUBuffer[i] = 0;
		QtZzMCUBuffer[i] = 0;
	}
	for(i = 0;i < 64;i++)
    {
		Y[i] = 0;
		U[i] = 0;
		V[i] = 0;
		BlockBuffer[i] = 0;
	}
	ycoef = 0;
	ucoef = 0;
	vcoef = 0;
}

/*************************************************************************
transfer order : Initialize_Fast_IDCT() :initialization
          DecodeMCUBlock()       Huffman Decode
          IQtIZzMCUComponent()   opposite quantitative change、opposite DCT
          GetYUV()               Get Y U V
          StoreBuffer()          YUV to RGB
*************************************************************************/
int Decode(void)
{
    int funcret;

    Y_in_MCU = SampRate_Y_H * SampRate_Y_V;     /* YDU YDU YDU YDU */
	U_in_MCU = SampRate_U_H * SampRate_U_V;     /* cRDU */
	V_in_MCU = SampRate_V_H * SampRate_V_V;     /* cBDU */
	H_YtoU = SampRate_Y_H / SampRate_U_H;
	V_YtoU = SampRate_Y_V / SampRate_U_V;
	H_YtoV = SampRate_Y_H / SampRate_V_H;
	V_YtoV = SampRate_Y_V / SampRate_V_V;
	Initialize_Fast_IDCT();
	funcret = DecodeMCUBlock();
	//printf("funcret_decodemcublock = %x\n",funcret);
	while(funcret == FUNC_OK)
    {                                          /* After Call DecodeMCUBUBlock() */

		interval++;                             /* The Digital has been Huffman Decoded and */
		if((restart != 0) && ((interval % restart) == 0))   /* be stored in MCUBuffer(YDU,YDU,YDU,YDU */
		{
		    IntervalFlag = HI_OK;                          /* UDU,VDU) Every DU := 8*8 */
		}
		else
		{
			IntervalFlag = HI_ERROR;
		}
		IQtIZzMCUComponent(0);   /* opposite quantitative change and IDCT The Data in QtZzMCUBuffer */
		IQtIZzMCUComponent(1);
		IQtIZzMCUComponent(2);
		GetYUV(0);                    /* Y cR cB */
		GetYUV(1);
		GetYUV(2);

		StoreBuffer();               /* To RGB */

		sizej = sizej + (UINT32)(SampRate_Y_H << 3);
		if(sizej >= ImgWidth)
        {
			sizej = 0;
			sizei = sizei + (UINT32)(SampRate_Y_V << 3);
		}
		if ((sizej == 0) && (sizei >= ImgHeight))
		{
			break;
        }

        funcret = DecodeMCUBlock();

    }

	return funcret;
}

/**********************************************************************
   in: QtZzMCUBuffer    out: Y[] U[] V[]
**********************************************************************/
void GetYUV(int flag)
{
    int H,VV,i,j,k,hk;
    int temp;
    int *buf,*tempbuf,*pQtZzMCU;
    switch(flag)
    {
        case 0:                             /* lum weight */
    		H = SampRate_Y_H;
    		VV = SampRate_Y_V;
    		buf = Y;
    		pQtZzMCU = QtZzMCUBuffer;
            break;
        case 1:                             /* red weight */
    		H = SampRate_U_H;
    		VV = SampRate_U_V;
    		buf = U;
    		pQtZzMCU = QtZzMCUBuffer;
            pQtZzMCU = pQtZzMCU + (Y_in_MCU << 6);
            break;
        case 2:                            /* blue weight */
    		H = SampRate_V_H;
    		VV = SampRate_V_V;
    		buf = V;
    		pQtZzMCU = QtZzMCUBuffer;
            pQtZzMCU = pQtZzMCU + ((Y_in_MCU + U_in_MCU) << 6);
            break;
        default:
            H = 0;
            VV = 0;
            buf = NULL;
            pQtZzMCU = NULL;
            break;
    }
    for(i = 0;i < VV;i++)
    {
        for(j = 0;j < H;j++)
        {
            for(k = 0;k < 8;k++)
            {
                for(hk = 0;hk < 8;hk++)
                {
        			temp = ((((i << 3) + k) * SampRate_Y_H) << 3) + (j << 3) + hk;
                    tempbuf = buf;
                    tempbuf = tempbuf + temp;
                   	*tempbuf = (int)(*(pQtZzMCU++));

                }
            }
        }
    }
}

/********************************************************************************
    storage in RGB format:lpbmp (BGR),(BGR) ......in: Y[] U[] V[] out:lpPtr
*********************************************************************************/
void StoreBuffer(void)
{
    int i,j;
    UINT8 *lprgb;
    UINT8 R,G,B;
    int yy,uu,vv,rr,gg,bb;
    int TempSamp1,TempSamp2;
    TempSamp1 = SampRate_Y_V << 3;
    TempSamp2 = SampRate_Y_H << 3;
    int *pY;
    int *pU;
    int *pV;

    for(i = 0;i < TempSamp1;i++)
   	{                                  /* sizei:row,   sizej: line */
		if((sizei + i) < ImgHeight)
        {
			/*lpbmp = lpPtr + (UINT32)((ImgHeight - sizei - (UINT32)(i) - 1) * LineBytes + sizej * 3);*/
			lprgb = lpPtr + ( (sizei + i) * LineBytes + (sizej << 1) );

			pY=Y+i*TempSamp2;// (i << 3) * SampRate_Y_H;
			pU=U+(i / V_YtoU)*TempSamp2;// << 3) * SampRate_Y_H;
			pV=V+(i / V_YtoV)*TempSamp2;// << 3) * SampRate_Y_H;

			for(j = 0;j < TempSamp2;j++)
            		{
				if((sizej + j) < ImgWidth)
                		{
					yy = *pY++;

					uu = pU[ j/H_YtoU ];  

					vv = pV[ j/H_YtoV];

					rr = ((yy << 8) + 18 * uu + 367 * vv) >> 8;
					gg = ((yy << 8) - 159 * uu - 220 * vv) >> 8;
					bb = ((yy << 8) + 411 * uu - 29 * vv) >> 8;
					R = (UINT8)(rr);
					G = (UINT8)(gg);
					B = (UINT8)(bb);
			if((rr && 0xffffff00) != 0)
			{
                        if (rr > 255)
                        {
                            R = 255;
                        }
                        else
                        {
                            if(rr < 0)
                            {
                                R = 0;
                            }
                        }
			}

			if((gg && 0xffffff00) != 0)
			{
                        if(gg > 255)
                        {
                            G = 255;
                        }
                        else
                        {
                            if(gg < 0)
                            {
                                G = 0;
                            }
                        }
                    	}

			if((bb && 0xffffff00) != 0)
                    	{
                        if(bb > 255)
                        {
                            B = 255;
                        }
                        else
                        {
                            if(bb < 0)
                            {
                                B = 0;
                            }
                        }
			}

			*lprgb++ = (UINT8)(B >> 3) + (UINT8)((G << 2) & 0xe0);
					*lprgb++ = (UINT8)(G >> 6) + (UINT8)((R >> 1) & 0x7c);
			}
			else
			{
			    break;
			}
		}
		}
		else
		{
		    break;
		}
	}
}

/******************************************************************************
  Huffman Decode   MCU  out: MCUBuffer  in: Blockbuffer[  ]
*******************************************************************************/
int DecodeMCUBlock(void)
{
    int *lpMCUBuffer;
    int i,j;
    int funcret=0;
    int tempX;
	if(IntervalFlag == HI_OK)
    {
		lp += 2;
		ycoef = 0;   
        ucoef = 0;
        vcoef = 0;
		BitPos = 0;
		CurByte = 0;
	}
	//printf("comp_num = %x\n", comp_num);
	switch(comp_num)
    {                             /* comp_num type of image（colour image, gray image）*/

	    case 3:   /* colour image */
    		lpMCUBuffer = MCUBuffer;
    		tempX = SampRate_Y_H * SampRate_Y_V;
    		for(i = 0;i < tempX;i++)   /* Y */
    		{
    			funcret = HufBlock(YDcIndex,YAcIndex);   /* decode 4 * (8*8) */
    			if(funcret != FUNC_OK)
                {
                	printf("funcret1 = %x\n", funcret);
    				return funcret;
                }
                BlockBuffer[0] = BlockBuffer[0] + ycoef;  
    			ycoef = BlockBuffer[0];
    			for(j = 0;j < 64;j++)
                {
    				*lpMCUBuffer++ = BlockBuffer[j];
                }
    		}
    		tempX = SampRate_U_H * SampRate_U_V;
    		for(i = 0;i < tempX;i++)    /* U */
    		{
    			funcret = HufBlock(UVDcIndex,UVAcIndex);
    			if(funcret != FUNC_OK)
                {
                    printf("funcret2 = %x\n", funcret);
                    return funcret;
                }
    			BlockBuffer[0] = BlockBuffer[0] + ucoef;
    			ucoef = BlockBuffer[0];
    			for(j = 0;j < 64;j++)
                {
    				*lpMCUBuffer++ = BlockBuffer[j];
                }
    		}
    		tempX = SampRate_V_H * SampRate_V_V;
    		for(i = 0;i < tempX;i++)  /* V */
    		{
    			funcret = HufBlock(UVDcIndex,UVAcIndex);
    			if(funcret != FUNC_OK)
                {

    				printf("funcret3 = %x\n", funcret);
    				return funcret;
                }
    			BlockBuffer[0] = BlockBuffer[0] + vcoef;
    			vcoef = BlockBuffer[0];
    			for(j = 0;j < 64;j++)
                {
    				*lpMCUBuffer++ = BlockBuffer[j];
                }
    		}
            break;
        case 1:   /* Gray Picture */
    		lpMCUBuffer = MCUBuffer;
    		funcret = HufBlock(YDcIndex,YAcIndex);
    		if(funcret != FUNC_OK)
            {
    			return funcret;
            }
    		BlockBuffer[0] = BlockBuffer[0] + ycoef;
    		ycoef = BlockBuffer[0];
    		for(j = 0;j < 64;j++)
    		{
    			*lpMCUBuffer++ = BlockBuffer[j];
            }
    		for(i = 0;i < 128;i++)
            {
    			*lpMCUBuffer++ = 0;
            }
            break;
        default:
    		// printf("comp_num1 = %x\n", comp_num);
    		return FUNC_FORMAT_ERROR;
	}
	return FUNC_OK;
}

/*****************************************************************
   Huffman Decode （8*8） DU   out: Blockbuffer[ ] in:vvalue
******************************************************************/
int HufBlock(UINT8 dchufindex,UINT8 achufindex)
{
    int count,i;
    int funcret;

    count = 0;
	/* dc */
	HufTabIndex = dchufindex;
	funcret = DecodeElement();  /* Read Byte Dc */
	if(funcret != FUNC_OK)
    {
		return funcret;
    }
	BlockBuffer[count++] = vvalue;/* direct current coefficient */

	/* ac */
	HufTabIndex = achufindex;
	while(count < 64)
    {         /* 63 Bytes AC */
		funcret = DecodeElement();
		if(funcret != FUNC_OK)
        {
			return funcret;
        }
		if((rrun == 0) && (vvalue == 0))
        {
			for(i = count;i < 64;i++)
			{
				BlockBuffer[i] = 0;
			}
			count = 64;
		}
		else
        {
			for(i = 0;i < rrun;i++)    
            {
				BlockBuffer[count++] = 0;
            }
			BlockBuffer[count++] = vvalue;
		}
	}
    return FUNC_OK;
}

/**************************************************************************
   decode Huffman out:vvalue  in: File ReadByte
***************************************************************************/
int DecodeElement(void)
{
	int thiscode,tempcode;
	UINT16 temp,valueex;
	int codelen;
	UINT8 hufexbyte,runsize,tempsize,sign;
	UINT8 newbyte,lastbyte;

	if(BitPos >= 1)             /* BitPos: current bit position*/
    {
		BitPos--;
		thiscode = (UINT8)CurByte >> BitPos; /* take out one bit */
		CurByte = CurByte & MyAnd[BitPos]; /* clean the take out bit */
	}
	else                                        /* use up one byte which is taken out */
    {                                          /* take out a new one */
		lastbyte = ReadByte(); /* read one byte */
		BitPos--;                     /* and[]:=0x0,0x1,0x3,0x7,0xf,0x1f,0x2f,0x3f,0x4f */
		newbyte = CurByte & MyAnd[BitPos];  //
		thiscode = lastbyte >> 7;
		CurByte = newbyte;
	}
	codelen = 1;
    /* matching witch the code of Huffman table, until find */
	while((thiscode < huf_min_value[HufTabIndex][codelen - 1]) ||
		  (code_len_table[HufTabIndex][codelen - 1] == 0) ||
		  (thiscode > huf_max_value[HufTabIndex][codelen - 1]))
	{
		if(BitPos >= 1)      
		{
		   	BitPos--;
			tempcode = (UINT8)CurByte >> BitPos;
			CurByte = CurByte & MyAnd[BitPos];
		}
		else
        {
			lastbyte = ReadByte();
			BitPos--;
			newbyte = CurByte & MyAnd[BitPos];
			tempcode = (UINT8)lastbyte >> 7;
			CurByte = newbyte;
		}
		thiscode = (thiscode << 1) + tempcode;
		codelen++;
		if(codelen > 16)
        {
			return FUNC_FORMAT_ERROR;
        }
	}  //while
	temp = thiscode - huf_min_value[HufTabIndex][codelen - 1] + code_pos_table[HufTabIndex][codelen - 1];
	hufexbyte = (UINT8)code_value_table[HufTabIndex][temp];
	rrun = (int)(hufexbyte >> 4);  /* the four high bit in byte is the zero number */
	runsize = hufexbyte & 0x0f;    /* the four low bit in byte is length of code *//* 后四位为后面字的尺寸 */
	if(runsize == 0)
    {
		vvalue = 0;
		return FUNC_OK;
    }
	tempsize = runsize;
	if(BitPos >= runsize)
    {
		BitPos = BitPos - runsize;
		valueex = (UINT8)CurByte >> BitPos;
		CurByte = CurByte & MyAnd[BitPos];
	}
	else
    {
		valueex = CurByte;
		tempsize = tempsize - BitPos;
		while(tempsize > 8)
        {
			lastbyte = ReadByte();
			valueex = (valueex << 8) + (UINT8)lastbyte;
			tempsize = tempsize - 8;
		}  //while
		lastbyte = ReadByte();
		BitPos = BitPos - tempsize;
		valueex = (valueex << tempsize) + (lastbyte >> BitPos);
		CurByte = lastbyte & MyAnd[BitPos];
	}  //else
	sign = valueex >> (runsize - 1);
	if(sign != 0)
	{
		vvalue = valueex;             
	}
	else
    {
		valueex = valueex ^ 0xffff;
		temp = 0xffff << runsize;
		vvalue = -(int)(valueex ^ temp);
    }
	return FUNC_OK;
}

/************************************************************************************
   each groupware in MCU opposite quantitative change  in:MCUBuffer   out:QtZzMCUBuffer
*************************************************************************************/
void IQtIZzMCUComponent(int flag)
{
    int H,VV,i,j;
    int *pQtZzMCUBuffer,*tempbuf1;
    int *pMCUBuffer,*tempbuf2;

	switch(flag)
	{
	    case 0:
    		H = SampRate_Y_H;
    		VV = SampRate_Y_V;
    		pMCUBuffer = MCUBuffer;  /* Huffman Decoded */
    		pQtZzMCUBuffer = QtZzMCUBuffer;
            break;
    	case 1:
    		H = SampRate_U_H;
    		VV = SampRate_U_V;
    		pMCUBuffer = MCUBuffer;
            pMCUBuffer += Y_in_MCU << 6;
    		pQtZzMCUBuffer = QtZzMCUBuffer;
            pQtZzMCUBuffer += Y_in_MCU << 6;
            break;
    	case 2:
    		H = SampRate_V_H;
    		VV = SampRate_V_V;
    		pMCUBuffer = MCUBuffer;
            pMCUBuffer += (Y_in_MCU + U_in_MCU) << 6;
    		pQtZzMCUBuffer = QtZzMCUBuffer;
            pQtZzMCUBuffer += (Y_in_MCU + U_in_MCU) << 6;
            break;
        default:
            H = 0;
            pQtZzMCUBuffer = NULL;
            pMCUBuffer = NULL;
            VV = 0;
            break;
    }
	for(i = 0;i < VV;i++)
	{
		for(j = 0;j < H;j++)
        {
            tempbuf2 = pMCUBuffer;
            tempbuf2 += (i * H + j) << 6;
            tempbuf1 = pQtZzMCUBuffer;
            tempbuf1 += (i * H + j) << 6;
		    IQtIZzBlock(tempbuf2,tempbuf1,flag);      /* 8*8DU */
        }
    }
}

            
/**********************************************************************************
opposite quantitative change 8*8 DU
***********************************************************************************/
void IQtIZzBlock(int *s,int *d,int flag)
{
    int i,j,tag;
    int *pQt,*temp1,*temp3;
    int buffer2[8][8];
    int *temp2;
    int offset;

	switch(flag)
	{
    	case 0:                /* lum */
    		pQt = YQtTable;      /* quantity table address */
        /* ShowMessage(IntTostr(YQtTable^)); */
    		offset = 128;
            break;
    	case 1:                /* red */
    		pQt = UQtTable;
    		offset = 0;
            break;
    	case 2:               /* blue */
    		pQt = VQtTable;
            offset = 0;
            break;
        default:
           pQt = NULL;
           offset = 0;
           break;
	}

	for(i = 0;i < 8;i++)
	{
		for(j = 0;j < 8;j++)
		{
            tag = Zig_Zag[i][j];
            temp1 = s;
            temp1 = temp1 + tag;
            temp3 = pQt;
            temp3 = temp3 + tag;
			buffer2[i][j] = (int)((*temp1) * (*temp3));
		}
    }
	Fast_IDCT(&buffer2[0][0]);    /* opposite DCT */
	for(i = 0;i < 8;i++)
	{
		for(j = 0;j < 8;j++)
        {
            temp2 = d;
            temp2 = temp2 + (i * 8 +j);
			*temp2 = buffer2[i][j] + offset;
        }
    }
}

void Fast_IDCT(int *block)
{
	int i;
	for(i = 0;i < 8;i++)
	{
    	idctrow(block + 8 * i);
    }
    for(i = 0;i < 8;i++)
    {
    	idctcol(block + i);
	}
}

UINT8 ReadByte(void)
{
	UINT8 i;

	i = *lp; 
    lp = lp + 1;
	if(i == 0xff)
	{
		lp = lp + 1;
	}
    BitPos = 8;
	CurByte = i;
	return i;
}

void Initialize_Fast_IDCT(void)
{
	int i;

	for(i = -512;i < 512;i++)
    {
        if(i < -256)
        {
            iclip[512 + i] = -256;
        }
        if(i > 255)
        {
            iclip[512 + i] = 255;
        }
        if(( i >= -256) && (i <= 255))
        {
            iclip[512 + i] = i;
        }
    }
}

void idctrow(int *blk)
{
	int x0, x1, x2, x3, x4, x5, x6, x7, x8;

	//intcut
    x1 = blk[4] << 11;
    x2 = blk[6];
    x3 = blk[2];
    x4 = blk[1];
    x5 = blk[7];
    x6 = blk[5];
    x7 = blk[3];
    if ((x1 || x2 || x3 || x4 || x5 || x6 || x7) == 0)
    {
        blk[1] = blk[0] << 3;
        blk[2] = blk[0] << 3;
        blk[3] = blk[0] << 3;
        blk[4] = blk[0] << 3;
        blk[5] = blk[0] << 3;
        blk[6] = blk[0] << 3;
        blk[7] = blk[0] << 3;
        blk[0] = blk[0] << 3;
        return;
    }
	x0 = (blk[0] << 11) + 128; /* for proper rounding in the fourth stage */
	/* first stage */
	x8 = W7 * (x4 + x5);
	x4 = x8 + (W1 - W7) * x4;
	x5 = x8 - (W1 + W7) * x5;
	x8 = W3 * (x6 + x7);
	x6 = x8 - (W3 - W5) * x6;
	x7 = x8 - (W3 + W5) * x7;
	/* second stage */
	x8 = x0 + x1;
	x0 = x0 - x1;
	x1 = W6 * (x3 + x2);
	x2 = x1 - (W2 + W6) * x2;
	x3 = x1 + (W2 - W6) * x3;
	x1 = x4 + x6;
	x4 = x4 - x6;
	x6 = x5 + x7;
	x5 = x5 - x7;
	/* third stage */
	x7 = x8 + x3;
	x8 = x8 - x3;
	x3 = x0 + x2;
	x0 = x0 - x2;

        x2 = (181 * (x4 + x5) + 128) >> 8;
        x4 = (181 * (x4 - x5) + 128) >> 8;
/* fourth stage */

        blk[0] = (x7 + x1) >> 8;
	    blk[1] = (x3 + x2) >> 8;
	    blk[2] = (x0 + x4) >> 8;
        blk[3] = (x8 + x6) >> 8;
        blk[4] = (x8 - x6) >> 8;
        blk[5] = (x0 - x4) >> 8;
        blk[6] = (x3 - x2) >> 8;
        blk[7] = (x7 - x1) >> 8;
}

void idctcol(int * blk)
{
    int x0, x1, x2, x3, x4, x5, x6, x7, x8;
    int Temp;

	/* intcut */

    x1 = blk[8 * 4] << 8;
    x2 = blk[8 * 6];
    x3 = blk[8 * 2];
    x4 = blk[8 * 1];
    x5 = blk[8 * 7];
    x6 = blk[8 * 5];
    x7 = blk[8 * 3];
	if ((x1 | x2 | x3 | x4 | x5 | x6 | x7) == 0)
	{
		Temp = (blk[8 * 0] + 32) >> 6;
  /*      if((blk[8 * 0] + 32) < 0)
        {
            Temp = ((blk[8 * 0] + 32) >> 6) | ((!(int)(0)) << (32 - 6));
        }
        else
        {
            Temp = (blk[8 * 0] + 32) >> 6;
        }*/
        blk[8 * 1] = iclip[512 + Temp];
        blk[8 * 2] = iclip[512 + Temp];
        blk[8 * 3] = iclip[512 + Temp];
        blk[8 * 4] = iclip[512 + Temp];
        blk[8 * 5] = iclip[512 + Temp];
        blk[8 * 6] = iclip[512 + Temp];
        blk[8 * 7] = iclip[512 + Temp];
	    blk[8 * 0] = iclip[512 + Temp];
        return;
	}
	x0 = (blk[8 * 0] << 8) + 8192;
	/* first stage */
	x8 = W7 * (x4 + x5) + 4;

        x4 = (x8 + (W1 - W7) * x4) >> 3;
        x5 = (x8 - (W1 + W7) * x5) >> 3;
    	x8 = W3 * (x6 + x7) + 4;

        x6 = (x8 -(W3 - W5) * x6) >> 3;
        x7 = (x8 - (W3 + W5) * x7) >> 3;
	/* second stage */
	x8 = x0 + x1;
	x0 = x0 - x1;
	x1 = W6 * (x3 + x2) + 4;

        x2 = (x1 - (W2 + W6) * x2) >> 3;
        x3 = (x1 + (W2 - W6) * x3) >> 3;

	x1 = x4 + x6;
	x4 = x4 - x6;
	x6 = x5 + x7;
	x5 = x5 - x7;
	/* third stage */
	x7 = x8 + x3;
	x8 = x8 - x3;
	x3 = x0 + x2;
	x0 = x0 - x2;

        x2 = (181 * (x4 + x5) + 128) >> 8;
        x4 = (181 * (x4 - x5) + 128) >> 8;
	/* fourth stage */
        Temp = (x7 + x1) >> 14;

    blk[0] = iclip[512 + Temp];
        Temp = (x3 + x2) >> 14;

    blk[8] = iclip[512 + Temp];
        Temp = (x0 + x4) >> 14;

    blk[16] = iclip[512 + Temp];
        Temp = (x8 + x6) >> 14;

    blk[24] = iclip[512 + Temp];
        Temp = (x8 - x6) >> 14;

    blk[32] = iclip[512 + Temp];
        Temp = (x0 - x4) >> 14;

    blk[40] = iclip[512 + Temp];
        Temp = (x3 - x2) >> 14;

    blk[48] = iclip[512 + Temp];
        Temp = (x7 - x1) >> 14;

    blk[56] = iclip[512 + Temp];

}


