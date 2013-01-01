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
    if (!WriteProcessMemory(pinfo.hProcess, (LPVOID*)(addr), &val, 4, &didwrite))
	WBailout("Couldn't write process memory");
    if (didwrite < 4)
	Bailout("Wrote less than expected");
    return;
}

static void bmagic(unsigned int addr, int oldval, int newval) {
    if (GetInt(addr) != oldval) {
	char blah[512];
	snprintf(blah, 512, "%x", addr);
	MessageBox(NULL, blah, "Blah", MB_OK);
	Bailout("Sorry, your cityofheroes.exe file is not a supported version.");
    }
    PutInt(addr, newval);
}

static void bmagicc(unsigned int addr, int oldaddr, int oldalevel) {
    if (GetInt(addr) != oldaddr || GetInt(addr - 4) != oldalevel)
	Bailout("Sorry, your cityofheroes.exe file is not a supported version.");
    PutInt(addr - 4, 0);
}

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
