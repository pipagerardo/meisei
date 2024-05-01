REM ******************************************************************************************
REM *** Build: Release (Windows 64) in Meisei 64 (compiler: GNU GCC Compiler UCRT 64 bits) ***
REM ******************************************************************************************

REM *******************************************************************
REM ***             ASIGNAMOS LA RUTA DEL COMPILADOR                ***
REM *******************************************************************
REM ***  MSYS2 DEBE ESTAR INSTALADO EN C:\msys64\                   ***
REM ***    https://www.msys2.org/                                   *** 
REM ***    C:\msys64\msys2.exe			                    ***
REM ***    pacman -S mingw-w64-ucrt-x86_64-gcc                      ***
REM *******************************************************************

PATH=%PATH%;C:\msys64\ucrt64\bin

REM *******************************************************************
REM ***            CREAMOS LOS DIRECTORIOS DE TRABAJO               ***
REM *******************************************************************

MKDIR bin
MKDIR obj\esp_64bits

REM *******************************************************************
REM ***              COMPILAMOS LOS ARCHIVOS OBJETO                 ***
REM *******************************************************************

gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\am29f040b.c  -o obj\esp_64bits\am29f040b.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\cont.c       -o obj\esp_64bits\cont.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\crystal.c    -o obj\esp_64bits\crystal.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\draw.c       -o obj\esp_64bits\draw.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\file.c       -o obj\esp_64bits\file.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\help.c       -o obj\esp_64bits\help.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\input.c      -o obj\esp_64bits\input.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\io.c         -o obj\esp_64bits\io.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\ioapi.c      -o obj\esp_64bits\ioapi.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\log.c        -o obj\esp_64bits\log.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\main.c       -o obj\esp_64bits\main.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\mapper.c     -o obj\esp_64bits\mapper.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\media.c      -o obj\esp_64bits\media.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\movie.c      -o obj\esp_64bits\movie.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\msx.c        -o obj\esp_64bits\msx.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\netplay.c    -o obj\esp_64bits\netplay.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\paste.c      -o obj\esp_64bits\paste.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\psg.c        -o obj\esp_64bits\psg.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\psglogger.c  -o obj\esp_64bits\psglogger.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\psgtoy.c     -o obj\esp_64bits\psgtoy.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\reverse.c    -o obj\esp_64bits\reverse.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\sample.c     -o obj\esp_64bits\sample.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\scc.c        -o obj\esp_64bits\scc.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\screenshot.c -o obj\esp_64bits\screenshot.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\settings.c   -o obj\esp_64bits\settings.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\sound.c      -o obj\esp_64bits\sound.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\spriteview.c -o obj\esp_64bits\spriteview.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\state.c      -o obj\esp_64bits\state.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\tape.c       -o obj\esp_64bits\tape.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\ti_ntsc.c    -o obj\esp_64bits\ti_ntsc.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\tileview.c   -o obj\esp_64bits\tileview.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\tool.c       -o obj\esp_64bits\tool.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\unzip.c      -o obj\esp_64bits\unzip.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\update.c     -o obj\esp_64bits\update.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\vdp.c        -o obj\esp_64bits\vdp.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\version.c    -o obj\esp_64bits\version.o
gcc.exe -fomit-frame-pointer -O3 -std=c17 -ftracer -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c .\src\z80.c        -o obj\esp_64bits\z80.o


REM *******************************************************************
REM ***             COMPILAMOS EL ARCHIVO DE RECURSOS               ***
REM *******************************************************************

windres.exe -Isrc\resources -J rc -O coff -i .\src\resource_esp.rc -o obj\esp_64bits\resource_esp.res

REM *******************************************************************
REM ***        CREAMOS EL EJECUTABLE Y ENLACE DE LIBRERIAS          ***
REM *******************************************************************

gcc.exe -LC:\msys64\ucrt64\lib -LC:\msys64\ucrt64\bin -LC:\msys64\ucrt64\x86_64-w64-mingw32\bin -LC:\msys64\ucrt64\x86_64-w64-mingw32\lib ^
-o bin\Meisei_esp_64bits.exe ^
obj\esp_64bits\am29f040b.o obj\esp_64bits\cont.o       obj\esp_64bits\crystal.o obj\esp_64bits\draw.o       obj\esp_64bits\file.o ^
obj\esp_64bits\help.o      obj\esp_64bits\input.o      obj\esp_64bits\io.o      obj\esp_64bits\ioapi.o      obj\esp_64bits\log.o ^
obj\esp_64bits\main.o      obj\esp_64bits\mapper.o     obj\esp_64bits\media.o   obj\esp_64bits\movie.o      obj\esp_64bits\msx.o ^
obj\esp_64bits\netplay.o   obj\esp_64bits\paste.o      obj\esp_64bits\psg.o     obj\esp_64bits\psglogger.o  obj\esp_64bits\psgtoy.o ^
obj\esp_64bits\reverse.o   obj\esp_64bits\sample.o     obj\esp_64bits\scc.o     obj\esp_64bits\screenshot.o obj\esp_64bits\settings.o ^
obj\esp_64bits\sound.o     obj\esp_64bits\spriteview.o obj\esp_64bits\state.o   obj\esp_64bits\tape.o       obj\esp_64bits\ti_ntsc.o ^
obj\esp_64bits\tileview.o  obj\esp_64bits\tool.o       obj\esp_64bits\unzip.o   obj\esp_64bits\update.o     obj\esp_64bits\vdp.o ^
obj\esp_64bits\version.o   obj\esp_64bits\z80.o        obj\esp_64bits\resource_esp.res ^
-static-libgcc -static -s -mwindows ^
-lkernel32 -luser32 -lgdi32 -lshell32 -lole32 -lcomdlg32 -lcomctl32 -lwinmm -lwininet -ld3d9 -lddraw -ldsound -ldinput -ldxguid -luuid -lz

REM *******************************************************************
REM ***                   BORRAMOS LA MORRALLA                      ***
REM *******************************************************************

DEL /S /Q obj\esp_64bits\*.*
RMDIR /S /Q obj\esp_64bits

REM *******************************************************************
REM ***                   ESO ES TODO AMIGOS!!!                     ***
REM *******************************************************************

PAUSE
