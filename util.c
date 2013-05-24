/* vim: set sts=4 sw=4 et: */

#define WINVER 0x0501
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "icon.h"

int Bailout(char *error) {
    if (pinfo.hProcess)
	TerminateProcess(pinfo.hProcess, 0);
    MessageBox(NULL, error, "Error!", MB_OK | MB_ICONEXCLAMATION);
    ExitProcess(0);
}

int WBailout(char *error) {
    char buffer[1024];
    DWORD err = GetLastError();

    if (pinfo.hProcess)
	TerminateProcess(pinfo.hProcess, 0);
    char *errormsg;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
	    | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err,
	    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&errormsg, 0,
	    NULL);
    strncpy(buffer, error, sizeof(buffer));
    buffer[sizeof(buffer)-1] = 0;
    strncat(buffer, ": ", sizeof(buffer) - strlen(buffer) - 1);
    strncat(buffer, errormsg, sizeof(buffer) - strlen(buffer) - 1);
    MessageBox(NULL, buffer, "Error!", MB_OK | MB_ICONEXCLAMATION);
    ExitProcess(0);
}

unsigned int GetInt(unsigned int addr) {
    unsigned int rval;
    SIZE_T didread;

    if (!ReadProcessMemory(pinfo.hProcess, (LPVOID*)(addr), &rval, 4, &didread))
	WBailout("Couldn't read process memory");
    if (didread < 4)
	Bailout("Read less than expected!");

    return rval;
}

void PutData(unsigned int addr, const void *data, int len) {
    DWORD didwrite = 0;
    DWORD oldprotect;
    VirtualProtectEx(pinfo.hProcess, (LPVOID*)(addr), len, PAGE_READWRITE, &oldprotect);
    if (!WriteProcessMemory(pinfo.hProcess, (LPVOID*)(addr), data, len, &didwrite))
	WBailout("Couldn't write process memory");
    if (didwrite < len)
	Bailout("Wrote less than expected");
    VirtualProtectEx(pinfo.hProcess, (LPVOID*)(addr), len, oldprotect, &oldprotect);
}

void PutInt(unsigned int addr, unsigned int val) {
    PutData(addr, &val, sizeof(val));
}

void bmagic(unsigned int addr, int oldval, int newval) {
    if (GetInt(addr) != oldval)
	Bailout("Sorry, your cityofheroes.exe file is not a supported version.");
    PutInt(addr, newval);
}

// Technically gets it wrong if dest is more than 2GB away, but shouldn't
// happen due to kernel/userspace split
int CalcRelAddr(unsigned int addr, unsigned int dest) {
    return (int)((long long int)dest - ((long long int)addr + 4));
}

// ONLY USE THIS WITH OPCODES THAT HAVE A 4-BYTE DISP!!!
void PutRelAddr(unsigned int addr, unsigned int dest) {
    PutInt(addr, CalcRelAddr(addr, dest));
}

// This writes 5 bytes at addr
void PutCall(unsigned int addr, unsigned int dest) {
    unsigned char temp[5];
    temp[0] = 0xE5;
    *(unsigned int*)(temp + 1) = CalcRelAddr(addr + 1, dest);
    PutData(addr, temp, 5);
}
