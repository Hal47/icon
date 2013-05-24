/* vim: set sts=4 sw=4 et: */

#define WINVER 0x0501
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "icon.h"
#include "util.h"
#include "code.h"
#include "data.h"
#include "strings.h"

static DWORD iconCodeBase = 0;

enum {
    RELOC_ICON_STR,
    RELOC_ICON_DATA,
    RELOC_ICON_CODE_ABS,
    RELOC_ICON_CODE_REL,
    RELOC_COH_ABS,
    RELOC_COH_REL,
};

typedef struct {
    int type;
    int id;
} reloc;

typedef struct {
    int id;
    DWORD offset;
    int sz;
    unsigned char *code;
    reloc *relocs;
} codedef;

static codedef **codedef_cache = 0;

// Magic sequence to mark a reloc in code
#define RELOC 0xDE,0xAD,0xAD,0xD0

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
0xA3,0x00,0x00,0x00,0x00,           // 00CE MOV DWORD PTR $enttbl[1], EAX
                                    // 00CF ***FIXUP*** COH absvar
0x61,                               // 00D3 POPAD
0xC3,                               // 00D4 RETN
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

unsigned char code_get_target[] = {
0xA1,0x00,0x00,0x00,0x00,           // 0000 MOV EAX, DWORD PTR [$target]
                                    // 0001 ***FIXUP*** COH absvar
0x85,0xC0,                          // 0005 TEST EAX,EAX
0x74,0x01,                          // 0007 JZ 000A
0xC3,                               // 0009 RETN
0xA1,0x00,0x00,0x00,0x00,           // 000A MOV EAX, DWORD PTR [$player_ent]
                                    // 000B ***FIXUP*** COH absvar
0xC3,                               // 000F RETN
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

unsigned char code_key_loadmap[] = {
0x6A,0x00,                          // 0000 PUSH 00
0x68,0xFF,0x00,0x00,0x00,           // 0002 PUSH 00FF
0x6A,0x00,                          // 0007 PUSH 00
0x6A,0x00,                          // 0009 PUSH 00
0x6A,0x00,                          // 000B PUSH 00
0x6A,0x00,                          // 000D PUSH 00
0x6A,0x00,                          // 0009 PUSH 00
0x6A,0x00,                          // 0011 PUSH 00
0x6A,0x00,                          // 0013 PUSH 00
0x68,0x00,0x00,0x00,0x00,           // 0015 PUSH OFFSET $loadmap_cb
                                    // 0016 ***FIXUP*** Icon absvar
0x6A,0x00,                          // 001A PUSH 00
0x68,0x00,0x00,0x00,0x00,           // 001C PUSH OFFSET $mapfile
                                    // 001D ***FIXUP*** Icon absvar
0x6A,0xFF,                          // 0021 PUSH -1
0x6A,0xFF,                          // 0023 PUSH -1
0x6A,0x07,                          // 0025 PUSH 7
0xE8,0x00,0x00,0x00,0x00,           // 0027 CALL $dialog
                                    // 0028 ***FIXUP*** COH relcall
0x83,0xC4,0x3C,                     // 002C ADD ESP, 3C
0xC3,                               // 002F RETN
};

unsigned char code_loadmap_cb[] = {
0xE8,0x00,0x00,0x00,0x00,           // 0000 CALL $dialog_get_text
                                    // 0001 ***FIXUP*** COH relcall
0x75,0x01,                          // 0005 JNZ 0008
0xC3,                               // 0007 RETN
0x50,                               // 0008 PUSH EAX
0xE8,0x00,0x00,0x00,0x00,           // 0009 CALL $map_clear
                                    // 000A ***FIXUP*** COH relcall
0x58,                               // 000E POP EAX
0xE8,0x00,0x00,0x00,0x00,           // 000F CALL $load_map_demo
				    // 0010 ***FIXUP*** COH relcall
0xC3,                               // RETN
};

#define CODE(id, c, r) { CODE_##id, 0, sizeof(c), c, r }
codedef icon_code[] = {
    CODE(ENTER_GAME, code_enter_game, NULL),
    CODE(GENERIC_MOV, code_generic_mov, NULL),
    CODE(GET_TARGET, code_get_target, NULL),
    CODE(KEY_HOOK, code_key_hook, NULL),
    CODE(KEY_FLY, code_key_fly, NULL),
    CODE(KEY_TORCH, code_key_torch, NULL),
    CODE(KEY_NOCOLL, code_key_nocoll, NULL),
    CODE(KEY_SEEALL, code_key_seeall, NULL),
    CODE(KEY_DETACH, code_key_detach, NULL),
    CODE(KEY_LOADMAP, code_key_loadmap, NULL),
    CODE(LOADMAP_CB, code_loadmap_cb, NULL),
    { 0, 0, 0, 0 }
};
#undef CODE

static void InitCode() {
    DWORD o = 0;

    iconCodeBase = (DWORD)VirtualAllocEx(pinfo.hProcess, NULL, ICON_CODE_SIZE,
            MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READ);
    if (!iconCodeBase)
        WBailout("Failed to allocate memory");

    codedef_cache = calloc(1, sizeof(codedef*) * CODE_END);
    codedef *cd = icon_code;
    while (cd && cd->sz) {
        cd->offset = o;
        codedef_cache[cd->id] = cd;
        o += cd->sz;
        ++cd;
    }

    if (o > ICON_CODE_SIZE)
        Bailout("Code section overflow");
}

static codedef *CodeById(int id) {
    if (!codedef_cache)
        InitCode();
    return codedef_cache[id];
}

unsigned long CodeAddr(int id) {
    if (!codedef_cache)
        InitCode();
    return iconCodeBase + codedef_cache[id]->offset;
}

void WriteCode() {
    codedef *c;

    // Write code
    c = icon_code;
    while (c && c->sz) {
	PutData(CodeAddr(c->id), c->code, c->sz);
	++c;
    }
}

#define SETBYTE(a, x) ((*(cd->code+(a))) = (x))
#define SETLONG(a, x) ((*(unsigned long*)(cd->code+(a))) = (unsigned long)(x))
#define SETCALL(a, x) (SETLONG((a), CalcRelAddr(CodeAddr(cd->id) + (a), (x))))
static void FixupIconOffsets() {
    codedef *cd;
    // Fixup enter_game
    cd = CodeById(CODE_ENTER_GAME);
    SETLONG(0x000A, DataAddr(DATA_ZONE_MAP));
    SETLONG(0x00A5, DataAddr(DATA_SPAWNCOORDS));

    // Fixup key_hook
    cd = CodeById(CODE_KEY_HOOK);
    SETLONG(0x0017, DataAddr(DATA_KBHOOKS));

    // Fixup key_torch
    cd = CodeById(CODE_KEY_TORCH);
    SETLONG(0x0007, StringAddr(STR_HOLDTORCH));
    SETCALL(0x000E, CodeAddr(CODE_GENERIC_MOV));

    // Fixup key_nocoll
    cd = CodeById(CODE_KEY_NOCOLL);
    SETLONG(0x000B, StringAddr(STR_NOCLIP_ON));
    SETLONG(0x0012, StringAddr(STR_NOCLIP_OFF));

    // Fixup key_detach
    cd = CodeById(CODE_KEY_DETACH);
    SETLONG(0x0020, StringAddr(STR_CAMERADETACH));
    SETLONG(0x0027, StringAddr(STR_CAMERAATTACH));

    // Fixup key_loadmap
    cd = CodeById(CODE_KEY_LOADMAP);
    SETLONG(0x0016, CodeAddr(CODE_LOADMAP_CB));
    SETLONG(0x001D, StringAddr(STR_MAPFILE));
}

static void FixupI24Offsets() {
    codedef *cd;
    unsigned long load_map_demo = 0x00535650;
    unsigned long player_ent = 0x00CAF580;
    unsigned long copy_attribs = 0x00495C90;
    unsigned long controls_from_server = 0x00CAF538;
    unsigned long start_choice = 0x00BB95F4;
    unsigned long annoying_alert = 0x005C31C0;
    unsigned long kboff = 0x5AC8;

    // Fixup enter_game
    cd = CodeById(CODE_ENTER_GAME);
    SETLONG(0x0003, start_choice);
    SETCALL(0x000F, load_map_demo);
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
    SETLONG(0x00CF, 0x012F6C44);    // $enttbl[1]

    // Fixup generic_mov
    cd = CodeById(CODE_GENERIC_MOV);
    SETCALL(0x001C, 0x00599710);            // $move_by_name
    SETLONG(0x0032, 0x1928);                // $next_mov offset

    // Fixup get_target
    cd = CodeById(CODE_GET_TARGET);
    SETLONG(0x0001, 0x00F07220);            // $target
    SETLONG(0x000B, player_ent);

    // Fixup key_hook
    cd = CodeById(CODE_KEY_HOOK);
    SETLONG(0x000A, 0x00E37F64);

    // Fixup key_fly
    cd = CodeById(CODE_KEY_FLY);
    SETLONG(0x0002, player_ent);
    SETLONG(0x0010, controls_from_server);

    // Fixup key_torch
    cd = CodeById(CODE_KEY_TORCH);
    SETLONG(0x0002, player_ent);

    // Fixup key_nocoll
    cd = CodeById(CODE_KEY_NOCOLL);
    SETLONG(0x0001, 0x016714AC);            // $nocoll
    SETCALL(0x001A, annoying_alert);

    // Fixup key_seeall
    cd = CodeById(CODE_KEY_SEEALL);
    SETLONG(0x0001, 0x0167CC04);            // $seeall

    // Fixup key_detach
    cd = CodeById(CODE_KEY_DETACH);
    SETLONG(0x0002, 0x016730C8);            // $is_detached
    SETLONG(0x000B, 0x012DF1A0);            // $camera
    SETCALL(0x0011, 0x004DF9E0);            // $detach_camera
    SETCALL(0x002C, annoying_alert);

    // Fixup key_loadmap
    cd = CodeById(CODE_KEY_LOADMAP);
    SETCALL(0x0028, 0x005B6E10);

    // Fixup loadmap_cb
    cd = CodeById(CODE_LOADMAP_CB);
    SETCALL(0x0001, 0x005BAD80);            // $dialog_get_text
    SETCALL(0x000A, 0x0053BFC0);            // $map_clear
    SETCALL(0x0010, load_map_demo);
}

void RelocateCode() {
    FixupIconOffsets();
    FixupI24Offsets();
}
