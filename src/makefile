release64 = 1

# comment to enable cmd attached window
win = -mwindows

#warn = -ansi -pedantic -Wall -Wshadow -Wpointer-arith -Wbad-function-cast -Wcast-qual -Wcast-align -Wwrite-strings -Wmissing-prototypes -Wnested-externs -Wno-long-long
warn = -Wall

ifdef release64
	arch   = -std=c17 -march=x86-64
	opt    = -O3 -fomit-frame-pointer -ftracer
	libdir = C:\msys64\ucrt64\lib
	libinc = C:\msys64\ucrt64\include
else
	arch   = -std=c17 -march=i686 -mfpmath=387
	opt    = -O3 -fomit-frame-pointer -ftracer
	libdir = C:\msys64\mingw32\lib
	libinc = C:\msys64\mingw32\include
endif

compile = gcc -c $(warn) $(arch) $(opt) -I$(libinc)

link = gcc $(win) -o

# requires zlib static lib, MS DirectX SDK headers
lib = -s -L$(libdir) -static -mwindows -lgdi32 -lkernel32 -lole32 -lcomdlg32 -lcomctl32 -lshell32 -lwinmm -lwininet -luser32 -ld3d9 -lddraw -ldsound -ldinput -ldxguid -luuid -lz

rm = @rm -f -v
md = @mkdir

obj = ..\obj

exe = ..\bin

globals = global.h log.h

objects = $(obj)\am29f040b.o $(obj)\cont.o $(obj)\crystal.o $(obj)\draw.o $(obj)\file.o $(obj)\help.o $(obj)\input.o $(obj)\io.o $(obj)\log.o $(obj)\main.o $(obj)\mapper.o $(obj)\media.o $(obj)\movie.o $(obj)\msx.o $(obj)\netplay.o $(obj)\paste.o $(obj)\psg.o $(obj)\psglogger.o $(obj)\psgtoy.o $(obj)\reverse.o $(obj)\sample.o $(obj)\scc.o $(obj)\screenshot.o $(obj)\settings.o $(obj)\sound.o $(obj)\spriteview.o $(obj)\state.o $(obj)\tape.o $(obj)\tileview.o $(obj)\tool.o $(obj)\update.o $(obj)\version.o $(obj)\vdp.o $(obj)\z80.o $(obj)\resource.o $(obj)\ti_ntsc.o $(obj)\ioapi.o $(obj)\unzip.o

all : makedirs $(exe)\meisei.exe

makedirs : $(obj) $(exe)
$(obj) :
	$(md) $(obj)
$(exe) :
	$(md) $(exe)

$(exe)\meisei.exe : $(objects) makefile
	$(link) $(exe)\meisei.exe $(objects) $(lib)

$(obj)\am29f040b.o : am29f040b.c am29f040b.h
	$(compile) am29f040b.c -o $(obj)\am29f040b.o

$(obj)\cont.o : cont.c cont.h
	$(compile) cont.c -o $(obj)\cont.o

$(obj)\crystal.o : crystal.c crystal.h
	$(compile) crystal.c -o $(obj)\crystal.o

$(obj)\draw.o : draw.c draw.h
	$(compile) draw.c -o $(obj)\draw.o

$(obj)\file.o : file.c file.h
	$(compile) file.c -o $(obj)\file.o

$(obj)\help.o : help.c help.h
	$(compile) help.c -o $(obj)\help.o

$(obj)\input.o : input.c input.h
	$(compile) input.c -o $(obj)\input.o

$(obj)\io.o : io.c io.h
	$(compile) io.c -o $(obj)\io.o

$(obj)\log.o : log.c log.h
	$(compile) log.c -o $(obj)\log.o

$(obj)\main.o : main.c main.h
	$(compile) main.c -o $(obj)\main.o

$(obj)\mapper.o : mapper.c mapper.h mapper_table.h
	$(compile) mapper.c -o $(obj)\mapper.o

$(obj)\media.o : media.c media.h
	$(compile) media.c -o $(obj)\media.o

$(obj)\movie.o : movie.c movie.h
	$(compile) movie.c -o $(obj)\movie.o

$(obj)\msx.o : msx.c msx.h
	$(compile) msx.c -o $(obj)\msx.o

$(obj)\netplay.o : netplay.c netplay.h
	$(compile) netplay.c -o $(obj)\netplay.o

$(obj)\paste.o : paste.c paste.h paste_table.h
	$(compile) paste.c -o $(obj)\paste.o

$(obj)\psg.o : psg.c psg.h
	$(compile) psg.c -o $(obj)\psg.o

$(obj)\psglogger.o : psglogger.c psglogger.h
	$(compile) psglogger.c -o $(obj)\psglogger.o

$(obj)\psgtoy.o : psgtoy.c psgtoy.h
	$(compile) psgtoy.c -o $(obj)\psgtoy.o

$(obj)\reverse.o : reverse.c reverse.h
	$(compile) reverse.c -o $(obj)\reverse.o

$(obj)\sample.o : sample.c sample.h
	$(compile) sample.c -o $(obj)\sample.o

$(obj)\scc.o : scc.c scc.h
	$(compile) scc.c -o $(obj)\scc.o

$(obj)\screenshot.o : screenshot.c screenshot.h
	$(compile) screenshot.c -o $(obj)\screenshot.o

$(obj)\settings.o : settings.c settings.h
	$(compile) settings.c -o $(obj)\settings.o

$(obj)\sound.o : sound.c sound.h
	$(compile) sound.c -o $(obj)\sound.o

$(obj)\spriteview.o : spriteview.c spriteview.h
	$(compile) spriteview.c -o $(obj)\spriteview.o

$(obj)\state.o : state.c state.h
	$(compile) state.c -o $(obj)\state.o

$(obj)\tape.o : tape.c tape.h
	$(compile) tape.c -o $(obj)\tape.o

$(obj)\tileview.o : tileview.c tileview.h
	$(compile) tileview.c -o $(obj)\tileview.o

$(obj)\tool.o : tool.c tool.h
	$(compile) tool.c -o $(obj)\tool.o

$(obj)\update.o : update.c update.h
	$(compile) update.c -o $(obj)\update.o

$(obj)\version.o : version.c version.h
	$(compile) version.c -o $(obj)\version.o

$(obj)\vdp.o : vdp.c vdp.h
	$(compile) vdp.c -o $(obj)\vdp.o

$(obj)\z80.o : z80.c z80.h
	$(compile) z80.c -o $(obj)\z80.o

$(obj)\resource.o : resource_eng.rc resource.h version.h resources\icon.ico resources\icon_blocky.ico resources\icon_doc.ico resources\meisei.manifest resources\p_315-5124.pal resources\p_v9938.pal resources\p_tms9129.pal resources\p_konami.pal resources\p_vampier.pal resources\p_wolf.pal resources\pause.raw resources\bm_tick_vertical.bmp resources\bm_tick_vertical_wide.bmp resources\bm_tick_horizontal.bmp resources\bm_tick_horizontal_wide.bmp resources\psg_presets.bin
	windres -Iresources resource_eng.rc -o $(obj)\resource.o

$(obj)\ti_ntsc.o : ti_ntsc.c ti_ntsc.h ti_ntsc_config.h
	$(compile) ti_ntsc.c -o $(obj)\ti_ntsc.o

$(obj)\ioapi.o : ioapi.c ioapi.h
	$(compile) ioapi.c -o $(obj)\ioapi.o

$(obj)\unzip.o : unzip.c unzip.h
	$(compile) unzip.c -o $(obj)\unzip.o

clean :
	$(rm) $(obj)\am29f040b.o
	$(rm) $(obj)\cont.o
	$(rm) $(obj)\crystal.o
	$(rm) $(obj)\draw.o
	$(rm) $(obj)\file.o
	$(rm) $(obj)\help.o
	$(rm) $(obj)\input.o
	$(rm) $(obj)\io.o
	$(rm) $(obj)\log.o
	$(rm) $(obj)\main.o
	$(rm) $(obj)\mapper.o
	$(rm) $(obj)\media.o
	$(rm) $(obj)\movie.o
	$(rm) $(obj)\msx.o
	$(rm) $(obj)\netplay.o
	$(rm) $(obj)\paste.o
	$(rm) $(obj)\psg.o
	$(rm) $(obj)\psglogger.o
	$(rm) $(obj)\psgtoy.o
	$(rm) $(obj)\reverse.o
	$(rm) $(obj)\sample.o
	$(rm) $(obj)\scc.o
	$(rm) $(obj)\screenshot.o
	$(rm) $(obj)\settings.o
	$(rm) $(obj)\sound.o
	$(rm) $(obj)\spriteview.o
	$(rm) $(obj)\state.o
	$(rm) $(obj)\tape.o
	$(rm) $(obj)\tileview.o
	$(rm) $(obj)\tool.o
	$(rm) $(obj)\update.o
	$(rm) $(obj)\version.o
	$(rm) $(obj)\vdp.o
	$(rm) $(obj)\z80.o
	$(rm) $(obj)\resource.o
	$(rm) $(obj)\ti_ntsc.o
	$(rm) $(obj)\ioapi.o
	$(rm) $(obj)\unzip.o
	$(rm) $(exe)\meisei.exe
