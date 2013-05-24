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

static datamap icon_data[] = {
    { DATA_ZONE_MAP, 6*sizeof(DWORD), 0 },
    { DATA_SPAWNCOORDS, sizeof(spawncoords), spawncoords },
    { DATA_KBHOOKS, 0x80*sizeof(DWORD), 0 },
    { 0, 0, 0 }
};

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
}

unsigned long DataAddr(int id) {
    if (!dataoffset_cache)
        InitData();

    return iconDataBase + dataoffset_cache[id];
}

void WriteData() {
    unsigned long *hooks;
    DWORD zoneMap[6];
    int l;

    // Do generic initializers
    datamap *dm = icon_data;
    while (dm && dm->sz) {
        if (dm->init)
            PutData(DataAddr(dm->id), dm->init, dm->sz);
        ++dm;
    }

    // Build zone map
    for (l = 0; l < 6; l++) {
	zoneMap[l] = StringAddr(STR_MAP_OUTBREAK + l);
    }
    PutData(DataAddr(DATA_ZONE_MAP), (char*)zoneMap, sizeof(zoneMap));

    l = 0x80 * sizeof(DWORD);
    hooks = calloc(1, l);
    // Set up hooks for keycodes
    hooks[2] = CodeAddr(CODE_KEY_FLY);
    hooks[3] = CodeAddr(CODE_KEY_TORCH);
    hooks[4] = CodeAddr(CODE_KEY_NOCOLL);
    hooks[5] = CodeAddr(CODE_KEY_SEEALL);
    hooks[0x3B] = CodeAddr(CODE_KEY_LOADMAP);    // F1
    hooks[0x3C] = CodeAddr(CODE_KEY_DETACH);     // F2
    PutData(DataAddr(DATA_KBHOOKS), (char*)hooks, l);
    free(hooks);
}
