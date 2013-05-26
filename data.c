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
    ARG_INT,
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

command icon_commands[] = {
    { 0, STR_CMD_FLY, CODE_CMD_FLY, {{ 0 }}, 1, STR_CMD_FLY_HELP },
    { 0, STR_CMD_TORCH, CODE_CMD_TORCH, {{ 0 }}, 1, STR_CMD_TORCH_HELP },
    { 0, STR_CMD_LOADMAP, CODE_CMD_LOADMAP, {{ ARG_LINE, DATA_PARAM1, 255 }}, 1, STR_CMD_LOADMAP_HELP },
    { 0, STR_CMD_LOADMAP_PROMPT, CODE_CMD_LOADMAP_PROMPT, {{ 0 }}, 9 },
    { 0, STR_CMD_DETACH, CODE_CMD_DETACH, {{ 0 }}, 1, STR_CMD_DETACH_HELP },
    { 0, STR_CMD_COORDS, CODE_CMD_COORDS, {{ 0 }}, 1, STR_CMD_COORDS_HELP },
    { 0, STR_CMD_MAPDEV, CODE_CMD_SEEALL, {{ 0 }}, 1, STR_CMD_MAPDEV_HELP },
    { 0, STR_CMD_NOCLIP, CODE_CMD_NOCOLL, {{ 0 }}, 1, STR_CMD_NOCLIP_HELP },
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
    { 0, 0 },
};

static datamap icon_data[] = {
    { DATA_ZONE_MAP, 6*sizeof(DWORD), 0 },
    { DATA_SPAWNCOORDS, sizeof(spawncoords), spawncoords },
    { DATA_BIND_LIST, sizeof(icon_bind_list), icon_bind_list },
    { DATA_BINDS, sizeof(DWORD), 0 },
    { DATA_COMMANDS, sizeof(icon_commands), icon_commands },
    { DATA_COMMAND_LIST, sizeof(icon_command_list), &icon_command_list },
    { DATA_COMMAND_FUNCS, CODE_END*sizeof(DWORD), 0 },
    { DATA_PARAM1, 255, 0 },
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
            if (c->args[i].out > 0)
                c->args[i].out = DataAddr(c->args[i].out);
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
