CC=mingw32-gcc
CFLAGS=-Os
WINDRES=mingw32-windres

icon.exe: icon.o
	$(CC) $(CFLAGS) -mwindows icon.o -o icon.exe
	mingw32-strip icon.exe

#icon.exe: icon.o iconres.o
#	$(CC) $(CFLAGS) -mwindows icon.o iconres.o -o icon.exe

icon.o: icon.c
	$(CC) $(CFLAGS) -c icon.c -o icon.o

iconres.o: icon.rc
	$(WINDRES) icon.rc iconres.o

clean::
	rm -f icon.exe icon.o iconres.o
