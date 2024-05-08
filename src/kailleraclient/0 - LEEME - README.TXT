----------------------------------------------------------------------

ESPAÑOL
=======

Código Fuente Original: https://github.com/God-Weapon/SupraclientC
Modificado por PipaGerardo para ser compilado en puro C.

Compilar en 64 bits:
-O3 -std=c17 -march=x86-64 -DMEISEI_ESP -IC:\msys64\ucrt64\include 

Compilar en 32 bits:
-O3 -std=c17 -march=i686 -DMEISEI_ESP -IC:\msys64\mingw32\include 

Linker:
-s -static -mwindows -lkernel32 -luser32 -lgdi32 -lwinspool -lcomdlg32 -ladvapi32 -lshell32 -lole32 -loleaut32 -luuid -lodbc32 -lodbccp32 -lws2_32 -lcomctl32

----------------------------------------------------------------------

ENGLISH
=======

Original Source Code: https://github.com/God-Weapon/SupraclientC
Modified by PipaGerardo to be compiled in pure C.

Compile in 64-bit:
-O3 -std=c17 -march=x86-64 -IC:\msys64\ucrt64\include 

Compile in 32-bit:
-O3 -std=c17 -march=i686 -IC:\msys64\mingw32\include 

Linker:
-s -static -mwindows -lkernel32 -luser32 -lgdi32 -lwinspool -lcomdlg32 -ladvapi32 -lshell32 -lole32 -loleaut32 -luuid -lodbc32 -lodbccp32 -lws2_32 -lcomctl32

----------------------------------------------------------------------