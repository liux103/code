/*
type:0		未使用
type:1		一个非IDR图像的编码条带
type:2		编码条带数据分割快A
type:3		编码条带数据分割快B
type:4		编码条带数据分割快C
type:5		IDR图像的编码条带
type:6		辅助增强信息(SEI)
type:7		序列参数集
type:8		图像参数集
type:9		访问单元分隔符
type:10		序列结尾
type:11		流结尾
type:12		填充数据
type:13		序列参数集扩展
type:14-18	保留
type:19		未分割的辅助编码图像的编码条带
type:20-23	保留
type:24-31	未指定



H264 NALU:
---------------------------------------------------------
   3/4byte  |  1bit  | 3bit  |  5bit    |    n byte     |
---------------------------------------------------------
nal起始标志 | 禁止位 | level | nal type | nal数据(EBSP) |
---------------------------------------------------------
起始标志: 000001  00000001
SODB:原始编码数据
RBSP:SODB末尾添加一个bit1若干bit0,用于字节对齐
EBSP:RBSP添加仿校验字节0x03


RTP Packet = RTP Header + RTP payload
RTP Header:
---------------------------------------------------------------------------------------------------------------
2bit | 	1bit  	|	1bit   |	4bit	|  1bit	|	7bit	   |  2byte | 4byte | 	4byte	   |	4byte 	  |
---------------------------------------------------------------------------------------------------------------
版本 | 填充标志 | 扩展标志 | CSRC计数器 | 标记  | 有效荷载类型 | 序列号 | 时戳  | 同步信源SSRC | 特约信源CSRC |
---------------------------------------------------------------------------------------------------------------
 2	 |	0		|	0	   |	0		|  0/1	|	H264(96)   |  递增	| 递增	|	唯一	   |	无        |
---------------------------------------------------------------------------------------------------------------
M: 标记,占1位,不同的有效载荷有不同的含义,对于视频,标记一帧的结束.对于音频,标记会话的开始。
同步信源SSRC:如果同时传输音视频则会有两个不同的值
有效荷载类型:表示rtp包传送的数据类型格式
序列号:只要发送一个rtp包,序列好增加一个
时戳:同一帧,或同一个音频数据块的时间戳是一样的

RTP payload Header:
--------------------------------------------
  1bit  | 3bit  |  5bit    |    n byte     |
--------------------------------------------
 禁止位 | level | nal type | nal数据(EBSP) |
--------------------------------------------

单NALU分组:一个RTP包传输一个NAL单元
RTP payload Header与H264的头一样,直接复制即可
1+2+5

聚合分组:一个RTP包传输多个NAL单元
多个NAL单元只要有一个禁止位置1,则这个RTP包的禁止位置1,level取所有NALU的level最大值,
nal type为聚合类型,通常是STAP-A(24)或STAP-B(25).数据部分=NALU Size+即为原始码流一个完整NALU
NALU Size: 表示此原始码流NALU长度，2字节。
NALU HDR + NALU Date: 即为原始码流一个完整NALU。
1+2+5 + 2字节+DATA + 2字节+DARTA ...原始码流NALU长度
type为聚合类型,通常是STAP-A(24)或STAP-B(25)

分片分组:多个RTP包传输一个NAL单元
1+2+5  + 1+1+1+5 + data(要去掉头部)
FU indicator + FU header
type = 28 S E R TYPE

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

typedef struct 
{ 
	FILE *fp;							/*文件句柄*/
	unsigned char* 	nalu_buf;			/*nalu单元数据指针*/
	unsigned int 	nalu_len; 			/*nalu单元实际长度*/
}NALU_INFO; 

NALU_INFO nalu_info={0};


int nalu_info_init(NALU_INFO *nalu_info, char* file_name, int max_len)
{
	nalu_info->fp = fopen(file_name, "rb+");
	nalu_info->nalu_buf = (unsigned char*)calloc(max_len, 1);
	nalu_info->nalu_len = 0;

	return 0;
}

int nalu_info_free(NALU_INFO *nalu_info)
{
	free(nalu_info->nalu_buf);
	fclose(nalu_info->fp);

	return 0;
}


/*
输入一个文件指针,返回一个偏移,若到达文件末尾,返回一个负偏移
*/
long find_start_code(FILE *fp)
{
	unsigned char temp[4]={0};
	unsigned char read_bytes=0;
	
	while(!feof(fp))
	{	
		if(fread(temp, 1, 4, fp)<4)
		return -ftell(fp);

		/* xx 00 00 01 */
		if(temp[1]==0&&temp[2]==0&&temp[3]==1)
		return ftell(fp);
		else
		fseek(fp, -3, SEEK_CUR);

		memset(temp, 0, 4);
	}	
}


//-1:一个起始码也没有 0正常 1到结尾
int get_next_nalu(NALU_INFO *nalu_info)
{
	long start_offset = 0;
	long end_offset = 0;
	
	static char run_once=1;
	int exit = 0;

	/* 先把文件fp偏移到第一个起始码的位置,注意有可能一个起始码也没有 */
	if(run_once)
	{	
		if(find_start_code(nalu_info->fp)<0)
		{
			printf("not find start code!!!\n");
			return -1;
		}	
		run_once = 0; 	
	}

	/* 获取当前偏移,和下一个起始码的偏移 */
	start_offset = ftell(nalu_info->fp);
	end_offset = find_start_code(nalu_info->fp);
	
	/* 退出标志*/
	if(end_offset<0)
	{
		end_offset = -end_offset;
		exit = 1;
	}

	/* 这里realloc有几率会分配失败,不建议使用
	if(realloc(nalu_info->nalu_buf, end_offset-start_offset)==NULL)
	{
		printf("error");system("pause");
	}*/
	
	/* 数据拷贝 */
	fseek(nalu_info->fp, start_offset-end_offset, SEEK_CUR);
	fread(nalu_info->nalu_buf, end_offset-start_offset, 1, nalu_info->fp);	
	
	/* 计算长度 */
	if(exit)
	nalu_info->nalu_len = end_offset-start_offset;	
	else if(nalu_info->nalu_buf[end_offset-start_offset-4]==0)
	nalu_info->nalu_len = end_offset-start_offset-4;
	else nalu_info->nalu_len = end_offset-start_offset-3;
	
	if(exit) return 1;
	else	 return 0;	

}

/*--------------------------------------------------------------------------------------------*/

//正好12字节,不需要字节对齐操作,网络字节序采用大端模式,(低字节存放低位,高字节存放高位)
//但是x86cpu都是小端模式,所以需要转换
typedef struct 
{
	/*1byte*/
	unsigned char csrc_len:4;		/* CSRC计数 */	
	unsigned char extension:1;		/* 扩展标志 */
	unsigned char padding:1;		/* 填充标志 */
	unsigned char version:2;		/* 版本 */
	
	/*2byte*/
	unsigned char payload:7;		/* 有效荷载类型 */
	unsigned char mark:1;			/* 标记 */
	
	/*3-4byte 需要字节需转换*/
	unsigned short seq;				/* 序列号 */	
	
	/*5-8byte 需要字节需转换*/
	unsigned int time_stamp;		/* 时戳 */
	
	/*9-12byte 需要字节需转换*/
	unsigned int ssrc;				/* 同步信源SSRC */
	
	//unsigned int csrc;			/* 特约信源CSRC */
	
	/*rtp payload数据*/
	unsigned char rtp_buf[1460];	/* 最大1460 = 1500-20-8-12 */
}RTP_INFO;

RTP_INFO rtp_info={0};


int rtp_info_init(RTP_INFO* rtp_info)
{
	/*1byte*/
	rtp_info->version = 2;
	rtp_info->padding = 0;
	rtp_info->extension = 0;
	rtp_info->csrc_len = 0;
	
	/*2byte*/
	rtp_info->mark = 0;
	rtp_info->payload = 96;//H264
	
	/*3-4byte*/
	rtp_info->seq = htons(0x0);//htons转2字节
	
	/*5-8byte*/
	rtp_info->time_stamp = htonl(0x0);//htonl转4字节
	
	/*9-12byte 会话唯一*/
	rtp_info->ssrc = htonl(0x1a2b3c4d);

	/*rtp payload数据*/
	memset(rtp_info->rtp_buf, 0, 1460);
	
	return 0;
}

/*----------------------------------------------------------------------------*/

typedef struct 
{
	SOCKET sockc;
	sockaddr_in servaddr;
}NETWORK_INFO;

NETWORK_INFO network_info={0};

int network_info_init(NETWORK_INFO* network_info, char *ip, int port)
{
	WSADATA  wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	network_info->sockc = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	memset(&network_info->servaddr, 0, sizeof(sockaddr_in));
    network_info->servaddr.sin_family = AF_INET;                   
    network_info->servaddr.sin_addr.s_addr = inet_addr(ip);
    network_info->servaddr.sin_port = htons(port);
	
	connect(network_info->sockc, (sockaddr*)&network_info->servaddr, sizeof(sockaddr));
	
	return 0;
}

/*----------------------------------------------------------------------------*/

int rtp_for_h264_process(NETWORK_INFO* network_info, NALU_INFO* nalu_info, RTP_INFO* rtp_info, unsigned char* buf, int len);
int test_send_data_from_memery();


int main()
{
	//test_send_data_from_memery();

	int ret = 0;

	network_info_init(&network_info, "1.2.3.4", 1234);

	nalu_info_init(&nalu_info, "1.h264", 100000);

	rtp_info_init(&rtp_info);
	
	//-1 0 1
	while(1)
	{
		ret = get_next_nalu(&nalu_info);
		if(ret!=-1)
		{
			rtp_for_h264_process(&network_info, &nalu_info, &rtp_info, NULL, 0);
		}
		
		if(ret!=0)
		break;
	}
	
	nalu_info_free(&nalu_info);

	closesocket(network_info.sockc);
	
	system("pause");

	return 0;	
}


int rtp_for_h264_process(NETWORK_INFO* network_info, NALU_INFO* nalu_info, RTP_INFO* rtp_info, unsigned char* buf, int len)
{
	//dump_all_info(network_info, nalu_info, rtp_info);
	
	/* network info */
	SOCKET sockc 			  = network_info->sockc;
	sockaddr_in* servaddr 	  = &network_info->servaddr;
	
	/* nalu info */
	unsigned char* 	nalu_buf  = NULL;
	unsigned int 	nalu_len  = 0;
	if(len>0)
	{
		nalu_buf  = buf;
		nalu_len  = len;
	}
	else
	{
		nalu_buf  = nalu_info->nalu_buf;
		nalu_len  = nalu_info->nalu_len;
	}	
	
	
	/* rtp info */
	unsigned char mark   	  = rtp_info->mark;
	unsigned short seq 	   	  = htons(rtp_info->seq);
	unsigned int time_stamp   = htonl(rtp_info->time_stamp);
	unsigned char* rtp_buf 	  = rtp_info->rtp_buf;
	
	/* 单个nalu分片 */
	if(nalu_len<=1460)
	{
		rtp_info->mark = 1;
		rtp_info->seq = htons(++seq);
		rtp_info->time_stamp = htonl(time_stamp+23);//90000khz/30fps
		memcpy(rtp_buf, nalu_buf, nalu_len);

		sendto(sockc, (char*)rtp_info, 12+nalu_len, 0, (sockaddr*)servaddr, sizeof(sockaddr));
		
		return 0;
	}
	
	/* 分片分组 注意有两个字节的rtp payload头 */
	int count=(nalu_len-1)/(1460-2);
	int last_byte=(nalu_len-1)%(1460-2);
	int i=0;
	//printf("count:%d last_byte:%d\n", count, last_byte);
	
	for(i=0; i<count+1; i++)
	{
		if(i==0)/*第一片*/
		{
			rtp_info->mark = 0;
			rtp_info->seq = htons(++seq);
			rtp_info->time_stamp = htonl(time_stamp+300);
			
			rtp_buf[0] = nalu_buf[0];
			rtp_buf[1] = (1<<7) | (0<<6) | (0<<5) | (28<<0);
			
			memcpy(&rtp_buf[2], nalu_buf+1+1458*i, 1458);
			sendto(sockc, (char*)rtp_info, 12+2+1458, 0, (sockaddr*)servaddr, sizeof(sockaddr));
		}
		else if(i==count)/*最后一片*/
		{
			rtp_info->mark = 1;
			rtp_info->seq = htons(++seq);	

			rtp_buf[0] = nalu_buf[0];
			rtp_buf[1] = (0<<7) | (0<<6) | (1<<5) | (28<<0);
			
			memcpy(&rtp_buf[2], nalu_buf+1+1458*i, last_byte);
			sendto(sockc, (char*)rtp_info, 12+2+last_byte, 0, (sockaddr*)servaddr, sizeof(sockaddr));	
		}		
		else/*中间分片*/
		{
			rtp_info->mark = 0;
			rtp_info->seq = htons(++seq);		
			
			rtp_buf[0] = nalu_buf[0];
			rtp_buf[1] = (0<<7) | (0<<6) | (0<<5) | (28<<0);
			
			memcpy(&rtp_buf[2], nalu_buf+1+1458*i, 1458);
			sendto(sockc, (char*)rtp_info, 12+2+1458, 0, (sockaddr*)servaddr, sizeof(sockaddr));
		}		
	}
	return 0;
}


int test_send_data_from_memery()
{
	int i = 0;
	unsigned char* data_buf[2283]={NULL};
	int data_len[2283]={0};
	
	network_info_init(&network_info, "1.2.3.4", 1234);
	nalu_info_init(&nalu_info, "2.h264", 100000);
	rtp_info_init(&rtp_info);
	
	for(i=0; i<2283; i++)
	{
		get_next_nalu(&nalu_info);
		data_len[i] = nalu_info.nalu_len;
	
		data_buf[i] = (unsigned char*)malloc(data_len[i]);
		if(!data_buf[i])
		{
			printf("error\n");
			system("pause");
		}	

		memcpy(data_buf[i], nalu_info.nalu_buf, data_len[i]);
		printf("%d: addr:%p data:%x  %x\n",i, data_buf[i], data_buf[i][0], data_buf[i][data_len[i]-1]);	
	}

	for(i=0; i<2283; i++)
	{
		rtp_for_h264_process(&network_info, &nalu_info, &rtp_info, data_buf[i], data_len[i]);
		Sleep(30);
	}

	for(i=0; i<2283; i++)
	{
		free(data_buf[i]);
	}	
	printf("over\n");	
	system("pause");	
}


#if 0
int dump_all_info(NETWORK_INFO* network_info, NALU_INFO* nalu_info, RTP_INFO* rtp_info)
{
	printf("=========network_info=========\n");
	printf("sockc:%d\n", network_info->sockc);
	printf("servaddr:0x%p\n", &network_info->servaddr);
	printf("==========nalu_info===========\n");
	printf("FILE:0x%p\n", nalu_info->fp);
	printf("nalu_index:%d\n", nalu_info->nalu_index);
	printf("nalu_max_len:%d\n", nalu_info->nalu_max_len);	
	printf("nalu_header:%d\n", nalu_info->nalu_header);
	printf("nalu_buf:0x%p\n", nalu_info->nalu_buf);
	printf("nalu_len:%d\n", nalu_info->nalu_len);
	printf("fp_offset:%ld\n", nalu_info->fp_offset);
	printf("nalu_start_type:%d\n", nalu_info->nalu_start_type);
	printf("==========rtp_info============\n");
	printf("version:%d\n", rtp_info->version);
	printf("padding:%d\n", rtp_info->padding);
	printf("extension:%d\n", rtp_info->extension);
	printf("csrc_len:%d\n", rtp_info->csrc_len);
	printf("mark:%d\n", rtp_info->mark);
	printf("payload:%d\n", rtp_info->payload);
	printf("seq:%d\n", rtp_info->seq);
	printf("time_stamp:%d\n", rtp_info->time_stamp);
	printf("ssrc:%d\n", rtp_info->ssrc);
	printf("rtp_buf:0x%p\n", rtp_info->rtp_buf);
	printf("==============================\n");
	
	return 0;
}

int dump_nalu_info(NALU_INFO *nalu_info)
{
	unsigned int len = nalu_info->nalu_len;
	unsigned char nalu_header = nalu_info->nalu_header;
	
	printf("nalu_index:%d\n", nalu_info->nalu_index);
	printf("nalu_max_len:%d\n", nalu_info->nalu_max_len);	
	printf("nalu_header:%d\n", nalu_header);
	printf("nalu_buf[0]:%x  nalu_buf[%d-1]:%x\n", nalu_info->nalu_buf[0], len, nalu_info->nalu_buf[len-1]);
	printf("nalu_len:%d\n", len);
	printf("fp_offset:%ld\n", nalu_info->fp_offset);
	printf("nalu_start_type:%d\n", nalu_info->nalu_start_type);
	
	return 0;
}

#endif


















