/******************************************************************************
 *                                                                            *
 *                                 "z80.c"                                    *
 *                                                                            *
 ******************************************************************************/

#define COBJMACROS
#include <io.h>

#include "global.h"
#include "z80.h"
#include "crystal.h"
#include "mapper.h"
#include "io.h"
#include "draw.h"
#include "state.h"
#include "file.h"
#include "input.h"
#include "cont.h"
#include "vdp.h"

/*

Tested on my MSXes (NEC, ZiLOG, tR), other people's MSXes, ZEXALL (and many MSX games).

Known models used in MSXes:
- NEC D780C-1
- SGS Z8400AB1   (uncommon, only seen it in the Philips VG8010 and FRAEL BRUC 100)
- SHARP LH0080A
- Toshiba T7937A (some 60Hz MSX1, Z80 inside this MSX engine) -- not tested yet
- Toshiba T9769A (some MSX2+, Z80 inside this MSX engine) -- not tested yet
- Toshiba T9769B (some MSX2+, Z80 inside this MSX engine)
- Toshiba T9769C (turboR, Z80 inside this MSX engine), R800 is separate and very different
- ZiLOG Z8400APS (Z80A) -- Z84C0004PSC is used too?

some differences found..

ZEXALL points: (it does not test 'ALL', btw)
- <daa,cpl,scf,ccf> fails (see below) on:
  + NEC    (crc:c4ab71f0)
  + T9769B (crc:bf563c6b)
  + T9769C (crc:7c56cfd2)

SCF/CCF X and Y flags:
- SGS/SHARP/ZiLOG:	(flags | A) & 0x28
- NEC:				flags & A & 0x28
- T9769B:			(flags & 0x28) | (A & 8) | (A & unknown & 0x20)
  Different programs with same input AF table show a different output F result for bit 5,
  this will need to be tested on more T9769Bs to see if they're the same.
  (for future reference, wouter's test2.com found crcs: scf:a7630d8a, ccf:03c234bd)
- T9769C:			(flags & 0x28) | (A & 8)

A few MSX models were seen to show a result of just (A & 0x28),
this will need to be looked into further. (Z80 brand not known)

People from the Speccy scene mention that SCF/CCF results are
inconsistant and may be influenced by I and R registers. Tests
on MSXes don't show any influence by I and R on most MSXes, though
consistency may be a problem too. (T9769B, and the above remark)

from Charles:
"The only confirmed difference between any Z80 CPU that I know of is this:
 A CMOS Z80 outputs $FF for the 'out (c), 0' instruction.
 A NMOS Z80 outputs $00 for the 'out (c), 0' instruction."

(Toshiba T9769B and C also output $ff with out (c), 0)

Not tested thoroughly on non-ZiLOG models:
 - WZ (aka "MEMPTR") behaviour, documented by Boo-boo (+tested well on ZiLOG/NEC)
 - P/V flag is reset if ld a,i or ld a,r is interrupted, even if IFF2 was set
   before this instruction. Behaviour tested on MSXes by GuyveR800.
   (known to be fixed on CMOS Z80 though, though it does happen on T9769B/C)

Power-on register contents: -- looks like it randomly differs per MSX
If cold booting an MSX1 to Basic (no ROM or diskdrive), IX, IY, AF', BC', DE', and HL'
are untouched. RH and I too, but those are always 0. Tested twice per MSX:
- ZiLOG: IX=AEBF IY=3778 AF'=9FEF BC'=FBFF DE'=F9AE HL'=B79E (Canon V-20)
- NEC:   IX=0001 IY=8408 AF'=08C0 BC'=4004 DE'=8441 HL'=C500 (CF2700)
- NEC(2):IX=AC21 IY=0018 AF'=EF15 BC'=4FF4 DE'=FF95 HL'=1465 (CF1200)

No NMI or multiple IRQ support here (unused in MSX1)
I emulate the ZiLOG model, likely most common in MSX(1) (NEC or SHARP is 2nd)

*/


#define Z_LOG_SPOP	0	/* enable custom debug logging on some opcodes, eg. ld b,b */

#define TR_RESET	0	/* trace from reset (only if Z80_TRACER is already on) */
#define TR_ADDINFO	1	/* tracer additional info (only if Z80_TRACER and tracer_on are already on) */
/* (Z80_TRACER is in z80.h) */

/* custom debug log shortcuts */
/* nothing */
#define ZL_NOP() printf("\n")

/* show registers */
#define ZL_SHOWREGS() printf(" AF(%04X) BC(%04X) DE(%04X) HL(%04X) IX(%04X) IY(%04X) SP(%04X) I(%02X,%cI)\n",AF,BC,DE,HL,IX,IY,SP,I,'D'+IFF1)

/* show VDP info */
#define ZL_SHOWVDP() \
	vdp_whereami(tracerpos); \
	printf(" VDP L(%s) A(%04X) S(%02X) R(%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X)\n",tracerpos,vdp_get_address(),vdp_get_status(),vdp_get_reg(0),vdp_get_reg(1),vdp_get_reg(2),vdp_get_reg(3),vdp_get_reg(4),vdp_get_reg(5),vdp_get_reg(6),vdp_get_reg(7))

/* profiler (not entirely accurate and has problems when reversing, but good enough for me) */
#define ZL_PROFILER(x) \
	printf(" P%X(%c) last(%I64d/%I64d) ",x,z80.profiler_toggle[x]?'E':'S',CCTOTAL-z80.cc_profiler[x],(CCTOTAL-z80.cc_profiler[x])/228); \
	if (z80.profiler_toggle[x]) { \
		if (z80.cc_profiler_min[x]==0||z80.cc_profiler_min[x]>(CCTOTAL-z80.cc_profiler[x])) z80.cc_profiler_min[x]=CCTOTAL-z80.cc_profiler[x]; \
		if (z80.cc_profiler_max[x]<(CCTOTAL-z80.cc_profiler[x])) z80.cc_profiler_max[x]=CCTOTAL-z80.cc_profiler[x]; \
	} \
	printf("min(%I64d/%I64d) max(%I64d/%I64d)\n",z80.cc_profiler_min[x],z80.cc_profiler_min[x]/228,z80.cc_profiler_max[x],z80.cc_profiler_max[x]/228); \
	z80.profiler_toggle[x]^=1; \
	z80.cc_profiler[x]=CCTOTAL+5 /* exclude ld r,r */

/* handlers */
#define ZL_LD_B_B	printf("B"); ZL_NOP();
#define ZL_LD_C_C	printf("C"); ZL_SHOWVDP();
#define ZL_LD_D_D	printf("D"); ZL_SHOWREGS();
#define ZL_LD_E_E	printf("E"); ZL_NOP();
#define ZL_LD_H_H	printf("H"); ZL_PROFILER(0);
#define ZL_LD_L_L	printf("L"); ZL_PROFILER(1);
#define ZL_LD_A_A	printf("A"); ZL_PROFILER(2);
#define ZL_DI_HALT	printf("di | halt"); ZL_NOP();


/* MSX Z80 timing */
#define MSX_EC		1	/* normal Z80: 0, MSX: 1 (1 extra cycle for each instruction byte) */
#define IO_EC		3	/* i/o extra cycles, assume rising edge of 2nd T-state (for accurate mid-opcode timing) */
#define W_EC		4	/* mem write extra cycles (for accurate mid-opcode timing) */


/* const opcode tables */
/* cycles */
static const u8 lut_cycles_main_raw[0x100]={
/*       0               1               2               3               4               5               6               7              8                9               A               B               C               D               E               F */
/* 0 */  4,              10,             7,              6,              4,              4,              7,              4,             4,               11,             7,              6,              4,              4,              7,              4,
/* 1 */  8,              10,             7,              6,              4,              4,              7,              4,             12,              11,             7,              6,              4,              4,              7,              4,
/* 2 */  7,              10,             16,             6,              4,              4,              7,              4,             7,               11,             16,             6,              4,              4,              7,              4,
/* 3 */  7,              10,             13,             6,              11,             11,             10,             4,             7,               11,             13,             6,              4,              4,              7,              4,
/* 4 */  4,              4,              4,              4,              4,              4,              7,              4,             4,               4,              4,              4,              4,              4,              7,              4,
/* 5 */  4,              4,              4,              4,              4,              4,              7,              4,             4,               4,              4,              4,              4,              4,              7,              4,
/* 6 */  4,              4,              4,              4,              4,              4,              7,              4,             4,               4,              4,              4,              4,              4,              7,              4,
/* 7 */  7,              7,              7,              7,              7,              7,              4,              7,             4,               4,              4,              4,              4,              4,              7,              4,
/* 8 */  4,              4,              4,              4,              4,              4,              7,              4,             4,               4,              4,              4,              4,              4,              7,              4,
/* 9 */  4,              4,              4,              4,              4,              4,              7,              4,             4,               4,              4,              4,              4,              4,              7,              4,
/* A */  4,              4,              4,              4,              4,              4,              7,              4,             4,               4,              4,              4,              4,              4,              7,              4,
/* B */  4,              4,              4,              4,              4,              4,              7,              4,             4,               4,              4,              4,              4,              4,              7,              4,
/* C */  5,              10,             10,             10,             10,             11,             7,              11,            5,               10,             10,             8+MSX_EC,       10,             17,             7,              11,
/* D */  5,              10,             10,             11-IO_EC,       10,             11,             7,              11,            5,               4,              10,             11-IO_EC,       10,             4,              7,              11,
/* E */  5,              10,             10,             19,             10,             11,             7,              11,            5,               4,              10,             4,              10,             0,              7,              11,
/* F */  5,              10,             10,             4,              10,             11,             7,              11,            5,               6,              10,             4,              10,             4,              7,              11
};

static const u8 lut_cycles_ed_raw[0x100]={
/*       0               1               2               3               4               5               6               7              8                9               A               B               C               D               E               F */
/* 0 */  8,              8,              8,              8,              8,              8,              8,              8,             8,               8,              8,              8,              8,              8,              8,              8,
/* 1 */  8,              8,              8,              8,              8,              8,              8,              8,             8,               8,              8,              8,              8,              8,              8,              8,
/* 2 */  8,              8,              8,              8,              8,              8,              8,              8,             8,               8,              8,              8,              8,              8,              8,              8,
/* 3 */  8,              8,              8,              8,              8,              8,              8,              8,             8,               8,              8,              8,              8,              8,              8,              8,
/* 4 */  12-IO_EC,       12-IO_EC,       15,             20,             8,              14,             8,              9,             12-IO_EC,        12-IO_EC,       15,             20,             8,              14,             8,              9,
/* 5 */  12-IO_EC,       12-IO_EC,       15,             20,             8,              14,             8,              9,             12-IO_EC,        12-IO_EC,       15,             20,             8,              14,             8,              9,
/* 6 */  12-IO_EC,       12-IO_EC,       15,             20,             8,              14,             8,              18,            12-IO_EC,        12-IO_EC,       15,             20,             8,              14,             8,              18,
/* 7 */  12-IO_EC,       12-IO_EC,       15,             20,             8,              14,             8,              8,             12-IO_EC,        12-IO_EC,       15,             20,             8,              14,             8,              8,
/* 8 */  8,              8,              8,              8,              8,              8,              8,              8,             8,               8,              8,              8,              8,              8,              8,              8,
/* 9 */  8,              8,              8,              8,              8,              8,              8,              8,             8,               8,              8,              8,              8,              8,              8,              8,
/* A */  16,             16,             16-(IO_EC+W_EC),16-IO_EC,       8,              8,              8,              8,             16,              16,             16-(IO_EC+W_EC),16-IO_EC,       8,              8,              8,              8,
/* B */  16,             16,             16-(IO_EC+W_EC),16-IO_EC,       8,              8,              8,              8,             16,              16,             16-(IO_EC+W_EC),16-IO_EC,       8,              8,              8,              8,
/* C */  8,              8,              8,              8,              8,              8,              8,              8,             8,               8,              8,              8,              8,              8,              8,              8,
/* D */  8,              8,              8,              8,              8,              8,              8,              8,             8,               8,              8,              8,              8,              8,              8,              8,
/* E */  8,              8,              8,              8,              8,              8,              8,              8,             8,               8,              8,              8,              8,              8,              8,              8,
/* F */  8,              8,              8,              8,              8,              8,              8,              8,             8,               8,              8,              8,              8,              8,              8,              8
};

#define LCM 0xff /* use lut_cycles_main instead */

static const u8 lut_cycles_ddfd_raw[0x100]={
/*       0               1               2               3               4               5               6               7              8                9               A               B               C               D               E               F */
/* 0 */  LCM,            LCM,            LCM,            LCM,            LCM,            LCM,            LCM,            LCM,           LCM,             LCM,            LCM,            LCM,            LCM,            LCM,            LCM,            LCM,
/* 1 */  LCM,            LCM,            LCM,            LCM,            LCM,            LCM,            LCM,            LCM,           LCM,             LCM,            LCM,            LCM,            LCM,            LCM,            LCM,            LCM,
/* 2 */  LCM,            LCM,            LCM,            LCM,            LCM,            LCM,            LCM,            LCM,           LCM,             LCM,            LCM,            LCM,            LCM,            LCM,            LCM,            LCM,
/* 3 */  LCM,            LCM,            LCM,            LCM,            19,             19,             15,             LCM,           LCM,             LCM,            LCM,            LCM,            LCM,            LCM,            LCM,            LCM,
/* 4 */  LCM,            LCM,            LCM,            LCM,            LCM,            LCM,            15,             LCM,           LCM,             LCM,            LCM,            LCM,            LCM,            LCM,            15,             LCM,
/* 5 */  LCM,            LCM,            LCM,            LCM,            LCM,            LCM,            15,             LCM,           LCM,             LCM,            LCM,            LCM,            LCM,            LCM,            15,             LCM,
/* 6 */  LCM,            LCM,            LCM,            LCM,            LCM,            LCM,            15,             LCM,           LCM,             LCM,            LCM,            LCM,            LCM,            LCM,            15,             LCM,
/* 7 */  15,             15,             15,             15,             15,             15,             LCM,            15,            LCM,             LCM,            LCM,            LCM,            LCM,            LCM,            15,             LCM,
/* 8 */  LCM,            LCM,            LCM,            LCM,            LCM,            LCM,            15,             LCM,           LCM,             LCM,            LCM,            LCM,            LCM,            LCM,            15,             LCM,
/* 9 */  LCM,            LCM,            LCM,            LCM,            LCM,            LCM,            15,             LCM,           LCM,             LCM,            LCM,            LCM,            LCM,            LCM,            15,             LCM,
/* A */  LCM,            LCM,            LCM,            LCM,            LCM,            LCM,            15,             LCM,           LCM,             LCM,            LCM,            LCM,            LCM,            LCM,            15,             LCM,
/* B */  LCM,            LCM,            LCM,            LCM,            LCM,            LCM,            15,             LCM,           LCM,             LCM,            LCM,            LCM,            LCM,            LCM,            15,             LCM,
/* C */  LCM,            LCM,            LCM,            LCM,            LCM,            LCM,            LCM,            LCM,           LCM,             LCM,            LCM,            19,             LCM,            LCM,            LCM,            LCM,
/* D */  LCM,            LCM,            LCM,            LCM,            LCM,            LCM,            LCM,            LCM,           LCM,             LCM,            LCM,            LCM,            LCM,            LCM,            LCM,            LCM,
/* E */  LCM,            LCM,            LCM,            LCM,            LCM,            LCM,            LCM,            LCM,           LCM,             LCM,            LCM,            LCM,            LCM,            LCM,            LCM,            LCM,
/* F */  LCM,            LCM,            LCM,            LCM,            LCM,            LCM,            LCM,            LCM,           LCM,             LCM,            LCM,            LCM,            LCM,            LCM,            LCM,            LCM
};


#if Z80_TRACER|Z_LOG_SPOP
/* mnemonics */
static const char* lut_mnemonics_main[0x100]={
/*       0               1               2               3               4               5               6               7              8                9               A               B               C               D               E               F */
/* 0 */ "nop",          "ld bc,A",      "ld (bc),a",    "inc bc",       "inc b",        "dec b",        "ld b,N",       "rlca",        "ex af,af'",     "add hl,bc",    "ld a,(bc)",    "dec bc",       "inc c",        "dec c",        "ld c,N",       "rrca",
/* 1 */ "djnz B",       "ld de,A",      "ld (de),a",    "inc de",       "inc d",        "dec d",        "ld d,N",       "rla",         "jr B",          "add hl,de",    "ld a,(de)",    "dec de",       "inc e",        "dec e",        "ld e,N",       "rra",
/* 2 */ "jr nz,B",      "ld hl,A",      "ld (A),hl",    "inc hl",       "inc h",        "dec h",        "ld h,N",       "daa",         "jr z,B",        "add hl,hl",    "ld hl,(A)",    "dec hl",       "inc l",        "dec l",        "ld l,N",       "cpl",
/* 3 */ "jr nc,B",      "ld sp,A",      "ld (A),a",     "inc sp",       "inc (hl)",     "dec (hl)",     "ld (hl),N",    "scf",         "jr c,B",        "add hl,sp",    "ld a,(A)",     "dec sp",       "inc a",        "dec a",        "ld a,N",       "ccf",
/* 4 */ "ld b,b",       "ld b,c",       "ld b,d",       "ld b,e",       "ld b,h",       "ld b,l",       "ld b,(hl)",    "ld b,a",      "ld c,b",        "ld c,c",       "ld c,d",       "ld c,e",       "ld c,h",       "ld c,l",       "ld c,(hl)",    "ld c,a",
/* 5 */ "ld d,b",       "ld d,c",       "ld d,d",       "ld d,e",       "ld d,h",       "ld d,l",       "ld d,(hl)",    "ld d,a",      "ld e,b",        "ld e,c",       "ld e,d",       "ld e,e",       "ld e,h",       "ld e,l",       "ld e,(hl)",    "ld e,a",
/* 6 */ "ld h,b",       "ld h,c",       "ld h,d",       "ld h,e",       "ld h,h",       "ld h,l",       "ld h,(hl)",    "ld h,a",      "ld l,b",        "ld l,c",       "ld l,d",       "ld l,e",       "ld l,h",       "ld l,l",       "ld l,(hl)",    "ld l,a",
/* 7 */ "ld (hl),b",    "ld (hl),c",    "ld (hl),d",    "ld (hl),e",    "ld (hl),h",    "ld (hl),l",    "halt",         "ld (hl),a",   "ld a,b",        "ld a,c",       "ld a,d",       "ld a,e",       "ld a,h",       "ld a,l",       "ld a,(hl)",    "ld a,a",
/* 8 */ "add a,b",      "add a,c",      "add a,d",      "add a,e",      "add a,h",      "add a,l",      "add a,(hl)",   "add a,a",     "adc a,b",       "adc a,c",      "adc a,d",      "adc a,e",      "adc a,h",      "adc a,l",      "adc a,(hl)",   "adc a,a",
/* 9 */ "sub b",        "sub c",        "sub d",        "sub e",        "sub h",        "sub l",        "sub (hl)",     "sub a",       "sbc a,b",       "sbc a,c",      "sbc a,d",      "sbc a,e",      "sbc a,h",      "sbc a,l",      "sbc a,(hl)",   "sbc a,a",
/* A */ "and b",        "and c",        "and d",        "and e",        "and h",        "and l",        "and (hl)",     "and a",       "xor b",         "xor c",        "xor d",        "xor e",        "xor h",        "xor l",        "xor (hl)",     "xor a",
/* B */ "or b",         "or c",         "or d",         "or e",         "or h",         "or l",         "or (hl)",      "or a",        "cp b",          "cp c",         "cp d",         "cp e",         "cp h",         "cp l",         "cp (hl)",      "cp a",
/* C */ "ret nz",       "pop bc",       "jp nz,A",      "jp A",         "call nz,A",    "push bc",      "add a,N",      "rst 0h",      "ret z",         "ret",          "jp z,A",       " ",            "call z,A",     "call A",       "adc a,N",      "rst 8h",
/* D */ "ret nc",       "pop de",       "jp nc,A",      "out (N),a",    "call nc,A",    "push de",      "sub N",        "rst 10h",     "ret c",         "exx",          "jp c,A",       "in a,(N)",     "call c,A",     " ",            "sbc a,N",      "rst 18h",
/* E */ "ret po",       "pop hl",       "jp po,A",      "ex (sp),hl",   "call po,A",    "push hl",      "and N",        "rst 20h",     "ret pe",        "jp (hl)",      "jp pe,A",      "ex de,hl",     "call pe,A",    " ",            "xor N",        "rst 28h",
/* F */ "ret p",        "pop af",       "jp p,A",       "di",           "call p,A",     "push af",      "or N",         "rst 30h",     "ret m",         "ld sp,hl",     "jp m,A",       "ei",           "call m,A",     " ",            "cp N",         "rst 38h"
};

static const char* lut_mnemonics_ddfd[0x100]={
/*       0               1               2               3               4               5               6               7              8                9               A               B               C               D               E               F */
/* 0 */ " ",            " ",            " ",            " ",            " ",            " ",            " ",            " ",           " ",             "add X,bc",     " ",            " ",            " ",            " ",            " ",            " ",
/* 1 */ " ",            " ",            " ",            " ",            " ",            " ",            " ",            " ",           " ",             "add X,de",     " ",            " ",            " ",            " ",            " ",            " ",
/* 2 */ " ",            "ld X,A",       "ld (A),X",     "inc X",        "inc Xh",       "dec Xh",       "ld Xh,N",      " ",           " ",             "add X,X",      "ld X,(A)",     "dec X",        "inc Xl",       "dec Xl",       "ld Xl,N",      " ",
/* 3 */ " ",            " ",            " ",            " ",            "inc (XD)",     "dec (XD)",     "ld (XD),N",    " ",           " ",             "add X,sp",     " ",            " ",            " ",            " ",            " ",            " ",
/* 4 */ " ",            " ",            " ",            " ",            "ld b,Xh",      "ld b,Xl",      "ld b,(XD)",    " ",           " ",             " ",            " ",            " ",            "ld c,Xh",      "ld c,Xl",      "ld c,(XD)",    " ",
/* 5 */ " ",            " ",            " ",            " ",            "ld d,Xh",      "ld d,Xl",      "ld d,(XD)",    " ",           " ",             " ",            " ",            " ",            "ld e,Xh",      "ld e,Xl",      "ld e,(XD)",    " ",
/* 6 */ "ld Xh,b",      "ld Xh,c",      "ld Xh,d",      "ld Xh,e",      "ld Xh,Xh",     "ld Xh,Xl",     "ld h,(XD)",    "ld Xh,a",     "ld Xl,b",       "ld Xl,c",      "ld Xl,d",      "ld Xl,e",      "ld Xl,Xh",     "ld Xl,Xl",     "ld l,(XD)",    "ld Xl,a",
/* 7 */ "ld (XD),b",    "ld (XD),c",    "ld (XD),d",    "ld (XD),e",    "ld (XD),h",    "ld (XD),l",    " ",            "ld (XD),a",   " ",             " ",            " ",            " ",            "ld a,Xh",      "ld a,Xl",      "ld a,(XD)",    " ",
/* 8 */ " ",            " ",            " ",            " ",            "add a,Xh",     "add a,Xl",     "add a,(XD)",   " ",           " ",             " ",            " ",            " ",            "adc a,Xh",     "adc a,Xl",     "adc a,(XD)",   " ",
/* 9 */ " ",            " ",            " ",            " ",            "sub Xh",       "sub Xl",       "sub (XD)",     " ",           " ",             " ",            " ",            " ",            "sbc a,Xh",     "sbc a,Xl",     "sbc a,(XD)",   " ",
/* A */ " ",            " ",            " ",            " ",            "and Xh",       "and Xl",       "and (XD)",     " ",           " ",             " ",            " ",            " ",            "xor Xh",       "xor Xl",       "xor (XD)",     " ",
/* B */ " ",            " ",            " ",            " ",            "or Xh",        "or Xl",        "or (XD)",      " ",           " ",             " ",            " ",            " ",            "cp Xh",        "cp Xl",        "cp (XD)",      " ",
/* C */ " ",            " ",            " ",            " ",            " ",            " ",            " ",            " ",           " ",             " ",            " ",            " ",            " ",            " ",            " ",            " ",
/* D */ " ",            " ",            " ",            " ",            " ",            " ",            " ",            " ",           " ",             " ",            " ",            " ",            " ",            " ",            " ",            " ",
/* E */ " ",            "pop X",        " ",            "ex (sp),X",    " ",            "push X",       " ",            " ",           " ",             "jp (X)",       " ",            " ",            " ",            " ",            " ",            " ",
/* F */ " ",            " ",            " ",            " ",            " ",            " ",            " ",            " ",           " ",             "ld sp,X",      " ",            " ",            " ",            " ",            " ",            " "
};

static const char* lut_mnemonics_cb[0x100]={
/*       0               1               2               3               4               5               6               7              8                9               A               B               C               D               E               F */
/* 0 */ "rlc b",        "rlc c",        "rlc d",        "rlc e",        "rlc h",        "rlc l",        "rlc (hl)",     "rlc a",        "rrc b",        "rrc c",        "rrc d",        "rrc e",        "rrc h",        "rrc l",        "rrc (hl)",     "rrc a",
/* 1 */ "rl b",         "rl c",         "rl d",         "rl e",         "rl h",         "rl l",         "rl (hl)",      "rl a",         "rr b",         "rr c",         "rr d",         "rr e",         "rr h",         "rr l",         "rr (hl)",      "rr a",
/* 2 */ "sla b",        "sla c",        "sla d",        "sla e",        "sla h",        "sla l",        "sla (hl)",     "sla a",        "sra b",        "sra c",        "sra d",        "sra e",        "sra h",        "sra l",        "sra (hl)",     "sra a",
/* 3 */ "sll b",        "sll c",        "sll d",        "sll e",        "sll h",        "sll l",        "sll (hl)",     "sll a",        "srl b",        "srl c",        "srl d",        "srl e",        "srl h",        "srl l",        "srl (hl)",     "srl a",
/* 4 */ "bit 0,b",      "bit 0,c",      "bit 0,d",      "bit 0,e",      "bit 0,h",      "bit 0,l",      "bit 0,(hl)",   "bit 0,a",      "bit 1,b",      "bit 1,c",      "bit 1,d",      "bit 1,e",      "bit 1,h",      "bit 1,l",      "bit 1,(hl)",   "bit 1,a",
/* 5 */ "bit 2,b",      "bit 2,c",      "bit 2,d",      "bit 2,e",      "bit 2,h",      "bit 2,l",      "bit 2,(hl)",   "bit 2,a",      "bit 3,b",      "bit 3,c",      "bit 3,d",      "bit 3,e",      "bit 3,h",      "bit 3,l",      "bit 3,(hl)",   "bit 3,a",
/* 6 */ "bit 4,b",      "bit 4,c",      "bit 4,d",      "bit 4,e",      "bit 4,h",      "bit 4,l",      "bit 4,(hl)",   "bit 4,a",      "bit 5,b",      "bit 5,c",      "bit 5,d",      "bit 5,e",      "bit 5,h",      "bit 5,l",      "bit 5,(hl)",   "bit 5,a",
/* 7 */ "bit 6,b",      "bit 6,c",      "bit 6,d",      "bit 6,e",      "bit 6,h",      "bit 6,l",      "bit 6,(hl)",   "bit 6,a",      "bit 7,b",      "bit 7,c",      "bit 7,d",      "bit 7,e",      "bit 7,h",      "bit 7,l",      "bit 7,(hl)",   "bit 7,a",
/* 8 */ "res 0,b",      "res 0,c",      "res 0,d",      "res 0,e",      "res 0,h",      "res 0,l",      "res 0,(hl)",   "res 0,a",      "res 1,b",      "res 1,c",      "res 1,d",      "res 1,e",      "res 1,h",      "res 1,l",      "res 1,(hl)",   "res 1,a",
/* 9 */ "res 2,b",      "res 2,c",      "res 2,d",      "res 2,e",      "res 2,h",      "res 2,l",      "res 2,(hl)",   "res 2,a",      "res 3,b",      "res 3,c",      "res 3,d",      "res 3,e",      "res 3,h",      "res 3,l",      "res 3,(hl)",   "res 3,a",
/* A */ "res 4,b",      "res 4,c",      "res 4,d",      "res 4,e",      "res 4,h",      "res 4,l",      "res 4,(hl)",   "res 4,a",      "res 5,b",      "res 5,c",      "res 5,d",      "res 5,e",      "res 5,h",      "res 5,l",      "res 5,(hl)",   "res 5,a",
/* B */ "res 6,b",      "res 6,c",      "res 6,d",      "res 6,e",      "res 6,h",      "res 6,l",      "res 6,(hl)",   "res 6,a",      "res 7,b",      "res 7,c",      "res 7,d",      "res 7,e",      "res 7,h",      "res 7,l",      "res 7,(hl)",   "res 7,a",
/* C */ "set 0,b",      "set 0,c",      "set 0,d",      "set 0,e",      "set 0,h",      "set 0,l",      "set 0,(hl)",   "set 0,a",      "set 1,b",      "set 1,c",      "set 1,d",      "set 1,e",      "set 1,h",      "set 1,l",      "set 1,(hl)",   "set 1,a",
/* D */ "set 2,b",      "set 2,c",      "set 2,d",      "set 2,e",      "set 2,h",      "set 2,l",      "set 2,(hl)",   "set 2,a",      "set 3,b",      "set 3,c",      "set 3,d",      "set 3,e",      "set 3,h",      "set 3,l",      "set 3,(hl)",   "set 3,a",
/* E */ "set 4,b",      "set 4,c",      "set 4,d",      "set 4,e",      "set 4,h",      "set 4,l",      "set 4,(hl)",   "set 4,a",      "set 5,b",      "set 5,c",      "set 5,d",      "set 5,e",      "set 5,h",      "set 5,l",      "set 5,(hl)",   "set 5,a",
/* F */ "set 6,b",      "set 6,c",      "set 6,d",      "set 6,e",      "set 6,h",      "set 6,l",      "set 6,(hl)",   "set 6,a",      "set 7,b",      "set 7,c",      "set 7,d",      "set 7,e",      "set 7,h",      "set 7,l",      "set 7,(hl)",   "set 7,a"
};

static const char* lut_mnemonics_ddfdcb[0x100]={
/*       0               1               2               3               4               5               6               7              8                9               A               B               C               D               E               F */
/* 0 */ "rlc (XD),b",   "rlc (XD),c",   "rlc (XD),d",   "rlc (XD),e",   "rlc (XD),h",   "rlc (XD),l",   "rlc (XD)",     "rlc (XD),a",   "rrc (XD),b",   "rrc (XD),c",   "rrc (XD),d",   "rrc (XD),e",   "rrc (XD),h",   "rrc (XD),l",   "rrc (XD)",     "rrc (XD),a",
/* 1 */ "rl (XD),b",    "rl (XD),c",    "rl (XD),d",    "rl (XD),e",    "rl (XD),h",    "rl (XD),l",    "rl (XD)",      "rl (XD),a",    "rr (XD),b",    "rr (XD),c",    "rr (XD),d",    "rr (XD),e",    "rr (XD),h",    "rr (XD),l",    "rr (XD)",      "rr (XD),a",
/* 2 */ "sla (XD),b",   "sla (XD),c",   "sla (XD),d",   "sla (XD),e",   "sla (XD),h",   "sla (XD),l",   "sla (XD)",     "sla (XD),a",   "sra (XD),b",   "sra (XD),c",   "sra (XD),d",   "sra (XD),e",   "sra (XD),h",   "sra (XD),l",   "sra (XD)",     "sra (XD),a",
/* 3 */ "sll (XD),b",   "sll (XD),c",   "sll (XD),d",   "sll (XD),e",   "sll (XD),h",   "sll (XD),l",   "sll (XD)",     "sll (XD),a",   "srl (XD),b",   "srl (XD),c",   "srl (XD),d",   "srl (XD),e",   "srl (XD),h",   "srl (XD),l",   "srl (XD)",     "srl (XD),a",
/* 4 */ "bit 0,(XD)",   "bit 0,(XD)",   "bit 0,(XD)",   "bit 0,(XD)",   "bit 0,(XD)",   "bit 0,(XD)",   "bit 0,(XD)",   "bit 0,(XD)",   "bit 1,(XD)",   "bit 1,(XD)",   "bit 1,(XD)",   "bit 1,(XD)",   "bit 1,(XD)",   "bit 1,(XD)",   "bit 1,(XD)",   "bit 1,(XD)",
/* 5 */ "bit 2,(XD)",   "bit 2,(XD)",   "bit 2,(XD)",   "bit 2,(XD)",   "bit 2,(XD)",   "bit 2,(XD)",   "bit 2,(XD)",   "bit 2,(XD)",   "bit 3,(XD)",   "bit 3,(XD)",   "bit 3,(XD)",   "bit 3,(XD)",   "bit 3,(XD)",   "bit 3,(XD)",   "bit 3,(XD)",   "bit 3,(XD)",
/* 6 */ "bit 4,(XD)",   "bit 4,(XD)",   "bit 4,(XD)",   "bit 4,(XD)",   "bit 4,(XD)",   "bit 4,(XD)",   "bit 4,(XD)",   "bit 4,(XD)",   "bit 5,(XD)",   "bit 5,(XD)",   "bit 5,(XD)",   "bit 5,(XD)",   "bit 5,(XD)",   "bit 5,(XD)",   "bit 5,(XD)",   "bit 5,(XD)",
/* 7 */ "bit 6,(XD)",   "bit 6,(XD)",   "bit 6,(XD)",   "bit 6,(XD)",   "bit 6,(XD)",   "bit 6,(XD)",   "bit 6,(XD)",   "bit 6,(XD)",   "bit 7,(XD)",   "bit 7,(XD)",   "bit 7,(XD)",   "bit 7,(XD)",   "bit 7,(XD)",   "bit 7,(XD)",   "bit 7,(XD)",   "bit 7,(XD)",
/* 8 */ "res 0,(XD),b", "res 0,(XD),c", "res 0,(XD),d", "res 0,(XD),e", "res 0,(XD),h", "res 0,(XD),l", "res 0,(XD)",   "res 0,(XD),a", "res 1,(XD),b", "res 1,(XD),c", "res 1,(XD),d", "res 1,(XD),e", "res 1,(XD),h", "res 1,(XD),l", "res 1,(XD)",   "res 1,(XD),a",
/* 9 */ "res 2,(XD),b", "res 2,(XD),c", "res 2,(XD),d", "res 2,(XD),e", "res 2,(XD),h", "res 2,(XD),l", "res 2,(XD)",   "res 2,(XD),a", "res 3,(XD),b", "res 3,(XD),c", "res 3,(XD),d", "res 3,(XD),e", "res 3,(XD),h", "res 3,(XD),l", "res 3,(XD)",   "res 3,(XD),a",
/* A */ "res 4,(XD),b", "res 4,(XD),c", "res 4,(XD),d", "res 4,(XD),e", "res 4,(XD),h", "res 4,(XD),l", "res 4,(XD)",   "res 4,(XD),a", "res 5,(XD),b", "res 5,(XD),c", "res 5,(XD),d", "res 5,(XD),e", "res 5,(XD),h", "res 5,(XD),l", "res 5,(XD)",   "res 5,(XD),a",
/* B */ "res 6,(XD),b", "res 6,(XD),c", "res 6,(XD),d", "res 6,(XD),e", "res 6,(XD),h", "res 6,(XD),l", "res 6,(XD)",   "res 6,(XD),a", "res 7,(XD),b", "res 7,(XD),c", "res 7,(XD),d", "res 7,(XD),e", "res 7,(XD),h", "res 7,(XD),l", "res 7,(XD)",   "res 7,(XD),a",
/* C */ "set 0,(XD),b", "set 0,(XD),c", "set 0,(XD),d", "set 0,(XD),e", "set 0,(XD),h", "set 0,(XD),l", "set 0,(XD)",   "set 0,(XD),a", "set 1,(XD),b", "set 1,(XD),c", "set 1,(XD),d", "set 1,(XD),e", "set 1,(XD),h", "set 1,(XD),l", "set 1,(XD)",   "set 1,(XD),a",
/* D */ "set 2,(XD),b", "set 2,(XD),c", "set 2,(XD),d", "set 2,(XD),e", "set 2,(XD),h", "set 2,(XD),l", "set 2,(XD)",   "set 2,(XD),a", "set 3,(XD),b", "set 3,(XD),c", "set 3,(XD),d", "set 3,(XD),e", "set 3,(XD),h", "set 3,(XD),l", "set 3,(XD)",   "set 3,(XD),a",
/* E */ "set 4,(XD),b", "set 4,(XD),c", "set 4,(XD),d", "set 4,(XD),e", "set 4,(XD),h", "set 4,(XD),l", "set 4,(XD)",   "set 4,(XD),a", "set 5,(XD),b", "set 5,(XD),c", "set 5,(XD),d", "set 5,(XD),e", "set 5,(XD),h", "set 5,(XD),l", "set 5,(XD)",   "set 5,(XD),a",
/* F */ "set 6,(XD),b", "set 6,(XD),c", "set 6,(XD),d", "set 6,(XD),e", "set 6,(XD),h", "set 6,(XD),l", "set 6,(XD)",   "set 6,(XD),a", "set 7,(XD),b", "set 7,(XD),c", "set 7,(XD),d", "set 7,(XD),e", "set 7,(XD),h", "set 7,(XD),l", "set 7,(XD)",   "set 7,(XD),a"
};

static const char* lut_mnemonics_ed[0x100]={
/*       0               1               2               3               4               5               6               7              8                9               A               B               C               D               E               F */
/* 0 */ "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",
/* 1 */ "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",
/* 2 */ "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",
/* 3 */ "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",
/* 4 */ "in b,(c)",     "out (c),b",    "sbc hl,bc",    "ld (A),bc",    "neg",          "retn",         "im 0",         "ld i,a",       "in c,(c)",     "out (c),c",    "adc hl,bc",    "ld bc,(A)",    "neg",          "reti",         "im 0",         "ld r,a",
/* 5 */ "in d,(c)",     "out (c),d",    "sbc hl,de",    "ld (A),de",    "neg",          "retn",         "im 1",         "ld a,i",       "in e,(c)",     "out (c),e",    "adc hl,de",    "ld de,(A)",    "neg",          "reti",         "im 2",         "ld a,r",
/* 6 */ "in h,(c)",     "out (c),h",    "sbc hl,hl",    "ld (A),hl",    "neg",          "retn",         "im 0",         "rrd",          "in l,(c)",     "out (c),l",    "adc hl,hl",    "ld hl,(A)",    "neg",          "reti",         "im 0",         "rld",
/* 7 */ "in f,(c)",     "out (c),0",    "sbc hl,sp",    "ld (A),sp",    "neg",          "retn",         "im 1",         "ed nop",       "in a,(c)",     "out (c),a",    "adc hl,sp",    "ld sp,(A)",    "neg",          "reti",         "im 2",         "ed nop",
/* 8 */ "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",
/* 9 */ "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",
/* A */ "ldi",          "cpi",          "ini",          "outi",         "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ldd",          "cpd",          "ind",          "outd",         "ed nop",       "ed nop",       "ed nop",       "ed nop",
/* B */ "ldir",         "cpir",         "inir",         "otir",         "ed nop",       "ed nop",       "ed nop",       "ed nop",       "lddr",         "cpdr",         "indr",         "otdr",         "ed nop",       "ed nop",       "ed nop",       "ed nop",
/* C */ "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",
/* D */ "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",
/* E */ "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",
/* F */ "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop",       "ed nop"
};

static char tracermsg[0x100]={0};
static char tracerpos[0x10]={0};
static int tracer_additional_info=TR_ADDINFO;
static int tracer_prev_int=0;
#endif /* Z80_TRACER|Z_LOG_SPOP */
static FILE* tracer_fd=NULL;
static int tracer_on=0;


static struct {
	/* p* = padding for 32-bit alignment */

	union {
		struct {
			u16 pc,p0;								/* programcounter */
			u16 sp,p1;								/* stackpointer */
			u16 af,p2,bc,p3,de,p4,hl,p5,wz,p6;		/* register pairs */
			u16 afs,p7,bcs,p8,des,p9,hls,pa,wzs,pb;	/* secondary set */
			u16 i[4];								/* index registers: 0=x, 2=y */
		} w;

		/* their 8-bit parts */
		struct {
			u8 pc_low,pc_high,p0[2];
			u8 sp_low,sp_high,p1[2];
			u8 f,a,p2[2],c,b,p3[2],e,d,p4[2],l,h,p5[2],z,w,p6[2];
			u8 fs,as,p7[2],cs,bs,p8[2],es,ds,p9[2],ls,hs,pa[2],zs,ws,pb[2];
			u8 i[8];								/* 0=xlow, 1=xhigh, 4=ylow, 5=yhigh */
		} b;
	} reg;

	u32 rh,rl;										/* memory refresh register (8 bit) */
	int i;											/* interrupt vector register (8 bit) */
	int iff1,iff2;									/* interrupt flip-flops (1/0) */
	int irq_ack;									/* irq acknowledge request (1/0) */
	int irq_next_cc;								/* pending irq cycles */
	int im;											/* interrupt mode (0,1,2) */
	int halt;										/* halt state (1/0) */
	int busrq;										/* bus request (1/0) */
	int busack;										/* bus acknowledge (1/0) */

	int op;
	int cycles;

	/* used in debug only */
	/* INT64 gives compiler warning for printf, it's for debug-only, so I don't care */
	int profiler_toggle[0x10];
	INT64 cc_profiler[0x10];
	INT64 cc_profiler_min[0x10];
	INT64 cc_profiler_max[0x10];
	INT64 cycles_total;
	int cycles_prev;

} z80;


/* registers */
/* register pairs */
#define A			z80.reg.b.a
#define F			z80.reg.b.f
#define B			z80.reg.b.b
#define C			z80.reg.b.c
#define D			z80.reg.b.d
#define E			z80.reg.b.e
#define H			z80.reg.b.h
#define L			z80.reg.b.l
#define W			z80.reg.b.w
#define Z			z80.reg.b.z

#define AF			z80.reg.w.af
#define BC			z80.reg.w.bc
#define DE			z80.reg.w.de
#define HL			z80.reg.w.hl
#define WZ			z80.reg.w.wz

/* register pairs secondary set */
#define AS			z80.reg.b.as
#define FS			z80.reg.b.fs
#define BS			z80.reg.b.bs
#define CS			z80.reg.b.cs
#define DS			z80.reg.b.ds
#define ES			z80.reg.b.es
#define HS			z80.reg.b.hs
#define LS			z80.reg.b.ls
#define WS			z80.reg.b.ws
#define ZS			z80.reg.b.zs

#define AFS			z80.reg.w.afs
#define BCS			z80.reg.w.bcs
#define DES			z80.reg.w.des
#define HLS			z80.reg.w.hls
#define WZS			z80.reg.w.wzs

/* index registers */
#define IX			z80.reg.w.i[0]
#define IXL			z80.reg.b.i[0]
#define IXH			z80.reg.b.i[1]
#define IY			z80.reg.w.i[2]
#define IYL			z80.reg.b.i[4]
#define IYH			z80.reg.b.i[5]

#define IXY			z80.reg.w.i[i]
#define IXYL		z80.reg.b.i[i<<1]
#define IXYH		z80.reg.b.i[i<<1|1]

/* misc registers */
#define PC			z80.reg.w.pc
#define SP			z80.reg.w.sp
#define R			z80.rl
#define RH			z80.rh
#define I			z80.i

#define IFF1		z80.iff1
#define IFF2		z80.iff2
#define IM			z80.im
#define HALT		z80.halt

/* cycles */
#define CC			z80.cycles
#define CCPREV		z80.cycles_prev
#define CCTOTAL		z80.cycles_total

/* pins */
#define IRQNEXT		z80.irq_next_cc
#define IRQACK		z80.irq_ack

#define _IRQ		(CC<IRQNEXT)
#define _BUSRQ		z80.busrq
#define _BUSACK		z80.busack

/* flags shortcuts */
static u8 lut_flags[0x100]; /* szy0xp00 */
#define FT			lut_flags
#define SF			(F&0x80)
#define ZF			(F&0x40)
#define YF			(F&0x20)
#define HF			(F&0x10)
#define XF			(F&8)
#define PF			(F&4)
#define VF			(F&4)
#define NF			(F&2)
#define CF			(F&1)

static int lut_cycles_main[0x100];
static int lut_cycles_ed[0x100];
static int lut_cycles_ddfd[0x100];

enum { EC_JRc_i=0, EC_CALLc_i, EC_RETc_i, EC_BLOCK_i, EC_IO_i, EC_IO_MW_i, EC_CBm_i, EC_CBmb_i, EC_CBib_i, EC_INT_i, EC_INT2_i, EC_MAX_i };
static const u8 lut_cycles_ec_raw[EC_MAX_i]={ 5, 7, 6, 5, IO_EC, IO_EC+W_EC, 7, 4, 3, 13, 19 };
static int lut_cycles_ec[EC_MAX_i];

#define EC_JRc		lut_cycles_ec[EC_JRc_i]		/* extra cycles on conditional jr */
#define EC_CALLc	lut_cycles_ec[EC_CALLc_i]	/* conditional call */
#define EC_RETc		lut_cycles_ec[EC_RETc_i]	/* conditional ret */
#define EC_BLOCK	lut_cycles_ec[EC_BLOCK_i]	/* block transfer */
#define EC_IO		lut_cycles_ec[EC_IO_i]		/* i/o (mid-op) */
#define EC_IO_MW	lut_cycles_ec[EC_IO_MW_i]	/* i/o + mem write (mid-op) */
#define EC_CBm		lut_cycles_ec[EC_CBm_i]		/* cb prefix mem opcodes (shortcut) */
#define EC_CBmb		lut_cycles_ec[EC_CBmb_i]	/* cb prefix bit mem opcode (shortcut) */
#define EC_CBib		lut_cycles_ec[EC_CBib_i]	/* ddfd cb bit opcodes (shortcut) */
#define EC_INT		lut_cycles_ec[EC_INT_i]		/* interrupt (im 0/1) */
#define EC_INT2		lut_cycles_ec[EC_INT2_i]	/* interrupt (im 2) */

/* memory shortcuts */
#define R8(a)		mapread[(a)>>13&7](a)
#define R16(a)		R8((a)+1)<<8|R8(a)
#define W8(a,v)		mapwrite[(a)>>13&7](a,v)
#define W16(a,v)	W8(a,(v)&0xff); W8((a)+1,((v)>>8))

#define POP()		R16(SP); SP+=2
#define PUSH(v)		SP-=2; W16(SP,v)

/* instruction shortcuts */
/* main arithmetic */
#define ADD(v)		u8 d=v,r=A+d; F=(FT[r]&0xf8)|(A>r)|((r^A^d)&0x10)|(((A^r)&(d^r))>>5&4); A=r
#define ADC(v)		u8 d=v; u16 r=A+d+(F&1); F=(FT[r&0xff]&0xf8)|(r>>8|((r^A^d)&0x10))|(((A^r)&(d^r))>>5&4); A=r

#define SUBF(m)		(FT[r]&m)|(A<r)|((r^A^d)&0x10)|(((A^d)&(A^r))>>5&4)|2
#define SUB(v)		u8 d=v,r=A-d; F=SUBF(0xf8); A=r
#define CP(v)		u8 d=v,r=A-d; F=SUBF(0xd0)|(d&0x28)
#define SBC(v)		u8 d=v; u16 r=A-d-(F&1); F=(FT[r&0xff]&0xf8)|(r>>8&1)|((r^A^d)&0x10)|(((A^d)&(A^r))>>5&4)|2; A=r

#define ADD16(d,v)	u16 r=d+v; WZ=d+1; F=(F&0xc4)|(r>>8&0x28)|(d>r)|((r^d^v)>>8&0x10); d=r

#define INCF(v,o)	(F&1)|(FT[r]&0xf8)|((v^r)&0x10)|(r==o)<<2
#define INCr(v)		u8 r=v+1; F=INCF(v,0x80); v=r
#define DECr(v)		u8 r=v-1; F=INCF(v,0x7f)|2; v=r
#define INCm(a)		u8 d=R8(a),r=d+1; F=INCF(d,0x80); W8(a,r)
#define DECm(a)		u8 d=R8(a),r=d-1; F=INCF(d,0x7f)|2; W8(a,r)

#define AND(v)		F=FT[A&=(v)]|0x10
#define OR(v)		F=FT[A|=(v)]
#define XOR(v)		F=FT[A^=(v)]

/* main jump */
#define JPc(c)		WZ=R16(PC); if (c) PC=WZ; else PC+=2
#define JR()		WZ=PC=PC+1+(s8)R8(PC)
#define JRc(c)		if (c) { JR(); CC-=EC_JRc; } else PC++

#define CALLc(c)	WZ=R16(PC); PC+=2; if (c) { PUSH(PC); PC=WZ; CC-=EC_CALLc; }
#define RETc(c)		if (c) { WZ=PC=POP(); CC-=EC_RETc; }
#define RST(a)		PUSH(PC); WZ=PC=a

/* main misc */
#define EX(a,b)		ex=a; a=b; b=ex

/* cb rotate */
#define RLC(r)		r=r<<1|r>>7; F=FT[r]|(r&1)
#define RRC(r)		F=r&1; F|=FT[r=r>>1|r<<7]
#define RL2(r)		c=r>>7; F=c|FT[r=r<<1|(F&1)]
#define RL(r)		int RL2(r)
#define RR2(r)		c=r&1; F=c|FT[r=r>>1|F<<7]
#define RR(r)		int RR2(r)

#define SLA(r)		F=r>>7; F|=FT[r<<=1]
#define SRA(r)		F=r&1; F|=FT[r=r>>1|(r&0x80)]
#define SLL(r)		F=r>>7; F|=FT[r=r<<1|1]
#define SRL(r)		F=r&1; F|=FT[r>>=1]

/* cb bit */
#define BITs		(1<<(op>>3&7))
#define BITF(r)		(F&1)|(FT[r&BITs]&0xc4)|0x10
#define BITo(r)		F=BITF(r)|(r&0x28)
#define RES(r)		r&=~BITs
#define SET(r)		r|=BITs

/* ed block */
#define BLOCK(c)	if (c) { PC-=2; CC-=EC_BLOCK; }
#define LDF			(F&0xc1)|(BC!=0)<<2|(v&8)|(v<<4&0x20)
#define LDI()		u8 v=R8(HL); W8(DE,v); DE++; v+=A; HL++; BC--; F=LDF
#define LDD()		u8 v=R8(HL); W8(DE,v); DE--; v+=A; HL--; BC--; F=LDF

#define CPF()		F=(F&1)|(FT[r]&0xc0)|((r^A^d)&0x10)|(BC!=0)<<2|2; r-=(F>>4&1); F|=(r&8)|(r<<4&0x20)
#define CPI()		u8 d=R8(HL),r=A-d; HL++; BC--; CPF()
#define CPD()		u8 d=R8(HL),r=A-d; HL--; BC--; CPF()

/* ed arithmetic */
#define ADC16(v)	u32 r=HL+v+(F&1); F=r>>16|((r^HL^v)>>8&0x10)|(r>>8&0xa8)|((r&0xffff)==0)<<6|(((HL^r)&(v^r))>>13&4); WZ=HL+1; HL=r
#define SBC16(v)	u32 r=HL-v-(F&1); F=(r>>16&1)|((r^HL^v)>>8&0x10)|(r>>8&0xa8)|((r&0xffff)==0)<<6|(((HL^v)&(HL^r))>>13&4)|2; WZ=HL+1; HL=r

/* ed i/o */
#define INc(r)		WZ=BC+1; F=(F&1)|FT[r=ioread[C]()]; CC-=EC_IO
#define OUTc(r)		WZ=BC+1; iowrite[C](r); CC-=EC_IO

#define IOF()		F=(FT[B]&0xf8)|(FT[(f&7)^B]&4)|f>>8|(f>>4&0x10)|(r>>6&2)
#define INI()		u8 r=ioread[C](); u16 f=r+((C+1)&0xff); WZ=BC+1; CC-=EC_IO_MW; W8(HL,r); HL++; B--; IOF()
#define IND()		u8 r=ioread[C](); u16 f=r+((C-1)&0xff); WZ=BC-1; CC-=EC_IO_MW; W8(HL,r); HL--; B--; IOF()
#define OUTI()		u8 r=R8(HL); u16 f; iowrite[C](r); CC-=EC_IO; HL++; f=r+L; B--; WZ=BC+1; IOF()
#define OUTD()		u8 r=R8(HL); u16 f; iowrite[C](r); CC-=EC_IO; HL--; f=r+L; B--; WZ=BC-1; IOF()

/* dd/fd main misc */
#define IA_NC()		u16 a=WZ=IXY+(s8)R8(PC)
#define IA()		IA_NC(); PC++

/* dd/fd+cb rotate */
#define RLCI(r)		r=R8(a); RLC(r); W8(a,r)
#define RRCI(r)		r=R8(a); RRC(r); W8(a,r)
#define RLI(r)		int c; r=R8(a); RL2(r); W8(a,r)
#define RRI(r)		int c; r=R8(a); RR2(r); W8(a,r)

#define SLAI(r)		r=R8(a); SLA(r); W8(a,r)
#define SRAI(r)		r=R8(a); SRA(r); W8(a,r)
#define SLLI(r)		r=R8(a); SLL(r); W8(a,r)
#define SRLI(r)		r=R8(a); SRL(r); W8(a,r)

/* dd/fd+cb bit */
#define RESI(r)		W8(a,r=R8(a)&~BITs)
#define SETI(r)		W8(a,r=R8(a)|BITs)

/* etc */
#define OP			z80.op


void z80_execute(int border)
{
	int op=OP;
#if Z80_TRACER|Z_LOG_SPOP
	int tr_i,tr_j;

	u16 tr_pc;		/* program counter */
	int tr_op[5];	/* opcode */
	int tr_opi;		/* opcode index */
	char tr_m[0x10];/* opcode mnemonic */

	u16 tr_opc;		/* opcode start (program counter) */
	int tr_d=0;		/* opcode data displacement */
	u16 tr_a=0;		/* opcode data address */
	u8 tr_n=0;		/* opcode data constant */
#endif

	while(CC>=border) {

	switch (op) {

	/* 8-bit load */
	case 0x41: B=C; break;													/* ld b,c */
	case 0x42: B=D; break;													/* ld b,d */
	case 0x43: B=E; break;													/* ld b,e */
	case 0x44: B=H; break;													/* ld b,h */
	case 0x45: B=L; break;													/* ld b,l */
	case 0x46: B=R8(HL); break;												/* ld b,(hl) */
	case 0x47: B=A; break;													/* ld b,a */

	case 0x48: C=B; break;													/* ld c,b */
	case 0x4a: C=D; break;													/* ld c,d */
	case 0x4b: C=E; break;													/* ld c,e */
	case 0x4c: C=H; break;													/* ld c,h */
	case 0x4d: C=L; break;													/* ld c,l */
	case 0x4e: C=R8(HL); break;												/* ld c,(hl) */
	case 0x4f: C=A; break;													/* ld c,a */

	case 0x50: D=B; break;													/* ld d,b */
	case 0x51: D=C; break;													/* ld d,c */
	case 0x53: D=E; break;													/* ld d,e */
	case 0x54: D=H; break;													/* ld d,h */
	case 0x55: D=L; break;													/* ld d,l */
	case 0x56: D=R8(HL); break;												/* ld d,(hl) */
	case 0x57: D=A; break;													/* ld d,a */

	case 0x58: E=B; break;													/* ld e,b */
	case 0x59: E=C; break;													/* ld e,c */
	case 0x5a: E=D; break;													/* ld e,d */
	case 0x5c: E=H; break;													/* ld e,h */
	case 0x5d: E=L; break;													/* ld e,l */
	case 0x5e: E=R8(HL); break;												/* ld e,(hl) */
	case 0x5f: E=A; break;													/* ld e,a */

	case 0x60: H=B; break;													/* ld h,b */
	case 0x61: H=C; break;													/* ld h,c */
	case 0x62: H=D; break;													/* ld h,d */
	case 0x63: H=E; break;													/* ld h,e */
	case 0x65: H=L; break;													/* ld h,l */
	case 0x66: H=R8(HL); break;												/* ld h,(hl) */
	case 0x67: H=A; break;													/* ld h,a */

	case 0x68: L=B; break;													/* ld l,b */
	case 0x69: L=C; break;													/* ld l,c */
	case 0x6a: L=D; break;													/* ld l,d */
	case 0x6b: L=E; break;													/* ld l,e */
	case 0x6c: L=H; break;													/* ld l,h */
	case 0x6e: L=R8(HL); break;												/* ld l,(hl) */
	case 0x6f: L=A; break;													/* ld l,a */

	case 0x70: W8(HL,B); break;												/* ld (hl),b */
	case 0x71: W8(HL,C); break;												/* ld (hl),c */
	case 0x72: W8(HL,D); break;												/* ld (hl),d */
	case 0x73: W8(HL,E); break;												/* ld (hl),e */
	case 0x74: W8(HL,H); break;												/* ld (hl),h */
	case 0x75: W8(HL,L); break;												/* ld (hl),l */
	case 0x77: W8(HL,A); break;												/* ld (hl),a */

	case 0x78: A=B; break;													/* ld a,b */
	case 0x79: A=C; break;													/* ld a,c */
	case 0x7a: A=D; break;													/* ld a,d */
	case 0x7b: A=E; break;													/* ld a,e */
	case 0x7c: A=H; break;													/* ld a,h */
	case 0x7d: A=L; break;													/* ld a,l */
	case 0x7e: A=R8(HL); break;												/* ld a,(hl) */

	case 0x06: B=R8(PC); PC++; break;										/* ld b,n */
	case 0x0e: C=R8(PC); PC++; break;										/* ld c,n */
	case 0x16: D=R8(PC); PC++; break;										/* ld d,n */
	case 0x1e: E=R8(PC); PC++; break;										/* ld e,n */
	case 0x26: H=R8(PC); PC++; break;										/* ld h,n */
	case 0x2e: L=R8(PC); PC++; break;										/* ld l,n */
	case 0x36: W8(HL,R8(PC)); PC++; break;									/* ld (hl),n */
	case 0x3e: A=R8(PC); PC++; break;										/* ld a,n */

	case 0x0a: A=R8(BC); WZ=BC+1; break;									/* ld a,(bc) */
	case 0x1a: A=R8(DE); WZ=DE+1; break;									/* ld a,(de) */
	case 0x3a: WZ=R16(PC); A=R8(WZ); WZ++; PC+=2; break;					/* ld a,(nn) */

	case 0x02: W8(BC,A); Z=BC+1; W=A; break;								/* ld (bc),a */
	case 0x12: W8(DE,A); Z=DE+1; W=A; break;								/* ld (de),a */
	case 0x32: WZ=R16(PC); W8(WZ,A); WZ++; W=A; PC+=2; break;				/* ld (nn),a */

	/* 16-bit load */
	case 0x01: BC=R16(PC); PC+=2; break;									/* ld bc,nn */
	case 0x11: DE=R16(PC); PC+=2; break;									/* ld de,nn */
	case 0x21: HL=R16(PC); PC+=2; break;									/* ld hl,nn */
	case 0x31: SP=R16(PC); PC+=2; break;									/* ld sp,nn */

	case 0x22: WZ=R16(PC); W16(WZ,HL); WZ++; PC+=2; break;					/* ld (nn),hl */
	case 0x2a: WZ=R16(PC); HL=R16(WZ); WZ++; PC+=2; break;					/* ld hl,(nn) */

	case 0xf9: SP=HL; break;												/* ld sp,hl */

	case 0xc5: PUSH(BC); break;												/* push bc */
	case 0xd5: PUSH(DE); break;												/* push de */
	case 0xe5: PUSH(HL); break;												/* push hl */
	case 0xf5: PUSH(AF); break;												/* push af */

	case 0xc1: BC=POP(); break;												/* pop bc */
	case 0xd1: DE=POP(); break;												/* pop de */
	case 0xe1: HL=POP(); break;												/* pop hl */
	case 0xf1: AF=POP(); break;												/* pop af */

	/* exchange */
	case 0xeb: { int EX(DE,HL); break; }									/* ex de,hl */
	case 0x08: { int EX(AF,AFS); break; }									/* ex af,af' */
	case 0xd9: { int EX(BC,BCS); EX(DE,DES); EX(HL,HLS); break; }			/* exx */
	case 0xe3: WZ=R16(SP); W16(SP,HL); HL=WZ; break;						/* ex (sp),hl */

	/* 8-bit arithmetic */
	case 0x80: { ADD(B); break; }											/* add a,b */
	case 0x81: { ADD(C); break; }											/* add a,c */
	case 0x82: { ADD(D); break; }											/* add a,d */
	case 0x83: { ADD(E); break; }											/* add a,e */
	case 0x84: { ADD(H); break; }											/* add a,h */
	case 0x85: { ADD(L); break; }											/* add a,l */
	case 0x86: { ADD(R8(HL)); break; }										/* add a,(hl) */
	case 0x87: { ADD(A); break; }											/* add a,a */

	case 0x88: { ADC(B); break; }											/* adc a,b */
	case 0x89: { ADC(C); break; }											/* adc a,c */
	case 0x8a: { ADC(D); break; }											/* adc a,d */
	case 0x8b: { ADC(E); break; }											/* adc a,e */
	case 0x8c: { ADC(H); break; }											/* adc a,h */
	case 0x8d: { ADC(L); break; }											/* adc a,l */
	case 0x8e: { ADC(R8(HL)); break; }										/* adc a,(hl) */
	case 0x8f: { ADC(A); break; }											/* adc a,a */

	case 0xc6: { ADD(R8(PC)); PC++; break; }								/* add a,n */
	case 0xce: { ADC(R8(PC)); PC++; break; }								/* adc a,n */

	case 0x90: { SUB(B); break; }											/* sub b */
	case 0x91: { SUB(C); break; }											/* sub c */
	case 0x92: { SUB(D); break; }											/* sub d */
	case 0x93: { SUB(E); break; }											/* sub e */
	case 0x94: { SUB(H); break; }											/* sub h */
	case 0x95: { SUB(L); break; }											/* sub l */
	case 0x96: { SUB(R8(HL)); break; }										/* sub (hl) */
	case 0x97: { SUB(A); break; }											/* sub a */

	case 0x98: { SBC(B); break; }											/* sbc a,b */
	case 0x99: { SBC(C); break; }											/* sbc a,c */
	case 0x9a: { SBC(D); break; }											/* sbc a,d */
	case 0x9b: { SBC(E); break; }											/* sbc a,e */
	case 0x9c: { SBC(H); break; }											/* sbc a,h */
	case 0x9d: { SBC(L); break; }											/* sbc a,l */
	case 0x9e: { SBC(R8(HL)); break; }										/* sbc a,(hl) */
	case 0x9f: { SBC(A); break; }											/* sbc a,a */

	case 0xd6: { SUB(R8(PC)); PC++; break; }								/* sub a,n */
	case 0xde: { SBC(R8(PC)); PC++; break; }								/* sbc a,n */

	case 0x04: { INCr(B); break; }											/* inc b */
	case 0x0c: { INCr(C); break; }											/* inc c */
	case 0x14: { INCr(D); break; }											/* inc d */
	case 0x1c: { INCr(E); break; }											/* inc e */
	case 0x24: { INCr(H); break; }											/* inc h */
	case 0x2c: { INCr(L); break; }											/* inc l */
	case 0x34: { INCm(HL); break; }											/* inc (hl) */
	case 0x3c: { INCr(A); break; }											/* inc a */

	case 0x05: { DECr(B); break; }											/* dec b */
	case 0x0d: { DECr(C); break; }											/* dec c */
	case 0x15: { DECr(D); break; }											/* dec d */
	case 0x1d: { DECr(E); break; }											/* dec e */
	case 0x25: { DECr(H); break; }											/* dec h */
	case 0x2d: { DECr(L); break; }											/* dec l */
	case 0x35: { DECm(HL); break; }											/* dec (hl) */
	case 0x3d: { DECr(A); break; }											/* dec a */

	case 0xb8: { CP(B); break; }											/* cp b */
	case 0xb9: { CP(C); break; }											/* cp c */
	case 0xba: { CP(D); break; }											/* cp d */
	case 0xbb: { CP(E); break; }											/* cp e */
	case 0xbc: { CP(H); break; }											/* cp h */
	case 0xbd: { CP(L); break; }											/* cp l */
	case 0xbe: { CP(R8(HL)); break; }										/* cp (hl) */
	case 0xbf: { CP(A); break; }											/* cp a */

	case 0xfe: { CP(R8(PC)); PC++; break; }									/* cp n */

	case 0xa0: AND(B); break;												/* and b */
	case 0xa1: AND(C); break;												/* and c */
	case 0xa2: AND(D); break;												/* and d */
	case 0xa3: AND(E); break;												/* and e */
	case 0xa4: AND(H); break;												/* and h */
	case 0xa5: AND(L); break;												/* and l */
	case 0xa6: AND(R8(HL)); break;											/* and (hl) */
	case 0xa7: AND(A); break;												/* and a */

	case 0xe6: AND(R8(PC)); PC++; break;									/* and n */

	case 0xa8: XOR(B); break;												/* xor b */
	case 0xa9: XOR(C); break;												/* xor c */
	case 0xaa: XOR(D); break;												/* xor d */
	case 0xab: XOR(E); break;												/* xor e */
	case 0xac: XOR(H); break;												/* xor h */
	case 0xad: XOR(L); break;												/* xor l */
	case 0xae: XOR(R8(HL)); break;											/* xor (hl) */
	case 0xaf: XOR(A); break;												/* xor a */

	case 0xee: XOR(R8(PC)); PC++; break;									/* xor n */

	case 0xb0: OR(B); break;												/* or b */
	case 0xb1: OR(C); break;												/* or c */
	case 0xb2: OR(D); break;												/* or d */
	case 0xb3: OR(E); break;												/* or e */
	case 0xb4: OR(H); break;												/* or h */
	case 0xb5: OR(L); break;												/* or l */
	case 0xb6: OR(R8(HL)); break;											/* or (hl) */
	case 0xb7: OR(A); break;												/* or a */

	case 0xf6: OR(R8(PC)); PC++; break;										/* or n */

	/* 16-bit arithmetic */
	case 0x09: { ADD16(HL,BC); break; }										/* add hl,bc */
	case 0x19: { ADD16(HL,DE); break; }										/* add hl,de */
	case 0x29: { ADD16(HL,HL); break; }										/* add hl,hl */
	case 0x39: { ADD16(HL,SP); break; }										/* add hl,sp */

	case 0x03: BC++; break;													/* inc bc */
	case 0x13: DE++; break;													/* inc de */
	case 0x23: HL++; break;													/* inc hl */
	case 0x33: SP++; break;													/* inc sp */

	case 0x0b: BC--; break;													/* dec bc */
	case 0x1b: DE--; break;													/* dec de */
	case 0x2b: HL--; break;													/* dec hl */
	case 0x3b: SP--; break;													/* dec sp */

	/* general purpose arithmetic/cpu control */
	/* optimised DAA implementation from http://wikiti.brandonw.net/?title=Z80_Instruction_Set */
	case 0x27: {															/* daa */
		u8 r=A;
		if NF {
			if (HF|((A&0xf)>9)) r-=6;
			if (CF|(A>0x99)) r-=0x60;
		}
		else {
			if (HF|((A&0xf)>9)) r+=6;
			if (CF|(A>0x99)) r+=0x60;
		}

		F=(F&3)|(A>0x99)|((A^r)&0x10)|FT[r]; A=r;
		break;
	}
	case 0x2f: F=(F&0xc5)|((A=~A)&0x28)|0x12; break;						/* cpl */

	case 0x37: F=(F&0xec)|(A&0x28)|1; break;								/* scf */
	case 0x3f: F=((F&0xed)^1)|(F<<4&0x10)|(A&0x28); break;					/* ccf */

	case 0x76:																/* halt */
		PC--;
#if Z_LOG_SPOP
		if (HALT==0&&IFF1==0&&IFF2==0) { ZL_DI_HALT }
#endif
		HALT=1;
		break;

	case 0xf3: IFF1=IFF2=0; break;											/* di */
	case 0xfb:
		IFF1=IFF2=1;														/* ei */
#if Z80_TRACER|Z_LOG_SPOP
		tr_opc=PC-1;
		if (!tracer_prev_int) {
			CCTOTAL+=((CCPREV-CC)/crystal->cycle);
			if (tracer_on) { vdp_whereami(tracerpos); fprintf(tracer_fd,"%04X: FB          ei                 BC:%04X DE:%04X HL:%04X AF:%04X(%c%c%c%c%c%c%c%c) SP:%04X IX:%04X IY:%04X R:%02X I:%02X/%d%c%c c:%s(%d,%I64d)\n",tr_opc,BC,DE,HL,AF,SF?'S':'.',ZF?'Z':'.',YF?'Y':'.',HF?'H':'.',XF?'X':'.',PF?'P':'.',NF?'N':'.',CF?'C':'.',SP,IX,IY,(R&0x7f)|RH,I,IM,IFF1?'1':'.',IFF2?'2':'.',tracerpos,(CCPREV-CC)/crystal->cycle,CCTOTAL); }
			CCPREV=CC;
		}
		else tracer_prev_int=FALSE;
#endif
		op=R8(PC); PC++; R++;
		CC-=lut_cycles_main[op];
		continue; /* skip interrupt routine */

	/* rotate */
	case 0x07: F=(F&0xc4)|((A=A<<1|A>>7)&0x29); break;						/* rlca */
	case 0x0f: { int c=A&1; F=c|(F&0xc4)|((A=A>>1|A<<7)&0x28); break; }		/* rrca */
	case 0x17: { int c=A>>7; F=c|(F&0xc4)|((A=A<<1|(F&1))&0x28); break; }	/* rla */
	case 0x1f: { int c=A&1; F=c|(F&0xc4)|((A=A>>1|F<<7)&0x28); break; }		/* rra */

	/* jump */
	case 0xc3: WZ=PC=R16(PC); break;										/* jp nn */
	case 0xe9: PC=HL; break;												/* jp (hl) */

	case 0xc2: JPc(!ZF); break;												/* jp nz,nn */
	case 0xca: JPc(ZF); break;												/* jp z,nn */
	case 0xd2: JPc(!CF); break;												/* jp nc,nn */
	case 0xda: JPc(CF); break;												/* jp c,nn */
	case 0xe2: JPc(!PF); break;												/* jp po,nn */
	case 0xea: JPc(PF); break;												/* jp pe,nn */
	case 0xf2: JPc(!SF); break;												/* jp p,nn */
	case 0xfa: JPc(SF); break;												/* jp m,nn */

	case 0x18: JR(); break;													/* jr dis */

	case 0x20: JRc(!ZF); break;												/* jr nz,dis */
	case 0x28: JRc(ZF); break;												/* jr z,dis */
	case 0x30: JRc(!CF); break;												/* jr nc,dis */
	case 0x38: JRc(CF); break;												/* jr c,dis */

	case 0x10: B--; JRc(B); break;											/* djnz dis */

	/* call/return */
	case 0xcd: WZ=R16(PC); PC+=2; PUSH(PC); PC=WZ; break;					/* call nn */

	case 0xc4: CALLc(!ZF) break;											/* call nz,nn */
	case 0xcc: CALLc(ZF) break;												/* call z,nn */
	case 0xd4: CALLc(!CF) break;											/* call nc,nn */
	case 0xdc: CALLc(CF) break;												/* call c,nn */
	case 0xe4: CALLc(!PF) break;											/* call po,nn */
	case 0xec: CALLc(PF) break;												/* call pe,nn */
	case 0xf4: CALLc(!SF) break;											/* call p,nn */
	case 0xfc: CALLc(SF) break;												/* call m,nn */

	case 0xc9: WZ=PC=POP(); break;											/* ret */

	case 0xc0: RETc(!ZF) break;												/* ret nz */
	case 0xc8: RETc(ZF) break;												/* ret z */
	case 0xd0: RETc(!CF) break;												/* ret nc */
	case 0xd8: RETc(CF) break;												/* ret c */
	case 0xe0: RETc(!PF) break;												/* ret po */
	case 0xe8: RETc(PF) break;												/* ret pe */
	case 0xf0: RETc(!SF) break;												/* ret p */
	case 0xf8: RETc(SF) break;												/* ret m */

	case 0xc7: case 0xcf: case 0xd7: case 0xdf: case 0xe7: case 0xef: case 0xf7: case 0xff:
		RST(op&0x38); break;												/* rst n */

	/* i/o */
	case 0xd3: iowrite[Z=R8(PC)](A); W=A; Z++; PC++; CC-=EC_IO; break;		/* out (n),a */
	case 0xdb: WZ=A<<8|R8(PC); A=ioread[Z](); WZ++; PC++; CC-=EC_IO; break;	/* in a,(n) */


	/* cb */
	case 0xcb:
		op=R8(PC); PC++; R++;

		switch (op) {

	/* rotate/shift */
	case 0x00: RLC(B); break;												/* rlc b */
	case 0x01: RLC(C); break;												/* rlc c */
	case 0x02: RLC(D); break;												/* rlc d */
	case 0x03: RLC(E); break;												/* rlc e */
	case 0x04: RLC(H); break;												/* rlc h */
	case 0x05: RLC(L); break;												/* rlc l */
	case 0x06: { u8 v=R8(HL); RLC(v); CC-=EC_CBm; W8(HL,v); break; }		/* rlc (hl) */
	case 0x07: RLC(A); break;												/* rlc a */

	case 0x08: RRC(B); break;												/* rrc b */
	case 0x09: RRC(C); break;												/* rrc c */
	case 0x0a: RRC(D); break;												/* rrc d */
	case 0x0b: RRC(E); break;												/* rrc e */
	case 0x0c: RRC(H); break;												/* rrc h */
	case 0x0d: RRC(L); break;												/* rrc l */
	case 0x0e: { u8 v=R8(HL); RRC(v); CC-=EC_CBm; W8(HL,v); break; }		/* rrc (hl) */
	case 0x0f: RRC(A); break;												/* rrc a */

	case 0x10: { RL(B); break; }											/* rl b */
	case 0x11: { RL(C); break; }											/* rl c */
	case 0x12: { RL(D); break; }											/* rl d */
	case 0x13: { RL(E); break; }											/* rl e */
	case 0x14: { RL(H); break; }											/* rl h */
	case 0x15: { RL(L); break; }											/* rl l */
	case 0x16: { u8 v=R8(HL); RL(v); CC-=EC_CBm; W8(HL,v); break; }			/* rl (hl) */
	case 0x17: { RL(A); break; }											/* rl a */

	case 0x18: { RR(B); break; }											/* rr b */
	case 0x19: { RR(C); break; }											/* rr c */
	case 0x1a: { RR(D); break; }											/* rr d */
	case 0x1b: { RR(E); break; }											/* rr e */
	case 0x1c: { RR(H); break; }											/* rr h */
	case 0x1d: { RR(L); break; }											/* rr l */
	case 0x1e: { u8 v=R8(HL); RR(v); CC-=EC_CBm; W8(HL,v); break; }			/* rr (hl) */
	case 0x1f: { RR(A); break; }											/* rr a */

	case 0x20: SLA(B); break;												/* sla b */
	case 0x21: SLA(C); break;												/* sla c */
	case 0x22: SLA(D); break;												/* sla d */
	case 0x23: SLA(E); break;												/* sla e */
	case 0x24: SLA(H); break;												/* sla h */
	case 0x25: SLA(L); break;												/* sla l */
	case 0x26: { u8 v=R8(HL); SLA(v); CC-=EC_CBm; W8(HL,v); break; }		/* sla (hl) */
	case 0x27: SLA(A); break;												/* sla a */

	case 0x28: SRA(B); break;												/* sra b */
	case 0x29: SRA(C); break;												/* sra c */
	case 0x2a: SRA(D); break;												/* sra d */
	case 0x2b: SRA(E); break;												/* sra e */
	case 0x2c: SRA(H); break;												/* sra h */
	case 0x2d: SRA(L); break;												/* sra l */
	case 0x2e: { u8 v=R8(HL); SRA(v); CC-=EC_CBm; W8(HL,v); break; }		/* sra (hl) */
	case 0x2f: SRA(A); break;												/* sra a */

	case 0x30: SLL(B); break;												/* sll b */
	case 0x31: SLL(C); break;												/* sll c */
	case 0x32: SLL(D); break;												/* sll d */
	case 0x33: SLL(E); break;												/* sll e */
	case 0x34: SLL(H); break;												/* sll h */
	case 0x35: SLL(L); break;												/* sll l */
	case 0x36: { u8 v=R8(HL); SLL(v); CC-=EC_CBm; W8(HL,v); break; }		/* sll (hl) */
	case 0x37: SLL(A); break;												/* sll a */

	case 0x38: SRL(B); break;												/* srl b */
	case 0x39: SRL(C); break;												/* srl c */
	case 0x3a: SRL(D); break;												/* srl d */
	case 0x3b: SRL(E); break;												/* srl e */
	case 0x3c: SRL(H); break;												/* srl h */
	case 0x3d: SRL(L); break;												/* srl l */
	case 0x3e: { u8 v=R8(HL); SRL(v); CC-=EC_CBm; W8(HL,v); break; }		/* srl (hl) */
	case 0x3f: SRL(A); break;												/* srl a */

	/* bit */
	case 0x40: case 0x48: case 0x50: case 0x58: case 0x60: case 0x68: case 0x70: case 0x78:
		BITo(B); break;														/* bit n,b */
	case 0x41: case 0x49: case 0x51: case 0x59: case 0x61: case 0x69: case 0x71: case 0x79:
		BITo(C); break;														/* bit n,c */
	case 0x42: case 0x4a: case 0x52: case 0x5a: case 0x62: case 0x6a: case 0x72: case 0x7a:
		BITo(D); break;														/* bit n,d */
	case 0x43: case 0x4b: case 0x53: case 0x5b: case 0x63: case 0x6b: case 0x73: case 0x7b:
		BITo(E); break;														/* bit n,e */
	case 0x44: case 0x4c: case 0x54: case 0x5c: case 0x64: case 0x6c: case 0x74: case 0x7c:
		BITo(H); break;														/* bit n,h */
	case 0x45: case 0x4d: case 0x55: case 0x5d: case 0x65: case 0x6d: case 0x75: case 0x7d:
		BITo(L); break;														/* bit n,l */
	case 0x46: case 0x4e: case 0x56: case 0x5e: case 0x66: case 0x6e: case 0x76: case 0x7e:
		CC-=EC_CBmb; F=(BITF(R8(HL)))|(W&0x28); break;						/* bit n,(hl) */
	case 0x47: case 0x4f: case 0x57: case 0x5f: case 0x67: case 0x6f: case 0x77: case 0x7f:
		BITo(A); break;														/* bit n,a */

	case 0x80: case 0x88: case 0x90: case 0x98: case 0xa0: case 0xa8: case 0xb0: case 0xb8:
		RES(B); break;														/* res n,b */
	case 0x81: case 0x89: case 0x91: case 0x99: case 0xa1: case 0xa9: case 0xb1: case 0xb9:
		RES(C); break;														/* res n,c */
	case 0x82: case 0x8a: case 0x92: case 0x9a: case 0xa2: case 0xaa: case 0xb2: case 0xba:
		RES(D); break;														/* res n,d */
	case 0x83: case 0x8b: case 0x93: case 0x9b: case 0xa3: case 0xab: case 0xb3: case 0xbb:
		RES(E); break;														/* res n,e */
	case 0x84: case 0x8c: case 0x94: case 0x9c: case 0xa4: case 0xac: case 0xb4: case 0xbc:
		RES(H); break;														/* res n,h */
	case 0x85: case 0x8d: case 0x95: case 0x9d: case 0xa5: case 0xad: case 0xb5: case 0xbd:
		RES(L); break;														/* res n,l */
	case 0x86: case 0x8e: case 0x96: case 0x9e: case 0xa6: case 0xae: case 0xb6: case 0xbe:
		CC-=EC_CBm; W8(HL,R8(HL)&~BITs); break;								/* res n,(hl) */
	case 0x87: case 0x8f: case 0x97: case 0x9f: case 0xa7: case 0xaf: case 0xb7: case 0xbf:
		RES(A); break;														/* res n,a */

	case 0xc0: case 0xc8: case 0xd0: case 0xd8: case 0xe0: case 0xe8: case 0xf0: case 0xf8:
		SET(B); break;														/* set n,b */
	case 0xc1: case 0xc9: case 0xd1: case 0xd9: case 0xe1: case 0xe9: case 0xf1: case 0xf9:
		SET(C); break;														/* set n,c */
	case 0xc2: case 0xca: case 0xd2: case 0xda: case 0xe2: case 0xea: case 0xf2: case 0xfa:
		SET(D); break;														/* set n,d */
	case 0xc3: case 0xcb: case 0xd3: case 0xdb: case 0xe3: case 0xeb: case 0xf3: case 0xfb:
		SET(E); break;														/* set n,e */
	case 0xc4: case 0xcc: case 0xd4: case 0xdc: case 0xe4: case 0xec: case 0xf4: case 0xfc:
		SET(H); break;														/* set n,h */
	case 0xc5: case 0xcd: case 0xd5: case 0xdd: case 0xe5: case 0xed: case 0xf5: case 0xfd:
		SET(L); break;														/* set n,l */
	case 0xc6: case 0xce: case 0xd6: case 0xde: case 0xe6: case 0xee: case 0xf6: case 0xfe:
		CC-=EC_CBm; W8(HL,R8(HL)|BITs); break;								/* set n,(hl) */
	case 0xc7: case 0xcf: case 0xd7: case 0xdf: case 0xe7: case 0xef: case 0xf7: case 0xff:
		SET(A); break;														/* set n,a */

		} /* cb switch */
		break;


	/* ed */
	case 0xed:
		op=R8(PC); PC++; R++;
		CC-=lut_cycles_ed[op];

		switch (op) {

	/* 8-bit load */
	case 0x47: I=A; break;													/* ld i,a */
	case 0x4f: R=A; RH=A&0x80; break;										/* ld r,a */

	case 0x57: F=((F&1)|IFF2<<2|(FT[A=I]&0xf8))&~((IFF1&_IRQ)<<2); break;	/* ld a,i */
	case 0x5f: F=((F&1)|IFF2<<2|(FT[A=(R&0x7f)|RH]&0xf8))&~((IFF1&_IRQ)<<2); break; /* ld a,r */

	/* 16-bit load */
	case 0x43: WZ=R16(PC); W16(WZ,BC); WZ++; PC+=2; break;					/* ld (nn),bc */
	case 0x53: WZ=R16(PC); W16(WZ,DE); WZ++; PC+=2; break;					/* ld (nn),de */
	case 0x63: WZ=R16(PC); W16(WZ,HL); WZ++; PC+=2; break;					/* ld (nn),hl (dup) */
	case 0x73: WZ=R16(PC); W16(WZ,SP); WZ++; PC+=2; break;					/* ld (nn),sp */

	case 0x4b: WZ=R16(PC); BC=R16(WZ); WZ++; PC+=2; break;					/* ld bc,(nn) */
	case 0x5b: WZ=R16(PC); DE=R16(WZ); WZ++; PC+=2; break;					/* ld de,(nn) */
	case 0x6b: WZ=R16(PC); HL=R16(WZ); WZ++; PC+=2; break;					/* ld hl,(nn) (dup) */
	case 0x7b: WZ=R16(PC); SP=R16(WZ); WZ++; PC+=2; break;					/* ld sp,(nn) */

	/* block transfer */
	case 0xa0: { LDI(); break; }											/* ldi */
	case 0xa8: { LDD(); break; }											/* ldd */
	case 0xb0: { LDI(); if (BC) WZ=PC; BLOCK(BC) break; }					/* ldir */
	case 0xb8: { LDD(); if (BC) WZ=PC; BLOCK(BC) break; }					/* lddr */

	case 0xa1: { CPI(); WZ++; break; }										/* cpi */
	case 0xa9: { CPD(); WZ--; break; }										/* cpd */
	case 0xb1: { CPI(); if (!BC||d==A) WZ++; else WZ=PC; BLOCK(!ZF&&BC) break; } /* cpir */
	case 0xb9: { CPD(); if (!BC||d==A) WZ--; else WZ=PC; BLOCK(!ZF&&BC) break; } /* cpdr */

	/* 16-bit arithmetic */
	case 0x42: { SBC16(BC); break; }										/* sbc hl,bc */
	case 0x52: { SBC16(DE); break; }										/* sbc hl,de */
	case 0x62: { SBC16(HL); break; }										/* sbc hl,hl */
	case 0x72: { SBC16(SP); break; }										/* sbc hl,sp */

	case 0x4a: { ADC16(BC); break; }										/* adc hl,bc */
	case 0x5a: { ADC16(DE); break; }										/* adc hl,de */
	case 0x6a: { ADC16(HL); break; }										/* adc hl,hl */
	case 0x7a: { ADC16(SP); break; }										/* adc hl,sp */

	/* general purpose arithmetic/cpu control */
	case 0x44: case 0x4c: case 0x54: case 0x5c: case 0x64: case 0x6c: case 0x74: case 0x7c:
		{ u8 r=0-A; F=((r^A)&0x10)|(FT[r]&0xf8)|(r!=0)|(r==0x80)<<2|2; A=r; break; } /* neg */

	case 0x46: case 0x4e: case 0x66: case 0x6e: IM=0; break;				/* im 0 */
	case 0x56: case 0x76: IM=1; break;										/* im 1 */
	case 0x5e: case 0x7e: IM=2; break;										/* im 2 */

	/* rotate */
	case 0x67: {															/* rrd */
		int v=R8(HL); WZ=HL+1; W8(HL,v>>4|(A<<4&0xf0));
		F=(F&1)|FT[A=(A&0xf0)|(v&0xf)];
		break;
	}
	case 0x6f: {															/* rld */
		int v=R8(HL); WZ=HL+1; W8(HL,v<<4|(A&0xf));
		F=(F&1)|FT[A=(A&0xf0)|(v>>4&0xf)];
		break;
	}

	/* return */
	case 0x45: case 0x4d: case 0x55: case 0x5d: case 0x65: case 0x6d: case 0x75: case 0x7d:
		WZ=PC=POP(); IFF1=IFF2; break;										/* reti/retn */

	/* i/o */
	case 0x40: INc(B); break;												/* in b,(c) */
	case 0x48: INc(C); break;												/* in c,(c) */
	case 0x50: INc(D); break;												/* in d,(c) */
	case 0x58: INc(E); break;												/* in e,(c) */
	case 0x60: INc(H); break;												/* in h,(c) */
	case 0x68: INc(L); break;												/* in l,(c) */
	case 0x70: WZ=BC+1; F=(F&1)|FT[ioread[C]()]; CC-=EC_IO; break;			/* in f,(c) */
	case 0x78: INc(A); break;												/* in a,(c) */

	case 0x41: OUTc(B); break;												/* out (c),b */
	case 0x49: OUTc(C); break;												/* out (c),c */
	case 0x51: OUTc(D); break;												/* out (c),d */
	case 0x59: OUTc(E); break;												/* out (c),e */
	case 0x61: OUTc(H); break;												/* out (c),h */
	case 0x69: OUTc(L); break;												/* out (c),l */
	case 0x71: OUTc(0); break;												/* out (c),0 */
	case 0x79: OUTc(A); break;												/* out (c),a */

	case 0xa2: { INI(); break; }											/* ini */
	case 0xaa: { IND(); break; }											/* ind */
	case 0xb2: { INI(); BLOCK(B) break; }									/* inir */
	case 0xba: { IND(); BLOCK(B) break; }									/* indr */

	case 0xa3: { OUTI(); break; }											/* outi */
	case 0xab: { OUTD(); break; }											/* outd */
	case 0xb3: { OUTI(); BLOCK(B) break; }									/* otir */
	case 0xbb: { OUTD(); BLOCK(B) break; }									/* otdr */

	/* rest */
	default: break;															/* nop 'illegal' */

		} /* ed switch */
		break;


	/* dd/fd */
	case 0xfd: op^=2;
	case 0xdd: {
		int i=op&2;
		op=R8(PC); PC++; R++;
		CC-=lut_cycles_ddfd[op];

		switch (op) {

	/* 8-bit load */
	case 0x44: B=IXYH; break;												/* ld b,ixyh */
	case 0x4c: C=IXYH; break;												/* ld c,ixyh */
	case 0x54: D=IXYH; break;												/* ld d,ixyh */
	case 0x5c: E=IXYH; break;												/* ld e,ixyh */
	case 0x7c: A=IXYH; break;												/* ld a,ixyh */

	case 0x45: B=IXYL; break;												/* ld b,ixyl */
	case 0x4d: C=IXYL; break;												/* ld c,ixyl */
	case 0x55: D=IXYL; break;												/* ld d,ixyl */
	case 0x5d: E=IXYL; break;												/* ld e,ixyl */
	case 0x7d: A=IXYL; break;												/* ld a,ixyl */

	case 0x46: { IA(); B=R8(a); break; }									/* ld b,(ixy+dis) */
	case 0x4e: { IA(); C=R8(a); break; }									/* ld c,(ixy+dis) */
	case 0x56: { IA(); D=R8(a); break; }									/* ld d,(ixy+dis) */
	case 0x5e: { IA(); E=R8(a); break; }									/* ld e,(ixy+dis) */
	case 0x66: { IA(); H=R8(a); break; }									/* ld h,(ixy+dis) */
	case 0x6e: { IA(); L=R8(a); break; }									/* ld l,(ixy+dis) */
	case 0x7e: { IA(); A=R8(a); break; }									/* ld a,(ixy+dis) */

	case 0x60: IXYH=B; break;												/* ld ixyh,b */
	case 0x61: IXYH=C; break;												/* ld ixyh,c */
	case 0x62: IXYH=D; break;												/* ld ixyh,d */
	case 0x63: IXYH=E; break;												/* ld ixyh,e */
	case 0x64: break;														/* ld ixyh,ixyh */
	case 0x65: IXYH=IXYL; break;											/* ld ixyh,ixyl */
	case 0x67: IXYH=A; break;												/* ld ixyh,a */

	case 0x68: IXYL=B; break;												/* ld ixyl,b */
	case 0x69: IXYL=C; break;												/* ld ixyl,c */
	case 0x6a: IXYL=D; break;												/* ld ixyl,d */
	case 0x6b: IXYL=E; break;												/* ld ixyl,e */
	case 0x6c: IXYL=IXYH; break;											/* ld ixyl,ixyh */
	case 0x6d: break;														/* ld ixyl,ixyl */
	case 0x6f: IXYL=A; break;												/* ld ixyl,a */

	case 0x70: { IA(); W8(a,B); break; }									/* ld (ixy+dis),b */
	case 0x71: { IA(); W8(a,C); break; }									/* ld (ixy+dis),c */
	case 0x72: { IA(); W8(a,D); break; }									/* ld (ixy+dis),d */
	case 0x73: { IA(); W8(a,E); break; }									/* ld (ixy+dis),e */
	case 0x74: { IA(); W8(a,H); break; }									/* ld (ixy+dis),h */
	case 0x75: { IA(); W8(a,L); break; }									/* ld (ixy+dis),l */
	case 0x77: { IA(); W8(a,A); break; }									/* ld (ixy+dis),a */

	case 0x26: IXYH=R8(PC); PC++; break;									/* ld ixyh,n */
	case 0x2e: IXYL=R8(PC); PC++; break;									/* ld ixyl,n */
	case 0x36: { IA(); W8(a,R8(PC)); PC++; break; }							/* ld (ixy+dis),n */

	/* 16-bit load */
	case 0x21: IXY=R16(PC); PC+=2; break;									/* ld ixy,nn */

	case 0x22: WZ=R16(PC); W16(WZ,IXY); WZ++; PC+=2; break;					/* ld (nn),ixy */
	case 0x2a: WZ=R16(PC); IXY=R16(WZ); WZ++; PC+=2; break;					/* ld ixy,(nn) */

	case 0xf9: SP=IXY; break;												/* ld sp,ixy */

	case 0xe5: PUSH(IXY); break;											/* push ixy */
	case 0xe1: IXY=POP(); break;											/* pop ixy */

	/* exchange */
	case 0xe3: WZ=R16(SP); W16(SP,IXY); IXY=WZ; break;						/* ex (sp),ixy */

	/* 8-bit arithmetic */
	case 0x84: { ADD(IXYH); break; }										/* add a,ixyh */
	case 0x8c: { ADC(IXYH); break; }										/* adc a,ixyh */
	case 0x94: { SUB(IXYH); break; }										/* sub ixyh */
	case 0x9c: { SBC(IXYH); break; }										/* sbc a,ixyh */
	case 0x24: { INCr(IXYH); break; }										/* inc ixyh */
	case 0x25: { DECr(IXYH); break; }										/* dec ixyh */
	case 0xbc: { CP(IXYH); break; }											/* cp ixyh */
	case 0xa4: AND(IXYH); break;											/* and ixyh */
	case 0xac: XOR(IXYH); break;											/* xor ixyh */
	case 0xb4: OR(IXYH); break;												/* or ixyh */

	case 0x85: { ADD(IXYL); break; }										/* add a,ixyl */
	case 0x8d: { ADC(IXYL); break; }										/* adc a,ixyl */
	case 0x95: { SUB(IXYL); break; }										/* sub ixyl */
	case 0x9d: { SBC(IXYL); break; }										/* sbc a,ixyl */
	case 0x2c: { INCr(IXYL); break; }										/* inc ixyl */
	case 0x2d: { DECr(IXYL); break; }										/* dec ixyl */
	case 0xbd: { CP(IXYL); break; }											/* cp ixyl */
	case 0xa5: AND(IXYL); break;											/* and ixyl */
	case 0xad: XOR(IXYL); break;											/* xor ixyl */
	case 0xb5: OR(IXYL); break;												/* or ixyl */

	case 0x86: { IA_NC(); ADD(R8(a)); PC++; break; }						/* add a,(ixy+dis) */
	case 0x8e: { IA_NC(); ADC(R8(a)); PC++; break; }						/* adc a,(ixy+dis) */
	case 0x96: { IA_NC(); SUB(R8(a)); PC++; break; }						/* sub (ixy+dis) */
	case 0x9e: { IA_NC(); SBC(R8(a)); PC++; break; }						/* sbc a,(ixy+dis) */
	case 0x34: { IA_NC(); INCm(a); PC++; break; }							/* inc (ixy+dis) */
	case 0x35: { IA_NC(); DECm(a); PC++; break; }							/* dec (ixy+dis) */
	case 0xbe: { IA_NC(); CP(R8(a)); PC++; break; }							/* cp (ixy+dis) */
	case 0xa6: { IA(); AND(R8(a)); break; }									/* and (ixy+dis) */
	case 0xae: { IA(); XOR(R8(a)); break; }									/* xor (ixy+dis) */
	case 0xb6: { IA(); OR(R8(a)); break; }									/* or (ixy+dis) */

	/* 16-bit arithmetic */
	case 0x09: { ADD16(IXY,BC); break; }									/* add ixy,bc */
	case 0x19: { ADD16(IXY,DE); break; }									/* add ixy,de */
	case 0x29: { ADD16(IXY,IXY); break; }									/* add ixy,ixy */
	case 0x39: { ADD16(IXY,SP); break; }									/* add ixy,sp */

	case 0x23: IXY++; break;												/* inc ixy */
	case 0x2b: IXY--; break;												/* dec ixy */

	/* jump */
	case 0xe9: PC=IXY; break;												/* jp (ixy) */


	/* dd/fd+cb */
	case 0xcb: {
		IA(); op=R8(PC); PC++;

		switch (op) {

	/* rotate/shift */
	case 0x00: RLCI(B); break;												/* rlc (ixy+dis),b */
	case 0x01: RLCI(C); break;												/* rlc (ixy+dis),c */
	case 0x02: RLCI(D); break;												/* rlc (ixy+dis),d */
	case 0x03: RLCI(E); break;												/* rlc (ixy+dis),e */
	case 0x04: RLCI(H); break;												/* rlc (ixy+dis),h */
	case 0x05: RLCI(L); break;												/* rlc (ixy+dis),l */
	case 0x06: { u8 v=R8(a); RLC(v); W8(a,v); break; }						/* rlc (ixy+dis) */
	case 0x07: RLCI(A); break;												/* rlc (ixy+dis),a */

	case 0x08: RRCI(B); break;												/* rrc (ixy+dis),b */
	case 0x09: RRCI(C); break;												/* rrc (ixy+dis),c */
	case 0x0a: RRCI(D); break;												/* rrc (ixy+dis),d */
	case 0x0b: RRCI(E); break;												/* rrc (ixy+dis),e */
	case 0x0c: RRCI(H); break;												/* rrc (ixy+dis),h */
	case 0x0d: RRCI(L); break;												/* rrc (ixy+dis),l */
	case 0x0e: { u8 v=R8(a); RRC(v); W8(a,v); break; }						/* rrc (ixy+dis) */
	case 0x0f: RRCI(A); break;												/* rrc (ixy+dis),a */

	case 0x10: { RLI(B); break; }											/* rl (ixy+dis),b */
	case 0x11: { RLI(C); break; }											/* rl (ixy+dis),c */
	case 0x12: { RLI(D); break; }											/* rl (ixy+dis),d */
	case 0x13: { RLI(E); break; }											/* rl (ixy+dis),e */
	case 0x14: { RLI(H); break; }											/* rl (ixy+dis),h */
	case 0x15: { RLI(L); break; }											/* rl (ixy+dis),l */
	case 0x16: { u8 v=R8(a); RL(v); W8(a,v); break; }						/* rl (ixy+dis) */
	case 0x17: { RLI(A); break; }											/* rl (ixy+dis),a */

	case 0x18: { RRI(B); break; }											/* rr (ixy+dis),b */
	case 0x19: { RRI(C); break; }											/* rr (ixy+dis),c */
	case 0x1a: { RRI(D); break; }											/* rr (ixy+dis),d */
	case 0x1b: { RRI(E); break; }											/* rr (ixy+dis),e */
	case 0x1c: { RRI(H); break; }											/* rr (ixy+dis),h */
	case 0x1d: { RRI(L); break; }											/* rr (ixy+dis),l */
	case 0x1e: { u8 v=R8(a); RR(v); W8(a,v); break; }						/* rr (ixy+dis) */
	case 0x1f: { RRI(A); break; }											/* rr (ixy+dis),a */

	case 0x20: SLAI(B); break;												/* sla (ixy+dis),b */
	case 0x21: SLAI(C); break;												/* sla (ixy+dis),c */
	case 0x22: SLAI(D); break;												/* sla (ixy+dis),d */
	case 0x23: SLAI(E); break;												/* sla (ixy+dis),e */
	case 0x24: SLAI(H); break;												/* sla (ixy+dis),h */
	case 0x25: SLAI(L); break;												/* sla (ixy+dis),l */
	case 0x26: { u8 v=R8(a); SLA(v); W8(a,v); break; }						/* sla (ixy+dis) */
	case 0x27: SLAI(A); break;												/* sla (ixy+dis),a */

	case 0x28: SRAI(B); break;												/* sra (ixy+dis),b */
	case 0x29: SRAI(C); break;												/* sra (ixy+dis),c */
	case 0x2a: SRAI(D); break;												/* sra (ixy+dis),d */
	case 0x2b: SRAI(E); break;												/* sra (ixy+dis),e */
	case 0x2c: SRAI(H); break;												/* sra (ixy+dis),h */
	case 0x2d: SRAI(L); break;												/* sra (ixy+dis),l */
	case 0x2e: { u8 v=R8(a); SRA(v); W8(a,v); break; }						/* sra (ixy+dis) */
	case 0x2f: SRAI(A); break;												/* sra (ixy+dis),a */

	case 0x30: SLLI(B); break;												/* sll (ixy+dis),b */
	case 0x31: SLLI(C); break;												/* sll (ixy+dis),c */
	case 0x32: SLLI(D); break;												/* sll (ixy+dis),d */
	case 0x33: SLLI(E); break;												/* sll (ixy+dis),e */
	case 0x34: SLLI(H); break;												/* sll (ixy+dis),h */
	case 0x35: SLLI(L); break;												/* sll (ixy+dis),l */
	case 0x36: { u8 v=R8(a); SLL(v); W8(a,v); break; }						/* sll (ixy+dis) */
	case 0x37: SLLI(A); break;												/* sll (ixy+dis),a */

	case 0x38: SRLI(B); break;												/* srl (ixy+dis),b */
	case 0x39: SRLI(C); break;												/* srl (ixy+dis),c */
	case 0x3a: SRLI(D); break;												/* srl (ixy+dis),d */
	case 0x3b: SRLI(E); break;												/* srl (ixy+dis),e */
	case 0x3c: SRLI(H); break;												/* srl (ixy+dis),h */
	case 0x3d: SRLI(L); break;												/* srl (ixy+dis),l */
	case 0x3e: { u8 v=R8(a); SRL(v); W8(a,v); break; }						/* srl (ixy+dis) */
	case 0x3f: SRLI(A); break;												/* srl (ixy+dis),a */

	/* bit */
	case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
	case 0x48: case 0x49: case 0x4a: case 0x4b: case 0x4c: case 0x4d: case 0x4e: case 0x4f:
	case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57:
	case 0x58: case 0x59: case 0x5a: case 0x5b: case 0x5c: case 0x5d: case 0x5e: case 0x5f:
	case 0x60: case 0x61: case 0x62: case 0x63: case 0x64: case 0x65: case 0x66: case 0x67:
	case 0x68: case 0x69: case 0x6a: case 0x6b: case 0x6c: case 0x6d: case 0x6e: case 0x6f:
	case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:
	case 0x78: case 0x79: case 0x7a: case 0x7b: case 0x7c: case 0x7d: case 0x7e: case 0x7f:
		CC+=EC_CBib; F=(BITF(R8(a)))|(W&0x28); break;						/* bit n,(ixy+dis) */

	case 0x80: case 0x88: case 0x90: case 0x98: case 0xa0: case 0xa8: case 0xb0: case 0xb8:
		RESI(B); break;														/* res n,(ixy+dis),b */
	case 0x81: case 0x89: case 0x91: case 0x99: case 0xa1: case 0xa9: case 0xb1: case 0xb9:
		RESI(C); break;														/* res n,(ixy+dis),c */
	case 0x82: case 0x8a: case 0x92: case 0x9a: case 0xa2: case 0xaa: case 0xb2: case 0xba:
		RESI(D); break;														/* res n,(ixy+dis),d */
	case 0x83: case 0x8b: case 0x93: case 0x9b: case 0xa3: case 0xab: case 0xb3: case 0xbb:
		RESI(E); break;														/* res n,(ixy+dis),e */
	case 0x84: case 0x8c: case 0x94: case 0x9c: case 0xa4: case 0xac: case 0xb4: case 0xbc:
		RESI(H); break;														/* res n,(ixy+dis),h */
	case 0x85: case 0x8d: case 0x95: case 0x9d: case 0xa5: case 0xad: case 0xb5: case 0xbd:
		RESI(L); break;														/* res n,(ixy+dis),l */
	case 0x86: case 0x8e: case 0x96: case 0x9e: case 0xa6: case 0xae: case 0xb6: case 0xbe:
		W8(a,R8(a)&~BITs); break;											/* res n,(ixy+dis) */
	case 0x87: case 0x8f: case 0x97: case 0x9f: case 0xa7: case 0xaf: case 0xb7: case 0xbf:
		RESI(A); break;														/* res n,(ixy+dis),a */

	case 0xc0: case 0xc8: case 0xd0: case 0xd8: case 0xe0: case 0xe8: case 0xf0: case 0xf8:
		SETI(B); break;														/* set n,(ixy+dis),b */
	case 0xc1: case 0xc9: case 0xd1: case 0xd9: case 0xe1: case 0xe9: case 0xf1: case 0xf9:
		SETI(C); break;														/* set n,(ixy+dis),c */
	case 0xc2: case 0xca: case 0xd2: case 0xda: case 0xe2: case 0xea: case 0xf2: case 0xfa:
		SETI(D); break;														/* set n,(ixy+dis),d */
	case 0xc3: case 0xcb: case 0xd3: case 0xdb: case 0xe3: case 0xeb: case 0xf3: case 0xfb:
		SETI(E); break;														/* set n,(ixy+dis),e */
	case 0xc4: case 0xcc: case 0xd4: case 0xdc: case 0xe4: case 0xec: case 0xf4: case 0xfc:
		SETI(H); break;														/* set n,(ixy+dis),h */
	case 0xc5: case 0xcd: case 0xd5: case 0xdd: case 0xe5: case 0xed: case 0xf5: case 0xfd:
		SETI(L); break;														/* set n,(ixy+dis),l */
	case 0xc6: case 0xce: case 0xd6: case 0xde: case 0xe6: case 0xee: case 0xf6: case 0xfe:
		W8(a,R8(a)|BITs); break;											/* set n,(ixy+dis) */
	case 0xc7: case 0xcf: case 0xd7: case 0xdf: case 0xe7: case 0xef: case 0xf7: case 0xff:
		SETI(A); break;														/* set n,(ixy+dis),a */

		} /* dd/fd+cb switch */
		break;
	} /* dd/fd+cb */

	/* rest */
	default:
#if Z80_TRACER|Z_LOG_SPOP
		/* dd/fd nop */
		tr_opc=PC-2;
		if (!tracer_prev_int) {
			CCTOTAL+=((CCPREV-CC)/crystal->cycle);
			if (tracer_on) { vdp_whereami(tracerpos); fprintf(tracer_fd,"%04X: %cD          %cd nop             BC:%04X DE:%04X HL:%04X AF:%04X(%c%c%c%c%c%c%c%c) SP:%04X IX:%04X IY:%04X R:%02X I:%02X/%d%c%c c:%s(%d,%I64d)\n",tr_opc,'D'+i,'d'+i,BC,DE,HL,AF,SF?'S':'.',ZF?'Z':'.',YF?'Y':'.',HF?'H':'.',XF?'X':'.',PF?'P':'.',NF?'N':'.',CF?'C':'.',SP,IX,IY,((R-1)&0x7f)|RH,I,IM,IFF1?'1':'.',IFF2?'2':'.',tracerpos,(CCPREV-CC)/crystal->cycle,CCTOTAL); }
			CCPREV=CC;
		}
		else tracer_prev_int=FALSE;
#endif
		continue;															/* standard main opcode */

		} /* dd/fd switch */
		break;
	} /* dd/fd */


	/* rest */
#if Z_LOG_SPOP
	case 0x40: ZL_LD_B_B break;												/* ld b,b */
	case 0x49: ZL_LD_C_C break;												/* ld c,c */
	case 0x52: ZL_LD_D_D break;												/* ld d,d */
	case 0x5b: ZL_LD_E_E break;												/* ld e,e */
	case 0x64: ZL_LD_H_H break;												/* ld h,h */
	case 0x6d: ZL_LD_L_L break;												/* ld l,l */
	case 0x7f: ZL_LD_A_A break;												/* ld a,a */
#endif
	default: break;															/* 00: nop, xx: ld r,r (same) */

	} /* main switch */

#if Z80_TRACER|Z_LOG_SPOP
	/* output current state */
	if (!tracer_prev_int) {
		CCTOTAL+=((CCPREV-CC)/crystal->cycle);
		if (strlen(tracermsg)&&tracer_on) { vdp_whereami(tracerpos); fprintf(tracer_fd,"%sBC:%04X DE:%04X HL:%04X AF:%04X(%c%c%c%c%c%c%c%c) SP:%04X IX:%04X IY:%04X R:%02X I:%02X/%d%c%c c:%s(%d,%I64d)\n",tracermsg,BC,DE,HL,AF,SF?'S':'.',ZF?'Z':'.',YF?'Y':'.',HF?'H':'.',XF?'X':'.',PF?'P':'.',NF?'N':'.',CF?'C':'.',SP,IX,IY,(R&0x7f)|RH,I,IM,IFF1?'1':'.',IFF2?'2':'.',tracerpos,(CCPREV-CC)/crystal->cycle,CCTOTAL); }
		CCPREV=CC;
	}
	else tracer_prev_int=FALSE;
#endif
#if Z80_TRACER
	/* get/decode next opcode */
	tr_opi=5; while (tr_opi--) tr_op[tr_opi]=-1; tr_opi=0;
	tr_pc=PC; tr_op[tr_opi++]=R8(tr_pc); tr_opc=tr_pc++;

	for (tr_j=0;tr_j<0x8000;tr_j++) {
		/* ignore ei */
		for (tr_i=0;tr_i<0x10000;tr_i++) {
			if (tr_op[0]==0xfb) { tr_op[0]=R8(tr_pc); tr_opc=tr_pc++; }
			else break;
		}

		switch (tr_op[0]) {
			case 0xcb:
				tr_op[tr_opi++]=R8(tr_pc); tr_pc++;
				strcpy(tr_m,lut_mnemonics_cb[tr_op[1]]); break;
			case 0xed:
				tr_op[tr_opi++]=R8(tr_pc); tr_pc++;
				strcpy(tr_m,lut_mnemonics_ed[tr_op[1]]); break;
			case 0xdd: case 0xfd:
				/* more dd/fd? */
				for (tr_i=0;tr_i<0x10000;tr_i++) {
					tr_op[1]=R8(tr_pc); tr_pc++;
					if (tr_op[1]==0xdd||tr_op[1]==0xfd) { tr_op[0]=tr_op[1]; tr_opc=tr_pc-1; }
					else break;
				}

				tr_opi=2;

				/* dd/fd+cb */
				if (tr_op[1]==0xcb) {
					tr_op[tr_opi++]=R8(tr_pc); tr_pc++; /* displacement */
					tr_op[tr_opi++]=R8(tr_pc); tr_pc++;
					strcpy(tr_m,lut_mnemonics_ddfdcb[tr_op[3]]); break;
				}

				/* standard main opcode */
				if (strlen(lut_mnemonics_ddfd[tr_op[1]])==1) {
					tr_op[0]=tr_op[1]; tr_op[1]=-1; tr_opi=1; tr_opc++;
					strcpy(tr_m,lut_mnemonics_main[tr_op[0]]);
					continue;
				}

				/* legal dd/fd opcode */
				else strcpy(tr_m,lut_mnemonics_ddfd[tr_op[1]]);
				break;
			default: strcpy(tr_m,lut_mnemonics_main[tr_op[0]]); break;
		}

		break;
	}

	tr_j=strlen(tr_m); tr_m[tr_j]=0;

	/* find opcode data */
	if (tr_op[3]!=-1) { tr_d=(s8)tr_op[2]; }
	else {
		/* index displacement */
		for (tr_i=0;tr_i<tr_j;tr_i++) if (tr_m[tr_i]=='D') break;
		if (tr_i!=tr_j) { tr_op[tr_opi]=R8(tr_pc); tr_pc++; tr_d=(s8)tr_op[tr_opi++]; }

		/* 8-bit constant (done after) */
		for (tr_i=0;tr_i<tr_j;tr_i++) if (tr_m[tr_i]=='N') break;
		if (tr_i!=tr_j) { tr_op[tr_opi]=R8(tr_pc); tr_pc++; tr_n=tr_op[tr_opi++]; }

		else {
			/* 16-bit constant (done after) */
			for (tr_i=0;tr_i<tr_j;tr_i++) if (tr_m[tr_i]=='A') break;
			if (tr_i!=tr_j) {
				tr_op[tr_opi]=R8(tr_pc); tr_pc++; tr_a=tr_op[tr_opi++];
				tr_op[tr_opi]=R8(tr_pc); tr_pc++; tr_a|=(tr_op[tr_opi++]<<8);
			}

			else {
				/* jump displacement */
				for (tr_i=0;tr_i<tr_j;tr_i++) if (tr_m[tr_i]=='B') { tr_m[tr_i]='A'; break; }
				if (tr_i!=tr_j) { tr_op[tr_opi]=R8(tr_pc); tr_pc++; tr_a=tr_opc+2+(s8)tr_op[tr_opi++]; }
			}
		}
	}

	/* PCPC: 00 01 02 03 */
	sprintf(tracermsg,"%04X: %02X %02X %02X %02X ",tr_opc,tr_op[0],tr_op[1],tr_op[2],tr_op[3]);
	for (tr_i=0;tr_i<4;tr_i++) if (tr_op[tr_i]==-1) break;
	tr_i=3*tr_i+6; memset(tracermsg+tr_i,' ',16);

	/* copy mnemonic+data (CPU state later) */
	tr_i=0; tr_j=18;
	for (;;) {
		if (tr_m[tr_i]==0) break;

		switch (tr_m[tr_i]) {
			case 'A': if (tr_a>=0xa000||(tr_a>=0xa00&&tr_a<0x1000)||(tr_a>=0xa0&&tr_a<0x100)||(tr_a>9&&tr_a<10)) tracermsg[tr_j++]='0'; sprintf(tracermsg+tr_j,"%X",tr_a); tr_i++; tr_j=tr_j+1+(tr_a>0xf)+(tr_a>0xff)+(tr_a>0xfff); if (tr_a>9) tracermsg[tr_j++]='h'; break;
			case 'D': tracermsg[tr_j++]=(tr_d<0)?'-':'+'; if (tr_d<0) tr_d=-tr_d; if (tr_d>9&&tr_d<0x10) tracermsg[tr_j++]='0'; sprintf(tracermsg+tr_j,"%X",tr_d); tr_i++; tr_j=tr_j+1+(tr_d>0xf); if (tr_d>9) tracermsg[tr_j++]='h'; break;
			case 'N': if (tr_n>=0xa0||(tr_n>9&&tr_n<0x10)) tracermsg[tr_j++]='0'; sprintf(tracermsg+tr_j,"%X",tr_n); tr_i++; tr_j=tr_j+1+(tr_n>0xf); if (tr_n>9) tracermsg[tr_j++]='h'; break;
			case 'X': tracermsg[tr_j++]='i'; tracermsg[tr_j++]=(tr_op[0]&0x20)?'y':'x'; tr_i++; break;

			default: tracermsg[tr_j++]=tr_m[tr_i++]; break;
		}
	}

	memset(tracermsg+tr_j,' ',20); tracermsg[37]=0;

#endif /* Z80_TRACER */

	/* pending IRQ */
	if _IRQ {
		/* now */
		if (IFF1) {
			/* According to tests, most MSXes don't have an M1 wait cycle at interrupts, but
			according to schematics, some early MSX models include an extra M1 wait cycle here. */
#if Z80_TRACER
			if (tracer_on&tracer_additional_info) fprintf(tracer_fd,"_IRQ\n");
#endif
			if (HALT) { HALT=0; PC++; }
			R++; IFF1=IFF2=0;

			/* possible acknowledge here */
			if (IRQACK) { IRQACK=0; IRQNEXT=-2*crystal->frame; }

			/* On most MSXes, the databus contents (used by IM 0 and 2) during interrupts
			is always $FF. On some MSXes it's undefined. IM 2 is still functional by
			using a 257 byte lut. IM 0 is not, and AFAIK unused in MSX software.

			MSXes known to have a different databus value on interrupts:
		 		Canon V-20 (UK):	0
		 		Gradiente Expert:	?
			*/

			if (IM&2) {
				/* MSX software using IM 2:
					INK (Matra)
					Seleniak (Guzuta)
					Sink King (Guzuta)
					Sudoku (dvik & joyrex)
					..?

					(no official 80s software)
				*/
				CC-=EC_INT2;
				PUSH(PC); WZ=PC=R16(I<<8|0xff);
			}
			else {
				CC-=EC_INT;
				op=0xff; /* IM 0/1: RST 38h */
#if Z80_TRACER|Z_LOG_SPOP
				tracer_prev_int=TRUE;
#endif
				continue;
			}
		}

		/* acknowledge */
		else if (IRQACK) { IRQACK=0; IRQNEXT=-2*crystal->frame; }
	}

	op=R8(PC); PC++; R++;
	CC-=lut_cycles_main[op];

	} /* big loop */

	OP=op;
}



void __fastcall z80_set_tracer(int t)
{
#if Z80_TRACER
	if (t^tracer_on) {
		if (t) {
			/* on */
			char fn[STRING_SIZE]={0};
			char c[0x10];
			int i,cut;

			mapper_get_current_name(fn);
			file_setfile(&file->appdir,fn,NULL,NULL);
			strcpy(fn,file->filename); strcat(fn,".");
			cut=strlen(fn);

			/* try to access name.i.log */
			for (i=1;i<10000;i++) {
				fn[cut]=0;
				sprintf(c,"%d",i);
				strcat(fn,c); strcat(fn,".log");
				if (access(fn,F_OK)) break;
			}

			if (i==10000) return;

			if (file_save_custom_text(&tracer_fd,fn)) {
				fprintf(tracer_fd,"Z80 tracer log, say cheese :)\n");
				tracer_on=1;
			}
			else file_close_custom(tracer_fd);
		}
		else {
			/* off */
			file_close_custom(tracer_fd);
			tracer_fd=NULL;

			tracer_on=0;
		}
	}
#else
    t = t;
	tracer_on=0;
#endif
}

void z80_new_frame(void)
{
	CC+=crystal->frame; CCPREV+=crystal->frame;
#if 0
	/* enable tracer on specific frame */
	if (crystal->fc==166) z80_set_tracer(TRUE);
#endif
#if Z80_TRACER
	if (!TR_RESET) z80_set_tracer(input_trigger_held(CONT_T_MAX+INPUT_T_Z80TRACER)!=0);
	if (tracer_on&tracer_additional_info) fprintf(tracer_fd,"\nFRAME %d\n",crystal->fc);
#endif
}

void z80_end_frame(void)
{
	if _IRQ IRQNEXT=2*crystal->frame;
}

void z80_poweron(void)
{
	memset(&z80,0,sizeof(z80));
	IRQNEXT=-2*crystal->frame;

	/*
	CPU start time measured to be consistent, exact timing differs per MSX (with VDP
	vblank bit set or clear? - can't be measured/doesn't matter). I assume this time
	is relative to true VDP power-on time, this needs to be tested on more MSXes to
	get a conclusive result.

	- National CF1200 (TMS9918A, RAM in slot 0): at the end of scanline 219
	- Panasonic CF2700 (TMS9129, RAM in slot 1): at the start of scanline 242
	- Canon V-20 (2 crystals, TMS9929A, RAM in slot 3): scanline 24x, hard to test due
	  to the non-standard crystal configuration, and it fluctuates

	(with 0 being the first active scanline)

	MSX software known to be affected by poweron time (real MSX too), these may lock up
	depending on BIOS/RAM configuration: A Life M36 Planet, Penguin Adventure
	*/
	if (crystal->mode) CC=CCPREV=-242*crystal->vdp_scanline;
	else CC=CCPREV=-219*crystal->vdp_scanline;
}

void __fastcall z80_set_cycles(int c) { CC=CCPREV=c; }
int* __fastcall z80_get_cycles_ptr(void) { return &CC; }
int __fastcall z80_get_cycles(void) { return CC; }
int __fastcall z80_get_rel_cycles(void) { return CC-CC%crystal->rel_cycle; }
void __fastcall z80_steal_cycles(int c) { CC=CCPREV=CC-c; }
void z80_adjust_rel_cycles(void) { int c=CC%crystal->vdp_cycle; CC-=c; CCPREV-=c; }

/* vdp.c (I/O) as IRQ source */
void __fastcall z80_irq_ack(void) { IRQACK=1; }
void __fastcall z80_irq_next(int c) { IRQNEXT=c-((IO_EC+W_EC)*crystal->cycle); }
int __fastcall z80_irq_pending(void) { return (CC-((IO_EC+W_EC)*crystal->cycle))<IRQNEXT; }

int __fastcall z80_get_busack(void) { return _BUSACK; }
void __fastcall z80_set_busack(int b) { _BUSACK=b; }
int __fastcall z80_get_busrq(void) { return _BUSRQ&1; }
int __fastcall z80_set_busrq(int b)
{
	if (b&&(OP&0xdd)==0xdd) {
		/* Bus request is not granted in mid-opcode. To emulate this 100% accurately,
		I'd have to check it after every opcode, but it's ok on MSX, since it's only
		used with hardware pause key. */
		return FALSE;
	}

	_BUSRQ=b;
	return TRUE;
}

void* __fastcall z80_get_tracer_fd(void) { return (void*)tracer_fd; }
int __fastcall z80_get_pc(void) { return PC; }
int __fastcall z80_get_f(void) { return F; }
int __fastcall z80_get_a(void) { return A; }
void __fastcall z80_set_f(int f) { F=f; }
void __fastcall z80_set_a(int a) { A=a; }
void __fastcall z80_di(void) { IFF1=IFF2=0; }

void z80_fill_op_cycles_lookup(void)
{
	int i;

	for (i=0;i<0x100;i++) {
		lut_cycles_main[i]=(lut_cycles_main_raw[i]+MSX_EC)*crystal->cycle;
		lut_cycles_ed[i]=(lut_cycles_ed_raw[i]+MSX_EC)*crystal->cycle;
		if (lut_cycles_ddfd_raw[i]==LCM) lut_cycles_ddfd[i]=lut_cycles_main[i];
		else lut_cycles_ddfd[i]=(lut_cycles_ddfd_raw[i]+MSX_EC)*crystal->cycle;
	}

	for (i=0;i<EC_MAX_i;i++) lut_cycles_ec[i]=lut_cycles_ec_raw[i]*crystal->cycle;
}

void z80_reset(void)
{
	char t[0x100];
	char oc[0x10];

	/* MSX software known to be relying on initial values:
		Pennant Race (Konami):	I=0 (MSX2 game though)
		Picture Puzzle (HAL):	RH=0

		(most are changed by the BIOS booting, so it's impossible to test accurately)
	*/
	AF=BC=DE=HL=WZ=AFS=BCS=DES=HLS=WZS=SP=IX=IY=0xffff;
	PC=I=IFF1=IFF2=IM=R=RH=0;
	OP=HALT=0;

	sprintf(oc," (%d%%)",crystal->overclock);
	sprintf(t,"%s %d0Hz MSX%s reset                          ",cont_get_region_shortname(cont_get_region()),6-crystal->mode,crystal->overclock==100?"":oc); t[42]=0;
	LOG(LOG_MISC|LOG_COLOUR(LC_YELLOW)|LOG_TYPE(LT_RESET),t);

	z80_set_tracer(TR_RESET&Z80_TRACER);
}

void z80_init(void)
{
	int i;

	z80_fill_op_cycles_lookup();

	/* fill flags table */
	for (i=0;i<0x100;i++) FT[i]=(i&0xa8)|(i==0)<<6|(4&(4^i<<2^i<<1^i^i>>1^i>>2^i>>3^i>>4^i>>5));
}

void z80_clean(void)
{
	;
}


/* state				size
AF						2
BC						2
DE						2
HL						2
WZ						2
AFS						2
BCS						2
DES						2
HLS						2
WZS						2
IX						2
IY						2
PC						2
SP						2
R						1
RH						1
I						1

IFF1					1
IFF2					1
IM						1
HALT					1

_BUSRQ					1
_BUSACK					1
IRQACK					1
IRQNEXT					4

CC						4
OP						1

==						47
*/
#define STATE_VERSION	3
/* version history:
1: initial
2: added WZS (unused yet, mentioned in some technical docs), added _BUSRQ/_BUSACK
3: changed _INT to IRQACK / IRQNEXT
*/
#define STATE_SIZE		47

int __fastcall z80_state_get_version(void)
{
	return STATE_VERSION;
}

int __fastcall z80_state_get_size(void)
{
	return STATE_SIZE;
}

/* save */
void __fastcall z80_state_save(u8** s)
{
	WZS=WZ; /* for later */

	STATE_SAVE_2(AF);		STATE_SAVE_2(BC);	STATE_SAVE_2(DE);	STATE_SAVE_2(HL);	STATE_SAVE_2(WZ);
	STATE_SAVE_2(AFS);		STATE_SAVE_2(BCS);	STATE_SAVE_2(DES);	STATE_SAVE_2(HLS);	STATE_SAVE_2(WZS);
	STATE_SAVE_2(IX);		STATE_SAVE_2(IY);

	STATE_SAVE_2(PC);		STATE_SAVE_2(SP);
	STATE_SAVE_1(R);		STATE_SAVE_1(RH);
	STATE_SAVE_1(I);

	STATE_SAVE_1(IFF1);		STATE_SAVE_1(IFF2);
	STATE_SAVE_1(IM);		STATE_SAVE_1(HALT);

	STATE_SAVE_1(_BUSRQ);	STATE_SAVE_1(_BUSACK);
	STATE_SAVE_1(IRQACK);	STATE_SAVE_4(IRQNEXT);

	STATE_SAVE_4(CC);
	STATE_SAVE_1(OP);
}

/* load */
void __fastcall z80_state_load_cur(u8** s)
{
	STATE_LOAD_2(AF);		STATE_LOAD_2(BC);	STATE_LOAD_2(DE);	STATE_LOAD_2(HL);	STATE_LOAD_2(WZ);
	STATE_LOAD_2(AFS);		STATE_LOAD_2(BCS);	STATE_LOAD_2(DES);	STATE_LOAD_2(HLS);	STATE_LOAD_2(WZS);
	STATE_LOAD_2(IX);		STATE_LOAD_2(IY);

	STATE_LOAD_2(PC);		STATE_LOAD_2(SP);
	STATE_LOAD_1(R);		STATE_LOAD_1(RH);
	STATE_LOAD_1(I);

	STATE_LOAD_1(IFF1);		STATE_LOAD_1(IFF2);
	STATE_LOAD_1(IM);		STATE_LOAD_1(HALT);

	STATE_LOAD_1(_BUSRQ);	STATE_LOAD_1(_BUSACK);
	STATE_LOAD_1(IRQACK);	STATE_LOAD_4(IRQNEXT);

	STATE_LOAD_4(CC);
	STATE_LOAD_1(OP);
}

void __fastcall z80_state_load_2(u8** s)
{
	/* version 2 */
	int i;

	STATE_LOAD_2(AF);		STATE_LOAD_2(BC);	STATE_LOAD_2(DE);	STATE_LOAD_2(HL);	STATE_LOAD_2(WZ);
	STATE_LOAD_2(AFS);		STATE_LOAD_2(BCS);	STATE_LOAD_2(DES);	STATE_LOAD_2(HLS);	STATE_LOAD_2(WZS);
	STATE_LOAD_2(IX);		STATE_LOAD_2(IY);

	STATE_LOAD_2(PC);		STATE_LOAD_2(SP);
	STATE_LOAD_1(R);		STATE_LOAD_1(RH);
	STATE_LOAD_1(I);
	STATE_LOAD_1(IFF1);		STATE_LOAD_1(IFF2);
	STATE_LOAD_1(IM);		STATE_LOAD_1(HALT);

	/* irq */
	IRQACK=FALSE;			IRQNEXT=-2*crystal->frame;
	STATE_LOAD_1(i);
	if (i) IRQNEXT=-IRQNEXT;

	STATE_LOAD_1(_BUSRQ);	STATE_LOAD_1(_BUSACK);

	STATE_LOAD_4(CC);
	STATE_LOAD_1(OP);
}

void __fastcall z80_state_load_1(u8** s)
{
	/* version 1 */
	int i;

	STATE_LOAD_2(AF);		STATE_LOAD_2(BC);	STATE_LOAD_2(DE);	STATE_LOAD_2(HL);
	STATE_LOAD_2(AFS);		STATE_LOAD_2(BCS);	STATE_LOAD_2(DES);	STATE_LOAD_2(HLS);
	STATE_LOAD_2(IX);		STATE_LOAD_2(IY);

	STATE_LOAD_2(PC);		STATE_LOAD_2(SP);	STATE_LOAD_2(WZ);
	STATE_LOAD_1(R);		STATE_LOAD_1(RH);
	STATE_LOAD_1(I);
	STATE_LOAD_1(IFF1);		STATE_LOAD_1(IFF2);
	STATE_LOAD_1(IM);		STATE_LOAD_1(HALT);

	/* irq */
	IRQACK=FALSE;			IRQNEXT=-2*crystal->frame;
	STATE_LOAD_1(i);
	if (i) IRQNEXT=-IRQNEXT;

	STATE_LOAD_4(CC);
	STATE_LOAD_1(OP);

	/* WZ' */
	WZS=WZ;

	/* bus request/acknowledge */
	_BUSRQ=_BUSACK=FALSE;
}

int __fastcall z80_state_load(int v,u8** s)
{
	switch (v) {
		case 1:
			z80_state_load_1(s);
			break;
		case 2:
			z80_state_load_2(s);
			break;

		case STATE_VERSION:
			z80_state_load_cur(s);
			break;

		default: return FALSE;
	}

	return TRUE;
}
