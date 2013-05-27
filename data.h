/* vim: set sts=4 sw=4 et: */

/* Titan Icon
 * Copyright (C) 2013 Titan Network
 * All Rights Reserved
 *
 * This code is for educational purposes only and is not licensed for
 * redistribution in source form.
 */

enum {
    DATA_ZONE_MAP = 1,
    DATA_SPAWNCOORDS,
    DATA_BIND_LIST,
    DATA_BINDS,
    DATA_COMMANDS,
    DATA_COMMAND_LIST,
    DATA_COMMAND_FUNCS,
    DATA_PARAM1,
    DATA_COYOTE_POS,
    DATA_COYOTE_ROT,
    DATA_SHOW_TOOLBAR,
    DATA_NUM_ENTS,
    DATA_END
};

unsigned long DataAddr(int id);
void WriteData();
