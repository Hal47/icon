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

static DWORD iconDataBase = 0;
static DWORD *dataoffset_cache = 0;

typedef struct {
    int id;
    int sz;
    void *init;
} datamap;

static float spawncoords[] =
{
    -63.7, 0, 157.7,		// Outbreak
    137.5, 0.3, -112.0,		// Atlas
    2696.3, 16.0, 8071.1,	// Nerva
    762.2, 148.0, -931.5,	// Pocket D
    -4665.8, 40.5, -253.0,	// Nova
    679.5, 109.0, 3202.5,	// Imperial
};

enum {
    ARG_INT = 1,
    ARG_UNSIGNED,
    ARG_FLOAT,
    ARG_UNK1,
    ARG_VECTOR,
    ARG_MATRIX,
    ARG_STRING,
    ARG_LINE,
    ARG_UNK2
};

typedef struct {
    int type;
    DWORD out;
    int maximum;
} command_arg;

typedef struct {
    int     access;
    DWORD   name;
    int     id;
    command_arg args[11];
    int     flags;
    DWORD   help;
    DWORD   callback;
    int     send;
    int     nargs;
    int     refcount;
    int     unknown;
} command;

#define OUT_COH 0x80000000
#define OUT_COH_MASK 0x7FFFFFFF
command icon_commands[] = {
    { 0, STR_CMD_FLY, CODE_CMD_FLY, {{ 0 }}, 1, STR_CMD_FLY_HELP },
    { 0, STR_CMD_TORCH, CODE_CMD_TORCH, {{ 0 }}, 1, STR_CMD_TORCH_HELP },
    { 0, STR_CMD_LOADMAP, CODE_CMD_LOADMAP, {{ ARG_LINE, DATA_PARAM1, 255 }}, 1, STR_CMD_LOADMAP_HELP },
    { 0, STR_CMD_LOADMAP_PROMPT, CODE_CMD_LOADMAP_PROMPT, {{ 0 }}, 9 },
    { 0, STR_CMD_DETACH, CODE_CMD_DETACH, {{ 0 }}, 1, STR_CMD_DETACH_HELP },
    { 0, STR_CMD_COORDS, CODE_CMD_COORDS, {{ 0 }}, 1, STR_CMD_COORDS_HELP },
    { 0, STR_CMD_MAPDEV, CODE_CMD_SEEALL, {{ 0 }}, 1, STR_CMD_MAPDEV_HELP },
    { 0, STR_CMD_NOCLIP, CODE_CMD_NOCOLL, {{ 0 }}, 1, STR_CMD_NOCLIP_HELP },
    { 0, STR_CMD_MOV, CODE_CMD_MOV, {{ ARG_STRING, DATA_PARAM1, 255 }}, 1, STR_CMD_MOV_HELP },
    { 0, STR_CMD_TIME, CODE_END + 1, {{ ARG_FLOAT, OUT_COH | COHVAR_GAME_TIME }}, 0, STR_CMD_TIME_HELP },
    { 0, STR_CMD_PREVSPAWN, CODE_CMD_PREVSPAWN, {{ 0 }}, 1, STR_CMD_PREVSPAWN_HELP },
    { 0, STR_CMD_NEXTSPAWN, CODE_CMD_NEXTSPAWN, {{ 0 }}, 1, STR_CMD_NEXTSPAWN_HELP },
    { 0, STR_CMD_RANDOMSPAWN, CODE_CMD_RANDOMSPAWN, {{ 0 }}, 1, STR_CMD_RANDOMSPAWN_HELP },
    { 0, STR_CMD_SPAWNNPC, CODE_CMD_SPAWNNPC, {{ ARG_STRING, DATA_PARAM1, 255 }, { ARG_LINE, DATA_PARAM2, 128 }}, 1, STR_CMD_SPAWNNPC_HELP },
    { 0, STR_CMD_MOVENPC, CODE_CMD_MOVENPC, {{ 0 }}, 1, STR_CMD_MOVENPC_HELP },
    { 0, STR_CMD_DELETENPC, CODE_CMD_DELETENPC, {{ 0 }}, 1, STR_CMD_DELETENPC_HELP },
    { 0, STR_CMD_CLEARNPC, CODE_CMD_CLEARNPC, {{ 0 }}, 1, STR_CMD_CLEARNPC_HELP },
    { 0, STR_CMD_LOADCOSTUME, CODE_CMD_LOADCOSTUME, {{ ARG_LINE, DATA_PARAM1, 255 }}, 1, STR_CMD_LOADCOSTUME_HELP },
    { 0, STR_CMD_BENPC, CODE_CMD_BENPC, {{ ARG_STRING, DATA_PARAM1, 255 }}, 1, STR_CMD_BENPC_HELP },
    { 0, STR_CMD_RENAME, CODE_CMD_RENAME, {{ ARG_LINE, DATA_PARAM1, 128 }}, 1, STR_CMD_RENAME_HELP },
    { 0, STR_CMD_ACCESSLEVEL, CODE_CMD_ACCESSLEVEL, {{ ARG_UNSIGNED, DATA_INT_PARAM }}, 1, STR_CMD_ACCESSLEVEL_HELP },
    { 0 }
};

typedef struct {
    DWORD   commands;
    DWORD   callback;
    char    unk1[32];
} command_group;

typedef struct {
    command_group groups[32];
    DWORD   callback;
    void *hash;
} command_list;

command_list icon_command_list = {
    {{ DATA_COMMANDS }, { 0 }}
};

typedef struct {
    const char *key;
    DWORD   command;
} bind_ent;

bind_ent icon_bind_list[] = {
    { "1", STR_CMD_FLY },
    { "2", STR_CMD_TORCH },
    { "f1", STR_CMD_LOADMAP_PROMPT },
    { "f2", STR_CMD_DETACH },
    { "f3", STR_CMD_COORDS },
    { "f4", STR_CMD_MAPDEV },
    { "f5", STR_CMD_NOCLIP },
    { "[", STR_CMD_PREVSPAWN },
    { "]", STR_CMD_NEXTSPAWN },
    { "f12", STR_CMD_RANDOMSPAWN },
    { "g", STR_CMD_MOVENPC },
    { "shift+delete", STR_CMD_DELETENPC },
    { 0, 0 },
};

int num_ents = 2;

static datamap icon_data[] = {
    { DATA_ZONE_MAP, 6*sizeof(DWORD), 0 },
    { DATA_SPAWNCOORDS, sizeof(spawncoords), spawncoords },
    { DATA_BIND_INIT, sizeof(int), 0 },
    { DATA_BIND_LIST, sizeof(icon_bind_list), icon_bind_list },
    { DATA_COMMANDS, sizeof(icon_commands), icon_commands },
    { DATA_COMMAND_LIST, sizeof(icon_command_list), &icon_command_list },
    { DATA_COMMAND_FUNCS, CODE_END*sizeof(DWORD), 0 },
    { DATA_PARAM1, 255, 0 },
    { DATA_PARAM2, 255, 0 },
    { DATA_INT_PARAM, sizeof(int), 0 },
    { DATA_SHOW_TOOLBAR, sizeof(int), 0 },
    { DATA_NUM_ENTS, sizeof(num_ents), &num_ents },
    { DATA_SPAWN_LIST, sizeof(DWORD), 0 },
    { DATA_LAST_SPAWN, sizeof(DWORD), 0 },
    { DATA_SPAWN_HASH, sizeof(DWORD), 0 },
    { DATA_NPC_HASH, sizeof(DWORD), 0 },
    { 0, 0, 0 }
};

static void FixupCommands() {
    command *c = icon_commands;
    bind_ent *b = icon_bind_list;
    int i;

    while (c && c->name) {
        c->name = StringAddr(c->name);
        if (c->help)
            c->help = StringAddr(c->help);
        if (c->callback)
            c->callback = CodeAddr(c->callback);
        for (i = 0; i < 11; i++) {
            if (c->args[i].out > 0) {
                if (c->args[i].out & OUT_COH)
                    c->args[i].out = CohAddr(c->args[i].out & OUT_COH_MASK);
                else
                    c->args[i].out = DataAddr(c->args[i].out);
            }
        }
        ++c;
    }

    for (i = 0; i < 32; i++) {
        if (icon_command_list.groups[i].commands > 0)
            icon_command_list.groups[i].commands = DataAddr(icon_command_list.groups[i].commands);
    }

    while (b && b->key) {
        b->key = (const char*)AddString(b->key);    // terrible abuse ;)
        b->command = StringAddr(b->command);
        ++b;
    }
}

static void InitData() {
    DWORD o = 0;

    iconDataBase = (DWORD)VirtualAllocEx(pinfo.hProcess, NULL, ICON_DATA_SIZE,
            MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!iconDataBase)
        WBailout("Failed to allocate memory");

    dataoffset_cache = calloc(1, sizeof(DWORD) * DATA_END);
    datamap *dm = icon_data;
    while (dm && dm->sz) {
        dataoffset_cache[dm->id] = o;
        o += dm->sz;
        // keep 4-byte alignment of data
        if (o % 4)
            o += 4 - (o % 4);
        ++dm;
    }

    if (o > ICON_DATA_SIZE)
	Bailout("Data section overflow");

    FixupCommands();
}

unsigned long DataAddr(int id) {
    if (!dataoffset_cache)
        InitData();

    return iconDataBase + dataoffset_cache[id];
}

void WriteData() {
    unsigned long *cmdmap;
    DWORD zoneMap[6];
    int i, l;

    // Do generic initializers
    datamap *dm = icon_data;
    while (dm && dm->sz) {
        if (dm->init)
            PutData(DataAddr(dm->id), dm->init, dm->sz);
        ++dm;
    }

    // Build zone map
    for (i = 0; i < 6; i++) {
	zoneMap[i] = StringAddr(STR_MAP_OUTBREAK + i);
    }
    PutData(DataAddr(DATA_ZONE_MAP), (char*)zoneMap, sizeof(zoneMap));

    // Do generic command mapping
    l = CODE_END * sizeof(DWORD);
    cmdmap = calloc(1, l);
    for (i = 1; i < CODE_END; i++) {
        cmdmap[i] = CodeAddr(i);
    }
    PutData(DataAddr(DATA_COMMAND_FUNCS), (char*)cmdmap, l);
    free(cmdmap);
}
