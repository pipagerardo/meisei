/******************************************************************************
 *                                                                            *
 *                                "tape.c"                                    *
 *                                                                            *
 ******************************************************************************/

#include <windows.h>

#include "global.h"
#include "file.h"
#include "io.h"
#include "z80.h"
#include "mapper.h"
#include "psg.h"
#include "tape.h"
#include "resource.h"
#include "state.h"
#include "main.h"
#include "zlib.h"

#define CAS_MAX_SIZE	20000000	/* 20MB */
#define CAS_MAX_FILE	0x20000		/* 128KB */
#define CAS_MAX_BLOCK	(CAS_MAX_FILE+0x120+(CAS_MAX_FILE>>5))
#define CAS_MAX_LISTBOX	6

static const u8 cas_header[8]={0x1f,0xa6,0xde,0xba,0xcc,0x13,0x7d,0x74};
static const u8 block_header[3]={0xd0,0xd3,0xea}; /* bin,bas,asc / 10 */
#ifdef MEISEI_ESP
static const char* block_description[CAS_TYPE_MAX]={"binario","BASIC","ASCII","personalizado","<bloque libre>","<final de cinta>"};
#else
static const char* block_description[CAS_TYPE_MAX]={"binary","BASIC","ASCII","custom","<free block>","<end of tape>"};
#endif // MEISEI_ESP


static u8* write_cache_block=NULL;
static char tapeblock_dir[STRING_SIZE]={0};
static int runtime_block_inserted=FALSE;

typedef struct _tape_block {
	int type;
	int size;
	int raw;
	char name[0x10];
	u8* data;
	struct _tape_block* next;
	struct _tape_block* prev;
} _tape_block;

typedef struct {
	char* fn;
	u32 crc;
	int read_only;
	int updated;
	int save_needed;
	int cur_block;
	int pos;
	_tape_block* blocks;
} _tape;

static _tape* tape_cur=NULL;
static _tape* tape_od=NULL;

static u32 tape_push_sp=0;
static _tape_block* tape_push_b[0x10];
static int tape_push_cb[0x10];
static int tape_push_pos[0x10];

int __fastcall tape_is_inserted(void) { return tape_cur!=NULL; }
int __fastcall tape_get_runtime_block_inserted(void) { return runtime_block_inserted; }
void tape_set_runtime_block_inserted(int i) { runtime_block_inserted=i; }

char* tape_get_fn_cur(void)        { if (!tape_cur) return NULL; else return tape_cur->fn; }
char* tape_get_fn_od(void)         { if (!tape_od) return NULL; else return tape_od->fn; }
u32   tape_get_crc_cur(void)       { if (!tape_cur) return 0; else return tape_cur->crc; }
u32   tape_get_crc_od(void)        { if (!tape_od) return 0; else return tape_od->crc; }
int   tape_get_pos_cur(void)       { if (!tape_cur) return 0; else return tape_cur->pos; }
int   tape_get_cur_block_cur(void) { if (!tape_cur) return 0; else return tape_cur->cur_block; }
int   tape_get_cur_block_od(void)  { if (!tape_od) return 0; else return tape_od->cur_block; }
int   tape_get_blocktype_cur(void) { if (!tape_cur||!tape_cur->blocks) return 0xff; else return tape_cur->blocks->type; }
int   tape_get_updated_od(void)    { if (!tape_od) return 0; else return tape_od->updated; }
void  tape_set_updated_od(void)    { if (tape_od) tape_od->updated=TRUE; }

int tape_cur_block_od_is_empty(void)
{
	if (!tape_od||!tape_od->blocks) return 4;
	else if (tape_od->blocks->type==CAS_TYPE_END) return 2;
	else if (tape_od->blocks->type==CAS_TYPE_NOP) return 1;
	else return 0;
}

void tape_init(void)
{
	MEM_CREATE_N(write_cache_block,CAS_MAX_BLOCK+1);
}

void tape_clean(void)
{
	/* save */
	tape_cur_to_od();
	tape_flush_cache_od();
	tape_save_od();

	/* clean both tape structs */
	tape_clean_od();
	tape_od_to_cur();

	MEM_CLEAN(write_cache_block);
}


/* pvt */
static void tape_push(_tape* tape)
{
	/* remember current block position */
	tape_push_b[tape_push_sp]=tape->blocks;
	tape_push_cb[tape_push_sp]=tape->cur_block;
	tape_push_pos[tape_push_sp]=tape->pos;

	tape_push_sp++;
	tape_push_sp&=0xf;
}

static void tape_pop(_tape* tape)
{
	tape_push_sp--;
	tape_push_sp&=0xf;

	tape->blocks=tape_push_b[tape_push_sp];
	tape->cur_block=tape_push_cb[tape_push_sp];
	tape->pos=tape_push_pos[tape_push_sp];
}

static void tape_rewind(_tape* tape)
{
	if (!tape->blocks) return;

	/* rewind tape */
	for (;;) {
		if (tape->blocks->prev) tape->blocks=tape->blocks->prev;
		else break;
	}

	tape->pos=0;
	tape->cur_block=0;
}

static void tape_insert_block(_tape* tape)
{
	_tape_block* bi;

	/* create free block */
	MEM_CREATE_T(bi,sizeof(_tape_block),_tape_block*);
	bi->type=CAS_TYPE_NOP;
	bi->next=tape->blocks;
	bi->prev=tape->blocks->prev;

	/* insert block at current position */
	if (tape->blocks->prev) tape->blocks->prev->next=bi;
	tape->blocks->prev=bi;

	tape->blocks=bi;
	tape->pos=0;
}

static int tape_next_used(_tape* tape)
{
	tape->pos=0;

	if (!tape->blocks||!tape->blocks->next) return FALSE;

	/* go to next used block */
	for (;;) {
		tape->blocks=tape->blocks->next; tape->cur_block++;
		if (tape->blocks->type!=CAS_TYPE_NOP&&tape->blocks->type!=CAS_TYPE_END) return TRUE;
		if (tape->blocks->type==CAS_TYPE_END||!tape->blocks->next) break;
	}

	return FALSE;
}

static void tape_clean_struct(_tape* tape)
{
	if (!tape) return;
	if (tape_cur==tape_od) return;

	/* clean blocks */
	tape_rewind(tape);
	if (tape->blocks) {
		for (;;) {
			if (!tape->blocks->next) {
				MEM_CLEAN(tape->blocks->data);
				MEM_CLEAN(tape->blocks);
				break;
			}
			tape->blocks=tape->blocks->next;
			MEM_CLEAN(tape->blocks->prev->data);
			MEM_CLEAN(tape->blocks->prev);
		}
	}

	MEM_CLEAN(tape->fn);
	MEM_CLEAN(tape);
}

static void tape_set_cur_block(u32 b,_tape* tape)
{
	if (!tape||!tape->blocks) return;
	tape_rewind(tape);

	while (b--) {
		if (tape->blocks->next&&tape->blocks->type!=CAS_TYPE_END) {
			tape->blocks=tape->blocks->next;
			tape->cur_block++;
		}
		else break;
	}
}

static int tape_get_size(_tape* tape)
{
	int size=0;

	if (!tape||!tape->blocks) return 0;
	tape_push(tape);

	tape_rewind(tape);
	for (;;) {
		size+=tape->blocks->size;
		tape->blocks=tape->blocks->next;
		if (!tape->blocks) break;
	}

	tape_pop(tape);
	return size;
}

static int tape_get_blocks(_tape* tape)
{
	int nb=0;

	if (!tape||!tape->blocks) return 0;
	tape_push(tape);

	tape_rewind(tape);
	for (;;) {
		nb++;
		tape->blocks=tape->blocks->next;
		if (!tape->blocks) break;
	}

	tape_pop(tape);
	return nb;
}

static void tape_recalc_crc(_tape* tape)
{
	int size,o=0;
	u8* data=NULL;

	if (!tape||!tape->blocks) return;
	size=tape_get_size(tape);
	if (size==0) { tape->crc=0; return; }

	tape_push(tape);

	/* copy all blocks */
	tape_rewind(tape);
	MEM_CREATE_N(data,size);
	for (;;) {
		if (tape->blocks->size&&tape->blocks->data) {
			memcpy(data+o,tape->blocks->data,tape->blocks->size);
			o+=tape->blocks->size;
		}

		tape->blocks=tape->blocks->next;
		if (!tape->blocks) break;
	}

	/* calculate new crc */
	if (o) tape->crc=crc32(0,data,o);
	else tape->crc=0;
	MEM_CLEAN(data);

	tape_pop(tape);
}

static _tape_block* tape_get_block(u32 b,_tape* tape)
{
	_tape_block* b_ret;

	if (!tape||!tape->blocks) return NULL;
	tape_push(tape);

	tape_set_cur_block(b,tape);
	b_ret=tape->blocks;

	tape_pop(tape);
	return b_ret;
}

static void tape_set_name(char* name)
{
	/* set 6 char name + remove trailing space */
	char t[0x10];
	int i;

	memset(t,0,0x10);
	memcpy(t,name,6);
	memset(name,0,7);

	i=6;
	while (i--) {
		/* remove illegal characters */
		if ((u8)t[i]==0x7f||(u8)t[i]==0xff||(u8)t[i]<0x20) t[i]=0x20;
	}

	i=6;
	while (i--) {
		/* trim right */
		if (t[i]==0x20) t[i]=0;
		else break;
	}

	for (i=0;i<6;i++) if (t[i]!=0x20) break; /* 'trim' left */

	if (strlen(t+i)) strcpy(name,t+i);
}

static int tape_find_blocktype(u8* header)
{
	int i;
	u8 header_cmp[10];

	if (memcmp(header+0x10,cas_header,8)) return CAS_TYPE_CUS;

	for (i=0;i<3;i++) {
		memset(header_cmp,block_header[i],10);
		if (!memcmp(header,header_cmp,10)) break;
	}

	return i;
}

static int tape_find_blocksize(u8* data,int* size,int* raw,int type)
{
	int ret=FALSE;

	if (data==NULL||size==NULL||raw==NULL||*size==0||data[0]!=cas_header[0]) return FALSE;

	*raw=0;

	switch (type) {

		case CAS_TYPE_BIN:
			if (*size>0x24) {
				/* BLOAD start/end address */
				int sa=data[0x21]<<8|data[0x20];
				int ea=(data[0x23]<<8|data[0x22])+1;
				if (ea>sa&&(*size-0x26)>=(ea-sa)) {
					ret=TRUE;
					*raw=(ea-sa)+6;
					*size=*raw+0x20; *size&=~7;
					(*raw)++; /* $FE header */
				}
			}
			break;

		case CAS_TYPE_BAS:
			if (*size>0x22) {
				/* tokenized BASIC, assume it starts at $e001, $c001, or $8001 */
				int ad,ad_prev=(data[0x21]<<8&0xe000)|1;
				for (;;) {
					ad=data[*raw+0x21]<<8|data[*raw+0x20];
					if (ad==0) {
						/* end */
						ret=TRUE;
						*raw+=2;
						*size=*raw+0x20; *size&=~7;
						(*raw)++; /* $FF header */

						break;
					}
					else if ((ad-ad_prev)>0&&(*raw+0x22+(ad-ad_prev))<=*size) {
						/* next line */
						*raw+=(ad-ad_prev);
						ad_prev=ad;
					}
					else {
						/* error */
						break;
					}
				}
			}

			break;

		case CAS_TYPE_ASC:
			if (*size>0x20) {
				for (;;) {
					if (data[*raw+0x20]==0x1a) {
						/* ^Z, end */
						ret=TRUE;
						(*raw)++;
						*size=*raw+0x20; *size&=~7;
						/* (raw includes combined size of short headers) */

						break;
					}
					else if ((*raw+0x20)==*size) {
						/* error */
						break;
					}

					(*raw)++;
				}
			}
			break;

		default:
			if (*size>8) {
				ret=TRUE;
				*raw=*size-8;
			}
			break;
	}

	return ret;
}

static void tape_flush_cache(_tape* tape)
{
	int size,raw=0;

	if (!tape||!tape->blocks||tape->blocks->type!=CAS_TYPE_NOP||tape->pos==0) return;

	/* find type */
	if (tape->pos>=0x20) {
		tape->blocks->type=tape_find_blocktype(write_cache_block+8);
		if (tape->blocks->type!=CAS_TYPE_CUS) {

			/* make sure it doesn't end with a header */
			if (tape->pos==0x20||(tape->blocks->type==CAS_TYPE_ASC&&!memcmp(write_cache_block-8,cas_header,8))) write_cache_block[tape->pos++]=(tape->blocks->type==CAS_TYPE_ASC)?0x1a:0;

			/* set name */
			memcpy(tape->blocks->name,write_cache_block+18,6);
			tape_set_name(tape->blocks->name);
		}
	}
	else {
		tape->blocks->type=CAS_TYPE_CUS;
		if (tape->pos==8) write_cache_block[tape->pos++]=0;
	}

	size=tape->pos;
	if (!tape_find_blocksize(write_cache_block,&size,&raw,tape->blocks->type)) {
		if (tape->blocks->type==CAS_TYPE_CUS) raw=tape->pos-8;
		else raw=(tape->pos-0x20)+((tape->blocks->type==CAS_TYPE_BIN)|(tape->blocks->type==CAS_TYPE_BAS));
	}

	/* set data */
	tape->blocks->size=tape->pos;
	tape->blocks->raw=raw;
	MEM_CREATE_N(tape->blocks->data,tape->pos);
	memcpy(tape->blocks->data,write_cache_block,tape->pos);

	tape_set_cur_block(tape->cur_block+1,tape);
	tape_recalc_crc(tape);
	tape->save_needed=TRUE;
}


/* control */
void tape_cur_to_od(void)
{
	_tape_block* b;

	if (!tape_cur) {
		tape_od=NULL;
		return;
	}

	/* copy tape struct */
	MEM_CREATE_T(tape_od,sizeof(_tape),_tape*);
	memcpy(tape_od,tape_cur,sizeof(_tape));
	MEM_CREATE(tape_od->fn,strlen(tape_cur->fn)+1);
	strcpy(tape_od->fn,tape_cur->fn);
	MEM_CREATE_T(tape_od->blocks,sizeof(_tape_block),_tape_block*);

	tape_rewind(tape_cur);

	/* copy blocks */
	for (;;) {
		b=tape_od->blocks->prev; /* don't copy prev pointer */
		memcpy(tape_od->blocks,tape_cur->blocks,sizeof(_tape_block));
		tape_od->blocks->prev=b;

		/* block data */
		if (tape_od->blocks->data) {
			MEM_CREATE_N(tape_od->blocks->data,tape_od->blocks->size);
			memcpy(tape_od->blocks->data,tape_cur->blocks->data,tape_od->blocks->size);
		}

		/* next block */
		if (tape_cur->blocks->next) {
			MEM_CREATE_T(tape_od->blocks->next,sizeof(_tape_block),_tape_block*);
			tape_od->blocks->next->prev=tape_od->blocks;
			tape_od->blocks=tape_od->blocks->next;
			tape_cur->blocks=tape_cur->blocks->next;
		}
		else break;
	}

	/* set pos */
	tape_set_cur_block(tape_od->cur_block,tape_cur); tape_cur->pos=tape_od->pos;
	tape_set_cur_block(tape_cur->cur_block,tape_od); tape_od->pos=tape_cur->pos;
}

void tape_od_to_cur(void)
{
	tape_clean_struct(tape_cur);
	tape_cur=tape_od;

	tape_od=NULL;
}

int tape_open_od(int auto_patch)
{
	u8* filedata=NULL;
	char fn[STRING_SIZE]={0};
	char n[STRING_SIZE]={0};
	u32 crc,crc_p=0;
	int s,o,f,t,i,e;

	/* open file, just to get checksum */
	if (!file_open()) {
		file_close();
		return FALSE;
	}
	crc=file->crc32;
	strcpy(fn,file->filename);
	file_close();

	if (auto_patch) {
		/* open optional ips (like for roms in mapper.c) */
		strcpy(n,file->name); i=TRUE;
		file_setfile(&file->patchdir,n,"ips",NULL);
		if (!file_open()) {
			file_close();
			file_setfile(&file->patchdir,n,"zip","ips");
			if (!file_open()) { i=FALSE; file_close(); }
		}
		crc_p=file->crc32;
		if (i&(crc_p!=crc)) {
			if (!file_patch_init()) { file_patch_close(); crc_p=0; }
		}
		else crc_p=0;
		file_close();

		/* set to rom file again */
		file_setfile(NULL,fn,NULL,"cas");
	}

	/* open/read entire file */
	if (!file_open()||file->size<9||file->size>CAS_MAX_SIZE) {
		file_patch_close(); file_close();
		return FALSE;
	}
	MEM_CREATE(filedata,file->size);
	if (!file_read(filedata,file->size)||memcmp(filedata,cas_header,8)) {
		file_patch_close(); file_close(); MEM_CLEAN(filedata);
		return FALSE;
	}
	file_patch_close();
	file_close();

	/* init tape struct */
	tape_clean_od();
	MEM_CREATE_T(tape_od,sizeof(_tape),_tape*);
	tape_od->read_only=(file->is_zip!=0);
	tape_od->crc=file->crc32;
	if (crc_p) {
		tape_od->read_only|=2;
		tape_od->crc=(tape_od->crc<<1|(tape_od->crc>>31&1))^crc_p; /* combine with patch crc */
	}
	MEM_CREATE(tape_od->fn,strlen(file->filename)+1);
	strcpy(tape_od->fn,file->filename);
	MEM_CREATE_T(tape_od->blocks,sizeof(_tape_block),_tape_block*);

	o=e=0;

	/* read blocks */
	for (;;) {
		s=8;

		/* create next block */
		MEM_CREATE_T(tape_od->blocks->next,sizeof(_tape_block),_tape_block*);
		tape_od->blocks->next->prev=tape_od->blocks;

		/* determine type */
		if ((file->size-(o+s))>24) {

			t=tape_find_blocktype(filedata+o+s);
			if (t!=CAS_TYPE_CUS) {
				/* set name */
				memcpy(tape_od->blocks->name,filedata+o+s+10,6);
				tape_set_name(tape_od->blocks->name);

				/* find end of block */
				s=file->size-o;
				if (!tape_find_blocksize(filedata+o,&s,&e,t)) {
					s=0x20;
					e=0;
				}
			}
		}
		else t=CAS_TYPE_CUS;

		/* find next block */
		f=TRUE;
		for (;;) {
			if ((file->size-(o+s))>8) {
				if (!memcmp(filedata+o+s,cas_header,8)) break;
				s+=8;
			}
			else {
				s+=(file->size-(o+s));
				f=FALSE;
				break;
			}
		}

		MEM_CREATE_N(tape_od->blocks->data,s);
		memcpy(tape_od->blocks->data,filedata+o,s);
		tape_od->blocks->type=t;
		tape_od->blocks->size=s;

		/* set assumed length */
		switch (t) {
			case CAS_TYPE_CUS:
				/* unknown, set to whole block minus header */
				tape_od->blocks->raw=s-8;
				break;

			default:
				if (e==0) tape_od->blocks->raw=(s-0x20)+((t==CAS_TYPE_BIN)|(t==CAS_TYPE_BAS)); /* bin/bas: 1 byte header */
				else tape_od->blocks->raw=e;

				break;
		}

		tape_od->blocks=tape_od->blocks->next;

		if (!f) break;
		o+=s;
	}

	tape_od->blocks->type=CAS_TYPE_END;
	tape_rewind(tape_od);

	MEM_CLEAN(filedata);
	return TRUE;
}

int tape_save_od(void)
{
	int size,nb=0;
	int t,s,o=0;
	int ret=1;
	u8* filedata=NULL;
	FILE* fd=NULL;

	if (!tape_od||!tape_od->blocks||!strlen(tape_od->fn)||!tape_od->save_needed||(size=tape_get_size(tape_od))==0) return 2;
	nb=tape_get_blocks(tape_od); nb*=8;
	tape_push(tape_od);
	size+=nb;
	MEM_CREATE_N(filedata,size);

	/* copy blocks */
	tape_rewind(tape_od);
	if (tape_od->blocks->size==0) tape_next_used(tape_od);
	for (;;) {
		memcpy(filedata+o,tape_od->blocks->data,tape_od->blocks->size);
		o+=tape_od->blocks->size;

		t=(tape_od->blocks->type==CAS_TYPE_ASC)?0x1a:0;
		s=tape_od->blocks->size&7;
		if (s) s=8-s;

		if (!tape_next_used(tape_od)) break;

		/* block (except last one) must be multiple of 8 */
		if (s) {
			memset(filedata+o,t,s);
			o+=s;
		}
	}

	/* save */
	if (!file_save_custom(&fd,tape_od->fn)||!file_write_custom(fd,filedata,o)) ret=0;

	file_close_custom(fd);
	MEM_CLEAN(filedata);
	tape_pop(tape_od);
	return ret;
}

int tape_new_od(HWND dialog)
{
	const char* filter="fMSX CAS File (*.cas)\0*.cas\0\0";
	const char* defext="cas";
	const char* title="Create New Empty Tape";
	char fn[STRING_SIZE]={0};
	OPENFILENAME of;

	memset(&of,0,sizeof(OPENFILENAME));

	of.lStructSize=sizeof(OPENFILENAME);
	of.hwndOwner=dialog;
	of.hInstance=MAIN->module;
	of.lpstrFile=fn;
	of.lpstrDefExt=defext;
	of.lpstrFilter=filter;
	of.lpstrTitle=title;
	of.nMaxFile=STRING_SIZE;
	of.nFilterIndex=1;
	of.Flags=OFN_ENABLESIZING|OFN_HIDEREADONLY|OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST;
	of.lpstrInitialDir=file->casdir?file->casdir:file->appdir;

	if (GetSaveFileName(&of)&&strlen(fn)) {
		if (!tape_save_od()) LOG_ERROR_WINDOW(dialog,"Couldn't save tape!");
		tape_clean_od();

		/* create new tape */
		MEM_CREATE_T(tape_od,sizeof(_tape),_tape*);
		MEM_CREATE(tape_od->fn,strlen(fn)+1);
		strcpy(tape_od->fn,fn);
		MEM_CREATE_T(tape_od->blocks,sizeof(_tape_block),_tape_block*);
		tape_od->blocks->type=CAS_TYPE_END;

		/* free block as 0 */
		tape_insert_block(tape_od);

		/* (don't remember casdir) */

		return TRUE;
	}
	else return FALSE;
}

void tape_clean_od(void) { tape_clean_struct(tape_od); tape_od=NULL; }
void tape_rewind_cur(void) { if (tape_cur) tape_rewind(tape_cur); }
void tape_set_cur_block_od(u32 i) { tape_set_cur_block(i,tape_od); }
void tape_flush_cache_od(void) { if (tape_od) tape_flush_cache(tape_od); }


/* GUI */
static LONG_PTR sub_listbox_prev;
static int popup_rb_down=FALSE;
static int popup_active=FALSE;
static POINT popup_pos;

void tape_init_gui(LONG_PTR p)
{
	popup_rb_down=FALSE;
	popup_active=FALSE;
	sub_listbox_prev=p;

	if (tape_od) tape_od->updated=FALSE;
}

void tape_draw_gui(HWND dialog)
{
	/* draw gui minus pos/listbox contents */
	if (!dialog) return;

	/* filename editbox */
	if (tape_od&&tape_od->fn&&strlen(tape_od->fn)) {
		int len=strlen(tape_od->fn);
		int i=len;

		while (i--) {
			/* find start of name */
			if (tape_od->fn[i]==':'||tape_od->fn[i]=='\\'||tape_od->fn[i]=='/') { i++; break; }
		}
		if (i<2||i>(len-2)) i=0;

		SetDlgItemText(dialog,IDC_MEDIA_TAPEFILE,tape_od->fn+i);
	}
	else SetDlgItemText(dialog,IDC_MEDIA_TAPEFILE,"");

	/* eject button enabled? */
	EnableWindow(GetDlgItem(dialog,IDC_MEDIA_TAPEEJECT),tape_od!=NULL);

	/* listbox enabled? */
	EnableWindow(GetDlgItem(dialog,IDC_MEDIA_TAPECONTENTST),tape_od!=NULL);
	EnableWindow(GetDlgItem(dialog,IDC_MEDIA_TAPECONTENTS),tape_od!=NULL);

	/* read-only? */
	if (tape_od&&tape_od->read_only) SetDlgItemText(dialog,IDC_MEDIA_TAPER,"R");
	else SetDlgItemText(dialog,IDC_MEDIA_TAPER,"");
}

void tape_set_popup_pos(void)
{
	/* slightly to the left */
	popup_pos.x=30; popup_pos.y=3;
	GetCursorPos(&popup_pos);
	popup_pos.x-=30; popup_pos.y-=3;
}

static INT_PTR CALLBACK tape_blockrename_dialog( HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam )
{
    lParam = lParam;
	switch (msg) {

		case WM_INITDIALOG: {
			RECT r;
			char t[0x100];

			SendDlgItemMessage(dialog,IDC_MEDIATAPE_RENAME_EDIT,EM_LIMITTEXT,6,0);
			if (strlen(tape_od->blocks->name)) SetDlgItemText(dialog,IDC_MEDIATAPE_RENAME_EDIT,tape_od->blocks->name);

			sprintf(t,"Rename Tape Block %d",tape_od->cur_block+1);
			SetWindowText(dialog,t);

			/* position window on popup menu location */
			GetWindowRect(GetParent(dialog),&r);
			r.top=popup_pos.y-r.top; if (r.top<0) r.top=0;
			r.left=popup_pos.x-r.left; if (r.left<0) r.left=0;
			main_parent_window(dialog,MAIN_PW_LEFT,MAIN_PW_LEFT,r.left,r.top,0);

			PostMessage(dialog,WM_NEXTDLGCTL,(WPARAM)GetDlgItem(dialog,IDC_MEDIATAPE_RENAME_EDIT),TRUE);

			break;
		}

		case WM_COMMAND:

			switch (LOWORD(wParam)) {

				/* close dialog */
				case IDOK:                  // Warning -Wimplicit-fallthrough
                {
					char t[0x10];

					memset(t,0,0x10);
					GetDlgItemText(dialog,IDC_MEDIATAPE_RENAME_EDIT,t,7);
					tape_set_name(t);

					if (memcmp(t,tape_od->blocks->name,6)) {
						/* name changed */
						int len=strlen(t);

						if (len) {
							strcpy(tape_od->blocks->name,t);
							memcpy(tape_od->blocks->data+18,t,len);
						}
						else memset(tape_od->blocks->name,0,7);
						if (len<6) memset(tape_od->blocks->data+18+len,0x20,6-len);
						tape_draw_listbox(GetDlgItem(GetParent(dialog),IDC_MEDIA_TAPECONTENTS));

						tape_recalc_crc(tape_od);
						tape_od->save_needed=TRUE;
					}
					/* fall through */
				}
				case IDCANCEL:
					EndDialog(dialog,0);
                break;

				default:
                break;
			}

			break;

		default: break;
	}

	return 0;
}

INT_PTR CALLBACK tape_blockinfo_dialog( HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam )
{
    lParam = lParam;
	switch (msg) {

		case WM_INITDIALOG: {
			RECT r;
			char t[0x1000]={0};
			char t2[0x200]={0};
			char t3[0x200]={0};
			int l,ba=FALSE;

			/* type */
			sprintf(t,"Block Type: %s\r\n",block_description[tape_od->blocks->type]);

			/* name */
			if (tape_od->blocks->type==CAS_TYPE_CUS) sprintf(t2,"(no filename)\r\n");
			else if (strlen(tape_od->blocks->name)) sprintf(t2,"Filename: \"%s\"\r\n",tape_od->blocks->name);
			else sprintf(t2,"(unknown filename)\r\n");
			strcat(t,t2);

			/* size */
			l=tape_od->blocks->raw;
			if (tape_od->blocks->type==CAS_TYPE_ASC) {
				/* short header every 256 bytes */
				l=(tape_od->blocks->raw-1)/0x108;
				l=tape_od->blocks->raw-l*8;
			}
			else l=tape_od->blocks->raw;
			sprintf(t2,"Size: %d bytes\r\n\r\n",l);
			strcat(t,t2);

			/* address */
			if (tape_od->blocks->type==CAS_TYPE_BIN&&tape_od->blocks->size>38) {
				int sa=tape_od->blocks->data[0x21]<<8|tape_od->blocks->data[0x20];
				int ea=tape_od->blocks->data[0x23]<<8|tape_od->blocks->data[0x22];
				int exa=tape_od->blocks->data[0x25]<<8|tape_od->blocks->data[0x24];
				if (ea>=sa) {
					sprintf(t2,"Start Address: $%04X\r\nEnd Address: $%04X\r\nExecute Address: $%04X\r\n\r\n",sa,ea,exa);
					strcat(t,t2); ba=TRUE;
				}
			}
			else if (tape_od->blocks->type==CAS_TYPE_BAS&&tape_od->blocks->size>34) {
				sprintf(t2,"Start Address: $%02X01\r\n\r\n",tape_od->blocks->data[0x21]&0xe0);
				strcat(t,t2);
			}

			/* loading info */
			switch (tape_od->blocks->type) {

				case CAS_TYPE_BIN:
					sprintf(t2,"Run this file with BLOAD\"CAS:\",R");
					if (strlen(tape_od->blocks->name)) {
						sprintf(t3,", or\r\nBLOAD\"%s\",R if not from block %d.",tape_od->blocks->name,tape_od->cur_block+1);
						strcat(t2,t3);
					}
					strcat(t2,"\r\nRemove ,R to prevent it from executing.");
					if (!ba||!strlen(tape_od->blocks->name)) strcat(t2,"\r\n");
					break;

				case CAS_TYPE_BAS:
					sprintf(t2,"Load this file with CLOAD");
					if (strlen(tape_od->blocks->name)) sprintf(t3,", or\r\nCLOAD\"%s\" if not from block %d.\r\n",tape_od->blocks->name,tape_od->cur_block+1);
					else sprintf(t3,"\r\n");
					strcat(t2,t3);
					break;

				case CAS_TYPE_ASC:
					sprintf(t2,"Run this file with RUN\"CAS:\"");
					if (strlen(tape_od->blocks->name)) {
						sprintf(t3,", or\r\nRUN\"%s\" if not from block %d.",tape_od->blocks->name,tape_od->cur_block+1);
						strcat(t2,t3);
					}
					strcat(t2,"\r\nChange RUN to LOAD to just load.\r\n");
					break;

				/* custom */
				default:
					sprintf(t2,"This file can't be loaded from BASIC.\r\n");
					break;
			}
			strcat(t,t2);


			sprintf(t2,"Tape Block %d Info",tape_od->cur_block+1);
			SetWindowText(dialog,t2);

			SetFocus(dialog);

			SetDlgItemText(dialog,IDC_MEDIATAPE_INFO_TEXT,t);

			/* position window on popup menu location */
			GetWindowRect(GetParent(dialog),&r);
			r.top=popup_pos.y-r.top; if (r.top<0) r.top=0;
			r.left=popup_pos.x-r.left; if (r.left<0) r.left=0;
			main_parent_window(dialog,MAIN_PW_LEFT,MAIN_PW_LEFT,r.left,r.top,0);

			break;
		}

		case WM_CTLCOLORSTATIC:
			/* prevent colour changing for read-only editbox */
			return GetSysColorBrush(COLOR_WINDOW) == NULL ? FALSE : TRUE;
			break;

		case WM_COMMAND:

			switch (LOWORD(wParam)) {

				/* close dialog */
				case IDOK: case IDCANCEL:
					EndDialog(dialog,0);
					break;

				default: break;
			}

			break;

		default: break;
	}

	return 0;
}

BOOL CALLBACK tape_sub_listbox( HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	switch (msg) {
		case WM_RBUTTONDOWN:        // Warning -Wimplicit-fallthrough
        {
			/* select nearest item */
			int i=SendMessage(wnd,LB_ITEMFROMPOINT,0,lParam);
			if (!(HIWORD(i))&&GetListBoxInfo(wnd)) {
				SendMessage(wnd,LB_SETCURSEL,i,0);
				popup_rb_down=TRUE;
			}
			/* fall through */
		}
		case WM_MOUSEMOVE: {
			TRACKMOUSEEVENT tme;
			tme.cbSize=sizeof(TRACKMOUSEEVENT);
			tme.dwFlags=TME_LEAVE;
			tme.hwndTrack=wnd;
			tme.dwHoverTime=0;
			TrackMouseEvent(&tme);
			break;
		}

		case WM_RBUTTONUP: {
			/* popup menu */
			_tape_block* b;
			HMENU menu,popup;
			HWND dialog=GetParent(wnd);
			LRESULT cb;
			UINT id;
			int i,gray_open,gray_save,gray_delete,gray_rename,gray_info,gray_insert=0;

			if (!popup_rb_down||popup_active||!tape_od||!tape_od->blocks) break;
			cb=SendMessage(wnd,LB_GETCURSEL,0,0); if (cb==LB_ERR) break;
			b=tape_get_block(cb,tape_od); if (!b) break;
			if ((menu=LoadMenu(MAIN->module,"mediatapepopup"))==NULL) break;

			popup_active=TRUE;

			/* only 1 free block allowed */
			tape_rewind(tape_od);
			for (;;) {
				if (tape_od->blocks->type==CAS_TYPE_NOP) { gray_insert=TRUE; break; }
				if (tape_od->blocks->next) tape_od->blocks=tape_od->blocks->next;
				else break;
			}

			tape_set_cur_block(cb,tape_od);
			tape_draw_pos(dialog);
			tape_od->updated=TRUE;

			gray_insert|=(tape_od->read_only!=0); if (gray_insert) EnableMenuItem(menu,IDM_MEDIATAPE_INSERT,MF_GRAYED);
			gray_open=(b->type!=CAS_TYPE_NOP)|(tape_od->read_only!=0); if (gray_open) EnableMenuItem(menu,IDM_MEDIATAPE_OPEN,MF_GRAYED);
			gray_delete=(b->type==CAS_TYPE_END)|(tape_od->read_only!=0); if (gray_delete) EnableMenuItem(menu,IDM_MEDIATAPE_DELETE,MF_GRAYED);
			gray_info=(b->type==CAS_TYPE_END)|(b->type==CAS_TYPE_NOP); if (gray_info) EnableMenuItem(menu,IDM_MEDIATAPE_INFO,MF_GRAYED);
			gray_save=gray_info; if (gray_save) EnableMenuItem(menu,IDM_MEDIATAPE_SAVE,MF_GRAYED);
			gray_rename=gray_delete|gray_info|(b->type==CAS_TYPE_CUS)|(b->size<24); if (gray_rename) EnableMenuItem(menu,IDM_MEDIATAPE_RENAME,MF_GRAYED);

			popup=GetSubMenu(menu,0);
			tape_set_popup_pos();

			#define CLEAN_MENU() if (menu) { DestroyMenu(menu); menu=NULL; } popup_rb_down=popup_active=FALSE

			id=TrackPopupMenuEx(popup,TPM_LEFTALIGN|TPM_TOPALIGN|TPM_NONOTIFY|TPM_RETURNCMD|TPM_RIGHTBUTTON,(int)popup_pos.x,(int)popup_pos.y,dialog,NULL);
			switch (id) {

				/* open file */
				case IDM_MEDIATAPE_OPEN: {
					char fn[STRING_SIZE]={0};
					char t[STRING_SIZE]={0};
					const char* filter="All Files (*.*)\0*.*\0Force binary\0*.*\0Force BASIC\0*.*\0Force ASCII\0*.*\0Force custom\0*.*\0\0";
					OPENFILENAME of;

					if (gray_open) break;
					CLEAN_MENU();

					sprintf(t,"Open File For Tape Block %d",tape_od->cur_block+1);

					memset(&of,0,sizeof(OPENFILENAME));

					of.lStructSize=sizeof(OPENFILENAME);
					of.hwndOwner=dialog;
					of.hInstance=MAIN->module;
					of.lpstrFile=fn;
					of.lpstrFilter=filter;
					of.lpstrTitle=t;
					of.nMaxFile=STRING_SIZE;
					of.nFilterIndex=1;
					of.Flags=OFN_ENABLESIZING|OFN_HIDEREADONLY|OFN_FILEMUSTEXIST;
					of.lpstrInitialDir=strlen(tapeblock_dir)?tapeblock_dir:file->tooldir?file->tooldir:file->appdir;

					if (GetOpenFileName(&of)) {
						FILE* fd=NULL;
						int size;

						/* open */
						if (strlen(fn)&&(size=file_open_custom(&fd,fn))>0) {
							if (size<=CAS_MAX_FILE) {
								u8* filedata;
								MEM_CREATE_N(filedata,size+1);

								if (file_read_custom(fd,filedata,size)) {
									int type=of.nFilterIndex-2;
									u8* data;

									MEM_CREATE_N(data,size+(CAS_MAX_BLOCK-CAS_MAX_FILE));

									/* autodetect type */
									if (type<0||type>3) {
										if (filedata[0]==0xfe) type=CAS_TYPE_BIN; /* $FE header */
										else if (filedata[0]==0xff&&size>2&&(filedata[size-2]|filedata[size-1])==0) type=CAS_TYPE_BAS; /* $FF header and ends with 00 00 */
										else if (filedata[0]>=48&&filedata[0]<=57&&(filedata[size-1]==0x1a||filedata[size-1]==0xa)) type=CAS_TYPE_ASC; /* starts with a number and ends with ^Z or return */
										else type=CAS_TYPE_CUS;
									}

									/* make sure ASCII type ends with ^Z */
									if (type==CAS_TYPE_ASC&&filedata[size-1]!=0x1a) {
										filedata[size++]=0x1a;
									}

									/* create header for loadable file */
									if (type!=CAS_TYPE_CUS) {
										memcpy(data,cas_header,8);
										memcpy(data+0x18,cas_header,8);
										memset(data+8,block_header[type],10);

										/* name */
										memset(t,0,7);
										if (strlen(fn+of.nFileOffset)) {
											strcpy(t,fn+of.nFileOffset);
											tape_set_name(t);
											if (strlen(t)) strcpy(tape_od->blocks->name,t);
										}

										/* change \0 to spaces for header */
										i=6;
										while (i--) if (t[i]==0) t[i]=0x20;
										memcpy(data+18,t,6);
									}

									i=0;

									/* copy data */
									switch (type) {

										case CAS_TYPE_BIN:
											if (size>1) memcpy(data+0x20,filedata+1,size-1);
											i=(size-1)+0x20;
											break;

										case CAS_TYPE_BAS:
											if (size>1) memcpy(data+0x20,filedata+1,size-1);
											memset(data+0x20+(size-1),0,7); /* 7 * 00 indicate eof */
											i=(size-1)+0x27;
											break;

										case CAS_TYPE_ASC: {
											int j=0; i=0x20;
											for (;;) {
												if ((size-j)>0x100) {
													/* 256 byte block + short header */
													memcpy(data+i,filedata+j,0x100);
													memcpy(data+i+0x100,cas_header,8);
													i+=0x108; j+=0x100;
												}
												else {
													/* last block, fill end with ^Z */
													memcpy(data+i,filedata+j,size-j);
													if ((size-j)<0x100) memset(data+i+(size-j),0x1a,0x100-(size-j));
													i+=0x100; size+=(j>>5);

													break;
												}
											}

											break;
										}

										case CAS_TYPE_CUS:
											memcpy(data,cas_header,8);
											memcpy(data+8,filedata,size);
											i=size+8;
											break;

										default: break;
									}

									/* set block */
									tape_od->blocks->type=type;
									tape_od->blocks->size=i;
									tape_od->blocks->raw=size;
									MEM_CREATE_N(tape_od->blocks->data,i);
									memcpy(tape_od->blocks->data,data,i);

									MEM_CLEAN(data);

									tape_recalc_crc(tape_od);
									tape_od->save_needed=TRUE;

									tape_draw_listbox(wnd);
								}
								else LOG_ERROR_WINDOW(dialog,"Couldn't read file!");

								MEM_CLEAN(filedata);
							}
							else LOG_ERROR_WINDOW(dialog,"File is too large!");
						}
						else LOG_ERROR_WINDOW(dialog,"Couldn't open file!");

						file_close_custom(fd);

						/* remember dir */
						if (strlen(fn)&&of.nFileOffset) {
							fn[of.nFileOffset]=0; strcpy(tapeblock_dir,fn);
						}
					}


					break;
				}

				/* save file */
				case IDM_MEDIATAPE_SAVE: {
					u8* data;
					int size=tape_od->blocks->raw;
					int err=FALSE;
					char fn[STRING_SIZE]={0};
					char t[STRING_SIZE]={0};
					const char* filter="All Files (*.*)\0*.*\0\0";
					OPENFILENAME of;

					/* block size 0 shouldn't happen */
					if (gray_save||tape_od->blocks->size==0||tape_od->blocks->raw==0) break;
					CLEAN_MENU();

					sprintf(t,"Save Tape Block %d As",tape_od->cur_block+1);

					MEM_CREATE_N(data,tape_od->blocks->size);

					/* copy data */
					switch (tape_od->blocks->type) {

						case CAS_TYPE_BIN:
							data[0]=0xfe; /* header */
							if (tape_od->blocks->size<=0x20) err=TRUE;
							else memcpy(data+1,tape_od->blocks->data+0x20,size-1);
							break;

						case CAS_TYPE_BAS:
							data[0]=0xff; /* header */
							if (tape_od->blocks->size<=0x20) err=TRUE;
							else {
								memcpy(data+1,tape_od->blocks->data+0x20,size-1);
								if (data[size-2]|data[size-1]) err=TRUE; /* make sure it ends with 00 00 */
							}
							break;

						case CAS_TYPE_ASC:
							if (tape_od->blocks->size<=0x20) err=TRUE;
							else {
								int bo=size+0x20;
								size=0; i=0x20;
								for (;;) {
									if ((bo-i)>0x100) {
										/* 256 bytes block */
										memcpy(data+size,tape_od->blocks->data+i,0x100);
										size+=0x100; i+=0x100;

										/* short header */
										if ((bo-i)>8&&!memcmp(tape_od->blocks->data+i,cas_header,8)) i+=8;
										else {
											/* error */
											err=TRUE;
											break;
										}
									}
									else {
										/* end, copy last part */
										memcpy(data+size,tape_od->blocks->data+i,bo-i);
										size+=(bo-i);
										if (data[size-1]!=0x1a) err=TRUE; /* make sure it ends with ^Z */

										break;
									}
								}
							}
							break;

						case CAS_TYPE_CUS:
							if (tape_od->blocks->size<=8) err=TRUE;
							else memcpy(data,tape_od->blocks->data+8,size);
							break;

						default: err=TRUE; break;
					}

					if (err) {
						LOG_ERROR_WINDOW(dialog,"Couldn't validate file type!");
						MEM_CLEAN(data);
						break;
					}

					/* set filename */
					if (tape_od->blocks->type!=CAS_TYPE_CUS) {
						memcpy(fn,tape_od->blocks->name,6);

						/* remove illegal characters */
						i=6;
						while (i--) if (fn[i]=='\\'||fn[i]=='/'||fn[i]==':'||fn[i]=='*'||fn[i]=='?'||fn[i]=='\"'||fn[i]=='<'||fn[i]=='>'||fn[i]=='|') fn[i]=' ';

						tape_set_name(fn);

						/* extension */
						if (strlen(fn)) {
							const char* ext[3]={".bin",".bas",".bas"};
							strcat(fn,ext[tape_od->blocks->type]);
						}
					}

					memset(&of,0,sizeof(OPENFILENAME));

					of.lStructSize=sizeof(OPENFILENAME);
					of.hwndOwner=dialog;
					of.hInstance=MAIN->module;
					of.lpstrFile=fn;
					of.lpstrFilter=filter;
					of.lpstrTitle=t;
					of.nMaxFile=STRING_SIZE;
					of.nFilterIndex=1;
					of.Flags=OFN_ENABLESIZING|OFN_HIDEREADONLY|OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST;
					of.lpstrInitialDir=strlen(tapeblock_dir)?tapeblock_dir:file->tooldir?file->tooldir:file->appdir;

					if (GetSaveFileName(&of)) {
						/* save */
						FILE* fd=NULL;

						if (!size||!strlen(fn)||!file_save_custom(&fd,fn)||!file_write_custom(fd,data,size)) {
							sprintf(t,"Couldn't save tape block %d!",tape_od->cur_block+1);
							LOG_ERROR_WINDOW(dialog,t);
						}
						file_close_custom(fd);

						/* remember dir */
						if (strlen(fn)&&of.nFileOffset) {
							fn[of.nFileOffset]=0; strcpy(tapeblock_dir,fn);
						}
					}

					MEM_CLEAN(data);

					break;
				}

				/* insert block */
				case IDM_MEDIATAPE_INSERT: {
					if (gray_insert) break;

					tape_insert_block(tape_od);
					tape_set_cur_block(tape_od->cur_block+1,tape_od);

					tape_draw_listbox(wnd);

					/* prevent refresh glitch when scrollbar appears */
					if (tape_get_blocks(tape_od)==(CAS_MAX_LISTBOX+1)) InvalidateRect(dialog,NULL,FALSE);

					break;
				}

				/* delete block */
				case IDM_MEDIATAPE_DELETE: {
					char t[0x100]={0};

					if (gray_delete) break;
					CLEAN_MENU();

					/* confirm */
					sprintf(t,"Are you sure you want to delete block %d?",tape_od->cur_block+1);
					if (MessageBox(dialog,t,"meisei",MB_ICONEXCLAMATION|MB_YESNO)==IDYES) {
						_tape_block* bd=tape_od->blocks;
						tape_od->save_needed|=(tape_od->blocks->type!=CAS_TYPE_NOP);

						if (tape_od->blocks->prev) tape_od->blocks->prev->next=tape_od->blocks->next;
						tape_od->blocks->next->prev=tape_od->blocks->prev;
						tape_od->blocks=tape_od->blocks->next;

						MEM_CLEAN(bd->data);
						MEM_CLEAN(bd);
						/* (b becomes invalid) */

						tape_recalc_crc(tape_od);

						tape_draw_listbox(wnd);
					}

					break;
				}

				/* rename */
				case IDM_MEDIATAPE_RENAME:
					if (gray_rename) break;
					CLEAN_MENU();
					DialogBox(MAIN->module,MAKEINTRESOURCE(IDD_MEDIATAPE_RENAME),dialog,(DLGPROC)tape_blockrename_dialog);
                break;

				/* info */
				case IDM_MEDIATAPE_INFO:
					if (gray_info) break;
					CLEAN_MENU();

					DialogBox(MAIN->module,MAKEINTRESOURCE(IDD_MEDIATAPE_INFO),dialog,(DLGPROC)tape_blockinfo_dialog);

					break;

				/* cancel */
				default: break;
			}

			PostMessage(dialog,WM_NEXTDLGCTL,(WPARAM)wnd,TRUE);

			CLEAN_MENU();
			#undef CLEAN_MENU

			break;
		}
		case WM_MOUSELEAVE:
			popup_rb_down=FALSE;
			break;

		default: break;
	}

	return CallWindowProc((WNDPROC)sub_listbox_prev,wnd,msg,wParam,lParam);
}

void tape_draw_listbox(HWND lb)
{
	char text[0x100];
	int i;

	if (!lb) return;

	/* empty */
	i=GetListBoxInfo(lb);
	while (i--) SendMessage(lb,LB_DELETESTRING,0,0);

	if (!tape_od||!tape_od->blocks) {
		tape_draw_pos(GetParent(lb));
		return;
	}

	tape_push(tape_od);
	tape_rewind(tape_od);

	/* fill */
	for (;;) {
		/* type */
		sprintf(text,"%s",block_description[tape_od->blocks->type]);

		/* filename */
		if (strlen(tape_od->blocks->name)) {
			strcat(text," \""); strcat(text,tape_od->blocks->name); strcat(text,"\"");
		}

		SendMessage(lb,LB_ADDSTRING,0,(LPARAM)(LPCTSTR)text);

		if (!tape_od->blocks->next) break;
		tape_od->blocks=tape_od->blocks->next;
	}

	tape_pop(tape_od);

	/* select */
	if (GetListBoxInfo(lb)>(DWORD)tape_od->cur_block) SendMessage(lb,LB_SETCURSEL,tape_od->cur_block,0);

	tape_draw_pos(GetParent(lb));
}

void tape_draw_pos(HWND dialog)
{
	POINT p;
	RECT r,rt;

	char tp[0x10]={0};
	char ts[0x10]={0};

	if (!dialog) return;

	if (!tape_od||!tape_od->blocks) {
		sprintf(tp," ");
		sprintf(ts," ");
	}
	else {
		if (tape_od->blocks->type==CAS_TYPE_END) {
			/* end of file, complete tape size */
			sprintf(tp,"EOF /");
			sprintf(ts,"%d",tape_get_size(tape_od));
		}
		else {
			/* tape position, block size */
			sprintf(tp,"%d /",tape_od->pos);
			sprintf(ts,"%d",tape_od->blocks->size);
		}
	}

	SetDlgItemText(dialog,IDC_MEDIA_TAPEPOS,tp);
	SetDlgItemText(dialog,IDC_MEDIA_TAPESIZE,ts);

	/* refresh */
	GetWindowRect(GetDlgItem(dialog,IDC_MEDIA_TAPEPOS),&r); CopyRect(&rt,&r);
	p.x=r.left; p.y=r.top; ScreenToClient(dialog,&p);
	GetWindowRect(GetDlgItem(dialog,IDC_MEDIA_TAPESIZE),&r);
	r.left=rt.left; r.top=rt.top;
	OffsetRect(&r,p.x-r.left,p.y-r.top);

	InvalidateRect(dialog,&r,FALSE);
}

void tape_log_info(void)
{
	/* on insert/eject */
	if (tape_cur) {
		char t[0x10]={0};
		if (tape_cur->blocks&&tape_cur->blocks->type<=CAS_TYPE_CUS) sprintf(t," (%s)",block_description[tape_cur->blocks->type]);
		LOG(LOG_MISC,"%stape inserted, block %d%s\n",(tape_cur->read_only&2)?"patched ":"",tape_cur->cur_block+1,t);
	}
	else LOG(LOG_MISC,"tape ejected\n");
}


/* BIOS hookshacks */
static void simulate_ctrlstop(void)
{
	/* BIOS BREAKX (046f, from 00b7) */
	io_write_ppic((io_read_ppic()&0xf0)|6); /* keyrow 6 */
	mapwrite[7](0xf3fa,mapread[7](0xf3f8)); mapwrite[7](0xf3fb,mapread[7](0xf3f9)); /* GETPNT=PUTPNT */
	mapwrite[7](0xfbe1,mapread[7](0xfbe1)&0xef); /* set STOP key in OLDKEY+7 */
	z80_set_a(0xd); mapwrite[7](0xf3f7,0xd); /* REPCNT */
	z80_set_f(z80_get_f()|1); /* scf */
}

static __fastcall void tape_write_block(int fi)
{
	if (tape_cur->blocks->type!=CAS_TYPE_NOP||tape_cur->pos==0) return;

	/* create new write block */
	tape_flush_cache(tape_cur);
	if (fi) tape_insert_block(tape_cur);

	runtime_block_inserted=TRUE;
}

void __fastcall tape_read_header(void)
{
	/* BIOS TAPION (1a63, from 00e1) */
	/* in reality, also changes some registers and LOWLIM(fca3) and WINWID(fca5) (and takes long, so _INT after return if on) */
	z80_di();
	io_write_ppicontrol(8); /* motor on */
	psg_write_address(0xe);

	if (!tape_cur||!tape_cur->blocks||tape_cur->blocks->type==CAS_TYPE_END) {
		simulate_ctrlstop();
		return;
	}

	tape_write_block(FALSE);

	if (tape_cur->pos&7) tape_cur->pos+=8;
	tape_cur->pos&=~7;

	/* find next header */
	for (;;) {
		if ((tape_cur->pos+8)<=tape_cur->blocks->size) {
			if (!memcmp(tape_cur->blocks->data+tape_cur->pos,cas_header,8)) break;
			tape_cur->pos+=8;
		}
		else {
			/* reached end of block */
			if (!tape_next_used(tape_cur)) {
				simulate_ctrlstop();
				return;
			}

			break;
		}
	}

	tape_cur->pos+=8;
	z80_set_f(z80_get_f()&0xfe); /* clear carry */

	/* end of block? */
	if (tape_cur->pos>=tape_cur->blocks->size) tape_next_used(tape_cur);
}

void __fastcall tape_read_byte(void)
{
	/* BIOS TAPIN (1abc, from 00e4) */
	/* in reality, also changes some registers */
	psg_read_data(); /* discard */

	if (!tape_cur||!tape_cur->blocks||tape_cur->blocks->type==CAS_TYPE_END) {
		z80_set_f(z80_get_f()|1); /* scf (denotes error) */
		return;
	}

	tape_write_block(FALSE);

	if (tape_cur->pos>=tape_cur->blocks->size) {
		/* reached end of block */
		if (!tape_next_used(tape_cur)) {
			z80_set_f(z80_get_f()|1); /* scf (denotes error) */
			return;
		}
	}

	z80_set_a(tape_cur->blocks->data[tape_cur->pos++]);
	z80_set_f(z80_get_f()&0xfe); /* clear carry */

	/* info */
	LOG(LOG_MISC|LOG_COLOUR(LC_GREEN)|LOG_TYPE(LT_TAPERW),"tape reading, block %d     ",tape_cur->cur_block+1);

	/* end of block? */
	if (tape_cur->pos==tape_cur->blocks->size) tape_next_used(tape_cur);
}

void __fastcall tape_write_header(void)
{
	/* BIOS TAPOON (19f1 from 00ea) */
	/* in reality, also changes some registers (and takes long, so _INT after return if on) */
	z80_di();
	io_write_ppicontrol(8); /* motor on */
	io_write_ppicontrol(0xa); /* low */

	if (!tape_cur||!tape_cur->blocks||tape_cur->blocks->type!=CAS_TYPE_NOP||tape_cur->pos>(CAS_MAX_BLOCK-8)) {
		simulate_ctrlstop();
		return;
	}

	if (tape_cur->pos>=0x18) {
		int t;
		memcpy(write_cache_block+tape_cur->pos,cas_header,8);
		t=tape_find_blocktype(write_cache_block+8);

		/* determine if it's a valid header position, ASCII header every 256 bytes */
		if (!(t==CAS_TYPE_ASC&&tape_cur->pos>=0x120&&((tape_cur->pos-0x120)%0x108)==0&&write_cache_block[tape_cur->pos-1]!=0x1a)&&(t==CAS_TYPE_CUS||tape_cur->pos>0x18)) tape_write_block(TRUE);
	}
	else tape_write_block(TRUE);

	memcpy(write_cache_block+tape_cur->pos,cas_header,8);
	tape_cur->pos+=8;
	z80_set_f(z80_get_f()&0xfe); /* clear carry */
}

void __fastcall tape_write_byte(void)
{
	/* BIOS TAPOUT (1a19 from 00ed) */
	/* in reality, also changes some registers */
	io_write_ppicontrol(0xa); /* low */

	if (!tape_cur||!tape_cur->blocks||tape_cur->blocks->type!=CAS_TYPE_NOP||tape_cur->pos>=CAS_MAX_BLOCK) {
		simulate_ctrlstop();
		return;
	}

	if (tape_cur->pos==0) {
		/* shouldn't normally happen */
		memcpy(write_cache_block,cas_header,8);
		tape_cur->pos=8;
	}

	/* info */
	LOG(LOG_MISC|LOG_COLOUR(LC_GREEN)|LOG_TYPE(LT_TAPERW),"tape writing, block %d     ",tape_cur->cur_block+1);

	write_cache_block[tape_cur->pos++]=z80_get_a();
	z80_set_f(z80_get_f()&0xfe); /* clear carry */
}


/* since meisei 1.2.2 */
/* state				size
crc						4
cur_block				3
cur_pos					3

==						10
*/
#define STATE_VERSION	1
#define STATE_SIZE		10

int __fastcall tape_state_get_version(void)
{
	return STATE_VERSION;
}

int __fastcall tape_state_get_size(void)
{
	return STATE_SIZE;
}

/* save */
void __fastcall tape_state_save(u8** s)
{
	if (tape_cur) {
		STATE_SAVE_4(tape_cur->crc);
		STATE_SAVE_3(tape_cur->cur_block);
		STATE_SAVE_3(tape_cur->pos);
	}
	else {
		STATE_SAVE_4(0);
		STATE_SAVE_3(0);
		STATE_SAVE_3(0);
	}
}

/* load */
void __fastcall tape_state_load_cur(u8** s)
{
	if (tape_cur) {
		u32 crc=0;

		STATE_LOAD_4(crc);

		if (crc==tape_cur->crc) {
			int b=0;
			int pos=0;

			STATE_LOAD_3(b);
			STATE_LOAD_3(pos);

			if (b!=tape_cur->cur_block) tape_set_cur_block(b,tape_cur);
			tape_cur->pos=pos;
		}
		else {
			/* skip block/pos */
			(*s)+=6;
		}
	}
	else {
		/* skip */
		(*s)+=STATE_SIZE;
	}
}

int __fastcall tape_state_load(int v,u8** s)
{
	switch (v) {
		case STATE_VERSION:
			tape_state_load_cur(s);
			break;

		default: return FALSE;
	}

	return TRUE;
}
