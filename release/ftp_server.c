#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <WinSock.h>
#include <Winsock2.h>
#include <windows.h>
#include <conio.h>

#define PORT 	1518
#define SERVER_IP 	"47.105.37.18"

char root_path[1024]={0};//绝对路径


void cmd_ls(SOCKET sockc);
void cmd_cd(SOCKET sockc);
void cmd_upload(SOCKET sockc);
void cmd_download(SOCKET sockc);
void cmd_quit(SOCKET sockc);
void cmd_help();
int  cmd_process();
void cmd_pwd(SOCKET sockc);
void show_path();
void cmd_fdisk(SOCKET sockc);
void cmd_rm(SOCKET sockc);
void cmd_touch(SOCKET sockc);
void cmd_mkdir(SOCKET sockc);
void cmd_audio(SOCKET sockc);
void cmd_video(SOCKET sockc);
void cmd_del_service(SOCKET sockc);	

typedef struct cmd_data
{
	int cmd_type;
	char cmd_data[100];
	
}CMD_DATA;
CMD_DATA cmd_data={0};

/*------------------------------------*/
DWORD WINAPI boom_task(LPVOID);
DWORD WINAPI server_process(LPVOID);
/*------------------------------------*/

int main()
{
    DWORD  main_threadId;
	DWORD  boom_threadId;	
	
    CreateThread(NULL, 0, boom_task, 0, 0, &main_threadId);
	CreateThread(NULL, 0, server_process, 0, 0, &boom_threadId); 
	
	while(1)
	{
		Sleep(5);
	}		
	
	return 0;
}

DWORD WINAPI boom_task(LPVOID p)
{       
	printf("boom_task = %d\n", GetCurrentThreadId()); 
	
	WSADATA  wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	
	struct sockaddr_in boom_addr;
	SOCKET boom_socket;

    boom_addr.sin_family = AF_INET;                   
    boom_addr.sin_addr.s_addr = htonl(INADDR_ANY);     
    boom_addr.sin_port = htons(1519);

    SOCKET new_socket = socket(AF_INET,SOCK_STREAM,0);      

    bind(new_socket,(struct sockaddr*)&boom_addr, sizeof(boom_addr)); 
	
    listen(new_socket, 1);    
	
	struct sockaddr_in boom_client; 
	int addr_len = sizeof(sockaddr_in);  
	boom_socket = accept(new_socket, (struct sockaddr*)&boom_client, &addr_len);  
	
	char buf[20]={0};
	recv(boom_socket, buf, 20, 0);    
	printf("%s\n", buf);
	memset(buf,0,20);
    /*-------------------------------------------------------*/
	
	while(1)
	{
		Sleep(5);
		buf[0]=0;	
		recv(boom_socket, buf, 1, 0);   
		if(buf[0]=='#')
		{
			send(boom_socket, "*", 1, 0); 
			buf[0]==0;
		}			
	}


	return 0;

}

DWORD WINAPI server_process(LPVOID p)
{
	printf("main_task = %d\n", GetCurrentThreadId());  
	
	//初始化
	system("md download && md upload");
	
	WSADATA  wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	
	struct sockaddr_in servaddr;                      
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;                   
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);     
    servaddr.sin_port = htons(PORT);

    SOCKET socks = socket(AF_INET,SOCK_STREAM,0);      

    bind(socks,(struct sockaddr*)&servaddr, sizeof(servaddr)); 
	
    listen(socks, 10);    
	
	struct sockaddr_in addrClient; 
	int addr_len = sizeof(sockaddr_in);  
	SOCKET sockc;

	printf("ftp run...\n");
	sockc = accept(socks, (struct sockaddr*)&addrClient, &addr_len);  
	printf("%s:%d\n",inet_ntoa(addrClient.sin_addr), addrClient.sin_port);

	/************************************/
	recv(sockc, root_path, 1024, 0);    
	printf("root path:%s\n", root_path);
	/************************************/		
	
	while(1)
	{
		Sleep(5);
		show_path();
		
		cmd_process();
		
		switch(cmd_data.cmd_type)
		{
			case 1:
				cmd_help();
				break;	
			case 2:
				send(sockc, "quit", strlen("quit"), 0);  
				cmd_quit(sockc);
				break;
			case 3:
				cmd_upload(sockc);
				break;
			case 4:
				send(sockc, "down", strlen("down"), 0);  
				cmd_download(sockc);
				break;
			case 5:
				send(sockc, "cd", strlen("cd"), 0);  
				cmd_cd(sockc);
				break;				
			case 6:
				send(sockc, "ls", strlen("ls"), 0);  
				cmd_ls(sockc);
				break;
			case 7:
				send(sockc, "pwd", strlen("pwd"), 0);   
				cmd_pwd(sockc);
				break;		
			case 8:
				send(sockc, "fdisk", strlen("fdisk"), 0); 
				cmd_fdisk(sockc);
				break;	
			case 9:
				send(sockc, "rm", strlen("rm"), 0);  
				cmd_rm(sockc);
				break;	
			case 10:
				send(sockc, "touch", strlen("touch"), 0); 
				cmd_touch(sockc);
				break;	
			case 11:
				send(sockc, "mkdir", strlen("mkdir"), 0);  
				cmd_mkdir(sockc);
				break;		
			case 13:
				send(sockc, "audio", strlen("audio"), 0); 
			case 14:
				send(sockc, "del_service", strlen("del_service"), 0);  
				cmd_del_service(sockc);
			case 15:
				send(sockc, "video", strlen("video"), 0);
				cmd_video(sockc);					
				break;						
			default:
				break;	
		}
		
	}	
	closesocket(sockc); 
	closesocket(socks); 
    system("pause");
    return 0;
}

void show_path()
{
	int path_len = strlen(root_path)-1;
	
	if(root_path[path_len] == '\\')
	{
		printf("[liux@ftp %s] #", root_path);
	}
	else
	{
		while(root_path[path_len] != '\\')
		{
			path_len--;
		}
		printf("[liux@ftp %s] #", &(root_path[path_len+1]));
	}
	return ;
}

int cmd_process()
{
	char cmd_buf[100]={0};
	int cmd_buf_len=0;
	int cmd_flag=0;
	
	memset(&cmd_data, 0, sizeof(cmd_data));

	fgets(cmd_buf, 100, stdin);
	
	//去掉尾部回车
	cmd_buf[strlen(cmd_buf)-1]=0;
	
	//只输入回车则退出
	cmd_buf_len = strlen(cmd_buf);
	if(cmd_buf_len==0)
	{
		return 0;
	}	
	
	//首尾是空格则退出
	if(cmd_buf[cmd_buf_len-1]==' ' || cmd_buf[0]==' ')
	{
		return 0;
	}

	cmd_buf_len--;

	//按空格分割命令与参数
	while(cmd_buf[cmd_buf_len] != ' ')
	{
		cmd_buf_len--;

		if(cmd_buf_len < 0)
		{
			cmd_flag = 1;
			break;
		}
	}

	if(cmd_flag)//没有空格
	{
		if(memcmp(cmd_buf, "ls", (strlen(cmd_buf)>2 ? strlen(cmd_buf):2))==0){cmd_data.cmd_type=6;}
		else if(memcmp(cmd_buf, "quit", (strlen(cmd_buf)>4 ? strlen(cmd_buf):4))==0){cmd_data.cmd_type=2;}
		else if(memcmp(cmd_buf, "help", (strlen(cmd_buf)>4 ? strlen(cmd_buf):4))==0){cmd_data.cmd_type=1;}
		else if(memcmp(cmd_buf, "pwd", (strlen(cmd_buf)>3 ? strlen(cmd_buf):3))==0){cmd_data.cmd_type=7;}
		else if(memcmp(cmd_buf, "del_service", (strlen(cmd_buf)>11 ? strlen(cmd_buf):11))==0){cmd_data.cmd_type= 14;}
		else{cmd_data.cmd_type=0;}
		
		//printf("cmd flag:1##type:%d\n", cmd_data.cmd_type);
	}
	else
	{
		if(memcmp(cmd_buf, "cd", (cmd_buf_len>2 ? cmd_buf_len:2))==0){cmd_data.cmd_type= 5;}
		else if(memcmp(cmd_buf, "up", (cmd_buf_len>2 ? cmd_buf_len:2))==0){cmd_data.cmd_type= 3;}
		else if(memcmp(cmd_buf, "down", (cmd_buf_len>4 ? cmd_buf_len:4))==0){cmd_data.cmd_type= 4;}
		else if(memcmp(cmd_buf, "fdisk", (cmd_buf_len>5 ? cmd_buf_len:5))==0){cmd_data.cmd_type= 8;}
		else if(memcmp(cmd_buf, "rm", (cmd_buf_len>2 ? cmd_buf_len:2))==0){cmd_data.cmd_type= 9;}
		else if(memcmp(cmd_buf, "touch", (cmd_buf_len>5 ? cmd_buf_len:5))==0){cmd_data.cmd_type= 10;}
		else if(memcmp(cmd_buf, "mkdir", (cmd_buf_len>5 ? cmd_buf_len:5))==0){cmd_data.cmd_type= 11;}
		else if(memcmp(cmd_buf, "audio", (cmd_buf_len>5 ? cmd_buf_len:5))==0){cmd_data.cmd_type= 13;}
		else if(memcmp(cmd_buf, "video", (cmd_buf_len>5 ? cmd_buf_len:5))==0){cmd_data.cmd_type= 15;}
		else{cmd_data.cmd_type=0;}
		
		memcpy(&(cmd_data.cmd_data[0]), &cmd_buf[cmd_buf_len+1], strlen(&cmd_buf[cmd_buf_len+1]));
		
		//printf("cmd flag:0##type:%d\n", cmd_data.cmd_type);
		//printf("%s\n", &(cmd_data.cmd_data[0]));
	}
		//printf("type:data--%d:%s\n", cmd_data.cmd_type,&(cmd_data.cmd_data[0]));

	return 0;
}

void cmd_video(SOCKET sockc)
{
	char buf[1024+1]={0};

	//video device+video=HP_Webcam-101+rtmp://169.254.96.183:1935/live/home
	
	send(sockc, &cmd_data.cmd_data[0], strlen(&cmd_data.cmd_data[0]), 0);
	
	recv(sockc, buf, 1024, 0);

	printf("%s\n", buf);
	
	return;
}

void cmd_ls(SOCKET sockc)
{
	char dir_list[1024*50]={0};

	//一次性接收到dir_list,可能越界
	recv(sockc, dir_list, 1024*50, 0);	
	printf("%s", dir_list);

	//目录可能太多显示不全
	if(strlen(dir_list)==1024*50)
	{
		printf("-----------------------\n");
	}
	
	return ;
}

void cmd_cd(SOCKET sockc)
{
	char buf[1024+1]={0};

	//发送cd目录
	send(sockc, &(cmd_data.cmd_data[0]), strlen(&(cmd_data.cmd_data[0])), 0); 

	recv(sockc, buf, 1024, 0);
	
	//目录不存在
	if(memcmp(buf, "no such directory!", 18)==0)
	{
		printf("no such directory && chdir error!\n", buf);
		return ;
	}

	//更新路径
	memset(root_path,0,strlen(root_path));
	memcpy(root_path, buf, strlen(buf));
	
	return ;
}

void cmd_upload(SOCKET sockc)
{
	FILE* fp=NULL;
	struct stat file_info={0};
	int send_size=0;
	int read_count=0;
	unsigned int all_size=0;
	char buf[1024+1]={0};	
	unsigned int n=0;
	
	//绝对路径
	memcpy(buf, ".\\upload\\", strlen(".\\upload\\"));
	memcpy(&buf[strlen(".\\upload\\")], &(cmd_data.cmd_data[0]), strlen(&(cmd_data.cmd_data[0])));

	//打开文件
	fp=fopen(buf,"rb");
	if(fp == NULL)
	{
		printf("no such file!\n"); 
		return ;
	}
	
	//发起upload
	send(sockc, "up", strlen("up"), 0);  
	
	//发送文件名
	send(sockc, &(cmd_data.cmd_data[0]), strlen(&(cmd_data.cmd_data[0])), 0); 
	Sleep(1000);
	
	//发送文件大小
	stat(buf,&file_info);	
	if(file_info.st_size<0)
	{
		all_size = (4294967295+file_info.st_size+1);
	}
	else
	{
		all_size = file_info.st_size;
	}	
	send(sockc, (char*)(&all_size), 4, 0); 
	
	//文件大小为0
	if(file_info.st_size==0)
	{
		printf("up:0/0  100%%\r");	
		printf("\n");
		fclose(fp);
		return;
	}

	//文件大小不为0
	while(!feof(fp))
	{
		memset(buf, 0, 1024+1);	
		read_count=fread(buf, 1, 1024, fp);
		send_size=send(sockc, buf, read_count, 0); 	
		printf("up:%u/%u  %d%%\r", n+send_size, all_size, (int)((float(n+send_size)/all_size)*100));	
		n = n+send_size;
	}
	printf("\n");
	
	fclose(fp);		
}

void cmd_download(SOCKET sockc)
{
	FILE* fp=NULL;
	char buf[1024+1]={0};
	int get_size=0;
	unsigned int file_size = 0;
	unsigned int all_size=0;

	//发送文件名
	send(sockc, &(cmd_data.cmd_data[0]), strlen(&(cmd_data.cmd_data[0])), 0); 
	
	//文件是否存在
	 recv(sockc, buf, 1024, 0);
	 if(memcmp(buf, "no such file!", 13)==0)
	 {
		printf("%s\n", buf);
		return ;
	 }
	//接受文件大小
	recv(sockc, (char*)(&file_size), 4, 0);	
	all_size = file_size;

	//文件存在,创建(会覆盖)
	memset(buf, 0, 1024+1);
	memcpy(buf, ".\\download\\", strlen(".\\download\\"));
	memcpy(&buf[strlen(".\\download\\")], &(cmd_data.cmd_data[0]), strlen(&(cmd_data.cmd_data[0])));
	
	fp=fopen(buf,"wb+");
	
	//文件大小为0
	if(file_size==0)
	{
		printf("up:0/0  100%%\r");	
		printf("\n");
		fclose(fp);
		return;
	}
	
	//文件大小不为0
	while(file_size)
	{
		memset(buf, 0, 1024+1);
		get_size = recv(sockc, buf, 1024, 0);
		fwrite(buf, get_size ,1 ,fp);	
		file_size = file_size-get_size;
		printf("down:%u/%u  %d%%\r", all_size-file_size, all_size, (int)((float(all_size-file_size)/all_size)*100));	
	}
	printf("\n");
	fclose(fp); 
}

void cmd_audio(SOCKET sockc)
{

	
} 

void cmd_rm(SOCKET sockc)
{
	send(sockc, &(cmd_data.cmd_data[0]), strlen(&(cmd_data.cmd_data[0])), 0); 
}
void cmd_touch(SOCKET sockc)
{
	send(sockc, &(cmd_data.cmd_data[0]), strlen(&(cmd_data.cmd_data[0])), 0); 	
}
void cmd_mkdir(SOCKET sockc)
{
	send(sockc, &(cmd_data.cmd_data[0]), strlen(&(cmd_data.cmd_data[0])), 0); 	
}

void cmd_fdisk(SOCKET sockc)
{
	char buf[1024+1]={0};
	
	send(sockc, &(cmd_data.cmd_data[0]), strlen(&(cmd_data.cmd_data[0])), 0); 
	
	recv(sockc, buf, 1024, 0);
	
	if(memcmp(buf, "no fdisk", 8)==0)
	{
		printf("%s!\n", buf);
	}
	else
	{
		memset(root_path, 0, 1024);
		memcpy(root_path, buf, strlen(buf));
	}
	
	return ;
}

void cmd_quit(SOCKET sockc)
{
	Sleep(1000);
	printf("please kill ftp_server!\n");
	while(1)
	{
		Sleep(5);
	}
	//system("taskkill /F /IM ftp_server.exe");
	
	return ;
}

void cmd_del_service(SOCKET sockc)
{
	Sleep(1000);
	printf("service delete!\n");
	while(1)
	{
		Sleep(5);
	}
	
	return ;	
}

void cmd_pwd(SOCKET sockc)
{
	char buf[1024+1]={0};

	recv(sockc, buf, 1024, 0);	
	
	//更新路径
	memset(root_path,0,strlen(root_path));
	memcpy(root_path, buf, strlen(buf));
	
	printf("%s\n", root_path);
	
	return ;
}

void cmd_help()
{
	printf("ls               --Dir & file\n"); 
	printf("cd               --Toggle Path\n"); 
	printf("pwd              --Absolute path\n"); 
	printf("down             --Download file\n"); 
	printf("up               --Upload file\n"); 
	printf("audio 1/2        --Show info|camera*\n"); 
	printf("fdisk            --Change path\n"); 
	printf("rm               --Rm file\n"); 
	printf("touch            --Create file\n"); 
	printf("mkdir            --Mkdir file\n"); 
	printf("del_service       --del_service\n"); 
	printf("quit             --Quit ftp\n");
	printf("video device+video=HP_Webcam-101+rtmp://169.254.96.183:1935/live/home\n");
	
	return ;
}















