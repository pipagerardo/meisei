REM *******************************************************************
REM ***   Build: Meisei ENG (compiler: GNU GCC Compiler 32 bits)    ***
REM *******************************************************************

REM *******************************************************************
REM ***             ASIGNAMOS LA RUTA DEL COMPILADOR                ***
REM *******************************************************************
REM ***  MSYS2 DEBE ESTAR INSTALADO EN C:\msys64\                   ***
REM ***    https://www.msys2.org/                                   *** 
REM ***    C:\msys64\msys2.exe			                    ***
REM ***    pacman -S mingw-w64-i686-gcc                             ***
REM *******************************************************************

PATH=%PATH%;C:\msys64\mingw32\bin

REM *******************************************************************
REM ***            CREAMOS LOS DIRECTORIOS DE TRABAJO               ***
REM *******************************************************************

MKDIR bin
MKDIR obj\eng_32bits

REM *******************************************************************
REM ***              COMPILAMOS LOS ARCHIVOS OBJETO                 ***
REM *******************************************************************

gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\am29f040b.c  -o obj\eng_32bits\am29f040b.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\cont.c       -o obj\eng_32bits\cont.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\crystal.c    -o obj\eng_32bits\crystal.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\draw.c       -o obj\eng_32bits\draw.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\file.c       -o obj\eng_32bits\file.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\help.c       -o obj\eng_32bits\help.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\input.c      -o obj\eng_32bits\input.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\io.c         -o obj\eng_32bits\io.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\ioapi.c      -o obj\eng_32bits\ioapi.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\log.c        -o obj\eng_32bits\log.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\main.c       -o obj\eng_32bits\main.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\mapper.c     -o obj\eng_32bits\mapper.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\media.c      -o obj\eng_32bits\media.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\movie.c      -o obj\eng_32bits\movie.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\msx.c        -o obj\eng_32bits\msx.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\netplay.c    -o obj\eng_32bits\netplay.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\paste.c      -o obj\eng_32bits\paste.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\psg.c        -o obj\eng_32bits\psg.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\psglogger.c  -o obj\eng_32bits\psglogger.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\psgtoy.c     -o obj\eng_32bits\psgtoy.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\reverse.c    -o obj\eng_32bits\reverse.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\sample.c     -o obj\eng_32bits\sample.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\scc.c        -o obj\eng_32bits\scc.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\screenshot.c -o obj\eng_32bits\screenshot.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\settings.c   -o obj\eng_32bits\settings.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\sound.c      -o obj\eng_32bits\sound.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\spriteview.c -o obj\eng_32bits\spriteview.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\state.c      -o obj\eng_32bits\state.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\tape.c       -o obj\eng_32bits\tape.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\ti_ntsc.c    -o obj\eng_32bits\ti_ntsc.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\tileview.c   -o obj\eng_32bits\tileview.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\tool.c       -o obj\eng_32bits\tool.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\unzip.c      -o obj\eng_32bits\unzip.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\update.c     -o obj\eng_32bits\update.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\vdp.c        -o obj\eng_32bits\vdp.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\version.c    -o obj\eng_32bits\version.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=i686 -DMEISEI_32BITS -IC:\msys64\mingw32\include -c .\src\z80.c        -o obj\eng_32bits\z80.o

REM *******************************************************************
REM ***             COMPILAMOS EL ARCHIVO DE RECURSOS               ***
REM *******************************************************************

windres.exe -Isrc\resources -J rc -O coff -i .\src\resource_eng.rc -o obj\eng_32bits\resource_eng.res

REM *******************************************************************
REM ***        CREAMOS EL EJECUTABLE Y ENLACE DE LIBRERIAS          ***
REM *******************************************************************

gcc.exe -LC:\msys64\mingw32\lib -LC:\msys64\mingw32\bin -LC:\msys64\mingw32\i686-w64-mingw32\bin -o ^
bin\Meisei32_eng.exe ^
obj\eng_32bits\am29f040b.o obj\eng_32bits\cont.o       obj\eng_32bits\crystal.o obj\eng_32bits\draw.o       obj\eng_32bits\file.o ^
obj\eng_32bits\help.o      obj\eng_32bits\input.o      obj\eng_32bits\io.o      obj\eng_32bits\ioapi.o      obj\eng_32bits\log.o ^
obj\eng_32bits\main.o      obj\eng_32bits\mapper.o     obj\eng_32bits\media.o   obj\eng_32bits\movie.o      obj\eng_32bits\msx.o ^
obj\eng_32bits\netplay.o   obj\eng_32bits\paste.o      obj\eng_32bits\psg.o     obj\eng_32bits\psglogger.o  obj\eng_32bits\psgtoy.o ^
obj\eng_32bits\reverse.o   obj\eng_32bits\sample.o     obj\eng_32bits\scc.o     obj\eng_32bits\screenshot.o obj\eng_32bits\settings.o ^
obj\eng_32bits\sound.o     obj\eng_32bits\spriteview.o obj\eng_32bits\state.o   obj\eng_32bits\tape.o       obj\eng_32bits\ti_ntsc.o ^
obj\eng_32bits\tileview.o  obj\eng_32bits\tool.o       obj\eng_32bits\unzip.o   obj\eng_32bits\update.o     obj\eng_32bits\vdp.o ^
obj\eng_32bits\version.o   obj\eng_32bits\z80.o        obj\eng_32bits\resource_eng.res ^
-static -s -mwindows ^
-lgdi32 -lkernel32 -lole32 -lcomdlg32 -lcomctl32 -lshell32 -lwinmm -lwininet -luser32 -ld3d9 -lddraw -ldsound -ldinput -ldxguid -luuid -lz

REM *******************************************************************
REM ***                   BORRAMOS LA MORRALLA                      ***
REM *******************************************************************

DEL /S /Q obj\eng_32bits\*.*
RMDIR /S /Q obj\eng_32bits

REM *******************************************************************
REM ***                   ESO ES TODO AMIGOS!!!                     ***
REM *******************************************************************

PAUSE



