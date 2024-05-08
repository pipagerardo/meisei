/******************************************************************************
 *                                                                            *
 *                                 "file.h"                                   *
 *                                                                            *
 ******************************************************************************/

#ifndef FILE_H
#define FILE_H

#include "global.h"

typedef struct {
	char* name;
	char* dir;
	char* ext;
	char* filename;

	char* filename_in_zip_pre;
	char* filename_in_zip_post;

	char* appdir;                   // Directorio de la aplicación
	char* appname;                  // Nombre de la aplicacion (Meisei)
	char* temp;                     // Ruta de archivo temporal.
	char* batterydir;               // ???
	char* tooldir;                  // Directorio Herramientas IO
	char* patchdir;                 // Directorio para los parches de los juegos.
	char* palettedir;               // Directorio para las paletas de color.
	char* romdir;                   // Directorio para los Cartuchos MSX.
	char* casdir;                   // Directorio para las Cintas de MSX.
	char* sampledir;                // Directorio para los Samples de Audio
	char* shotdir;                  // Directorio para las Capturas
	char* biosdir;                  // Directorio de las BIOS de MSX
	char* statedir;                 // Directorio Estados, puntos de guardado de los juegos.

	u32 crc32;
	u32 size;

	int is_zip;
} File;

// File* file;
extern File* file;

FILE* file_get_fd(void);    // retorna el puntero FILE*

void file_setfile( char** filedir, const char* filename, const char* fileext, const char* zipextension );

int file_accessible(void);
int file_open(void);
int file_is_open(void);
int file_open_rw(void);
int file_open_custom(FILE**,const char*);
u8* file_from_resource(int,int*);
void file_from_resource_close(void);
int file_save(void);
int file_save_text(void);
int file_save_custom(FILE**,const char*);
int file_save_custom_text(FILE**,const char*);
int file_read(u8*,const int);
int file_read_custom(FILE*,u8*,const int);
int file_write(const u8*,const int);
int file_write_custom(FILE*,const u8*,const int);
void file_close(void);
void file_close_custom(FILE*);
int file_patch_init(void);
void file_patch_close(void);

void file_init(void);
void file_directories(void);
INT_PTR CALLBACK file_directories_dialog( HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam );
void file_clean(void);
int file_setdirectory(char**);
void file_settings_save(void);

int file_get_zn(void);
void file_zn_reset(void);
void file_zn_next(void);
void file_zn_prev(void);

#endif /* FILE_H */
