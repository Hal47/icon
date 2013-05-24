int Bailout(char *error);
int WBailout(char *error);
unsigned int GetInt(unsigned int addr);
void PutData(unsigned int addr, void *data, int len);
void PutInt(unsigned int addr, unsigned int val);
void bmagic(unsigned int addr, int oldval, int newval);
int CalcRelAddr(unsigned int addr, unsigned int dest);
void PutRelAddr(unsigned int addr, unsigned int dest);
void PutCall(unsigned int addr, unsigned int dest);
