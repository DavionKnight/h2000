/*
 *
 * @file		imageapi.c
 * @author	Tianzhy <tianzy@huahuan.com>
 * @date		2014-04-25
 * @modif   zhangjj<zhangjj@huahuan.com>
 */

#include <stdio.h>
#include <linux/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include "api.h"

#define TYPE_IMAGE 		1
#define TYPE_KERNEL1 	2
#define TYPE_RAMFS1 		3
#define TYPE_DTB1		4
#define TYPE_APP1		5
#define TYPE_KERNEL2 	6
#define TYPE_RAMFS2 		7
#define TYPE_DTB2		8
#define TYPE_APP2		9
#define TYPE_FPGA_LOCAL		10
#define TYPE_FPGA_REMOTE	11

#define FPGA_WRITE_ENABLE	0x0130
#define FPGA_READ_ENABLE	0x0132
#define FPGA_DEV_SLAVE_ADD	0x0134
#define FPGA_WRITE_DATA		0x0136
#define FPGA_READ_DATA		0x0138
#define FPGA_DATA_VALID		0x013a

unsigned char mem[256];

typedef		unsigned char 		uint8_t;
typedef		unsigned short 		uint16_t;
typedef		unsigned int 		uint32_t;


#define local static
#define ZEXPORT	/* empty */

#define uswap_32(x) \
    ((((x) & 0xff000000) >> 24) | \
     (((x) & 0x00ff0000) >>  8) | \
     (((x) & 0x0000ff00) <<  8) | \
     (((x) & 0x000000ff) << 24))

//#define uswap_32(x) x
#define cpu_to_le32(x)		uswap_32(x)
#define le32_to_cpu(x)		uswap_32(x)

#define tole(x) cpu_to_le32(x)

/* ========================================================================
 * Table of CRC-32's of all single-byte values (made by make_crc_table)
 */

local const uint32_t crc_table[256] = {
    tole(0x00000000L), tole(0x77073096L), tole(0xee0e612cL), tole(0x990951baL),
    tole(0x076dc419L), tole(0x706af48fL), tole(0xe963a535L), tole(0x9e6495a3L),
    tole(0x0edb8832L), tole(0x79dcb8a4L), tole(0xe0d5e91eL), tole(0x97d2d988L),
    tole(0x09b64c2bL), tole(0x7eb17cbdL), tole(0xe7b82d07L), tole(0x90bf1d91L),
    tole(0x1db71064L), tole(0x6ab020f2L), tole(0xf3b97148L), tole(0x84be41deL),
    tole(0x1adad47dL), tole(0x6ddde4ebL), tole(0xf4d4b551L), tole(0x83d385c7L),
    tole(0x136c9856L), tole(0x646ba8c0L), tole(0xfd62f97aL), tole(0x8a65c9ecL),
    tole(0x14015c4fL), tole(0x63066cd9L), tole(0xfa0f3d63L), tole(0x8d080df5L),
    tole(0x3b6e20c8L), tole(0x4c69105eL), tole(0xd56041e4L), tole(0xa2677172L),
    tole(0x3c03e4d1L), tole(0x4b04d447L), tole(0xd20d85fdL), tole(0xa50ab56bL),
    tole(0x35b5a8faL), tole(0x42b2986cL), tole(0xdbbbc9d6L), tole(0xacbcf940L),
    tole(0x32d86ce3L), tole(0x45df5c75L), tole(0xdcd60dcfL), tole(0xabd13d59L),
    tole(0x26d930acL), tole(0x51de003aL), tole(0xc8d75180L), tole(0xbfd06116L),
    tole(0x21b4f4b5L), tole(0x56b3c423L), tole(0xcfba9599L), tole(0xb8bda50fL),
    tole(0x2802b89eL), tole(0x5f058808L), tole(0xc60cd9b2L), tole(0xb10be924L),
    tole(0x2f6f7c87L), tole(0x58684c11L), tole(0xc1611dabL), tole(0xb6662d3dL),
    tole(0x76dc4190L), tole(0x01db7106L), tole(0x98d220bcL), tole(0xefd5102aL),
    tole(0x71b18589L), tole(0x06b6b51fL), tole(0x9fbfe4a5L), tole(0xe8b8d433L),
    tole(0x7807c9a2L), tole(0x0f00f934L), tole(0x9609a88eL), tole(0xe10e9818L),
    tole(0x7f6a0dbbL), tole(0x086d3d2dL), tole(0x91646c97L), tole(0xe6635c01L),
    tole(0x6b6b51f4L), tole(0x1c6c6162L), tole(0x856530d8L), tole(0xf262004eL),
    tole(0x6c0695edL), tole(0x1b01a57bL), tole(0x8208f4c1L), tole(0xf50fc457L),
    tole(0x65b0d9c6L), tole(0x12b7e950L), tole(0x8bbeb8eaL), tole(0xfcb9887cL),
    tole(0x62dd1ddfL), tole(0x15da2d49L), tole(0x8cd37cf3L), tole(0xfbd44c65L),
    tole(0x4db26158L), tole(0x3ab551ceL), tole(0xa3bc0074L), tole(0xd4bb30e2L),
    tole(0x4adfa541L), tole(0x3dd895d7L), tole(0xa4d1c46dL), tole(0xd3d6f4fbL),
    tole(0x4369e96aL), tole(0x346ed9fcL), tole(0xad678846L), tole(0xda60b8d0L),
    tole(0x44042d73L), tole(0x33031de5L), tole(0xaa0a4c5fL), tole(0xdd0d7cc9L),
    tole(0x5005713cL), tole(0x270241aaL), tole(0xbe0b1010L), tole(0xc90c2086L),
    tole(0x5768b525L), tole(0x206f85b3L), tole(0xb966d409L), tole(0xce61e49fL),
    tole(0x5edef90eL), tole(0x29d9c998L), tole(0xb0d09822L), tole(0xc7d7a8b4L),
    tole(0x59b33d17L), tole(0x2eb40d81L), tole(0xb7bd5c3bL), tole(0xc0ba6cadL),
    tole(0xedb88320L), tole(0x9abfb3b6L), tole(0x03b6e20cL), tole(0x74b1d29aL),
    tole(0xead54739L), tole(0x9dd277afL), tole(0x04db2615L), tole(0x73dc1683L),
    tole(0xe3630b12L), tole(0x94643b84L), tole(0x0d6d6a3eL), tole(0x7a6a5aa8L),
    tole(0xe40ecf0bL), tole(0x9309ff9dL), tole(0x0a00ae27L), tole(0x7d079eb1L),
    tole(0xf00f9344L), tole(0x8708a3d2L), tole(0x1e01f268L), tole(0x6906c2feL),
    tole(0xf762575dL), tole(0x806567cbL), tole(0x196c3671L), tole(0x6e6b06e7L),
    tole(0xfed41b76L), tole(0x89d32be0L), tole(0x10da7a5aL), tole(0x67dd4accL),
    tole(0xf9b9df6fL), tole(0x8ebeeff9L), tole(0x17b7be43L), tole(0x60b08ed5L),
    tole(0xd6d6a3e8L), tole(0xa1d1937eL), tole(0x38d8c2c4L), tole(0x4fdff252L),
    tole(0xd1bb67f1L), tole(0xa6bc5767L), tole(0x3fb506ddL), tole(0x48b2364bL),
    tole(0xd80d2bdaL), tole(0xaf0a1b4cL), tole(0x36034af6L), tole(0x41047a60L),
    tole(0xdf60efc3L), tole(0xa867df55L), tole(0x316e8eefL), tole(0x4669be79L),
    tole(0xcb61b38cL), tole(0xbc66831aL), tole(0x256fd2a0L), tole(0x5268e236L),
    tole(0xcc0c7795L), tole(0xbb0b4703L), tole(0x220216b9L), tole(0x5505262fL),
    tole(0xc5ba3bbeL), tole(0xb2bd0b28L), tole(0x2bb45a92L), tole(0x5cb36a04L),
    tole(0xc2d7ffa7L), tole(0xb5d0cf31L), tole(0x2cd99e8bL), tole(0x5bdeae1dL),
    tole(0x9b64c2b0L), tole(0xec63f226L), tole(0x756aa39cL), tole(0x026d930aL),
    tole(0x9c0906a9L), tole(0xeb0e363fL), tole(0x72076785L), tole(0x05005713L),
    tole(0x95bf4a82L), tole(0xe2b87a14L), tole(0x7bb12baeL), tole(0x0cb61b38L),
    tole(0x92d28e9bL), tole(0xe5d5be0dL), tole(0x7cdcefb7L), tole(0x0bdbdf21L),
    tole(0x86d3d2d4L), tole(0xf1d4e242L), tole(0x68ddb3f8L), tole(0x1fda836eL),
    tole(0x81be16cdL), tole(0xf6b9265bL), tole(0x6fb077e1L), tole(0x18b74777L),
    tole(0x88085ae6L), tole(0xff0f6a70L), tole(0x66063bcaL), tole(0x11010b5cL),
    tole(0x8f659effL), tole(0xf862ae69L), tole(0x616bffd3L), tole(0x166ccf45L),
    tole(0xa00ae278L), tole(0xd70dd2eeL), tole(0x4e048354L), tole(0x3903b3c2L),
    tole(0xa7672661L), tole(0xd06016f7L), tole(0x4969474dL), tole(0x3e6e77dbL),
    tole(0xaed16a4aL), tole(0xd9d65adcL), tole(0x40df0b66L), tole(0x37d83bf0L),
    tole(0xa9bcae53L), tole(0xdebb9ec5L), tole(0x47b2cf7fL), tole(0x30b5ffe9L),
    tole(0xbdbdf21cL), tole(0xcabac28aL), tole(0x53b39330L), tole(0x24b4a3a6L),
    tole(0xbad03605L), tole(0xcdd70693L), tole(0x54de5729L), tole(0x23d967bfL),
    tole(0xb3667a2eL), tole(0xc4614ab8L), tole(0x5d681b02L), tole(0x2a6f2b94L),
    tole(0xb40bbe37L), tole(0xc30c8ea1L), tole(0x5a05df1bL), tole(0x2d02ef8dL)
};


/* ========================================================================= */
# if __BYTE_ORDER == __LITTLE_ENDIAN
#  define DO_CRC(x) crc = tab[(crc ^ (x)) & 255] ^ (crc >> 8)
# else
#  define DO_CRC(x) crc = tab[((crc >> 24) ^ (x)) & 255] ^ (crc << 8)
# endif

/* ========================================================================= */

/* No ones complement version. JFFS2 (and other things ?)
 * don't use ones compliment in their CRC calculations.
 */
uint32_t ZEXPORT crc32_no_comp(uint32_t crc, const unsigned char *buf, uint32_t len)
{
    const uint32_t *tab = crc_table;
    const uint32_t *b =(const uint32_t *)buf;
    size_t rem_len;

    crc = cpu_to_le32(crc);
    /* Align it */
    if (((long)b) & 3 && len) {
        uint8_t *p = (uint8_t *)b;
        do {
            DO_CRC(*p++);
        } while ((--len) && ((long)p)&3);
        b = (uint32_t *)p;
    }

    rem_len = len & 3;
    len = len >> 2;
    for (--b; len; --len) {
        /* load data 32 bits wide, xor data 32 bits wide. */
        crc ^= *++b; /* use pre increment for speed */
        DO_CRC(0);
        DO_CRC(0);
        DO_CRC(0);
        DO_CRC(0);
    }
    len = rem_len;
    /* And the last few bytes */
    if (len) {
        uint8_t *p = (uint8_t *)(b + 1) - 1;
        do {
            DO_CRC(*++p); /* use pre increment for speed */
        } while (--len);
    }

    return le32_to_cpu(crc);
}
#undef DO_CRC

uint32_t ZEXPORT crc32 (uint32_t crc, const unsigned char *p, uint32_t len)
{
    return crc32_no_comp(crc ^ 0xffffffffL, p, len) ^ 0xffffffffL;
}

/*
 * Calculate the crc32 checksum triggering the watchdog every 'chunk_sz' bytes
 * of input.
 */
uint32_t ZEXPORT crc32_wd (uint32_t crc,
        const unsigned char *buf,
        uint32_t len, uint32_t chunk_sz)
{
    crc = crc32 (crc, buf, len);

    return crc;
}
/*
*/
#if 0
int readI2cInfo(void)
{
    int fd,ret;
    int i,j;
    unsigned int crc=0;
    memset(mem, 0, 256);

    unsigned char active[2];

    fd=open("/dev/fpga",O_RDWR);
    if(fd<0)
    {
        cdebug("open /dev/fpga error1\n");
        return(-1);
    }
    for(i=0;i<256;i++)
    {
        active[0] = (unsigned char)0xad ;
        active[1] = (unsigned char)i;

        lseek(fd, FPGA_DEV_SLAVE_ADD, SEEK_SET);
        write(fd, active,2);

        /* enable read */
        lseek(fd, FPGA_READ_ENABLE, SEEK_SET);
        read(fd, active,2);
        active[1] ^= (0x01);
        lseek(fd, FPGA_READ_ENABLE, SEEK_SET);
        write(fd, active,2);

        /*write is ok? */
        active[1]=0;
        while(!active[1])
        {
            lseek(fd, FPGA_DATA_VALID, SEEK_SET);
            read(fd, active,2);
        }

        /* get the data */
        lseek(fd, FPGA_READ_DATA, SEEK_SET);
        read(fd, active,2);
        mem[i]= active[1];
    }
    close(fd);
    // Debug
    for(i=0;i<256; i=i+16)
    {
        for(j=0;j<16; j++)
        {
            cdebug("%02x ", mem[i+j]);
            if(j == 15)
                cdebug("\n");
        }
    }

    return 0;
}
#endif
/*
 *
 *
 *
 */
int getAddress(int type)
{
    unsigned char buf[256];
    int i, add;

    memcpy(buf, mem, 256);

    if(type == TYPE_IMAGE)  /* get active image */
    {
        add = 0;
        for(i=0; i<0x100; i++)
        {
            if((buf[i] == 'K')  && (buf[i+1]=='e') && (buf[i+2]=='r') && (buf[i+3]=='n') && (buf[i+4]=='e') && (buf[i+5]=='l') && (buf[i+6]=='F')  \
                    && (buf[i+7]=='s'))
            {
                add = i+9;
                break;
            }
        }
        //cdebug(" iamge addr is %x \n", add );
        cdebug(" iamge addr is %x \n", add );
        return add;
    }
    else 	if(type == TYPE_KERNEL1)  /* get KERNEL1 flag */
    {
        add = 0;	
        for(i=0; i<0x100; i++)
        {

            if((buf[i] == 'K')  && (buf[i+1]=='1')  && (buf[i+2]=='=')  )
            {
                add = i+3;
                break;
            }
        }
        cdebug(" K1 addr is %x \n", add );
        return add;
    }
    else 	if(type == TYPE_RAMFS1)  /* get RAMFS1 flag */
    {
        add = 0;
        for(i=0; i<0x100; i++)
        {

            if((buf[i] == 'F')  && (buf[i+1]=='1')  && (buf[i+2]=='=')  )
            {
                add = i+3;
                break;
            }
        }
        cdebug(" R1 addr is %x \n", add );
        return add;
    }
    else 	if(type == TYPE_KERNEL2)  /* get KERNEL2 flag */
    {
        add = 0;
        for(i=0; i<0x100; i++)
        {
            if((buf[i] == 'K')  && (buf[i+1]=='2')  && (buf[i+2]=='=')  )
            {
                add = i+3;
                break;
            }
        }
        cdebug(" K2 addr is %x \n", add );
        return add;
    }
    else 	if(type == TYPE_RAMFS2)  /* get RAMFS2 flag */
    {
        add = 0;
        for(i=0; i<0x100; i++)
        {
            if((buf[i] == 'F')  && (buf[i+1]=='2')  && (buf[i+2]=='=')  )
            {
                add = i+3;
                break;
            }
        }
        cdebug(" R2 addr is %x \n", add );
        return add;
    }
}

int readRunningImage(void)
{
    FILE *thisFile = NULL;

    char buf[16];
    int image=0;

    thisFile = fopen("/image/activeImage", "rb"); 
    memset(buf,0,16);

    fread(&buf,1,16,thisFile);
    fclose(thisFile);

    image = buf[5]-48;

    //cdebug("image is %d\n", image);
    cdebug("image is %d\n", image);

    return image;
}

#if 1 
/***************************************
  type 					value
#define TYPE_IMAGE 	1	 	1 or 2
#define TYPE_KERNEL1 	2		0 or 1 0为无效，1为有效
#define TYPE_RAMFS1 	3		0 or 1
#define TYPE_DTB1		4		0 or 1
#define TYPE_APP1		5		0 or 1
#define TYPE_KERNEL2 	6		0 or 1
#define TYPE_RAMFS2 	7		0 or 1
#define TYPE_DTB2		8		0 or 1
#define TYPE_APP2		9		0 or 1
 ***************************************/

int writeImageFlag(int type, int value)
{
    int addr;
    int fd,ret;
    int i,j;
    unsigned int crc=0;
    unsigned char tmp[2];
    unsigned char active[2];
    unsigned char data[2];

    if((type > TYPE_FPGA_REMOTE)  || (type < TYPE_IMAGE))
    {
        cdebug("writeImageFlag() type%d  error...\n", type);
        return(-1);
    }
    if(type == TYPE_IMAGE)
    {
        if((value > 2) || (value < 1))
        {
            cdebug("Image value%d error.....\n");
            return(-1);
        }
    }
    else
    {
        if((value > 1) || (value < 0))
        {
            cdebug("error value%d error.....\n");
            return(-1);
        }
    }
#if 0
    readI2cInfo();
#else
  ret = EepromRead(0, mem, 256);	
  if(ret <0)
  { 
    cdebug("Error to read EEprom informaior in function boardinfo_init");
    return (-1);
  }
#endif

    addr = getAddress(type);
#if 0
    fd=open("/dev/fpga",O_RDWR);
    if(fd<0)
    {
        cdebug("open /dev/i2c-0 error1\n");
        close(fd);
        return(-1);
    }	
#endif

    mem[addr]=value+48;
    cdebug("mem[addr]=%d\n", mem[addr]);

    crc=crc32(crc, &mem[4],256-4);
    cdebug(" crc = %x %c\n", crc,mem[addr]);
    mem[0] = (unsigned char)(crc >> 24);
    mem[1] = (unsigned char)(crc >> 16);
    mem[2] = (unsigned char)(crc >> 8);
    mem[3] = (unsigned char)(crc >> 0);
    cdebug("mem[0-3] = %x %x %x %x\n", mem[0],mem[1],mem[2],mem[3]);
#if 0
    for(i=0;i<256;i++)
    {
        active[0] = (unsigned char)0xac ;
        active[1] = (unsigned char)i;

        lseek(fd, FPGA_DEV_SLAVE_ADD, SEEK_SET);
        if(write(fd, active,2)==2)
        {
            cdebug("write slave address and register address");
        } else {
            cdebug("Error to write slave address and register address\n");
        }

        //delay
        for(j=0; j<0x2ffffff; j++);

        /* put the data */
        data[0] = 0;
        data[1] = mem[i];
        //data[1] = 0x01 + i;
        lseek(fd, FPGA_WRITE_DATA, SEEK_SET);
        if(write(fd, data,2) ==2)
        {
            cdebug("write data to the eeprom");
        } else {
            cdebug("Error to write data to the eeprom");
        }

        /* enable write */
        lseek(fd, FPGA_WRITE_ENABLE, SEEK_SET);
        read(fd, active,2);
        cdebug("read the enable flag\n");
        active[1] ^= (0x01);

        lseek(fd, FPGA_WRITE_ENABLE, SEEK_SET);
        cdebug("\nTo enable the read flag\n");
        //pdata(active, 2);
        if(write(fd, active,2) == 2)
        {
            cdebug("Flag is enable");
        } else {
            cdebug("Error to enable write flag");
        }


        /*write is ok? */
        //active[1]=0;
        tmp[1] = 0;
        int count = 0;
        while(!tmp[1])
        {
            lseek(fd, FPGA_DATA_VALID, SEEK_SET);
            read(fd, tmp,2);
            count ++;
        }
        cdebug("The count is %d",count);
    }

    close(fd);	
#else
	ret = EepromWrite(0, mem, 256);	  
	if(ret <0)
	{ 
	  cdebug("Error to read EEprom informaior in function boardinfo_init");
	  return (-1);
	}
#endif
    return 0;
}
#endif
/***************************************
  type 					value
#define TYPE_IMAGE 	1	 	1 or 2
#define TYPE_KERNEL1 	2		0 or 1 0为无效，1为有效
#define TYPE_RAMFS1 	3		0 or 1
#define TYPE_DTB1		4		0 or 1
#define TYPE_APP1		5		0 or 1
#define TYPE_KERNEL2 	6		0 or 1
#define TYPE_RAMFS2 	7		0 or 1
#define TYPE_DTB2		8		0 or 1
#define TYPE_APP2		9		0 or 1
 ***************************************/

int readImageFlag(int type, int *value)
{
    int addr;
    int fd,ret;
    int i,j;
    unsigned int crc=0;

    if((type > TYPE_FPGA_REMOTE)  || (type < TYPE_IMAGE))
    {
        //cdebug("writeImageFlag() type%d  error...\n", type);
        cdebug("writeImageFlag() type%d  error...\n", type);
        return(-1);
    }

    if(value == NULL)
    {
        //cdebug("readImageFlag() value address error  error...\n");
        cdebug("readImageFlag() value address error  error...\n");
        return(-1);
    }	 
#if 0
		readI2cInfo();
#else
	  ret = EepromRead(0, mem, 256);	
	  if(ret <0)
	  { 
		cdebug("Error to read EEprom informaior in function boardinfo_init");
		return (-1);
	  }
#endif

    addr = getAddress(type);

    *value = mem[addr] - 48;

    cdebug("value = %d \n", *value);

    return 0;


}

#if 0
int main(void)
{
int va=0,ret = 0,i = 0, j = 0;

//readImageFlag(1, &va);
memcpy(&mem[5],"KernelFs",9);
EepromWrite(0, mem, 256);

writeImageFlag(1,2);
printf("Read the data\n");
#if 0
    readI2cInfo();
#else
  ret = EepromRead(0, mem, 256);	
  if(ret <0)
  { 
    cdebug("Error to read EEprom informaior in function boardinfo_init");
    return (-1);
  }

	for(i=0;i<256; i=i+16)
	{
		for(j=0;j<16; j++)
		{
		    printf("%02x ", mem[i+j]);
		    if(j == 15)
		        printf("\n");
		}
	}
#endif

return 0;
}
#endif

