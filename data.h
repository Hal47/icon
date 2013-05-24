enum {
    DATA_ZONE_MAP,
    DATA_SPAWNCOORDS,
    DATA_KBHOOKS,
};

extern DWORD iconDataBase;
extern DWORD *iconDataOffsets;

void WriteIconData();
