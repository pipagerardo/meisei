REM *************************************************************************
REM *** Build: SupraclientC ESP (compiler: GNU GCC Compiler UCRT 64 bits) ***
REM *************************************************************************

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
REM ***              COMPILAMOS LA LIBRERIA DLL                     ***
REM *******************************************************************

gcc.exe -O3 -std=c17 -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include -c src\kailleraclient\kailleraclient.c -o obj\esp_64bits\kailleraclient.o

gcc.exe -shared ^
-Wl,--output-def=bin\kaillera\libkailleraclient_esp.def ^
-Wl,--out-implib=bin\kaillera\libkailleraclient_esp.a ^
-Wl,--dll -LC:\msys64\ucrt64\lib obj\esp_64bits\kailleraclient.o -o bin\kaillera\kailleraclient_esp.dll ^
-s -static -mwindows -lkernel32 -luser32 -lgdi32 -lwinspool -lcomdlg32 -ladvapi32 -lshell32 -lole32 -loleaut32 -luuid -lodbc32 -lodbccp32 -lws2_32 -lcomctl32

REM *******************************************************************
REM ***                   BORRAMOS LA MORRALLA                      ***
REM *******************************************************************

DEL /S /Q bin\kaillera\*.a
DEL /S /Q bin\kaillera\*.def
DEL /S /Q obj\esp_64bits\*.*
RMDIR /S /Q obj\esp_64bits

REM *******************************************************************
REM ***                   ESO ES TODO AMIGOS!!!                     ***
REM *******************************************************************

PAUSE

