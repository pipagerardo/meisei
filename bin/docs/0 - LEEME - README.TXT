----------------------------------------------------------------------------

ESPAÑOL
=======

----------------------------------------------------------------------------
CONSIDERACIONES IMPORTANTES:
============================

Meisei es un emulador de MSX1, no emula MSX2, ni MSX2+, ni leches en vinagre.
¿Darle soporte a MSX2? A día de hoy dudo mucho que esté cualificado para esa
tarea por falta de conocimientos. Cuando suba el código fuente a Github 
veremos si alguien se anima y me echa una mano...

Está programado en C usando Winapi, DirectDraw y Direct X, así que olvídate
de compilarlo en otras plataformas que no son Windows 8, 10 y 11.
Adaptarlo a otras plataformas usando SDL2 no digo que no sea posible, pero
está fuera de mi tiempo limitado de vida en este planeta.

Meisei estaba originalmente en Inglés, ha sido traducido al Español y no
pienso darle soporte a ningún otro idioma. Si quieres traducirlo al Élfico
Oscuro de Mordor... pues píllate el código fuente y al lio ¡Campeón!

Tengo intención de darle soporte para cintas de cassete en formato TSX, pero
esto aún no está implementado. No creo que sea muy complicado.

Puedes escribirme a: pipagerardo@gmail.com

----------------------------------------------------------------------------

WEBS INTERESANTES:
==================

Bios de MSX Requeridas:
	https://download.file-hunter.com/System%20ROMs/machines/
	http://fms.komkon.org/fMSX/#Downloads

Vincent DSK2ROM, necesario para cargar Disquetes:
	https://github.com/joyrex2001/dsk2rom

Imágenes Cartuchos ROM de MSX1
	https://download.file-hunter.com/Games/MSX1/ROM/

Imágenes CAS de cintas MSX1
	https://download.file-hunter.com/Games/MSX1/CAS/
	https://computeremuzone.com/msx/

Imágenes DSK de Disquetes MSX1
	https://download.file-hunter.com/Games/MSX1/DSK/

Imágenes TSX de cintas MSX1
	https://tsx.eslamejor.com/

Imágenes CAS, DSK y ROM de MSX
	http://msxcas.webcindario.com/

Carátulas de MSX:
	https://computeremuzone.com/msx/extras/caratulas

Mapas y Soluciones para los juegos
	http://msxsolutions.msxblue.com/index-2.html

----------------------------------------------------------------------------

BIOS Y ROM's REQUERIDAS:
========================

Es obligatorio tener como mínimo en el directorio "./bios" una BIOS de MSX
llamada "default_bios.zip". Si usas una C-BIOS no puedes cargar cintas
de cassete, ni disquetes, sólo sirve para cartuchos ROM.
Para poder cargar cintas de cassete y disquetes es obligatorio tener una 
BIOS con BASIC. Yo uso la del MSX Philips VG-8020 pero puedes bajarte la 
BIOS que más te guste siempre y cuando sea de MSX1.

Para poder cargar Disquetes se utiliza la Vincent DSK2ROM que convierte un
DSK a ROM y se ejecuta como un cartucho. Evidentemente no se puede guardar
nada en estos discos convertidos a ROM, sólo son de lectura.

El Vicent DSK2ROM debe estar en la carpeta "./bios" y puede llamarse:
	dsk2rom.rom		dsk2rom.zip
	dsk2rom-0.80.rom	dsk2rom-0.80.zip
	dsk2rom-0.70.rom	dsk2rom-0.70.zip

----------------------------------------------------------------------------

CARGA DE JUEGOS EN CARTUCHO (ROM): [C-BIOS Requerida]
=====================================================

1 - Archivo -> Medios (Ctrl+M) -> Cartuchos  -> ... -> Bien+Reinicio

----------------------------------------------------------------------------

CARGA DE JUEGOS EN CINTA (CAS): [BIOS MSX-BASIC Requerida]
==========================================================

1 - Archivo -> Medios (Crtl+M) -> Cintas -> ... -> Bien

2 - Selecionamos una imagen de Cinta CAS.

3 - Miramos en Bloques "Blocks" el tipo de imagen:
    BASIC:	CLOAD (despues de cargarse, apretar F5 para ejecutar)
    binary:	BLOAD"CAS:",R
    ASCII:	RUN"CAS:"
            	LOAD"CAS:",R  

----------------------------------------------------------------------------

CARGA DE JUEGOS EN DISQUETE (DSK): [Vincent DSK2ROM Requerido]
==============================================================

1 - Archivo -> Medios (Ctrl+M) -> Cartuchos  -> ... -> Bien+Reinicio

----------------------------------------------------------------------------

CONTROLES DEL EMULADOR:
=======================

+ Controles predeterminados de uso común:
F6/F7:		Cambiar estado de guardado/ranura de película
F8:		Guardar estado
F9:		Estado de carga
F10 (mantener):	Desaceleración
F11 (mantener):	Marcha atrás
F12 (mantener):	Avance rápido

+ Controles predeterminados para teclas MSX no disponibles en teclados de PC:
Graph:		Alt Izquierdo
Code:		Alt Derecho
Stop:		Avpág (Page Up)
Select:		Fin   (End)
Accent Mark:	Repág (Page Down)

Ver imagen "MSX_Teclado.png" para hacerse una idea de un teclado de MSX.

----------------------------------------------------------------------------

MAPEADORES DE CARTUCHOS:
========================

Las ROMs de MSX no tienen encabezado, por lo que depende de ti elegir el tipo 
de mapeador. Meisei lo hace un poco más fácil al tener una base de datos interna 
con algunas ROMs GoodMSX, y autodetección para ROMs comunes. También es posible 
especificar el tipo de mapeador dentro del nombre del archivo ROM, insensible a 
mayúsculas y minúsculas, como: 

	nombre_juego{mapper}.rom
	nombre_juego{mapper}.zip
	...
	nemesis{SCC-I}.rom
	nemesis{SCC-I}.zip
	salamander{SCC}.rom 
	salamander{SCC}.zip
	...

Mapeador	Nombre largo
------------------------------------------
{Qur'an}	"Al-Alamiah Al-Qur'an"
{ASCII}		"ASCII MegaROM"
{BTL80}		"Bootleg 80-in-1"
{BTL90}		"Bootleg 90-in-1"
{BTL126}	"Bootleg 126-in-1"
{CBlaim}	"dB-Soft Cross Blaim"
{TAM-S1}	"Irem TAM-S1"
{GM2}		"Konami Game Master 2"
{SCC}		"Konami SCC"
{SCC-I}		"Konami Sound Cartridge"
{KSyn}		"Konami Synthesizer"
{VRC}		"Konami VRC"
{INK}		"Matra INK"
{HFox}		"MicroCabin Harry Fox"
{NM}		"No mapper"
{PAC}		"Panasoft PAC"
{MFRSCC}	"Pazos Mega Flash ROM SCC"	
{Playball}	"Sony Playball"
{DSK2ROM}	"Vincent DSK2ROM"

----------------------------------------------------------------------------

OPCIONES DE AUTOARRANQUE:
=========================

Si en el directorio principal donde está el ejecutable del Emulador Meisei,
pones un juego de cartucho [ROM] llamado:
	"meisei_autorun.rom" o "meisei_autorun.zip"
El emulador arrancará directamente dicho juego en formato ROM. Si está 
compirmido en ZIP y dentro hay un juego DSK se utilizará Vincent DSK2ROM.

También puede introducirse un juego en ROM o DSK por línea de comandos, para
eso tienes que crear un archivo BAT con por ejemplo el siguiente:

REM INICIO autorun.bat
Meisei_esp.exe games\rom\MJTT.zip
REM FIN autorun.bat

----------------------------------------------------------------------------

JUEGO EN RED
============

Meisei utiliza "SupraclientC" que es una versión de kaillera.
	https://github.com/God-Weapon/SupraclientC

Para usar "kailleraclient_esp.dll" o "kailleraclient32_esp.dll" deben estar en 
la carpeta "./kaillera". Hay dos versiones que corresponden a su versión
corespondiente de Meisei a 64 y 32 bits. Si borras estas DLL's no funcionará
el juego en red.

----------------------------------------------------------------------------

ENGLISH
=======

IMPORTANT CONSIDERATIONS:
============================

Meisei is an MSX1 emulator, it does not emulate MSX2, MSX2+, or pickled milk.
Support MSX2? To this day I highly doubt that I am qualified for that
task due to lack of knowledge. When you upload the source code to Github
We'll see if someone is encouraged and gives me a hand...

It is programmed in C using Winapi, DirectDraw and Direct
to compile it on platforms other than Windows 8, 10 and 11.
Adapt it to other platforms using SDL2, I'm not saying it's not possible, but
It is outside my limited time of life on this planet.

Meisei was originally in English, it has been translated into Spanish and does not
I plan to support no other language. If you want to translate it into Elvish
Dark One of Mordor... then grab the source code and get to it, Champion!

I intend to support TSX format cassette tapes, but
this is not implemented yet. I don't think it's very complicated.

You can write to me at: pipagerardo@gmail.com

----------------------------------------------------------------------------

INTERESTING WEBSITES:
=====================

MSX Bios Required:
	https://download.file-hunter.com/System%20ROMs/machines/
	http://fms.komkon.org/fMSX/#Downloads

Vincent DSK2ROM, required to load Floppy Disks:
	https://github.com/joyrex2001/dsk2rom

Images MSX1 ROM Cartridges
	https://download.file-hunter.com/Games/MSX1/ROM/

CAS images of MSX1 tapes
	https://download.file-hunter.com/Games/MSX1/CAS/
	https://computeremuzone.com/msx/

MSX1 Floppy Disk DSK Images
	https://download.file-hunter.com/Games/MSX1/DSK/

TSX images of MSX1 tapes
	https://tsx.eslamejor.com/

MSX CAS, DSK and ROM images
	http://msxcas.webcindario.com/

MSX covers:
	https://computeremuzone.com/msx/extras/caratulas

Maps and Solutions for games
	http://msxsolutions.msxblue.com/index-2.html

----------------------------------------------------------------------------

BIOS AND ROM's REQUIRED:
========================

It is mandatory to have at least an MSX BIOS called "default_bios.zip" in 
the "./bios" directory. If you use a C-BIOS you cannot load cassette tapes or 
floppy disks, it only works for ROM cartridges.
In order to load cassette tapes and floppy disks, it is mandatory to have a 
BIOS with BASIC. I use the one from the MSX Philips VG-8020 but you can download 
the BIOS that you like the most as long as it is from MSX1.

In order to load floppy disks, the Vincent DSK2ROM is used, which converts a
DSK to ROM and runs as a cartridge. Obviously it cannot be saved
nothing on these disks converted to ROM, they are read only.

The Vicent DSK2ROM should be in the "./bios" folder and can be called:
	dsk2rom.rom		dsk2rom.zip
	dsk2rom-0.80.rom	dsk2rom-0.80.zip
	dsk2rom-0.70.rom	dsk2rom-0.70.zip

----------------------------------------------------------------------------

LOADING GAMES INTO CARTRIDGE (ROM): [C-BIOS Required]
=====================================================

1 - File -> Media (Ctrl+M) -> Cartridges -> ... -> OK + Reset

----------------------------------------------------------------------------

LOADING GAMES ON TAPE (CAS): [MSX-BASIC BIOS Required]
======================================================

1 - File -> Media (Ctrl+M) -> Tape -> ... -> OK

2 - We select a CAS Tape image.

3 - We look at the type of image in Blocks:
     BASIC:	CLOAD 		(after loading, press F5 to execute)
     binary:	BLOAD"CAS:",R
     ASCII:	RUN"CAS:"
		LOAD"CAS:",R

----------------------------------------------------------------------------

LOADING GAMES ON FLOPPY DISKET (DSK): [Vincent DSK2ROM Required]
================================================================

1 - File -> Media (Ctrl+M) -> Cartridges -> ... -> OK + Reset

----------------------------------------------------------------------------

EMULATOR CONTROLS:
==================

+ Commonly used default controls:
F6/F7:		Change save status/movie slot
F8:		Save status
F9:		Charging status
F10 (hold):	Deceleration
F11 (hold):	Reverse
F12 (hold):	Fast forward

+ Default controls for MSX keys not available on PC keyboards:
Graph:		Left Alt
Code:		Right Alt
Stop:		Page Up
Select:		End
Accent Mark:	Page Down

See image "MSX_Keyboard.png" to get an idea of an MSX keyboard.

----------------------------------------------------------------------------

CARTRIDGE MAPPERS:
==================

MSX ROMs have no header, so it's up to you to choose the type
of mapper. Meisei makes it a little easier by having an internal database
with some GoodMSX ROMs, and auto-detection for common ROMs. It is also possible
specify the mapper type within the ROM file name, insensitive to
upper and lower case, such as:

	game_name{mapper}.rom
	game_name{mapper}.zip
	...
	nemesis{SCC-I}.rom
	nemesis{SCC-I}.zip
	salamander{SCC}.rom
	salamander{SCC}.zip
	...

Mapper 		Long Name
-------------------------------------------
{Qur'an}	"Al-Alamiah Al-Qur'an"
{ASCII}		"ASCII MegaROM"
{BTL80}		"Bootleg 80-in-1"
{BTL90}		"Bootleg 90-in-1"
{BTL126}	"Bootleg 126-in-1"
{CBlaim}	"dB-Soft Cross Blaim"
{TAM-S1}	"Irem TAM-S1"
{GM2}		"Konami Game Master 2"
{SCC}		"Konami SCC"
{SCC-I}		"Konami Sound Cartridge"
{KSyn}		"Konami Synthesizer"
{VRC}		"Konami VRC"
{INK}		"Matra INK"
{HFox}		"MicroCabin Harry Fox"
{NM}		"No mapper"
{PAC}		"Panasoft PAC"
{MFRSCC}	"Pazos Mega Flash ROM SCC"	
{Playball}	"Sony Playball"
{DSK2ROM}	"Vincent DSK2ROM"

----------------------------------------------------------------------------

AUTOSTART OPTIONS:
==================

If in the main directory where the Meisei Emulator executable is,
you put a game cartridge [ROM] called:
	"meisei_autorun.rom" or "meisei_autorun.zip"
The emulator will directly boot said game in ROM format. Snap
Compressed in ZIP and inside there is a DSK game Vincent DSK2ROM will be used.

A game can also be entered in ROM or DSK via the command line, to
That's why you have to create a BAT file with, for example, the following:

REM START autorun.bat
Meisei_eng.exe games\rom\MJTT.zip
REM END autorun.bat

----------------------------------------------------------------------------

NETWORK GAME
============

Meisei uses "SupraclientC" which is a version of kaillera.
	https://github.com/God-Weapon/SupraclientC

To use "kailleraclient_eng.dll" or "kailleraclient32_eng.dll" they must be in 
the folder "./kaillera". There are two versions that correspond to your version
Meisei counterpart at 64 and 32 bits. If you delete these DLL's it won't work
networked play.

----------------------------------------------------------------------------
