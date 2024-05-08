Archivos Código Fuente:
-----------------------
Carpeta: ./src
-----------------------

global.h
log.h		log.c
am29f040b.h	am29f040b.c
cont.h		cont.c
crystal.h	crystal.c
draw.h		draw.c
file.h		file.c
help.h		help.c
input.h		input.c
io.h		io.c
ioapi.h		ioapi.c
main.h		main.c
mapper.h	mapper_table.h	mapper.c
media.h		media.c
movie.h		movie.c
msx.h		msx.c
netplay.h	netplay.c
paste.h		paste_table.h	paste.c
psg.h		psg.c
psglogger.h	psglogger.c
psgtoy.h	psgtoy.c
reverse.h	reverse.c
sample.h	sample.c
scc.h		scc.c
screenshot.h	screenshot.c
settings.h	settings.c
sound.h		sound.c
spriteview.h	spriteview.c
state.h		state.c
tape.h		tape.c
ti_ntsc.h	ti_ntsc.c
tileview.h	tileview.c
tool.h		tool.c
zconf.h		zlib.h	
unzip.h		crypt.h		unzip.c
update.h	update.c
vdp.h		vdp.c
version.h	version.c
z80.h		z80.c

resource.h
resource_esp.rc		// Español
resource_eng.rc		// Inglés

Archivos de Recursos:
------------------------
Carpeta: ./src/resources
------------------------

icon.ico 
icon_blocky.ico 
icon_doc.ico 
meisei.manifest 
p_315-5124.pal 
p_v9938.pal 
p_tms9129.pal 
p_konami.pal 
p_vampier.pal 
p_wolf.pal 
pause.raw 
bm_tick_vertical.bmp 
bm_tick_vertical_wide.bmp 
bm_tick_horizontal.bmp 
bm_tick_horizontal_wide.bmp 
psg_presets.bin

MAKEFILE:
---------
makefile	(No funciona)

OPCIONES DE ENLAZADO:
---------------------

Parar Windows 64 bits
---------------------
-march=x86-64

Para Windows 32 bits
--------------------
-march=i686 
-mfpmath=387

Comunes:
--------

-std=c17
-static
-fomit-frame-pointer
-ftracer
-O3
-s

-mwindows		// Windows GUI App
-mconsole		// Consola Windows [Opcional]
-lgdi32
-lkernel32
-lole32
-lcomdlg32
-lcomctl32
-lshell32
-lwinmm
-lwininet		// NET [Opcional]
-luser32
-ld3d9
-lddraw
-ldsound
-ldinput
-ldxguid
-luuid
-lz

DEFINES:
--------

MEISEI_ESP		Activa el idioma en Español

MEISEI_32BITS		Para compilar a 32 bits

MEISEI_UPDATE		Activa actualización del programa (no funciona)

DEBUG_PROFILER
