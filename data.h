enum {
    DATA_ZONE_MAP,
    DATA_SPAWNCOORDS,
    DATA_KBHOOKS,
};

extern DWORD iconDataBase;
extern DWORD *iconDataOffsets;

#define DATA_ADDR(id) (iconDataBase + iconDataOffsets[id])

void WriteIconData();
