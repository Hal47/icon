/* vim: set sts=4 sw=4 et: */

/* Titan Icon
 * Copyright (C) 2013 Titan Network
 * All Rights Reserved
 *
 * This code is for educational purposes only and is not licensed for
 * redistribution in source form.
 */

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

// ===== enter_game =====
// Calling convention: N/A
//      No stack changes, all registers preserved
// Called by a hook inserted into the main game when the user clicks
// 'Enter Game'.
static unsigned char code_enter_game[] = {
0x60,                               // PUSHAD
// Lookup map to load in our data table based on player selection.
0x8B,0x0D,RELOC,                    // MOV ECX,DWORD PTR [$start_choice]
0x8B,0x04,0x8D,RELOC,               // MOV EAX,DWORD PTR DS:[ECX*4+$STR]
0xE8,RELOC,                         // CALL $load_map_demo
0xE8,RELOC,                         // CALL $find_spawns
// Get the player entity and its character sub-entry.
0xA1,RELOC,                         // MOV EAX,DWORD PTR [$player_ent]
0x8B,0x80,RELOC,                    // MOV EAX,DWORD PTR DS:[EAX+$charoff]
0x50,                               // PUSH EAX    ; save extra copy
// Copy attributes (HP, end, etc) from class defaults.
0x8D,0x90,0xA4,0x00,0x00,0x00,      // LEA EDX,[EAX+0A4]
0xE8,RELOC,                         // CALL $copy_attribs
// Do the same for max attribs.
0x58,                               // POP EAX     ; $player_ent+$charoff
0x8D,0x90,0x3C,0x04,0x00,0x00,      // LEA EDX,[EAX+43C]
0xE8,RELOC,                         // CALL $copy_attribs
// Create a keybind table for the player so the game doesn't crash...
0x8B,0x15,RELOC,                    // MOV EDX,DWORD PTR [$player_ent]
0x8B,0x52,0x30,                     // MOV EDX,DWORD PTR DS:[EDX+30]
0x83,0xBA,RELOC,0x00,               // CMP DWORD PTR DS:[EDX+$kboff],0
0x75,0x17,                          // JNE SHORT 005F
0x52,                               // PUSH EDX
0x68,0x00,0x30,0x00,0x00,           // PUSH 3000
0x6A,0x01,                          // PUSH 1
0xE8,RELOC,                         // CALL $calloc
0x83,0xC4,0x08,                     // ADD ESP,8
0x5A,                               // POP EDX
0x89,0x82,RELOC,                    // MOV DWORD PTR DS:[EDX+$kboff],EAX
// ...and initialize it.
0xE8,RELOC,                         // CALL $init_keybinds

// Control state stuff.
0xBF,RELOC,                         // MOV EDI,OFFSET $controls_from_server
// Populate movement speeds and surface physics.
0xB8,0x00,0x00,0x80,0x3F,           // MOV EAX,3F800000 (1.f)
0xB9,0x0F,0x00,0x00,0x00,           // MOV ECX,0F
0xF3,0xAB,                          // REP STOSD [EDI]
// There's a copy of surface physics in the entity for some reason.
0x8B,0x3D,RELOC,                    // MOV EDI,DWORD PTR DS:[$player_ent]
0x8B,0x7F,0x2C,                     // MOV EDI,DWORD PTR DS:[EDI+2C]
0x8D,0xBF,0xBC,0x00,0x00,0x00,      // LEA EDI,[EDI+0BC]
0xB9,0x0A,0x00,0x00,0x00,           // MOV ECX,0A
0xF3,0xAB,                          // REP STOSD [EDI]

// Set the clock.
0x8D,0x15,RELOC,                    // LEA EDX,[$game_time]
0xC7,0x02,0x00,0x00,0x40,0x41,      // MOV DWORD PTR DS:[EDX],41400000

// Set player spawn point.
0xA1,RELOC,                         // MOV EAX,DWORD PTR [$start_choice]
0xBA,0x0C,0x00,0x00,0x00,           // MOV EDX,0C
0xF7,0xE2,                          // MUL EDX
0x8D,0x90,RELOC,                    // LEA EDX,[EAX+$spawncoords]
0x8B,0x0D,RELOC,                    // MOV ECX,DWORD PTR [$player_ent]
0xE8,RELOC,                         // CALL $ent_teleport

// Set a database ID, hardcode 1 for the player.
0xA1,RELOC,                         // MOV EAX,DWORD PTR [$player_ent]
0x8D,0x50,0x74,                     // LEA EDX,[EAX+74]
0xC7,0x02,0x01,0x00,0x00,0x00,      // MOV DWORD PTR DS:[EDX], 1
0x8D,0x90,RELOC,                    // LEA EDX,[EAX+$svrid]
0xC7,0x02,0x01,0x00,0x00,0x00,      // MOV DWORD PTR DS:[EDX], 1
// Add it to the entity lookup table, too.
0x31,0xFF,                          // XOR EDI,EDI
0x47,                               // INC EDI
0x89,0x04,0xBD,RELOC,               // MOV DWORD PTR [$enttbl+EDI*4], EAX

// Set edit toolbar to absolute mode.
0x31,0xC0,                          // XOR EAX,EAX
0xB0,0x01,                          // MOV AL, 1
0xA2,RELOC,                         // MOV BYTE PTR [$edit_transform_abs], AL

0xE8,RELOC,                         // CALL $setup_binds

// Add easter egg if they picked the tutorial.
0x8B,0x0D,RELOC,                    // MOV ECX,DWORD PTR [$start_choice]
0x83,0xF9,0x00,                     // CMP ECX,0
0x75,0x05,                          // JNE SHORT out
0xE8,RELOC,                         // CALL $setup_tutorial

// out:
0x61,                               // POPAD
0xC3,                               // RETN
};
reloc reloc_enter_game[] = {
    { COH_ABS, COHVAR_START_CHOICE },
    { ICON_DATA, DATA_ZONE_MAP },
    { COH_REL, COHFUNC_LOAD_MAP_DEMO },
    { ICON_CODE_REL, CODE_FIND_SPAWNS },
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
    { COH_ABS, COHVAR_EDIT_TRANSFORM_ABS },
    { ICON_CODE_REL, CODE_SETUP_BINDS },
    { COH_ABS, COHVAR_START_CHOICE },
    { ICON_CODE_REL, CODE_SETUP_TUTORIAL },
    { RELOC_END, 0 }
};

// ===== setup_binds =====
// Calling convention: Custom
//      No stack changes
//      Invalidates EAX, EDX, EDI
// Initializes command processor and sets up key bindings
unsigned char code_setup_binds[] = {
// Init the command structure (sets things like argument count).
0x68,RELOC,                         // PUSH $commands
0xE8,RELOC,                         // CALL $cmd_init
0x83,0xC4,0x04,                     // ADD ESP, 4
// Allocate room for a keybind profile.
0x68,0x10,0x30,0x00,0x00,           // PUSH 3010
0x6A,0x01,                          // PUSH 1
0xE8,RELOC,                         // CALL $calloc
0x83,0xC4,0x08,                     // ADD ESP,8
0xA3,RELOC,                         // MOV DWORD PTR [$binds], EAX
// Add the new profile to the top of the stack.
0xE8,RELOC,                         // CALL $bind_push
0xA1,RELOC,                         // MOV EAX, DWORD PTR [$binds]
// 2 = flag for fallthrough to other profiles.
0xC6,0x00,0x02,                     // MOV BYTE PTR [EAX], 2
// Associate our handler callback.
0xBA,RELOC,                         // MOV EDX, $cmd_handler
0x89,0x90,0x08,0x30,0x00,0x00,      // MOV DWORD PTR [EAX+3008], EDX
0xBF,RELOC,                         // MOV EDI, OFFSET $bind_list
// loop:
// Iterate through the bind list {name, callback} and add them.
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
    { ICON_DATA, DATA_COMMANDS },
    { COH_REL, COHFUNC_CMD_INIT },
    { COH_REL, COHFUNC_CALLOC },
    { ICON_DATA, DATA_BINDS },
    { COH_REL, COHFUNC_BIND_PUSH },
    { ICON_DATA, DATA_BINDS },
    { ICON_CODE_ABS, CODE_CMD_HANDLER },
    { ICON_DATA, DATA_BIND_LIST },
    { COH_REL, COHFUNC_BIND },

    { RELOC_END, 0 }
};

// ===== cmd_handler =====
// Calling convention: cdecl
// Parameters:
//      command - pointer to a C string
//      x - DWORD integer
//      y - DWORD integer
// Callback function that the command parser calls when a user types in a
// slash command or presses a bound key. x and y contain the current mouse
// coordinates, but we don't use them here.
unsigned char code_cmd_handler[] = {
0x55,                           // PUSH EBP
0x89,0xE5,                      // MOV EBP, ESP
0x81,0xEC,0x60,0x04,0x00,0x00,  // SUB ESP, 460
0x53,                           // PUSH EBX
0x56,                           // PUSH ESI
0x57,                           // PUSH EDI
// cmd_parse needs a big state struct, make some room on the stack...
0x8D,0xBD,0xA0,0xFB,0xFF,0xFF,  // LEA EDI, [EBP-460]
// ...and zero fill it (not doing this caused all sorts of weird issues).
0xB9,0x18,0x01,0x00,0x00,       // MOV ECX, 118
0x31,0xC0,                      // XOR EAX, EAX
0xF3,0xAB,                      // REP STOSD [EDI]
// Send the temp struct, as well as the command were were passed and a pointer
// to our list of command tables (dept. of redundancy dept. approved) to
// the parser itself
0x8D,0x85,0xA0,0xFB,0xFF,0xFF,  // LEA EAX, [EBP-460]
0x8B,0x7D,0x08,                 // MOV EDI, DWORD PTR [EBP+8]
0x57,                           // PUSH EDI
0x68,RELOC,                     // PUSH $commands
0xE8,RELOC,                     // CALL $cmd_parse
0x83,0xC4,0x08,                 // ADD ESP, 8
// Any matches?
0x85,0xC0,                      // TEST EAX,EAX
0x74,0x1F,                      // JNZ SHORT out
0x8B,0x78,0x08,                 // MOV EDI, DWORD PTR [EAX+8]
// If this is a variable-only command we use an ID higher than CODE_END.
0x81,0xFF,RELOC,                // CMP EDI, [$CODE_END]
0x7D,0x14,                      // JGE SHORT out
// If it's below it's associated with a callback. look it up.
0x8B,0x14,0xBD,RELOC,           // MOV EDX, DWORD PTR [EDI*4+$command_funcs]
0x31,0xC0,                      // XOR EAX, EAX
// Make sure it's valid...
0x85,0xD2,                      // TEST EDX, EDX
0x74,0x07,                      // JZ SHORT out
// ...and call it.
0xFF,0xD2,                      // CALL EDX
// Return 1 so the command stops here and doesn't filter to other handlers,
0xB8,0x01,0x00,0x00,0x00,       // MOV EAX, 1
// out:
0x5F,                           // POP EDI
0x5E,                           // POP ESI
0x5B,                           // POP EBX
0xC9,                           // LEAVE
0xC3                            // RETN
};
reloc reloc_cmd_handler[] = {
    { ICON_DATA, DATA_COMMAND_LIST },
    { COH_REL, COHFUNC_CMD_PARSE },
    { IMMEDIATE, CODE_END },
    { ICON_DATA, DATA_COMMAND_FUNCS },
    { RELOC_END, 0 }
};

// ===== generic_mov =====
// Calling convention: stdcall
// Parameters:
//      entity - pointer to entity structure
//      name - pointer to C string
// Looks up a sequencer move based on name, and causes the specified entity
// to play that move next if it exists.
unsigned char code_generic_mov[] = {
// We need a local to store the result in.
0x83,0xEC,0x04,                     // SUB ESP,4
0x89,0xE0,                          // MOV EAX,ESP
0x57,                               // PUSH EDI

// Dive into the entity to get its sequencer info.
0x8B,0x7C,0xE4,0x0C,                // MOV EDI,DWORD PTR SS:[ESP+0C]
0x8B,0x57,0x28,                     // MOV EDX,DWORD PTR DS:[EDI+28]
0x8B,0x92,0x60,0x01,0x00,0x00,      // MOV EDX,DWORD PTR DS:[EDX+160]
// mov_by_name takes name, sequencer info, and a pointer to store the result.
0x50,                               // PUSH EAX
0x52,                               // PUSH EDX
0xFF,0x74,0xE4,0x18,                // PUSH DWORD PTR SS:[ESP+18]
0xE8,RELOC,                         // CALL $mov_by_name
0x83,0xC4,0x0C,                     // ADD ESP,0C
// Did we find a move?
0x84,0xC0,                          // TEST AL,AL
0x74,0x07,                          // JE SHORT notfound
// Yes. it's a 16-bit signed word, so put it in EAX.
0x0F,0xB7,0x44,0xE4,0x04,           // MOVZX EAX,WORD PTR SS:[ESP+04]
0xEB,0x02,                          // JMP SHORT go
// notfound:
0x31,0xC0,                          // XOR EAX,EAX
// go:
// The entity struct varies slightly between versions, hence the reloc.
0x89,0x87,RELOC,                    // MOV DWORD PTR [EDI+$next_mov],EAX
// Put a 0 in the sequencer timer. technically it's a float but all 0s works.
0x31,0xC0,                          // XOR EAX,EAX
0x89,0x87,0x58,0x01,0x00,0x00,      // MOV DWORD PTR [EDI+158], EAX

0x5F,                               // POP EDI
0x83,0xC4,0x04,                     // ADD ESP,4
0xC2,0x08,0x00,                     // RETN 8
};
reloc reloc_generic_mov[] = {
    { COH_REL, COHFUNC_MOV_BY_NAME },
    { COH_ABS, COHVAR_ENT_NEXT_MOV_OFFSET },
    { RELOC_END, 0 }
};

// ===== get_target =====
// Calling convention: Custom
//      No stack changes, returns in EAX
// Returns either the currently targeted entity, or the player if
// nothing is targeted.
unsigned char code_get_target[] = {
0xA1,RELOC,                         // MOV EAX, DWORD PTR [$target]
0x85,0xC0,                          // TEST EAX,EAX
0x74,0x01,                          // JZ notarget
0xC3,                               // RETN
// notarget:
0xA1,RELOC,                         // MOV EAX, DWORD PTR [$player_ent]
0xC3,                               // RETN
};
reloc reloc_get_target[] = {
    { COH_ABS, COHVAR_TARGET },
    { COH_ABS, COHVAR_PLAYER_ENT },
    { RELOC_END, 0 }
};

// ===== loadmap =====
// Calling convention: stdcall, probably
//      (OK... I haven't checked to make sure the COH calls don't clobber
//      non-stdcall registers)
// Parameters:
//      pathname - pointer to C string
// Common map loading routine. Clears out old map and any entities, loads
// new map by name, populates spawn point array, and moves the player to
// a random spawn.
unsigned char code_loadmap[] = {
// Is caller a dummy and passing a NULL?
0x8B,0x44,0xE4,0x04,                // MOV EAX, [ESP+4]
0x85,0xC0,                          // TEST EAX, EAX
0x75,0x01,                          // JNZ ok
0xC3,                               // RETN
// ok:
0x50,                               // PUSH EAX
0xE8,RELOC,                         // CALL $map_clear
0x58,                               // POP EAX
0xE8,RELOC,                         // CALL $load_map_demo
0xE8,RELOC,                         // CALL $clear_ents
0x31,0xC0,                          // XOR EAX, EAX
0xB0,0x02,                          // MOV AL, 2
0xA3,RELOC,                         // MOV DWORD_PTR [$num_ents], EAX
0xE8,RELOC,                         // CALL $find_spawns
0xE8,RELOC,                         // CALL $cmd_randomspawn
0xC2,0x04,0x00,                     // RETN 4
};
reloc reloc_loadmap[] = {
    { COH_REL, COHFUNC_MAP_CLEAR },
    { COH_REL, COHFUNC_LOAD_MAP_DEMO },
    { COH_REL, COHFUNC_CLEAR_ENTS },
    { ICON_DATA, DATA_NUM_ENTS },
    { ICON_CODE_REL, CODE_FIND_SPAWNS },
    { ICON_CODE_REL, CODE_CMD_RANDOMSPAWN },
    { RELOC_END, 0 }
};

// ===== find_spawns =====
// Calling convention: stdcall
// Parses the currently loaded map to find all the player spawn points.
unsigned char code_find_spawns[] = {
// Clear out old list if one exists.
0xA1,RELOC,                     // MOV EAX, DWORD PTR [$spawn_list]
0x85,0xC0,                      // TEST EAX, EAX
0x74,0x05,                      // JZ nolist
0xE8,RELOC,                     // CALL $free_array
// nolist:
// Set up our callback and walk the map.
0xB9,RELOC,                     // MOV ECX, OFFSET $map_root
0xB8,RELOC,                     // MOV EAX, OFFSET $map_traverser
0xE8,RELOC,                     // CALL $walk_map
// Save array pointer.
0xA3,RELOC,                     // MOV DWORD PTR [$spawn_list], EAX
0xC3,                           // RETN
};
reloc reloc_find_spawns[] = {
    { ICON_DATA, DATA_SPAWN_LIST },
    { COH_REL, COHFUNC_FREE_ARRAY },
    { COH_ABS, COHVAR_MAP_ROOT },
    { ICON_CODE_ABS, CODE_MAP_TRAVERSER },
    { COH_REL, COHFUNC_WALK_MAP },
    { ICON_DATA, DATA_SPAWN_LIST },
    { RELOC_END, 0 }
};

// ===== map_traverser =====
// Calling convention: cdecl
// Parameters:
//      definfo - pointer a structure containing info about the map def
// Callback function that the game's recursive map walker calls repeatedly.
// Checks each map def it's passed to see if it's a spawn point.
unsigned char code_map_traverser[] = {
0x55,                           // PUSH EBP
0x89,0xE5,                      // MOV EBP, ESP
// We're passed a pointer to information about this def node.
0x8B,0x55,0x08,                 // MOV EDX, DWORD PTR [EBP+8]
// Get the node itself.
0x8B,0x12,                      // MOV EDX, DWORD PTR [EDX]

// Check flags to see if it has any properties.
0xF6,0x42,0x3A,0x02,            // TEST BYTE PTR [EDX+3A], 02
0x74,0x20,                      // JZ SHORT nomatch
// Get pointer to the properties hash table.
0x8B,0xBA,0xE0,0x00,0x00,0x00,  // MOV EDI, DWORD PTR [EDX+E0]
// See if it has a property called "SpawnLocation".
0x52,                           // PUSH EDX
0x68,RELOC,                     // PUSH OFFSET $spawnlocation
0xE8,RELOC,                     // CALL $hash_lookup
0x83,0xC4,0x04,                 // ADD ESP, 4
0x5A,                           // POP EDX
0x85,0xC0,                      // TEST EAX, EAX
0x74,0x07,                      // JZ SHORT nomatch
// It does! return 3 to let the traverser know to add this to the results
0xB8,0x03,0x00,0x00,0x00,       // MOV EAX,3
0xEB,0x0F,                      // JMP SHORT out

// nomatch:
// Ok, found nothing, does this node's children have any properties?
0xF6,0x42,0x3A,0x04,            // TEST BYTE PTR [EDX+3A], 04
0x75,0x07,                      // JNZ SHORT childrenhaveprops
// Children have no properties, return 2 so the traverser skips them.
0xB8,0x02,0x00,0x00,0x00,       // MOV EAX,2
0xEB,0x02,                      // JMP SHORT out
// continue:
// Return 0 so the traverser will keep going.
0x31,0xC0,                      // XOR EAX, EAX

// out:
0xC9,                           // LEAVE
0xC3,                           // RETN
};
reloc reloc_map_traverser[] = {
    { ICON_STR, STR_SPAWNLOCATION },
    { COH_REL, COHFUNC_HASH_LOOKUP },
    { RELOC_END, 0 }
};

// ===== goto_spawn =====
// Calling convention: stdcall
// Parameters:
//      spawn - DWORD integer
// Teleports the player to a given spawn point. The parameter is an index
// into the spawn point array. Generally, nearby spawn points will be close
// together in the array due to how the map traverser works.
unsigned char code_goto_spawn[] = {
// Get argument passed in on stack.
0x8B,0x44,0xE4,0x04,                // MOV EAX, DWORD PTR [ESP+4]
0x8B,0x15,RELOC,                    // MOV EDX, DWORD PTR [$spawn_list]
// Make sure it's less than the size of the array.
0x8B,0x0A,                          // MOV ECX, DWORD PTR [EDX]
0x39,0xC8,                          // CMP EAX, ECX
0x72,0x03,                          // JB argok
0xC2,0x04,0x00,                     // RETN 4
// argok:
// Follow the array pointer itself, then go to the element we want.
0x8B,0x52,0x08,                     // MOV EDX, DWORD PTR [EDX+8]
0x8B,0x14,0x82,                     // MOV EDX, DWORD PTR [EDX+EAX*4]
// The location matrix starts at EDX+4, and we want the vector matrix[3]
0x83,0xC2,0x28,                     // ADD EDX, 28
// Finally, send the player there.
0x8B,0x0D,RELOC,                    // MOV ECX, DWORD PTR [$player_ent]
0xE8,RELOC,                         // CALL $ent_teleport
0xC2,0x04,0x00,                     // RETN 4
};
reloc reloc_goto_spawn[] = {
    { ICON_DATA, DATA_SPAWN_LIST },
    { COH_ABS, COHVAR_PLAYER_ENT },
    { COH_REL, COHFUNC_ENT_TELEPORT },
    { RELOC_END, 0 }
};

// ===== ent_set_facing =====
// Calling convention: stdcall
// Parameters:
//      entity - pointer to entity struct
//      facing - pointer to a float[3] vector
// Make the given entity face in a particular direction. The facing vector
// is specified in radians. If the entity is the player, also updates the
// control structure so they don't immediately snap back.
unsigned char code_ent_set_facing[] = {
0x55,                           // PUSH EBP
0x89,0xE5,                      // MOV EBP, ESP
0x56,                           // PUSH ESI
0x57,                           // PUSH EDI

// Set up EAX to point at the entity's transform matrix...
0x8B,0x45,0x08,                 // MOV EAX, DWORD PTR [EBP+8]
0x8D,0x40,0x38,                 // LEA EAX, [EAX+38]
// ...and ESI to our vector parameter.
0x8B,0x75,0x0C,                 // MOV ESI, DWORD PTR [EBP+C]
// Call a convenient function to update the matrix based on the vector.
0xE8,RELOC,                     // CALL $matrix_from_pyr
// Check and see if this is the player.
0x8B,0x45,0x08,                 // MOV EAX, DWORD PTR [EBP+8]
0x3B,0x05,RELOC,                // CMP EAX, DWORD PTR [$player_ent]
0x75,0x0F,                      // JNE SHORT out
// Yes it is, so copy the passed-in vector to the control state too.
0xBF,RELOC,                     // MOV EDI, OFFSET $controls
0x8D,0x7F,0x04,                 // LEA EDI, [EDI+4]
0xB9,0x03,0x00,0x00,0x00,       // MOV ECX, 3
0xF3,0xA5,                      // REP MOVSD [EDI], [ESI]
// out:
0x5F,                           // POP EDI
0x5E,                           // POP ESI
0xC9,                           // LEAVE
0xC2,0x08,0x00,                 // RETN 8
};
reloc reloc_ent_set_facing[] = {
    { COH_REL, COHFUNC_MATRIX_FROM_PYR },
    { COH_ABS, COHVAR_PLAYER_ENT },
    { COH_ABS, COHVAR_CONTROLS },
    { COH_REL, COHFUNC_MATRIX_FROM_PYR },
    { RELOC_END, 0 }
};

// ===== setup_tutorial =====
// Calling convention: Custom
//      No stack changes
//      Clobbers EAX, ECD, EDX, EDI
// Sets up something special in the tutorial...
unsigned char code_setup_tutorial[] = {
0x55,                           // PUSH EBP
0x89,0xE5,                      // MOV EBP, ESP
0x68,RELOC,                     // PUSH OFFSET $coyote_name
0x6A,0x01,                      // PUSH 1
0xE8,RELOC,                     // CALL $create_ent
0x50,                           // PUSH EAX
0x50,                           // PUSH EAX
0x83,0x88,RELOC,0x08,           // OR DWORD PTR [EAX+$ent_flags], 8
0x89,0xC1,                      // MOV ECX, EAX
0xBA,RELOC,                     // MOV EDX, OFFSET $coyote_pos
0xE8,RELOC,                     // CALL $ent_teleport
0x58,                           // POP EAX
0x68,RELOC,                     // PUSH OFFSET $coyote_rot
0x50,                           // PUSH EAX
0xE8,RELOC,                     // CALL $ent_set_facing
0x81,0xEC,0x94,0x00,0x00,0x00,  // SUB ESP, 94
0x89,0xE7,                      // MOV EDI, ESP
0xB9,0x25,0x00,0x00,0x00,       // MOV ECX, 25
0x31,0xC0,                      // XOR EAX, EAX
0x57,                           // PUSH EDI
0xF3,0xAB,                      // REP STOSD [EDI]
0x5F,                           // POP EDI
0x89,0xE0,                      // MOV EAX, ESP
0x83,0xEC,0x04,                 // SUB ESP, 4
0x54,                           // PUSH ESP
0x68,RELOC,                     // PUSH OFFSET $coyote_costume
0xE8,RELOC,                     // CALL $get_npc_costume
0x83,0xC4,0x08,                 // ADD ESP, 8
0x8B,0x0C,0xE4,                 // MOV ECX, DWORD PTR [ESP]
0x89,0x8F,0x88,0x00,0x00,0x00,  // MOV DWORD PTR [EDI+88], ECX
0xFF,0x75,0xFC,                 // PUSH DWORD PTR [EBP-4]
0x89,0xF8,                      // MOV EAX, EDI
0xE8,RELOC,                     // CALL $ent_set_costume_demo
0x83,0xC4,0x04,                 // ADD ESP, 4
0x68,RELOC,                     // PUSH OFFSET $coyote_mov
0xFF,0x75,0xFC,                 // PUSH DWORD PTR [EDI-4]
0xE8,RELOC,                     // CALL $generic_mov

0xC9,                           // LEAVE
0xC3,                           // RETN
};
reloc reloc_setup_tutorial[] = {
    { ICON_STR, STR_COYOTE_NAME },
    { ICON_CODE_REL, CODE_CREATE_ENT },
    { COH_ABS, COHVAR_ENT_FLAGS_OFFSET },
    { ICON_DATA, DATA_COYOTE_POS },
    { COH_REL, COHFUNC_ENT_TELEPORT },
    { ICON_DATA, DATA_COYOTE_ROT },
    { ICON_CODE_REL, CODE_ENT_SET_FACING },
    { ICON_STR, STR_COYOTE_MODEL },
    { COH_REL, COHFUNC_GET_NPC_COSTUME },
    { COH_REL, COHFUNC_ENT_SET_COSTUME_DEMO },
    { ICON_STR, STR_COYOTE_MOV },
    { ICON_CODE_REL, CODE_GENERIC_MOV },
    { RELOC_END, 0 }
};

// ===== create_ent =====
// Calling convention: stdcall
// Parameters:
//      type - integer
//      name - pointer to C string
// Creates an entity of [type] (1 = NPC, 2 = Player, 4 = Critter, 8 = Door)
// with the specified name. Creates player and character structures if
// applicable. Returns a pointer to the entity.
unsigned char code_create_ent[] = {
0x55,                           // PUSH EBP
0x89,0xE5,                      // MOV EBP, ESP
0x53,                           // PUSH EBX
0x57,                           // PUSH EDI
// Allocate the new entity.
0xE8,RELOC,                     // CALL $ent_new
// Get a new ID for it (this will be the "server" ID).
0xBA,RELOC,                     // MOV EDX, OFFSET $num_ents
0x8B,0x1A,                      // MOV EBX, DWORD PTR [EDX]
// Put it in the game's ID lookup table.
0x89,0x04,0x9D,RELOC,           // MOV DWORD PTR [$enttbl+EBX*4], EAX
// Increment the ID counter.
0xFF,0x02,                      // INC DWORD PTR [EDX]
// Set the entity's "server" ID.
0x89,0x98,RELOC,                // MOV DWORD PTR [EAX+$svrid], EBX
// Make it look like a demoplay entity so that it doesn't try to predict the
// motion and snap it back to 0,0,0.
0x89,0x98,RELOC,                // MOV DWORD PTR [EAX+$demo], EBX
// See what ID the client assigned this enity, and update its info table
// with the type.
0x8B,0x4D,0x08,                 // MOV ECX, DWORD PTR [EBP+08]
0x8B,0x58,0x04,                 // MOV EBX, DWORD PTR [EAX+04]
0x89,0x0C,0xDD,RELOC,           // MOV DWORD PTR [ent_types+EBX*8], ECX
// Allocate and initialize all the usual fields.
0x89,0xC7,                      // MOV EDI, EAX
0x50,                           // PUSH EAX
0xE8,RELOC,                     // CALL $ent_initplayer
0x83,0xC4,0x04,                 // ADD ESP, 4
// See if this is a player or a critter.
0x8B,0x4D,0x08,                 // MOV ECX, DWORD PTR [EBP+08]
0x83,0xF9,0x02,                 // CMP ECX,2
0x72,0x40,                      // JB SHORT nonplayer
0x83,0xF9,0x02,                 // CMP ECX,4
0x77,0x3B,                      // JA short nonplayer
// If so, its character structure will have been populated by initplayer.
0x8B,0x97,RELOC,                // MOV EDX, DWORD PTR [EDI+$char_offset]
0x57,                           // PUSH EDI     ; for init_char call later
0x52,                           // PUSH EDX
// Set the character's parent pointer back to us.
0x89,0x7A,0x64,                 // MOV DWORD PTR [EDX+64], EDI
0x57,                           // PUSH EDI
// Get a (dummy) origin and class.
0xBF,RELOC,                     // MOV EDI, OFFSET $origin_tbl
0x68,RELOC,                     // PUSH OFFSET $default_origin
0xE8,RELOC,                     // CALL $get_origin
0x83,0xC4,0x04,                 // ADD ESP, 4
0x50,                           // PUSH EAX
0xBF,RELOC,                     // MOV EDI, OFFSET $class_tbl
0x68,RELOC,                     // PUSH OFFSET $default_class
0xE8,RELOC,                     // CALL $get_class
0x83,0xC4,0x04,                 // ADD ESP, 4
// Finish the rest of the initalization using those.
0x59,                           // POP ECX
0x5F,                           // POP EDI
0xE8,RELOC,                     // CALL $ent_initchar
0x83,0xC4,0x08,                 // ADD ESP, 8
// nonplayer:
// Set the name if the caller wasn't a dummy and passed a NULL.
0x8B,0x07,                      // MOV EAX, DWORD PTR [EDI]
0x8B,0x4D,0x0C,                 // MOV ECX, DWORD PTR [EBP+C]
0x85,0xC9,                      // TEST ECX, ECX
0x74,0x05,                      // JZ out
0xE8,RELOC,                     // CALL $strcpy
// out:
// Return pointer to the new entity.
0x89,0xF8,                      // MOV EAX, EDI
0x5F,                           // POP EDI
0x5B,                           // POP EBX
0xC9,                           // LEAVE
0xC2,0x08,0x00                  // RETN 8
};
reloc reloc_create_ent[] = {
    { COH_REL, COHFUNC_ENT_NEW },
    { ICON_DATA, DATA_NUM_ENTS },
    { COH_ABS, COHVAR_ENTTBL },
    { COH_ABS, COHVAR_ENT_SERVERID_OFFSET },
    { COH_ABS, COHVAR_ENT_DEMO_OFFSET },
    { COH_ABS, COHVAR_ENT_TYPES },
    { COH_REL, COHFUNC_ENT_INITPLAYER },
    { COH_ABS, COHVAR_ENT_CHAR_OFFSET },
    { COH_ABS, COHVAR_ORIGIN_TBL },
    { ICON_STR, STR_DEFAULT_ORIGIN },
    { COH_REL, COHFUNC_GET_ORIGIN },
    { COH_ABS, COHVAR_CLASS_TBL },
    { ICON_STR, STR_DEFAULT_CLASS },
    { COH_REL, COHFUNC_GET_CLASS },
    { COH_REL, COHFUNC_ENT_INITCHAR },
    { COH_REL, COHFUNC_STRCPY },
    { RELOC_END, 0 }
};

// ===== delete_ent =====
// Calling convention: stdcall
// Parameters:
//      entity - pointer to entity
// Deletes the specified entity.
unsigned char code_delete_ent[] = {
0x55,                           // PUSH EBP
0x89,0xE5,                      // MOV EBP, ESP
0x53,                           // PUSH EBX

// Get the entity's "server" ID.
0x8B,0x55,0x08,                 // MOV EDX, DWORD PTR [EBP+8]
0x8B,0x9A,RELOC,                // MOV EBX, DWORD PTR [EDX+$svrid]
// Remove it from the lookup table.
0x31,0xC0,                      // XOR EAX, EAX
0x89,0x04,0x9D,RELOC,           // MOV DWORD PTR [$enttbl+EBX*4], EAX
// Finally deallocate it.
0x52,                           // PUSH EDX
0xE8,RELOC,                     // CALL $ent_free
0x83,0xC4,0x04,                 // ADD ESP, 4

0x5B,                           // POP EBX
0xC9,                           // LEAVE
0xC2,0x04,0x00                  // RETN 4
};
reloc reloc_delete_ent[] = {
    { COH_ABS, COHVAR_ENT_SERVERID_OFFSET },
    { COH_ABS, COHVAR_ENTTBL },
    { COH_REL, COHFUNC_ENT_FREE },
    { RELOC_END, 0 }
};

// ===== move_ent_to_player =====
// Calling convention: stdcall
// Parameters:
//      entity - pointer to entity
// Moves the specified entity to the player's position and facing, bumping
// them out of the way. Citizens > All.
unsigned char code_move_ent_to_player[] = {
0x55,                           // PUSH EBP
0x89,0xE5,                      // MOV EBP, ESP
0x56,                           // PUSH ESI

// Get the player's transformation matrix.
0x8B,0x0D,RELOC,                // MOV ECX, DWORD PTR [$player_ent]
0x8D,0x49,0x38,                 // LEA ECX, [ECX+38]
// Apply that matrix to the entity.
0x8B,0x75,0x08,                 // MOV ESI, DWORD PTR [EBP+08]
0xE8,RELOC,                     // CALL $ent_set_matrix

0x5E,                           // POP ESI
0xC9,                           // LEAVE
0xC2,0x04,0x00                  // RETN 4
};
reloc reloc_move_ent_to_player[] = {
    { COH_ABS, COHVAR_PLAYER_ENT },
    { COH_REL, COHFUNC_ENT_SET_MATRIX },
    { RELOC_END, 0 }
};


// ===== cmd_fly =====
// Calling convention: Custom
//      No stack changes
//      Clobbers EAX, EBX, ECX, EDX, EDI
// Makes the player fly like an eagle! Or something relatively close.
unsigned char code_cmd_fly[] = {
// Get a pointer to a copy of the physics info that entities have some some
// unknown reason. Put it in EDI for the duration.
0x8B,0x3D,RELOC,                    // MOV EDI,DWORD PTR [$player_ent]
0x8B,0x7F,0x2C,                     // MOV EDI,DWORD PTR DS:[EDI+2C]
0x8D,0xBF,0xA8,0x00,0x00,0x00,      // LEA EDI,[EDI+0A8]
// Also need the client's idea of what the "server" is telling it that its
// controls should look like.
0xBA,RELOC,                         // MOV EDX,OFFSET $controls_from_server
0xB1,0x01,                          // MOV CL,1
0xB3,0x08,                          // MOV BL,8
// Flight is bit 0 in the entity's physics flags. Flip it.
0x30,0x4F,0x3C,                     // XOR BYTE PTR DS:[EDI+3C],CL
// See if we just turned it on or off.
0x84,0x4F,0x3C,                     // TEST BYTE PTR DS:[EDI+3C],CL
0x74,0x0F,                          // JZ SHORT flyoff
// We just turned it on, so turn it on in the control state, too.
0x08,0x5A,0x3C,                     // OR BYTE PTR DS:[EDX+3C],BL
// Increase traction and friction in the air to 10.0. Increase max speed in
// the air to 5.0. Yes, these are floats represented directly in hex so
// I can copy them without using FPU instructions.
0xB8,0x00,0x00,0x20,0x41,           // MOV EAX,41200000
0xBB,0x00,0x00,0xA0,0x40,           // MOV EBX,40A00000
0xEB,0x0C,                          // JMP SHORT save
// flyoff:
// Flight is now off, so clear the bit from the control state to match.
0xF6,0xD3,                          // NOT BL
0x20,0x5A,0x3C,                     // AND BYTE PTR SS:[EDX+3C],BL
// And set all the physics parameters back to 1.0.
0xB8,0x00,0x00,0x80,0x3F,           // MOV EAX,3F800000
0x89,0xC3,                          // MOV EBX,EAX
// save:
// Save the values set up above. In order, max total speed,
// traction (air), friction (air), max speed (air).
0x89,0x5F,0x0C,                     // MOV DWORD PTR DS:[EDI+0C],EBX
0x89,0x47,0x28,                     // MOV DWORD PTR DS:[EDI+28],EAX
0x89,0x47,0x2C,                     // MOV DWORD PTR DS:[EDI+2C],EAX
0x89,0x5F,0x38,                     // MOV DWORD PTR DS:[EDI+38],EBX
0xC3,                               // RETN
};
reloc reloc_cmd_fly[] = {
    { COH_ABS, COHVAR_PLAYER_ENT },
    { COH_ABS, COHVAR_CONTROLS_FROM_SERVER },
    { RELOC_END, 0 }
};

// ===== cmd_torch =====
// Calling convention: stdcall
// Just a wrapper around generic_mov to be called as the command handler.
unsigned char code_cmd_torch[] = {
0x8B,0x15,RELOC,                    // MOV EDX,DWORD PTR [$player_ent]
0xB8,RELOC,                         // MOV EAX, OFFSET $holdtorch
0x50,                               // PUSH EAX
0x52,                               // PUSH EDX
0xE8,RELOC,                         // CALL $generic_mov
0xC3,                               // RETN
};
reloc reloc_cmd_torch[] = {
    { COH_ABS, COHVAR_PLAYER_ENT },
    { ICON_STR, STR_HOLDTORCH },
    { ICON_CODE_REL, CODE_GENERIC_MOV },
    { RELOC_END, 0 }
};

// ===== cmd_nocoll =====
// Calling convention: stdcall
// Handler for the enhanced 'noclip' command.
unsigned char code_cmd_nocoll[] = {
// Flip the nocoll state
0xBA,RELOC,                         // MOV EDX, OFFSET $nocoll
0x83,0x32,0x01,                     // XOR DWORD PTR [EDX], 1
0x74,0x07,                          // JZ offnow
0xB8,RELOC,                         // MOV EAX, $noclip_on
0xEB,0x05,                          // JMP display
// offnow:
0xB8,RELOC,                         // MOV EAX, $noclip_off
// display:
// Show the message. 0x6A sign extends, so pushing -1 is a shortcut for
// pushing 0xFFFFFFFF (white with full alpha).
0x6A,0xFF,                          // PUSH -1
0x50,                               // PUSH EAX
0xE8,RELOC,                         // CALL annoying_alert
0x83,0xC4,0x08,                     // ADD ESP,8
0xC3,                               // RETN
};
reloc reloc_cmd_nocoll[] = {
    { COH_ABS, COHVAR_NOCOLL },
    { ICON_STR, STR_NOCLIP_ON },
    { ICON_STR, STR_NOCLIP_OFF },
    { COH_REL, COHFUNC_ANNOYING_ALERT },
    { RELOC_END, 0 }
};

// ===== cmd_seeall =====
// Calling convention: stdcall
// Very simple handler that toggles the /see_everything bit.
unsigned char code_cmd_seeall[] = {
0xBA,RELOC,                         // MOV EDX, OFFSET $seeall
0x83,0x32,0x01,                     // XOR DWORD PTR [EDX], 1
0xC3,                               // RETN
};
reloc reloc_cmd_seeall[] = {
    { COH_ABS, COHVAR_SEEALL },
    { RELOC_END, 0 }
};

// ===== cmd_coords =====
// Calling convention: stdcall
// Very simple handler that toggles the coordinate display status.
unsigned char code_cmd_coords[] = {
0xBA,RELOC,                         // MOV EDX, OFFSET $draw_edit_bar
0x83,0x32,0x01,                     // XOR DWORD PTR [EDX], 1
0xC3,                               // RETN
};
reloc reloc_cmd_coords[] = {
    { ICON_DATA, DATA_SHOW_TOOLBAR },
    { RELOC_END, 0 }
};

// ===== cmd_detach =====
// Calling convention: stdcall
// Wrapper around the normal COH /detach_camera handler that also provides
// on-screen feedback.
unsigned char code_cmd_detach[] = {
// Get current detached state and invert it.
0x8B,0x15,RELOC,                    // MOV EDX, DWORD PTR [$is_detached]
0x83,0xF2,0x01,                     // XOR EDX, 1
0x52,                               // PUSH EDX
// Call detach_camera with the standard camera and new state.
0x68,RELOC,                         // PUSH OFFSET $camera
0x52,                               // PUSH EDX
0xE8,RELOC,                         // CALL $detach_camera
0x83,0xC4,0x08,                     // ADD ESP, 8
0x5A,                               // POP EDX
// Push color (0xFFFFFFFF) for later call to annoying_alert.
0x6A,0xFF,                          // PUSH -1
// Push appropriate message to display.
0x85,0xD2,                          // TEST EDX,EDX
0x74,0x07,                          // JZ SHORT reattached
0x68,RELOC,                         // PUSH OFFSET $cameradetach
0xEB,0x05,                          // JMP SHORT display
// reattached:
0x68,RELOC,                         // PUSH OFFSET $cameraattach
// display:
0xE8,RELOC,                         // CALL $annoying_alert
0x83,0xC4,0x08,                     // ADD ESP, 8
0xC3,                               // RETN
};
reloc reloc_cmd_detach[] = {
    { COH_ABS, COHVAR_CAM_IS_DETACHED },
    { COH_ABS, COHVAR_CAMERA },
    { COH_REL, COHFUNC_DETACH_CAMERA },
    { ICON_STR, STR_CAMERADETACH },
    { ICON_STR, STR_CAMERAATTACH },
    { COH_REL, COHFUNC_ANNOYING_ALERT },
    { RELOC_END, 0 }
};

// ===== cmd_loadmap =====
// Calling convention: stdcall
// Simple wrapper around loadmap that uses the static command buffer.
unsigned char code_cmd_loadmap[] = {
0x68,RELOC,                         // PUSH $param1
0xE8,RELOC,                         // CALL $loadmap
0xC3,                               // RETN
};
reloc reloc_cmd_loadmap[] = {
    { ICON_DATA, DATA_PARAM1 },
    { ICON_CODE_REL, CODE_LOADMAP },
    { RELOC_END, 0 }
};

// ===== cmd_loadmap_prompt =====
// Calling convention: stdcall
// Displays a dialog box asking for a map to load.
unsigned char code_cmd_loadmap_prompt[] = {
// The COH dialog routine support text input but takes a lot of other
// parameters we don't care about.
0x6A,0x00,                          // PUSH 00
// Max text length is 255
0x68,0xFF,0x00,0x00,0x00,           // PUSH 00FF
0x6A,0x00,                          // PUSH 00
0x6A,0x00,                          // PUSH 00
0x6A,0x00,                          // PUSH 00
0x6A,0x00,                          // PUSH 00
0x6A,0x00,                          // PUSH 00
0x6A,0x00,                          // PUSH 00
0x6A,0x00,                          // PUSH 00
// Callback function for 'ok'.
0x68,RELOC,                         // PUSH OFFSET $loadmap_cb
0x6A,0x00,                          // PUSH 00
// Text for the dialog box.
0x68,RELOC,                         // PUSH OFFSET $mapfile
// These are screen coordinates, -1 means we don't care (and it centers it).
0x6A,0xFF,                          // PUSH -1
0x6A,0xFF,                          // PUSH -1
// Type 7 is a text input box.
0x6A,0x07,                          // PUSH 7
0xE8,RELOC,                         // CALL $dialog
0x83,0xC4,0x3C,                     // ADD ESP, 3C
0xC3,                               // RETN
};
reloc reloc_cmd_loadmap_prompt[] = {
    { ICON_CODE_ABS, CODE_LOADMAP_CB },
    { ICON_STR, STR_MAPFILE },
    { COH_REL, COHFUNC_DIALOG },
    { RELOC_END, 0 }
};

// ===== cmd_mov =====
// Calling convention: stdcall
// /mov handler. Just wraps generic_mov using the target as determined by
// get_target.
unsigned char code_cmd_mov[] = {
0xE8,RELOC,                     // CALL $get_target
0x68,RELOC,                     // PUSH $param1
0x50,                           // PUSH EAX
0xE8,RELOC,                     // CALL $generic_mov
0xC3,                           // RETN
};
reloc reloc_cmd_mov[] = {
    { ICON_CODE_REL, CODE_GET_TARGET },
    { ICON_DATA, DATA_PARAM1 },
    { ICON_CODE_REL, CODE_GENERIC_MOV },
    { RELOC_END, 0 }
};

// ===== cmd_prevspawn =====
// Calling convention: stdcall
// Handler to cycle through spawn points.
unsigned char code_cmd_prevspawn[] = {
// Get size member from array struct.
0xA1,RELOC,                     // MOV EAX, DWORD PTR [$spawn_list]
0x8B,0x00,                      // MOV EAX, DWORD PTR [EAX]
// Get last spawn.
0x8B,0x0D,RELOC,                // MOV ECX, DWORD PTR [$last_spawn]
0x49,                           // DEC ECX
// See if it's still in range using unsigned math so that a rollover below
// 0 will be "above" the range. This is done so that the logic can work
// the same as nextspawn.
0x39,0xC1,                      // CMP ECX, EAX
0x72,0x03,                      // JB SHORT inrange
// Went below zero, reset it to size - 1
0x89,0xC1,                      // MOV ECX, EAX
0x49,                           // DEC ECX
// inrange:
0x89,0x0D,RELOC,                // MOV DWORD PTR [$last_spawn], ECX
0x51,                           // PUSH ECX
0xE8,RELOC,                     // CALL $goto_spawn
0xC3,                           // RETN
};
reloc reloc_cmd_prevspawn[] = {
    { ICON_DATA, DATA_SPAWN_LIST },
    { ICON_DATA, DATA_LAST_SPAWN },
    { ICON_DATA, DATA_LAST_SPAWN },
    { ICON_CODE_REL, CODE_GOTO_SPAWN },
    { RELOC_END, 0 }
};

// ===== cmd_nextspawn =====
// Calling convention: stdcall
// Handler to cycle through spawn points.
unsigned char code_cmd_nextspawn[] = {
// Get size member from array struct.
0xA1,RELOC,                     // MOV EAX, DWORD PTR [$spawn_list]
0x8B,0x00,                      // MOV EAX, DWORD PTR [EAX]
// Get last spawn.
0x8B,0x0D,RELOC,                // MOV ECX, DWORD PTR [$last_spawn]
0x41,                           // INC ECX
// See if it's still in range.
0x39,0xC1,                      // CMP ECX, EAX
0x72,0x02,                      // JB SHORT inrange
// Too high, go back to 0.
0x31,0xC9,                      // XOR ECX, ECX
// inrange:
0x89,0x0D,RELOC,                // MOV DWORD PTR [$last_spawn], ECX
0x51,                           // PUSH ECX
0xE8,RELOC,                     // CALL $goto_spawn
0xC3,                           // RETN
};
reloc reloc_cmd_nextspawn[] = {
    { ICON_DATA, DATA_SPAWN_LIST },
    { ICON_DATA, DATA_LAST_SPAWN },
    { ICON_DATA, DATA_LAST_SPAWN },
    { ICON_CODE_REL, CODE_GOTO_SPAWN },
    { RELOC_END, 0 }
};

// ===== cmd_randomspawn =====
// Calling convention: stdcall
// Handler to jump to a random spawn point. Also called by loadmap.
unsigned char code_cmd_randomspawn[] = {
0x57,                           // PUSH EDI
// Get size member from array struct.
0x8B,0x3D,RELOC,                // MOV EDI,DWORD PTR [$spawn_list]
0x8B,0x0F,                      // MOV ECX,DWORD PTR [EDI]
// Make sure we found some, to avoid dividing by zero or worse.
0x85,0xC9,                      // TEST ECX,ECX
0x74,0x15,                      // JZ SHORT out
0x31,0xD2,                      // XOR EDX,EDX
0xE8,RELOC,                     // CALL $rand
0xF7,0x37,                      // DIV DWORD PTR [EDI]
// Save which one we picked so that prev/next work right.
0x89,0x15,RELOC,                // MOV DWORD PTR [$last_spawn], EDX
// Send the player there.
0x52,                           // PUSH EDX
0xE8,RELOC,                     // CALL $goto_spawn
// out:
0x5F,                           // POP EDI
0xC3,                           // RETN
};
reloc reloc_cmd_randomspawn[] = {
    { ICON_DATA, DATA_SPAWN_LIST },
    { COH_REL, COHFUNC_RAND },
    { ICON_DATA, DATA_LAST_SPAWN },
    { ICON_CODE_REL, CODE_GOTO_SPAWN },
    { RELOC_END, 0 }
};

// ===== cmd_spawnnpc =====
// Calling convention: stdcall
// Handler for spawning an NPC right on top of the player.
unsigned char code_cmd_spawnnpc[] = {
0x55,                           // PUSH EBP
0x89,0xE5,                      // MOV EBP, ESP
// Allocate space for the demo costume. Zero it all out.
0x81,0xEC,0x94,0x00,0x00,0x00,  // SUB ESP, 94
0x89,0xF8,                      // MOV EAX, EDI
0x89,0xE7,                      // MOV EDI, ESP
0x50,                           // PUSH EAX     ; preserve EDI for stdcall
0xB9,0x25,0x00,0x00,0x00,       // MOV ECX, 25
0x31,0xC0,                      // XOR EAX, EAX
0xF3,0xAB,                      // REP STOSD [EDI]
0x5F,                           // POP EDI
// First see if this costume exists and the user actually specified a name.
// NPC ID goes at offset 88 of the demo costume info, which starts at EBP-94
0x8D,0x45,0xF4,                 // LEA EAX, [EBP-0C]
0x50,                           // PUSH EAX
0x68,RELOC,                     // PUSH OFFSET $param1
0xE8,RELOC,                     // CALL $get_npc_costume
0x83,0xC4,0x08,                 // ADD ESP, 8
// Test the first byte of param2 to make sure it's not NULL.
0xB9,RELOC,                     // MOV ECX, OFFSET $param2
0x0F,0xB6,0x11,                 // MOVZX EDX, BYTE PTR [ECX]
0x85,0xD2,                      // TEST EDX, EDX
0x74,0x04,                      // JZ SHORT bad
// Also test return value of get_npc_costume.
0x85,0xC0,                      // TEST EAX, EAX
0x75,0x02,                      // JNZ short paramsok
// bad:
0xC9,                           // LEAVE
0xC3,                           // RETN

// paramsok:
// Everything checks out, so create the entity.
0x51,                           // PUSH ECX     ; $param2
0x6A,0x01,                      // PUSH 1
0xE8,RELOC,                     // CALL $create_ent
0x50,                           // PUSH EAX     ; for later call to move_ent
// Abuse demo playback's costume setting thing.
0x50,                           // PUSH EAX
0x8D,0x85,0x6C,0xFF,0xFF,0xFF,  // LEA EAX, [EBP-94]
0xE8,RELOC,                     // CALL $ent_set_costume_demo
0x83,0xC4,0x04,                 // ADD ESP, 4
// Now move the newly created entity to the player.
0xE8,RELOC,                     // CALL $move_ent_to_player

0xC9,                           // LEAVE
0xC3                            // RETN
};
reloc reloc_cmd_spawnnpc[] = {
    { ICON_DATA, DATA_PARAM1 },
    { COH_REL, COHFUNC_GET_NPC_COSTUME },
    { ICON_DATA, DATA_PARAM2 },
    { ICON_CODE_REL, CODE_CREATE_ENT },
    { COH_REL, COHFUNC_ENT_SET_COSTUME_DEMO },
    { ICON_CODE_REL, CODE_MOVE_ENT_TO_PLAYER },
    { RELOC_END, 0 }
};

// ===== cmd_movenpc =====
// Calling convention: stdcall
// Handler for moving an NPC to the player's position.
unsigned char code_cmd_movenpc[] = {
0xA1,RELOC,                     // MOV EAX, DWORD PTR [$target]
0x85,0xC0,                      // TEST EAX,EAX
0x75,0x01,                      // JNZ hastarget
0xC3,                           // RETN
// hastarget,
0x50,                           // PUSH EAX
0xE8,RELOC,                     // CALL $move_ent_to_player
0xC3,                           // RETN
};
reloc reloc_cmd_movenpc[] = {
    { COH_ABS, COHVAR_TARGET },
    { ICON_CODE_REL, CODE_MOVE_ENT_TO_PLAYER },
    { RELOC_END, 0 }
};

// ===== cmd_deletenpc =====
// Calling convention: stdcall
// Handler for deleting an NPC. Wraps delete_ent with some sanity checking.
unsigned char code_cmd_deletenpc[] = {
0xA1,RELOC,                     // MOV EAX, DWORD PTR [$target]
0x85,0xC0,                      // TEST EAX,EAX
0x75,0x01,                      // JNZ hastarget
0xC3,                           // RETN
// hastarget:
// Make sure the player isn't a dummy and trying to delete themselves, as it
// will crash (only possible if you're a cheater and have selectanyentity on).
0x8B,0x15,RELOC,                // MOV EDX, DWORD PTR [$player_ent]
0x39,0xD0,                      // CMP EAX, EDX
0x75,0x01,                      // JNE targetok
0xC3,                           // RETN
// targetok:
0x50,                           // PUSH EAX
0xE8,RELOC,                     // CALL $delete_ent
// Make sure that the current target is cleared out.
0x31,0xC0,                      // XOR EAX,EAX
0xA3,RELOC,                     // MOV DWORD PTR [$target], EAX
0xC3,                           // RETN
};
reloc reloc_cmd_deletenpc[] = {
    { COH_ABS, COHVAR_TARGET },
    { COH_ABS, COHVAR_PLAYER_ENT },
    { ICON_CODE_REL, CODE_DELETE_ENT },
    { COH_ABS, COHVAR_TARGET },
    { RELOC_END, 0 }
};


// ===== loadmap_cb =====
// Calling convention: cdecl
// Callback used by dialog box. Just gets the user input and hands it off
// to loadmap.
unsigned char code_loadmap_cb[] = {
0xE8,RELOC,                         // CALL $dialog_get_text
0x50,                               // PUSH EAX
0xE8,RELOC,                         // CALL $loadmap
0xC3,                               // RETN
};
reloc reloc_loadmap_cb[] = {
    { COH_REL, COHFUNC_DIALOG_GET_TEXT },
    { ICON_CODE_REL, CODE_LOADMAP },
    { RELOC_END, 0 }
};

// ===== pos_update_cb =====
// Calling convention: Custom
//      No stack changes (accesses parent function's stack)
//      Clobbers EAX, ECX, EDX, ESI
// Hook called from the edit toolbar's "something changed" code path.
unsigned char code_pos_update_cb[] = {
0x8D,0x74,0x24,0x20,            // LEA ESI, [ESP+20]
0xE8,RELOC,                     // CALL $get_target
// See if this is an absoulte or a relative change.
0x80,0x3D,RELOC,0x00,           // CMP BYTE PTR [$edit_transform_abs], 0
0x74, 0x12,                     // JE reltrans
0x50,                           // PUSH EAX
// Absolute transform, so push the entity and vector to face.
0x56,                           // PUSH ESI
0x50,                           // PUSH EAX
0xE8,RELOC,                     // CALL $set_ent_facing
// Use the XYZ part as new coordinates for the entity.
0x59,                           // POP ECX
0x8D,0x56,0x18,                 // LEA EDX, [ESI+18]
0xE8,RELOC,                     // CALL $ent_teleport
0xC3,                           // RETN

// reltrans:
// Relative, so we'll need some stack space for a temporary vector.
0x83,0xEC,0x10,                 // SUB ESP, 10
0x89,0x44,0xE4,0x0C,            // MOV DWORD PTR [ESP+0C], EAX
// First convert the player's matrix into a PYR trio. This suffers from some
// bad gimbal lock, but is the best we can do with the interface available.
0x8D,0x48,0x38,                 // LEA ECX, [EAX+38]
0x89,0xE2,                      // MOV EDX, ESP
0xE8,RELOC,                     // CALL $matrix_to_pyr
// Now add in the relative values entered.
0xD9,0x04,0xE4,                 // FLD DWORD PTR [ESP]
0xD8,0x06,                      // FADD DWORD PTR [ESI]
0xD9,0x1C,0xE4,                 // FSTP DWORD PTR [ESP]
0xD9,0x44,0xE4,0x04,            // FLD DWORD PTR [ESP+4]
0xD8,0x46,0x04,                 // FADD DWORD PTR [ESI+4]
0xD9,0x5C,0xE4,0x04,            // FSTP DWORD PTR [ESP+4]
0xD9,0x44,0xE4,0x08,            // FLD DWORD PTR [ESP+8]
0xD8,0x46,0x08,                 // FADD DWORD PTR [ESI+8]
0xD9,0x5C,0xE4,0x08,            // FSTP DWORD PTR [ESP+8]
// Make the entity face the resulting vector.
0x8B,0x44,0xE4,0x0C,            // MOV EAX, DWORD PTR [ESP+0C]
0x54,                           // PUSH ESP
0x50,                           // PUSH EAX
0xE8,RELOC,                     // CALL $set_ent_facing
// Now add the player's position with the entered coordinates, storing the
// result in our temporary space.
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
// Send the entity to the resulting coordinates.
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

#define CODE(id, c) { CODE_##id, 0, sizeof(code_##c), code_##c, reloc_##c }
codedef icon_code[] = {
    CODE(ENTER_GAME, enter_game),
    CODE(SETUP_BINDS, setup_binds),
    CODE(CMD_HANDLER, cmd_handler),
    CODE(GENERIC_MOV, generic_mov),
    CODE(GET_TARGET, get_target),
    CODE(LOADMAP, loadmap),
    CODE(FIND_SPAWNS, find_spawns),
    CODE(MAP_TRAVERSER, map_traverser),
    CODE(GOTO_SPAWN, goto_spawn),
    CODE(ENT_SET_FACING, ent_set_facing),
    CODE(SETUP_TUTORIAL, setup_tutorial),
    CODE(CREATE_ENT, create_ent),
    CODE(DELETE_ENT, delete_ent),
    CODE(MOVE_ENT_TO_PLAYER, move_ent_to_player),

    CODE(CMD_FLY, cmd_fly),
    CODE(CMD_TORCH, cmd_torch),
    CODE(CMD_NOCOLL, cmd_nocoll),
    CODE(CMD_SEEALL, cmd_seeall),
    CODE(CMD_COORDS, cmd_coords),
    CODE(CMD_DETACH, cmd_detach),
    CODE(CMD_LOADMAP, cmd_loadmap),
    CODE(CMD_LOADMAP_PROMPT, cmd_loadmap_prompt),
    CODE(CMD_MOV, cmd_mov),
    CODE(CMD_PREVSPAWN, cmd_prevspawn),
    CODE(CMD_NEXTSPAWN, cmd_nextspawn),
    CODE(CMD_RANDOMSPAWN, cmd_randomspawn),
    CODE(CMD_SPAWNNPC, cmd_spawnnpc),
    CODE(CMD_MOVENPC, cmd_movenpc),
    CODE(CMD_DELETENPC, cmd_deletenpc),

    CODE(LOADMAP_CB, loadmap_cb),
    CODE(POS_UPDATE_CB, pos_update_cb),
    { 0, 0, 0, 0 }
};
#undef CODE

static void InitCode() {
    DWORD o = 0;
    int i;
    DWORD *temp;

    iconCodeBase = (DWORD)VirtualAllocEx(pinfo.hProcess, NULL, ICON_CODE_SIZE,
            MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READ);
    if (!iconCodeBase)
        WBailout("Failed to allocate memory");
    
    temp = malloc(ICON_CODE_SIZE);
    for (i = 0; i < ICON_CODE_SIZE / 4; i++) {
        temp[i] = 0xcccccccc;
    }
    PutData(iconCodeBase, temp, ICON_CODE_SIZE);
    free(temp);

    codedef_cache = calloc(1, sizeof(codedef*) * CODE_END);
    codedef *cd = icon_code;
    while (cd && cd->sz) {
        cd->offset = o;
        codedef_cache[cd->id] = cd;
        o += cd->sz;
        // keep 4-byte alignment of functions
        if (o % 4)
            o += 4 - (o % 4);
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

// Version-specific workarounds
void FixupCode(int vers) {
    if (vers == 23) {
        code_cmd_detach[0] = 0xC3;      // Missing camera support in I23
    }
}
