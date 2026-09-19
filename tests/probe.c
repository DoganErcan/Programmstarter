#define WIN32_LEAN_AND_MEAN
#include <windows.h>
int WINAPI wWinMain(HINSTANCE a, HINSTANCE b, PWSTR c, int d)
{
    (void)a; (void)b; (void)c; (void)d;
    HANDLE file=CreateFileW(L"gestartet.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return 1;
    DWORD written;WriteFile(file,"OK",2,&written,NULL);CloseHandle(file);return 0;
}
