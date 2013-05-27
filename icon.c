/* vim: set sts=4 sw=4 et: */

#define WINVER 0x0501
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "icon.h"
#include "code.h"
#include "data.h"
#include "strings.h"
#include "patch.h"
#include "util.h"

int editnpc = 0;
int random = 0;

static void RunPatch();
static void PromptUserForCohLocation();
PROCESS_INFORMATION pinfo;

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance,
	PSTR szCmdParam, int iCmdShow)
{
    if (!stricmp(szCmdParam, "-n"))
	editnpc = 1;
    if (!stricmp(szCmdParam, "-r"))
	random = 1;

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

    RunPatch();

    ResumeThread(pinfo.hThread);
    return 0;
}

static void RunPatch() {
    int vers = 0;

    if (GetInt(0x00BE15D4) == 0xa77f40)
	vers = 23;
    else if (GetInt(0x00BE38BC) == 0xa76044)
	vers = 24;
    else
	Bailout("Sorry, your cityofheroes.exe file is not a supported version.");

    InitCoh(vers);

    WriteStrings();
    WriteData();
    RelocateCode();
    FixupCode(vers);
    WriteCode();

    if (vers == 23)
	PatchI23();
    else if (vers == 24)
	PatchI24();
}

static void PromptUserForCohLocation() {
    OPENFILENAME ofn;
    char szFile[1024];

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
