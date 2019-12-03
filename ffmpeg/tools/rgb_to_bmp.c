#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
 
#define WIDTH 	640
#define HEIGTH 	480
#define BITS_PER_PIXCEL 24
 
/** 必须对齐，所以用这个来对齐 */
#pragma pack(1)
 
typedef struct
{
	unsigned short    	bfType;
    unsigned int   		bfSize;
    unsigned short		bfReserved1;
    unsigned short    	bfReserved2;
    unsigned int   		bfOffBits;
} BMP_FILE_HEADER;
 
typedef struct{
    unsigned int      	biSize;
    int       			biWidth;
    int       			biHeight;
    unsigned short      biPlanes;
    unsigned short      biBitCount;
    unsigned int      	biCompression;
    unsigned int      	biSizeImage;
    int       			biXPelsPerMeter;
    int       			biYPelsPerMeter;
    unsigned int      	biClrUsed;
    unsigned int      	biClrImportant;
}BMP_INFO_HEADER;
 
#pragma pack()
 
int main()
{
    BMP_FILE_HEADER bmpHeader;
    BMP_INFO_HEADER bmpInfo;
 
	/* 字节对齐,每一行的字节数必须是4的整数倍,
	此处根据传递的颜色格式进行处理。这里的处理逻辑是：
	传入的颜色格式是RGBA。
	写到文件的格式BGR（否则会红蓝颠倒）*/
    int bytesPerLine = (WIDTH*BITS_PER_PIXCEL+31)/32*4;
    int pixcelBytes  = bytesPerLine*HEIGTH;
	printf("one line bytes:%d  all bytes:%d\n",bytesPerLine);
 
    bmpHeader.bfType        = 0x4D42;
    bmpHeader.bfReserved1   = 0;
    bmpHeader.bfReserved2   = 0;
    bmpHeader.bfOffBits     = sizeof(BMP_FILE_HEADER)+sizeof(BMP_INFO_HEADER);
    bmpHeader.bfSize        = bmpHeader.bfOffBits+pixcelBytes;
  
    bmpInfo.biSize          = sizeof(BMP_INFO_HEADER);
    bmpInfo.biWidth         = WIDTH;
    /** 这样图片才不会倒置 */
    bmpInfo.biHeight        = -HEIGTH; 
    bmpInfo.biPlanes        = 1;
    bmpInfo.biBitCount      = BITS_PER_PIXCEL;
    bmpInfo.biCompression   = 0;
    bmpInfo.biSizeImage     = pixcelBytes;
    bmpInfo.biXPelsPerMeter = 0;
    bmpInfo.biYPelsPerMeter = 0;
    bmpInfo.biClrUsed       = 0;
    bmpInfo.biClrImportant  = 0;
 
	FILE* fp1=NULL;
	FILE* fp2=NULL;
	fp1 = fopen("data.bgr","rb+");
	fp2 = fopen("data.bmp","wb+");
	
    fwrite(&bmpHeader, sizeof(BMP_FILE_HEADER), 1, fp2);
    fwrite(&bmpInfo,   sizeof(BMP_INFO_HEADER), 1, fp2);
	
	int i,j;
	unsigned char buf[3]={0};
    for(i=0;i<HEIGTH;i++)
    {
        for(j=0;j<WIDTH;j++)
        {
			fread(buf, 3, 1, fp1);
			fwrite(buf, 3, 1, fp2);
        }
        //必须4字节对齐，否则会显示错误。
        fwrite(buf, bytesPerLine-WIDTH*3, 1, fp2);
    }
	
    fclose(fp1);
	fclose(fp2);

    return 0;
}


