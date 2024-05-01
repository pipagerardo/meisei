release = 1

# comment to enable cmd attached window
win = -mwindows

# uncomment to enable memleak debugger
#dmss = -DMSS
#lmss = -Lmss -lmss

#warn = -ansi -pedantic -Wall -Wshadow -Wpointer-arith -Wbad-function-cast -Wcast-qual -Wcast-align -Wwrite-strings -Wmissing-prototypes -Wnested-externs -Wno-long-long
warn = -ansi -pedantic -Wall

# change libdir to match your environment
libdir = C:\msys64\mingw64\lib
libinc = C:\msys64\mingw32\include

ifdef release
	arch = -march=i686 -mfpmath=387
	opt = -O3 -fomit-frame-pointer -ftracer
else
	# (gcc 4.3.2 -O3 makes meisei crash from -ftree-vectorize, in combination with specific arch)
	arch = -march=core2 -mfpmath=sse -mmmx -msse3
	opt = -O2 -finline-functions -funswitch-loops -fomit-frame-pointer -ftracer
endif

compile = gcc -c $(warn) $(arch) $(opt) -I$(libinc) $(dmss)

link = gcc $(win) -o

# requires zlib static lib, MS DirectX SDK headers
lib = -s -L$(libdir) -static-libgcc -static -mwindows -lgdi32 -lkernel32 -lole32 -lcomdlg32 -lcomctl32 -lshell32 -lwinmm -lwininet -luser32 -ld3d9 -lddraw -ldsound -ldinput -ldxguid -luuid -lz $(lmss)

rm = @rm -f -v
md = @mkdir

obj = obj

exe = exe

globals = global.h log.h makefile

objects = $(obj)\am29f040b.o $(obj)\cont.o $(obj)\crystal.o $(obj)\draw.o $(obj)\file.o $(obj)\help.o $(obj)\input.o $(obj)\io.o $(obj)\log.o $(obj)\main.o $(obj)\mapper.o $(obj)\media.o $(obj)\movie.o $(obj)\msx.o $(obj)\netplay.o $(obj)\paste.o $(obj)\psg.o $(obj)\psglogger.o $(obj)\psgtoy.o $(obj)\reverse.o $(obj)\sample.o $(obj)\scc.o $(obj)\screenshot.o $(obj)\settings.o $(obj)\sound.o $(obj)\spriteview.o $(obj)\state.o $(obj)\tape.o $(obj)\tileview.o $(obj)\tool.o $(obj)\update.o $(obj)\version.o $(obj)\vdp.o $(obj)\z80.o $(obj)\resource.o $(obj)\ti_ntsc.o $(obj)\ioapi.o $(obj)\unzip.o

all : makedirs $(exe)\meisei.exe

makedirs : $(obj) $(exe)
$(obj) :
	$(md) $(obj)
$(exe) :
	$(md) $(exe)

$(exe)\meisei.exe : $(objects) makefile
	$(link) $(exe)\meisei.exe $(objects) $(lib)

$(obj)\am29f040b.o : am29f040b.c am29f040b.h state.h $(globals)
	$(compile) am29f040b.c -o $(obj)\am29f040b.o

$(obj)\cont.o : cont.c cont.h io.h z80.h crystal.h movie.h main.h resource.h msx.h draw.h reverse.h state.h paste.h netplay.h input.h settings.h $(globals)
	$(compile) cont.c -o $(obj)\cont.o

$(obj)\crystal.o : crystal.c crystal.h vdp.h movie.h reverse.h cont.h netplay.h z80.h input.h settings.h msx.h resource.h main.h draw.h $(globals)
	$(compile) crystal.c -o $(obj)\crystal.o

$(obj)\draw.o : draw.c draw.h mapper.h screenshot.h tool.h movie.h vdp.h input.h file.h sound.h ti_ntsc.h crystal.h settings.h main.h msx.h resource.h $(globals)
	$(compile) draw.c -o $(obj)\draw.o

$(obj)\file.o : file.c file.h zlib.h msx.h resource.h state.h movie.h draw.h mapper.h unzip.h media.h settings.h main.h tool.h spriteview.h tileview.h psglogger.h psgtoy.h $(globals)
	$(compile) file.c -o $(obj)\file.o

$(obj)\help.o : help.c help.h main.h resource.h file.h version.h settings.h $(globals)
	$(compile) help.c -o $(obj)\help.o

$(obj)\input.o : input.c input.h netplay.h resource.h z80.h version.h crystal.h cont.h sound.h draw.h settings.h msx.h main.h $(globals)
	$(compile) input.c -o $(obj)\input.o

$(obj)\io.o : io.c io.h state.h sound.h z80.h crystal.h mapper.h vdp.h psg.h cont.h $(globals)
	$(compile) io.c -o $(obj)\io.o

$(obj)\log.o : log.c draw.h main.h crystal.h $(globals)
	$(compile) log.c -o $(obj)\log.o

$(obj)\main.o : main.c main.h tape.h psglogger.h tool.h movie.h reverse.h state.h psg.h paste.h netplay.h z80.h media.h vdp.h io.h mapper.h crystal.h sound.h help.h input.h settings.h msx.h version.h draw.h resource.h file.h cont.h update.h $(globals)
	$(compile) main.c -o $(obj)\main.o

$(obj)\mapper.o : mapper.c mapper_table.h mapper.h msx.h cont.h main.h resource.h reverse.h tape.h io.h am29f040b.h scc.h netplay.h sound.h z80.h crystal.h psg.h state.h file.h settings.h sample.h $(globals)
	$(compile) mapper.c -o $(obj)\mapper.o

$(obj)\media.o : media.c media.h vdp.h psg.h reverse.h tape.h psglogger.h movie.h crystal.h draw.h netplay.h mapper.h file.h settings.h resource.h main.h $(globals)
	$(compile) media.c -o $(obj)\media.o

$(obj)\movie.o : movie.c movie.h tape.h psglogger.h settings.h main.h resource.h version.h netplay.h file.h reverse.h crystal.h state.h draw.h z80.h vdp.h cont.h io.h psg.h mapper.h msx.h $(globals)
	$(compile) movie.c -o $(obj)\movie.o

$(obj)\msx.o : msx.c msx.h tool.h psglogger.h reverse.h state.h paste.h io.h mapper.h vdp.h resource.h z80.h settings.h cont.h main.h file.h sound.h crystal.h draw.h input.h $(globals)
	$(compile) msx.c -o $(obj)\msx.o

$(obj)\netplay.o : netplay.c netplay.h tape.h cont.h reverse.h settings.h crystal.h mapper.h version.h file.h main.h resource.h msx.h zlib.h $(globals)
	$(compile) netplay.c -o $(obj)\netplay.o

$(obj)\paste.o : paste.c paste.h paste_table.h movie.h reverse.h mapper.h cont.h tape.h netplay.h main.h resource.h $(globals)
	$(compile) paste.c -o $(obj)\paste.o

$(obj)\psg.o : psg.c psg.h psgtoy.h io.h settings.h reverse.h state.h crystal.h z80.h sound.h cont.h $(globals)
	$(compile) psg.c -o $(obj)\psg.o

$(obj)\psglogger.o : psglogger.c psglogger.h resource.h crystal.h psg.h psgtoy.h msx.h mapper.h movie.h main.h settings.h file.h $(globals)
	$(compile) psglogger.c -o $(obj)\psglogger.o

$(obj)\psgtoy.o : psgtoy.c psgtoy.h psg.h zlib.h tileview.h psglogger.h movie.h sound.h input.h settings.h main.h resource.h file.h tool.h $(globals)
	$(compile) psgtoy.c -o $(obj)\psgtoy.o

$(obj)\reverse.o : reverse.c reverse.h tape.h psglogger.h crystal.h movie.h settings.h netplay.h draw.h input.h sound.h z80.h vdp.h io.h psg.h mapper.h msx.h cont.h $(globals)
	$(compile) reverse.c -o $(obj)\reverse.o

$(obj)\sample.o : sample.c sample.h state.h file.h $(globals)
	$(compile) sample.c -o $(obj)\sample.o

$(obj)\scc.o : scc.c scc.h psg.h crystal.h z80.h state.h sound.h settings.h $(globals)
	$(compile) scc.c -o $(obj)\scc.o

$(obj)\screenshot.o : screenshot.c screenshot.h file.h draw.h mapper.h zlib.h $(globals)
	$(compile) screenshot.c -o $(obj)\screenshot.o

$(obj)\settings.o : settings.c settings.h version.h tool.h draw.h input.h sound.h file.h $(globals)
	$(compile) settings.c -o $(obj)\settings.o

$(obj)\sound.o : sound.c sound.h psg.h scc.h vdp.h cont.h input.h reverse.h mapper.h io.h settings.h msx.h crystal.h main.h resource.h $(globals)
	$(compile) sound.c -o $(obj)\sound.o

$(obj)\spriteview.o : spriteview.c spriteview.h tool.h settings.h file.h vdp.h main.h resource.h input.h screenshot.h movie.h netplay.h $(globals)
	$(compile) spriteview.c -o $(obj)\spriteview.o

$(obj)\state.o : state.c state.h tape.h psglogger.h settings.h movie.h reverse.h crystal.h main.h resource.h file.h netplay.h msx.h draw.h z80.h vdp.h cont.h io.h psg.h mapper.h $(globals)
	$(compile) state.c -o $(obj)\state.o

$(obj)\tape.o : tape.c tape.h zlib.h main.h state.h resource.h io.h mapper.h z80.h psg.h file.h $(globals)
	$(compile) tape.c -o $(obj)\tape.o

$(obj)\tileview.o : tileview.c tileview.h tool.h settings.h file.h vdp.h main.h resource.h input.h screenshot.h movie.h netplay.h $(globals)
	$(compile) tileview.c -o $(obj)\tileview.o

$(obj)\tool.o : tool.c tool.h settings.h tileview.h spriteview.h psgtoy.h psg.h psglogger.h vdp.h draw.h main.h resource.h $(globals)
	$(compile) tool.c -o $(obj)\tool.o

$(obj)\update.o : update.c update.h file.h main.h version.h resource.h zlib.h $(globals)
	$(compile) update.c -o $(obj)\update.o

$(obj)\version.o : version.c version.h $(globals)
	$(compile) version.c -o $(obj)\version.o

$(obj)\vdp.o : vdp.c vdp.h resource.h reverse.h msx.h tool.h main.h settings.h state.h crystal.h io.h draw.h z80.h sound.h $(globals)
	$(compile) vdp.c -o $(obj)\vdp.o

$(obj)\z80.o : z80.c z80.h state.h draw.h io.h mapper.h crystal.h file.h input.h cont.h vdp.h $(globals)
	$(compile) z80.c -o $(obj)\z80.o

$(obj)\resource.o : resource_eng.rc resource.h version.h icon.ico icon_blocky.ico icon_doc.ico meisei.manifest p_315-5124.pal p_v9938.pal p_tms9129.pal p_konami.pal p_vampier.pal p_wolf.pal pause.raw bm_tick_vertical.bmp bm_tick_vertical_wide.bmp bm_tick_horizontal.bmp bm_tick_horizontal_wide.bmp psg_presets.bin makefile
	windres -Iresources resource_eng.rc -o $(obj)\resource.o

$(obj)\ti_ntsc.o : ti_ntsc.c ti_ntsc.h ti_ntsc_config.h $(globals)
	$(compile) ti_ntsc.c -o $(obj)\ti_ntsc.o

$(obj)\ioapi.o : ioapi.c ioapi.h zlib.h zconf.h makefile
	$(compile) ioapi.c -o $(obj)\ioapi.o

$(obj)\unzip.o : unzip.c zlib.h zconf.h unzip.h ioapi.h crypt.h makefile
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
