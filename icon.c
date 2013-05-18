/* vim: set sts=4 sw=4: */

#define WINVER 0x0501
#include <windows.h>
#include <stdio.h>
#include <string.h>

int editnpc = 0;

// void ReadPE();
static void RunPatch();
static void PromptUserForCohLocation();
PROCESS_INFORMATION pinfo;

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

    // First check to see if the file exists.
    //
    while (GetFileAttributes("cityofheroes.exe") == INVALID_FILE_ATTRIBUTES) {
	PromptUserForCohLocation();
    }

    STARTUPINFO startup;
    memset(&startup, 0, sizeof(startup));
    startup.cb = sizeof(STARTUPINFO);
    memset(&pinfo, 0, sizeof(pinfo));

    if(!CreateProcess("cityofheroes.exe", "cityofheroes.exe -project coh -noverify", NULL, NULL, FALSE, CREATE_NEW_PROCESS_GROUP | CREATE_SUSPENDED | DETACHED_PROCESS, NULL, NULL, &startup, &pinfo)) {
	MessageBox(NULL, "Failed to launch process!", "Error", MB_OK | MB_ICONEXCLAMATION);
	return 0;
    }

    // delete old crap previous version used
    if (GetFileAttributes("data\\charcreate.txt"))
	DeleteFile("data\\charcreate.txt");

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
    DWORD oldprotect;
    VirtualProtectEx(pinfo.hProcess, (LPVOID*)(addr), 4, PAGE_READWRITE, &oldprotect);

    if (!WriteProcessMemory(pinfo.hProcess, (LPVOID*)(addr), &val, 4, &didwrite))
	WBailout("Couldn't write process memory");
    if (didwrite < 4)
	Bailout("Wrote less than expected");
    VirtualProtectEx(pinfo.hProcess, (LPVOID*)(addr), 4, oldprotect, &oldprotect);
    return;
}

static void PutStr(unsigned int addr, char *str, int len) {
    DWORD didwrite = 0;
    DWORD oldprotect;
    VirtualProtectEx(pinfo.hProcess, (LPVOID*)(addr), len, PAGE_READWRITE, &oldprotect);
    if (!WriteProcessMemory(pinfo.hProcess, (LPVOID*)(addr), str, len, &didwrite))
	WBailout("Couldn't write process memory");
    if (didwrite < len)
	Bailout("Wrote less than expected");
    VirtualProtectEx(pinfo.hProcess, (LPVOID*)(addr), len, oldprotect, &oldprotect);
    return;
}

static void bmagic(unsigned int addr, int oldval, int newval) {
    if (GetInt(addr) != oldval)
	Bailout("Sorry, your cityofheroes.exe file is not a supported version.");
    PutInt(addr, newval);
}

static void bmagicc(unsigned int addr, int oldaddr, int oldalevel) {
    if (GetInt(addr) != oldaddr || GetInt(addr - 4) != oldalevel)
	Bailout("Sorry, your cityofheroes.exe file is not a supported version.");
    PutInt(addr - 4, 0);
}

static void bmagics(unsigned int addr, int oldval, char *str, int size) {
    if (GetInt(addr) != oldval)
	Bailout("Sorry, your cityofheroes.exe file is not a supported version.");
    PutStr(addr, str, size);
}

static char strovr1[] = "maps/City_Zones/City_01_01/City_01_01.txt_33\0maps/City_Zones/V_City_03_02/V_City_03_02.txt\0maps/City_Zones/City_00_01/City_00_01.txt\0maps/City_Zones/City_02_04/City_02_04.txt";
static char strovr2[] = "maps/City_Zones/P_City_00_01/P_City_00_01.txt\0maps/City_Zones/P_City_00_02/P_City_00_02.txt";
static char strovr3[] = "EMOTE_HOLD_TORCH";

static float spawncoords[] =
{
    -63.7, 0, 157.7,		// Outbreak
    137.5, 0.3, -112.0,		// Atlas
    2696.3, 16.0, 8071.1,	// Nerva
    762.2, 148.0, -931.5,	// Pocket D
    -4665.8, 40.5, -253.0,	// Nova
    679.5, 109.0, 3202.5,	// Imperial
};

static unsigned char i24_sandbox[] = {
0x8B, 0x0D, 0xF4, 0x95, 0xBB, 0x00, 0x8B, 0x04, 0x8D, 0x50, 0xFD, 0xA7, 0x00, 0xE8, 0x33, 0x90,
0x06, 0x00, 0xA1, 0x80, 0xF5, 0xCA, 0x00, 0x8B, 0x80, 0x00, 0x0E, 0x00, 0x00, 0x50, 0x8D, 0x90,
0xA4, 0x00, 0x00, 0x00, 0xE8, 0x5C, 0x96, 0xFC, 0xFF, 0x58, 0x8D, 0x90, 0x3C, 0x04, 0x00, 0x00,
0xE8, 0x50, 0x96, 0xFC, 0xFF, 0x8B, 0x15, 0x80, 0xF5, 0xCA, 0x00, 0x8B, 0x52, 0x30, 0x83, 0xBA,
0xC8, 0x5A, 0x00, 0x00, 0x00, 0x75, 0x17, 0x52, 0x68, 0x00, 0x30, 0x00, 0x00, 0x6A, 0x01, 0xE8,
0xAD, 0x9C, 0x50, 0x00, 0x83, 0xC4, 0x08, 0x5A, 0x89, 0x82, 0xC8, 0x5A, 0x00, 0x00, 0xE8, 0xE2,
0xC9, 0x0F, 0x00, 0xBF, 0x38, 0xF5, 0xCA, 0x00, 0xB8, 0x00, 0x00, 0x80, 0x3F, 0xB9, 0x0F, 0x00,
0x00, 0x00, 0xF3, 0xAB, 0x8B, 0x3D, 0x80, 0xF5, 0xCA, 0x00, 0x8B, 0x7F, 0x2C, 0x8D, 0xBF, 0xBC,
0x00, 0x00, 0x00, 0xB9, 0x0A, 0x00, 0x00, 0x00, 0xF3, 0xAB, 0x8D, 0x15, 0x64, 0x13, 0x67, 0x01,
0xC7, 0x02, 0x00, 0x00, 0x40, 0x41, 0xA1, 0xF4, 0x95, 0xBB, 0x00, 0xBA, 0x0C, 0x00, 0x00, 0x00,
0xF7, 0xE2, 0x8D, 0x90, 0x68, 0xFD, 0xA7, 0x00, 0x8B, 0x0D, 0x80, 0xF5, 0xCA, 0x00, 0xE8, 0xD2,
0x70, 0xFE, 0xFF, 0xE9, 0x0C, 0x03, 0x00, 0x00,

0xF6, 0x44, 0xE4, 0x0C, 0x80, 0x74, 0x0C, 0x60, 0x83, 0xFE, 0x02, 0x74, 0x12,
0x83, 0xFE, 0x03, 0x74, 0x7B, 0x61, 0xA1, 0x64, 0x7F, 0xE3, 0x00, 0xC3, 0x90, 0x90, 0x90, 0x90,
0x90, 0x90, 0x8B, 0x3D, 0x80, 0xF5, 0xCA, 0x00, 0x8B, 0x7F, 0x2C, 0x8D, 0xBF, 0xA8, 0x00, 0x00,
0x00, 0xBD, 0x38, 0xF5, 0xCA, 0x00, 0xB1, 0x01, 0xB3, 0x08, 0x30, 0x4F, 0x3C, 0x84, 0x4F, 0x3C,
0x74, 0x0F, 0x08, 0x5D, 0x3C, 0xB8, 0x00, 0x00, 0x20, 0x41, 0xBB, 0x00, 0x00, 0xA0, 0x40, 0xEB,
0x0C, 0xF6, 0xD3, 0x20, 0x5D, 0x3C, 0xB8, 0x00, 0x00, 0x80, 0x3F, 0x89, 0xC3, 0x89, 0x5F, 0x0C,
0x89, 0x47, 0x28, 0x89, 0x47, 0x2C, 0x89, 0x5F, 0x38, 0xEB, 0xAA, 0x90, 0x90, 0x90, 0x90, 0x90,
0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
0x83, 0xEC, 0x04, 0x8D, 0x04, 0xE4, 0x8B, 0x3D, 0x80, 0xF5, 0xCA, 0x00, 0x8B, 0x57, 0x28, 0x8B,
0x92, 0x60, 0x01, 0x00, 0x00, 0x50, 0x52, 0x68, 0xB0, 0xFD, 0xA7, 0x00, 0xE8, 0x9F, 0xCF, 0x0C,
0x00, 0x83, 0xC4, 0x0C, 0x84, 0xC0, 0x74, 0x06, 0x0F, 0xB7, 0x04, 0xE4, 0xEB, 0x02, 0x31, 0xC0,
0x89, 0x87, 0x28, 0x19, 0x00, 0x00, 0xD9, 0x05, 0xB4, 0x62, 0xA6, 0x00, 0xD9, 0x9F, 0x58, 0x01,
0x00, 0x00, 0x83, 0xC4, 0x04, 0xE9, 0x3B, 0xFF, 0xFF, 0xFF, 0x90, 0x90,
};

static void PatchI24() {
    // product published?
    bmagic(0x00830259, 0xc032cc33, 0x01b0cc33);

    // owns product?
    bmagic(0x0083147B, 0xff853a74, 0x5aeb01b0);

    // create character
    bmagic(0x0083B246, 0x0410ec81, 0xc90ed5e8);
    bmagic(0x0083B24a, 0x84a10000, 0xbb80e8ff);
    bmagic(0x0083B24e, 0x3300b1ba, 0x05c7ffee);
    bmagic(0x0083B252, 0x248489c4, 0x0167c800);
    bmagic(0x0083B256, 0x0000040c, editnpc);
    bmagic(0x0083B25a, 0xc6c83d80, 0xc35dec89);

    // costume unlock BS
    bmagic(0x00458273, 0x950fc084, 0x950f91eb);
    bmagic(0x00458206, 0xcccccccc, 0x75433e81);
    bmagic(0x0045820a, 0xcccccccc, 0x6e757473);
    bmagic(0x0045820e, 0x5553cccc, 0x555368eb);

    // don't show "hide store pieces" box
    bmagic(0x00719FE5, 2, 1);

    if (editnpc) {
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

    // "Sandbox Mode" stuff below

    // String tables
    bmagics(0x00A7D8FA, 0x6b614d28, strovr1, sizeof(strovr1));
    bmagics(0x00A7FCE8, 0x20756f59, strovr2, sizeof(strovr2));
    bmagics(0x00A7FDB0, 0x656e616d, strovr3, sizeof(strovr3));
    bmagic(0x00A7FD50, 0x6e726157, 0x00a7d955);
    bmagic(0x00A7FD54, 0x21676e69, 0x00a7d8fa);
    bmagic(0x00A7FD58, 0x6c654420, 0x00a7d927);
    bmagic(0x00A7FD5c, 0x6e697465, 0x00a7d97f);
    bmagic(0x00A7FD60, 0x20612067, 0x00a7fce8);
    bmagic(0x00A7FD64, 0x7262696c, 0x00a7fd16);
    bmagics(0x00A7FD68, 0X20797261, (char*)spawncoords, sizeof(spawncoords));

    // always create new character
    bmagic(0x004cc116, 1, 0);

    // NOP out checks on char create screen
    bmagic(0x0077DE41, 0xF3C41D89, 0x90909090);
    bmagic(0x0077DE45, 0x3D83012D, 0x3D839090);

    // NOP out comm check
    bmagic(0x00409332, 0x5E0C053B, 0x90909090);
    bmagic(0x00409336, 0xC01B0168, 0x90909090);

    // always return 1 for connected
    bmagic(0x0040DA1D, 3, 1);

    // ignore check for mapserver in main loop
    bmagic(0x00838249, 0x3d392c77, 0x3d392ceb);

    // nocoll command
    bmagic(0x00BD12A4, 1, 0);

    // write big assembly blob
    bmagics(0x004CC60B, 0x67150ca1, i24_sandbox, sizeof(i24_sandbox));

    // hook keyboard
    bmagic(0x005C94E0, 0xE37F64A1, 0xF031DEE8);
    bmagic(0x005C94E4, 0x8B555300, 0x8B5553FF);

}

static void PatchI23() {
    // product published?
    bmagic(0x00832f09, 0xc032cc33, 0x01b0cc33);

    // owns product?
    bmagic(0x0083408B, 0xff853a74, 0x5aeb01b0);

    // create character
/*    bmagic(0x0083DDD6, 0x0410ec81, 0x89a405c7);
    bmagic(0x0083DDDA, 0x84a10000, editnpc ? 0x00010168 : 0x00000167);
    bmagic(0x0083DDDE, 0x3300b1ba, 0x3be80000);
    bmagic(0x0083DDE2, 0x248489c4, 0xe8ffc8d5);
    bmagic(0x0083DDE6, 0x0000040c, 0xffee9216);
    bmagic(0x0083DDEA, 0x88683d80, 0xc35dec89); */
    bmagic(0x0083DDD6, 0x0410ec81, 0xc8d545e8);
    bmagic(0x0083DDDA, 0x84a10000, 0x9220e8ff);
    bmagic(0x0083DDDE, 0x3300b1ba, 0x05c7ffee);
    bmagic(0x0083DDE2, 0x248489c4, 0x016789a4);
    bmagic(0x0083DDE6, 0x0000040c, editnpc);
    bmagic(0x0083DDEA, 0x88683d80, 0xc35dec89);

    // costume unlock BS
    bmagic(0x00458183, 0x950fc084, 0x950f91eb);
    bmagic(0x00458116, 0xcccccccc, 0x75433e81);
    bmagic(0x0045811a, 0xcccccccc, 0x6e757473);
    bmagic(0x0045811e, 0x5553cccc, 0x555368eb);

    // don't show "hide store pieces" box
    bmagic(0x0071A095, 2, 1);

    if (editnpc) {
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
	Bailout("Sorry, your cityofheroes.exe file is not a supported version.");
}

static void PromptUserForCohLocation() {
    OPENFILENAME ofn;
    char szFile[1024];
    HANDLE h;

    MessageBox(NULL, "We couldn't find a cityofheroes.exe file in the current directory.\n\nPlease select the game installation that you wish to use.", "Titan Icon", MB_OK);

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = szFile;
    ofn.lpstrFile[0] = 0;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Cityofheroes.exe\0cityofheroes.exe\0";
    ofn.nFilterIndex = 0;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_DONTADDTORECENT;

    if (GetOpenFileName(&ofn) == FALSE)
	exit(0);


}
