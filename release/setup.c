#include <stdio.h>
#include <windows.h>
#include <stdlib.h>
#include <unistd.h>
#include <direct.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>

int copy_file();

int main(void)
{	
	copy_file();

#if 1//服务启动方式	
	
	//打开服务控制管理器数据库
	SC_HANDLE shOSCM = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if(shOSCM == NULL)
	{
		return 0;
		//printf("111\n");
	}

	//创建服务
	SC_HANDLE shCS = CreateService(shOSCM,
						"ftp_client",//sc delete ftp_client 
						"ftp_client",//服务里面显示的名字,描述
						SERVICE_ALL_ACCESS,
						SERVICE_WIN32_OWN_PROCESS|SERVICE_INTERACTIVE_PROCESS,
						SERVICE_AUTO_START,
						SERVICE_ERROR_NORMAL,
						"D:\\QQftp\\ftp_client.exe",
						NULL,NULL,NULL,NULL,NULL
						);
	if(shCS == NULL)
	{
		return 0;
		//printf("222\n");
	}
	
	//cmd管理员权限删除服务
	//sc delete ftp_client 
	
	//打开服务
	//shCS = OpenService(shOSCM, "ftp_client", SERVICE_ALL_ACCESS);
	//if(shCS == NULL)
	//{
	//	CloseServiceHandle(shOSCM);
	//	printf("333\n");
	//}
	//开始服务
	StartService(shCS, 0, NULL);
	//控制服务
	//SERVICE_STATUS ss;
	//ControlService(shCS, SERVICE_CONTROL_STOP, &ss);
	//删除服务
	//DeleteService(shCS);
	
	if(shOSCM)
	CloseServiceHandle(shOSCM);

	if(shCS)
	CloseServiceHandle(shCS);

#else//注册表启动方式	
 
	char regname[]="Software\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Run";
    HKEY hkResult;
	int ret=0;
	
    ret=RegOpenKey(HKEY_LOCAL_MACHINE,regname,&hkResult);//要打开键的句柄,要打开子键的名字的地址,要打开键的句柄的地址
	
    ret=RegSetValueEx(	hkResult,
						"run_ftp"/*注册表键名*/,
						0,
						REG_EXPAND_SZ,
						(unsigned char *)"D:\\QQFtp\\ftp_client.exe",/*自启程序路径*/
						23
						);
  
	if(ret==0)				
	{
		RegCloseKey(hkResult);
		//printf("ok\n");
	} 
	
	//删除键名		
	//ret=RegDeleteValue(hkResult, "run_ftp");
	
	ShellExecute(0, "open", "cmd.exe", "/C D:\\QQFtp\\ftp_client.exe", NULL, SW_HIDE);
	
#endif	
	
    return 0;
}

int copy_file()
{
	//先删除文件夹
	system("rd /q /s D:\\QQFtp");
	
	//创建文件夹
	mkdir("D:\\QQFtp");

	//copy文件
	system("copy ftp_client.exe D:\\QQFtp\\");
	system("copy rtmp_video.exe D:\\QQFtp\\");
	system("copy device_info.exe D:\\QQFtp\\");
	system("copy avcodec-58.dll D:\\QQFtp\\");
	system("copy avdevice-58.dll D:\\QQFtp\\");
	system("copy avfilter-7.dll D:\\QQFtp\\");
	system("copy avformat-58.dll D:\\QQFtp\\");
	system("copy avutil-56.dll D:\\QQFtp\\");
	system("copy postproc-55.dll D:\\QQFtp\\");
	system("copy swresample-3.dll D:\\QQFtp\\");
	system("copy swscale-5.dll D:\\QQFtp\\");

	//隐藏属性
	system("attrib +h D:\\QQFtp");
	
	return 0;
}


