
#include <zephyr.h>
#include <string.h>
#include <uart.h>
#include <crc16.h>
#include <misc/printk.h>
#include <flash.h>
#include <watchdog.h>

#define BOOT_VERSION  "V100"


char const s_u8BoardName[] = "ENT2CTLA_2538";

#define FLASH_BASE_ADDR               0x200000                             
#define APP_RUN_IMG_MAX_SIZE         (200*1024)
#define APP_SOFTWARE_BK_LZMA_MAX_SIZE (140*1024)
#define FLASH_SECTOR_SIZES            2048

#define APP_RUN_START_ADDR FLASH_BASE_ADDR
#define APP_RESET_ADD     (FLASH_BASE_ADDR+4)
#define APP_BK_LZMA_ST_ADDR (FLASH_BASE_ADDR+APP_RUN_IMG_MAX_SIZE)

#define IH_NMLEN         32
#define IH_MAGIC         0x54CC2538
#define IH_IMG_BIOS      0
#define IH_IMG_APP       1
#define BOARD_TYPE		 0xCC
#define SOFTWARE_ID      1
#define IH_LZMA_IMG      1


#define FLASH_DEV_NAME "flash"
#define WATCHDOG_DEV_NAME "WDT"

static struct device *uart_console_dev=(struct device *)0;
static struct device *flash_dev=(struct device *)0;
static struct device *wdg_dev=(struct device *)0;

extern int lzma_update(int (*Read)(void *, void *, int *),int (*Write)(void *, const void *, int));

typedef enum
{
	//����Xmodem�����ַ�
	 XMODEM_NUL = 0x00, 
	 XMODEM_SOH = 0x01, 
	 XMODEM_STX = 0x02, 
	 XMODEM_EOT = 0x04, 
	 XMODEM_ACK = 0x06, 
	 XMODEM_NAK = 0x15, 
	 XMODEM_CAN = 0x18, 
	 XMODEM_EOF = 0x1A, 
	 XMODEM_RECIEVING_WAIT_CHAR =  'C' 
}Xmodem;

#define ESC_KEY   0X1B

typedef enum
{
	//����Xmoden��������ַ�
    XMODEM_SEQ_ERO= 0x11, 
	XMODEM_SEQ_REPEAT= 0x22, 
	XMODEM_SEQ_OK= 0x33, 
	XMODEM_START_OK= 0x44, 
	XMODEM_START_ERO = 0x55,  
	XMODEM_END_EOF = 0x66,
	XMODEM_FLASH_ERO = 0x77,
	XMODEM_FRAME_ERO = 0x99,
	XMODEM_REMOT_CANCEL =0xaa,
	XMODEM_OK = 0xbb
}XmodemERO;


static void WdgClear(void)
{
	if((struct device *)0 != wdg_dev)
	{
		wdt_reload(wdg_dev);
	}
	
}


static inline int MyPutC(unsigned char c)
{
	return uart_poll_out(uart_console_dev,c);
}

//ȡ��XMODEMģʽ
static void CancelXModem(void)
{
    (void)MyPutC(XMODEM_CAN);
    (void)MyPutC(XMODEM_CAN);
}

static int FlashErase(unsigned int u32FlashStartAddr,int i32FlashSizes)
{
	u32FlashStartAddr -= FLASH_BASE_ADDR;
	while(i32FlashSizes > 0)
	{
		if ( 0!= flash_erase(flash_dev,u32FlashStartAddr, FLASH_SECTOR_SIZES))
		{
			return -1;
		}
		u32FlashStartAddr += FLASH_SECTOR_SIZES;
		i32FlashSizes -= FLASH_SECTOR_SIZES;
		WdgClear();
	}
	return 0;
}

static int FlashWrite(unsigned int u32FlashAddr,unsigned char *pData,int i32DataLen)
{
	u32FlashAddr -= FLASH_BASE_ADDR;
	return flash_write(flash_dev,u32FlashAddr,pData, i32DataLen);
}
//�ȴ���RS232����һ����Ч���ֽ�
static unsigned char uart_waitchar(void)
{
    int rec=-1;
    unsigned int DeyTime = 0;
	unsigned char rev_char;

    while(rec < 0)
    {
        rec = uart_poll_in(uart_console_dev, &rev_char);
        DeyTime++;
        if(0x6fff0 == DeyTime)
        {
            CancelXModem();
            return 0xff; /*ero*/
        }
    }
    return rev_char;
}


static unsigned char CheckSart(int * DataNum)
{
    unsigned char CheckChar = 0;

    CheckChar = uart_waitchar();
    switch(CheckChar)
    {
    case XMODEM_SOH:
        *DataNum = 128;
        return XMODEM_START_OK;

    case XMODEM_STX:
        *DataNum = 1024;
        return XMODEM_START_OK;

    case XMODEM_EOT:
        (void)MyPutC(XMODEM_ACK); //֪ͨPC��ȫ���յ�
        return XMODEM_END_EOF;

    case XMODEM_CAN:
        CancelXModem();
        printk("\r\nRemote Cancel !!");
        return XMODEM_REMOT_CANCEL;

    default:
        CancelXModem();
        printk("\nCheckChar = %02X\n", CheckChar);
        return XMODEM_START_ERO;
    }
}


static unsigned char CheckSeq(unsigned char PackID)
{
    unsigned char CheckPID;
    unsigned char _CheckPID;

    CheckPID = uart_waitchar();
    _CheckPID = uart_waitchar();

    _CheckPID = ~_CheckPID ;
    if( CheckPID == _CheckPID )
    {
        if(CheckPID == PackID)
        {
            return XMODEM_SEQ_OK;
        }
        else
        {
            /*06/04/20 bug: PackID = CheckSeq+1 -> PackID == CheckPID+1*/
            if(PackID == CheckPID+1)
            {
                return XMODEM_SEQ_REPEAT;
            }
            else
            {
                return XMODEM_SEQ_ERO;
            }
        }
    }
    else
    {
        return XMODEM_SEQ_ERO;
    }
}

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

static unsigned char CheckImgHeader(unsigned char *pBuf)
{
    image_header_t *pImage_Header = (image_header_t*)pBuf;

	if ((0 != memcmp((char*)s_u8BoardName, (char*)(pImage_Header->ih_name), strlen(s_u8BoardName)))
		|| (BOARD_TYPE	!= pImage_Header->ih_boardtype)
		|| (IH_IMG_APP	< pImage_Header->ih_imagetype)
		|| (SOFTWARE_ID != pImage_Header->ih_software_id)
		|| (IH_MAGIC    != pImage_Header->ih_magic)
		|| ((APP_SOFTWARE_BK_LZMA_MAX_SIZE < pImage_Header->ih_size)
		     &&(IH_LZMA_IMG == pImage_Header->ih_compress_type)))
	{
        return XMODEM_FRAME_ERO;
	}
	return XMODEM_OK;
}

static int GetImageCompressType(unsigned char *pBuf)
{
	image_header_t *pImage_Header = (image_header_t*)pBuf;
	return pImage_Header->ih_compress_type;
}

#if 0
static int GetImageSizes(unsigned char *pBuf)
{
	image_header_t *pImage_Header = (image_header_t*)pBuf;
	return pImage_Header->ih_size;
}
#endif

static unsigned char CheckImgContent_Lzma(unsigned int ImageAddr)
{
	image_header_t *pImage_Header = (image_header_t*)ImageAddr;
	unsigned char *pcrc = (unsigned char *)(ImageAddr+sizeof(image_header_t)+pImage_Header-> ih_size);
	if (pImage_Header->ih_dcrc != ((pcrc[1]<<8)+pcrc[0]))
		return XMODEM_FRAME_ERO;
	WdgClear();
	unsigned short ImgCrc = crc16_itu_t( 0,(unsigned char *) (ImageAddr+sizeof(image_header_t)), pImage_Header-> ih_size);
	WdgClear();
	if (ImgCrc != pImage_Header->ih_dcrc)
		return XMODEM_FRAME_ERO;
	return XMODEM_OK;
}

static unsigned char g_RecDataBuf[1024];
static bool g_bImageLzma = false;

/********************************************/
/********************************************/
static unsigned char WaitXmodem (void)
{
    unsigned int DeyTime = 0;
    unsigned char  packNO = 1;
    unsigned int charcount = 0;
    unsigned short Temp = 0;
    unsigned int AppAddress = APP_BK_LZMA_ST_ADDR;
	unsigned int AppMaxSizes = 0;
    int RecDataNum = 0;
    int  XmodemReturn = 0;
	unsigned char rev_char;
	unsigned char *pFlashWriteData=(unsigned char *)0;
	unsigned char FlashEraseFlag=0;

    union
    {
        unsigned short TWord;
        struct{unsigned char lo;unsigned char hi;}DDWord;
    }unCrc;

    WdgClear();

    //ÿ����PC������һ�������ַ�"C"���ȴ������֡�soh��
    while(0 == RecDataNum)
    {
        XmodemReturn = uart_poll_in(uart_console_dev, &rev_char);
        if(XmodemReturn < 0)
        {
            DeyTime++;
            if(DeyTime > 0x7fff0)//timer0 over flow
            {
                DeyTime = 0;
                WdgClear();
                (void)MyPutC(XMODEM_RECIEVING_WAIT_CHAR); //file://send a "C"

                /*��ʱ����*/
                if ((Temp++) > 120)
                {
                    return XMODEM_REMOT_CANCEL;
                }
            }
        }
        else
        {
            switch(rev_char)
            {
            case XMODEM_SOH:
                RecDataNum = 128;
                break;

            case XMODEM_STX:
                RecDataNum = 1024;
                break;

            case 'r':
            case 'R':
                printk("\nReset!\n");
                while(1)
                {}

            default:
                CancelXModem();
                printk("\nERO Modem!Please Change!\n");
            }
        }
    }

    packNO = 1;
    //��ʼ�������ݿ�
    do
    {
        if (( DeyTime = CheckSeq(packNO)) != XMODEM_SEQ_OK )
        {
            
            if( XMODEM_SEQ_ERO == DeyTime)
            {
                (void)MyPutC(XMODEM_NAK); //file://Ҫ���ط����ݿ�
            }
            else
            {
                (void)MyPutC(XMODEM_ACK); //�ظ���ȷ�յ�һ�����ݿ�
            }
			continue;
		}
        //file://�˶����ݿ�����ȷ
        for(charcount=0; charcount< RecDataNum; charcount++)//����һ֡����
        {
            g_RecDataBuf[charcount] = uart_waitchar();
        }

        unCrc.DDWord.hi = uart_waitchar(); //����2���ֽڵ�CRCЧ����
        unCrc.DDWord.lo = uart_waitchar();
        WdgClear();

        if( crc16_itu_t(0,g_RecDataBuf, RecDataNum) != ((unCrc.DDWord.hi<<8)+unCrc.DDWord.lo))//unCrc.TWord)//file://CRCУ����֤
        {
            (void)MyPutC(XMODEM_NAK); //file://Ҫ���ط����ݿ�
            continue;
		}
        pFlashWriteData = g_RecDataBuf;

        WdgClear();
        //��ȷ����һ֡����
        /*�յ���һ֡����������Լ���ļ�ͷ������flash*/
        if (0 == FlashEraseFlag)//(1 == packNO)
        {
            if (XMODEM_OK != CheckImgHeader(g_RecDataBuf))
            {
                CancelXModem();
				//image_header_t *pImage_Header=(image_header_t *)g_RecDataBuf;
				//printk("ih_name=%s,ih_magic=%X\n",pImage_Header->ih_name,pImage_Header->ih_magic);
				//printk("ih_software_id=%d,ih_imagetype=%X\n",pImage_Header->ih_software_id,pImage_Header->ih_imagetype);
				//printk("ih_boardtype=%X\n",pImage_Header->ih_boardtype);
				printk("\nInvalid Image\n");
				return XMODEM_FRAME_ERO;
			}
			if (IH_LZMA_IMG == GetImageCompressType(g_RecDataBuf))
			{
				g_bImageLzma = true;
				AppAddress = APP_BK_LZMA_ST_ADDR;
				AppMaxSizes = APP_SOFTWARE_BK_LZMA_MAX_SIZE;
			}
			else
			{
				g_bImageLzma = false;
				AppAddress = APP_RUN_START_ADDR;
				AppMaxSizes = APP_RUN_IMG_MAX_SIZE + APP_SOFTWARE_BK_LZMA_MAX_SIZE;
				//AppMaxSizes = GetImageSizes(g_RecDataBuf)+2;//����������ֽ�CRC
				pFlashWriteData += sizeof(image_header_t);
			    RecDataNum -= sizeof(image_header_t);
			}

            if(0 != FlashErase(AppAddress, AppMaxSizes))
            {
				CancelXModem();
				printk("\n********Flash Erase Error!\n");
				return XMODEM_FLASH_ERO;
            }
			FlashEraseFlag = 1;
        }
		WdgClear();
		//���յ�����д��Flash��
		XmodemReturn = FlashWrite(AppAddress,pFlashWriteData, RecDataNum);
        if (XmodemReturn != 0)
		{
            CancelXModem();
            printk("\n********Write flash fail,ret=%d\n",XmodemReturn);
            return XMODEM_FLASH_ERO;
        }
        AppAddress += RecDataNum;
        (void)MyPutC(XMODEM_ACK); //��ȷ�յ�һ�����ݿ�
        packNO++; //���ݿ��ż�1
    }
    while((XmodemReturn = CheckSart(&RecDataNum)) == XMODEM_START_OK); //ѭ�����գ�ֱ��ȫ������
    if(XmodemReturn == XMODEM_END_EOF)
    {
        XmodemReturn = XMODEM_OK;
        printk("\nUpdata OK!\n");
    }
    else
    {
        printk("Rtn = %d\n", XmodemReturn);
        printk("\nXMODEM_Frame_ERO!Waiting reset!\n");
        XmodemReturn = XMODEM_FRAME_ERO;
    }
    printk("Last Pack ID =[%d]",packNO);

    return XmodemReturn;
}

/*******************************************************************************
* Function Name : JumpToApp
* Description : ��ת��APP������
* Input : None
* Output : None
* Return : None
*******************************************************************************/
void JumpToApp(void)
{
	unsigned int AppAddress;
	AppAddress = *((unsigned int *)(APP_RESET_ADD));
	printk("\nJump To App\n");
	printk("PC_POINTER = 0x%X\n",AppAddress);
    WdgClear();
	/*������ת��APPADDRESS��ַ��ִ��*/
	((void ( *)(void))(AppAddress))();
}

static unsigned int s_AppAddress=APP_RUN_START_ADDR;
static unsigned int s_AppAddress_lzma=APP_BK_LZMA_ST_ADDR+sizeof(image_header_t);
static int MyRead(void *p, void *buf, int *size)
{
  (void)p;
  memcpy(buf,(void *)s_AppAddress_lzma,*size);
  s_AppAddress_lzma += *size;
  return 0;
}
static int MyWrite(void *p, const void *buf, int size)
{
  (void)p;
  if ( 0 != FlashWrite(s_AppAddress, (unsigned char *)buf, size))
  {
	return -1;
  }
  WdgClear();
  s_AppAddress += size;
  return size;
}

static void UpdateRunApp_Lzma(void)
{
	(void)FlashErase(APP_RUN_START_ADDR, APP_RUN_IMG_MAX_SIZE);
	s_AppAddress=APP_RUN_START_ADDR;
	s_AppAddress_lzma=APP_BK_LZMA_ST_ADDR+sizeof(image_header_t);
	lzma_update(MyRead,MyWrite);
	(void)FlashErase(APP_BK_LZMA_ST_ADDR, APP_SOFTWARE_BK_LZMA_MAX_SIZE);
}
/*******************************************************************************
* Function Name : main
* Description : Main program
* Input : None
* Output : None
* Return : None
*******************************************************************************/
void main(void)
{
	printk("\nBoot start... ");
	printk(BOOT_VERSION);
	printk("\nPress Esc key to update your APP!");
	uart_console_dev = device_get_binding(CONFIG_UART_CONSOLE_ON_DEV_NAME);
	if ((struct device *)0 == uart_console_dev)
	{
		return;
	}
	flash_dev = device_get_binding(FLASH_DEV_NAME);
    if ((struct device *)0 == uart_console_dev)
	{
		return;
	}
	wdg_dev = device_get_binding(WATCHDOG_DEV_NAME);
	if ((struct device *)0 == wdg_dev)
	{
		return;
	}
	WdgClear();
	/*1-->�ȴ���������*/
	if(ESC_KEY != uart_waitchar())
	{   //δ����xmodem�������鿴�Ƿ��г����Ǩ��������
		if ((0xFFFFFFFF != *((unsigned int *)APP_BK_LZMA_ST_ADDR))
			&&(XMODEM_OK == CheckImgHeader((unsigned char *)APP_BK_LZMA_ST_ADDR))
		    &&(IH_LZMA_IMG == GetImageCompressType((unsigned char *)APP_BK_LZMA_ST_ADDR))
		    &&(XMODEM_OK == CheckImgContent_Lzma(APP_BK_LZMA_ST_ADDR)))
		{
			UpdateRunApp_Lzma();
		}
		JumpToApp();
	}
	/*�û�����ESC����*/
	printk("\nPlease Download your APP:>\n");

	/*2-->�ȴ�XMODEM�·��ļ�*/
	if(WaitXmodem()==XMODEM_OK)
	{
	    //У���ļ�����Ǩ�����������ȵ�
		if (true == g_bImageLzma) 
		{
			if (XMODEM_OK == CheckImgContent_Lzma(APP_BK_LZMA_ST_ADDR))
			{
				UpdateRunApp_Lzma();
			}
			else
			{
				printk("\nUpdate app failed.Crc err!\n");
			}		
		}
	}
	JumpToApp();
}


