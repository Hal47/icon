enum {
    STR_NOCLIP_ON = 1,
    STR_NOCLIP_OFF,
    STR_MAPFILE,
    STR_MOV,
    STR_FX,
    STR_CAMERADETACH,
    STR_CAMERAATTACH,
    STR_HOLDTORCH,
    STR_MAP_OUTBREAK,
    STR_MAP_ATLAS,
    STR_MAP_NERVA,
    STR_MAP_POCKETD,
    STR_MAP_NOVA,
    STR_MAP_IMPERIAL,
    STR_END
};

unsigned long StringAddr(int id);
void WriteStrings();