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

#if 1//����������ʽ	
	
	//�򿪷�����ƹ��������ݿ�
	SC_HANDLE shOSCM = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if(shOSCM == NULL)
	{
		return 0;
		//printf("111\n");
	}

	//��������
	SC_HANDLE shCS = CreateService(shOSCM,
						"ftp_client",//sc delete ftp_client 
						"ftp_client",//����������ʾ������,����
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
	
	//cmd����ԱȨ��ɾ������
	//sc delete ftp_client 
	
	//�򿪷���
	//shCS = OpenService(shOSCM, "ftp_client", SERVICE_ALL_ACCESS);
	//if(shCS == NULL)
	//{
	//	CloseServiceHandle(shOSCM);
	//	printf("333\n");
	//}
	//��ʼ����
	StartService(shCS, 0, NULL);
	//���Ʒ���
	//SERVICE_STATUS ss;
	//ControlService(shCS, SERVICE_CONTROL_STOP, &ss);
	//ɾ������
	//DeleteService(shCS);
	
	if(shOSCM)
	CloseServiceHandle(shOSCM);

	if(shCS)
	CloseServiceHandle(shCS);

#else//ע���������ʽ	
 
	char regname[]="Software\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Run";
    HKEY hkResult;
	int ret=0;
	
    ret=RegOpenKey(HKEY_LOCAL_MACHINE,regname,&hkResult);//Ҫ�򿪼��ľ��,Ҫ���Ӽ������ֵĵ�ַ,Ҫ�򿪼��ľ���ĵ�ַ
	
    ret=RegSetValueEx(	hkResult,
						"run_ftp"/*ע������*/,
						0,
						REG_EXPAND_SZ,
						(unsigned char *)"D:\\QQFtp\\ftp_client.exe",/*��������·��*/
						23
						);
  
	if(ret==0)				
	{
		RegCloseKey(hkResult);
		//printf("ok\n");
	} 
	
	//ɾ������		
	//ret=RegDeleteValue(hkResult, "run_ftp");
	
	ShellExecute(0, "open", "cmd.exe", "/C D:\\QQFtp\\ftp_client.exe", NULL, SW_HIDE);
	
#endif	
	
    return 0;
}

int copy_file()
{
	//��ɾ���ļ���
	system("rd /q /s D:\\QQFtp");
	
	//�����ļ���
	mkdir("D:\\QQFtp");

	//copy�ļ�
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

	//��������
	system("attrib +h D:\\QQFtp");
	
	return 0;
}


