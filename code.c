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
#include "patch.h"

static DWORD iconCodeBase = 0;

enum {
    ICON_STR = 1,
    ICON_DATA,
    ICON_CODE_ABS,
    ICON_CODE_REL,
    COH_ABS,
    COH_REL,
    IMMEDIATE,
    RELOC_END
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
0x8B,0x0D,RELOC,                    // 0001 MOV ECX,DWORD PTR [$start_choice]
0x8B,0x04,0x8D,RELOC,               // 0007 MOV EAX,DWORD PTR DS:[ECX*4+$STR]
0xE8,RELOC,                         // 000E CALL $load_map_demo
0xA1,RELOC,	                    // 0013 MOV EAX,DWORD PTR [$player_ent]
0x8B,0x80,RELOC,                    // 0018 MOV EAX,DWORD PTR DS:[EAX+$charoff]
0x50,                               // 001E PUSH EAX
0x8D,0x90,0xA4,0x00,0x00,0x00,      // 001F LEA EDX,[EAX+0A4]
0xE8,RELOC,                         // 0025 CALL $copy_attribs
0x58,                               // 002A POP EAX
0x8D,0x90,0x3C,0x04,0x00,0x00,      // 002B LEA EDX,[EAX+43C]
0xE8,RELOC,                         // 0031 CALL $copy_attribs
0x8B,0x15,RELOC,                    // 0036 MOV EDX,DWORD PTR [$player_ent]
0x8B,0x52,0x30,                     // 003C MOV EDX,DWORD PTR DS:[EDX+30]
0x83,0xBA,RELOC,0x00,               // 003F CMP DWORD PTR DS:[EDX+$kboff],0
0x75,0x17,                          // 0046 JNE SHORT 005F
0x52,                               // 0048 PUSH EDX
0x68,0x00,0x30,0x00,0x00,           // 0049 PUSH 3000
0x6A,0x01,                          // 004E PUSH 1
0xE8,RELOC,                         // 0050 CALL $calloc
0x83,0xC4,0x08,                     // 0055 ADD ESP,8
0x5A,                               // 0058 POP EDX
0x89,0x82,RELOC,                    // 0059 MOV DWORD PTR DS:[EDX+$kboff],EAX
0xE8,RELOC,                         // 005F CALL $init_keybinds

// control state stuff
0xBF,RELOC,	                    // 0064 MOV EDI,OFFSET $controls_from_server
0xB8,0x00,0x00,0x80,0x3F,           // 0069 MOV EAX,3F800000 (1.f)
0xB9,0x0F,0x00,0x00,0x00,           // 006E MOV ECX,0F
0xF3,0xAB,                          // 0073 REP STOS DWORD PTR ES:[EDI]
// surface physics in entity
0x8B,0x3D,RELOC,                    // 0075 MOV EDI,DWORD PTR DS:[$player_ent]
0x8B,0x7F,0x2C,                     // 007B MOV EDI,DWORD PTR DS:[EDI+2C]
0x8D,0xBF,0xBC,0x00,0x00,0x00,      // 007E LEA EDI,[EDI+0BC]
0xB9,0x0A,0x00,0x00,0x00,           // 0084 MOV ECX,0A
0xF3,0xAB,                          // 0089 REP STOS DWORD PTR ES:[EDI]

// set the clock
0x8D,0x15,RELOC,                    // 008B LEA EDX,[$game_time]
0xC7,0x02,0x00,0x00,0x40,0x41,      // 0091 MOV DWORD PTR DS:[EDX],41400000

// set player spawn point
0xA1,RELOC,                         // 0097 MOV EAX,DWORD PTR [$start_choice]
0xBA,0x0C,0x00,0x00,0x00,           // 009C MOV EDX,0C
0xF7,0xE2,                          // 00A1 MUL EDX
0x8D,0x90,RELOC,                    // 00A3 LEA EDX,[EAX+$spawncoords]
0x8B,0x0D,RELOC,                    // 00A9 MOV ECX,DWORD PTR [$player_ent]
0xE8,RELOC,                         // 00AF CALL $ent_teleport

// set a database ID
0xA1,RELOC,	                    // 00B4 MOV EAX,DWORD PTR [$player_ent]
0x8D,0x50,0x74,                     // 00B9 LEA EDX,[EAX+74]
0xC7,0x02,0x01,0x00,0x00,0x00,      // 00BC MOV DWORD PTR DS:[EDX], 1
0x8D,0x90,RELOC,	            // 00C2 LEA EDX,[EAX+$svrid]
0xC7,0x02,0x01,0x00,0x00,0x00,      // 00C8 MOV DWORD PTR DS:[EDX], 1
0x31,0xFF,                          // 00CD XOR EDI,EDI
0x47,                               // 00CF INC EDI
0x89,0x04,0xBD,RELOC,               // 00D0 MOV DWORD PTR $enttbl+EDI*4, EAX

// edit toolbar gets turned on somewhere during game init, turn it back off
0x31,0xC0,                          // XOR EAX,EAX
0xA3,RELOC,                         // MOV DWORD PTR [$draw_edit_bar], EAX
0xB0,0x01,                          // MOV AL, 1
0xA2,RELOC,                         // MOV BYTE PTR [$edit_transform_abs], AL

0xE8,RELOC,                         // CALL $setup_binds

0x61,                               // POPAD
0xC3,                               // RETN
};
reloc reloc_enter_game[] = {
    { COH_ABS, COHVAR_START_CHOICE },
    { ICON_DATA, DATA_ZONE_MAP },
    { COH_REL, COHFUNC_LOAD_MAP_DEMO },
    { COH_ABS, COHVAR_PLAYER_ENT },
    { COH_ABS, COHVAR_ENT_CHAR_OFFSET },
    { COH_REL, COHFUNC_COPY_ATTRIBS },
    { COH_REL, COHFUNC_COPY_ATTRIBS },
    { COH_ABS, COHVAR_PLAYER_ENT },
    { COH_ABS, COHVAR_PLAYER_KBOFFSET },
    { COH_REL, COHFUNC_CALLOC },
    { COH_ABS, COHVAR_PLAYER_KBOFFSET },
    { COH_REL, COHFUNC_INIT_KEYBINDS },
    { COH_ABS, COHVAR_CONTROLS_FROM_SERVER },
    { COH_ABS, COHVAR_PLAYER_ENT },
    { COH_ABS, COHVAR_GAME_TIME },
    { COH_ABS, COHVAR_START_CHOICE },
    { ICON_DATA, DATA_SPAWNCOORDS },
    { COH_ABS, COHVAR_PLAYER_ENT },
    { COH_REL, COHFUNC_ENT_TELEPORT },
    { COH_ABS, COHVAR_PLAYER_ENT },
    { COH_ABS, COHVAR_ENT_SERVERID_OFFSET },
    { COH_ABS, COHVAR_ENTTBL },
    { COH_ABS, COHVAR_DRAW_EDIT_BAR },
    { COH_ABS, COHVAR_EDIT_TRANSFORM_ABS },
    { ICON_CODE_REL, CODE_SETUP_BINDS },
    { RELOC_END, 0 }
};

unsigned char code_setup_binds[] = {
0x68,0x10,0x30,0x00,0x00,           // PUSH 3010
0x6A,0x01,                          // PUSH 1
0xE8,RELOC,                         // CALL $calloc
0x83,0xC4,0x08,                     // ADD ESP,8
0xA3,RELOC,                         // MOV DWORD PTR [$binds], EAX
0xE8,RELOC,                         // CALL $bind_push
0xA1,RELOC,                         // MOV EAX, DWORD PTR [$binds]
0xC6,0x00,0x02,                     // MOV BYTE PTR [EAX], 2
0xBA,RELOC,                         // MOV EDX, $cmd_handler
0x89,0x90,0x08,0x30,0x00,0x00,      // MOV DWORD PTR [EAX+3008], EDX
0xBF,RELOC,                         // MOV EDI, OFFSET $bind_list
// loop:
0x8B,0x07,                          // MOV EAX, DWORD PTR [EDI]
0x85,0xC0,                          // TEST EAX, EAX
0x74,0x13,                          // JZ SHORT done
0x6A,0x00,                          // PUSH 0
0xFF,0x77,0x04,                     // PUSH DWORD PTR [EDI+04]
0x50,                               // PUSH EAX
0xE8,RELOC,                         // CALL $bind
0x83,0xC4,0x0C,                     // ADD ESP, 0C
0x83,0xC7,0x08,                     // ADD EDI, 8
0xEB,0xE7,                          // JMP SHORT loop
// done:
0xC3,                               // RETN
};
reloc reloc_setup_binds[] = {
    { COH_REL, COHFUNC_CALLOC },
    { ICON_DATA, DATA_BINDS },
    { COH_REL, COHFUNC_BIND_PUSH },
    { ICON_DATA, DATA_BINDS },
    { ICON_CODE_ABS, CODE_CMD_HANDLER },
    { ICON_DATA, DATA_BIND_LIST },
    { COH_REL, COHFUNC_BIND },

    { RELOC_END, 0 }
};

unsigned char code_cmd_handler[] = {
0x55,                           // PUSH EBP
0x89,0xE5,                      // MOV EBP, ESP
0x81,0xEC,0x60,0x04,0x00,0x00,  // SUB ESP, 460
0x57,                           // PUSH EDI
0x8D,0x85,0xA0,0xFB,0xFF,0xFF,  // LEA EAX, [EBP-460]
0x8B,0x7D,0x08,                 // MOV EDI, DWORD PTR [EBP+8]
0x57,                           // PUSH EDI
0x68,RELOC,                     // PUSH $commands
0xE8,RELOC,                     // CALL $cmd_parse
0x83,0xC4,0x08,                 // ADD ESP, 8
0x85,0xC0,                      // TEST EAX,EAX
0x74,0x13,                      // JNZ SHORT out
0x8B,0x78,0x08,                 // MOV EDI, DWORD PTR [EAX+8]
0x8B,0x14,0xBD,RELOC,           // MOV EDX, DWORD PTR [EDI*4+$command_funcs]
0x31,0xC0,                      // XOR EAX, EAX
0x85,0xD2,                      // TEST EDX, EDX
0x74,0x03,                      // JZ SHORT out
0xFF,0xD2,                      // CALL EDX
0x40,                           // INC EAX
// out:
0x5F,                           // POP EDI
0xC9,                           // LEAVE
0xC3                            // RETN
};
reloc reloc_cmd_handler[] = {
    { ICON_DATA, DATA_COMMAND_LIST },
    { COH_REL, COHFUNC_CMD_PARSE },
    { ICON_DATA, DATA_COMMAND_FUNCS },
    { RELOC_END, 0 }
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
0xE8,RELOC,                         // 001B CALL $mov_by_name
0x83,0xC4,0x0C,                     // 0020 ADD ESP,0C
0x84,0xC0,                          // 0023 TEST AL,AL
0x74,0x07,                          // 0025 JE SHORT 002E
0x0F,0xB7,0x44,0xE4,0x04,           // 0027 MOVZX EAX,WORD PTR SS:[ESP+04]
0xEB,0x02,                          // 002C JMP SHORT 0031
0x31,0xC0,                          // 002E XOR EAX,EAX
0x89,0x87,RELOC,                    // 0030 MOV DWORD PTR [EDI+$next_mov],EAX
0x31,0xC0,                          // 0036 XOR EAX,EAX
0x89,0x87,0x58,0x01,0x00,0x00,      // 0038 MOV DWORD PTR [EDI+158], EAX
0x5F,                               // 003E POP EDI
0x83,0xC4,0x04,                     // 003F ADD ESP,4
0xC3,                               // 0042 RETN
};
reloc reloc_generic_mov[] = {
    { COH_REL, COHFUNC_MOV_BY_NAME },
    { COH_ABS, COHVAR_ENT_NEXT_MOV_OFFSET },
    { RELOC_END, 0 }
};

unsigned char code_get_target[] = {
0xA1,RELOC,                         // 0000 MOV EAX, DWORD PTR [$target]
0x85,0xC0,                          // 0005 TEST EAX,EAX
0x74,0x01,                          // 0007 JZ 000A
0xC3,                               // 0009 RETN
0xA1,RELOC,                         // 000A MOV EAX, DWORD PTR [$player_ent]
0xC3,                               // 000F RETN
};
reloc reloc_get_target[] = {
    { COH_ABS, COHVAR_TARGET },
    { COH_ABS, COHVAR_PLAYER_ENT },
    { RELOC_END, 0 }
};

unsigned char code_key_hook[] = {
0xF6,0x44,0xE4,0x0C,0x80,           // 0000 TEST BYTE PTR SS:[ESP+0C],80
0x60,                               // 0005 PUSHAD

0x75,0x07,                          // 0006 JNE SHORT 000F
0x61,                               // 0008 POPAD
0xA1,RELOC,                         // 0009 MOV EAX,DWORD PTR DS:[$binds]
0xC3,                               // 000E RETN
0x83,0xFE,0x7F,                     // 000F CMP ESI, 7F
0x77,0xF4,                          // 0012 JA SHORT 0008
0x8B,0x14,0xB5,RELOC,               // 0014 MOV EDX, DWORD PTR [ESI*4+$kbhooks]
0x85,0xD2,                          // 001B TEST EDX,EDX
0x74,0xE9,                          // 001D JZ SHORT 0008
0xFF,0xD2,                          // 001F CALL EDX
0xEB,0xE5,                          // 0021 JMP SHORT 0008
};
reloc reloc_key_hook[] = {
    { COH_ABS, COHVAR_BINDS },
    { ICON_DATA, DATA_KBHOOKS },
    { RELOC_END, 0 }
};

unsigned char code_key_fly[] = {
0x8B,0x3D,RELOC,                    // 0000 MOV EDI,DWORD PTR [$player_ent]
0x8B,0x7F,0x2C,                     // 0006 MOV EDI,DWORD PTR DS:[EDI+2C]
0x8D,0xBF,0xA8,0x00,0x00,0x00,      // 0009 LEA EDI,[EDI+0A8]
0xBD,RELOC,                         // 000F MOV EBP,OFFSET $controls_from_server
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
reloc reloc_key_fly[] = {
    { COH_ABS, COHVAR_PLAYER_ENT },
    { COH_ABS, COHVAR_CONTROLS_FROM_SERVER },
    { RELOC_END, 0 }
};

unsigned char code_key_torch[] = {
0x8B,0x15,RELOC,                    // 0000 MOV EDX,DWORD PTR [$player_ent]
0xB8,RELOC,                         // 0006 MOV EAX, OFFSET $holdtorch
0x50,                               // 000B PUSH EAX
0x52,                               // 000C PUSH EDX
0xE8,RELOC,                         // 000D CALL $generic_mov
0x83,0xC4,0x08,                     // 0012 ADD ESP,8
0xC3,                               // 0015 RETN
};
reloc reloc_key_torch[] = {
    { COH_ABS, COHVAR_PLAYER_ENT },
    { ICON_STR, STR_HOLDTORCH },
    { ICON_CODE_REL, CODE_GENERIC_MOV },
    { RELOC_END, 0 }
};

unsigned char code_cmd_nocoll[] = {
0xBA,RELOC,                         // 0000 MOV EDX, OFFSET $nocoll
0x83,0x32,0x01,                     // 0005 XOR DWORD PTR [EDX], 1
0x74,0x07,                          // 0008 JZ 0017
0xB8,RELOC,                         // 000A MOV EAX, $noclip_on
0xEB,0x05,                          // 000F JMP 001C
0xB8,RELOC,                         // 0011 MOV EAX, $noclip_off
0x6A,0xFF,                          // 0016 PUSH -1
0x50,                               // 0018 PUSH EAX
0xE8,RELOC,                         // 0019 CALL annoying_alert
0x83,0xC4,0x08,                     // 001E ADD ESP,8
0xC3,                               // 0021 RETN
};
reloc reloc_cmd_nocoll[] = {
    { COH_ABS, COHVAR_NOCOLL },
    { ICON_STR, STR_NOCLIP_ON },
    { ICON_STR, STR_NOCLIP_OFF },
    { COH_REL, COHFUNC_ANNOYING_ALERT },
    { RELOC_END, 0 }
};

unsigned char code_cmd_seeall[] = {
0xBA,RELOC,                         // 0000 MOV EDX, OFFSET $seeall
0x83,0x32,0x01,                     // 0005 XOR DWORD PTR [EDX], 1
0xC3,                               // 0008 RETN
};
reloc reloc_cmd_seeall[] = {
    { COH_ABS, COHVAR_SEEALL },
    { RELOC_END, 0 }
};

unsigned char code_key_coords[] = {
0xBA,RELOC,                         // 0000 MOV EDX, OFFSET $draw_edit_bar
0x83,0x32,0x01,                     // 0005 XOR DWORD PTR [EDX], 1
0xC3,                               // 0008 RETN
};
reloc reloc_key_coords[] = {
    { COH_ABS, COHVAR_DRAW_EDIT_BAR },
    { RELOC_END, 0 }
};

unsigned char code_key_detach[] = {
0x8B,0x15,RELOC,                    // 0000 MOV EDX, DWORD PTR [$is_detached]
0x83,0xF2,0x01,                     // 0006 XOR EDX, 1
0x52,                               // 0009 PUSH EDX
0x68,RELOC,                         // 000A PUSH OFFSET $camera
0x52,                               // 000F PUSH EDX
0xE8,RELOC,                         // 0010 CALL $detach_camera
0x83,0xC4,0x08,                     // 0015 ADD ESP, 8
0x5A,                               // 0018 POP EDX
0x6A,0xFF,                          // 0019 PUSH -1
0x85,0xD2,                          // 001B TEST EDX,EDX
0x74,0x07,                          // 001D JZ SHORT 0026
0x68,RELOC,                         // 001F PUSH OFFSET $cameradetach
0xEB,0x05,                          // 0024 JMP SHORT 002B
0x68,RELOC,                         // 0026 PUSH OFFSET $cameraattach
0xE8,RELOC,                         // 002B CALL $annoying_alert
0x83,0xC4,0x08,                     // 0030 ADD ESP, 8
0xC3,                               // 0033 RETN
};
reloc reloc_key_detach[] = {
    { COH_ABS, COHVAR_CAM_IS_DETACHED },
    { COH_ABS, COHVAR_CAMERA },
    { COH_REL, COHFUNC_DETACH_CAMERA },
    { ICON_STR, STR_CAMERADETACH },
    { ICON_STR, STR_CAMERAATTACH },
    { COH_REL, COHFUNC_ANNOYING_ALERT },
    { RELOC_END, 0 }
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
0x68,RELOC,                         // 0015 PUSH OFFSET $loadmap_cb
0x6A,0x00,                          // 001A PUSH 00
0x68,RELOC,                         // 001C PUSH OFFSET $mapfile
0x6A,0xFF,                          // 0021 PUSH -1
0x6A,0xFF,                          // 0023 PUSH -1
0x6A,0x07,                          // 0025 PUSH 7
0xE8,RELOC,                         // 0027 CALL $dialog
0x83,0xC4,0x3C,                     // 002C ADD ESP, 3C
0xC3,                               // 002F RETN
};
reloc reloc_key_loadmap[] = {
    { ICON_CODE_ABS, CODE_LOADMAP_CB },
    { ICON_STR, STR_MAPFILE },
    { COH_REL, COHFUNC_DIALOG },
    { RELOC_END, 0 }
};

unsigned char code_loadmap_cb[] = {
0xE8,RELOC,                         // 0000 CALL $dialog_get_text
0x75,0x01,                          // 0005 JNZ 0008
0xC3,                               // 0007 RETN
0x50,                               // 0008 PUSH EAX
0xE8,RELOC,                         // 0009 CALL $map_clear
0x58,                               // 000E POP EAX
0xE8,RELOC,                         // 000F CALL $load_map_demo
0xC3,                               // RETN
};
reloc reloc_loadmap_cb[] = {
    { COH_REL, COHFUNC_DIALOG_GET_TEXT },
    { COH_REL, COHFUNC_MAP_CLEAR },
    { COH_REL, COHFUNC_LOAD_MAP_DEMO },
    { RELOC_END, 0 }
};

unsigned char code_pos_update_cb[] = {
0x8D,0x74,0x24,0x20,            // 0000 LEA ESI, [ESP+20]
0xE8,RELOC,                     // CALL $get_target
0x80,0x3D,RELOC,0x00,           // CMP BYTE PTR [$edit_transform_abs], 0
0x74, 0x15,                     // JE reltrans
0x50,                           // PUSH EAX
0x56,                           // PUSH ESI
0x50,                           // PUSH EAX
0xE8,RELOC,                     // CALL $set_ent_facing
0x83,0xC4,0x08,                 // ADD ESP, 8
0x59,                           // POP ECX
0x8D,0x56,0x18,                 // LEA EDX, [ESI+18]
0xE8,RELOC,                     // CALL $ent_teleport
0xC3,                           // RETN

// reltrans:
0x83,0xEC,0x10,                 // SUB ESP, 10
0x89,0x44,0xE4,0x0C,            // MOV DWORD PTR [ESP+0C], EAX
0x8D,0x48,0x38,                 // LEA ECX, [EAX+38]
0x89,0xE2,                      // MOV EDX, ESP
0xE8,RELOC,                     // CALL $matrix_to_pyr
0xD9,0x04,0xE4,                 // FLD DWORD PTR [ESP]
0xD8,0x06,                      // FADD DWORD PTR [ESI]
0xD9,0x1C,0xE4,                 // FSTP DWORD PTR [ESP]
0xD9,0x44,0xE4,0x04,            // FLD DWORD PTR [ESP+4]
0xD8,0x46,0x04,                 // FADD DWORD PTR [ESI+4]
0xD9,0x5C,0xE4,0x04,            // FSTP DWORD PTR [ESP+4]
0xD9,0x44,0xE4,0x08,            // FLD DWORD PTR [ESP+8]
0xD8,0x46,0x08,                 // FADD DWORD PTR [ESI+8]
0xD9,0x5C,0xE4,0x08,            // FSTP DWORD PTR [ESP+8]
0x8B,0x44,0xE4,0x0C,            // MOV EAX, DWORD PTR [ESP+0C]
0x54,                           // PUSH ESP
0x50,                           // PUSH EAX
0xE8,RELOC,                     // CALL $set_ent_facing
0x83,0xC4,0x08,                 // ADD ESP, 8
0x8B,0x4C,0xE4,0x0C,            // MOV ECX, DWORD PTR [ESP+0C]
0xD9,0x41,0x5C,                 // FLD DWORD PTR [ECX+5C]
0xD8,0x46,0x18,                 // FADD DWORD PTR [ESI+18]
0xD9,0x1C,0xE4,                 // FSTP DWORD PTR [ESP]
0xD9,0x41,0x60,                 // FLD DWORD PTR [ECX+60]
0xD8,0x46,0x1C,                 // FADD DWORD PTR [ESI+1C]
0xD9,0x5C,0xE4,0x04,            // FSTP DWORD PTR [ESP+4]
0xD9,0x41,0x64,                 // FLD DWORD PTR [ECX+64]
0xD8,0x46,0x20,                 // FADD DWORD PTR [ESI+20]
0xD9,0x5C,0xE4,0x08,            // FSTP DWORD PTR [ESP+8]
0x89,0xE2,                      // MOV EDX, ESP
0xE8,RELOC,                     // CALL $ent_teleport
0x83,0xC4,0x10,                 // ADD ESP, 10
0xC3,                           // RETN
};
reloc reloc_pos_update_cb[] = {
    { ICON_CODE_REL, CODE_GET_TARGET },
    { COH_ABS, COHVAR_EDIT_TRANSFORM_ABS },
    { ICON_CODE_REL, CODE_ENT_SET_FACING },
    { COH_REL, COHFUNC_ENT_TELEPORT },
    { COH_REL, COHFUNC_MATRIX_TO_PYR },
    { ICON_CODE_REL, CODE_ENT_SET_FACING },
    { COH_REL, COHFUNC_ENT_TELEPORT },
    { RELOC_END, 0 }
};

unsigned char code_ent_set_facing[] = {
0x55,                           // PUSH EBP
0x89,0xE5,                      // MOV EBP, ESP
0x56,                           // PUSH ESI
0x57,                           // PUSH EDI
0x8B,0x45,0x08,                 // MOV EAX, DWORD PTR [EBP+8]
0x8D,0x40,0x38,                 // LEA EAX, [EAX+38]
0x8B,0x75,0x0C,                 // MOV ESI, DWORD PTR [EBP+C]
0xE8,RELOC,                     // CALL $matrix_from_pyr
0x8B,0x45,0x08,                 // MOV EAX, DWORD PTR [EBP+8]
0x3B,0x05,RELOC,                // CMP EAX, DWORD PTR [$player_ent]
0x75,0x0F,                      // JNE SHORT out
0xBF,RELOC,                     // MOV EDI, OFFSET $controls
0x8D,0x7F,0x04,                 // LEA EDI, [EDI+4]
0xB9,0x03,0x00,0x00,0x00,       // MOV ECX, 3
0xF3,0xA5,                      // REP MOVSD [EDI], [ESI]
// out:
0x5F,                           // POP EDI
0x5E,                           // POP ESI
0xC9,                           // LEAVE
0xC3,                           // RETN
};
reloc reloc_ent_set_facing[] = {
    { COH_REL, COHFUNC_MATRIX_FROM_PYR },
    { COH_ABS, COHVAR_PLAYER_ENT },
    { COH_ABS, COHVAR_CONTROLS },
    { COH_REL, COHFUNC_MATRIX_FROM_PYR },
    { RELOC_END, 0 }
};

#define CODE(id, c, r) { CODE_##id, 0, sizeof(c), c, r }
codedef icon_code[] = {
    CODE(ENTER_GAME, code_enter_game, reloc_enter_game),
    CODE(SETUP_BINDS, code_setup_binds, reloc_setup_binds),
    CODE(CMD_HANDLER, code_cmd_handler, reloc_cmd_handler),
    CODE(GENERIC_MOV, code_generic_mov, reloc_generic_mov),
    CODE(GET_TARGET, code_get_target, reloc_get_target),
    CODE(KEY_HOOK, code_key_hook, reloc_key_hook),
    CODE(KEY_FLY, code_key_fly, reloc_key_fly),
    CODE(KEY_TORCH, code_key_torch, reloc_key_torch),
    CODE(CMD_NOCOLL, code_cmd_nocoll, reloc_cmd_nocoll),
    CODE(CMD_SEEALL, code_cmd_seeall, reloc_cmd_seeall),
    CODE(KEY_COORDS, code_key_coords, reloc_key_coords),
    CODE(KEY_DETACH, code_key_detach, reloc_key_detach),
    CODE(KEY_LOADMAP, code_key_loadmap, reloc_key_loadmap),
    CODE(LOADMAP_CB, code_loadmap_cb, reloc_loadmap_cb),
    CODE(POS_UPDATE_CB, code_pos_update_cb, reloc_pos_update_cb),
    CODE(ENT_SET_FACING, code_ent_set_facing, reloc_ent_set_facing),
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

unsigned long CodeAddr(int id) {
    if (!codedef_cache)
        InitCode();
    if (!codedef_cache[id])
        return 0;
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

static void RelocAddr(codedef *cd, unsigned long addr, reloc *r) {
    DWORD val;

    switch(r->type) {
        case ICON_STR:
            val = StringAddr(r->id);
            break;
        case ICON_DATA:
            val = DataAddr(r->id);
            break;
        case ICON_CODE_ABS:
        case ICON_CODE_REL:
            val = CodeAddr(r->id);
            break;
        case COH_ABS:
        case COH_REL:
            val = CohAddr(r->id);
            break;
        case IMMEDIATE:
            val = r->id;
            break;
        default:
            Bailout("Unhandled relocation type");
            return;
    }

    if (r->type == ICON_CODE_REL || r->type == COH_REL) {
        SETCALL(addr, val);
    } else {
        SETLONG(addr, val);
    }
}

static void ScanRelocs(codedef *cd) {
    unsigned long a;
    reloc *r = cd->relocs;

    if (r->type == RELOC_END)
        return;

    // scan for relocation marker sequences
    for (a = 0; a < cd->sz - 4; a++) {
        if (*(unsigned long*)(cd->code + a) == 0xD0ADADDE) {
            RelocAddr(cd, a, r);
            if ((++r)->type == RELOC_END)
                return;
        }
    }
}

void RelocateCode() {
    codedef *cd;

    // Generic relocations
    cd = icon_code;
    while (cd && cd->sz) {
        if (cd->relocs) {
            ScanRelocs(cd);
        }
        ++cd;
    }
}
