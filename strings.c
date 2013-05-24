/* vim: set sts=4 sw=4 et: */

#define WINVER 0x0501
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "icon.h"
#include "util.h"
#include "strings.h"

static DWORD iconStrBase = 0;
static DWORD *stringoffset_cache = 0;

typedef struct {
    int id;
    const char *str;
    int sz;
} stringmap;

#define STR(id, s) { STR_##id, s, sizeof(s) }
static stringmap icon_strs[] = {
    STR(NOCLIP_ON, "Noclip Enabled"),
    STR(NOCLIP_OFF, "Noclip Disabled"),
    STR(MAPFILE, "Enter a map file name:"),
    STR(MOV, "Enter a MOV name:"),
    STR(FX, "Enter an FX name:"),
    STR(CAMERADETACH, "Camera Detached"),
    STR(CAMERAATTACH, "Camera Reattached"),
    STR(HOLDTORCH, "EMOTE_HOLD_TORCH"),
    STR(MAP_OUTBREAK, "maps/City_Zones/City_00_01/City_00_01.txt"),
    STR(MAP_ATLAS, "maps/City_Zones/City_01_01/City_01_01.txt_33"),
    STR(MAP_NERVA, "maps/City_Zones/V_City_03_02/V_City_03_02.txt"),
    STR(MAP_POCKETD, "maps/City_Zones/City_02_04/City_02_04.txt"),
    STR(MAP_NOVA, "maps/City_Zones/P_City_00_01/P_City_00_01.txt"),
    STR(MAP_IMPERIAL, "maps/City_Zones/P_City_00_02/P_City_00_02.txt"),
    { 0, 0, 0 },
};

static void InitStrings() {
    DWORD o = 0;

    iconStrBase = (DWORD)VirtualAllocEx(pinfo.hProcess, NULL, ICON_STR_SIZE,
            MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!iconStrBase)
        WBailout("Failed to allocate memory");

    stringoffset_cache = calloc(1, sizeof(DWORD) * STR_END);
    stringmap *sm = icon_strs;
    while (sm && sm->str) {
        stringoffset_cache[sm->id] = o;
        o += sm->sz;
        ++sm;
    }

    if (o > ICON_STR_SIZE)
        Bailout("String section overflow");
}

unsigned long StringAddr(int id) {
    if (!stringoffset_cache)
        InitStrings();
    return iconStrBase + stringoffset_cache[id];
}

void WriteStrings() {
    stringmap *sm = icon_strs;

    while (sm && sm->str) {
	PutData(StringAddr(sm->id), sm->str, sm->sz);
	++sm;
    }
}
