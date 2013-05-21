CC=mingw32-gcc
CFLAGS=-Os -Wall
WINDRES=mingw32-windres

icon.exe: icon.o icon.rc.o
	$(CC) $(CFLAGS) -mwindows icon.o icon.rc.o -lcomdlg32 -o icon.exe
	mingw32-strip icon.exe

icon.o: icon.c
	$(CC) $(CFLAGS) -c icon.c -o icon.o

icon.rc.o: icon.rc
	$(WINDRES) -o icon.rc.o icon.rc

clean::
	rm -f icon.exe icon.o icon.rc.o
