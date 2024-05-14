DEL /Q *.depend 
DEL /Q *.layout
DEL /S /Q ..\bin\kaillera\*.a
DEL /S /Q ..\bin\kaillera\*.def
DEL /S /Q ..\obj\*.o
DEL /S /Q ..\obj\*.res
RMDIR /S /Q ..\obj\esp_64bits
RMDIR /S /Q ..\obj\eng_64bits
RMDIR /S /Q ..\obj\esp_32bits
RMDIR /S /Q ..\obj\eng_32bits
