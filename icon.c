/* vim: set sts=4 sw=4: */

#define WINVER 0x0501
#include <windows.h>
#include <stdio.h>
#include <string.h>

int editnpc = 0;

// void ReadPE();
static void RunPatch();
PROCESS_INFORMATION pinfo;

const char *fstring = "textparserdebug\r\ncreatehero\r\n";
const char *nstring = "editnpc 1\r\n";

static int Bailout(char *error) {
    if (pinfo.hProcess)
	TerminateProcess(pinfo.hProcess, 0);
    MessageBox(NULL, error, "Error!", MB_OK | MB_ICONEXCLAMATION);
    ExitProcess(0);
}

static int WBailout(char *error) {
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


int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance,
	PSTR szCmdParam, int iCmdShow)
{
    MSG msg;
    WNDCLASSEX wndclass;
    HANDLE h;
    DWORD didwrite;

    if (!stricmp(szCmdParam, "-n"))
	editnpc = 1;

    STARTUPINFO startup;
    memset(&startup, 0, sizeof(startup));
    startup.cb = sizeof(STARTUPINFO);
    memset(&pinfo, 0, sizeof(pinfo));

    if(!CreateProcess("cityofheroes.exe", "cityofheroes.exe -project coh -startupexec charcreate.txt", NULL, NULL, FALSE, CREATE_NEW_PROCESS_GROUP | CREATE_SUSPENDED | DETACHED_PROCESS, NULL, NULL, &startup, &pinfo)) {
	MessageBox(NULL, "Failed to launch process!", "Error", MB_OK | MB_ICONEXCLAMATION);
	return 0;
    }

    CreateDirectory("data", NULL);
    h = CreateFile("data\\charcreate.txt", GENERIC_WRITE,
	    0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
	    NULL);
    if (h == INVALID_HANDLE_VALUE)
	WBailout("Could not create data\\charcreate.txt");

    if (editnpc) {
	if (!WriteFile(h, nstring, strlen(nstring), &didwrite, NULL))
	    WBailout("Could not write to data\\charcreate.txt");
    }
    if (!WriteFile(h, fstring, strlen(fstring), &didwrite, NULL))
	WBailout("Could not write to data\\charcreate.txt");

    CloseHandle(h);

    //ReadPE();
    RunPatch();

    ResumeThread(pinfo.hThread);
    /*WaitForSingleObject(pinfo.hProcess, INFINITE); */
    return 0;
}

static unsigned int GetInt(unsigned int addr) {
    unsigned int rval;
    SIZE_T didread;

    if (!ReadProcessMemory(pinfo.hProcess, (LPVOID*)(addr), &rval, 4, &didread))
	WBailout("Couldn't read process memory");
    if (didread < 4)
	Bailout("Read less than expected!");

    return rval;
}

static void PutInt(unsigned int addr, unsigned int val) {
    DWORD didwrite = 0;
    if (!WriteProcessMemory(pinfo.hProcess, (LPVOID*)(addr), &val, 4, &didwrite))
	WBailout("Couldn't write process memory");
    if (didwrite < 4)
	Bailout("Wrote less than expected");
    return;
}

static void bmagic(unsigned int addr, int oldval, int newval) {
    if (GetInt(addr) != oldval)
	Bailout("Black magic failure!");
    PutInt(addr, newval);
}

static void bmagicc(unsigned int addr, int oldaddr, int oldalevel) {
    if (GetInt(addr) != oldaddr || GetInt(addr - 4) != oldalevel)
	Bailout("Black magic failure!");
    PutInt(addr - 4, 0);
}

static void PatchI24() {
    // exec
    bmagicc(0x00BE38BC, 0xa76044, 9);

    // startupexec
    bmagicc(0x00BE3968, 0xa76020, 9);

    // textparserdebug
    bmagicc(0x00BF457C, 0xa70090, 9);

    // CALL for textparserdebug
    bmagic(0x0041A621, 0x45940b, 0x0b1afb);

    // product published?
    bmagic(0x00830259, 0xc032cc33, 0x01b0cc33);

    // owns product?
    bmagic(0x0083147B, 0xff853a74, 0x5aeb01b0);

    // costume unlock BS
    //bmagic(0x0045828B, 0xc35bc032, 0xc35b01b0);
    bmagic(0x00458273, 0x950fc084, 0x950f91eb);
    bmagic(0x00458206, 0xcccccccc, 0x75433e81);
    bmagic(0x0045820a, 0xcccccccc, 0x6e757473);
    bmagic(0x0045820e, 0x5553cccc, 0x555368eb);

    // don't show "hide store pieces" box
    bmagic(0x00719FE5, 2, 1);

    if (editnpc) {
	bmagicc(0x00BE4020, 0xa75dd4, 9);

	// don't skip origin menu
	bmagic(0x0077E255, 0x3d833574, 0x3d8335eb);

	// don't skip playstyle menu
	bmagic(0x0077ECFC, 0x35891274, 0x358912eb);

	// don't skip archetype menu
	bmagic(0x0076D222, 0x3d833074, 0x3d8330eb);

	// don't skip power selection
	bmagic(0x0078151F, 0x03da840f, 0x0003dbe9);
	bmagic(0x00781523, 0x74a10000, 0x74a19000);
    }
}

static void PatchI23() {
    // exec
    bmagicc(0x00BE1528, 0xa77f64, 9);

    // startupexec
    bmagicc(0x00BE15D4, 0xa77f40, 9);

    // textparserdebug
    bmagicc(0x00BF213C, 0xa72010, 9);

    // CALL for textparserdebug
    bmagic(0x0041A581, 0x45cddb, 0x0b0d9b);

    // product published?
    bmagic(0x00832f09, 0xc032cc33, 0x01b0cc33);

    // owns product?
    bmagic(0x0083408B, 0xff853a74, 0x5aeb01b0);

    // costume unlock BS
    bmagic(0x00458183, 0x950fc084, 0x950f91eb);
    bmagic(0x00458116, 0xcccccccc, 0x75433e81);
    bmagic(0x0045811a, 0xcccccccc, 0x6e757473);
    bmagic(0x0045811e, 0x5553cccc, 0x555368eb);

    // don't show "hide store pieces" box
    bmagic(0x0071A095, 2, 1);

    if (editnpc) {
	bmagicc(0x00BE1C8C, 0xa77cf4, 9);

	// don't skip origin menu
	bmagic(0x00780A05, 0x3d833574, 0x3d8335eb);

	// don't skip playstyle menu
	bmagic(0x007814AD, 0x35891274, 0x358912eb);

	// don't skip archetype menu
	bmagic(0x0076F9F2, 0x3d833074, 0x3d8330eb);

	// don't skip power selection
	bmagic(0x00783DC0, 0x03da840f, 0x0003dbe9);
	bmagic(0x00783DC4, 0x8ca10000, 0x8ca19000);
    }
}

static void RunPatch() {
    unsigned int v;

    if (GetInt(0x00BE15D4) == 0xa77f40)
	PatchI23();
    else if (GetInt(0x00BE38BC) == 0xa76044)
	PatchI24();
    else
	Bailout("Unrecognized version.");
}
