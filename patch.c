/* vim: set sts=4 sw=4 et: */

#define WINVER 0x0501
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "icon.h"
#include "util.h"
#include "code.h"
#include "patch.h"

typedef struct {
    int id;
    DWORD addr;
} addrmap;

static DWORD addrmap_cache[COH_END];

static addrmap addrs_i23[] = {
    { 0, 0 }
};

static addrmap addrs_i24[] = {
    { COHVAR_PLAYER_ENT, 0x00CAF580 },
    { COHFUNC_DIALOG, 0x005B6E10 },
    { 0, 0 }
};

void InitCoh(int vers) {
    addrmap *am;

    ZeroMemory(addrmap_cache, sizeof(addrmap_cache));
    if (vers == 23)
        am = addrs_i23;
    else if (vers == 24)
        am = addrs_i24;
    else {
        Bailout("An impossible thing happened! Check that the laws of physics are still working.");
        return;
    }

    while (am && am->addr) {
        addrmap_cache[am->id] = am->addr;
        ++am;
    }
}

unsigned long CohAddr(int id) {
    return addrmap_cache[id];
}

void PatchI24() {
    // product published?
    bmagic(0x00830259, 0xc032cc33, 0x01b0cc33);

    // owns product?
    bmagic(0x0083147B, 0xff853a74, 0x5aeb01b0);

    // create character
    bmagic(0x0083B246, 0x0410ec81, 0xc90ed5e8);
    if (random) {
        bmagic(0x0083B24a, 0x84a10000, 0xe980e8ff);
        bmagic(0x0083B24e, 0x3300b1ba, 0x1e6affc9);
        bmagic(0x0083B252, 0x248489c4, 0xd88b59e8);
        bmagic(0x0083B256, 0x0000040c, 0x04c483ff);
    } else {
        bmagic(0x0083B24a, 0x84a10000, 0xbb80e8ff);
        bmagic(0x0083B24e, 0x3300b1ba, 0x05c7ffee);
        bmagic(0x0083B252, 0x248489c4, 0x0167c800);
        bmagic(0x0083B256, 0x0000040c, editnpc);
    }
    bmagic(0x0083B25a, 0xc6c83d80, 0xc35dec89);

    // costume unlock BS
    bmagic(0x00458273, 0x950fc084, 0x950f91eb);
    bmagic(0x00458206, 0xcccccccc, 0x75433e81);
    bmagic(0x0045820a, 0xcccccccc, 0x6e757473);
    bmagic(0x0045820e, 0x5553cccc, 0x555368eb);

    // don't show "hide store pieces" box
    bmagic(0x00719FE5, 2, 1);

    if (editnpc) {
	// don't skip origin menu
	bmagic(0x0077E255, 0x3d833574, 0x3d8335eb);

	// don't skip playstyle menu
	bmagic(0x0077ECFC, 0x35891274, 0x358912eb);

	// don't skip archetype menu
	bmagic(0x0076D222, 0x3d833074, 0x3d8330eb);

	// don't skip power selection
	bmagic(0x0078151F, 0x03da840f, 0x0003dbe9);
	bmagic(0x00781523, 0x74a10000, 0x74a19000);
    }

    // "Sandbox Mode" stuff below

    // NOP out comm check
    bmagic(0x00409332, 0x5E0C053B, 0x90909090);
    bmagic(0x00409336, 0xC01B0168, 0x90909090);

    // always return 1 for connected
    bmagic(0x0040DA1D, 3, 1);

    // ignore check for mapserver in main loop
    bmagic(0x00838249, 0x3d392c77, 0x3d392ceb);

    // nocoll command
    bmagic(0x00BD12A4, 1, 0);

    // hook keyboard
    bmagic(0x005C94E0, 0xE37F64A1, 0x000000E8);
    PutRelAddr(0x005C94E1, CodeAddr(CODE_KEY_HOOK));

    // turn on invert mouse
    bmagic(0x00B34E00, 0, 1);

    // Hook "enter game"
    bmagic(0x004CC608, 0xA1000003, 0xE8000003);
    PutRelAddr(0x004CC60C, CodeAddr(CODE_ENTER_GAME));
    bmagic(0x004CC610, 0xC01BD8F7, 0xC4A3C031);
    bmagic(0x004CC614, 0x83A6E083, 0xE9012DF3);
    bmagic(0x004CC618, 0x44895AC0, 0x00000390);

    // Modify editor toolbar to affect entity position
    bmagic(0x00440D3C, 0xD8C08310, 0x81C08310);
    bmagic(0x00440D80, 0x74C08500, 0xEBC08500);
    bmagic(0x00440DE0, 0xA1302444, 0xE8302444);
    PutRelAddr(0x00440DE4, CodeAddr(CODE_GET_TARGET));
    bmagic(0x00440DFC, 0x4440D921, 0x5C40D921);
    bmagic(0x00440E00, 0xD920488D, 0xD938488D);
    bmagic(0x00440E0C, 0x5CD94840, 0x5CD96040);
    bmagic(0x00440E14, 0x245CD94C, 0x245CD964);
    bmagic(0x00440E28, 0x8A302444, 0xB9302444);
    bmagic(0x00440E2C, 0xDD7EFF0D, 0x00DD7EFF);
    bmagic(0x00440E30, 0x2444D900, 0x2444D990);
    bmagic(0x00440E34, 0xD9C9842C, 0xD901B42C);
    bmagic(0x00440E38, 0x74282444, 0x88282444);
    bmagic(0x00440E3C, 0x2444D942, 0x2444D921);
    bmagic(0x00440E80, 0x8B6F75C0, 0x906F75C0);
    bmagic(0x00440E84, 0x61313015, 0x90909090);
    bmagic(0x00440E88, 0xA8153B01, 0x90909090);
    bmagic(0x00440E8C, 0x7500C2DC, 0x90909090);
    bmagic(0x00440E90, 0x2444D961, 0x2444D990);
    bmagic(0x00440FE0, 0xFF9C1BE8, 0x90909090);
    bmagic(0x00440FE4, 0xFF3D80FF, 0xFF3D8090);
    bmagic(0x00440FEC, 0x30A13E74, 0x85E83E74);
    bmagic(0x00440FF0, 0x83016131, 0x8311F046);
    PutRelAddr(0x00440FEF, CodeAddr(CODE_GET_TARGET));
    bmagic(0x00440FF4, 0x748D20C0, 0x748D38C0);
    bmagic(0x00440FFC, 0xD90042AB, 0xE80042AB);
    PutRelAddr(0x00441000, CodeAddr(CODE_GET_TARGET));
    bmagic(0x00441004, 0x6131300D, 0x548DC189);
    bmagic(0x00441008, 0x4459D901, 0x81E834E4);
    bmagic(0x0044100C, 0x3130158B, 0xEB000727);
    bmagic(0x00441010, 0x44D90161, 0x44D90114);

    // and here too
    bmagic(0x004406C7, 0x7E8B1174, 0x7E8B9090);
    bmagic(0x0044078C, 0xC1950F01, 0x9001B101);
    bmagic(0x0044079C, 0x07750001, 0x07EB0001);
    bmagic(0x00440878, 0x7E801F74, 0x7E809090);
    bmagic(0x00440894, 0x75000161, 0xEB000161);

    // Display editor toolbar in main loop
    bmagic(0x00838DC8, 0x0A301D39, 0xE9B01D39);
    bmagic(0x00838DD0, 0x62B405D9, 0x5404EC83);
    bmagic(0x00838DD4, 0x1DD900A6, 0xC07F07E8);
    bmagic(0x00838DD8, 0x0167ABDC, 0x08C483FF);
    bmagic(0x00838DDC, 0xFFFD4FE8, 0x909036EB);
    bmagic(0x00838DE0, 0x24448DFF, 0x24448D90);
}

void PatchI23() {
    // product published?
    bmagic(0x00832f09, 0xc032cc33, 0x01b0cc33);

    // owns product?
    bmagic(0x0083408B, 0xff853a74, 0x5aeb01b0);

    // create character
    bmagic(0x0083DDD6, 0x0410ec81, 0xc8d545e8);
    bmagic(0x0083DDDA, 0x84a10000, 0x9220e8ff);
    bmagic(0x0083DDDE, 0x3300b1ba, 0x05c7ffee);
    bmagic(0x0083DDE2, 0x248489c4, 0x016789a4);
    bmagic(0x0083DDE6, 0x0000040c, editnpc);
    bmagic(0x0083DDEA, 0x88683d80, 0xc35dec89);

    // costume unlock BS
    bmagic(0x00458183, 0x950fc084, 0x950f91eb);
    bmagic(0x00458116, 0xcccccccc, 0x75433e81);
    bmagic(0x0045811a, 0xcccccccc, 0x6e757473);
    bmagic(0x0045811e, 0x5553cccc, 0x555368eb);

    // don't show "hide store pieces" box
    bmagic(0x0071A095, 2, 1);

    if (editnpc) {
	// don't skip origin menu
	bmagic(0x00780A05, 0x3d833574, 0x3d8335eb);

	// don't skip playstyle menu
	bmagic(0x007814AD, 0x35891274, 0x358912eb);

	// don't skip archetype menu
	bmagic(0x0076F9F2, 0x3d833074, 0x3d8330eb);

	// don't skip power selection
	bmagic(0x00783DC0, 0x03da840f, 0x0003dbe9);
	bmagic(0x00783DC4, 0x8ca10000, 0x8ca19000);
    }

    // "Sandbox Mode" stuff below

    // NOP out comm check
    bmagic(0x00409332, 0x1FAC053B, 0x90909090);
    bmagic(0x00409336, 0xC01B0168, 0x90909090);

    // always return 1 for connected
    bmagic(0x0040D9CD, 3, 1);

    // ignore check for mapserver in main loop
    bmagic(0x0083ADF9, 0x3d392c77, 0x3d392ceb);

    // nocoll command
    bmagic(0x00BCEFBC, 1, 0);

    // hook keyboard
    bmagic(0x005C7BB0, 0xE35994A1, 0xF03D09E8);
    bmagic(0x005C7BB4, 0x8B555300, 0x8B5553FF);

    // turn on invert mouse
    bmagic(0x00B349F0, 0, 1);
}
