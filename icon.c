/* vim: set sts=4 sw=4 et: */

#define WINVER 0x0501
#include <windows.h>
#include <stdio.h>
#include <string.h>

int editnpc = 0;
int random = 0;

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

    if(!CreateProcess("cityofheroes.exe", "cityofheroes.exe -project coh -noverify -usetexenvcombine", NULL, NULL, FALSE, CREATE_NEW_PROCESS_GROUP | CREATE_SUSPENDED | DETACHED_PROCESS, NULL, NULL, &startup, &pinfo)) {
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

#if 0
// Not used anymore
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
#endif

static void adjcall(unsigned int addr, unsigned int olddest, unsigned int newdest) {
    unsigned int temp;
    int disp = newdest - olddest;

    temp = GetInt(addr);
    temp += disp;
    PutInt(addr, temp);
}

/* static void fixupbase(unsigned int addr, unsigned int newbase) {
    int oldaddr = GetInt(addr);
    newbase += oldaddr;
    PutInt(addr, newbase);
} */

// Technically gets it wrong if dest is more than 2GB away, but shouldn't
// happen due to kernel/userspace split
static int relcall(unsigned int addr, unsigned int dest) {
    return (int)((long long int)dest - ((long long int)addr + 4));
}

// ONLY USE THIS WITH OPCODES THAT HAVE A 4-BYTE DISP!!!
static void PutRelCall(unsigned int addr, unsigned int dest) {
    PutInt(addr, relcall(addr, dest));
}

static DWORD iconStrBase = 0;
static DWORD iconDataBase = 0;
static DWORD iconCodeBase = 0;

#define ICON_STR_SIZE 4096
#define ICON_DATA_SIZE 4096
#define ICON_CODE_SIZE 16384

static char *icon_strs[] = {
    "Noclip enabled",	            // 0
    "Noclip disabled",	            // 1
    "Enter a map file name:",	    // 2
    "Enter a MOV name:",	    // 3
    "Enter an FX name:",	    // 4
    "Camera detached",		    // 5
    "Camera reattached",	    // 6
    "EMOTE_HOLD_TORCH",		    // 7
    "maps/City_Zones/City_00_01/City_00_01.txt",     // 8
    "maps/City_Zones/City_01_01/City_01_01.txt_33",  // 9
    "maps/City_Zones/V_City_03_02/V_City_03_02.txt", // 10
    "maps/City_Zones/City_02_04/City_02_04.txt",     // 11
    "maps/City_Zones/P_City_00_01/P_City_00_01.txt", // 12
    "maps/City_Zones/P_City_00_02/P_City_00_02.txt", // 13
    0
};

enum {
    STR_NOCLIP_ON,
    STR_NOCLIP_OFF,
    STR_MAPFILE,
    STR_MOV,
    STR_FX,
    STR_CAMERADETACH,
    STR_CAMERAATTACH,
    STR_HOLDTORCH,
    STR_MAP_OUTBREAK,
    STR_MAP_ATLAS,
    STR_MAP_NERVA,
    STR_MAP_POCKETD,
    STR_MAP_NOVA,
    STR_MAP_IMPERIAL,
};

enum {
    DATA_ZONE_MAP,
    DATA_SPAWNCOORDS,
    DATA_KBHOOKS,
};

enum {
    CODE_ENTER_GAME,
    CODE_GENERIC_MOV,
    CODE_KEY_HOOK,
    CODE_KEY_FLY,
    CODE_KEY_TORCH,
    CODE_KEY_NOCOLL,
    CODE_KEY_SEEALL,
    CODE_KEY_DETACH,
    CODE_KEY_LOADMAP,
    CODE_KEY_LOADMAP_CB,
    CODE_KEY_MOV,
    CODE_KEY_MOV_CB,
    CODE_KEY_FX,
    CODE_KEY_FX_CB,
    CODE_KEY_KILLFX,
    CODE_KEY_SPAWN,
    CODE_KEY_SPAWNCB,
    CODE_KEY_KILLENT,
    CODE_KEY_TARGETMODE,
    CODE_FRAME_HOOK,
};

static DWORD *iconStrOffsets = 0;
static DWORD *iconDataOffsets = 0;

static float spawncoords[] =
{
    -63.7, 0, 157.7,		// Outbreak
    137.5, 0.3, -112.0,		// Atlas
    2696.3, 16.0, 8071.1,	// Nerva
    762.2, 148.0, -931.5,	// Pocket D
    -4665.8, 40.5, -253.0,	// Nova
    679.5, 109.0, 3202.5,	// Imperial
};

static unsigned char code_enter_game[] = {
0x60,		                    // 0000 PUSHAD
0x8B,0x0D,0x00,0x00,0x00,0x00,      // 0001 MOV ECX,DWORD PTR [$start_choice]
				    // 0003 ***FIXUP*** COH absvar
0x8B,0x04,0x8D,0x00,0x00,0x00,0x00, // 0007 MOV EAX,DWORD PTR DS:[ECX*4+$STR]
				    // 000A ***FIXUP*** Icon absvar
0xE8,0x00,0x00,0x00,0x00,           // 000E CALL $load_map_demo
				    // 000F ***FIXUP*** COH relcall
0xA1,0x00,0x00,0x00,0x00,	    // 0013 MOV EAX,DWORD PTR [$player_ent]
				    // 0014 ***FIXUP*** COH absvar
0x8B,0x80,0x00,0x00,0x00,0x00,      // 0018 MOV EAX,DWORD PTR DS:[EAX+$charoff]
				    // 001A ***FIXUP*** offset
0x50,                               // 001E PUSH EAX
0x8D,0x90,0xA4,0x00,0x00,0x00,      // 001F LEA EDX,[EAX+0A4]
0xE8,0x00,0x00,0x00,0x00,           // 0025 CALL $copy_attribs
				    // 0026 ***FIXUP*** COH relcall
0x58,                               // 002A POP EAX
0x8D,0x90,0x3C,0x04,0x00,0x00,      // 002B LEA EDX,[EAX+43C]
0xE8,0x00,0x00,0x00,0x00,           // 0031 CALL $copy_attribs
                                    // 0032 ***FIXUP*** COH relcall
0x8B,0x15,0x00,0x00,0x00,0x00,      // 0036 MOV EDX,DWORD PTR [$player_ent]
				    // 0038 ***FIXUP*** COH absvar
0x8B,0x52,0x30,                     // 003C MOV EDX,DWORD PTR DS:[EDX+30]
0x83,0xBA,0x00,0x00,0x00,0x00,0x00, // 003F CMP DWORD PTR DS:[EDX+$kboff],0
				    // 0041 ***FIXUP*** offset
0x75,0x17,                          // 0046 JNE SHORT 005F
0x52,                               // 0048 PUSH EDX
0x68,0x00,0x30,0x00,0x00,           // 0049 PUSH 3000
0x6A,0x01,                          // 004E PUSH 1
0xE8,0xB7,0x62,0x86,0x00,           // 0050 CALL $calloc
                                    // 0051 ***FIXUP*** COH relcall
0x83,0xC4,0x08,                     // 0055 ADD ESP,8
0x5A,                               // 0058 POP EDX
0x89,0x82,0x00,0x00,0x00,0x00,      // 0059 MOV DWORD PTR DS:[EDX+$kboff],EAX
                                    // 005B ***FIXUP*** offset
0xE8,0x00,0x00,0x00,0x00,           // 005F CALL $init_keybinds
                                    // 0060 ***FIXUP*** COH relcall

// control state stuff
0xBF,0x00,0x00,0x00,0x00,	    // 0064 MOV EDI,OFFSET $controls_from_server
                                    // 0065 ***FIXUP*** COH absvar
0xB8,0x00,0x00,0x80,0x3F,           // 0069 MOV EAX,3F800000 (1.f)
0xB9,0x0F,0x00,0x00,0x00,           // 006E MOV ECX,0F
0xF3,0xAB,                          // 0073 REP STOS DWORD PTR ES:[EDI]
// surface physics in entity
0x8B,0x3D,0x00,0x00,0x00,0x00,      // 0075 MOV EDI,DWORD PTR DS:[$player_ent]
                                    // 0077 ***FIXUP*** COH absvar
0x8B,0x7F,0x2C,                     // 007B MOV EDI,DWORD PTR DS:[EDI+2C]
0x8D,0xBF,0xBC,0x00,0x00,0x00,      // 007E LEA EDI,[EDI+0BC]
0xB9,0x0A,0x00,0x00,0x00,           // 0084 MOV ECX,0A
0xF3,0xAB,                          // 0089 REP STOS DWORD PTR ES:[EDI]

// set the clock
0x8D,0x15,0x00,0x00,0x00,0x00,      // 008B LEA EDX,[$game_time]
                                    // 008D ***FIXUP*** COH absvar
0xC7,0x02,0x00,0x00,0x40,0x41,      // 0091 MOV DWORD PTR DS:[EDX],41400000

// set player spawn point
0xA1,0x00,0x00,0x00,0x00,           // 0097 MOV EAX,DWORD PTR [$start_choice]
                                    // 0098 ***FIXUP*** COH absvar
0xBA,0x0C,0x00,0x00,0x00,           // 009C MOV EDX,0C
0xF7,0xE2,                          // 00A1 MUL EDX
0x8D,0x90,0x00,0x00,0x00,0x00,      // 00A3 LEA EDX,[EAX+$spawncoords]
                                    // 00A5 ***FIXUP*** Icon absvar
0x8B,0x0D,0x00,0x00,0x00,0x00,      // 00A9 MOV ECX,DWORD PTR [$player_ent]
                                    // 00AB ***FIXUP*** COH absvar
0xE8,0x00,0x00,0x00,0x00,           // 00AF CALL $entity_setpos
                                    // 00B0 ***FIXUP*** COH relcall

// set a database ID
0xA1,0x00,0x00,0x00,0x00,	    // 00B4 MOV EAX,DWORD PTR [$player_ent]
				    // 00B5 ***FIXUP*** COH absvar
0x8D,0x50,0x74,                     // 00B9 LEA EDX,[EAX+74]
0xC7,0x02,0x01,0x00,0x00,0x00,      // 00BC MOV DWORD PTR DS:[EDX], 1
0x8D,0x90,0x00,0x00,0x00,0x00,	    // 00C2 LEA EDX,[EAX+$svrid]
				    // 00C4 ***FIXUP*** offset
0xC7,0x02,0x01,0x00,0x00,0x00,      // 00C8 MOV DWORD PTR DS:[EDX], 1

0x61,                               // 00CE POPAD
0xC3,                               // 00CF RETN
};

unsigned char code_generic_mov[] = {
0x83,0xEC,0x04,                     // 0000 SUB ESP,4
0x57,                               // 0003 PUSH EDI
0x8D,0x44,0xE4,0x04,                // 0004 LEA EAX,[ESP+04]
0x8B,0x7C,0xE4,0x0C,                // 0008 MOV EDI,DWORD PTR SS:[ESP+0C]
0x8B,0x57,0x28,                     // 000C MOV EDX,DWORD PTR DS:[EDI+28]
0x8B,0x92,0x60,0x01,0x00,0x00,      // 000F MOV EDX,DWORD PTR DS:[EDX+160]
0x50,                               // 0015 PUSH EAX
0x52,                               // 0016 PUSH EDX
0xFF,0x74,0xE4,0x18,                // 0017 PUSH DWORD PTR SS:[ESP+18]
0xE8,0x00,0x00,0x00,0x00,           // 001B CALL $mov_by_name
                                    // 001C ***FIXUP*** COH relcall
0x83,0xC4,0x0C,                     // 0020 ADD ESP,0C
0x84,0xC0,                          // 0023 TEST AL,AL
0x74,0x07,                          // 0025 JE SHORT 002E
0x0F,0xB7,0x44,0xE4,0x04,           // 0027 MOVZX EAX,WORD PTR SS:[ESP+04]
0xEB,0x02,                          // 002C JMP SHORT 0031
0x31,0xC0,                          // 002E XOR EAX,EAX
0x89,0x87,0x00,0x00,0x00,0x00,      // 0030 MOV DWORD PTR [EDI+$next_mov],EAX
                                    // 0032 ***FIXUP*** offset
0x31,0xC0,                          // 0036 XOR EAX,EAX
0x89,0x87,0x58,0x01,0x00,0x00,      // 0038 MOV DWORD PTR [EDI+158], EAX
0x5F,                               // 003E POP EDI
0x83,0xC4,0x04,                     // 003F ADD ESP,4
0xC3,                               // 0042 RETN
};

unsigned char code_key_hook[] = {
0xF6,0x44,0xE4,0x0C,0x80,           // 0000 TEST BYTE PTR SS:[ESP+0C],80
0x60,                               // 0005 PUSHAD

0x75,0x07,                          // 0006 JNE SHORT 000F
0x61,                               // 0008 POPAD
0xA1,0x00,0x00,0x00,0x00,           // 0009 MOV EAX,DWORD PTR DS:[$binds]
                                    // 000A ***FIXUP*** COH absvar
0xC3,                               // 000E RETN
0x83,0xFE,0x7F,                     // 000F CMP ESI, 7F
0x77,0xF4,                          // 0012 JA SHORT 0008
0x8B,0x14,0xB5,0x00,0x00,0x00,0x00, // 0014 MOV EDX, DWORD PTR [ESI*4+$kbhooks]
                                    // 0017 ***FIXUP*** Icon absvar
0x85,0xD2,                          // 001B TEST EDX,EDX
0x74,0xE9,                          // 001D JZ SHORT 0008
0xFF,0xD2,                          // 001F CALL EDX
0xEB,0xE5,                          // 0021 JMP SHORT 0008
};

unsigned char code_key_fly[] = {
0x8B,0x3D,0x00,0x00,0x00,0x00,      // 0000 MOV EDI,DWORD PTR [$player_ent]
                                    // 0002 ***FIXUP*** COH absvar
0x8B,0x7F,0x2C,                     // 0006 MOV EDI,DWORD PTR DS:[EDI+2C]
0x8D,0xBF,0xA8,0x00,0x00,0x00,      // 0009 LEA EDI,[EDI+0A8]
0xBD,0x00,0x00,0x00,0x00,           // 000F MOV EBP,OFFSET $controls_from_server
                                    // 0010 ***FIXUP*** COH absvar
0xB1,0x01,                          // 0014 MOV CL,1
0xB3,0x08,                          // 0016 MOV BL,8
0x30,0x4F,0x3C,                     // 0018 XOR BYTE PTR DS:[EDI+3C],CL
0x84,0x4F,0x3C,                     // 001B TEST BYTE PTR DS:[EDI+3C],CL
0x74,0x0F,                          // 001E JE SHORT 002F
0x08,0x5D,0x3C,                     // 0020 OR BYTE PTR SS:[EBP+3C],BL
0xB8,0x00,0x00,0x20,0x41,           // 0023 MOV EAX,41200000
0xBB,0x00,0x00,0xA0,0x40,           // 0028 MOV EBX,40A00000
0xEB,0x0C,                          // 002D JMP SHORT 003B
0xF6,0xD3,                          // 002F NOT BL
0x20,0x5D,0x3C,                     // 0031 AND BYTE PTR SS:[EBP+3C],BL
0xB8,0x00,0x00,0x80,0x3F,           // 0034 MOV EAX,3F800000
0x89,0xC3,                          // 0039 MOV EBX,EAX
0x89,0x5F,0x0C,                     // 003B MOV DWORD PTR DS:[EDI+0C],EBX
0x89,0x47,0x28,                     // 003E MOV DWORD PTR DS:[EDI+28],EAX
0x89,0x47,0x2C,                     // 0041 MOV DWORD PTR DS:[EDI+2C],EAX
0x89,0x5F,0x38,                     // 0044 MOV DWORD PTR DS:[EDI+38],EBX
0xC3,                               // 0047 RETN
};

unsigned char code_key_torch[] = {
0x8B,0x15,0x00,0x00,0x00,0x00,      // 0000 MOV EDX,DWORD PTR [$player_ent]
                                    // 0002 ***FIXUP*** COH absvar
0xB8,0x00,0x00,0x00,0x00,           // 0006 MOV EAX, OFFSET $holdtorch
                                    // 0007 ***FIXUP*** Icon absvar
0x50,                               // 000B PUSH EAX
0x52,                               // 000C PUSH EDX
0xE8,0x00,0x00,0x00,0x00,           // 000D CALL $generic_mov
                                    // 000E ***FIXUP*** Icon relcall
0x83,0xC4,0x08,                     // 0012 ADD ESP,8
0xC3,                               // 0015 RETN
};

unsigned char code_key_nocoll[] = {
0xBA,0x00,0x00,0x00,0x00,           // 0000 MOV EDX, OFFSET $nocoll
                                    // 0001 ***FIXUP*** COH absvar
0x83,0x32,0x01,                     // 0005 XOR DWORD PTR [EDX], 1
0x74,0x07,                          // 0008 JZ 0017
0xB8,0x00,0x00,0x00,0x00,           // 000A MOV EAX, $noclip_on
                                    // 000B ***FIXUP*** Icon absvar
0xEB,0x05,                          // 000F JMP 001C
0xB8,0x00,0x00,0x00,0x00,           // 0011 MOV EAX, $noclip_off
                                    // 0012 ***FIXUP*** Icon absvar
0x6A,0xFF,                          // 0016 PUSH -1
0x50,                               // 0018 PUSH EAX
0xE8,0x00,0x00,0x00,0x00,           // 0019 CALL annoying_alert
                                    // 001A ***FIXUP*** COH relcall
0x83,0xC4,0x08,                     // 001E ADD ESP,8
0xC3,                               // 0021 RETN
};

unsigned char code_key_seeall[] = {
0xBA,0x00,0x00,0x00,0x00,           // 0000 MOV EDX, OFFSET $seeall
                                    // 0001 ***FIXUP*** COH absvar
0x83,0x32,0x01,                     // 0005 XOR DWORD PTR [EDX], 1
0xC3,                               // 0008 RETN
};

unsigned char code_key_detach[] = {
0x8B,0x15,0x00,0x00,0x00,0x00,      // 0000 MOV EDX, DWORD PTR [$is_detached]
                                    // 0002 ***FIXUP*** COH absvar
0x83,0xF2,0x01,                     // 0006 XOR EDX, 1
0x52,                               // 0009 PUSH EDX
0x68,0x00,0x00,0x00,0x00,           // 000A PUSH OFFSET $camera
                                    // 000B ***FIXUP*** COH absvar
0x52,                               // 000F PUSH EDX
0xE8,0x00,0x00,0x00,0x00,           // 0010 CALL $detach_camera
                                    // 0011 ***FIXUP*** COH relcall
0x83,0xC4,0x08,                     // 0015 ADD ESP, 8
0x5A,                               // 0018 POP EDX
0x6A,0xFF,                          // 0019 PUSH -1
0x85,0xD2,                          // 001B TEST EDX,EDX
0x74,0x07,                          // 001D JZ SHORT 0026
0x68,0x00,0x00,0x00,0x00,           // 001F PUSH OFFSET $cameradetach
                                    // 0020 ***FIXUP*** Icon absvar
0xEB,0x05,                          // 0024 JMP SHORT 002B
0x68,0x00,0x00,0x00,0x00,           // 0026 PUSH OFFSET $cameraattach
                                    // 0027 ***FIXUP*** Icon absvar
0xE8,0x00,0x00,0x00,0x00,           // 002B CALL $annoying_alert
                                    // 002C ***FIXUP*** COH relcall
0x83,0xC4,0x08,                     // 0030 ADD ESP, 8
0xC3,                               // 0033 RETN
};

typedef struct {
    unsigned long offset;
    unsigned long len;
    unsigned char *code;
} codedef;

static codedef icon_code[] = {
    { 0, sizeof(code_enter_game), code_enter_game },
    { 0, sizeof(code_generic_mov), code_generic_mov },
    { 0, sizeof(code_key_hook), code_key_hook },
    { 0, sizeof(code_key_fly), code_key_fly },
    { 0, sizeof(code_key_torch), code_key_torch },
    { 0, sizeof(code_key_nocoll), code_key_nocoll },
    { 0, sizeof(code_key_seeall), code_key_seeall },
    { 0, sizeof(code_key_detach), code_key_detach },
    { 0, 0, 0 }
};

static void AllocIconMem() {
    iconStrBase = (DWORD)VirtualAllocEx(pinfo.hProcess, NULL, ICON_STR_SIZE,
	    MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!iconStrBase)
	WBailout("Failed to allocate memory");
    iconDataBase = (DWORD)VirtualAllocEx(pinfo.hProcess, NULL, ICON_DATA_SIZE,
	    MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!iconDataBase)
	WBailout("Failed to allocate memory");
    iconCodeBase = (DWORD)VirtualAllocEx(pinfo.hProcess, NULL, ICON_CODE_SIZE,
	    MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READ);
    if (!iconCodeBase)
	WBailout("Failed to allocate memory");
}

static void WriteIconData() {
    unsigned long *hooks;
    char **s;
    int i, l;
    DWORD o;
    DWORD zoneMap[6];

    // Write string table
    i = 0; o = 0;
    s = icon_strs;
    while (*s && **s) {
	l = strlen(*s) + 1;

	if (o + l > ICON_STR_SIZE)
	    Bailout("String table overflow");

	iconStrOffsets = realloc(iconStrOffsets, sizeof(DWORD) * (i + 1));
	iconStrOffsets[i] = o;
	PutStr(iconStrBase + o, *s, l);

	o += l;
	++i; ++s;
    }

    // Build zone map
    for (i = 0; i < 6; i++) {
	zoneMap[i] = iconStrBase + iconStrOffsets[STR_MAP_OUTBREAK + i];
    }

    // Write data
    o = 0;

    iconDataOffsets = malloc(3 * sizeof(DWORD));
    iconDataOffsets[DATA_ZONE_MAP] = o;
    l = sizeof(zoneMap);
    if (o + l > ICON_DATA_SIZE)
	Bailout("Data section overflow");
    PutStr(iconDataBase + o, (char*)zoneMap, l);
    o += l;

    iconDataOffsets[DATA_SPAWNCOORDS] = o;
    l = sizeof(spawncoords);
    if (o + l > ICON_DATA_SIZE)
	Bailout("Data section overflow");
    PutStr(iconDataBase + o, (char*)spawncoords, l);
    o += l;

    iconDataOffsets[DATA_KBHOOKS] = o;
    l = 0x80 * sizeof(DWORD);
    hooks = calloc(1, l);

    // Set up hooks for keycodes
    hooks[2] = iconCodeBase + icon_code[CODE_KEY_FLY].offset;
    hooks[3] = iconCodeBase + icon_code[CODE_KEY_TORCH].offset;
    hooks[4] = iconCodeBase + icon_code[CODE_KEY_NOCOLL].offset;
    hooks[5] = iconCodeBase + icon_code[CODE_KEY_SEEALL].offset;
    hooks[6] = iconCodeBase + icon_code[CODE_KEY_DETACH].offset;

    if (o + l > ICON_DATA_SIZE)
        Bailout("Data section overflow");
    PutStr(iconDataBase + o, (char*)hooks, l);
    o += l;
    free(hooks);

}

static void WriteIconCode() {
    codedef *c;
    DWORD o;

    // Write code
    o = 0;
    c = icon_code;
    while (c && c->len) {
	if (o + c->len > ICON_CODE_SIZE)
	    Bailout("Code section overflow");
	PutStr(iconCodeBase + o, (char*)c->code, c->len);

	o += c->len;
	++c;
    }
}

static void CalculateIconOffsets() {
    codedef *cd;
    int i;
    DWORD o;

    // Calculate code offsets; need them for jumps and data refs
    i = 0; o = 0;
    cd = icon_code;
    while (cd && cd->len) {
	cd->offset = o;
	o += cd->len;
	++i; ++cd;
    }
}

#define SETBYTE(a, x) ((*(c+(a))) = (x))
#define SETLONG(a, x) ((*(unsigned long*)(c+(a))) = (unsigned long)(x))
#define SETCALL(a, x) (SETLONG((a), relcall(iconCodeBase + cd->offset + (a), (x))))
static void FixupIconOffsets() {
    codedef *cd;
    unsigned char *c;
    // Fixup enter_game
    c = icon_code[CODE_ENTER_GAME].code;
    SETLONG(0x000A, iconDataBase + iconDataOffsets[DATA_ZONE_MAP]);
    SETLONG(0x00A5, iconDataBase + iconDataOffsets[DATA_SPAWNCOORDS]);

    // Fixup key_hook
    c = icon_code[CODE_KEY_HOOK].code;
    SETLONG(0x0017, iconDataBase + iconDataOffsets[DATA_KBHOOKS]);

    // Fixup key_torch
    cd = &icon_code[CODE_KEY_TORCH];
    c = cd->code;
    SETLONG(0x0007, iconStrBase + iconStrOffsets[STR_HOLDTORCH]);
    SETCALL(0x000E, iconCodeBase + icon_code[CODE_GENERIC_MOV].offset);

    // Fixup key_nocoll
    cd = &icon_code[CODE_KEY_NOCOLL];
    c = cd->code;
    SETLONG(0x000B, iconStrBase + iconStrOffsets[STR_NOCLIP_ON]);
    SETLONG(0x0012, iconStrBase + iconStrOffsets[STR_NOCLIP_OFF]);

    // Fixup key_detach
    cd = &icon_code[CODE_KEY_DETACH];
    c = cd->code;
    SETLONG(0x0020, iconStrBase + iconStrOffsets[STR_CAMERADETACH]);
    SETLONG(0x0027, iconStrBase + iconStrOffsets[STR_CAMERAATTACH]);
}

static void FixupI24Offsets() {
    codedef *cd;
    unsigned char *c;
    unsigned long player_ent = 0x00CAF580;
    unsigned long copy_attribs = 0x00495C90;
    unsigned long controls_from_server = 0x00CAF538;
    unsigned long start_choice = 0x00BB95F4;
    unsigned long annoying_alert = 0x005C31C0;
    unsigned long kboff = 0x5AC8;

    // Fixup enter_game
    cd = &icon_code[CODE_ENTER_GAME];
    c = cd->code;

    SETLONG(0x0003, start_choice);
    SETCALL(0x000F, 0x00535650);
    SETLONG(0x0014, player_ent);
    SETLONG(0x001A, 0x0E00);
    SETCALL(0x0026, copy_attribs);
    SETCALL(0x0032, copy_attribs);
    SETLONG(0x0038, player_ent);
    SETLONG(0x0041, kboff);
    SETCALL(0x0051, 0x009D630C);
    SETLONG(0x005B, kboff);
    SETCALL(0x0060, 0x005C9050);
    SETLONG(0x0065, controls_from_server);
    SETLONG(0x0077, player_ent);
    SETLONG(0x008D, 0x01671364);
    SETLONG(0x0098, start_choice);
    SETLONG(0x00AB, player_ent);
    SETCALL(0x00B0, 0x004B3790);
    SETLONG(0x00B5, player_ent);
    SETLONG(0x00C4, 0x0F68); // XXX 0x0f6c for I23

    // Fixup generic_mov
    cd = &icon_code[CODE_GENERIC_MOV];
    c = cd->code;
    SETCALL(0x001C, 0x00599710);            // $move_by_name
    SETLONG(0x0032, 0x1928);                // $next_mov offset

    // Fixup key_hook
    cd = &icon_code[CODE_KEY_HOOK];
    c = cd->code;
    SETLONG(0x000A, 0x00E37F64);

    // Fixup key_fly
    cd = &icon_code[CODE_KEY_FLY];
    c = cd->code;
    SETLONG(0x0002, player_ent);
    SETLONG(0x0010, controls_from_server);

    // Fixup key_torch
    cd = &icon_code[CODE_KEY_TORCH];
    c = cd->code;
    SETLONG(0x0002, player_ent);

    // Fixup key_nocoll
    cd = &icon_code[CODE_KEY_NOCOLL];
    c = cd->code;
    SETLONG(0x0001, 0x016714AC);            // $nocoll
    SETCALL(0x001A, annoying_alert);

    // Fixup key_seeall
    cd = &icon_code[CODE_KEY_SEEALL];
    c = cd->code;
    SETLONG(0x0001, 0x0167CC04);            // $seeall

    // Fixup key_detach
    cd = &icon_code[CODE_KEY_DETACH];
    c = cd->code;
    SETLONG(0x0002, 0x016730C8);            // $is_detached
    SETLONG(0x000B, 0x012DF1A0);            // $camera
    SETCALL(0x0011, 0x004DF9E0);            // $detach_camera
    SETCALL(0x002C, annoying_alert);
}

static void FixupI23Offsets() {
}

static void PatchI24() {
    // product published?
    bmagic(0x00830259, 0xc032cc33, 0x01b0cc33);

    // owns product?
    bmagic(0x0083147B, 0xff853a74, 0x5aeb01b0);

    // create character
    bmagic(0x0083B246, 0x0410ec81, 0xc90ed5e8);
    if (random) {
        bmagic(0x0083B24a, 0x84a10000, 0xe980e8ff);
        bmagic(0x0083B24e, 0x3300b1ba, 0x1e6affc9);
        bmagic(0x0083B252, 0x248489c4, 0xd88b59e8);
        bmagic(0x0083B256, 0x0000040c, 0x04c483ff);
    } else {
        bmagic(0x0083B24a, 0x84a10000, 0xbb80e8ff);
        bmagic(0x0083B24e, 0x3300b1ba, 0x05c7ffee);
        bmagic(0x0083B252, 0x248489c4, 0x0167c800);
        bmagic(0x0083B256, 0x0000040c, editnpc);
    }
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

    // NOP out comm check
    bmagic(0x00409332, 0x5E0C053B, 0x90909090);
    bmagic(0x00409336, 0xC01B0168, 0x90909090);

    // always return 1 for connected
    bmagic(0x0040DA1D, 3, 1);

    // ignore check for mapserver in main loop
    bmagic(0x00838249, 0x3d392c77, 0x3d392ceb);

    // nocoll command
    bmagic(0x00BD12A4, 1, 0);

    // hook keyboard
    bmagic(0x005C94E0, 0xE37F64A1, 0x000000E8);
    PutRelCall(0x005C94E1, iconCodeBase + icon_code[CODE_KEY_HOOK].offset);

    // turn on invert mouse
    bmagic(0x00B34E00, 0, 1);

    // Hook "enter game"
    bmagic(0x004CC608, 0xA1000003, 0xE8000003);
    PutRelCall(0x004CC60C, iconCodeBase + icon_code[CODE_ENTER_GAME].offset);
    bmagic(0x004CC610, 0xC01BD8F7, 0xC4A3C031);
    bmagic(0x004CC614, 0x83A6E083, 0xE9012DF3);
    bmagic(0x004CC618, 0x44895AC0, 0x00000390);
}

static void PatchI23() {
    // product published?
    bmagic(0x00832f09, 0xc032cc33, 0x01b0cc33);

    // owns product?
    bmagic(0x0083408B, 0xff853a74, 0x5aeb01b0);

    // create character
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

    // "Sandbox Mode" stuff below

    // NOP out comm check
    bmagic(0x00409332, 0x1FAC053B, 0x90909090);
    bmagic(0x00409336, 0xC01B0168, 0x90909090);

    // always return 1 for connected
    bmagic(0x0040D9CD, 3, 1);

    // ignore check for mapserver in main loop
    bmagic(0x0083ADF9, 0x3d392c77, 0x3d392ceb);

    // nocoll command
    bmagic(0x00BCEFBC, 1, 0);

    // hook keyboard
    bmagic(0x005C7BB0, 0xE35994A1, 0xF03D09E8);
    bmagic(0x005C7BB4, 0x8B555300, 0x8B5553FF);

    // turn on invert mouse
    bmagic(0x00B349F0, 0, 1);
}

static void RunPatch() {
    int vers = 0;

    if (GetInt(0x00BE15D4) == 0xa77f40)
	vers = 23;
    else if (GetInt(0x00BE38BC) == 0xa76044)
	vers = 24;
    else
	Bailout("Sorry, your cityofheroes.exe file is not a supported version.");

    AllocIconMem();
    CalculateIconOffsets();
    WriteIconData();
    FixupIconOffsets();
    if (vers == 23)
	FixupI23Offsets();
    else if (vers == 24)
	FixupI24Offsets();

    WriteIconCode();

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
