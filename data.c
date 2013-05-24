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

DWORD iconDataBase = 0;
DWORD *iconDataOffsets = 0;

static float spawncoords[] =
{
    -63.7, 0, 157.7,		// Outbreak
    137.5, 0.3, -112.0,		// Atlas
    2696.3, 16.0, 8071.1,	// Nerva
    762.2, 148.0, -931.5,	// Pocket D
    -4665.8, 40.5, -253.0,	// Nova
    679.5, 109.0, 3202.5,	// Imperial
};

void WriteIconData() {
    unsigned long *hooks;
    int i, l;
    DWORD o;
    DWORD zoneMap[6];

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
    PutData(iconDataBase + o, (char*)zoneMap, l);
    o += l;

    iconDataOffsets[DATA_SPAWNCOORDS] = o;
    l = sizeof(spawncoords);
    if (o + l > ICON_DATA_SIZE)
	Bailout("Data section overflow");
    PutData(iconDataBase + o, (char*)spawncoords, l);
    o += l;

    iconDataOffsets[DATA_KBHOOKS] = o;
    l = 0x80 * sizeof(DWORD);
    hooks = calloc(1, l);

    // Set up hooks for keycodes
    hooks[2] = iconCodeBase + icon_code[CODE_KEY_FLY].offset;
    hooks[3] = iconCodeBase + icon_code[CODE_KEY_TORCH].offset;
    hooks[4] = iconCodeBase + icon_code[CODE_KEY_NOCOLL].offset;
    hooks[5] = iconCodeBase + icon_code[CODE_KEY_SEEALL].offset;
    hooks[0x3B] = iconCodeBase + icon_code[CODE_KEY_LOADMAP].offset;    // F1
    hooks[0x3C] = iconCodeBase + icon_code[CODE_KEY_DETACH].offset;     // F2

    if (o + l > ICON_DATA_SIZE)
        Bailout("Data section overflow");
    PutData(iconDataBase + o, (char*)hooks, l);
    o += l;
    free(hooks);
}
