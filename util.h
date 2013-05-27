/* vim: set sts=4 sw=4 et: */

/* Titan Icon
 * Copyright (C) 2013 Titan Network
 * All Rights Reserved
 *
 * This code is for educational purposes only and is not licensed for
 * redistribution in source form.
 */

int Bailout(char *error);
int WBailout(char *error);
unsigned int GetInt(unsigned int addr);
void PutData(unsigned int addr, const void *data, int len);
void PutInt(unsigned int addr, unsigned int val);
void bmagic(unsigned int addr, int oldval, int newval);
int CalcRelAddr(unsigned int addr, unsigned int dest);
void PutRelAddr(unsigned int addr, unsigned int dest);
void PutCall(unsigned int addr, unsigned int dest);
