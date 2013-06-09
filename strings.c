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
#include "strings.h"

static DWORD iconStrBase = 0;
static DWORD strDynamic = 0;
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
    STR(DEFAULT_CLASS, "Class_Scrapper"),
    STR(DEFAULT_ORIGIN, "Natural"),
    STR(SPAWNLOCATION, "SpawnLocation"),
    STR(PERSISTENTNPC, "PersistentNPC"),
    STR(DOOR, "Door"),
    STR(GENERATOR, "Generator"),
    STR(DUMMY_FILENAME, "icon.c"),
    STR(CMD_MAPDEV, "map_dev"),
    STR(CMD_MAPDEV_HELP, "Toggles display of hidden developer markers."),
    STR(CMD_NOCLIP, "no_clip"),
    STR(CMD_NOCLIP_HELP, "Toggles no-clipping mode."),
    STR(CMD_FLY, "fly"),
    STR(CMD_FLY_HELP, "Take to the skies!"),
    STR(CMD_TORCH, "rememberap33"),
    STR(CMD_TORCH_HELP, "Raise a torch in remembrance of all the heroes who keep hope alive."),
    STR(CMD_COORDS, "editpos"),
    STR(CMD_COORDS_HELP, "Open a window to view and edit the player position."),
    STR(CMD_DETACH, "detach_camera"),
    STR(CMD_DETACH_HELP, "Detaches the camera from the player and allows it to be moved freely."),
    STR(CMD_LOADMAP, "loadmap"),
    STR(CMD_LOADMAP_HELP, "Loads the specified map."),
    STR(CMD_LOADMAP_PROMPT, "loadmap_prompt"),
    STR(CMD_MOV, "mov"),
    STR(CMD_MOV_HELP, "Play an animation given its demorecord MOV name."),
    STR(CMD_TIME, "time"),
    STR(CMD_TIME_HELP, "Change the time of date (0-24)"),
    STR(CMD_PREVSPAWN, "prev_spawn"),
    STR(CMD_PREVSPAWN_HELP, "Jump to the previous spawn point."),
    STR(CMD_NEXTSPAWN, "next_spawn"),
    STR(CMD_NEXTSPAWN_HELP, "Jump to the next spawn point."),
    STR(CMD_RANDOMSPAWN, "random_spawn"),
    STR(CMD_RANDOMSPAWN_HELP, "Jump to a random spawn point."),
    STR(CMD_SPAWNNPC, "spawn_npc"),
    STR(CMD_SPAWNNPC_HELP, "Creates an NPC at your current location. The fist parameter is the costume, and the second is the name of the NPC."),
    STR(CMD_MOVENPC, "move_npc"),
    STR(CMD_MOVENPC_HELP, "Moves the targeted NPC to your current location."),
    STR(CMD_DELETENPC, "delete_npc"),
    STR(CMD_DELETENPC_HELP, "Deletes the targeted NPC. Why would you do that?"),
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
        // keep 4-byte alignment of strings
        if (o % 4)
            o += 4 - (o % 4);
        ++sm;
    }

    if (o > ICON_STR_SIZE)
        Bailout("String section overflow");
    strDynamic = o;
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

unsigned long AddString(const char *str) {
    unsigned long ret;
    int l;
    if (!stringoffset_cache)
        InitStrings();
  
    ret = iconStrBase + strDynamic;
    l = strlen(str) + 1;

    if (strDynamic + l > ICON_STR_SIZE)
        Bailout("String section overflow");
    PutData(ret, str, l);
    strDynamic += l;
    // keep 4-byte alignment of strings
    if (strDynamic % 4)
        strDynamic += 4 - (strDynamic % 4);

    return ret;
}
