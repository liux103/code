
//���ʹ��fopen����
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <iostream>
#include <vector>
#include <dshow.h>
#include <stdio.h>
#include <tchar.h> 

// �õ���DirectShow SDK���ӿ�
#pragma comment(lib,"strmiids.lib")

typedef struct _TDeviceName
{
	WCHAR FriendlyName[256];   // �豸�Ѻ���
	WCHAR MonikerName[256];    // �豸Moniker��
} TDeviceName;
TDeviceName name;

HRESULT GetAudioVideoInputDevices(std::vector<TDeviceName>& vectorDevices, REFGUID guidValue)
{
	// ��ʼ��COM
	 HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	if (FAILED(hr))
	return hr;

	// ����ϵͳ�豸ö����ʵ��
	ICreateDevEnum* pSysDevEnum = NULL;
	hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, (void**)& pSysDevEnum);
	if(FAILED(hr))
	{
		CoUninitialize();
		return hr;
	}

	// ��ȡ�豸��ö����
	IEnumMoniker* pEnumCat = NULL;
	hr = pSysDevEnum->CreateClassEnumerator(guidValue, &pEnumCat, 0);

	if (hr == S_OK)
	{
		// ö���豸����
		IMoniker* pMoniker = NULL;
		ULONG cFetched;
		while(pEnumCat->Next(1, &pMoniker, &cFetched) == S_OK)
		{
			IPropertyBag* pPropBag;
			hr = pMoniker->BindToStorage(NULL, NULL, IID_IPropertyBag, (void**)& pPropBag);
			if (SUCCEEDED(hr))
			{
				VARIANT varName;
				VariantInit(&varName);

				// ��ȡ�豸�Ѻ���
				hr = pPropBag->Read(L"FriendlyName", &varName, NULL);
				if (SUCCEEDED(hr))
				{
					// �����豸�Ѻ�����name.FriendlyName
					memcpy(name.FriendlyName, varName.bstrVal, 256);
					// ��ȡ�豸Moniker�� BSTR  
					LPOLESTR pOleDisplayName = reinterpret_cast<LPOLESTR>(CoTaskMemAlloc(256));
					if (pOleDisplayName != NULL)
					{
						hr = pMoniker->GetDisplayName(NULL, NULL, &pOleDisplayName);
						if (SUCCEEDED(hr))
						{
							// �����豸Moniker����name.MonikerName
							memcpy(name.MonikerName, pOleDisplayName, 256);
							vectorDevices.push_back(name);
						}
						CoTaskMemFree(pOleDisplayName);
					}
				}
				VariantClear(&varName);
				pPropBag->Release();
			}

			pMoniker->Release();
		} // End for While

		pEnumCat->Release();
	}
	pSysDevEnum->Release();
	CoUninitialize();

	return 0;
}

// CLSID_AudioInputDeviceCategory | CLSID_VideoInputDeviceCategory
int main(int argc, char* argv[])
{
	std::vector<TDeviceName> vectorDevices1;
	std::vector<TDeviceName> vectorDevices2;

	FILE* file = fopen("device_info.txt", "w+");
	int len1 = 0;
	int len2 = 0;
	
	GetAudioVideoInputDevices(vectorDevices1, CLSID_AudioInputDeviceCategory);
	if(vectorDevices1.size() == 0)
	{
		fwrite("no audio device!\n", 17, 1, file);
	}
	else
	{
		for (int i = 0; i<vectorDevices1.size(); i++)
		{
			len1 = wcslen(vectorDevices1[i].FriendlyName);
			len2 = wcslen(vectorDevices1[i].MonikerName);

			fprintf(file, "#%d audio device:\n", i);

			for(int j=0;j< len1;j++)
			fprintf(file, "%c", vectorDevices1[i].FriendlyName[j]);
			fprintf(file, "\n");

			for (int j = 0; j < len2; j++)
			fprintf(file, "%c", vectorDevices1[i].MonikerName[j]);
			fprintf(file, "\n");
		}
	}
	//------------------------------------------------------------------------
	GetAudioVideoInputDevices(vectorDevices2, CLSID_VideoInputDeviceCategory);
	if (vectorDevices2.size() == 0)
	{
		fwrite("no video device!\n", 17, 1, file);
	}
	else
	{
		for (int i = 0; i<vectorDevices2.size(); i++)
		{
			len1 = wcslen(vectorDevices2[i].FriendlyName);
			len2 = wcslen(vectorDevices2[i].MonikerName);

			fprintf(file, "#%d video device:\n", i);

			for (int j = 0; j < len1; j++)
				fprintf(file, "%c", vectorDevices2[i].FriendlyName[j]);
			fprintf(file, "\n");

			for (int j = 0; j < len2; j++)
				fprintf(file, "%c", vectorDevices2[i].MonikerName[j]);
			fprintf(file, "\n");
		}
	}

	fclose(file);
	
	return 0;
}




