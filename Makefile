CC=mingw32-gcc
CFLAGS=-Os -Wall
WINDRES=mingw32-windres
OFILES=icon.o code.o data.o strings.o patch.o util.o icon.rc.o

icon.exe: $(OFILES)
	$(CC) $(CFLAGS) -mwindows $(OFILES) -lcomdlg32 -o icon.exe
	mingw32-strip icon.exe

icon.rc.o: icon.rc
	$(WINDRES) -o icon.rc.o icon.rc

code.o: code.c icon.h util.h code.h data.h strings.h patch.h
data.o: data.c icon.h util.h code.h data.h strings.h
patch.o: patch.c icon.h util.h code.h patch.h
strings.o: strings.c icon.h util.h strings.h
util.o: util.c icon.h

clean::
	rm -f icon.exe $(OFILES)
