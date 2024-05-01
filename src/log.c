/******************************************************************************
 *                                                                            *
 *                                 "log.c"                                    *
 *                                                                            *
 ******************************************************************************/

#include <stdarg.h>

#include "global.h"
#include "crystal.h"
#include "draw.h"
#include "main.h"

static struct {
	u32 frame_start;
	int enabled;
} log_properties;

void log_init(void)
{
	/* can't LOG */
	memset(&log_properties,0,sizeof(log_properties));

	log_enable();
	log_set_frame_start(0);
}

void log_set_frame_start(u32 f) { log_properties.frame_start=f; }
void log_enable(void) { log_properties.enabled=TRUE; }
void log_disable(void) { log_properties.enabled=FALSE; }

/* log a string, quite like printf */
void __cdecl LOG(int type,const char* text,...)
{
	int i,j,k,l,m,n,o,p,max;
	int chop=~type&LOG_NOCHOP;
	const char cut[]= { ' ', ',', '?', ':', ';', '/', '\\', '\0' };
	const char add[]= { '\n', ' ', '\0' };
	const char skip[]={ ' ', '\t', '\0' };
	char buffer[STRING_SIZE<<1];
	char temp[STRING_SIZE];
	va_list ap;

	type&=0x3fffffff;

	if (!(type&LOG_ERROR)&&(!log_properties.enabled||crystal->fc<log_properties.frame_start)) return;

	va_start(ap,text);
	vsprintf(buffer,text,ap);
	va_end(ap);

	if (LOG_DOS_BOX) max=STRING_SIZE-1;
	else max=LOG_MAX_WIDTH;

	if ((size_t)chop&&strlen(buffer)>(size_t)max) {
		memcpy(temp,buffer,STRING_SIZE);

		/* chop the string up at correct places: [cut], at cuts add: [add](newline+space), but skip: [skip] ... uh yeah, like this: */
		for(i=1,l=m=0;;i++) {
start:
			for (j=(i*max)-max-l;j<(i*max)-l;j++) {
				buffer[j+m]=temp[j];
				if (temp[j]=='\n') { l--; goto start; }
				if (temp[j]=='\0') goto end;
			}
			for (j=(i*max)-1-l;j>(i*max)-max-l;j--)
				for (k=0;cut[k]!='\0';k++)
					if (temp[j]==cut[k]) {
						for (n=0;add[n]!='\0';n++) buffer[j+m+n+1]=add[n];
						for (m+=n,n=o=p=0;skip[n]!='\0';o++)
							for (n=0;skip[n]!='\0';n++)
								if (temp[o+j+1]==skip[n]) { p++; m--; break; }
						l=l+((i++*max)-1-l-j)-p;
						goto start;
					}
			for (n=0;add[n]!='\0';n++) buffer[j+m+n+max]=add[n];
			for (m+=n,n=o=0;skip[n]!='\0';o++)
				for (n=0;skip[n]!='\0';n++)
					if (temp[o+j+1]==skip[n]) { l--; m--; break; }
		}
	}
end:
	buffer[STRING_SIZE-1]='\0';

	#define L_PRINTF(x) if (LOG_DOS_BOX) printf(x)

	switch (type&0xf) {
		case LOG_MISC:
			if ((DEBUG_MISC)|(type&(LOG_WARNING|LOG_ERROR))) {
				if ((type&LOG_ERROR)==0) draw_text_add(type>>16,((type>>8)|(type&(LOG_WARNING)))&0x1f,buffer);
				L_PRINTF(buffer);
			}
			break;

		case LOG_VERBOSE: if (DEBUG_VERBOSE) L_PRINTF(buffer); break;
		case LOG_PROFILER: if (DEBUG_PROFILER) L_PRINTF(buffer); break;
		default: if (log_properties.frame_start<=crystal->fc) L_PRINTF(buffer); break;
	}

	fflush(stdout);

	if (type&LOG_ERROR) {
		HWND wnd=MAIN->window;
		if (wnd==NULL) {
			L_PRINTF("I am Error.");
			fflush(stdin);
			if (LOG_DOS_BOX) system("pause");
			fflush(stdout);
		}

		LOG_ERROR_WINDOW(wnd,buffer);
	}
}

/* log a binary number, n=newline */
void LOG_BIN(int logtype,u32 number,int n)
{
	int i=8;
	if (number&0xff00) i=16;
	if (number&0xffff0000) i=32;
	while (i--) LOG(logtype,"%d",number>>i&1);
	if (n) LOG(logtype,"\n");
}
