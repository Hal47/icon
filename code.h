/* vim: set sts=4 sw=4 et: */

/* Titan Icon
 * Copyright (C) 2013 Titan Network
 * All Rights Reserved
 *
 * This code is for educational purposes only and is not licensed for
 * redistribution in source form.
 */

enum {
    CODE_ENTER_GAME = 1,
    CODE_ICON_INIT,
    CODE_SETUP_BINDS,
    CODE_CMD_HANDLER,
    CODE_CMD_HOOK,
    CODE_GENERIC_MOV,
    CODE_GET_TARGET,
    CODE_LOADMAP,
    CODE_SCAN_MAP,
    CODE_MAP_TRAVERSER,
    CODE_CHECK_NPC_SPAWN,
    CODE_CHECK_DOOR_SPAWN,
    CODE_GOTO_SPAWN,
    CODE_ENT_SET_FACING,
    CODE_ENT_FLIP,
    CODE_CREATE_ENT,
    CODE_DELETE_ENT,
    CODE_ENT_NPC_COSTUME,
    CODE_MOVE_ENT_TO_PLAYER,

    CODE_CMD_FLY,
    CODE_CMD_TORCH,
    CODE_CMD_NOCOLL,
    CODE_CMD_SEEALL,
    CODE_CMD_DETACH,
    CODE_CMD_LOADMAP,
    CODE_CMD_LOADMAP_PROMPT,
    CODE_CMD_COORDS,
    CODE_CMD_MOV,
    CODE_CMD_PREVSPAWN,
    CODE_CMD_NEXTSPAWN,
    CODE_CMD_RANDOMSPAWN,
    CODE_CMD_SPAWNNPC,
    CODE_CMD_MOVENPC,
    CODE_CMD_DELETENPC,
    CODE_CMD_CLEARNPC,
    CODE_CMD_LOADCOSTUME,
    CODE_CMD_BENPC,
    CODE_CMD_RENAME,
    CODE_CMD_ACCESSLEVEL,

    CODE_LOADMAP_CB,
    CODE_POS_UPDATE_CB,
    CODE_END,
};

unsigned long CodeAddr(int id);
void RelocateCode();
void WriteCode();
void FixupCode(int vers);
