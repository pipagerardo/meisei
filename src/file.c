/******************************************************************************
 *                                                                            *
 *                                "file.c"                                    *
 *                                                                            *
 ******************************************************************************/
#include "file.h"

#define COBJMACROS
#include <shlobj.h>
#include <io.h>

#include "zlib.h"
#include "unzip.h"
#include "settings.h"
#include "media.h"
#include "main.h"
#include "mapper.h"
#include "draw.h"
#include "resource.h"
#include "msx.h"
#include "state.h"
#include "movie.h"
#include "tool.h"
#include "spriteview.h"
#include "tileview.h"
#include "psgtoy.h"
#include "psglogger.h"

/* EXTERN */
File* file;

static struct {
	FILE*   fd;
	unzFile zfd;
	HGLOBAL g;

	char zipext[16];
	int zn;
} filedes;

static struct {
	FILE* fd;
	unzFile zfd;

	int offset;
	int patching;
	int rle;
	u32 address;
	u16 bytes;
	u8* buffer;
	u16 buffer_offset;
	u32 show_size;
} patch;


FILE* file_get_fd(void) { return filedes.fd; }
int file_get_zn(void) { return filedes.zn; }
void file_zn_reset(void) { filedes.zn=0; }
void file_zn_next(void) { filedes.zn++; }
void file_zn_prev(void) { filedes.zn-=(filedes.zn>0); }
int file_accessible(void) { return access(file->filename,F_OK)==0; }
int file_is_open(void) { if (filedes.fd||filedes.zfd) return TRUE; else return FALSE; }

int file_open (void)
{
	int i;
	char fn[STRING_SIZE];

	if (filedes.fd||filedes.zfd) return FALSE; /* already open */
	if (file->filename==NULL||strlen(file->filename)==0) return FALSE;

	memset(fn,0,STRING_SIZE);

	if (file->ext&&(strlen(file->ext)==3)&&(stricmp(file->ext,"zip")==0)) file->is_zip=TRUE;
	else file->is_zip=FALSE;

	/* zipped */
	if (file->is_zip) {

		unz_file_info info;
		int len,zn=0;

		if ((filedes.zfd=unzOpen(file->filename))==NULL) return FALSE;
		unzGoToFirstFile(filedes.zfd);

		if (filedes.zn==-1) filedes.zn=0;

		#define ZGETFILEINFO()										\
			memset(&info,0,sizeof(info));							\
			memset(fn,0,STRING_SIZE);								\
			unzGetCurrentFileInfo(									\
				filedes.zfd,	/* file */							\
				&info,			/* info */							\
				fn,				/* filename */						\
				STRING_SIZE,	/* filename size */					\
				NULL,0,			/* extra field, extra field size */	\
				NULL,0			/* comment, comment size */			\
			)

		/* filename in zip */
		if (file->filename_in_zip_pre&&strlen(file->filename_in_zip_pre)) {
			if (unzLocateFile(filedes.zfd,file->filename_in_zip_pre,2)==UNZ_OK) { ZGETFILEINFO(); }
			else return FALSE;
		}

		/* other filename */
		else for (i=0;;i++) {
			ZGETFILEINFO();
			len=strlen(fn);
			if ((len>3)&&(stricmp(fn+len-4,filedes.zipext)==0)&&(u32)info.uncompressed_size) {
				if (zn==filedes.zn) break;
				zn++;
			}

			if (unzGoToNextFile(filedes.zfd)!=0) {

				/* go to first valid file */
				unzGoToFirstFile(filedes.zfd);
				for (;;) {
					ZGETFILEINFO();
					if ((u32)info.uncompressed_size) break;
					if (unzGoToNextFile(filedes.zfd)!=0) break;
				}

				filedes.zn=-1;
				break;
			}
		}

		unzOpenCurrentFile(filedes.zfd);
		file->crc32=(u32)info.crc;
		file->size=(u32)info.uncompressed_size;
		MEM_CLEAN(file->filename_in_zip_post);

		if (file->size==0) return FALSE;

		if (strlen(fn)) {
			MEM_CREATE(file->filename_in_zip_post,strlen(fn)+1);
			memcpy(file->filename_in_zip_post,fn,strlen(fn));
		}
	}

	/* normal */
	else {
		if ((filedes.fd=fopen(file->filename,"rb"))==NULL) return FALSE;
		else {
			MEM_CLEAN(file->filename_in_zip_post);

			file->crc32=0;
			fseek(filedes.fd,0,SEEK_END); file->size=ftell(filedes.fd); fseek(filedes.fd,0,SEEK_SET);
			if (file->size==0) return FALSE;

			/* calculate checksum, not on huge files */
			if (file->size<=ROM_MAXSIZE) {
				u8* f=NULL;

				MEM_CREATE(f,file->size);
				if (!fread(f,1,file->size,filedes.fd)) { MEM_CLEAN(f); return FALSE; }
				clearerr(filedes.fd); fseek(filedes.fd,0,SEEK_SET);
				file->crc32=crc32(0,f,file->size);
				MEM_CLEAN(f);
			}
		}
	}

	return TRUE;
}

int file_open_rw(void)
{
	if ((filedes.fd=fopen(file->filename,"r+b"))==NULL) return FALSE;
	else return TRUE;
}

int file_open_custom(FILE** fd,const char* fn)
{
	if ((*fd=fopen(fn,"rb"))==NULL) return FALSE;
	else {
		int size=0;
		fseek(*fd,0,SEEK_END); size=ftell(*fd); fseek(*fd,0,SEEK_SET);
		return size;
	}
}

int file_save(void)
{
	if ((filedes.fd=fopen(file->filename,"wb"))==NULL) return FALSE;
	else return TRUE;
}

int file_save_text(void)
{
	if ((filedes.fd=fopen(file->filename,"wt"))==NULL) return FALSE;
	else return TRUE;
}

int file_save_custom(FILE** fd,const char* fn)
{
	if ((*fd=fopen(fn,"wb"))==NULL) return FALSE;
	else return TRUE;
}

int file_save_custom_text(FILE** fd,const char* fn)
{
	if ((*fd=fopen(fn,"wt"))==NULL) return FALSE;
	else return TRUE;
}

void file_close(void)
{
	if (filedes.fd) { clearerr(filedes.fd); fclose(filedes.fd); filedes.fd=NULL; }
	if (filedes.zfd) { unzCloseCurrentFile(filedes.zfd); unzClose(filedes.zfd); filedes.zfd=NULL; }
}

void file_close_custom(FILE* fd)
{
	if (fd) { clearerr(fd); fclose(fd); }
}

int file_patch_init(void)
{
	u8 pc[5]={0};

	MEM_CREATE(patch.buffer,0x10000);
	if (!file_read(pc,5)) { LOG(LOG_MISC|LOG_WARNING,"patch read error\n"); return FALSE; }
	if (memcmp(pc,"PATCH",5)!=0) { LOG(LOG_MISC|LOG_WARNING,"bad patch header\n"); return FALSE; }
	patch.fd=filedes.fd;
	patch.zfd=filedes.zfd;
	filedes.fd=filedes.zfd=NULL;
	patch.patching=TRUE;
	patch.bytes=patch.offset=0;

	return TRUE;
}

void file_patch_close(void)
{
	MEM_CLEAN(patch.buffer);
	if (patch.fd) { clearerr(patch.fd); fclose(patch.fd); patch.fd=NULL; }
	if (patch.zfd) { unzCloseCurrentFile(patch.zfd); unzClose(patch.zfd); patch.zfd=NULL; }
	patch.patching=FALSE;
}

int file_read (u8* dest,const int size)
{
	#define FILE_READ_DES(x,d,s)						\
		c=TRUE;											\
		if (x.fd) { if (!fread(d,1,s,x.fd)) c=FALSE; }	\
		else { if (!unzReadCurrentFile(x.zfd,d,s)) c=FALSE; }

	int o,c;

	/* +IPS */
	if (patch.patching) {

		const int offset_max=patch.offset+size;
		const int offset_old=patch.offset;

		for (;patch.offset!=offset_max;) {

			/* get source data */
			if (patch.bytes) {
				o=offset_max-patch.offset;

				/* patch */
				if (patch.address<=(u32)patch.offset) {
					if (o>patch.bytes) o=patch.bytes;
					FILE_READ_DES(filedes,dest+patch.offset-offset_old,o); /* dummy read to increase file pointer. */
					if (patch.rle) {
						memset(dest+patch.offset-offset_old,patch.buffer[2],o);
						if ((u32)size>=patch.show_size) LOG(LOG_MISC,"r"); /* rle */
					}
					else {
						memcpy(dest+patch.offset-offset_old,patch.buffer+patch.buffer_offset,o);
						patch.buffer_offset+=o;
						if ((u32)size>=patch.show_size) LOG(LOG_MISC,"p"); /* patch */
					}
					patch.bytes-=o;
					patch.offset+=o;
				}

				/* normal read */
				else {
					if ((u32)o>(patch.address-(u32)patch.offset)) o=patch.address-(u32)patch.offset;
					FILE_READ_DES(filedes,dest+patch.offset-offset_old,o);
					if (!c) { /* no return on error: patched result size may be larger than source size */
						memset(dest+patch.offset-offset_old,0,o);
						if ((u32)size>=patch.show_size) LOG(LOG_MISC,"e"); /* extend */
					}
					patch.offset+=o;
				}
			}

			/* get patch data */
			else {
				memset(patch.buffer,0,5);
				FILE_READ_DES(patch,patch.buffer,5);
				if (!c) { LOG(LOG_MISC|LOG_WARNING,"patch read error at address fetch!\n"); return FALSE; }
				patch.address=patch.buffer[0]<<16|patch.buffer[1]<<8|patch.buffer[2]; /* next address/offset */
				patch.bytes=patch.buffer[3]<<8|patch.buffer[4]; /* #of bytes to patch */

				/* eof */
				if ((patch.address&0x00ffffff)==0x454f46) {
					patch.patching=FALSE;
					patch.bytes=1;
					patch.address=~0;
				}

				else {
					if (patch.address<(u32)patch.offset) { LOG(LOG_MISC|LOG_WARNING,"patch read error: recursive address!\n"); return FALSE; }

					/* 0=rle patching */
					if (patch.bytes==0) {
						patch.rle=TRUE;
						memset(patch.buffer,0,3);
						FILE_READ_DES(patch,patch.buffer,3);
						if (!c) { LOG(LOG_MISC|LOG_WARNING,"patch read error at RLE fetch!\n"); return FALSE; }
						patch.bytes=patch.buffer[0]<<8|patch.buffer[1];
						if (patch.bytes==0) { LOG(LOG_MISC|LOG_WARNING,"patch read error at RLE fetch: 0 bytes!\n"); return FALSE; }
					}

					/* normal patching */
					else {
						patch.buffer_offset=patch.rle=0;
						memset(patch.buffer,0,patch.bytes);
						FILE_READ_DES(patch,patch.buffer,patch.bytes);
						if (!c) { LOG(LOG_MISC|LOG_WARNING,"patch read error at bytes fetch!\n"); return FALSE; }
					}
				}
			}
		}
	}

	/* normal */
	else { FILE_READ_DES(filedes,dest,size); return c; }

	return TRUE;
}

int file_read_custom(FILE* fd,u8* dest,const int size)
{
	return fread(dest,1,size,fd);
}

int file_write(const u8* source,const int size)
{
	fflush(filedes.fd);
	if (!fwrite(source,1,size,filedes.fd)) return size==0;
	else return TRUE;
}

int file_write_custom(FILE* fd,const u8* source,const int size)
{
	fflush(fd);
	if (!fwrite(source,1,size,fd)) return size==0;
	else return TRUE;
}

void file_init(void)
{
	char temp[STRING_SIZE]={0};

	MEM_CREATE(file,sizeof(File));
	memset(&filedes,0,sizeof(filedes));
	memset(&patch,0,sizeof(patch));

	patch.show_size=~0;

	/* get appdir, name */
	GetModuleFileName(NULL,temp,STRING_SIZE);
	file_setfile(&file->appdir,temp,NULL,NULL);
	file->appname=file->name;
	file->name=NULL;

	/* get temp filename */
	if (!GetTempPath(STRING_SIZE,temp)) sprintf(temp,"%s\\",file->appdir);
	strcat(temp,file->appname); strcat(temp,".tmp");
	MEM_CREATE(file->temp,strlen(temp)+1); strcpy(file->temp,temp);

	LOG(LOG_VERBOSE,"file I/O initialised\n");
}

void file_directories(void)
{
	char* c;

	/* convert dot to appdir (and undefined to NULL).. no validity checks: done in setfile */
	#define FILE_GETDIR(x,y)												\
		c=NULL; SETTINGS_GET_STRING(settings_info(x),&c);					\
		if (c==NULL&&settings_notfound()) {									\
			MEM_CREATE(c,2); c[0]='.';										\
		}																	\
		if (c&&strlen(c)&&stricmp(c,"<undefined>")) {						\
			int len=strlen(c);												\
			int dot=FALSE;													\
			if (c[0]=='.') { dot=TRUE; len+=strlen(file->appdir); len--; }	\
			MEM_CREATE(y,len+1);											\
			if (dot) strcat(y,file->appdir);								\
			if (strlen(c)>(size_t)dot) strcat(y,c+dot);						\
		}																	\
		MEM_CLEAN(c)

	FILE_GETDIR(SETTINGS_BIOSDIR,file->biosdir);
	FILE_GETDIR(SETTINGS_ROMDIR,file->romdir);
	FILE_GETDIR(SETTINGS_CASDIR,file->casdir);
	FILE_GETDIR(SETTINGS_PALETTEDIR,file->palettedir);
	FILE_GETDIR(SETTINGS_PATCHDIR,file->patchdir);
	FILE_GETDIR(SETTINGS_SAMPLEDIR,file->sampledir);
	FILE_GETDIR(SETTINGS_SHOTDIR,file->shotdir);
	FILE_GETDIR(SETTINGS_SRAMDIR,file->batterydir);
	FILE_GETDIR(SETTINGS_STATEDIR,file->statedir);
	FILE_GETDIR(SETTINGS_TOOLDIR,file->tooldir);
}

void file_settings_save(void)
{
	char c[STRING_SIZE]={0};

	SETTINGS_PUT_STRING(NULL,"; *** File settings ***\n\n");
	SETTINGS_PUT_STRING(settings_info(SETTINGS_BIOS),mapper_get_bios_name());
	settings_put_yesnoauto(SETTINGS_AUTODETECTROM,mapper_get_autodetect_rom());
	settings_put_yesnoauto(SETTINGS_CRCLOADSTATEIGNORE,msx_get_ignore_loadstate_crc());

	SETTINGS_PUT_STRING(NULL,"\n");
	SETTINGS_PUT_INT(settings_info(SETTINGS_FILTERINDEX_BIOS),media_get_filterindex_bios());
	SETTINGS_PUT_INT(settings_info(SETTINGS_FILTERINDEX_CART),media_get_filterindex_cart());
	SETTINGS_PUT_INT(settings_info(SETTINGS_FILTERINDEX_TAPE),media_get_filterindex_tape());
	SETTINGS_PUT_INT(settings_info(SETTINGS_FILTERINDEX_STATE),state_get_filterindex());
	SETTINGS_PUT_INT(settings_info(SETTINGS_FILTERINDEX_MOVIE),movie_get_filterindex());
	SETTINGS_PUT_INT(settings_info(SETTINGS_FILTERINDEX_PALETTE),draw_get_filterindex_palette());
	SETTINGS_PUT_INT(settings_info(SETTINGS_FILTERINDEX_ZOOMPAT),tool_get_pattern_fi_shared());
	SETTINGS_PUT_INT(settings_info(SETTINGS_FILTERINDEX_OPENTILES),tileview_get_open_fi());
	SETTINGS_PUT_INT(settings_info(SETTINGS_FILTERINDEX_SAVETILES),tileview_get_save_fi());
	SETTINGS_PUT_INT(settings_info(SETTINGS_FILTERINDEX_OPENSPRT),spriteview_get_open_fi());
	SETTINGS_PUT_INT(settings_info(SETTINGS_FILTERINDEX_SAVESPRT),spriteview_get_save_fi());
	SETTINGS_PUT_INT(settings_info(SETTINGS_FILTERINDEX_OPENCWAVE),psgtoy_get_open_fi());
	SETTINGS_PUT_INT(settings_info(SETTINGS_FILTERINDEX_SAVECWAVE),psgtoy_get_save_fi());
	SETTINGS_PUT_INT(settings_info(SETTINGS_FILTERINDEX_PSGLOG),psglogger_get_fi());

	SETTINGS_PUT_STRING(NULL,"\n; Use \".\\path\" to specify a relative directory path.\n\n");

	/* convert appdir to dot, and NULL to undefined */
	#define FILE_PUTDIR(x,y)											        \
		if (y&&strlen(y)) {												        \
			int off=0;													        \
			memset(c,0,STRING_SIZE);									        \
			if (strlen(y)>=strlen(file->appdir)) {						        \
				memcpy(c,y,strlen(file->appdir));						        \
				if ((stricmp(c,file->appdir)==0)&&                              \
                   ((strlen(y)==strlen(file->appdir))||                         \
                   ((y[strlen(file->appdir)]=='\\')|                            \
                   (y[strlen(file->appdir)]=='/'))))                            \
                        off=strlen(file->appdir);                               \
			}															        \
			memset(c,0,STRING_SIZE);									        \
			c[0]='.';													        \
			if ((size_t)off<strlen(y)) memcpy(c+(off!=0),y+off,strlen(y)-off);	\
			SETTINGS_PUT_STRING(settings_info(x),c);					        \
		}																        \
		else SETTINGS_PUT_STRING(settings_info(x),"<undefined>")

	FILE_PUTDIR(SETTINGS_ROMDIR,file->romdir);
	FILE_PUTDIR(SETTINGS_CASDIR,file->casdir);
	FILE_PUTDIR(SETTINGS_BIOSDIR,file->biosdir);
	FILE_PUTDIR(SETTINGS_PALETTEDIR,file->palettedir);
	FILE_PUTDIR(SETTINGS_PATCHDIR,file->patchdir);
	FILE_PUTDIR(SETTINGS_SAMPLEDIR,file->sampledir);
	FILE_PUTDIR(SETTINGS_SHOTDIR,file->shotdir);
	FILE_PUTDIR(SETTINGS_SRAMDIR,file->batterydir);
	FILE_PUTDIR(SETTINGS_STATEDIR,file->statedir);
	FILE_PUTDIR(SETTINGS_TOOLDIR,file->tooldir);
}

void file_clean(void)
{
	file_close();
	MEM_CLEAN(file->dir); MEM_CLEAN(file->ext); MEM_CLEAN(file->name); MEM_CLEAN(file->filename);
	MEM_CLEAN(file->filename_in_zip_pre); MEM_CLEAN(file->filename_in_zip_post); MEM_CLEAN(file->appdir); MEM_CLEAN(file->appname); MEM_CLEAN(file->temp);
	MEM_CLEAN(file->tooldir); MEM_CLEAN(file->statedir); MEM_CLEAN(file->batterydir); MEM_CLEAN(file->patchdir); MEM_CLEAN(file->palettedir); MEM_CLEAN(file->romdir); MEM_CLEAN(file->casdir); MEM_CLEAN(file->biosdir); MEM_CLEAN(file->sampledir); MEM_CLEAN(file->shotdir);
	MEM_CLEAN(file);

	LOG(LOG_VERBOSE,"file I/O cleaned\n");
}

void file_setfile(char** filedir,const char* filename,const char* fileext,const char* zipextension)
{
	if (filename&&strlen(filename)) {
		int len=strlen(filename);
		if (filedir&&*filedir) len+=strlen(*filedir)+1;
		if (fileext) len+=strlen(fileext)+1;

		MEM_CLEAN(file->dir); MEM_CLEAN(file->ext); MEM_CLEAN(file->name); MEM_CLEAN(file->filename);
		if (len>=STRING_SIZE) { LOG(LOG_MISC|LOG_ERROR,"Filename too long!\n"); exit(1); }
		else {
			char temp[STRING_SIZE];
			int off,end,ed,d=0;

			memset(temp,0,STRING_SIZE);

			if (filedir&&*filedir) {
				strcat(temp,*filedir);
				/* in case dir has a \ at the end */
				if (strlen(temp)&&(temp[strlen(temp)-1]!='\\')) strcat(temp,"\\");
				else len--;
			}
			strcat(temp,filename);
			if (fileext) { strcat(temp,"."); strcat(temp,fileext); }

			off=len;
			while (off--) {
				switch (temp[off]) {
					/* remove quotes and other garbage */
					case '?': case '*': case '>': case '<': case '|': case '\"': case '\t':
						temp[off]=' '; break;

					/* change slash to backslash */
					case '/': temp[off]='\\'; break;

					default: break;
				}
			}

			/* find start and end */
			for (off=0;off<len;off++) if (temp[off]!=' ') break;
			if (off==len) return; /* all garbage */
			end=len; while (end--) if (temp[end]!=' ') break;
			len=end-off; len++;

			/* 'create' dir " [c:\dir]\name.ext " */
			#define FILEDIR_WARN(x) LOG(LOG_MISC|LOG_WARNING,"using default %s path\n",x)

			#define FILEDIR_APP_B()                                                 \
			    if      (filedir==&file->batterydir) FILEDIR_WARN("sram");	        \
				else if (filedir==&file->tooldir)    FILEDIR_WARN("tool I/O");		\
				else if (filedir==&file->statedir)   FILEDIR_WARN("states");		\
				else if (filedir==&file->patchdir)   FILEDIR_WARN("patches");		\
				else if (filedir==&file->sampledir)  FILEDIR_WARN("samples");		\
				else if (filedir==&file->shotdir)    FILEDIR_WARN("screenshots");	\
				else if (filedir==&file->palettedir) FILEDIR_WARN("palette");		\
				else if (filedir==&file->biosdir)    FILEDIR_WARN("bios");			\
				else if (filedir==&file->romdir)     FILEDIR_WARN("cartridges");	\
				else if (filedir==&file->casdir)     FILEDIR_WARN("tape");			\
				/* set application directory to that directory */					\
				MEM_CLEAN(*filedir); MEM_CREATE(*filedir,strlen(file->appdir)+1);	\
				memcpy(*filedir,file->appdir,strlen(file->appdir))

			#define FILEDIR_APP_E()                                                     \
                if (SetCurrentDirectory(file->appdir)==FALSE) {                         \
                    LOG(LOG_MISC|LOG_ERROR,"Can't locate application directory!\n");    \
                    exit(1);                                                            \
                }                                                                       \
				MEM_CREATE(file->dir,strlen(*filedir)+1);								\
				memcpy(file->dir,*filedir,strlen(*filedir))

			ed=end+1;
			while (ed--) if (temp[ed]=='\\') { d=TRUE; break; }
			if (d&(ed>off)) {
				len=ed-off;
				if (filedir) {
					/* copy to specific directory */
					MEM_CLEAN(*filedir); MEM_CREATE(*filedir,len+1);
					memcpy(*filedir,&temp[off],len);
					if ((SetCurrentDirectory(*filedir)==FALSE)&&(filedir!=&file->appdir)) { FILEDIR_APP_B(); }
					FILEDIR_APP_E();
				}
				else { MEM_CREATE(file->dir,len+1); memcpy(file->dir,&temp[off],len); }
				ed++;
			}
			else {
				/* dir not found: use app dir */
				if (filedir) { FILEDIR_APP_B(); FILEDIR_APP_E(); }
				else { MEM_CREATE(file->dir,strlen(file->appdir)+1); memcpy(file->dir,file->appdir,strlen(file->appdir)); }
				ed=off;
			}

			if (ed<=end) {
				for (off=end+1;off>ed;off--) if (temp[off]=='.') break;
				if (off==ed) off=end+1; /* .x not found */
				else if (end>off) {
					/* create ext " c:\dir\name.[ext] " */
					off++; len=end-off; len++;
					MEM_CREATE(file->ext,len+1);
					memcpy(file->ext,&temp[off],len);
					off--;
				}
				/* create name " c:\dir\[name].ext " */
				len=off-ed;
				MEM_CREATE(file->name,len+1);
				memcpy(file->name,&temp[ed],len);
			}

			/* create full path+filename " [c:\dir\name.ext] " */
			memset(temp,0,STRING_SIZE);
			if (file->dir) { strcpy(temp,file->dir); strcat(temp,"\\"); }
			if (file->name) { strcat(temp,file->name); }
			if (file->ext) { strcat(temp,"."); strcat(temp,file->ext); }
			MEM_CREATE(file->filename,strlen(temp)+1);
			memcpy(file->filename,temp,strlen(temp));
		}
	}
	else LOG(LOG_MISC|LOG_WARNING,"no file given\n");

	memset(filedes.zipext,0,16);

	if (zipextension) {
		if (strlen(zipextension)<15) sprintf(filedes.zipext,".%s",zipextension);
		else { LOG(LOG_MISC|LOG_ERROR,"ZIP extension too long!\n"); exit(1); }
	}
}

int file_setdirectory(char** dest)
{
	if (file->dir) {
		int len=strlen(file->dir);
		if (len) {
			MEM_CLEAN(*dest);
			MEM_CREATE(*dest,len+1);
			memcpy(*dest,file->dir,len);
			return TRUE;
		}
	}
	return FALSE;
}

u8* file_from_resource(int id,int* size)
{
	HRSRC r=NULL;
	filedes.g=NULL;

	if ((r=FindResource(MAIN->module,MAKEINTRESOURCE(id),RT_RCDATA))==NULL) { return NULL; }
	if ((filedes.g=LoadResource(MAIN->module,r))==NULL) { return NULL; }

	if (size!=NULL) *size=SizeofResource(MAIN->module,r);
	return LockResource(filedes.g);
}

void file_from_resource_close(void)
{
	if (filedes.g) { FreeResource(filedes.g); filedes.g=NULL; }
}


/* directories dialog */
static int CALLBACK BrowseCallbackProc(HWND hwnd,UINT uMsg,LPARAM lParam,LPARAM lpData)
{
    lParam = lParam;
	switch (uMsg) {
		case BFFM_INITIALIZED:
			SendMessage(hwnd,BFFM_SETSELECTION,TRUE,lpData);
			break;

		default: break;
	}

	return 0;
}

static void file_browse_directory(HWND dialog,UINT edit_id)
{
	BROWSEINFO bi;
	LPITEMIDLIST list;
	// char c[0x100];
	char c[0x200]; // Warning -Wformat-overvlow
	char ct[0x100];
	static char initdir[STRING_SIZE];
	memset(initdir,0,STRING_SIZE);

	switch (edit_id) {
		case IDC_DIR_TOOLEDIT: sprintf(ct,"Tool I/O"); break;
		case IDC_DIR_PATCHESEDIT: sprintf(ct,"Patches"); break;
		case IDC_DIR_SAMPLESEDIT: sprintf(ct,"Samples"); break;
		case IDC_DIR_SCREENSHOTSEDIT: sprintf(ct,"Screenshots"); break;
		case IDC_DIR_SRAMEDIT: sprintf(ct,"SRAM"); break;
		case IDC_DIR_STATESEDIT: sprintf(ct,"States"); break;

		default: sprintf(ct,"?"); break;
	}
	sprintf(
        c,
        "Select %s directory:",
        ct
    );

	memset(&bi,0,sizeof(BROWSEINFO));
	bi.lpszTitle = c;
	bi.hwndOwner = dialog;
	bi.ulFlags = BIF_RETURNONLYFSDIRS;
	bi.lpfn = BrowseCallbackProc;

	/* set init directory */
	if (!GetDlgItemText(dialog,edit_id,initdir,STRING_SIZE)||!strlen(initdir)||!SetCurrentDirectory(initdir)) {
		strcpy(initdir,file->appdir);
		SetCurrentDirectory(file->appdir);
	}
	bi.lParam=(LPARAM)initdir;


	if ((list=SHBrowseForFolder(&bi))) {
		char dir[STRING_SIZE]={0};
		IMalloc* imalloc;

		if (SHGetPathFromIDList(list,dir)&&strlen(dir)) SetDlgItemText(dialog,edit_id,dir);

		if (SHGetMalloc(&imalloc)==S_OK) {
			IMalloc_Free(imalloc,list);
			IMalloc_Release(imalloc);
		}
	}
}

INT_PTR CALLBACK file_directories_dialog( HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam )
{
    lParam = lParam;
	switch (msg) {

		case WM_INITDIALOG:

			if (file->tooldir) SetDlgItemText(dialog,IDC_DIR_TOOLEDIT,file->tooldir);
			if (file->patchdir) SetDlgItemText(dialog,IDC_DIR_PATCHESEDIT,file->patchdir);
			if (file->sampledir) SetDlgItemText(dialog,IDC_DIR_SAMPLESEDIT,file->sampledir);
			if (file->shotdir) SetDlgItemText(dialog,IDC_DIR_SCREENSHOTSEDIT,file->shotdir);
			if (file->batterydir) SetDlgItemText(dialog,IDC_DIR_SRAMEDIT,file->batterydir);
			if (file->statedir) SetDlgItemText(dialog,IDC_DIR_STATESEDIT,file->statedir);

			main_parent_window(dialog,0,0,0,0,0);
			return 1;
			break;

		case WM_COMMAND:

			switch (LOWORD(wParam)) {

				/* browse */
				#define DIR_BROWSE(x) file_browse_directory(dialog,x); PostMessage(dialog,WM_NEXTDLGCTL,(WPARAM)GetDlgItem(dialog,IDOK),TRUE)
				case IDC_DIR_TOOLBROWSE: DIR_BROWSE(IDC_DIR_TOOLEDIT); break;
				case IDC_DIR_PATCHESBROWSE: DIR_BROWSE(IDC_DIR_PATCHESEDIT); break;
				case IDC_DIR_SAMPLESBROWSE: DIR_BROWSE(IDC_DIR_SAMPLESEDIT); break;
				case IDC_DIR_SCREENSHOTSBROWSE: DIR_BROWSE(IDC_DIR_SCREENSHOTSEDIT); break;
				case IDC_DIR_SRAMBROWSE: DIR_BROWSE(IDC_DIR_SRAMEDIT); break;
				case IDC_DIR_STATESBROWSE: DIR_BROWSE(IDC_DIR_STATESEDIT); break;

				/* set default */
				#define DIR_DEFAULT(x) SetDlgItemText(dialog,x,file->appdir); PostMessage(dialog,WM_NEXTDLGCTL,(WPARAM)GetDlgItem(dialog,IDOK),TRUE)
				case IDC_DIR_TOOLDEFAULT: DIR_DEFAULT(IDC_DIR_TOOLEDIT); break;
				case IDC_DIR_PATCHESDEFAULT: DIR_DEFAULT(IDC_DIR_PATCHESEDIT); break;
				case IDC_DIR_SAMPLESDEFAULT: DIR_DEFAULT(IDC_DIR_SAMPLESEDIT); break;
				case IDC_DIR_SCREENSHOTSDEFAULT: DIR_DEFAULT(IDC_DIR_SCREENSHOTSEDIT); break;
				case IDC_DIR_SRAMDEFAULT: DIR_DEFAULT(IDC_DIR_SRAMEDIT); break;
				case IDC_DIR_STATESDEFAULT: DIR_DEFAULT(IDC_DIR_STATESEDIT); break;

				/* close dialog */
				case IDCANCEL:
					EndDialog(dialog,0);
					break;

				case IDOK: {
					char c[STRING_SIZE];

					#define DIR_OK(e,d)												\
						memset(c,0,STRING_SIZE);									\
						if (GetDlgItemText(dialog,e,c,STRING_SIZE)&&strlen(c)) {	\
							MEM_CLEAN(d); MEM_CREATE(d,strlen(c)+1);				\
							strcpy(d,c);											\
						}

					msx_wait();
					DIR_OK(IDC_DIR_TOOLEDIT,file->tooldir)
					DIR_OK(IDC_DIR_PATCHESEDIT,file->patchdir)
					DIR_OK(IDC_DIR_SAMPLESEDIT,file->sampledir)
					DIR_OK(IDC_DIR_SCREENSHOTSEDIT,file->shotdir)
					DIR_OK(IDC_DIR_SRAMEDIT,file->batterydir)
					DIR_OK(IDC_DIR_STATESEDIT,file->statedir)
					msx_wait_done();

					EndDialog(dialog,0);
					break;
				}

				default: break;
			}
			break;

		default: break;
	}

	return 0;
}
