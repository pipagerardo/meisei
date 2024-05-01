/******************************************************************************
 *                                                                            *
 *                              "settings.c"                                  *
 *                                                                            *
 ******************************************************************************/

#include <math.h>
#include <float.h>
#include "global.h"
#include "file.h"
#include "settings.h"
#include "version.h"
#include "draw.h"
#include "input.h"
#include "sound.h"
#include "tool.h"

#define SETTINGS_ALIGN 25

static const char* settings_info_s[SETTINGS_MAX]={
	"enable vsync",
	"renderer",
	"mirror mode",
	"bilinear filtering",
	"software rendering",
	"show messages",
	"refresh at 50Hz/60Hz",
	"upper border",		"lower border",		"left border",		"right border",
	"correct aspect ratio",
	"stretch to fit",
	"window size",
	"start in full screen",
	"VDP chip type",
	"video signal",
	"palette  ",		/* spaces to prevent conflict with "palette path" */
	"decoder",
	"R-Y angle",		"G-Y angle",		"B-Y angle",
	"R-Y gain",			"G-Y gain",			"B-Y gain",
	"hue shift angle",
	"saturation",
	"contrast",
	"brightness",
	"sharpness",
	"gamma correction",
	"luma bandwidth",
	"chroma artifacts",
	"luma artifacts",
	"colour bleed",
	"auto save palette",
	"frameskip",
	"border overlay colour",
	"show movie time code",
	"show tile layer",
	"show sprite layer",
	"unlimited sprites",

	"enable sound",
	"sleep while waiting",
	"frames in buffer",
	"enable lumi noise",
	"lumi noise volume",
	"SCC volume",
	"priority",
	"Z80 clock percentage",
	"speed up percentage",
	"slowdown percentage",
	"reverse buffer length",
	"ignore reverse limit",
	"PSG chip type",
	"RAM size",
	"RAM primary slot",

	"trim PSG log silence",
	"save YM log interleaved",
	"offset cents",
	"enable channel waveforms",
	"channel waveforms amp",
	"channel A waveform",
	"channel B waveform",
	"channel C waveform",
	"tile viewer map source",
	"zoom edit to all blocks",
	"import tiles NT",
	"import tiles PGT",
	"import tiles CT",
	"load simulate other mode",
	"save tiles BLOAD header",
	"mimic masks on save",

	"update frequency",
	"enable in background",
	"enable keyboard netplay",
	"enable joystick",
	"analog stick sensitivity",
	"mouse rotary sensitivity",
	"mouse joystick emulation",
	"Arkanoid dial minimum",
	"Arkanoid dial maximum",
	"port 1",
	"port 2",
	"keyboard region",

	"bios filename",
	"autodetect cartridges",
	"overrule loadstate crc32",
	"open bios filetype",
	"open cart filetype",
	"open tape filetype",
	"open state filetype",
	"open movie filetype",
	"open palette filetype",
	"open tiles filetype",
	"save tiles filetype",
	"open sprites filetype",
	"save sprites filetype",
	"zoom pattern filetype",
	"open PSG wave filetype",
	"save PSG wave filetype",
	"log PSG filetype",
	"cartridges path",
	"tape path",
	"bios path",
	"tool I/O path",
	"palette path",
	"patches path",
	"samples path",
	"screenshots path",
	"sram path",
	"states path"
};

static struct {
	char* file;
	int runtimes;
	int warn;
	int notfound;
} settings;

void settings_disable_warning(void) { settings.warn=FALSE; }
int settings_notfound(void) { return settings.notfound; }
int settings_get_runtimes(void) { return settings.runtimes; }

void settings_init(void)
{
	int i;

	memset(&settings,0,sizeof(settings));
	settings.runtimes=settings.warn=1;

	file_setfile(&file->appdir,file->appname,"ini",NULL);

	/* open [appdir]\[appname].ini */
	if (!file_open()||((file->size<10)|(file->size>100000))) {
		settings.notfound=TRUE;
		LOG(
            LOG_MISC | LOG_WARNING,
        #ifdef MEISEI_ESP
            "no se pudo abrir la configuración\n"
        #else
            "couldn't open settings\n"
        #endif // MEISEI_ESP
        );
		file_close();
		return;
	}

	/* copy whole file, instead of slowly reading from file when getting a setting */
	MEM_CREATE(settings.file,file->size+1);
	file_read((u8*)settings.file,file->size);
	file_close();

	for (i=0;(u32)i<file->size;i++) {
		/* replace nulls, linefeeds, tabs, double quotes */
		if ((settings.file[i]=='\0')|(settings.file[i]=='\r')|(settings.file[i]=='\t')|(settings.file[i]=='\"')) { settings.file[i]=' '; }
	}

	/* get own settings */
	settings.runtimes=0;
	SETTINGS_GET_INT("; and is very proud of it.",&settings.runtimes);
	if (settings.runtimes<0) settings.runtimes=0;
	settings.runtimes++;
}

void settings_save(void)
{
	char c[STRING_SIZE];

	file_setfile(&file->appdir,file->appname,"ini",NULL);

	if (!file_save_text()) {
        LOG(
            LOG_MISC|LOG_WARNING,
        #ifdef MEISEI_ESP
            "no se pudo guardar la configuración\n"
        #else
            "couldn't save settings\n"
        #endif // MEISEI_ESP
        );
        file_close();
        return;
    }

	sprintf(c,"; %s %s automatically generated this configuration file,\n; and is very proud of it. %d time%s already and still not fed up?\n",version_get(VERSION_NAME),version_get(VERSION_NUMBER),settings.runtimes,settings.runtimes==1?"":"s");
	if (SETTINGS_PUT_STRING(NULL,c)<0) {
        LOG(
            LOG_MISC|LOG_WARNING,
        #ifdef MEISEI_ESP
            "no se pudo escribir la configuración\n"
        #else
            "couldn't write settings\n"
        #endif
        );
        file_close();
        return;
    }
	/* no further write validity checks, assuming that if it goes wrong, it will right at the start */

	SETTINGS_PUT_STRING(NULL,"\n\n");	draw_settings_save();
	SETTINGS_PUT_STRING(NULL,"\n\n");	sound_settings_save();
	SETTINGS_PUT_STRING(NULL,"\n\n");	tool_settings_save();
	SETTINGS_PUT_STRING(NULL,"\n\n");	file_settings_save();
	SETTINGS_PUT_STRING(NULL,"\n\n");	input_settings_save();

	file_close();
}


void settings_clean(void)
{
	MEM_CLEAN(settings.file);
}

const char* settings_info(u32 i)
{
	if (i>=SETTINGS_MAX) return NULL;

	return settings_info_s[i];
}

int settings_get(const char* search,char** cg,int* ig,float* fg)
{
	#define S_CLOSE(x) MEM_CLEAN(val); MEM_CLEAN(comp); return x

	int ll,sl,off,len,fp=0,e=FALSE;
	char line[1+(STRING_SIZE>>1)];
	char *val=NULL,*comp=NULL;
	int w=settings.warn;

	settings.warn=TRUE;

	if (((cg==NULL)&(ig==NULL)&(fg==NULL))|(settings.file==NULL)|(search==NULL)) return FALSE;
	sl=strlen(search);
	if (sl==0) return FALSE;

	MEM_CREATE(comp,sl+1);

	while (!e) {

		/* read line */
		for (ll=0;ll<(STRING_SIZE>>1);ll++) {
			line[ll]=settings.file[fp++];
			e=line[ll]=='\0'; /* eo'f' */
			if (e|(line[ll]=='\n')) { line[ll]=' '; break; }
		}

		/* valid line? */
		if (ll>sl) {
			for (off=0;off<ll;off++) {
				if (line[off]!=' ') {

					memset(comp,0,sl+1);
					memcpy(comp,&line[off],sl);
					if (stricmp(comp,search)!=0) break;
					else {

						/* found */
						len=ll-(off+sl);
						MEM_CREATE(val,len+1);
						memcpy(val,&line[off+sl],len);

						/* get int */
						if (ig) {
							int in,inv;

							in=0; sscanf(val,"%d",&in); inv=in==0;
							in=1; sscanf(val,"%d",&in); inv&=(in==1);

							if (inv) {
								if (w) LOG(LOG_MISC|LOG_WARNING,"[%s] set value invalid\n",search);
								S_CLOSE(FALSE);
							}
							else { *ig=in; S_CLOSE(TRUE); }
						}

						/* get float */
						else if (fg) {
							float in; int inv;

							in=0.0; sscanf(val,"%f",&in);
							// inv=in==0.0;
                            inv = ( fabs(in - 0.0f) < FLT_EPSILON ) ? TRUE : FALSE;
							in=1.0; sscanf(val,"%f",&in);
							// inv&=(in==1.0);
                            inv&= (( fabs(in - 1.0f) < FLT_EPSILON ) ? TRUE : FALSE );

							if (inv) {
								if (w) LOG(LOG_MISC|LOG_WARNING,"[%s] set value invalid\n",search);
								S_CLOSE(FALSE);
							}
							else { *fg=in; S_CLOSE(TRUE); }
						}

						/* get string */
						else {
							for (off=0;off<len;off++) if (val[off]!=' ') break;

							if (off==len) {
								if (w) LOG(LOG_MISC|LOG_WARNING,"[%s] set text invalid\n",search);
								S_CLOSE(FALSE);
							}
							else {
								len-=off;
								memmove(val,val+off,len);
								for (off=len-1;off>0;off--) if (val[off]!=' ') { break; }
								MEM_CREATE(*cg,off+2); /* gotta clean this elsewhere */
								memcpy(*cg,val,off+1);
								S_CLOSE(TRUE);
							}
						}
					}
				}
			}
		}
	}

	if (w) LOG(LOG_MISC|LOG_WARNING,"[%s] setting not found\n",search);
	S_CLOSE(FALSE);
}

int settings_put(const int type,const char* set,const char* c,const int i,const float f)
{
	FILE* fd=file_get_fd();
	int al=SETTINGS_ALIGN,ret=0,n=FALSE;
	char* s=NULL;

	fflush(fd);

	if (set&&(strlen(set)>0)) {
		n=TRUE;

		if (strlen(set)>=(size_t)al) al=strlen(set)+1;

		MEM_CREATE(s,al+1);
		memset(s,' ',al);
		memcpy(s,set,strlen(set));

		ret|=fprintf(fd,"%s",s);
		fflush(fd);
	}

	switch (type) {
		case SETTINGS_TYPE_STRING: ret=fprintf(fd,"%s",c); break;
		case SETTINGS_TYPE_INT: ret=fprintf(fd,"%d",i); break;
		case SETTINGS_TYPE_FLOAT: ret=fprintf(fd,"%f",f); break;

		default: break;
	}

	fflush(fd);
	if (n) ret|=fprintf(fd,"\n");

	MEM_CLEAN(s);
	return ret;
}

int settings_get_yesnoauto(int id)
{
	const char* yn[]={"no","yes","auto"};
	const char* oo[]={"off","on","auto"};
	char* c=NULL;
	int i=-1;

	SETTINGS_GET_STRING(settings_info(id),&c);
	if (c&&strlen(c)) {
		for (i=0;i<3;i++) if (stricmp(c,yn[i])==0||stricmp(c,oo[i])==0) break;
		if (i==3) i=-1;
	}
	MEM_CLEAN(c);

	return i;
}

void settings_put_yesnoauto(int id,int v)
{
	const char* yn[]={"no","yes","auto"};

	if (v<0||v>2) v=0;
	SETTINGS_PUT_STRING(settings_info(id),yn[v]);
}
