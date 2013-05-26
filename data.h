enum {
    DATA_ZONE_MAP = 1,
    DATA_SPAWNCOORDS,
    DATA_KBHOOKS,
    DATA_BIND_LIST,
    DATA_BINDS,
    DATA_COMMANDS,
    DATA_COMMAND_LIST,
    DATA_COMMAND_FUNCS,
    DATA_END
};

unsigned long DataAddr(int id);
void WriteData();
