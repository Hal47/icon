/* vim: set sts=4 sw=4 et: */

#define WINVER 0x0501
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "icon.h"
#include "util.h"
#include "strings.h"

DWORD iconStrBase = 0;
DWORD *iconStrOffsets = 0;

static char *icon_strs[] = {
    "Noclip Enabled",	            // 0
    "Noclip Disabled",	            // 1
    "Enter a map file name:",	    // 2
    "Enter a MOV name:",	    // 3
    "Enter an FX name:",	    // 4
    "Camera Detached",		    // 5
    "Camera Reattached",	    // 6
    "EMOTE_HOLD_TORCH",		    // 7
    "maps/City_Zones/City_00_01/City_00_01.txt",     // 8
    "maps/City_Zones/City_01_01/City_01_01.txt_33",  // 9
    "maps/City_Zones/V_City_03_02/V_City_03_02.txt", // 10
    "maps/City_Zones/City_02_04/City_02_04.txt",     // 11
    "maps/City_Zones/P_City_00_01/P_City_00_01.txt", // 12
    "maps/City_Zones/P_City_00_02/P_City_00_02.txt", // 13
    0
};

void WriteIconStrings() {
    char **s;
    int i, l;
    DWORD o;

    // Write string table
    i = 0; o = 0;
    s = icon_strs;
    while (*s && **s) {
	l = strlen(*s) + 1;

	if (o + l > ICON_STR_SIZE)
	    Bailout("String table overflow");

	iconStrOffsets = realloc(iconStrOffsets, sizeof(DWORD) * (i + 1));
	iconStrOffsets[i] = o;
	PutData(iconStrBase + o, *s, l);

	o += l;
	++i; ++s;
    }
}
