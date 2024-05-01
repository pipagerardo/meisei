/******************************************************************************
 *                                                                            *
 *                             "paste_table.h"                                *
 *                                                                            *
 ******************************************************************************/

/* paste conversion tables, pastable characters only, source copy: MSX in favour of codepage 437 */

/* hello, I am huge */
static const u16 pc2msx[4][0x100]={
{
	/* International */
	0,							/* 00 */
	FEXT|FREASS|0x41,			/* 01: see extended table, or [01+41] */
	FEXT|FREASS|0x42,			/* 02: [01+42] */
	FEXT|FREASS|0x43,			/* 03: [01+43] */
	FEXT|FREASS|0x44,			/* 04: [01+44] */
	FEXT|FREASS|0x45,			/* 05: [01+45] */
	FEXT|FREASS|0x46,			/* 06: [01+46] */
	FEXT|FREASS|0x47,			/* 07: [01+47] */
	FEXT|FREASS|0x48,			/* 08: [01+48] (backspace) */
	KEY(7,3),					/* 09: tab */
	KEY(7,7),					/* 0a: line feed (return) */
	FEXT|FREASS|0x4b,			/* 0b: [01+4b] */
	FEXT|FREASS|0x4c,			/* 0c: [01+4c] */
	0,							/* 0d: (carriage return, ignore) */
	FEXT|FREASS|0x4e,			/* 0e: [01+4e] */
	FEXT|FREASS|0x4f,			/* 0f: [01+4f] */
	FREASS|0xcf,				/* 10: [cf] */
	FREASS|0xd0,				/* 11: [d0] */
	FREASS|0xd1,				/* 12: [d1] */
	FREASS|0x21,				/* 13: [21] */
	FREASS|0xbe,				/* 14: [be] */
	FREASS|0xbf,				/* 15: [bf] */
	FEXT|FREASS|0x57,			/* 16: [01+57] */
	FREASS|0xd1,				/* 17: [d1] */
	FREASS|0xce,				/* 18: [ce] */
	FREASS|0xcd,				/* 19: [cd] */
	0,							/* 1a: (end of file) */
	FREASS|0xd0,				/* 1b: [d0] (escape) */
	FEXT|FREASS|0x5a,			/* 1c: [01+5a] */
	FREASS|0xd2,				/* 1d: [d2] */
	FREASS|0xce,				/* 1e: [ce] */
	FREASS|0xcd,				/* 1f: [cd] */
	KEY(8,0),					/* 20: space */
	FSHFT|KEY(0,1),				/* 21: ! shift+1 */
	FSHFT|KEY(2,0),				/* 22: " shift+' */
	FSHFT|KEY(0,3),				/* 23: # shift+3 */
	FSHFT|KEY(0,4),				/* 24: $ shift+4 */
	FSHFT|KEY(0,5),				/* 25: % shift+5 */
	FSHFT|KEY(0,7),				/* 26: & shift+7 */
	KEY(2,0),					/* 27: ' */
	FSHFT|KEY(1,1),				/* 28: ( shift+9 */
	FSHFT|KEY(0,0),				/* 29: ) shift+0 */
	FSHFT|KEY(1,0),				/* 2a: * shift+8 */
	FSHFT|KEY(1,3),				/* 2b: + shift+= */
	KEY(2,2),					/* 2c: , */
	KEY(1,2),					/* 2d: - */
	KEY(2,3),					/* 2e: . */
	KEY(2,4),					/* 2f: / */
	KEY(0,0),					/* 30: 0 */
	KEY(0,1),					/* 31: 1 */
	KEY(0,2),					/* 32: 2 */
	KEY(0,3),					/* 33: 3 */
	KEY(0,4),					/* 34: 4 */
	KEY(0,5),					/* 35: 5 */
	KEY(0,6),					/* 36: 6 */
	KEY(0,7),					/* 37: 7 */
	KEY(1,0),					/* 38: 8 */
	KEY(1,1),					/* 39: 9 */
	FSHFT|KEY(1,7),				/* 3a: : shift+; */
	KEY(1,7),					/* 3b: ; */
	FSHFT|KEY(2,2),				/* 3c: < shift+, */
	KEY(1,3),					/* 3d: = */
	FSHFT|KEY(2,3),				/* 3e: > shift+. */
	FSHFT|KEY(2,4),				/* 3f: ? shift+/ */
	FSHFT|KEY(0,2),				/* 40: @ shift+2 */
	FSHFT|KEY(2,6),				/* 41: A shift+a */
	FSHFT|KEY(2,7),				/* 42: B shift+b */
	FSHFT|KEY(3,0),				/* 43: C shift+c */
	FSHFT|KEY(3,1),				/* 44: D shift+d */
	FSHFT|KEY(3,2),				/* 45: E shift+e */
	FSHFT|KEY(3,3),				/* 46: F shift+f */
	FSHFT|KEY(3,4),				/* 47: G shift+g */
	FSHFT|KEY(3,5),				/* 48: H shift+h */
	FSHFT|KEY(3,6),				/* 49: I shift+i */
	FSHFT|KEY(3,7),				/* 4a: J shift+j */
	FSHFT|KEY(4,0),				/* 4b: K shift+k */
	FSHFT|KEY(4,1),				/* 4c: L shift+l */
	FSHFT|KEY(4,2),				/* 4d: M shift+m */
	FSHFT|KEY(4,3),				/* 4e: N shift+n */
	FSHFT|KEY(4,4),				/* 4f: O shift+o */
	FSHFT|KEY(4,5),				/* 50: P shift+p */
	FSHFT|KEY(4,6),				/* 51: Q shift+q */
	FSHFT|KEY(4,7),				/* 52: R shift+r */
	FSHFT|KEY(5,0),				/* 53: S shift+s */
	FSHFT|KEY(5,1),				/* 54: T shift+t */
	FSHFT|KEY(5,2),				/* 55: U shift+u */
	FSHFT|KEY(5,3),				/* 56: V shift+v */
	FSHFT|KEY(5,4),				/* 57: W shift+w */
	FSHFT|KEY(5,5),				/* 58: X shift+x */
	FSHFT|KEY(5,6),				/* 59: Y shift+y */
	FSHFT|KEY(5,7),				/* 5a: Z shift+z */
	KEY(1,5),					/* 5b: [ */
	KEY(1,4),					/* 5c: \ */
	KEY(1,6),					/* 5d: ] */
	FSHFT|KEY(0,6),				/* 5e: ^ shift+6 */
	FSHFT|KEY(1,2),				/* 5f: _ shift+- */
	KEY(2,1),					/* 60: ` */
	KEY(2,6),					/* 61: a */
	KEY(2,7),					/* 62: b */
	KEY(3,0),					/* 63: c */
	KEY(3,1),					/* 64: d */
	KEY(3,2),					/* 65: e */
	KEY(3,3),					/* 66: f */
	KEY(3,4),					/* 67: g */
	KEY(3,5),					/* 68: h */
	KEY(3,6),					/* 69: i */
	KEY(3,7),					/* 6a: j */
	KEY(4,0),					/* 6b: k */
	KEY(4,1),					/* 6c: l */
	KEY(4,2),					/* 6d: m */
	KEY(4,3),					/* 6e: n */
	KEY(4,4),					/* 6f: o */
	KEY(4,5),					/* 70: p */
	KEY(4,6),					/* 71: q */
	KEY(4,7),					/* 72: r */
	KEY(5,0),					/* 73: s */
	KEY(5,1),					/* 74: t */
	KEY(5,2),					/* 75: u */
	KEY(5,3),					/* 76: v */
	KEY(5,4),					/* 77: w */
	KEY(5,5),					/* 78: x */
	KEY(5,6),					/* 79: y */
	KEY(5,7),					/* 7a: z */
	FSHFT|KEY(1,5),				/* 7b: { shift+[ */
	FSHFT|KEY(1,4),				/* 7c: | shift+\ */
	FSHFT|KEY(1,6),				/* 7d: } shift+] */
	FSHFT|KEY(2,1),				/* 7e: ~ shift+` */
	FREASS|0xd8,				/* 7f: [d8] */
	FSHFT|FCODE|KEY(1,1),		/* 80: Ä shift+code+9 */
	FCODE|KEY(3,4),				/* 81: Å code+g */
	FCODE|KEY(5,2),				/* 82: Ç code+u */
	FCODE|KEY(4,6),				/* 83: É code+q */
	FCODE|KEY(2,6),				/* 84: Ñ code+a */
	FCODE|KEY(5,7),				/* 85: Ö code+z */
	FCODE|KEY(2,2),				/* 86: Ü code+, */
	FCODE|KEY(1,1),				/* 87: á code+9 */
	FCODE|KEY(5,4),				/* 88: à code+w */
	FCODE|KEY(5,0),				/* 89: â code+s */
	FCODE|KEY(5,5),				/* 8a: ä code+x */
	FCODE|KEY(3,1),				/* 8b: ã code+d */
	FCODE|KEY(3,2),				/* 8c: å code+e */
	FCODE|KEY(3,0),				/* 8d: ç code+c */
	FSHFT|FCODE|KEY(2,6),		/* 8e: é shift+code+a */
	FSHFT|FCODE|KEY(2,2),		/* 8f: è shift+code+, */
	FSHFT|FCODE|KEY(5,2),		/* 90: ê shift+code+u */
	FCODE|KEY(3,7),				/* 91: ë code+j */
	FSHFT|FCODE|KEY(3,7),		/* 92: í shift+code+j */
	FCODE|KEY(4,7),				/* 93: ì code+r */
	FCODE|KEY(3,3),				/* 94: î code+f */
	FCODE|KEY(5,3),				/* 95: ï code+v */
	FCODE|KEY(5,1),				/* 96: ñ code+t */
	FCODE|KEY(2,7),				/* 97: ó code+b */
	FCODE|KEY(0,5),				/* 98: ò code+5 */
	FSHFT|FCODE|KEY(3,3),		/* 99: ô shift+code+f */
	FSHFT|FCODE|KEY(3,4),		/* 9a: ö shift+code+g */
	FCODE|KEY(0,4),				/* 9b: õ code+4 */
	FSHFT|FCODE|KEY(0,4),		/* 9c: ú shift+code+4 */
	FSHFT|FCODE|KEY(0,5),		/* 9d: ù shift+code+5 */
	FSHFT|FCODE|KEY(0,2),		/* 9e: û shift+code+2 */
	FCODE|KEY(0,1),				/* 9f: ü code+1 */
	FCODE|KEY(5,6),				/* a0: † code+y */
	FCODE|KEY(3,6),				/* a1: ° code+i */
	FCODE|KEY(4,4),				/* a2: ¢ code+o */
	FCODE|KEY(4,5),				/* a3: £ code+p */
	FCODE|KEY(4,3),				/* a4: § code+n */
	FSHFT|FCODE|KEY(4,3),		/* a5: • shift+code+n */
	FCODE|KEY(2,3),				/* a6: ¶ code+. */
	FCODE|KEY(2,4),				/* a7: ß code+/ */
	FSHFT|FCODE|KEY(2,4),		/* a8: ® shift+code+/ */
	FSHFT|FGRAPH|KEY(4,7),		/* a9: © shift+graph+r */
	FSHFT|FGRAPH|KEY(5,6),		/* aa: ™ shift+graph+y */
	FGRAPH|KEY(0,2),			/* ab: ´ graph+2 */
	FGRAPH|KEY(0,1),			/* ac: ¨ graph+1 */
	FSHFT|FCODE|KEY(0,1),		/* ad: ≠ shift+code+1 */
	FSHFT|FGRAPH|KEY(2,2),		/* ae: Æ shift+graph+, */
	FSHFT|FGRAPH|KEY(2,3),		/* af: Ø shift+graph+. */
	FSHFT|FCODE|KEY(3,5),		/* b0: shift+code+h */
	FCODE|KEY(3,5),				/* b1: code+h */
	FSHFT|FCODE|KEY(4,0),		/* b2: shift+code+k */
	FCODE|KEY(4,0),				/* b3: code+k */
	FSHFT|FCODE|KEY(4,1),		/* b4: shift+code+l */
	FCODE|KEY(4,1),				/* b5: code+l */
	FSHFT|FCODE|KEY(1,7),		/* b6: shift+code+; */
	FCODE|KEY(1,7),				/* b7: code+; */
	FSHFT|FCODE|KEY(2,0),		/* b8: shift+code+' */
	FCODE|KEY(2,0),				/* b9: code+' */
	FGRAPH|KEY(0,3),			/* ba: graph+3 */
	FGRAPH|KEY(2,1),			/* bb: graph+` */
	FGRAPH|KEY(3,0),			/* bc: graph+c */
	FGRAPH|KEY(0,5),			/* bd: graph+5 */
	FSHFT|FCODE|KEY(0,3),		/* be: shift+code+3 */
	FCODE|KEY(0,3),				/* bf: code+3 */
	FGRAPH|KEY(5,2),			/* c0: graph+u */
	FSHFT|FGRAPH|KEY(3,1),		/* c1: shift+graph+d */
	FGRAPH|KEY(4,4),			/* c2: graph+o */
	FSHFT|FGRAPH|KEY(4,4),		/* c3: shift+graph+o */
	FGRAPH|KEY(2,6),			/* c4: graph+a */
	FSHFT|FGRAPH|KEY(5,2),		/* c5: shift+graph+u */
	FGRAPH|KEY(3,7),			/* c6: graph+j */
	FGRAPH|KEY(3,1),			/* c7: graph+d */
	FGRAPH|KEY(4,1),			/* c8: graph+l */
	FSHFT|FGRAPH|KEY(4,1),		/* c9: shift+graph+l */
	FSHFT|FGRAPH|KEY(3,7),		/* ca: shift+graph+j */
	FSHFT|FGRAPH|KEY(4,6),		/* cb: shift+graph+q */
	FGRAPH|KEY(4,6),			/* cc: graph+q */
	FGRAPH|KEY(3,2),			/* cd: graph+e */
	FSHFT|FGRAPH|KEY(3,2),		/* ce: shift+graph+e */
	FGRAPH|KEY(5,4),			/* cf: graph+w */
	FSHFT|FGRAPH|KEY(5,4),		/* d0: shift+graph+w */
	FSHFT|FGRAPH|KEY(5,0),		/* d1: shift+graph+s */
	FGRAPH|KEY(5,0),			/* d2: graph+s */
	FSHFT|FGRAPH|KEY(4,3),		/* d3: shift+graph+n */
	FSHFT|FGRAPH|KEY(3,3),		/* d4: shift+graph+f */
	FSHFT|FGRAPH|KEY(5,3),		/* d5: shift+graph+v */
	FSHFT|FGRAPH|KEY(3,5),		/* d6: shift+graph+h */
	FSHFT|FGRAPH|KEY(4,5),		/* d7: shift+graph+p */
	FSHFT|FCODE|KEY(0,0),		/* d8: shift+code+0 */
	FCODE|KEY(0,2),				/* d9: code+2 */
	FCODE|KEY(1,6),				/* da: code+] */
	FGRAPH|KEY(4,5),			/* db: € graph+p */
	FGRAPH|KEY(3,6),			/* dc: ‹ graph+i */
	FGRAPH|KEY(4,0),			/* dd: › graph+k */
	FSHFT|FGRAPH|KEY(4,0),		/* de: ﬁ shift+graph+k */
	FSHFT|FGRAPH|KEY(3,6),		/* df: ﬂ shift+graph+i */
	FCODE|KEY(0,6),				/* e0: ‡ code+6 */
	FCODE|KEY(0,7),				/* e1: · code+7 */
	FSHFT|FCODE|KEY(1,0),		/* e2: ‚ shift+code+8 */
	FSHFT|FCODE|KEY(4,5),		/* e3: „ shift+code+p */
	FSHFT|FCODE|KEY(2,1),		/* e4: ‰ shift+code+` */
	FCODE|KEY(2,1),				/* e5: Â code+` */
	FCODE|KEY(4,2),				/* e6: Ê code+m */
	FCODE|KEY(1,0),				/* e7: Á code+8 */
	FSHFT|FCODE|KEY(1,5),		/* e8: Ë shift+code+[ */
	FCODE|KEY(1,3),				/* e9: È code+= */
	FSHFT|FCODE|KEY(1,6),		/* ea: Í shift+code+] */
	FCODE|KEY(0,0),				/* eb: Î code+0 */
	FGRAPH|KEY(1,0),			/* ec: Ï graph+8 */
	FCODE|KEY(1,5),				/* ed: Ì code+[ */
	FCODE|KEY(1,2),				/* ee: Ó code+- */
	FGRAPH|KEY(0,4),			/* ef: Ô graph+4 */
	FSHFT|FGRAPH|KEY(1,3),		/* f0:  shift+graph+= */
	FGRAPH|KEY(1,3),			/* f1: Ò graph+= */
	FGRAPH|KEY(2,3),			/* f2: Ú graph+. */
	FGRAPH|KEY(2,2),			/* f3: Û graph+, */
	FGRAPH|KEY(0,6),			/* f4: Ù graph+6 */
	FSHFT|FGRAPH|KEY(0,6),		/* f5: ı shift+graph+6 */
	FSHFT|FGRAPH|KEY(2,4),		/* f6: ˆ shift+graph+/ */
	FSHFT|FGRAPH|KEY(2,1),		/* f7: ˜ shift+graph+` */
	FSHFT|FGRAPH|KEY(5,7),		/* f8: ¯ shift+graph+z */
	FSHFT|FGRAPH|KEY(5,5),		/* f9: shift+graph+x */
	FSHFT|FGRAPH|KEY(3,0),		/* fa: shift+graph+c */
	FGRAPH|KEY(0,7),			/* fb: ˚ graph+7 */
	FSHFT|FGRAPH|KEY(0,3),		/* fc: ¸ shift+graph+3 */
	FSHFT|FGRAPH|KEY(0,2),		/* fd: ˝ shift+graph+2 */
	FSHFT|FGRAPH|KEY(2,6),		/* fe: shift+graph+a */
	FREASS|0x20					/* ff: [20] */
},{
	/* Japanese, the most common configuration (some have a different kana layout) */
	0,							/* 00 */
	FREASS|0x84,				/* 01: see extended table, or [84] */
	FREASS|0x85,				/* 02: [85] */
	FREASS|0x81,				/* 03: [81] */
	FREASS|0x83,				/* 04: [83] */
	FREASS|0x82,				/* 05: [82] */
	FREASS|0x80,				/* 06: [80] */
	0,							/* 07 */
	0,							/* 08: (backspace) */
	KEY(7,3),					/* 09: tab */
	KEY(7,7),					/* 0a: line feed (return) */
	0,							/* 0b */
	0,							/* 0c */
	0,							/* 0d: (carriage return, ignore) */
	0,							/* 0e */
	FREASS|0x84,				/* 0f: [84] */
	0,							/* 10 */
	0,							/* 11 */
	0,							/* 12 */
	0,							/* 13 */
	0,							/* 14 */
	0,							/* 15 */
	0,							/* 16 */
	0,							/* 17 */
	0,							/* 18 */
	0,							/* 19 */
	0,							/* 1a: (end of file) */
	0,							/* 1b: (escape) */
	0,							/* 1c */
	0,							/* 1d */
	0,							/* 1e */
	0,							/* 1f */
	KEY(8,0),					/* 20: space */
	FSHFT|KEY(0,1),				/* 21: ! shift+1 */
	FSHFT|KEY(0,2),				/* 22: " shift+2 */
	FSHFT|KEY(0,3),				/* 23: # shift+3 */
	FSHFT|KEY(0,4),				/* 24: $ shift+4 */
	FSHFT|KEY(0,5),				/* 25: % shift+5 */
	FSHFT|KEY(0,6),				/* 26: & shift+6 */
	FSHFT|KEY(0,7),				/* 27: ' ahift+7 */
	FSHFT|KEY(1,0),				/* 28: ( shift+8 */
	FSHFT|KEY(1,1),				/* 29: ) shift+9 */
	FSHFT|KEY(2,0),				/* 2a: * shift+' */
	FSHFT|KEY(1,7),				/* 2b: + shift+; */
	KEY(2,2),					/* 2c: , */
	KEY(1,2),					/* 2d: - */
	KEY(2,3),					/* 2e: . */
	KEY(2,4),					/* 2f: / */
	KEY(0,0),					/* 30: 0 */
	KEY(0,1),					/* 31: 1 */
	KEY(0,2),					/* 32: 2 */
	KEY(0,3),					/* 33: 3 */
	KEY(0,4),					/* 34: 4 */
	KEY(0,5),					/* 35: 5 */
	KEY(0,6),					/* 36: 6 */
	KEY(0,7),					/* 37: 7 */
	KEY(1,0),					/* 38: 8 */
	KEY(1,1),					/* 39: 9 */
	KEY(2,0),					/* 3a: : ' */
	KEY(1,7),					/* 3b: ; */
	FSHFT|KEY(2,2),				/* 3c: < shift+, */
	FSHFT|KEY(1,2),				/* 3d: = shift+- */
	FSHFT|KEY(2,3),				/* 3e: > shift+. */
	FSHFT|KEY(2,4),				/* 3f: ? shift+/ */
	KEY(1,5),					/* 40: @ [ */
	FSHFT|KEY(2,6),				/* 41: A shift+a */
	FSHFT|KEY(2,7),				/* 42: B shift+b */
	FSHFT|KEY(3,0),				/* 43: C shift+c */
	FSHFT|KEY(3,1),				/* 44: D shift+d */
	FSHFT|KEY(3,2),				/* 45: E shift+e */
	FSHFT|KEY(3,3),				/* 46: F shift+f */
	FSHFT|KEY(3,4),				/* 47: G shift+g */
	FSHFT|KEY(3,5),				/* 48: H shift+h */
	FSHFT|KEY(3,6),				/* 49: I shift+i */
	FSHFT|KEY(3,7),				/* 4a: J shift+j */
	FSHFT|KEY(4,0),				/* 4b: K shift+k */
	FSHFT|KEY(4,1),				/* 4c: L shift+l */
	FSHFT|KEY(4,2),				/* 4d: M shift+m */
	FSHFT|KEY(4,3),				/* 4e: N shift+n */
	FSHFT|KEY(4,4),				/* 4f: O shift+o */
	FSHFT|KEY(4,5),				/* 50: P shift+p */
	FSHFT|KEY(4,6),				/* 51: Q shift+q */
	FSHFT|KEY(4,7),				/* 52: R shift+r */
	FSHFT|KEY(5,0),				/* 53: S shift+s */
	FSHFT|KEY(5,1),				/* 54: T shift+t */
	FSHFT|KEY(5,2),				/* 55: U shift+u */
	FSHFT|KEY(5,3),				/* 56: V shift+v */
	FSHFT|KEY(5,4),				/* 57: W shift+w */
	FSHFT|KEY(5,5),				/* 58: X shift+x */
	FSHFT|KEY(5,6),				/* 59: Y shift+y */
	FSHFT|KEY(5,7),				/* 5a: Z shift+z */
	KEY(1,6),					/* 5b: [ ] */
	KEY(1,4),					/* 5c: ù \ */
	KEY(2,1),					/* 5d: ] ` */
	KEY(1,3),					/* 5e: ^ = */
	FSHFT|KEY(2,5),				/* 5f: _ shift+underscore */
	FSHFT|KEY(1,5),				/* 60: ` shift+[ */
	KEY(2,6),					/* 61: a */
	KEY(2,7),					/* 62: b */
	KEY(3,0),					/* 63: c */
	KEY(3,1),					/* 64: d */
	KEY(3,2),					/* 65: e */
	KEY(3,3),					/* 66: f */
	KEY(3,4),					/* 67: g */
	KEY(3,5),					/* 68: h */
	KEY(3,6),					/* 69: i */
	KEY(3,7),					/* 6a: j */
	KEY(4,0),					/* 6b: k */
	KEY(4,1),					/* 6c: l */
	KEY(4,2),					/* 6d: m */
	KEY(4,3),					/* 6e: n */
	KEY(4,4),					/* 6f: o */
	KEY(4,5),					/* 70: p */
	KEY(4,6),					/* 71: q */
	KEY(4,7),					/* 72: r */
	KEY(5,0),					/* 73: s */
	KEY(5,1),					/* 74: t */
	KEY(5,2),					/* 75: u */
	KEY(5,3),					/* 76: v */
	KEY(5,4),					/* 77: w */
	KEY(5,5),					/* 78: x */
	KEY(5,6),					/* 79: y */
	KEY(5,7),					/* 7a: z */
	FSHFT|KEY(1,6),				/* 7b: { shift+] */
	FSHFT|KEY(1,4),				/* 7c: | shift+\ */
	FSHFT|KEY(2,1),				/* 7d: } shift+` */
	FSHFT|KEY(1,3),				/* 7e: ~ shift+= */
	0,							/* 7f */
	FGRAPH|KEY(2,4),			/* 80: graph+/ */
	FGRAPH|KEY(2,0),			/* 81: graph+' */
	FGRAPH|KEY(1,7),			/* 82: graph+; */
	FGRAPH|KEY(2,5),			/* 83: graph+underscore */
	FGRAPH|KEY(1,6),			/* 84: graph+] */
	FGRAPH|KEY(2,1),			/* 85: graph+` */
	FKANA|KEY(2,4),				/* 86: kana+/ */
	FKANA|FSHFT|KEY(0,1),		/* 87: kana+shift+1 */
	FKANA|FSHFT|KEY(0,2),		/* 88: kana+shift+2 */
	FKANA|FSHFT|KEY(0,3),		/* 89: kana+shift+3 */
	FKANA|FSHFT|KEY(0,4),		/* 8a: kana+shift+4 */
	FKANA|FSHFT|KEY(0,5),		/* 8b: kana+shift+5 */
	FKANA|FSHFT|KEY(4,3),		/* 8c: kana+shift+n */
	FKANA|FSHFT|KEY(4,2),		/* 8d: kana+shift+m */
	FKANA|FSHFT|KEY(2,2),		/* 8e: kana+shift+, */
	FKANA|FSHFT|KEY(3,0),		/* 8f: kana+shift+c */
	FREASS|0x20,				/* 90: [20] */
	FKANA|KEY(0,1),				/* 91: kana+1 */
	FKANA|KEY(0,2),				/* 92: kana+2 */
	FKANA|KEY(0,3),				/* 93: kana+3 */
	FKANA|KEY(0,4),				/* 94: kana+4 */
	FKANA|KEY(0,5),				/* 95: kana+5 */
	FKANA|KEY(4,6),				/* 96: kana+q */
	FKANA|KEY(5,4),				/* 97: kana+w */
	FKANA|KEY(3,2),				/* 98: kana+e */
	FKANA|KEY(4,7),				/* 99: kana+r */
	FKANA|KEY(5,1),				/* 9a: kana+t */
	FKANA|KEY(2,6),				/* 9b: kana+a */
	FKANA|KEY(5,0),				/* 9c: kana+s */
	FKANA|KEY(3,1),				/* 9d: kana+d */
	FKANA|KEY(3,3),				/* 9e: kana+f */
	FKANA|KEY(3,4),				/* 9f: kana+g */
	FREASS|0x20,				/* a0: [20] */
	FKANA|FSHFT|KEY(2,4),		/* a1: kana+shift+/ */
	FKANA|FSHFT|KEY(1,6),		/* a2: kana+shift+] */
	FKANA|FSHFT|KEY(2,1),		/* a3: kana+shift+` */
	FKANA|FSHFT|KEY(2,3),		/* a4: kana+shift+. */
	FKANA|FSHFT|KEY(2,5),		/* a5: kana+shift+underscore */
	FKANA|FCAPS|KEY(2,4),		/* a6: kana+caps+/ */
	FKANA|FCAPS|FSHFT|KEY(0,1),	/* a7: kana+caps+shift+1 */
	FKANA|FCAPS|FSHFT|KEY(0,2),	/* a8: kana+caps+shift+2 */
	FKANA|FCAPS|FSHFT|KEY(0,3),	/* a9: kana+caps+shift+3 */
	FKANA|FCAPS|FSHFT|KEY(0,4),	/* aa: kana+caps+shift+4 */
	FKANA|FCAPS|FSHFT|KEY(0,5),	/* ab: kana+caps+shift+5 */
	FKANA|FCAPS|FSHFT|KEY(4,3),	/* ac: kana+caps+shift+n */
	FKANA|FCAPS|FSHFT|KEY(4,2),	/* ad: kana+caps+shift+m */
	FKANA|FCAPS|FSHFT|KEY(2,2),	/* ae: kana+caps+shift+, */
	FKANA|FCAPS|FSHFT|KEY(3,0),	/* af: kana+caps+shift+c */
	FKANA|FSHFT|KEY(2,0),		/* b0: kana+shift+' */
	FKANA|FCAPS|KEY(0,1),		/* b1: kana+caps+1 */
	FKANA|FCAPS|KEY(0,2),		/* b2: kana+caps+2 */
	FKANA|FCAPS|KEY(0,3),		/* b3: kana+caps+3 */
	FKANA|FCAPS|KEY(0,4),		/* b4: kana+caps+4 */
	FKANA|FCAPS|KEY(0,5),		/* b5: kana+caps+5 */
	FKANA|FCAPS|KEY(4,6),		/* b6: kana+caps+q */
	FKANA|FCAPS|KEY(5,4),		/* b7: kana+caps+w */
	FKANA|FCAPS|KEY(3,2),		/* b8: kana+caps+e */
	FKANA|FCAPS|KEY(4,7),		/* b9: kana+caps+r */
	FKANA|FCAPS|KEY(5,1),		/* ba: kana+caps+t */
	FKANA|FCAPS|KEY(2,6),		/* bb: kana+caps+a */
	FKANA|FCAPS|KEY(5,0),		/* bc: kana+caps+s */
	FKANA|FCAPS|KEY(3,1),		/* bd: kana+caps+d */
	FKANA|FCAPS|KEY(3,3),		/* be: kana+caps+f */
	FKANA|FCAPS|KEY(3,4),		/* bf: kana+caps+g */
	FKANA|FCAPS|KEY(5,7),		/* c0: kana+caps+z */
	FKANA|FCAPS|KEY(5,5),		/* c1: kana+caps+x */
	FKANA|FCAPS|KEY(3,0),		/* c2: kana+caps+c */
	FKANA|FCAPS|KEY(5,3),		/* c3: kana+caps+v */
	FKANA|FCAPS|KEY(2,7),		/* c4: kana+caps+b */
	FKANA|FCAPS|KEY(0,6),		/* c5: kana+caps+6 */
	FKANA|FCAPS|KEY(0,7),		/* c6: kana+caps+7 */
	FKANA|FCAPS|KEY(1,0),		/* c7: kana+caps+8 */
	FKANA|FCAPS|KEY(1,1),		/* c8: kana+caps+9 */
	FKANA|FCAPS|KEY(0,0),		/* c9: kana+caps+0 */
	FKANA|FCAPS|KEY(5,6),		/* ca: kana+caps+y */
	FKANA|FCAPS|KEY(5,2),		/* cb: kana+caps+u */
	FKANA|FCAPS|KEY(3,6),		/* cc: kana+caps+i */
	FKANA|FCAPS|KEY(4,4),		/* cd: kana+caps+o */
	FKANA|FCAPS|KEY(4,5),		/* ce: kana+caps+p */
	FKANA|FCAPS|KEY(3,5),		/* cf: kana+caps+h */
	FKANA|FCAPS|KEY(3,7),		/* d0: kana+caps+j */
	FKANA|FCAPS|KEY(4,0),		/* d1: kana+caps+k */
	FKANA|FCAPS|KEY(4,1),		/* d2: kana+caps+l */
	FKANA|FCAPS|KEY(1,7),		/* d3: kana+caps+; */
	FKANA|FCAPS|KEY(4,3),		/* d4: kana+caps+n */
	FKANA|FCAPS|KEY(4,2),		/* d5: kana+caps+m */
	FKANA|FCAPS|KEY(2,2),		/* d6: kana+caps+, */
	FKANA|FCAPS|KEY(1,2),		/* d7: kana+caps+- */
	FKANA|FCAPS|KEY(1,3),		/* d8: kana+caps+= */
	FKANA|FCAPS|KEY(1,4),		/* d9: kana+caps+\ */
	FKANA|FCAPS|KEY(1,5),		/* da: kana+caps+[ */
	FKANA|FCAPS|KEY(1,6),		/* db: kana+caps+] */
	FKANA|FCAPS|KEY(2,3),		/* dc: kana+caps+. */
	FKANA|FCAPS|KEY(2,5),		/* dd: kana+caps+underscore */
	FKANA|KEY(2,0),				/* de: kana+' */
	FKANA|KEY(2,1),				/* df: kana+` */
	FKANA|KEY(5,7),				/* e0: kana+z */
	FKANA|KEY(5,5),				/* e1: kana+x */
	FKANA|KEY(3,0),				/* e2: kana+c */
	FKANA|KEY(5,3),				/* e3: kana+v */
	FKANA|KEY(2,7),				/* e4: kana+b */
	FKANA|KEY(0,6),				/* e5: kana+6 */
	FKANA|KEY(0,7),				/* e6: kana+7 */
	FKANA|KEY(1,0),				/* e7: kana+8 */
	FKANA|KEY(1,1),				/* e8: kana+9 */
	FKANA|KEY(0,0),				/* e9: kana+0 */
	FKANA|KEY(5,6),				/* ea: kana+y */
	FKANA|KEY(5,2),				/* eb: kana+u */
	FKANA|KEY(3,6),				/* ec: kana+i */
	FKANA|KEY(4,4),				/* ed: kana+o */
	FKANA|KEY(4,5),				/* ee: kana+p */
	FKANA|KEY(3,5),				/* ef: kana+h */
	FKANA|KEY(3,7),				/* f0: kana+j */
	FKANA|KEY(4,0),				/* f1: kana+k */
	FKANA|KEY(4,1),				/* f2: kana+l */
	FKANA|KEY(1,7),				/* f3: kana+; */
	FKANA|KEY(4,3),				/* f4: kana+n */
	FKANA|KEY(4,2),				/* f5: kana+m */
	FKANA|KEY(2,2),				/* f6: kana+, */
	FKANA|KEY(1,2),				/* f7: kana+- */
	FKANA|KEY(1,3),				/* f8: kana+= */
	FKANA|KEY(1,4),				/* f9: kana+\ */
	FKANA|KEY(1,5),				/* fa: kana+[ */
	FKANA|KEY(1,6),				/* fb: kana+] */
	FKANA|KEY(2,3),				/* fc: kana+. */
	FKANA|KEY(2,5),				/* fd: kana+underscore */
	FREASS|0x20,				/* fe: [20] */
	FREASS|0x20					/* ff: [20] */
},{
	/* Korean (Basic set similar to Japanese, there's more in the charset but it can't be typed(?)) */
	0,							/* 00 */
	FREASS|0x84,				/* 01: see extended table, or [84] */
	FREASS|0x85,				/* 02: [85] */
	FREASS|0x81,				/* 03: [81] */
	FREASS|0x83,				/* 04: [83] */
	FREASS|0x82,				/* 05: [82] */
	FREASS|0x80,				/* 06: [80] */
	0,							/* 07 */
	0,							/* 08: (backspace) */
	KEY(7,3),					/* 09: tab */
	KEY(7,7),					/* 0a: line feed (return) */
	0,							/* 0b */
	0,							/* 0c */
	0,							/* 0d: (carriage return, ignore) */
	0,							/* 0e */
	FREASS|0x84,				/* 0f: [84] */
	0,							/* 10 */
	0,							/* 11 */
	0,							/* 12 */
	0,							/* 13 */
	0,							/* 14 */
	0,							/* 15 */
	0,							/* 16 */
	0,							/* 17 */
	0,							/* 18 */
	0,							/* 19 */
	0,							/* 1a: (end of file) */
	0,							/* 1b: (escape) */
	0,							/* 1c */
	0,							/* 1d */
	0,							/* 1e */
	0,							/* 1f */
	KEY(8,0),					/* 20: space */
	FSHFT|KEY(0,1),				/* 21: ! shift+1 */
	FSHFT|KEY(0,2),				/* 22: " shift+2 */
	FSHFT|KEY(0,3),				/* 23: # shift+3 */
	FSHFT|KEY(0,4),				/* 24: $ shift+4 */
	FSHFT|KEY(0,5),				/* 25: % shift+5 */
	FSHFT|KEY(0,6),				/* 26: & shift+6 */
	FSHFT|KEY(0,7),				/* 27: ' ahift+7 */
	FSHFT|KEY(1,0),				/* 28: ( shift+8 */
	FSHFT|KEY(1,1),				/* 29: ) shift+9 */
	FSHFT|KEY(2,0),				/* 2a: * shift+' */
	FSHFT|KEY(1,7),				/* 2b: + shift+; */
	KEY(2,2),					/* 2c: , */
	KEY(1,2),					/* 2d: - */
	KEY(2,3),					/* 2e: . */
	KEY(2,4),					/* 2f: / */
	KEY(0,0),					/* 30: 0 */
	KEY(0,1),					/* 31: 1 */
	KEY(0,2),					/* 32: 2 */
	KEY(0,3),					/* 33: 3 */
	KEY(0,4),					/* 34: 4 */
	KEY(0,5),					/* 35: 5 */
	KEY(0,6),					/* 36: 6 */
	KEY(0,7),					/* 37: 7 */
	KEY(1,0),					/* 38: 8 */
	KEY(1,1),					/* 39: 9 */
	KEY(2,0),					/* 3a: : ' */
	KEY(1,7),					/* 3b: ; */
	FSHFT|KEY(2,2),				/* 3c: < shift+, */
	FSHFT|KEY(1,2),				/* 3d: = shift+- */
	FSHFT|KEY(2,3),				/* 3e: > shift+. */
	FSHFT|KEY(2,4),				/* 3f: ? shift+/ */
	KEY(1,5),					/* 40: @ [ */
	FSHFT|KEY(2,6),				/* 41: A shift+a */
	FSHFT|KEY(2,7),				/* 42: B shift+b */
	FSHFT|KEY(3,0),				/* 43: C shift+c */
	FSHFT|KEY(3,1),				/* 44: D shift+d */
	FSHFT|KEY(3,2),				/* 45: E shift+e */
	FSHFT|KEY(3,3),				/* 46: F shift+f */
	FSHFT|KEY(3,4),				/* 47: G shift+g */
	FSHFT|KEY(3,5),				/* 48: H shift+h */
	FSHFT|KEY(3,6),				/* 49: I shift+i */
	FSHFT|KEY(3,7),				/* 4a: J shift+j */
	FSHFT|KEY(4,0),				/* 4b: K shift+k */
	FSHFT|KEY(4,1),				/* 4c: L shift+l */
	FSHFT|KEY(4,2),				/* 4d: M shift+m */
	FSHFT|KEY(4,3),				/* 4e: N shift+n */
	FSHFT|KEY(4,4),				/* 4f: O shift+o */
	FSHFT|KEY(4,5),				/* 50: P shift+p */
	FSHFT|KEY(4,6),				/* 51: Q shift+q */
	FSHFT|KEY(4,7),				/* 52: R shift+r */
	FSHFT|KEY(5,0),				/* 53: S shift+s */
	FSHFT|KEY(5,1),				/* 54: T shift+t */
	FSHFT|KEY(5,2),				/* 55: U shift+u */
	FSHFT|KEY(5,3),				/* 56: V shift+v */
	FSHFT|KEY(5,4),				/* 57: W shift+w */
	FSHFT|KEY(5,5),				/* 58: X shift+x */
	FSHFT|KEY(5,6),				/* 59: Y shift+y */
	FSHFT|KEY(5,7),				/* 5a: Z shift+z */
	KEY(1,6),					/* 5b: [ ] */
	KEY(1,4),					/* 5c: \ */
	KEY(2,1),					/* 5d: ] ` */
	KEY(1,3),					/* 5e: ^ = */
	FSHFT|KEY(2,5),				/* 5f: _ shift+underscore */
	FSHFT|KEY(1,5),				/* 60: ` shift+[ */
	KEY(2,6),					/* 61: a */
	KEY(2,7),					/* 62: b */
	KEY(3,0),					/* 63: c */
	KEY(3,1),					/* 64: d */
	KEY(3,2),					/* 65: e */
	KEY(3,3),					/* 66: f */
	KEY(3,4),					/* 67: g */
	KEY(3,5),					/* 68: h */
	KEY(3,6),					/* 69: i */
	KEY(3,7),					/* 6a: j */
	KEY(4,0),					/* 6b: k */
	KEY(4,1),					/* 6c: l */
	KEY(4,2),					/* 6d: m */
	KEY(4,3),					/* 6e: n */
	KEY(4,4),					/* 6f: o */
	KEY(4,5),					/* 70: p */
	KEY(4,6),					/* 71: q */
	KEY(4,7),					/* 72: r */
	KEY(5,0),					/* 73: s */
	KEY(5,1),					/* 74: t */
	KEY(5,2),					/* 75: u */
	KEY(5,3),					/* 76: v */
	KEY(5,4),					/* 77: w */
	KEY(5,5),					/* 78: x */
	KEY(5,6),					/* 79: y */
	KEY(5,7),					/* 7a: z */
	FSHFT|KEY(1,6),				/* 7b: { shift+] */
	FSHFT|KEY(1,4),				/* 7c: | shift+\ */
	FSHFT|KEY(2,1),				/* 7d: } shift+` */
	FSHFT|KEY(1,3),				/* 7e: ~ shift+= */
	0,							/* 7f */
	FGRAPH|KEY(2,4),			/* 80: graph+/ */
	FGRAPH|KEY(2,0),			/* 81: graph+' */
	FGRAPH|KEY(1,7),			/* 82: graph+; */
	FGRAPH|KEY(2,5),			/* 83: graph+underscore */
	FGRAPH|KEY(1,6),			/* 84: graph+] */
	FGRAPH|KEY(2,1),			/* 85: graph+` */
	FKANA|KEY(4,7),				/* 86: hangul+r */
	FKANA|FSHFT|KEY(4,7),		/* 87: hangul+shift+r */
	FKANA|KEY(5,0),				/* 88: hangul+s */
	FKANA|KEY(3,2),				/* 89: hangul+e */
	FKANA|FSHFT|KEY(3,2),		/* 8a: hangul+shift+e */
	FKANA|KEY(3,3),				/* 8b: hangul+f */
	FKANA|KEY(2,6),				/* 8c: hangul+a */
	FKANA|KEY(4,6),				/* 8d: hangul+q */
	FKANA|FSHFT|KEY(4,6),		/* 8e: hangul+shift+q */
	FKANA|KEY(5,1),				/* 8f: hangul+t */
	FKANA|FSHFT|KEY(5,1),		/* 90: hangul+shift+t */
	FKANA|KEY(3,1),				/* 91: hangul+d */
	FKANA|KEY(5,4),				/* 92: hangul+w */
	FKANA|FSHFT|KEY(5,4),		/* 93: hangul+shift+w */
	FKANA|KEY(3,0),				/* 94: hangul+c */
	FKANA|KEY(5,7),				/* 95: hangul+z */
	FKANA|KEY(5,5),				/* 96: hangul+x */
	FKANA|KEY(5,3),				/* 97: hangul+v */
	FKANA|KEY(3,4),				/* 98: hangul+g */
	FKANA|KEY(4,0),				/* 99: hangul+k */
	FKANA|KEY(4,4),				/* 9a: hangul+o */
	FKANA|KEY(3,6),				/* 9b: hangul+i */
	FKANA|FSHFT|KEY(4,4),		/* 9c: hangul+shift+o */
	FKANA|KEY(3,7),				/* 9d: hangul+j */
	FKANA|KEY(4,5),				/* 9e: hangul+p */
	FKANA|KEY(5,2),				/* 9f: hangul+u */
	FKANA|FSHFT|KEY(4,5),		/* a0: hangul+shift+p */
	FKANA|KEY(3,5),				/* a1: hangul+h */
	FKANA|KEY(5,6),				/* a2: hangul+y */
	FKANA|KEY(4,3),				/* a3: hangul+n */
	FKANA|KEY(2,7),				/* a4: hangul+b */
	FKANA|KEY(4,2),				/* a5: hangul+m */
	FKANA|KEY(4,1),				/* a6: hangul+l */
	FREASS|0x20,				/* a7: [20] */
	FREASS|0x20,				/* a8: [20] */
	FREASS|0x20,				/* a9: [20] */
	FREASS|0x20,				/* aa: [20] */
	FREASS|0x20,				/* ab: [20] */
	FREASS|0x20,				/* ac: [20] */
	FREASS|0x20,				/* ad: [20] */
	FREASS|0x20,				/* ae: [20] */
	FREASS|0x20,				/* af: [20] */
	FREASS|0x20,				/* b0: [20] */
	FREASS|0x20,				/* b1: [20] */
	FREASS|0x20,				/* b2: [20] */
	FREASS|0x20,				/* b3: [20] */
	FREASS|0x20,				/* b4: [20] */
	FREASS|0x20,				/* b5: [20] */
	FREASS|0x20,				/* b6: [20] */
	FREASS|0x20,				/* b7: [20] */
	FREASS|0x20,				/* b8: [20] */
	FREASS|0x20,				/* b9: [20] */
	FREASS|0x20,				/* ba: [20] */
	FREASS|0x20,				/* bb: [20] */
	FREASS|0x20,				/* bc: [20] */
	FREASS|0x20,				/* bd: [20] */
	FREASS|0x20,				/* be: [20] */
	FREASS|0x20,				/* bf: [20] */
	FREASS|0x20,				/* c0: [20] */
	FREASS|0x20,				/* c1: [20] */
	FREASS|0x20,				/* c2: [20] */
	FREASS|0x20,				/* c3: [20] */
	FREASS|0x20,				/* c4: [20] */
	FREASS|0x20,				/* c5: [20] */
	FREASS|0x20,				/* c6: [20] */
	FREASS|0x20,				/* c7: [20] */
	FREASS|0x20,				/* c8: [20] */
	FREASS|0x20,				/* c9: [20] */
	FREASS|0x20,				/* ca: [20] */
	FREASS|0x20,				/* cb: [20] */
	FREASS|0x20,				/* cc: [20] */
	FREASS|0x20,				/* cd: [20] */
	FREASS|0x20,				/* ce: [20] */
	FREASS|0x20,				/* cf: [20] */
	FREASS|0x20,				/* d0: [20] */
	FREASS|0x20,				/* d1: [20] */
	FREASS|0x20,				/* d2: [20] */
	FREASS|0x20,				/* d3: [20] */
	FREASS|0x20,				/* d4: [20] */
	FREASS|0x20,				/* d5: [20] */
	FREASS|0x20,				/* d6: [20] */
	FREASS|0x20,				/* d7: [20] */
	FREASS|0x20,				/* d8: [20] */
	FREASS|0x20,				/* d9: [20] */
	FREASS|0x20,				/* da: [20] */
	FREASS|0x20,				/* db: [20] */
	FREASS|0x20,				/* dc: [20] */
	FREASS|0x20,				/* dd: [20] */
	FREASS|0x20,				/* de: [20] */
	FREASS|0x20,				/* df: [20] */
	FREASS|0x20,				/* e0: [20] */
	FREASS|0x20,				/* e1: [20] */
	FREASS|0x20,				/* e2: [20] */
	FREASS|0x20,				/* e3: [20] */
	FREASS|0x20,				/* e4: [20] */
	FREASS|0x20,				/* e5: [20] */
	FREASS|0x20,				/* e6: [20] */
	FREASS|0x20,				/* e7: [20] */
	FREASS|0x20,				/* e8: [20] */
	FREASS|0x20,				/* e9: [20] */
	FREASS|0x20,				/* ea: [20] */
	FREASS|0x20,				/* eb: [20] */
	FREASS|0x20,				/* ec: [20] */
	FREASS|0x20,				/* ed: [20] */
	FREASS|0x20,				/* ee: [20] */
	FREASS|0x20,				/* ef: [20] */
	FREASS|0x20,				/* f0: [20] */
	FREASS|0x20,				/* f1: [20] */
	FREASS|0x20,				/* f2: [20] */
	FREASS|0x20,				/* f3: [20] */
	FREASS|0x20,				/* f4: [20] */
	FREASS|0x20,				/* f5: [20] */
	FREASS|0x20,				/* f6: [20] */
	FREASS|0x20,				/* f7: [20] */
	FREASS|0x20,				/* f8: [20] */
	FREASS|0x20,				/* f9: [20] */
	FREASS|0x20,				/* fa: [20] */
	FREASS|0x20,				/* fb: [20] */
	FREASS|0x20,				/* fc: [20] */
	FREASS|0x20,				/* fd: [20] */
	FREASS|0x20,				/* fe: [20] */
	FREASS|0x20,				/* ff: [20] */
},{
	/* United Kingdom (almost the same: ` key is pound sterling, and code+\ is `) */
	0,							/* 00 */
	FEXT|FREASS|0x41,			/* 01: see extended table, or [01+41] */
	FEXT|FREASS|0x42,			/* 02: [01+42] */
	FEXT|FREASS|0x43,			/* 03: [01+43] */
	FEXT|FREASS|0x44,			/* 04: [01+44] */
	FEXT|FREASS|0x45,			/* 05: [01+45] */
	FEXT|FREASS|0x46,			/* 06: [01+46] */
	FEXT|FREASS|0x47,			/* 07: [01+47] */
	FEXT|FREASS|0x48,			/* 08: [01+48] (backspace) */
	KEY(7,3),					/* 09: tab */
	KEY(7,7),					/* 0a: line feed (return) */
	FEXT|FREASS|0x4b,			/* 0b: [01+4b] */
	FEXT|FREASS|0x4c,			/* 0c: [01+4c] */
	0,							/* 0d: (carriage return, ignore) */
	FEXT|FREASS|0x4e,			/* 0e: [01+4e] */
	FEXT|FREASS|0x4f,			/* 0f: [01+4f] */
	FREASS|0xcf,				/* 10: [cf] */
	FREASS|0xd0,				/* 11: [d0] */
	FREASS|0xd1,				/* 12: [d1] */
	FREASS|0x21,				/* 13: [21] */
	FREASS|0xbe,				/* 14: [be] */
	FREASS|0xbf,				/* 15: [bf] */
	FEXT|FREASS|0x57,			/* 16: [01+57] */
	FREASS|0xd1,				/* 17: [d1] */
	FREASS|0xce,				/* 18: [ce] */
	FREASS|0xcd,				/* 19: [cd] */
	0,							/* 1a: (end of file) */
	FREASS|0xd0,				/* 1b: [d0] (escape) */
	FEXT|FREASS|0x5a,			/* 1c: [01+5a] */
	FREASS|0xd2,				/* 1d: [d2] */
	FREASS|0xce,				/* 1e: [ce] */
	FREASS|0xcd,				/* 1f: [cd] */
	KEY(8,0),					/* 20: space */
	FSHFT|KEY(0,1),				/* 21: ! shift+1 */
	FSHFT|KEY(2,0),				/* 22: " shift+' */
	FSHFT|KEY(0,3),				/* 23: # shift+3 */
	FSHFT|KEY(0,4),				/* 24: $ shift+4 */
	FSHFT|KEY(0,5),				/* 25: % shift+5 */
	FSHFT|KEY(0,7),				/* 26: & shift+7 */
	KEY(2,0),					/* 27: ' */
	FSHFT|KEY(1,1),				/* 28: ( shift+9 */
	FSHFT|KEY(0,0),				/* 29: ) shift+0 */
	FSHFT|KEY(1,0),				/* 2a: * shift+8 */
	FSHFT|KEY(1,3),				/* 2b: + shift+= */
	KEY(2,2),					/* 2c: , */
	KEY(1,2),					/* 2d: - */
	KEY(2,3),					/* 2e: . */
	KEY(2,4),					/* 2f: / */
	KEY(0,0),					/* 30: 0 */
	KEY(0,1),					/* 31: 1 */
	KEY(0,2),					/* 32: 2 */
	KEY(0,3),					/* 33: 3 */
	KEY(0,4),					/* 34: 4 */
	KEY(0,5),					/* 35: 5 */
	KEY(0,6),					/* 36: 6 */
	KEY(0,7),					/* 37: 7 */
	KEY(1,0),					/* 38: 8 */
	KEY(1,1),					/* 39: 9 */
	FSHFT|KEY(1,7),				/* 3a: : shift+; */
	KEY(1,7),					/* 3b: ; */
	FSHFT|KEY(2,2),				/* 3c: < shift+, */
	KEY(1,3),					/* 3d: = */
	FSHFT|KEY(2,3),				/* 3e: > shift+. */
	FSHFT|KEY(2,4),				/* 3f: ? shift+/ */
	FSHFT|KEY(0,2),				/* 40: @ shift+2 */
	FSHFT|KEY(2,6),				/* 41: A shift+a */
	FSHFT|KEY(2,7),				/* 42: B shift+b */
	FSHFT|KEY(3,0),				/* 43: C shift+c */
	FSHFT|KEY(3,1),				/* 44: D shift+d */
	FSHFT|KEY(3,2),				/* 45: E shift+e */
	FSHFT|KEY(3,3),				/* 46: F shift+f */
	FSHFT|KEY(3,4),				/* 47: G shift+g */
	FSHFT|KEY(3,5),				/* 48: H shift+h */
	FSHFT|KEY(3,6),				/* 49: I shift+i */
	FSHFT|KEY(3,7),				/* 4a: J shift+j */
	FSHFT|KEY(4,0),				/* 4b: K shift+k */
	FSHFT|KEY(4,1),				/* 4c: L shift+l */
	FSHFT|KEY(4,2),				/* 4d: M shift+m */
	FSHFT|KEY(4,3),				/* 4e: N shift+n */
	FSHFT|KEY(4,4),				/* 4f: O shift+o */
	FSHFT|KEY(4,5),				/* 50: P shift+p */
	FSHFT|KEY(4,6),				/* 51: Q shift+q */
	FSHFT|KEY(4,7),				/* 52: R shift+r */
	FSHFT|KEY(5,0),				/* 53: S shift+s */
	FSHFT|KEY(5,1),				/* 54: T shift+t */
	FSHFT|KEY(5,2),				/* 55: U shift+u */
	FSHFT|KEY(5,3),				/* 56: V shift+v */
	FSHFT|KEY(5,4),				/* 57: W shift+w */
	FSHFT|KEY(5,5),				/* 58: X shift+x */
	FSHFT|KEY(5,6),				/* 59: Y shift+y */
	FSHFT|KEY(5,7),				/* 5a: Z shift+z */
	KEY(1,5),					/* 5b: [ */
	KEY(1,4),					/* 5c: \ */
	KEY(1,6),					/* 5d: ] */
	FSHFT|KEY(0,6),				/* 5e: ^ shift+6 */
	FSHFT|KEY(1,2),				/* 5f: _ shift+- */
	FCODE|KEY(1,4),				/* 60: ` code+\ */
	KEY(2,6),					/* 61: a */
	KEY(2,7),					/* 62: b */
	KEY(3,0),					/* 63: c */
	KEY(3,1),					/* 64: d */
	KEY(3,2),					/* 65: e */
	KEY(3,3),					/* 66: f */
	KEY(3,4),					/* 67: g */
	KEY(3,5),					/* 68: h */
	KEY(3,6),					/* 69: i */
	KEY(3,7),					/* 6a: j */
	KEY(4,0),					/* 6b: k */
	KEY(4,1),					/* 6c: l */
	KEY(4,2),					/* 6d: m */
	KEY(4,3),					/* 6e: n */
	KEY(4,4),					/* 6f: o */
	KEY(4,5),					/* 70: p */
	KEY(4,6),					/* 71: q */
	KEY(4,7),					/* 72: r */
	KEY(5,0),					/* 73: s */
	KEY(5,1),					/* 74: t */
	KEY(5,2),					/* 75: u */
	KEY(5,3),					/* 76: v */
	KEY(5,4),					/* 77: w */
	KEY(5,5),					/* 78: x */
	KEY(5,6),					/* 79: y */
	KEY(5,7),					/* 7a: z */
	FSHFT|KEY(1,5),				/* 7b: { shift+[ */
	FSHFT|KEY(1,4),				/* 7c: | shift+\ */
	FSHFT|KEY(1,6),				/* 7d: } shift+] */
	FSHFT|KEY(2,1),				/* 7e: ~ shift+` */
	FREASS|0xd8,				/* 7f: [d8] */
	FSHFT|FCODE|KEY(1,1),		/* 80: Ä shift+code+9 */
	FCODE|KEY(3,4),				/* 81: Å code+g */
	FCODE|KEY(5,2),				/* 82: Ç code+u */
	FCODE|KEY(4,6),				/* 83: É code+q */
	FCODE|KEY(2,6),				/* 84: Ñ code+a */
	FCODE|KEY(5,7),				/* 85: Ö code+z */
	FCODE|KEY(2,2),				/* 86: Ü code+, */
	FCODE|KEY(1,1),				/* 87: á code+9 */
	FCODE|KEY(5,4),				/* 88: à code+w */
	FCODE|KEY(5,0),				/* 89: â code+s */
	FCODE|KEY(5,5),				/* 8a: ä code+x */
	FCODE|KEY(3,1),				/* 8b: ã code+d */
	FCODE|KEY(3,2),				/* 8c: å code+e */
	FCODE|KEY(3,0),				/* 8d: ç code+c */
	FSHFT|FCODE|KEY(2,6),		/* 8e: é shift+code+a */
	FSHFT|FCODE|KEY(2,2),		/* 8f: è shift+code+, */
	FSHFT|FCODE|KEY(5,2),		/* 90: ê shift+code+u */
	FCODE|KEY(3,7),				/* 91: ë code+j */
	FSHFT|FCODE|KEY(3,7),		/* 92: í shift+code+j */
	FCODE|KEY(4,7),				/* 93: ì code+r */
	FCODE|KEY(3,3),				/* 94: î code+f */
	FCODE|KEY(5,3),				/* 95: ï code+v */
	FCODE|KEY(5,1),				/* 96: ñ code+t */
	FCODE|KEY(2,7),				/* 97: ó code+b */
	FCODE|KEY(0,5),				/* 98: ò code+5 */
	FSHFT|FCODE|KEY(3,3),		/* 99: ô shift+code+f */
	FSHFT|FCODE|KEY(3,4),		/* 9a: ö shift+code+g */
	FCODE|KEY(0,4),				/* 9b: õ code+4 */
	KEY(2,1),					/* 9c: ú ` */
	FSHFT|FCODE|KEY(0,5),		/* 9d: ù shift+code+5 */
	FSHFT|FCODE|KEY(0,2),		/* 9e: û shift+code+2 */
	FCODE|KEY(0,1),				/* 9f: ü code+1 */
	FCODE|KEY(5,6),				/* a0: † code+y */
	FCODE|KEY(3,6),				/* a1: ° code+i */
	FCODE|KEY(4,4),				/* a2: ¢ code+o */
	FCODE|KEY(4,5),				/* a3: £ code+p */
	FCODE|KEY(4,3),				/* a4: § code+n */
	FSHFT|FCODE|KEY(4,3),		/* a5: • shift+code+n */
	FCODE|KEY(2,3),				/* a6: ¶ code+. */
	FCODE|KEY(2,4),				/* a7: ß code+/ */
	FSHFT|FCODE|KEY(2,4),		/* a8: ® shift+code+/ */
	FSHFT|FGRAPH|KEY(4,7),		/* a9: © shift+graph+r */
	FSHFT|FGRAPH|KEY(5,6),		/* aa: ™ shift+graph+y */
	FGRAPH|KEY(0,2),			/* ab: ´ graph+2 */
	FGRAPH|KEY(0,1),			/* ac: ¨ graph+1 */
	FSHFT|FCODE|KEY(0,1),		/* ad: ≠ shift+code+1 */
	FSHFT|FGRAPH|KEY(2,2),		/* ae: Æ shift+graph+, */
	FSHFT|FGRAPH|KEY(2,3),		/* af: Ø shift+graph+. */
	FSHFT|FCODE|KEY(3,5),		/* b0: shift+code+h */
	FCODE|KEY(3,5),				/* b1: code+h */
	FSHFT|FCODE|KEY(4,0),		/* b2: shift+code+k */
	FCODE|KEY(4,0),				/* b3: code+k */
	FSHFT|FCODE|KEY(4,1),		/* b4: shift+code+l */
	FCODE|KEY(4,1),				/* b5: code+l */
	FSHFT|FCODE|KEY(1,7),		/* b6: shift+code+; */
	FCODE|KEY(1,7),				/* b7: code+; */
	FSHFT|FCODE|KEY(2,0),		/* b8: shift+code+' */
	FCODE|KEY(2,0),				/* b9: code+' */
	FGRAPH|KEY(0,3),			/* ba: graph+3 */
	FGRAPH|KEY(2,1),			/* bb: graph+` */
	FGRAPH|KEY(3,0),			/* bc: graph+c */
	FGRAPH|KEY(0,5),			/* bd: graph+5 */
	FSHFT|FCODE|KEY(0,3),		/* be: shift+code+3 */
	FCODE|KEY(0,3),				/* bf: code+3 */
	FGRAPH|KEY(5,2),			/* c0: graph+u */
	FSHFT|FGRAPH|KEY(3,1),		/* c1: shift+graph+d */
	FGRAPH|KEY(4,4),			/* c2: graph+o */
	FSHFT|FGRAPH|KEY(4,4),		/* c3: shift+graph+o */
	FGRAPH|KEY(2,6),			/* c4: graph+a */
	FSHFT|FGRAPH|KEY(5,2),		/* c5: shift+graph+u */
	FGRAPH|KEY(3,7),			/* c6: graph+j */
	FGRAPH|KEY(3,1),			/* c7: graph+d */
	FGRAPH|KEY(4,1),			/* c8: graph+l */
	FSHFT|FGRAPH|KEY(4,1),		/* c9: shift+graph+l */
	FSHFT|FGRAPH|KEY(3,7),		/* ca: shift+graph+j */
	FSHFT|FGRAPH|KEY(4,6),		/* cb: shift+graph+q */
	FGRAPH|KEY(4,6),			/* cc: graph+q */
	FGRAPH|KEY(3,2),			/* cd: graph+e */
	FSHFT|FGRAPH|KEY(3,2),		/* ce: shift+graph+e */
	FGRAPH|KEY(5,4),			/* cf: graph+w */
	FSHFT|FGRAPH|KEY(5,4),		/* d0: shift+graph+w */
	FSHFT|FGRAPH|KEY(5,0),		/* d1: shift+graph+s */
	FGRAPH|KEY(5,0),			/* d2: graph+s */
	FSHFT|FGRAPH|KEY(4,3),		/* d3: shift+graph+n */
	FSHFT|FGRAPH|KEY(3,3),		/* d4: shift+graph+f */
	FSHFT|FGRAPH|KEY(5,3),		/* d5: shift+graph+v */
	FSHFT|FGRAPH|KEY(3,5),		/* d6: shift+graph+h */
	FSHFT|FGRAPH|KEY(4,5),		/* d7: shift+graph+p */
	FSHFT|FCODE|KEY(0,0),		/* d8: shift+code+0 */
	FCODE|KEY(0,2),				/* d9: code+2 */
	FCODE|KEY(1,6),				/* da: code+] */
	FGRAPH|KEY(4,5),			/* db: € graph+p */
	FGRAPH|KEY(3,6),			/* dc: ‹ graph+i */
	FGRAPH|KEY(4,0),			/* dd: › graph+k */
	FSHFT|FGRAPH|KEY(4,0),		/* de: ﬁ shift+graph+k */
	FSHFT|FGRAPH|KEY(3,6),		/* df: ﬂ shift+graph+i */
	FCODE|KEY(0,6),				/* e0: ‡ code+6 */
	FCODE|KEY(0,7),				/* e1: · code+7 */
	FSHFT|FCODE|KEY(1,0),		/* e2: ‚ shift+code+8 */
	FSHFT|FCODE|KEY(4,5),		/* e3: „ shift+code+p */
	FSHFT|FCODE|KEY(2,1),		/* e4: ‰ shift+code+` */
	FCODE|KEY(2,1),				/* e5: Â code+` */
	FCODE|KEY(4,2),				/* e6: Ê code+m */
	FCODE|KEY(1,0),				/* e7: Á code+8 */
	FSHFT|FCODE|KEY(1,5),		/* e8: Ë shift+code+[ */
	FCODE|KEY(1,3),				/* e9: È code+= */
	FSHFT|FCODE|KEY(1,6),		/* ea: Í shift+code+] */
	FCODE|KEY(0,0),				/* eb: Î code+0 */
	FGRAPH|KEY(1,0),			/* ec: Ï graph+8 */
	FCODE|KEY(1,5),				/* ed: Ì code+[ */
	FCODE|KEY(1,2),				/* ee: Ó code+- */
	FGRAPH|KEY(0,4),			/* ef: Ô graph+4 */
	FSHFT|FGRAPH|KEY(1,3),		/* f0:  shift+graph+= */
	FGRAPH|KEY(1,3),			/* f1: Ò graph+= */
	FGRAPH|KEY(2,3),			/* f2: Ú graph+. */
	FGRAPH|KEY(2,2),			/* f3: Û graph+, */
	FGRAPH|KEY(0,6),			/* f4: Ù graph+6 */
	FSHFT|FGRAPH|KEY(0,6),		/* f5: ı shift+graph+6 */
	FSHFT|FGRAPH|KEY(2,4),		/* f6: ˆ shift+graph+/ */
	FSHFT|FGRAPH|KEY(2,1),		/* f7: ˜ shift+graph+` */
	FSHFT|FGRAPH|KEY(5,7),		/* f8: ¯ shift+graph+z */
	FSHFT|FGRAPH|KEY(5,5),		/* f9: shift+graph+x */
	FSHFT|FGRAPH|KEY(3,0),		/* fa: shift+graph+c */
	FGRAPH|KEY(0,7),			/* fb: ˚ graph+7 */
	FSHFT|FGRAPH|KEY(0,3),		/* fc: ¸ shift+graph+3 */
	FSHFT|FGRAPH|KEY(0,2),		/* fd: ˝ shift+graph+2 */
	FSHFT|FGRAPH|KEY(2,6),		/* fe: shift+graph+a */
	FREASS|0x20					/* ff: [20] */
} };


/* extended table */
static const u16 pc2msxe[4][0x1f]={
{
	/* International */
	FEXT|FGRAPH|KEY(1,5),		/* 01+41: graph+[ */
	FEXT|FSHFT|FGRAPH|KEY(1,5),	/* 01+42: shift+graph+[ */
	FEXT|FSHFT|FGRAPH|KEY(2,0),	/* 01+43: shift+graph+' */
	FEXT|FSHFT|FGRAPH|KEY(1,7),	/* 01+44: shift+graph+; */
	FEXT|FGRAPH|KEY(2,0),		/* 01+45: graph+' */
	FEXT|FGRAPH|KEY(1,7),		/* 01+46: graph+; */
	FEXT|FGRAPH|KEY(1,1),		/* 01+47: graph+9 */
	FEXT|FSHFT|FGRAPH|KEY(1,1),	/* 01+48: shift+graph+9 */
	FEXT|FGRAPH|KEY(0,0),		/* 01+49: graph+0 */
	FEXT|FSHFT|FGRAPH|KEY(0,0),	/* 01+4a: shift+graph+0 */
	FEXT|FGRAPH|KEY(4,2),		/* 01+4b: graph+m */
	FEXT|FSHFT|FGRAPH|KEY(4,2),	/* 01+4c: shift+graph+m */
	FEXT|FGRAPH|KEY(1,6),		/* 01+4d: graph+] */
	FEXT|FSHFT|FGRAPH|KEY(1,6),	/* 01+4e: shift+graph+] */
	FEXT|FGRAPH|KEY(5,7),		/* 01+4f: graph+z */
	FEXT|FSHFT|FGRAPH|KEY(3,4),	/* 01+50: shift+graph+g */
	FEXT|FGRAPH|KEY(2,7),		/* 01+51: graph+b */
	FEXT|FGRAPH|KEY(5,1),		/* 01+52: graph+t */
	FEXT|FGRAPH|KEY(3,5),		/* 01+53: graph+h */
	FEXT|FGRAPH|KEY(3,3),		/* 01+54: graph+f */
	FEXT|FGRAPH|KEY(3,4),		/* 01+55: graph+g */
	FEXT|FSHFT|FGRAPH|KEY(1,4),	/* 01+56: shift+graph+\ */
	FEXT|FGRAPH|KEY(1,2),		/* 01+57: graph+- */
	FEXT|FGRAPH|KEY(4,7),		/* 01+58: graph+r */
	FEXT|FGRAPH|KEY(5,6),		/* 01+59: graph+y */
	FEXT|FGRAPH|KEY(5,3),		/* 01+5a: graph+v */
	FEXT|FGRAPH|KEY(4,3),		/* 01+5b: graph+n */
	FEXT|FGRAPH|KEY(5,5),		/* 01+5c: graph+x */
	FEXT|FGRAPH|KEY(2,4),		/* 01+5d: graph+/ */
	FEXT|FGRAPH|KEY(1,4),		/* 01+5e: graph+\ */
	FEXT|FSHFT|FGRAPH|KEY(1,2)	/* 01+5f: shift+graph+- */
},{
	/* Japanese */
	FEXT|FGRAPH|KEY(0,2),		/* 01+41: graph+2 */
	FEXT|FGRAPH|KEY(0,3),		/* 01+42: graph+3 */
	FEXT|FGRAPH|KEY(0,4),		/* 01+43: graph+4 */
	FEXT|FGRAPH|KEY(0,5),		/* 01+44: graph+5 */
	FEXT|FGRAPH|KEY(0,6),		/* 01+45: graph+6 */
	FEXT|FGRAPH|KEY(0,7),		/* 01+46: graph+7 */
	FEXT|FGRAPH|KEY(0,1),		/* 01+47: graph+1 */
	FEXT|FGRAPH|KEY(5,6),		/* 01+48: graph+y */
	FEXT|FGRAPH|KEY(1,4),		/* 01+49: graph+\ */
	FEXT|FGRAPH|KEY(3,5),		/* 01+4a: graph+h */
	FEXT|FGRAPH|KEY(4,2),		/* 01+4b: graph+m */
	FEXT|FGRAPH|KEY(5,0),		/* 01+4c: graph+s */
	FEXT|FGRAPH|KEY(1,0),		/* 01+4d: graph+8 */
	FEXT|FGRAPH|KEY(1,1),		/* 01+4e: graph+9 */
	FEXT|FGRAPH|KEY(0,0),		/* 01+4f: graph+0 */
	FEXT|FGRAPH|KEY(4,5),		/* 01+50: „ graph+p */
	FEXT|FGRAPH|KEY(5,3),		/* 01+51: graph+v */
	FEXT|FGRAPH|KEY(4,7),		/* 01+52: graph+r */
	FEXT|FGRAPH|KEY(3,4),		/* 01+53: graph+g */
	FEXT|FGRAPH|KEY(3,1),		/* 01+54: graph+d */
	FEXT|FGRAPH|KEY(3,3),		/* 01+55: graph+f */
	FEXT|FGRAPH|KEY(3,6),		/* 01+56: graph+i */
	FEXT|FGRAPH|KEY(1,2),		/* 01+57: graph+- */
	FEXT|FGRAPH|KEY(3,2),		/* 01+58: graph+e */
	FEXT|FGRAPH|KEY(5,1),		/* 01+59: graph+t */
	FEXT|FGRAPH|KEY(3,0),		/* 01+5a: graph+c */
	FEXT|FGRAPH|KEY(2,7),		/* 01+5b: graph+b */
	FEXT|FGRAPH|KEY(5,5),		/* 01+5c: graph+x */
	FEXT|FGRAPH|KEY(2,3),		/* 01+5d: graph+. */
	FEXT|FGRAPH|KEY(4,1),		/* 01+5e: graph+l */
	FEXT|FGRAPH|KEY(2,2)		/* 01+5f: graph+, */
},{
	/* Korean */
	FEXT,						/* 01+41 */
	FEXT,						/* 01+42 */
	FEXT,						/* 01+43 */
	FEXT,						/* 01+44 */
	FEXT,						/* 01+45 */
	FEXT,						/* 01+46 */
	FEXT,						/* 01+47 */
	FEXT,						/* 01+48 */
	FEXT,						/* 01+49 */
	FEXT,						/* 01+4a */
	FEXT,						/* 01+4b */
	FEXT,						/* 01+4c */
	FEXT,						/* 01+4d */
	FEXT,						/* 01+4e */
	FEXT,						/* 01+4f */
	FEXT|FGRAPH|KEY(4,5),		/* 01+50: graph+p */
	FEXT|FGRAPH|KEY(5,3),		/* 01+51: graph+v */
	FEXT|FGRAPH|KEY(4,7),		/* 01+52: graph+r */
	FEXT|FGRAPH|KEY(3,4),		/* 01+53: graph+g */
	FEXT|FGRAPH|KEY(3,1),		/* 01+54: graph+d */
	FEXT|FGRAPH|KEY(3,3),		/* 01+55: graph+f */
	FEXT|FGRAPH|KEY(3,6),		/* 01+56: graph+i */
	FEXT|FGRAPH|KEY(1,2),		/* 01+57: graph+- */
	FEXT|FGRAPH|KEY(3,2),		/* 01+58: graph+e */
	FEXT|FGRAPH|KEY(5,1),		/* 01+59: graph+t */
	FEXT|FGRAPH|KEY(3,0),		/* 01+5a: graph+c */
	FEXT|FGRAPH|KEY(2,7),		/* 01+5b: graph+b */
	FEXT|FGRAPH|KEY(5,5),		/* 01+5c: graph+x */
	FEXT,						/* 01+5d */
	FEXT,						/* 01+5e */
	FEXT						/* 01+5f */
},{
	/* United Kingdom (same as International) */
	FEXT|FGRAPH|KEY(1,5),			/* 01+41: graph+[ */
	FEXT|FSHFT|FGRAPH|KEY(1,5),	/* 01+42: shift+graph+[ */
	FEXT|FSHFT|FGRAPH|KEY(2,0),	/* 01+43: shift+graph+' */
	FEXT|FSHFT|FGRAPH|KEY(1,7),	/* 01+44: shift+graph+; */
	FEXT|FGRAPH|KEY(2,0),		/* 01+45: graph+' */
	FEXT|FGRAPH|KEY(1,7),		/* 01+46: graph+; */
	FEXT|FGRAPH|KEY(1,1),		/* 01+47: graph+9 */
	FEXT|FSHFT|FGRAPH|KEY(1,1),	/* 01+48: shift+graph+9 */
	FEXT|FGRAPH|KEY(0,0),		/* 01+49: graph+0 */
	FEXT|FSHFT|FGRAPH|KEY(0,0),	/* 01+4a: shift+graph+0 */
	FEXT|FGRAPH|KEY(4,2),		/* 01+4b: graph+m */
	FEXT|FSHFT|FGRAPH|KEY(4,2),	/* 01+4c: shift+graph+m */
	FEXT|FGRAPH|KEY(1,6),		/* 01+4d: graph+] */
	FEXT|FSHFT|FGRAPH|KEY(1,6),	/* 01+4e: shift+graph+] */
	FEXT|FGRAPH|KEY(5,7),		/* 01+4f: graph+z */
	FEXT|FSHFT|FGRAPH|KEY(3,4),	/* 01+50: shift+graph+g */
	FEXT|FGRAPH|KEY(2,7),		/* 01+51: graph+b */
	FEXT|FGRAPH|KEY(5,1),		/* 01+52: graph+t */
	FEXT|FGRAPH|KEY(3,5),		/* 01+53: graph+h */
	FEXT|FGRAPH|KEY(3,3),		/* 01+54: graph+f */
	FEXT|FGRAPH|KEY(3,4),		/* 01+55: graph+g */
	FEXT|FSHFT|FGRAPH|KEY(1,4),	/* 01+56: shift+graph+\ */
	FEXT|FGRAPH|KEY(1,2),		/* 01+57: graph+- */
	FEXT|FGRAPH|KEY(4,7),		/* 01+58: graph+r */
	FEXT|FGRAPH|KEY(5,6),		/* 01+59: graph+y */
	FEXT|FGRAPH|KEY(5,3),		/* 01+5a: graph+v */
	FEXT|FGRAPH|KEY(4,3),		/* 01+5b: graph+n */
	FEXT|FGRAPH|KEY(5,5),		/* 01+5c: graph+x */
	FEXT|FGRAPH|KEY(2,4),		/* 01+5d: graph+/ */
	FEXT|FGRAPH|KEY(1,4),		/* 01+5e: graph+\ */
	FEXT|FSHFT|FGRAPH|KEY(1,2)	/* 01+5f: shift+graph+- */
} };
