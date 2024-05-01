/******************************************************************************
 *                                                                            *
 *                                "update.c"                                  *
 *                                                                            *
 ******************************************************************************/

#include <windows.h>
#include <wininet.h>

#include "global.h"
#include "file.h"
#include "main.h"
#include "version.h"
#include "resource.h"
#include "zlib.h"

#ifdef MEISEI_UPDATE

#define UPDATE_SIZE	0x2000  // <- OVERFLOW

#define UPDATE_FILE	"http://home.kpn.nl/koele844/meisei_update_v1.txt"
/* versioning in case the format gets updated:
v1: initial, support for single files, local zip decompression possible, encrypted for obvious reasons
*/

#define USER_AGENT	"Mozilla/5.0 (Windows; U; Windows NT 5.1; en-GB; rv:1.9.2) Gecko/20100115 Firefox/3.6 (.NET CLR 3.5.30729)"

#define U_ID_EOF	0
#define U_ID_NORMAL	1
#define U_ID_ZIP	2

#define UPDATE_OK	0
#define UPDATE_FAIL	1
#define UPDATE_HAVE	2
#define UPDATE_EOF	3

static const char* um_msg[]={ "OK", "FAIL", "HAVE" };

static int dialog_closed=TRUE;
static HWND dialog_handle=NULL;
static HANDLE thread_handle=NULL;

typedef struct update_file {
	int size;
	char name[0x100];
	u8* data;
	struct update_file* next;
} update_file;
static update_file* update_file_begin=NULL;


static void clean_file_ll(void)
{
	update_file* uf;
	update_file* ufo;

	/* clean linked list */
	for (uf=update_file_begin;uf;) {
		ufo=uf;
		MEM_CLEAN(uf->data);
		uf=uf->next;
		MEM_CLEAN(ufo);
	}

	update_file_begin=NULL;
}

static void update_view(char* view,const char* t)
{
	if (dialog_closed) return;

	strcat(view,t);
	SetDlgItemText(dialog_handle,IDC_UPDATE_VIEW,view);
	SendDlgItemMessage(dialog_handle,IDC_UPDATE_VIEW,WM_VSCROLL,SB_BOTTOM,0);
}

static int get_line(const char* in,char* out)
{
	int i,o;

	for (i=0;in[i]==' '||in[i]=='\t'||in[i]=='\n'||in[i]=='\r';i++) { ; }
	for (o=0;in[i]!=' '&&in[i]!='\t'&&in[i]!='\n'&&in[i]!='\r'&&in[i]!='\0';o++,i++) out[o]=in[i];
	out[o]=0;

	return i;
}

static int check_valid(HINTERNET internet,char* update)
{
	char line[UPDATE_SIZE];
	u8 c[UPDATE_SIZE];
	HINTERNET hurl;
	DWORD br;
	uLongf d=UPDATE_SIZE-1;
	u32 crc;
	int i,j,k,o=19,s,v;

	memset(update,0,UPDATE_SIZE);

	/* download file */
	strcpy(line,UPDATE_FILE);
	if (!(hurl=InternetOpenUrl(internet,line,NULL,0,INTERNET_FLAG_PRAGMA_NOCACHE|INTERNET_FLAG_RELOAD,0))) return FALSE;

	if (!InternetReadFile(hurl,update,UPDATE_SIZE-2,&br)||update[8]!=' '||update[14]!=' '||update[18]!=' ') {
		InternetCloseHandle(hurl);
		return FALSE;
	}
	InternetCloseHandle(hurl);
	update[UPDATE_SIZE-1]=0;

	/* get checksum */
	if (get_line(update,line)!=8) return FALSE;
	crc=0; sscanf(line,"%08X",&crc); j=crc==0;
	crc=1; sscanf(line,"%08X",&crc); j&=(crc==1);
	if (j) return FALSE;

	/* get size */
	if (get_line(update+9,line)!=5) return FALSE;
	i=0; sscanf(line,"%d",&i); j=i==0;
	i=1; sscanf(line,"%d",&i); j&=(i==1);
	if (j||i<=0||i>(UPDATE_SIZE-21)) return FALSE;

	/* validate checksum */
	if (crc!=(u32)crc32(0,(u8*)(update+19),i)) return FALSE;

	/* get meisei version */
	if (get_line(update+15,line)!=3) return FALSE;
	i=0; sscanf(line,"%d",&i); j=i==0;
	i=1; sscanf(line,"%d",&i); j&=(i==1);
	if (j||i<(VERSION_NUMBER_D-1)) return FALSE;
	v=i;

	/* get data */
	o+=get_line(update+o,line); sscanf(line,"%d",&s);
	for(i=0,j=0,get_line(update+o,line);j<s;i+=4) {
		k=((line[i]-62)^6)|((line[i+1]-62)^0x15)<<6|((line[i+2]-62)^0x22)<<12|(line[i+3]-62)<<18;
		c[j++]=k&0xff; c[j++]=k>>16&0xff; c[j++]=k>>8&0xff;
	}
	if (uncompress((u8*)update,&d,c,s)!=Z_OK) return FALSE;

	return v;
}

static u32 get_file(HINTERNET internet,update_file* uf,char* update,char* view,int offset)
{
	char line[UPDATE_SIZE];
	char local_fn[UPDATE_SIZE] = {0};
	char url_fn[UPDATE_SIZE];
	char zip_fn[UPDATE_SIZE];
	HINTERNET hurl;
	DWORD br;
	u32 local_crc=0;
	int local_size=0;
	int url_size=0;
	int is_zip=FALSE;
	u8* data=NULL;

	/* get id */
	offset+=get_line(update+offset,line); sscanf(line,"%d",&is_zip);
	if (is_zip==U_ID_EOF) return UPDATE_EOF;
	is_zip=(is_zip==U_ID_ZIP);

	/* get local filename */
	offset+=get_line(update+offset,local_fn);
	sprintf( line, "\r\n%s...", local_fn );
	update_view(view,line);

	/* get local checksum, local size, url */
	offset+=get_line(update+offset,line); sscanf(line,"%08X",&local_crc);
	offset+=get_line(update+offset,line); sscanf(line,"%d",&local_size);
	offset+=get_line(update+offset,url_fn);

	if (is_zip) {
		/* get url size, filename in zip */
		offset+=get_line(update+offset,line); sscanf(line,"%d",&url_size);
		offset+=get_line(update+offset,zip_fn);
	}
	else url_size=local_size;

	/* check if file exists locally */
	file_setfile(&file->appdir,local_fn,NULL,NULL);
	MEM_CLEAN(file->ext); /* don't want crc of file inside zip */
	if (file_open()&&file->crc32==local_crc) {
		file_close();
		return offset<<3|UPDATE_HAVE;
	}
	file_close();

	/* download file */
	if (!(hurl=InternetOpenUrl(internet,url_fn,NULL,0,0,0))) return offset<<3|UPDATE_FAIL;
	MEM_CREATE(data,url_size);
	InternetReadFile(hurl,data,url_size,&br);
	InternetCloseHandle(hurl);

	if (dialog_closed) { MEM_CLEAN(data); return 0; }

	if (is_zip) {
		/* get uncompressed file */
		char temp[0x100];
		sprintf(temp,"%s.zip",file->temp);

		file_setfile(NULL,temp,NULL,NULL);
		if (!file_save()||!file_write(data,url_size)) {
			file_close(); MEM_CLEAN(data); remove(temp);
			return offset<<3|UPDATE_FAIL;
		}

		file_close(); MEM_CLEAN(data); MEM_CREATE(data,local_size);
		MEM_CLEAN(file->filename_in_zip_pre);
		MEM_CREATE(file->filename_in_zip_pre,strlen(zip_fn)+1); strcpy(file->filename_in_zip_pre,zip_fn);

		if (!file_open()||!file_read(data,local_size)) {
			MEM_CLEAN(file->filename_in_zip_pre);
			file_close(); MEM_CLEAN(data); remove(temp);
			return offset<<3|UPDATE_FAIL;
		}
		MEM_CLEAN(file->filename_in_zip_pre);
		file_close(); remove(temp);
	}

	if (local_crc!=(u32)crc32(0,data,local_size)) {
		MEM_CLEAN(data);
		return offset<<3|UPDATE_FAIL;
	}

	uf->data=data; data=NULL;
	uf->size=local_size;
	strcpy(uf->name,local_fn);
	MEM_CREATE_T(uf->next,sizeof(update_file),update_file*);

	return offset<<3|UPDATE_OK;
}


static DWORD WINAPI download_thread(void*)
{
	const char user_agent[]=USER_AGENT;
	char view_text[UPDATE_SIZE*4]={0};
	char update_text[UPDATE_SIZE];
	HINTERNET internet=NULL;
	update_file* uf;
	int version,offset=0;
	int c[]={0,0,0};
	u32 ret;

	MEM_CREATE_T(update_file_begin,sizeof(update_file),update_file*);
	uf=update_file_begin;

	/* init */
	update_view(view_text,"Connecting...");
	if (!(internet=InternetOpen(user_agent,INTERNET_OPEN_TYPE_DIRECT,NULL,NULL,0))||!(version=check_valid(internet,update_text))) {
		update_view(view_text,um_msg[UPDATE_FAIL]);
		update_view(view_text,"\r\n\r\nDe heu!");

		if (internet) { InternetCloseHandle(internet); internet=NULL; }
		clean_file_ll(); return 0;
	}
	else {
		update_view(view_text,um_msg[UPDATE_OK]);
		if (dialog_closed) { InternetCloseHandle(internet); clean_file_ll(); return 0; }
	}

	/* get files */
	for (;;) {
		Sleep(1);
		ret=get_file(internet,uf,update_text,view_text,offset);
		offset=(ret>>3); ret&=7;

		if (dialog_closed) { InternetCloseHandle(internet); clean_file_ll(); return 0; }

		if (ret==UPDATE_OK) uf=uf->next;
		else if (ret==UPDATE_EOF) break;

		update_view(view_text,um_msg[ret]);
		c[ret]++;
	}

	InternetCloseHandle(internet);

	if (version>VERSION_NUMBER_D) {
		update_view(view_text,"\r\n\r\nA new version of meisei is available at\r\n");
		update_view(view_text,version_get(VERSION_URL));
	}
	else {
		if (c[UPDATE_FAIL]) update_view(view_text,"\r\n");
		else if (!c[UPDATE_OK]) update_view(view_text,"\r\n\r\nEverything was already up to date.");
		else update_view(view_text,"\r\n\r\nDone!");
	}
	if (c[UPDATE_FAIL]) update_view(view_text,"\r\nCouldn't download all updates,\r\ntry again later.");
	if (c[UPDATE_OK]&&!dialog_closed) EnableWindow(GetDlgItem(dialog_handle,IDC_UPDATE_APPLY),TRUE);

	return 0;
}
#endif // MEISEI_UPDATE

int update_download_thread_is_active(void)
{
#ifdef MEISEI_UPDATE
	DWORD ec;

	if (!thread_handle) return FALSE;
	else if (!GetExitCodeThread(thread_handle,&ec)) return TRUE;
	else if (ec==STILL_ACTIVE) return TRUE;
	else return FALSE;
#else
    return FALSE;
#endif // MEISEI_UPDATE
}

INT_PTR CALLBACK update_dialog( HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam )
{
#ifdef MEISEI_UPDATE
    lParam = lParam;
	switch (msg) {

		case WM_INITDIALOG:
        {
			DWORD tid; /* unused */

			dialog_handle=dialog;
			dialog_closed=FALSE;

			if (thread_handle) CloseHandle(thread_handle);
			clean_file_ll();
			thread_handle=CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)download_thread,NULL,0,&tid);

			main_parent_window(dialog,0,0,0,0,0);

			return 1;
        }
        break;

		case WM_COMMAND:

			switch (LOWORD(wParam)) {

				/* save files */
				case IDC_UPDATE_APPLY:
                {
					update_file* uf=update_file_begin;
					char t[UPDATE_SIZE];
					int save;

					if (!uf||!uf->data||dialog_closed) break;

					for (;uf->data;uf=uf->next) {
						save=TRUE;

						file_setfile(&file->appdir,uf->name,NULL,NULL);
						if (file_accessible()) {
							sprintf(t,"Overwrite %s?",uf->name);
							if (MessageBox(dialog,t,"meisei",MB_ICONEXCLAMATION|MB_YESNO)==IDNO) save=FALSE;
						}

						if (save) {
							if (!file_save()||!file_write(uf->data,uf->size)) {
								sprintf(t,"Couldn't save %s!",uf->name);
								LOG_ERROR_WINDOW(dialog,t);
							}
							file_close();
						}
					}

					clean_file_ll();
					EnableWindow(GetDlgItem(dialog,IDC_UPDATE_APPLY),FALSE);
				}
                break;

				/* close dialog */
				case IDOK:
                case IDCANCEL:
                {
					DWORD ec;

					dialog_closed=TRUE;
					Sleep(1);

					if (thread_handle&&GetExitCodeThread(thread_handle,&ec)&&ec!=STILL_ACTIVE) {
						CloseHandle(thread_handle);
						thread_handle=NULL;
						clean_file_ll();
					}
					dialog_handle=NULL;

					EndDialog(dialog,0);
				}
                break;

				default:
                break;
			}
			break;

		default:
        break;
	}

	return 0;
#else
    dialog = dialog;
    msg    = msg;
    wParam = wParam;
    lParam = lParam;
	return 0;
#endif // MEISEI_UPDATE
}


void update_clean(void)
{
#ifdef MEISEI_UPDATE
	/* close thread */
	if (thread_handle) {
		WaitForSingleObject(thread_handle,INFINITE);
		CloseHandle(thread_handle);
		thread_handle=NULL;
	}

	clean_file_ll();
#endif // MEISEI_UPDATE
}
