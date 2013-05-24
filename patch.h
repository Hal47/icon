enum {
    COHVAR_PLAYER_ENT = 1,
    COHFUNC_DIALOG,
    COH_END
};

void InitCoh(int vers);
unsigned long CohAddr(int id);

void PatchI24();
void PatchI23();
