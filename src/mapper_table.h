/******************************************************************************
 *                                                                            *
 *                            "mapper_table.h"                                *
 *                                                                            *
 ******************************************************************************/

/* Lookup table for known correct dumps that require extra information, and some free games that aren't autoguessed.
I don't like this much (eg. hack a ROM and it won't be detected properly anymore), but as MSX ROMs don't include metadata
such as a short header like with NES ROMs, this is a must (or at least convenient). */

typedef struct {
	u32 crc;
	u32 size;

	int type;

	/* extra config info */
	u32 extra1;
	u32 extra2;

} _lutmaptype;

/* official ASCII type MegaROMs */
#define A				CARTTYPE_ASCII

typedef struct {
	u32 mainw;
	u32 board;
	char company[0x10];
	char name[0x10];
} _lutasciiboard;

typedef struct {
	u32 uid;
	char company[0x10];
	char name[0x10];
	u32 flags;
} _lutasciichip;

#define A_MCF_SRAM		1	/* can have SRAM */
#define A_MCF_PROTECT	2	/* can have SRAM write protect */
#define A_MCF_SWLOW		4	/* allows SRAM writes to $4000-$5fff */

/* first dword is main bankswitch register range info:
 c = bank (same as e, but this is clearer)
 e = don't care
 f = unmapped if set
 5 = enable SRAM
*/
/* second dword contains board configuration: */

/* bits 0,1: bank size:
0=8KB, 3=16KB, 1 and 2 reserved for possible bugged hardware with one
half configured as 8KB and the other half as 16KB and vice versa */
#define A_BS_SHIFT		0
#define A_BS_MASK		(3<<A_BS_SHIFT)
#define A_BS(x)			((x)<<A_BS_SHIFT&A_BS_MASK)

#define A_08			A_BS(0) /* standard ASCII 8 */
#define A_16			A_BS(3) /* standard ASCII 16 */

/* bits 2-9: mapper chip */
#define A_MC_SHIFT		2
#define A_MC_MASK		(0xf<<A_MC_SHIFT)
#define A_MC(x)			((x)<<A_MC_SHIFT&A_MC_MASK)

#define A_MC_LZ93A13	A_MC(0) /* Sharp LZ93A13						32 pins	(ASC1) */
#define A_MC_M6K2125	A_MC(1) /* Mitsubishi Electric M60002-0125SP	42 pins	(ASC2) */
#define A_MC_IBS6101	A_MC(2) /* IS BS6101							42 pins	(ASC3) */
#define A_MC_NMR6401	A_MC(3) /* NEOS MR6401							28 pins	(ASC4) */
#define A_MC_IBS6202	A_MC(4) /* IS BS6202							42 pins	       */
/* unconfirmed: Mitsubishi Electric M60002-0115FP, supposedly used in "Junior Mate" */

/* chip,		company,name,			flags (known differences) */
static const _lutasciichip lutasciichip[]={
{ A_MC_IBS6101, "IS",	"BS6101",		0							},
{ A_MC_IBS6202, "IS",	"BS6202",		0							},
{ A_MC_M6K2125, "ME",	"M60002-0125SP",A_MCF_SRAM | A_MCF_PROTECT	}, /* ME = Mitsubishi Electric */
{ A_MC_NMR6401, "NEOS",	"MR6401",		0							},
{ A_MC_LZ93A13, "Sharp","LZ93A13",		A_MCF_SRAM | A_MCF_SWLOW	},
{ ~0,           " ",	" ",			0							}
};

/* bits 10,11,12: SRAM size in KB:
0=no SRAM, 1=1KB, 2=2KB, 3=4KB, 4=8KB, 5=16KB, 6=32KB, 7=64KB
(officially, only 0, 2, 4, 6, always assume it's battery-backed) */
#define A_SS_SHIFT		10
#define A_SS_MASK		(7<<A_SS_SHIFT)
#define A_SS(x)			((x)<<A_SS_SHIFT&A_SS_MASK)

#define A_SS_NO			0		/* no SRAM */
#define A_SS_02			A_SS(2)	/* 2KB */
#define A_SS_08			A_SS(4)	/* 8KB */
#define A_SS_32			A_SS(6)	/* 32KB */

/* bits 13-16: SRAM write-protect bit (+1) */
#define A_SP_SHIFT		13
#define A_SP_MASK		(0xf<<A_SP_SHIFT)
#define A_SP(x)			(((x)+1)<<A_SP_SHIFT&A_SP_MASK)

#define A_SP_NO			0
#define A_SPSTD			A_SP(3)	/* all games known to have write-protect have it on bit 3 */

/* bits 17-24: board */
#define AB_SHIFT		17
#define AB_MASK			(0xff<<AB_SHIFT)
#define A_BOARD(x)		((x)<<AB_SHIFT&AB_MASK)

#define AB_CUSTOM		0

/* known std boards: */
#define AB_TAS1M008S	A_BOARD(1)
#define AB_TAS1M016S	A_BOARD(2)
#define AB_TAS4M01603M	A_BOARD(3)
#define AB_TAS2M008E2M	A_BOARD(4)
#define AB_TAS4M008M	A_BOARD(5)
#define AB_TAS2M20803A	A_BOARD(6)
#define AB_TAS4M20803A	A_BOARD(7)
#define AB_TAS8M30803A	A_BOARD(8)
#define AB_TAS8M30864	A_BOARD(9)
#define AB_TAS4M11603A	A_BOARD(10)
#define AB_TAS1M216A	A_BOARD(11)
#define AB_TA1M8K		A_BOARD(12)
#define AB_TA1M16K		A_BOARD(13)
#define AB_TA2M8K		A_BOARD(14)
#define AB_TA2M16K		A_BOARD(15)
#define AB_TA6228		A_BOARD(16)
#define AB_TA6218K		A_BOARD(17)
#define AB_TA62164K		A_BOARD(18)
#define AB_TA621KAGA	A_BOARD(19)
#define AB_TA621M		A_BOARD(20)
#define AB_MSX2MX8K		A_BOARD(21)
#define AB_MSX2MX16K	A_BOARD(22)
#define AB_IS020300		A_BOARD(23)
#define AB_IS030010		A_BOARD(24)
#define AB_1M8KB		A_BOARD(25)
#define AB_1M16KB		A_BOARD(26)
#define AB_2M8KB		A_BOARD(27)
#define AB_2M16KB		A_BOARD(28)
#define AB_TAS4M30803A	A_BOARD(29)

/* writerange,board,             banksize, mapper chip,   sramsize, sram write-protect ... company, name */
static const _lutasciiboard lutasciiboard[]={
{ 0xeeffffff, AB_TA1M8K        | A_08    | A_MC_M6K2125                     , "ASCII",	"TA-1M(8K)"		},
{ 0xeeffefff, AB_TA1M16K       | A_16    | A_MC_M6K2125                     , "ASCII",	"TA-1M(16K)"	},
{ 0xeeffffff, AB_TA2M8K        | A_08    | A_MC_M6K2125                     , "ASCII",	"TA-2M 8K"		},
{ 0xeeffffff, AB_TA2M16K       | A_16    | A_MC_M6K2125                     , "ASCII",	"TA-2M 16K"		},
{ 0xeef5ffff, AB_TA6218K       | A_08    | A_MC_M6K2125 | A_SS_02 | A_SPSTD , "ASCII",	"TA-621-8K"		}, /* 2KB SRAM, even though 8K implies 8Kbit(1KB) */
{ 0xeef5ffff, AB_TA62164K      | A_08    | A_MC_M6K2125 | A_SS_08 | A_SPSTD , "ASCII",	"TA-621-64K"	},
{ 0xef5fffff, AB_TA621KAGA     | A_08    | A_MC_M6K2125 | A_SS_08 | A_SP_NO , "ASCII",	"TA-621 KAGA"	}, /* no SRAM write-protect */
{ 0xeef5efff, AB_TA621M        | A_16    | A_MC_M6K2125 | A_SS_02 | A_SPSTD , "ASCII",	"TA-621M"		},
{ 0xefffffff, AB_TA6228        | A_08    | A_MC_M6K2125                     , "ASCII",	"TA6228"		},
{ 0xfeeeffff, AB_TAS1M008S     | A_08    | A_MC_LZ93A13                     , "ASCII",	"TAS-1M008S"	},
{ 0xfeeeefff, AB_TAS1M016S     | A_16    | A_MC_LZ93A13                     , "ASCII",	"TAS-1M016S"	},
{ 0xfee5efff, AB_TAS1M216A     | A_16    | A_MC_LZ93A13 | A_SS_08           , "ASCII",	"TAS-1M216A"	},
{ 0xfeefffff, AB_TAS2M008E2M   | A_08    | A_MC_LZ93A13                     , "ASCII",	"TAS-2M008-E2M"	},
{ 0xee5fffff, AB_TAS2M20803A   | A_08    | A_MC_LZ93A13 | A_SS_08           , "ASCII",	"TAS-2M208-03A"	},
{ 0xffefffff, AB_TAS4M008M     | A_08    | A_MC_LZ93A13                     , "ASCII",	"TAS-4M-008M"	},
{ 0xffeeffff, AB_TAS4M01603M   | A_16    | A_MC_LZ93A13                     , "ASCII",	"TAS-4M016-03M"	},
{ 0x5eeeffff, AB_TAS4M11603A   | A_16    | A_MC_LZ93A13 | A_SS_02           , "ASCII",	"TAS-4M116-03A"	},
{ 0xe5ffffff, AB_TAS4M20803A   | A_08    | A_MC_LZ93A13 | A_SS_08           , "ASCII",	"TAS-4M208-03A"	},
{ 0xe5ffffff, AB_TAS4M30803A   | A_08    | A_MC_LZ93A13 | A_SS_32           , "ASCII",	"TAS-4M308-03A"	},
{ 0x5fffffff, AB_TAS8M30803A   | A_08    | A_MC_LZ93A13 | A_SS_32           , "ASCII",	"TAS-8M308-03A"	},
{ 0x5fffffff, AB_TAS8M30864    | A_08    | A_MC_LZ93A13 | A_SS_08           , "ASCII",	"TAS-8M308-64"	},
{ 0xefffffff, AB_IS020300      | A_08    | A_MC_IBS6202                     , "IS",		"02-030-0"		},
{ 0xeeffffff, AB_IS030010      | A_08    | A_MC_IBS6202                     , "IS",		"03-001-0"		},
{ 0xeeffffff, AB_MSX2MX8K      | A_08    | A_MC_IBS6101                     , "IS",		"MSX-2MX(8K)"	},
{ 0xeeefffff, AB_MSX2MX16K     | A_16    | A_MC_IBS6101                     , "IS",		"MSX-2MX(16K)"	},
{ 0xeeefffff, AB_1M8KB         | A_08    | A_MC_NMR6401                     , "NEOS",	"1M-8KB"		},
{ 0xeeefefff, AB_1M16KB        | A_16    | A_MC_NMR6401                     , "NEOS",	"1M-16KB"		},
{ 0xeeefffff, AB_2M8KB         | A_08    | A_MC_NMR6401                     , "NEOS",	"2M-8KB"		},
{ 0xeeefffff, AB_2M16KB        | A_16    | A_MC_NMR6401                     , "NEOS",	"2M-16KB"		},
{ 0xeeeeeeee, AB_CUSTOM                                                     , "custom",	" "				},
{ ~0        , ~0                                                            , " ",		" "				}
};

/* other bits: reserved (0) */

/* information accuracy:
 A = tested thoroughly
 B = board labels known, range test done
 C = board labels unknown, range test done, board and mapper chip were determined by range
 D = board labels known, range was guessed well, assume SRAM write-protect if it's the same as A
     most of these (and E) were obtained from http://gigamix.jp/rom/rom.php
 E = board labels known, range was guessed poorly
 F = game with SRAM tested in an emulator, specifics guessed by SRAM bit if possible (else G)
 G = unknown, no data
*/
static const _lutmaptype lutmaptype[]={
/* boards with LZ93A13 */
/* TAS-1M008S */
{ 0x7C867D98, 0x010000, A, 0xfeeefccc, AB_TAS1M008S     | A_08 | A_MC_LZ93A13                     }, /* C 2 Family Billiard */
{ 0x7122E8E3, 0x010000, A, 0xfeeefccc, AB_TAS1M008S     | A_08 | A_MC_LZ93A13                     }, /* D 1 Synthe Saurus */
{ 0x00D36AF6, 0x020000, A, 0xfeeecccc, AB_TAS1M008S     | A_08 | A_MC_LZ93A13                     }, /* A 2 Arkanoid II */
{ 0x94982598, 0x020000, A, 0xfeeecccc, AB_TAS1M008S     | A_08 | A_MC_LZ93A13                     }, /* D 2 Druid */
{ 0xCDCDC6C0, 0x020000, A, 0xfeeecccc, AB_TAS1M008S     | A_08 | A_MC_LZ93A13                     }, /* D 2 Fire Ball */
{ 0x3F69A464, 0x020000, A, 0xfeeecccc, AB_TAS1M008S     | A_08 | A_MC_LZ93A13                     }, /* D 2 Mississippi Satsujinjiken */
{ 0x1B10CF79, 0x020000, A, 0xfeeecccc, AB_TAS1M008S     | A_08 | A_MC_LZ93A13                     }, /* D 1 MSX English-Japanese Dictionary */
{ 0xEBCCB796, 0x020000, A, 0xfeeecccc, AB_TAS1M008S     | A_08 | A_MC_LZ93A13                     }, /* B 2 Mon Mon Monster */
{ 0xF56A00E6, 0x020000, A, 0xfeeecccc, AB_TAS1M008S     | A_08 | A_MC_LZ93A13                     }, /* D 2 Ninja Kun - Ashura no Shou */
{ 0x5CCC09AB, 0x020000, A, 0xfeeecccc, AB_TAS1M008S     | A_08 | A_MC_LZ93A13                     }, /* D 2 Return of Jelda */
{ 0x7E637801, 0x020000, A, 0xfeeecccc, AB_TAS1M008S     | A_08 | A_MC_LZ93A13                     }, /* A 2 Tengoku Yoitoko */
{ 0xAA598DFD, 0x020000, A, 0xfeeecccc, AB_TAS1M008S     | A_08 | A_MC_LZ93A13                     }, /* D 2 Tetris */
/* TAS-1M016S */
{ 0x33C37313, 0x010000, A, 0xfeeeefcc, AB_TAS1M016S     | A_16 | A_MC_LZ93A13                     }, /* E 2 Oekaki Soft - Garakuta */
{ 0xF5174422, 0x020000, A, 0xfeeeeccc, AB_TAS1M016S     | A_16 | A_MC_LZ93A13                     }, /* D 2 A-Class Mahjong */
{ 0x2526E568, 0x020000, A, 0xfeeeeccc, AB_TAS1M016S     | A_16 | A_MC_LZ93A13                     }, /* D 1 Dungeon Hunter */
{ 0x28094960, 0x020000, A, 0xfeeeeccc, AB_TAS1M016S     | A_16 | A_MC_LZ93A13                     }, /* D 2 Girly Block */
{ 0x4A3ABB65, 0x020000, A, 0xfeeeeccc, AB_TAS1M016S     | A_16 | A_MC_LZ93A13                     }, /* B 2 Strategic Mars */
{ 0x94EBC319, 0x020000, A, 0xfeeeeccc, AB_TAS1M016S     | A_16 | A_MC_LZ93A13                     }, /* D 2 Victorious Nine II - Koukouyakyuu Hen */
{ 0x2605031E, 0x020000, A, 0xfeeeeccc, AB_TAS1M016S     | A_16 | A_MC_LZ93A13                     }, /* B 2 Zoids - Chuuoudai Riku no Tatakai */
/* TAS-2M008-E2M (2 ROM chips) */
{ 0x353382CE, 0x040000, A, 0xfeeccccc, AB_TAS2M008E2M   | A_08 | A_MC_LZ93A13                     }, /* E 2 Ashguine 3 - Fukushuu no Honoo -- unsure about board label */
{ 0x8D94AF7D, 0x040000, A, 0xfeeccccc, AB_TAS2M008E2M   | A_08 | A_MC_LZ93A13                     }, /* E 2 Quinpl */
/* TAS-4M-008M */
{ 0x47A82D74, 0x040000, A, 0xffeccccc, AB_TAS4M008M     | A_08 | A_MC_LZ93A13                     }, /* D 2 Ashguine 2 - Kyokuu no Gajou */
{ 0x15328BE7, 0x040000, A, 0xffeccccc, AB_TAS4M008M     | A_08 | A_MC_LZ93A13                     }, /* B 2 Ashguine 3 - Fukushuu no Honoo */
{ 0x8528A795, 0x040000, A, 0xffeccccc, AB_TAS4M008M     | A_08 | A_MC_LZ93A13                     }, /* D 2 Crimuzon */
{ 0x8076FEC6, 0x040000, A, 0xffeccccc, AB_TAS4M008M     | A_08 | A_MC_LZ93A13                     }, /* D 1 Dragon Quest II - Akuryou no Kamigami */
{ 0x0BF0C3F5, 0x040000, A, 0xffeccccc, AB_TAS4M008M     | A_08 | A_MC_LZ93A13                     }, /* D 2 Dragon Quest II - Akuryou no Kamigami */
{ 0x0E03847B, 0x040000, A, 0xffeccccc, AB_TAS4M008M     | A_08 | A_MC_LZ93A13                     }, /* D 2 Famicle Parodic */
{ 0x91955BCD, 0x040000, A, 0xffeccccc, AB_TAS4M008M     | A_08 | A_MC_LZ93A13                     }, /* A 1 Gambler Jikochuushinha */
{ 0x53C25B6B, 0x040000, A, 0xffeccccc, AB_TAS4M008M     | A_08 | A_MC_LZ93A13                     }, /* D 1 MSX Program Collection Fandom Library 2 */
{ 0x790D5335, 0x040000, A, 0xffeccccc, AB_TAS4M008M     | A_08 | A_MC_LZ93A13                     }, /* D 1 MSX Program Collection Fandom Library 3 */
/* { 0x?????, 0x040000, A, 0xffeccccc, AB_TAS4M008M     | A_08 | A_MC_LZ93A13                     }, ** D t MSX View -- no dump exists */
{ 0x0A9F8D6C, 0x040000, A, 0xffeccccc, AB_TAS4M008M     | A_08 | A_MC_LZ93A13                     }, /* C 2 Out Run */
{ 0x7DF16FC8, 0x040000, A, 0xffeccccc, AB_TAS4M008M     | A_08 | A_MC_LZ93A13                     }, /* E 2 Pac-Mania -- reconfigured TAS-4M016-03M */
{ 0xC065BC29, 0x040000, A, 0xffeccccc, AB_TAS4M008M     | A_08 | A_MC_LZ93A13                     }, /* D 2 Pro Yakyuu Fan */
{ 0x84461E73, 0x040000, A, 0xffeccccc, AB_TAS4M008M     | A_08 | A_MC_LZ93A13                     }, /* D 2 Rastan Saga */
{ 0xDCB1A61E, 0x040000, A, 0xffeccccc, AB_TAS4M008M     | A_08 | A_MC_LZ93A13                     }, /* D 2 Sanson Misa Kyouto Ryuu no Tera Satsujinjiken */
{ 0xFF974EE3, 0x040000, A, 0xffeccccc, AB_TAS4M008M     | A_08 | A_MC_LZ93A13                     }, /* B 2 Yaksa */
{ 0x712AE3C6, 0x040000, A, 0xffeccccc, AB_TAS4M008M     | A_08 | A_MC_LZ93A13                     }, /* D 2 Yuurei Kun */
{ 0x00C5D5B5, 0x080000, A, 0xffcccccc, AB_TAS4M008M     | A_08 | A_MC_LZ93A13                     }, /* A 1 Hydlide 3 */
{ 0x00DB86C8, 0x080000, A, 0xffcccccc, AB_TAS4M008M     | A_08 | A_MC_LZ93A13                     }, /* D 2 Hydlide 3 */
{ 0x94191AAE, 0x080000, A, 0xffcccccc, AB_TAS4M008M     | A_08 | A_MC_LZ93A13                     }, /* D 2 Mezon Ikkoku - Kanketsu Hen */
{ 0xFE797D88, 0x080000, A, 0xffcccccc, AB_TAS4M008M     | A_08 | A_MC_LZ93A13                     }, /* D 2 Satsujin Club */
{ 0xE564A25F, 0x080000, A, 0xffcccccc, AB_TAS4M008M     | A_08 | A_MC_LZ93A13                     }, /* D 2 Urusei Yatsura */
/* TAS-4M016-03M */
{ 0xC6F74489, 0x040000, A, 0xffeecccc, AB_TAS4M01603M   | A_16 | A_MC_LZ93A13                     }, /* A 2 Andorogynus */
{ 0xA9A66075, 0x040000, A, 0xffeecccc, AB_TAS4M01603M   | A_16 | A_MC_LZ93A13                     }, /* C 2 Ikari */
{ 0xB1B24DA1, 0x040000, A, 0xffeecccc, AB_TAS4M01603M   | A_16 | A_MC_LZ93A13                     }, /* C 2 Penguin Kun Wars 2 */
{ 0x356B7D2A, 0x040000, A, 0xffeecccc, AB_TAS4M01603M   | A_16 | A_MC_LZ93A13                     }, /* D 2 Pro Yakyuu Family Stadium Pennantlers */
{ 0x8EE8212C, 0x040000, A, 0xffeecccc, AB_TAS4M01603M   | A_16 | A_MC_LZ93A13                     }, /* D 2 The Return Of Ishtar */
{ 0x97591725, 0x040000, A, 0xffeecccc, AB_TAS4M01603M   | A_16 | A_MC_LZ93A13                     }, /* D 2 Xevious - Fardraut Densetsu */
/* TAS-1M216A */
{ 0xC956E1DF, 0x020000, A, 0xfee5eccc, AB_TAS1M216A     | A_16 | A_MC_LZ93A13 | A_SS_08           }, /* E 2 Take the A-Train */
/* TAS-2M208-03A */
{ 0x39E1C37A, 0x040000, A, 0xee5ccccc, AB_TAS2M20803A   | A_08 | A_MC_LZ93A13 | A_SS_08           }, /* E 2 Shiryou Sensen */
{ 0xAE82A874, 0x040000, A, 0xee5ccccc, AB_TAS2M20803A   | A_08 | A_MC_LZ93A13 | A_SS_08           }, /* E 2 Taiyou no Shinden - Asteka II */
{ 0x37B9E10F, 0x040000, A, 0xee5ccccc, AB_TAS2M20803A   | A_08 | A_MC_LZ93A13 | A_SS_08           }, /* E 2 Ultima Exodus */
/* TAS-4M208-03A */
{ 0xEC7A8012, 0x080000, A, 0xe5cccccc, AB_TAS4M20803A   | A_08 | A_MC_LZ93A13 | A_SS_08           }, /* D 2 Daikoukai Jidai */
{ 0x3B5B3FD8, 0x080000, A, 0xe5cccccc, AB_TAS4M20803A   | A_08 | A_MC_LZ93A13 | A_SS_08           }, /* A 2 Aoki Ookami to Shiroki Mejika - Genghis Khan */
{ 0x6AC18445, 0x080000, A, 0xe5cccccc, AB_TAS4M20803A   | A_08 | A_MC_LZ93A13 | A_SS_08           }, /* F 2 Heroes of the Lance */
{ 0x8BC34DCA, 0x080000, A, 0xe5cccccc, AB_TAS4M20803A   | A_08 | A_MC_LZ93A13 | A_SS_08           }, /* A 2 Ishin no Arashi */
{ 0xC7521809, 0x080000, A, 0xe5cccccc, AB_TAS4M20803A   | A_08 | A_MC_LZ93A13 | A_SS_08           }, /* D 2 L'Empereur */
{ 0x69C11D81, 0x080000, A, 0xe5cccccc, AB_TAS4M20803A   | A_08 | A_MC_LZ93A13 | A_SS_08           }, /* A 2 Nobunaga no Yabou - Sengoku Gunyuu Den */
{ 0x653F1C73, 0x080000, A, 0xe5cccccc, AB_TAS4M20803A   | A_08 | A_MC_LZ93A13 | A_SS_08           }, /* D 2 Nobunaga no Yabou - Zenkokuban */
{ 0x187D3A21, 0x080000, A, 0xe5cccccc, AB_TAS4M20803A   | A_08 | A_MC_LZ93A13 | A_SS_08           }, /* D 2 Suikoden - Tenmei no Chikai */
/* TAS-4M116-03A */
{ 0x8DF9945A, 0x040000, A, 0x5eeecccc, AB_TAS4M11603A   | A_16 | A_MC_LZ93A13 | A_SS_02           }, /* A 2 Super Daisenryaku */
/* TAS-4M308-03A */
{ 0x459478CB, 0x080000, A, 0xe5cccccc, AB_TAS4M30803A   | A_08 | A_MC_LZ93A13 | A_SS_32           }, /* F 2 Japanese MSX Write II */
{ 0xB83AA6F5, 0x080000, A, 0xe5cccccc, AB_TAS4M30803A   | A_08 | A_MC_LZ93A13 | A_SS_32           }, /* A 2 Sangokushi */
/* TAS-8M308-03A */
{ 0x075AB646, 0x100000, A, 0x5ccccccc, AB_TAS8M30803A   | A_08 | A_MC_LZ93A13 | A_SS_32           }, /* D 2 Aoki Ookami to Shiroki Mejika - Genchou Hishi */
{ 0x413D4700, 0x100000, A, 0x5ccccccc, AB_TAS8M30803A   | A_08 | A_MC_LZ93A13 | A_SS_32           }, /* D 2 Europe Sensen */
{ 0x0FF8E2C7, 0x100000, A, 0x5ccccccc, AB_TAS8M30803A   | A_08 | A_MC_LZ93A13 | A_SS_32           }, /* D 2 Nobunaga no Yabou - Bushou Fuu-un Roku */
{ 0xC89B93B5, 0x100000, A, 0x5ccccccc, AB_TAS8M30803A   | A_08 | A_MC_LZ93A13 | A_SS_32           }, /* A 2 Sangokushi II */
{ 0xC31F459C, 0x100000, A, 0x5ccccccc, AB_TAS8M30803A   | A_08 | A_MC_LZ93A13 | A_SS_32           }, /* D 2 World War II - Teitoku no Ketsudan */
/* TAS-8M308-64 */
{ 0x881B3849, 0x100000, A, 0x5ccccccc, AB_TAS8M30864    | A_08 | A_MC_LZ93A13 | A_SS_08           }, /* D 2 Inindou Datou Nobunaga */
{ 0xFB9E42E1, 0x100000, A, 0x5ccccccc, AB_TAS8M30864    | A_08 | A_MC_LZ93A13 | A_SS_08           }, /* D 2 Royal Blood */
/* other */
{ 0x1BDFAEEB, 0x040000, A, 0xffeecccc, AB_CUSTOM        | A_16 | A_MC_LZ93A13                     }, /* E 2 Fantasy Zone II ~ Opa Opa no Namida ~ -- board: TA-2M (but that would mean a M60002-0125SP and 2 ROMs, this game has 1) */
{ 0x9F152E9B, 0x040000, A, 0xffeccccc, AB_CUSTOM        | A_08 | A_MC_LZ93A13                     }, /* E 2 Fleet Commander II - Tasogare no Kaiiki -- board: FLEET COMMANDER II */
{ 0x68745C64, 0x040000, A, 0xfeeccccc, AB_CUSTOM        | A_08 | A_MC_LZ93A13                     }, /* A 2 Moero!! Nettou Yakyuu '88 -- bit 7 is for D7756C -- board: JX-16 */
{ 0x08A5CA60, 0x080000, A, 0x5ecccccc, AB_CUSTOM        | A_08 | A_MC_LZ93A13 | A_SS_08           }, /* A 2 Wizardry -- board: WIZARDRY MSX ROM */

/* boards with M60002-0125SP */
/* TA-1M(8K) / TA-1M(16K) (sometimes (8K) or (16K) isn't on the board label) */
/* some boards (wrongly) labeled (8K) have had the jumper pads reconfigured to (16K) and vice versa */
/* { 0x....., 0x010000, A, 0xeefffccc, AB_TA1M8K        | A_08 | A_MC_M6K2125                     }, ** C 2 Family Billiard -- CRC32 duplicate of one in LZ93A13 - TAS-4M-008M */
{ 0xA27787AF, 0x020000, A, 0xeeffcccc, AB_TA1M8K        | A_08 | A_MC_M6K2125                     }, /* D 1 1942 */
{ 0xC99E5601, 0x020000, A, 0xeeffcccc, AB_TA1M8K        | A_08 | A_MC_M6K2125                     }, /* C 2 1942 */
{ 0x15A0F98A, 0x020000, A, 0xeeffcccc, AB_TA1M8K        | A_08 | A_MC_M6K2125                     }, /* D 1 Animal Land */
{ 0x6524D9FC, 0x020000, A, 0xeeffcccc, AB_TA1M8K        | A_08 | A_MC_M6K2125                     }, /* D 2 Bubble Bobble */
{ 0x7098FAF0, 0x020000, A, 0xeeffcccc, AB_TA1M8K        | A_08 | A_MC_M6K2125                     }, /* D 2 Darwin 4078 */
{ 0xF221B845, 0x020000, A, 0xeeffcccc, AB_TA1M8K        | A_08 | A_MC_M6K2125                     }, /* C 2 Deep Forest */
{ 0x3CB34EFB, 0x020000, A, 0xeeffcccc, AB_TA1M8K        | A_08 | A_MC_M6K2125                     }, /* D 2 Densetsu no Seisen Samurai - Ashguine */
{ 0x25FC11FA, 0x020000, A, 0xeeffcccc, AB_TA1M8K        | A_08 | A_MC_M6K2125                     }, /* A 1 Digital Devil Story - Megamitensei */
{ 0x49252882, 0x020000, A, 0xeeffcccc, AB_TA1M8K        | A_08 | A_MC_M6K2125                     }, /* D 2 Dragon Buster */
{ 0x49F5478B, 0x020000, A, 0xeeffcccc, AB_TA1M8K        | A_08 | A_MC_M6K2125                     }, /* A 1 Dragon Quest */
{ 0x7729E7A9, 0x020000, A, 0xeeffcccc, AB_TA1M8K        | A_08 | A_MC_M6K2125                     }, /* D 2 Dragon Quest */
{ 0x14913432, 0x020000, A, 0xeeffcccc, AB_TA1M8K        | A_08 | A_MC_M6K2125                     }, /* D 2 Eidolon */
{ 0x3E96D005, 0x020000, A, 0xeeffcccc, AB_TA1M8K        | A_08 | A_MC_M6K2125                     }, /* D 1 Fantasy Zone */
{ 0x14E2EFCC, 0x020000, A, 0xeeffcccc, AB_TA1M8K        | A_08 | A_MC_M6K2125                     }, /* D 1 Final Zone */
{ 0x6A603B8C, 0x020000, A, 0xeeffcccc, AB_TA1M8K        | A_08 | A_MC_M6K2125                     }, /* D 1 Haja no Fuuin */
{ 0x1D7040AF, 0x020000, A, 0xeeffcccc, AB_TA1M8K        | A_08 | A_MC_M6K2125                     }, /* D 2 Hard Ball */
{ 0x0767AF40, 0x020000, A, 0xeeffcccc, AB_TA1M8K        | A_08 | A_MC_M6K2125                     }, /* D 2 Honkaku Shogi Kitaihei */
{ 0x6A30C707, 0x020000, A, 0xeeffcccc, AB_TA1M8K        | A_08 | A_MC_M6K2125                     }, /* A 1 Karuizawa */
{ 0xC4096E6B, 0x020000, A, 0xeeffcccc, AB_TA1M8K        | A_08 | A_MC_M6K2125                     }, /* D 2 Lupin the Third - Kariosutoro no Shiro */
{ 0xF87BD172, 0x020000, A, 0xeeffcccc, AB_TA1M8K        | A_08 | A_MC_M6K2125                     }, /* D 2 Mad Rider */
{ 0x44AA5422, 0x020000, A, 0xeeffcccc, AB_TA1M8K        | A_08 | A_MC_M6K2125                     }, /* D 1 Marchen Veil */
{ 0x72EB9D0E, 0x020000, A, 0xeeffcccc, AB_TA1M8K        | A_08 | A_MC_M6K2125                     }, /* D 1 Mirai */
{ 0x70AA93CF, 0x020000, A, 0xeeffcccc, AB_TA1M8K        | A_08 | A_MC_M6K2125                     }, /* G 2 Mirai */
{ 0x309D996C, 0x020000, A, 0xeeffcccc, AB_TA1M8K        | A_08 | A_MC_M6K2125                     }, /* A 1 Mugen Senshi Valis (Fantasm Soldier) */
{ 0x1B285FE3, 0x020000, A, 0xeeffcccc, AB_TA1M8K        | A_08 | A_MC_M6K2125                     }, /* D 2 Project A2 - Shijousaidai no Kessen */
{ 0xB612D79A, 0x020000, A, 0xeeffcccc, AB_TA1M8K        | A_08 | A_MC_M6K2125                     }, /* D 1 Relics */
{ 0xE79AA962, 0x020000, A, 0xeeffcccc, AB_TA1M8K        | A_08 | A_MC_M6K2125                     }, /* C 2 Rick no Mick Daibouken */
{ 0xE37B681E, 0x020000, A, 0xeeffcccc, AB_TA1M8K        | A_08 | A_MC_M6K2125                     }, /* D 2 Seikima II Special */
{ 0x427B3F14, 0x020000, A, 0xeeffcccc, AB_TA1M8K        | A_08 | A_MC_M6K2125                     }, /* D 1 Senjou no Ookami */
{ 0xCBB00EF1, 0x020000, A, 0xeeffcccc, AB_TA1M8K        | A_08 | A_MC_M6K2125                     }, /* D 2 Super Triton -- reconfigured */
{ 0x96B33497, 0x020000, A, 0xeeffcccc, AB_TA1M8K        | A_08 | A_MC_M6K2125                     }, /* D 2 Topple Zip */
{ 0x5C9D8F62, 0x020000, A, 0xeeffcccc, AB_TA1M8K        | A_08 | A_MC_M6K2125                     }, /* D 1 Wing Man 2 - Dakukira no Fukkatsu */
{ 0xD0ECECC6, 0x020000, A, 0xeeffcccc, AB_TA1M8K        | A_08 | A_MC_M6K2125                     }, /* D 2 Zukkokeyajikita Onmitsu Douchuu -- reconfigured */
{ 0x4968E84E, 0x020000, A, 0xeeffcccc, AB_TA1M8K        | A_08 | A_MC_M6K2125                     }, /* D 2 Zukkokeyajikita Onmitsu Douchuu -- reconfigured */
/* { 0x....., 0x040000, A, 0xeefccccc, AB_TA1M8K        | A_08 | A_MC_M6K2125                     }, ** E 2 Relics -- 2Mbit ROM on 1Mbit board? -- CRC32 duplicate of one in BS6101 */
{ 0xC6FC7BD7, 0x020000, A, 0xeeffeccc, AB_TA1M16K       | A_16 | A_MC_M6K2125                     }, /* B 1 Aliens */
{ 0xE040E8A1, 0x020000, A, 0xeeffeccc, AB_TA1M16K       | A_16 | A_MC_M6K2125                     }, /* A 1 Borfes to 5-nin no Akuma -- reconfigured */
{ 0x9935E9BD, 0x020000, A, 0xeeffeccc, AB_TA1M16K       | A_16 | A_MC_M6K2125                     }, /* D 2 Dynamite Bowl */
{ 0x99EF860E, 0x020000, A, 0xeeffeccc, AB_TA1M16K       | A_16 | A_MC_M6K2125                     }, /* D 1 Gall Force - Kaosu no Koubou */
{ 0x5EAC55DF, 0x020000, A, 0xeeffeccc, AB_TA1M16K       | A_16 | A_MC_M6K2125                     }, /* D 1 Golvellius */
{ 0xF35DF283, 0x020000, A, 0xeeffeccc, AB_TA1M16K       | A_16 | A_MC_M6K2125                     }, /* D 2 Hole In One Special */
{ 0x4708BDD1, 0x020000, A, 0xeeffeccc, AB_TA1M16K       | A_16 | A_MC_M6K2125                     }, /* D 2 Hole In One Special */
{ 0xD6465702, 0x020000, A, 0xeeffeccc, AB_TA1M16K       | A_16 | A_MC_M6K2125                     }, /* D 1 Jagur 5 */
{ 0x00B8E82E, 0x020000, A, 0xeeffeccc, AB_TA1M16K       | A_16 | A_MC_M6K2125                     }, /* A 2 Jansei */
{ 0x2C2924ED, 0x020000, A, 0xeeffeccc, AB_TA1M16K       | A_16 | A_MC_M6K2125                     }, /* G 2 Jansei */
{ 0xA0481321, 0x020000, A, 0xeeffeccc, AB_TA1M16K       | A_16 | A_MC_M6K2125                     }, /* D 1 King's Knight */
{ 0x2F8065EB, 0x020000, A, 0xeeffeccc, AB_TA1M16K       | A_16 | A_MC_M6K2125                     }, /* D 1 Meikyuu Shinwa (Eggerland 2) */
{ 0xF2544E2E, 0x020000, A, 0xeeffeccc, AB_TA1M16K       | A_16 | A_MC_M6K2125                     }, /* D 2 Ogre */
{ 0x17BAF3ED, 0x020000, A, 0xeeffeccc, AB_TA1M16K       | A_16 | A_MC_M6K2125                     }, /* C 2 Pirate Ship Higemaru */
{ 0x67BF8337, 0x020000, A, 0xeeffeccc, AB_TA1M16K       | A_16 | A_MC_M6K2125                     }, /* D 1 The Black Onyx II */
{ 0xB9B69A43, 0x020000, A, 0xeeffeccc, AB_TA1M16K       | A_16 | A_MC_M6K2125                     }, /* D 1 Tsumego 120 */
{ 0xFEA70207, 0x020000, A, 0xeeffeccc, AB_TA1M16K       | A_16 | A_MC_M6K2125                     }, /* D 1 Vaxol */
{ 0xAEA8D064, 0x020000, A, 0xeeffeccc, AB_TA1M16K       | A_16 | A_MC_M6K2125                     }, /* D 2 Woody Poco */
{ 0xD153B2F2, 0x020000, A, 0xeeffeccc, AB_TA1M16K       | A_16 | A_MC_M6K2125                     }, /* B 2 Zanac EX */
/* TA-2M 8K / TA-2M 16K (2 ROMs) (same note as TA-1M applies) */
{ 0x2FB0ED30, 0x040000, A, 0xeefccccc, AB_TA2M8K        | A_08 | A_MC_M6K2125                     }, /* D 2 Garyuu Ou */
{ 0x803CC690, 0x040000, A, 0xeefccccc, AB_TA2M8K        | A_08 | A_MC_M6K2125                     }, /* A 1 Sangokushi */
/* TA6228 */
{ 0x99198ED9, 0x040000, A, 0xeffccccc, AB_TA6228        | A_08 | A_MC_M6K2125                     }, /* D 1 Daiva - Asura no Ketsuryuu */
{ 0xE1148738, 0x040000, A, 0xeffccccc, AB_TA6228        | A_08 | A_MC_M6K2125                     }, /* D 2 Double Vision */
{ 0x87DCD309, 0x040000, A, 0xeffccccc, AB_TA6228        | A_08 | A_MC_M6K2125                     }, /* D 1 Dragon Slayer IV */
{ 0x98095852, 0x040000, A, 0xeffccccc, AB_TA6228        | A_08 | A_MC_M6K2125                     }, /* A 2 Dragon Slayer IV */
{ 0x9926F43C, 0x040000, A, 0xeffccccc, AB_TA6228        | A_08 | A_MC_M6K2125                     }, /* C 2 Labyrinth */
{ 0xF4D127C0, 0x040000, A, 0xeffccccc, AB_TA6228        | A_08 | A_MC_M6K2125                     }, /* D 2 Mezon Ikkoku */
{ 0x519BE53E, 0x040000, A, 0xeffccccc, AB_TA6228        | A_08 | A_MC_M6K2125                     }, /* D 1 MSX Program Collection Fandom Library 1 */
{ 0x5DC45624, 0x040000, A, 0xeffccccc, AB_TA6228        | A_08 | A_MC_M6K2125                     }, /* B 1 Super Laydock - Mission Striker */
{ 0x10621440, 0x040000, A, 0xeffccccc, AB_TA6228        | A_08 | A_MC_M6K2125                     }, /* D 1 Sangokushi */
{ 0x226795E0, 0x040000, A, 0xeffccccc, AB_TA6228        | A_08 | A_MC_M6K2125                     }, /* D 2 Scramble Formation */
/* TA-621M */
{ 0xA987A0D8, 0x020000, A, 0xeef5eccc, AB_TA621M        | A_16 | A_MC_M6K2125 | A_SS_02 | A_SPSTD }, /* A 2 Daisenryaku - Strategic Confrontation */
{ 0x96B7FACA, 0x020000, A, 0xeef5eccc, AB_TA621M        | A_16 | A_MC_M6K2125 | A_SS_02 | A_SPSTD }, /* A 1 Harry Fox Special */
{ 0xD8055F5F, 0x020000, A, 0xeef5eccc, AB_TA621M        | A_16 | A_MC_M6K2125 | A_SS_02 | A_SPSTD }, /* A 1 Hydlide II */
{ 0x2ABB1EC4, 0x020000, A, 0xeef5eccc, AB_TA621M        | A_16 | A_MC_M6K2125 | A_SS_02 | A_SPSTD }, /* D 2 Professional Mahjong Gokuu */
/* TA-621-8K */
{ 0x5ACBD591, 0x020000, A, 0xeef5cccc, AB_TA6218K       | A_08 | A_MC_M6K2125 | A_SS_02 | A_SPSTD }, /* A 2 Dires - Giger Loop */
{ 0x167A53DA, 0x020000, A, 0xeef5cccc, AB_TA6218K       | A_08 | A_MC_M6K2125 | A_SS_02 | A_SPSTD }, /* D 2 Kisei */
/* TA-621-64K */
{ 0x0E691C47, 0x020000, A, 0xeef5cccc, AB_TA62164K      | A_08 | A_MC_M6K2125 | A_SS_08 | A_SPSTD }, /* A 1 Shogun */
/* TA-621 KAGA */
{ 0x4EAD5098, 0x040000, A, 0xef5ccccc, AB_TA621KAGA     | A_08 | A_MC_M6K2125 | A_SS_08           }, /* D 1 Aoki Ookami to Shiroki Mejika - Genghis Khan */
{ 0x3AA33A30, 0x040000, A, 0xef5ccccc, AB_TA621KAGA     | A_08 | A_MC_M6K2125 | A_SS_08           }, /* A 1 Nobunaga no Yabou - Zenkokuban */
{ 0xD640DEAF, 0x040000, A, 0xef5ccccc, AB_TA621KAGA     | A_08 | A_MC_M6K2125 | A_SS_08           }, /* A 1 Xanadu */
/* other */
{ 0xEF02E4F3, 0x080000, A, 0xffcccccc, AB_CUSTOM        | A_08 | A_MC_M6K2125                     }, /* E 1 Japanese MSX-Write -- also contains a 128KB Kanji ROM -- board: MSX WRITE 900178B */
/* { 0x?????, 0x040000, A, 0xef5ccccc, AB_CUSTOM        | A_08 | A_MC_M6K2125 | A_SS_08           }, ** E 2 Junior Mate -- also contains a 128KB Kanji ROM -- board: JCI-N1S, mapper chip: M60002-0115FP -- no dump exists */

/* boards with MR6401 */
/* 1M-8KB */
{ 0x53CCE88D, 0x020000, A, 0xeeefcccc, AB_1M8KB         | A_08 | A_MC_NMR6401                     }, /* D 2 F15 Strike Eagle */
{ 0xA6165BD4, 0x020000, A, 0xeeefcccc, AB_1M8KB         | A_08 | A_MC_NMR6401                     }, /* D 1 Flight Simulator with Torpedo Attack */
{ 0x935F1BDE, 0x020000, A, 0xeeefcccc, AB_1M8KB         | A_08 | A_MC_NMR6401                     }, /* A 2 Ide Yousuke Meijin no Jissen Mahjong */
/* { 0x?????, 0x020000, A, 0xeeefcccc, AB_1M8KB         | A_08 | A_MC_NMR6401                     }, ** D 2 Michelangelo II -- no dump exists */
{ 0x5A5BE701, 0x020000, A, 0xeeefcccc, AB_1M8KB         | A_08 | A_MC_NMR6401                     }, /* D 2 Super Runner */
/* 2M-8KB */
/* { 0x....., 0x040000, A, 0xeeeccccc, AB_2M8KB         | A_08 | A_MC_NMR6401                     }, ** E 1 Gambler Jikochuushinha -- CRC32 duplicate of one in LZ93A13 - TAS-4M-008M */
{ 0xEB67D4D6, 0x040000, A, 0xeeeccccc, AB_2M8KB         | A_08 | A_MC_NMR6401                     }, /* E 2 Gambler Jikochuushinha 2 - Jishou! Kyougou Suzume Samurai Hen */
/* 1M-16KB */
{ 0x2C2020A0, 0x020000, A, 0xeeefeccc, AB_1M16KB        | A_16 | A_MC_NMR6401                     }, /* D 1 Meikyuu Heno Tobira */
{ 0xF00B4BBC, 0x020000, A, 0xeeefeccc, AB_1M16KB        | A_16 | A_MC_NMR6401                     }, /* D 1 Super Pierrot */
{ 0x5F354E65, 0x020000, A, 0xeeefeccc, AB_1M16KB        | A_16 | A_MC_NMR6401                     }, /* A 2 The Cockpit */
/* 2M-16KB */
{ 0xC29EDD84, 0x040000, A, 0xeeefcccc, AB_2M16KB        | A_16 | A_MC_NMR6401                     }, /* B 2 Aleste */

/* boards with BS6101 */
/* MSX-2MX (board label seems to have no indication of bank size) */
{ 0x6EEE12C4, 0x020000, A, 0xeeffcccc, AB_MSX2MX8K      | A_08 | A_MC_IBS6101                     }, /* E 2 Replicart */
{ 0xBDF332F2, 0x020000, A, 0xeeffcccc, AB_MSX2MX8K      | A_08 | A_MC_IBS6101                     }, /* E 1 Young Sherlock - Doiru no Isan */
{ 0x387C1DE7, 0x020000, A, 0xeeeffccc, AB_MSX2MX16K     | A_16 | A_MC_IBS6101                     }, /* A 1 Romancia (Dragon Slayer Jr.) */
{ 0x17F9EE7F, 0x020000, A, 0xeeeffccc, AB_MSX2MX16K     | A_16 | A_MC_IBS6101                     }, /* D 2 Romancia (Dragon Slayer Jr.) */
{ 0xB2392EF3, 0x020000, A, 0xeeeffccc, AB_MSX2MX16K     | A_16 | A_MC_IBS6101                     }, /* C 2 Super Rambo Special */
/* other */
{ 0xBABC68F2, 0x040000, A, 0xeefccccc, AB_CUSTOM        | A_08 | A_MC_IBS6101                     }, /* A 2 Relics -- no PCB label, 2 ROMs */

/* boards with BS6202 */
/* IS-02-030-0 (seems same as TA-1M) */
{ 0x5C859406, 0x020000, A, 0xeeffcccc, AB_IS020300      | A_08 | A_MC_IBS6202                     }, /* A 2 Kikikaikai */
/* { 0x....., 0x020000, A, 0xeeffcccc, AB_IS020300      | A_08 | A_MC_IBS6202                     }, ** D 2 Mon Mon Monster -- CRC32 duplicate of one in LZ93A13 - TAS-1M008S */
/* IS-03-001-0 */
{ 0x7030F2AB, 0x040000, A, 0xeffccccc, AB_IS030010      | A_08 | A_MC_IBS6202                     }, /* B 2 Family Boxing */

/* no data */
/* SRAM boards (2KB or 8KB, may be TA-621-8K/64K, or an unknown TAS-1M board) */
{ 0x27FD8F9A, 0x020000, A, 0x0005cccc, A_08 | A_SS_08                                             }, /* G 1 Deep Dungeon */
{ 0x101DB19C, 0x020000, A, 0x0005cccc, A_08 | A_SS_08                                             }, /* G 1 Deep Dungeon II -- most likely 8KB */
{ 0xA8E9AC9B, 0x020000, A, 0x0005cccc, A_08 | A_SS_08                                             }, /* G 2 Elthlead */
{ 0xF640670C, 0x020000, A, 0x0005cccc, A_08 | A_SS_08                                             }, /* G 2 Tanigawa Koji no Shogi Shinan II */
/* standard */
{ 0x8CF0E6C0, 0x020000, A, 0         , A_08                                                       }, /* G 1 Bomber King */
#if 0 /* A8/A16 autodetected properly */
{ 0xFA8F9BBC, 0x020000, A, 0         , A_08                                                       }, /* G 1 A Life M36 Planet */
{ 0xD2EE4051, 0x020000, A, 0         , A_16                                                       }, /* G 2 Acrojet */
{ 0xBCF03D8B, 0x010000, A, 0         , A_08                                                       }, /* G 2 American Soccer */
{ 0xAE3167EE, 0x020000, A, 0         , A_16                                                       }, /* G 2 Arctic */
{ 0x225705CC, 0x040000, A, 0         , A_08                                                       }, /* G 2 Alges no Yoku */
{ 0x637F0494, 0x010000, A, 0         , A_08                                                       }, /* G 1 Batman */
{ 0xD83966D0, 0x020000, A, 0         , A_16                                                       }, /* G 1 Craze */
{ 0x47EC57DA, 0x020000, A, 0         , A_16                                                       }, /* G 1 Dynamite Bowl */
{ 0x26E34F05, 0x020000, A, 0         , A_08                                                       }, /* G 1 Fairy Land Story */
{ 0xFBDAFE4E, 0x040000, A, 0         , A_16                                                       }, /* G 2 Gakuen Monogatari - High School Story */
{ 0x8DF56D42, 0x020000, A, 0         , A_16                                                       }, /* G 2 Hacker */
{ 0x1123545C, 0x020000, A, 0         , A_08                                                       }, /* G 2 High School Kimengumi */
{ 0x053AA66E, 0x020000, A, 0         , A_16                                                       }, /* G 2 Kempelen Chess */
{ 0xA3B2FE71, 0x020000, A, 0         , A_16                                                       }, /* G 1 Knither Special */
{ 0xFB348DE6, 0x040000, A, 0         , A_16                                                       }, /* G 2 Koronis Rift */
{ 0xBFE150C4, 0x020000, A, 0         , A_08                                                       }, /* G 2 Lupin the Third - Babylon no Ougon Densetsu */
{ 0xD446BA1E, 0x020000, A, 0         , A_16                                                       }, /* G 1 Magunam - Kiki Ippatsu */
{ 0xF7A9591D, 0x020000, A, 0         , A_16                                                       }, /* G 2 Malaya no Hihou */
{ 0x4FACCAE0, 0x020000, A, 0         , A_08                                                       }, /* G 1 Mitsume ga Tooru */
{ 0x758C6087, 0x040000, A, 0         , A_08                                                       }, /* G 2 Nekketsu Judo */
{ 0x85D47F39, 0x020000, A, 0         , A_08                                                       }, /* G 1 New Horizon - English Course 1 */
{ 0x252798F6, 0x020000, A, 0         , A_08                                                       }, /* G 2 Pachi Pro Densetsu */
{ 0xE85A4A6B, 0x020000, A, 0         , A_08                                                       }, /* G 2 Predator */
{ 0xB8FC19A4, 0x040000, A, 0         , A_08                                                       }, /* G 1 Psychic War */
{ 0x788637D5, 0x020000, A, 0         , A_16                                                       }, /* G 1 Robo Wres 2001 */
{ 0x3B815DC6, 0x020000, A, 0         , A_16                                                       }, /* G 2 Shanghai */
{ 0x14811951, 0x010000, A, 0         , A_08                                                       }, /* G 1 Sofia */
{ 0xCBDFCC35, 0x020000, A, 0         , A_08                                                       }, /* G 2 Star Virgin */
{ 0xFF7395A3, 0x020000, A, 0         , A_08                                                       }, /* G 2 Tsurikichi Sanpei - Blue Marine Hen */
{ 0x24238B21, 0x040000, A, 0         , A_08                                                       }, /* G 2 Tsurikichi Sanpei - Tsuri Sennin Hen */
{ 0xA29E6AB4, 0x020000, A, 0         , A_08                                                       }, /* G 2 Zombie Hunter */

/* Arabian educational software by Sakhr / Al-Alamiah: (romanized titles are unknown) */
{ 0xF5427547, 0x020000, A, 0         , A_08                                                       }, /* G 1 Ali Baba */
{ 0x22528D5C, 0x020000, A, 0         , A_08                                                       }, /* G 1 Dictionary */
{ 0x97330C37, 0x020000, A, 0         , A_08                                                       }, /* G 1 Info Race */
{ 0xC3B051E6, 0x020000, A, 0         , A_08                                                       }, /* G 1 Landmarks */
{ 0xEA5C9F48, 0x020000, A, 0         , A_08                                                       }, /* G 1 Pictures and Words */
{ 0x36477F60, 0x020000, A, 0         , A_08                                                       }, /* G 2 Poetry */
{ 0x34EBA5D3, 0x020000, A, 0         , A_08                                                       }, /* G 1 Test Knowledge 3 */
{ 0x25013264, 0x020000, A, 0         , A_08                                                       }, /* G 1 Young Artist */
{ 0xEB40E36F, 0x020000, A, 0         , A_08                                                       }, /* G 1 Zoo */
#endif /* G */

#undef A

/* Korean bootleg multicarts */
{ 0x8D452FA1, 0x080000, CARTTYPE_BTL80,			0, 0 },	/* Zemmix 30-in-1 */
{ 0xE50BAB84, 0x100000, CARTTYPE_BTL80,			0, 0 },	/* Zemmix 64-in-1 */
{ 0xD9AA0055, 0x080000, CARTTYPE_BTL80,			0, 0 },	/* Zemmix 80-in-1 */
{ 0x123BE4E7, 0x100000, CARTTYPE_BTL90,			0, 0 },	/* Zemmix 90-in-1 */
{ 0x2EE35C23, 0x200000, CARTTYPE_BTL126,		0, 0 },	/* Zemmix 126-in-1 */

/* SCC ROMs not autoguessed */
{ 0xB88A7FDE, 0x004000, CARTTYPE_KONAMISCC,		0, 0 },	/* Jos'b's Los Jardines de Zee Wang Zu */
{ 0xD6C395F8, 0x020000, CARTTYPE_KONAMISCC,		0, 0 },	/* Nerlaska's Monster Hunter (English) */
{ 0xB876A92F, 0x020000, CARTTYPE_KONAMISCC,		0, 0 },	/* Nerlaska's Monster Hunter (Spanish) */
{ 0xAA6F2968, 0x020000, CARTTYPE_KONAMISCC,		0, 0 },	/* Nerlaska's Mr. Mole */

/* Mega Flash ROM SCC */
{ 0x3B38CFEA, 0x020000, CARTTYPE_MFLASHSCC,		0, 0 },	/* dvik&joyrex's Lotus F3 */
{ 0x2A515904, 0x020000, CARTTYPE_MFLASHSCC,		0, 0 },	/* dvik&joyrex's Lotus F3 (december 2008 update) */

/* Konami Game Collection hacks, just because they're fun */
/* SCC ROM */
{ 0xF3E0F940, 0x00a000, CARTTYPE_KONAMISCC,		0, 0 },	/* Antarctic Adventure */
{ 0x04391BC8, 0x00a000, CARTTYPE_KONAMISCC,		0, 0 },	/* Hyper Sports 2 */
{ 0x9AB41FF5, 0x00e000, CARTTYPE_KONAMISCC,		0, 0 },	/* Knightmare */
{ 0xE4A8EE40, 0x00e000, CARTTYPE_KONAMISCC,		0, 0 },	/* Konami's Boxing */
{ 0x37183EFD, 0x00a000, CARTTYPE_KONAMISCC,		0, 0 },	/* Super Cobra */
{ 0xB29F137C, 0x00a000, CARTTYPE_KONAMISCC,		0, 0 },	/* Video Hustler */
{ 0xE36710CE, 0x00a000, CARTTYPE_KONAMISCC,		0, 0 },	/* Yie ar Kung-Fu */
{ 0x4CF4D4CF, 0x00e000, CARTTYPE_KONAMISCC,		0, 0 },	/* Yie ar Kung-Fu 2 */
/* SCC-I RAM, 64KB, Snatcher */
{ 0x037D1ED8, 0x00a000, CARTTYPE_KONAMISCCI,	1, 0 },	/* Antarctic Adventure */
{ 0x2CA8E6BF, 0x00a000, CARTTYPE_KONAMISCCI,	1, 0 },	/* Hyper Rally */
{ 0x62F1531C, 0x00a000, CARTTYPE_KONAMISCCI,	1, 0 },	/* Hyper Sports 2 */
{ 0xDBC8B11B, 0x00e000, CARTTYPE_KONAMISCCI,	1, 0 },	/* Knightmare */
{ 0x0E8EFD8B, 0x00e000, CARTTYPE_KONAMISCCI,	1, 0 },	/* Konami's Boxing */
{ 0x9EA714C2, 0x00a000, CARTTYPE_KONAMISCCI,	1, 0 },	/* Pippols */
{ 0x39B45611, 0x00a000, CARTTYPE_KONAMISCCI,	1, 0 },	/* Road Fighter */
{ 0x1B47F46E, 0x00e000, CARTTYPE_KONAMISCCI,	1, 0 },	/* Sky Jaguar */
{ 0x8A4B51BC, 0x00a000, CARTTYPE_KONAMISCCI,	1, 0 },	/* Super Cobra */
{ 0xF338052F, 0x00a000, CARTTYPE_KONAMISCCI,	1, 0 },	/* Video Hustler */
{ 0x278114CD, 0x00a000, CARTTYPE_KONAMISCCI,	1, 0 },	/* Yie ar Kung-Fu */
{ 0xC080159C, 0x00e000, CARTTYPE_KONAMISCCI,	1, 0 },	/* Yie ar Kung-Fu 2 */
/* SCC-I RAM, 128KB */
/* these claim to be for the SDS cart, but still use the lower 64KB */
{ 0x14CCB2FD, 0x00a000, CARTTYPE_KONAMISCCI,	3, 0 },	/* Hyper Rally */
{ 0x04A889BF, 0x00a000, CARTTYPE_KONAMISCCI,	3, 0 },	/* Hyper Sports 2 */
{ 0x408070C4, 0x00e000, CARTTYPE_KONAMISCCI,	3, 0 },	/* Konami's Boxing */
{ 0x1EC24C26, 0x00a000, CARTTYPE_KONAMISCCI,	3, 0 },	/* Pippols */
{ 0x6616395A, 0x00a000, CARTTYPE_KONAMISCCI,	3, 0 },	/* Road Fighter */
{ 0x7A5FEC5A, 0x00a000, CARTTYPE_KONAMISCCI,	3, 0 },	/* Video Hustler */
/* these claim to be for 64KB, but are larger than 64KB */
{ 0x619B4B4A, 0x020000, CARTTYPE_KONAMISCCI,	3, 0 },	/* Nemesis */
{ 0x4A653152, 0x012000, CARTTYPE_KONAMISCCI,	3, 0 },	/* Twinbee */
{ 0x97A62DBA, 0x012000, CARTTYPE_KONAMISCCI,	3, 0 },	/* Twinbee */

/* unique types */
{ 0x084C5803, 0x020000, CARTTYPE_GAMEMASTER2,	0, 0 },	/* Konami's Game Master 2 */
{ 0x47273220, 0x010000, CARTTYPE_CROSSBLAIM,	0, 0 },	/* Cross Blaim */
{ 0xC7FE3EE1, 0x010000, CARTTYPE_HARRYFOX,		0, 0 },	/* Harry Fox - Yuki no Maou Hen */
{ 0x3B003E5C, 0x100000, CARTTYPE_QURAN,			0, 0 },	/* Al-Qur'an */
{ 0x8B248084, 0x100000, CARTTYPE_QURAN,			0, 0 },	/* Al-Qur'an (bad?) */

{ 0x35823FD5, 0x00c000, CARTTYPE_MATRAINK,		0, 0 },	/* INK - Exxon Surfing			(012345ee) */
{ 0xB9B0999A, 0x008000, CARTTYPE_KONAMISYN,		0, 0 },	/* Konami's Synthesizer			(uu0123uu) */
{ 0xD178833B, 0x008000, CARTTYPE_PLAYBALL,		0, 0 },	/* Playball 					(uu0123uu) */

/* small no-mapper ROMs */
#define NM CARTTYPE_NOMAPPER
/* first dword is rom layout: e = empty, f = unmapped, eg. 0xff01ffff is uu01uuuu */
/* second dword is ram layout, similar to rom + a/b/c is 1/2/4KB RAM mirrored */

/* confirmed boards only (all other roms work fine though) */

/* standard 4H */
{ 0xAAE7028B, 0x004000, NM, 0xff01ffff, ~0 },			/* Antarctic Adventure (Japan) */
{ 0x7B1ACDEA, 0x004000, NM, 0xff01ffff, ~0 },			/* Athletic Land */
{ 0xA580B72A, 0x004000, NM, 0xff01ffff, ~0 },			/* Becky */
{ 0x652D0E39, 0x008000, NM, 0xff0123ff, ~0 },			/* Doki Doki Penguin Land */
{ 0x232B1050, 0x008000, NM, 0xff0123ff, ~0 },			/* Eggerland Mystery */
{ 0x4A4F3084, 0x004000, NM, 0xff01ffff, ~0 },			/* Flappy Limited '85 */
{ 0x968FA8D6, 0x004000, NM, 0xff01ffff, ~0 },			/* Hyper Sports 2 */
{ 0x5A141C44, 0x004000, NM, 0xff01ffff, ~0 },			/* King's Valley */
{ 0x0DB84205, 0x008000, NM, 0xff0123ff, ~0 },			/* Knightmare */
{ 0xCFCE0A28, 0x004000, NM, 0xff01ffff, ~0 },			/* Konami's Tennis */
{ 0xD40F481D, 0x008000, NM, 0xff0123ff, ~0 },			/* Mokarimakka */
{ 0x01DDB68F, 0x004000, NM, 0xff01ffff, ~0 },			/* Road Fighter */
{ 0x2149C32D, 0x008000, NM, 0xff0123ff, ~0 },			/* The Castle */
{ 0xDB327847, 0x008000, NM, 0xff0123ff, ~0 },			/* The Goonies */
{ 0x599AA9AC, 0x008000, NM, 0xff0123ff, ~0 },			/* Thexder */
{ 0xA0A19FD5, 0x008000, NM, 0xff0123ff, ~0 },			/* Warroid */
/* standard 8H */
{ 0xB6AB6786, 0x004000, NM, 0xffff01ff, ~0 },			/* Candoo Ninja */
{ 0x56200FEF, 0x004000, NM, 0xffff01ff, ~0 },			/* Rollerball */
/* not standard */
{ 0xE9E8E123, 0x008000, NM, 0x01230123, ~0 },			/* MSX-PLAN */
{ 0x068B6349, 0x002000, NM, 0xff0fffff, 0xfffbffff },	/* MSXtra 2.0 */

#undef NM

{ ~0, ~0, CARTTYPE_MAX, 0, 0 }
};
