enum {
    CODE_ENTER_GAME,
    CODE_GENERIC_MOV,
    CODE_GET_TARGET,
    CODE_KEY_HOOK,
    CODE_KEY_FLY,
    CODE_KEY_TORCH,
    CODE_KEY_NOCOLL,
    CODE_KEY_SEEALL,
    CODE_KEY_DETACH,
    CODE_KEY_LOADMAP,
    CODE_LOADMAP_CB,
    CODE_KEY_COORDS,
    CODE_KEY_MOV,
    CODE_MOV_CB,
    CODE_KEY_FX,
    CODE_FX_CB,
    CODE_KEY_KILLFX,
    CODE_KEY_SPAWN,
    CODE_SPAWNCB,
    CODE_KEY_KILLENT,
    CODE_KEY_TARGETMODE,
    CODE_FRAME_HOOK,
};

enum {
    RELOC_ICON_STR,
    RELOC_ICON_DATA,
    RELOC_ICON_CODE_ABS,
    RELOC_ICON_CODE_REL,
    RELOC_COH_ABS,
    RELOC_COH_REL,
};

typedef struct {
    unsigned long type;
    unsigned long id;
} reloc;

typedef struct {
    unsigned long offset;
    unsigned long len;
    unsigned char *code;
    reloc *relocs;
} codedef;

extern DWORD iconCodeBase;
extern codedef icon_code[];

#define CODE_ADDR(id) (iconCodeBase + icon_code[id].offset)

void CalcCodeOffsets();
void RelocateCode();
void WriteCode();
