enum {
    DATA_ZONE_MAP = 1,
    DATA_SPAWNCOORDS,
    DATA_KBHOOKS,
    DATA_END
};

unsigned long DataAddr(int id);
void WriteData();
