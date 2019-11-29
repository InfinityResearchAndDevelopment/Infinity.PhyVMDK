#include <vector>
#include <cstdio>
#include <utility>
#include <Windows.h>
#include <wincon.h>
#include <io.h>
#include <fcntl.h>
#include <Setupapi.h>
#include <Ntddstor.h>
#include <iostream>
#include <string>
#include <devguid.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <tchar.h>
#include <initguid.h>
#include <devpropdef.h>
#include <devpkey.h>
#include "Infinity.PhyVMDK.h"
using namespace std;
#pragma comment (lib, "setupapi.lib")

struct DriveInfo{
    unsigned int num;
    uint64_t size;
    wchar_t desc[4096];
};

wstring ReadRegValue(HKEY root,LPCWSTR key,LPCWSTR name){
    HKEY hKey;
    if(RegOpenKeyEx(root,key,0,KEY_QUERY_VALUE|KEY_WOW64_64KEY,&hKey)!=ERROR_SUCCESS)
        throw "Could not open registry key";

    DWORD type;
    DWORD cbData;
    if(RegQueryValueEx(hKey,name,NULL,&type,NULL,&cbData)!=ERROR_SUCCESS){
        RegCloseKey(hKey);
        throw "Could not read registry value";
    }

    if(type!=REG_SZ){
        RegCloseKey(hKey);
        throw "Incorrect registry value type";
    }

    wstring value(cbData/sizeof(wchar_t),L'\0');
    if(RegQueryValueEx(hKey,name,NULL,NULL,reinterpret_cast<LPBYTE>(&value[0]),&cbData)!=ERROR_SUCCESS){
        RegCloseKey(hKey);
        throw "Could not read registry value";
    }

    RegCloseKey(hKey);

    size_t firstNull=value.find_first_of(L'\0');
    if(firstNull!=string::npos)
        value.resize(firstNull);

    return value;
}

int main(int argc,char** argv){
    bool bConsole;
    vector<DriveInfo> drives;
    FreeConsole();
    bConsole=AttachConsole(ATTACH_PARENT_PROCESS)!=FALSE;
    if(bConsole){
        HANDLE hStdOut=GetStdHandle(STD_OUTPUT_HANDLE);
        int fd=_open_osfhandle((intptr_t)hStdOut,_O_TEXT);
        if(fd>0){
            *stdout=*_fdopen(fd,"w");
            setvbuf(stdout,NULL,_IONBF,0);
        }
    }
    DEVPROPTYPE ulPropertyType;
    DWORD dwSize;
    TCHAR szDeviceInstanceID[520];
    HDEVINFO diskClassDevices;
    GUID diskClassDeviceInterfaceGuid=GUID_DEVINTERFACE_DISK;
    SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
    PSP_DEVICE_INTERFACE_DETAIL_DATA deviceInterfaceDetailData;
    DWORD requiredSize;
    DWORD deviceIndex;
    SP_DEVINFO_DATA DeviceInfoData;
    HANDLE disk=INVALID_HANDLE_VALUE;
    STORAGE_DEVICE_NUMBER diskNumber;
    GET_LENGTH_INFORMATION diskSize;
    DWORD bytesReturned;
    CONFIGRET status;
    string sizes[5]={"B", "KB", "MB", "GB", "TB"};

    START_ERROR_CHK();

    diskClassDevices=SetupDiGetClassDevs(&diskClassDeviceInterfaceGuid,
                                         NULL,
                                         NULL,
                                         DIGCF_PRESENT|
                                         DIGCF_DEVICEINTERFACE);
    CHK(INVALID_HANDLE_VALUE!=diskClassDevices,
        "SetupDiGetClassDevs");
    unsigned i;

    WCHAR szBuffer[4096];
    ZeroMemory(&deviceInterfaceData,sizeof(SP_DEVICE_INTERFACE_DATA));
    deviceInterfaceData.cbSize=sizeof(SP_DEVICE_INTERFACE_DATA);
    deviceIndex=0;
    wprintf(L"Welcome to Infinity.PhyVMDK v1.0.0.0\n");
    wprintf(L"  This utility will create a VMDK Disk Image\n  that points to a Physical Drive.\n\n");
    wprintf(L"    Drive Path           Size       Description\n");
    //wprintf(L"%d:  \\\\?\\PhysicalDrive%d  %8.3f%hs  %s\n",i+1,diskNumber.DeviceNumber,len,tmp.c_str(),szBuffer);
    for(i=0; ; i++){
        DeviceInfoData.cbSize=sizeof(DeviceInfoData);
        if(!SetupDiEnumDeviceInfo(diskClassDevices,i,&DeviceInfoData))
            break;
        status=CM_Get_Device_ID(DeviceInfoData.DevInst,szDeviceInstanceID,MAX_PATH,0);
        if(status!=CR_SUCCESS)
            continue;
        if(SetupDiGetDeviceProperty(diskClassDevices,&DeviceInfoData,&DEVPKEY_Device_BusReportedDeviceDesc,&ulPropertyType,(BYTE*)szBuffer,sizeof(szBuffer),&dwSize,0)){
            if(SetupDiGetDeviceProperty(diskClassDevices,&DeviceInfoData,&DEVPKEY_Device_BusReportedDeviceDesc,&ulPropertyType,(BYTE*)szBuffer,sizeof(szBuffer),&dwSize,0)){

            }
                //_tprintf(TEXT("    Bus Reported Device Description: \"%ls\"\n"));
  
        }
        SetupDiEnumDeviceInterfaces(diskClassDevices,
                                    NULL,
                                    &diskClassDeviceInterfaceGuid,
                                    (DWORD)i,
                                    &deviceInterfaceData);

        SetupDiGetDeviceInterfaceDetail(diskClassDevices,
                                        &deviceInterfaceData,
                                        NULL,
                                        0,
                                        &requiredSize,
                                        NULL);
        CHK(ERROR_INSUFFICIENT_BUFFER==GetLastError(),
            "SetupDiGetDeviceInterfaceDetail - 1");

        deviceInterfaceDetailData=(PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(requiredSize);
        CHK(NULL!=deviceInterfaceDetailData,
            "malloc");

        ZeroMemory(deviceInterfaceDetailData,requiredSize);
        deviceInterfaceDetailData->cbSize=sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        CHK(SetupDiGetDeviceInterfaceDetail(diskClassDevices,
            &deviceInterfaceData,
            deviceInterfaceDetailData,
            requiredSize,
            NULL,
            NULL),
            "SetupDiGetDeviceInterfaceDetail - 2");

        disk=CreateFile(deviceInterfaceDetailData->DevicePath,
                        GENERIC_READ,
                        FILE_SHARE_READ|FILE_SHARE_WRITE,
                        NULL,
                        OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL,
                        NULL);
        CHK(INVALID_HANDLE_VALUE!=disk,
            "CreateFile");
        CHK(DeviceIoControl(disk,
            IOCTL_STORAGE_GET_DEVICE_NUMBER,
            NULL,
            0,
            &diskNumber,
            sizeof(STORAGE_DEVICE_NUMBER),
            &bytesReturned,
            NULL),
            "IOCTL_STORAGE_GET_DEVICE_NUMBER");
        CHK(DeviceIoControl(disk,
            IOCTL_DISK_GET_LENGTH_INFO,
            NULL,
            0,
            &diskSize,
            sizeof(GET_LENGTH_INFORMATION),
            &bytesReturned,
            NULL),
            "IOCTL_DISK_GET_LENGTH_INFO");
        CloseHandle(disk);
        disk=INVALID_HANDLE_VALUE;
        float len=(float)diskSize.Length.QuadPart;
        unsigned int order=0;
        while(len>=1024&&order<5-1){
            order++;
            len=len/1024;
        }
        string tmp=sizes[order];
        wprintf(L"%d:  \\\\?\\PhysicalDrive%d  %8.3f%hs  %s\n",i+1,diskNumber.DeviceNumber,len,tmp.c_str(),szBuffer);
        drives.push_back(DriveInfo());
        drives[i].num=diskNumber.DeviceNumber;
        drives[i].size=diskSize.Length.QuadPart;
        wcscpy_s(drives[i].desc,szBuffer);
    }
    CHK(ERROR_NO_MORE_ITEMS==GetLastError(),"SetupDiEnumDeviceInterfaces");
    END_ERROR_CHK();
    wprintf(L"%d:  Exit\n\n",i);
    if(INVALID_HANDLE_VALUE!=diskClassDevices){
        SetupDiDestroyDeviceInfoList(diskClassDevices);
    }
    if(INVALID_HANDLE_VALUE!=disk){
        CloseHandle(disk);
    }
    unsigned int choice=0;
    do{
        cout<<":";
        cin>>choice;
    } while(choice>i+1);
    if(choice==i+1)
        exit(0);
    char consent;
    cout<<endl;
    if(choice<=i+1)
        do{
            cout<<"WARNING: This may lead to irreversible dataloss and/or system instability!"<<endl;
            cout<<"  Do you wish to continue? [Y/N]:";
            cin>>consent;
        } while(consent!='Y'&&consent!='N');
        if(consent=='N'){
            exit(2);
        }
        cout<<endl;
    wstring vbmPath=ReadRegValue(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Oracle\\VirtualBox",L"InstallDir");
    wstring cmd=vbmPath+L"\\VBoxManage.exe internalcommands createrawvmdk -filename \""+(drives[choice-1].desc)+L".vmdk\" -rawdisk \\\\.\\PhysicalDrive"+to_wstring(drives[choice-1].num);
    const size_t stringSize=1000;
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    DWORD exit_code;
    ZeroMemory(&si,sizeof(si));
    si.cb=sizeof(si);
    ZeroMemory(&pi,sizeof(pi));
    if(!CreateProcess(NULL,&cmd[0],NULL,NULL,FALSE,0,NULL,NULL,&si,&pi))
        return -1;
    WaitForSingleObject(pi.hProcess,INFINITE);
    GetExitCodeProcess(pi.hProcess,&exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return exit_code+2;
}
