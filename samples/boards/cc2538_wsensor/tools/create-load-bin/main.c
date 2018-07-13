//input: read userapp.bin
//output: generate userapp_.bin

//0x0000 - 0x007f      use
//0x0080 - 0x0fff      not use
//0x1000 - end of file use

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//FILE * header_bin;
FILE * appboot_bin;
FILE * remote_bin;
//FILE * boot_burn;
//FILE * user_burn;

#define U16 unsigned short
#define U8 unsigned char
#define BOOT_START_ADD 0x0000
#define APP_START_ADD 0x2000
#define APP_ADD (APP_START_ADD - BOOT_START_ADD) 
enum 
{
    PMU_BOOT_BIN = 0,
    XMODEM_BOOT_BIN,
    USER_APP_BIN
};
union U8_U16
{
    U16 D_U16;
    U8 D_U8[2]; //H in [1] L in [0]
};

 
U16 crc_ta[256]={               // CRC table
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
    0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
    0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
    0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
    0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
    0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
    0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
    0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
    0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
    0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
    0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
    0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
    0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
    0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
    0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
    0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
    0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
    0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
    0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
    0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
    0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
    0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
    0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
    };

#define IH_NMLEN         32
//#define IH_MAGIC         0x54CC1310
#define IH_IMG_BIOS      0
#define IH_IMG_APP       1
#define BOARD_TYPE		 0xCC
#define SOFTWARE_ID      1

typedef struct  image_header {
    unsigned char	ih_name[IH_NMLEN];  /* Image Name		*/
    unsigned int	ih_magic;           /* Image Header Magic Number	*/
    unsigned int	ih_size;            /* Image Data Size,not include image header*/
    unsigned int	ih_load;            /* Data	 Load  Address		*/
    unsigned short	ih_dcrc;            /* Image Data CRC Checksum,not include image header*/
    unsigned short  ih_vers;            /* Image version*/
    unsigned char	ih_arch;            /* CPU architecture		*/
    unsigned char 	ih_boardtype;       /* Board type*/
    unsigned char 	ih_manufacturer;    /* manufacturer*/
    unsigned char   ih_imagetype;       /* Image type*/
	unsigned char   ih_boottype;        /* Boot type*/
    unsigned char   ih_rfu0[64];        /* Reserve for fucture use*/
    unsigned char   ih_software_id;     /* Software ID*/
	unsigned char   ih_compress_type;   /* Image compress type*/
    unsigned char   ih_rfu1[9];        /* Reserve for fucture use*/
} image_header_t;


#define crc_flag_restart 0
#define crc_flag_countBT 1
#define crc_flag_get_H   2
#define crc_flag_get_L   3

U8 cal_crc ( U8 tastName, U8 * crcStr, int len )
{
    static union U8_U16 crc;
    int i;

    switch ( tastName )
    {
        case crc_flag_restart:
            crc.D_U16 = 0x0000;
            break;
        case crc_flag_countBT: // count 1 byte
            for ( i = 0; i < len; i++ )
            {
	        //			crc.D_U16 = ((crc.D_U16<<8) | crcStr[i]) ^ crc_ta[(crc.D_U16>>8)&0xFF];
                //change crc to crc_ccitt
                crc.D_U16  = (unsigned short)((crc.D_U16 << 8) ^ crc_ta[((crc.D_U16 >> 8) ^ *(char *)crcStr++) & 0x00FF]);
            }
            break;
        case crc_flag_get_H: //end1
            return crc.D_U8[1]; //high
        case crc_flag_get_L: //end2
            return crc.D_U8[0]; //low
        default:
            break;
    }
    return 0;
}

//#define m_debug



void process ( int argc, char *argv[ ], char *envp[ ] )
{
    U8 tmpStr[256];
    unsigned int len;
    unsigned int count_len= 0;
    image_header_t pImage_Header = {0};

    appboot_bin = fopen(argv[1], "rb" );
    remote_bin  = fopen(argv[2], "wb" );

    if (!appboot_bin)
    {
        printf("Cannot open appboot file.\n");
        exit(0);
    }

    /* 计算appboot文件长度及CRC  Modify by l00192499 2013-09-23 */
    do
    {
        len = fread( tmpStr, 1, 128, appboot_bin);
        if ( len > 0 ) 
        {
            (void)cal_crc( crc_flag_countBT, tmpStr, len);
            count_len += len;
        }
        else 
        {
            break;
        }
    } while (1);

    /* 文件头信息填充 Modify by l00192499 2013-09-23 */
	if (strlen(argv[3]) <= IH_NMLEN)  //该参数在打包的bat文件中传进来
	{
		memcpy(pImage_Header.ih_name, argv[3], strlen(argv[3]));
	}
	else
	{
		memcpy(pImage_Header.ih_name, argv[3], IH_NMLEN);
	}

    pImage_Header.ih_magic = strtoul(argv[4],NULL,16);//IH_MAGIC;
    pImage_Header.ih_size  = count_len;
    pImage_Header.ih_load  = 0;
    pImage_Header.ih_dcrc  = (U16)cal_crc(crc_flag_get_L, NULL, 0);
    pImage_Header.ih_dcrc |= (U16)(cal_crc(crc_flag_get_H, NULL, 0) << 8);
    pImage_Header.ih_vers  = 0xFFFF;
    pImage_Header.ih_arch  = 2;
    pImage_Header.ih_boardtype    = BOARD_TYPE;
    pImage_Header.ih_manufacturer = 0;
    pImage_Header.ih_imagetype    = atoi(argv[5]);//IH_IMG_APP;
	pImage_Header.ih_boottype = atoi(argv[6]);
    memset(pImage_Header.ih_rfu0, 0, 64);
    pImage_Header.ih_software_id  = atoi(argv[7]);
	pImage_Header.ih_compress_type = atoi(argv[8]);
    //memset(pImage_Header.ih_rfu1, 0, 10);
	memset(pImage_Header.ih_rfu1, 0, 9);

    /* 将文件头数据写入remote_bin文件 */
    len = fwrite((char *)&pImage_Header, 1, 128, remote_bin);

    if (128 != len)
    {
        printf("write head info err.\n");
		return;
    }

    /* 拷贝appboot文件 */
    fseek(appboot_bin, 0, SEEK_SET);
    do
    {
        len = fread( tmpStr, 1, sizeof(tmpStr), appboot_bin );
        if (len > 0)
        {
            fwrite( tmpStr, 1, len, remote_bin ); 
        }
        else 
        {
            break;
        }
    } while( 1 );
	tmpStr[0] = pImage_Header.ih_dcrc&0xFF;
	tmpStr[1] = pImage_Header.ih_dcrc>>8;
	fwrite( tmpStr, 1, 2, remote_bin ); 

    /*关闭所有打开的文件*/	
    fclose(appboot_bin);
    fclose(remote_bin);
}  



int main (int argc, char *argv[ ], char *envp[ ])
{   
    if (argc < 9)
    {
        printf("usage:%s input_file output_file board_name image_magic image_type boot_type software_id compress_type\n",argv[0]);
		return 1;
	}
	process( argc, argv, envp ); 

    return 0;
}


