/******************************************************************************
 *                                                                            *
 *                               "resource.h"                                 *
 *                                                                            *
 ******************************************************************************/

#ifndef RESOURCE_H
#define RESOURCE_H

#define ID_MENU							101
#define ID_ICON							102
#define ID_ICON_BLOCKY					103
#define ID_ICON_DOC						104

#define ID_PAL_TMS9129					111
#define ID_PAL_V9938					112
#define ID_PAL_SMS						113
#define ID_PAL_KONAMI					114
#define ID_PAL_VAMPIER					115
#define ID_PAL_WOLF						116

#define ID_PSG_PRESETS					121
#define ID_SAMPLE_PAUSE					122

#define ID_BITMAP_TICKV					131
#define ID_BITMAP_TICKVW				132
#define ID_BITMAP_TICKH					133
#define ID_BITMAP_TICKHW				134


/* dialogs */
#define IDC_STATIC						981

/* media */
#define IDD_MEDIA						1001
#define IDC_MEDIA_OK					1002
#define IDC_MEDIA_TAB					1003
/* carts tab */
#define IDC_MEDIA_SLOT1TEXT				1010
#define IDC_MEDIA_SLOT2TEXT				1011
#define IDC_MEDIA_SLOT1FILE				1012
#define IDC_MEDIA_SLOT2FILE				1013
#define IDC_MEDIA_SLOT1TYPE				1014
#define IDC_MEDIA_SLOT2TYPE				1015
#define IDC_MEDIA_SLOT1BROWSE			1016
#define IDC_MEDIA_SLOT2BROWSE			1017
#define IDC_MEDIA_SLOT1EJECT			1018
#define IDC_MEDIA_SLOT2EJECT			1019
#define IDC_MEDIA_SLOT1EXTRA			1020
#define IDC_MEDIA_SLOT2EXTRA			1021

/* carts extra config */
#define IDD_MEDIACART_E_A				11010
#define IDC_MCE_A_BOARD					11011
#define IDC_MCE_A_MAPPERTEXT			11012
#define IDC_MCE_A_MAPPER				11013
#define IDC_MCE_A_BANKSTEXT				11014
#define IDC_MCE_A_BANKS					11015
#define IDC_MCE_A_WRITETEXT				11016
#define IDC_MCE_A_WRITE					11017
#define IDC_MCE_A_SRAMTEXT1				11018
#define IDC_MCE_A_SRAM					11019
#define IDC_MCE_A_SRAMTEXT2				11020
#define IDC_MCE_A_GROUPSTART			IDC_MCE_A_MAPPERTEXT
#define IDC_MCE_A_GROUPEND				IDC_MCE_A_SRAMTEXT2

#define IDD_MEDIACART_E_NM				11030
#define IDC_MCE_NM_ROM					11031
#define IDC_MCE_NM_RAM					11032

#define IDD_MEDIACART_E_SCCI			11040
#define IDC_MCE_SCCI_RAM				11041

/* internal tab */
#define IDC_MEDIA_BIOSTEXT				1024
#define IDC_MEDIA_BIOSFILE				1025
#define IDC_MEDIA_BIOSBROWSE			1026
#define IDC_MEDIA_BIOSDEFAULT			1027
#define IDC_MEDIA_RAMSIZETEXT			1028
#define IDC_MEDIA_RAMSIZE				1029
#define IDC_MEDIA_RAMSLOTTEXT			1030
#define IDC_MEDIA_RAMSLOT				1031
#define IDC_MEDIA_VDPTEXT				1032
#define IDC_MEDIA_VDP					1033
#define IDC_MEDIA_PSGTEXT				1034
#define IDC_MEDIA_PSG					1035
#define IDC_MEDIA_INTTEXT				1036

/* tape tab */
#define IDC_MEDIA_TAPEFILET				1040
#define IDC_MEDIA_TAPEBROWSE			1041
#define IDC_MEDIA_TAPEFILE				1042
#define IDC_MEDIA_TAPEEJECT				1043
#define IDC_MEDIA_TAPENEW				1044
#define IDC_MEDIA_TAPECONTENTST			1045
#define IDC_MEDIA_TAPECONTENTS			1046
#define IDC_MEDIA_TAPER					1047
#define IDC_MEDIA_TAPEPOS				1048
#define IDC_MEDIA_TAPESIZE				1049
/* tape popup menu */
#define IDM_MEDIATAPE_INSERT			1050
#define IDM_MEDIATAPE_OPEN				1051
#define IDM_MEDIATAPE_SAVE				1052
#define IDM_MEDIATAPE_DELETE			1053
#define IDM_MEDIATAPE_RENAME			1054
#define IDM_MEDIATAPE_INFO				1055
/* tape block rename dialog */
#define IDD_MEDIATAPE_RENAME			1056
#define IDC_MEDIATAPE_RENAME_EDIT		1057
/* tape block info dialog */
#define IDD_MEDIATAPE_INFO				1058
#define IDC_MEDIATAPE_INFO_TEXT			1059
/* start/end of tab */
#define IDC_MEDIA_TAB0START				IDC_MEDIA_SLOT1TEXT
#define IDC_MEDIA_TAB0END				IDC_MEDIA_SLOT2EXTRA
#define IDC_MEDIA_TAB1START				IDC_MEDIA_TAPEFILET
#define IDC_MEDIA_TAB1END				IDC_MEDIA_TAPESIZE
#define IDC_MEDIA_TAB2START				IDC_MEDIA_BIOSTEXT
#define IDC_MEDIA_TAB2END				IDC_MEDIA_INTTEXT

/* custom vidsignal */
#define IDD_VIDEOSIGNAL					1100
#define IDC_VIDSIGNAL_SHARPNESS_SLIDER	1101
#define IDC_VIDSIGNAL_SHARPNESS_EDIT	1102
#define IDC_VIDSIGNAL_RESOLUTION_SLIDER	1103
#define IDC_VIDSIGNAL_RESOLUTION_EDIT	1104
#define IDC_VIDSIGNAL_FRINGING_SLIDER	1105
#define IDC_VIDSIGNAL_FRINGING_EDIT		1106
#define IDC_VIDSIGNAL_ARTIFACTS_SLIDER	1107
#define IDC_VIDSIGNAL_ARTIFACTS_EDIT	1108
#define IDC_VIDSIGNAL_BLEED_SLIDER		1109
#define IDC_VIDSIGNAL_BLEED_EDIT		1110

/* palette general */
#define IDD_PALETTE						1130
#define IDC_PALGEN_PRESET				1131
#define IDC_PALGEN_EDIT					1132
#define IDC_PALGEN_HUE_SLIDER			1133
#define IDC_PALGEN_SAT_SLIDER			1134
#define IDC_PALGEN_CON_SLIDER			1135
#define IDC_PALGEN_BRI_SLIDER			1136
#define IDC_PALGEN_GAM_SLIDER			1137
#define IDC_PALGEN_HUE_EDIT				1138
#define IDC_PALGEN_SAT_EDIT				1139
#define IDC_PALGEN_CON_EDIT				1140
#define IDC_PALGEN_BRI_EDIT				1141
#define IDC_PALGEN_GAM_EDIT				1142
#define IDC_PALGEN_DEFAULT				1143
/* palette edit */
#define IDD_PALETTE_EDIT				1160
#define IDC_PALEDIT_REDIT				1161
#define IDC_PALEDIT_GEDIT				1162
#define IDC_PALEDIT_BEDIT				1163
#define IDC_PALEDIT_RSLIDER				1164
#define IDC_PALEDIT_GSLIDER				1165
#define IDC_PALEDIT_BSLIDER				1166
#define IDC_PALEDIT_COLOUR				1167
#define IDC_PALEDIT_SAVE				1168
/* palette decoder */
#define IDC_PALDEC_PRESET				1190
#define IDC_PALDEC_PROP					1191
#define IDC_PALDEC_RY_SLIDER			1192
#define IDC_PALDEC_GY_SLIDER			1193
#define IDC_PALDEC_BY_SLIDER			1194
#define IDC_PALDEC_RY_EDIT				1195
#define IDC_PALDEC_GY_EDIT				1196
#define IDC_PALDEC_BY_EDIT				1197
#define IDC_PALDEC_RY_GAIN				1198
#define IDC_PALDEC_GY_GAIN				1199
#define IDC_PALDEC_BY_GAIN				1200
#define IDC_PALDEC_PROPG				1201
#define IDC_PALDEC_PROPRY				1202
#define IDC_PALDEC_PROPGY				1203
#define IDC_PALDEC_PROPBY				1204

/* help */
#define IDD_HELP						1220
#define IDC_HELPTEXT					1221

/* about */
#define IDD_ABOUT						1225
#define IDC_ABOUT_URL					1226
#define IDC_ABOUT_ICON					1227
#define IDC_ABOUT_TEXT					1228
#define IDC_ABOUT_EMAIL					1229

/* update */
#define IDD_UPDATE						1231
#define IDC_UPDATE_VIEW					1232
#define IDC_UPDATE_APPLY				1233

/* directories */
#define IDD_DIRECTORIES					1251
#define IDC_DIR_TOOLEDIT				1252
#define IDC_DIR_TOOLBROWSE				1253
#define IDC_DIR_TOOLDEFAULT				1254
#define IDC_DIR_PATCHESEDIT				1255
#define IDC_DIR_PATCHESBROWSE			1256
#define IDC_DIR_PATCHESDEFAULT			1257
#define IDC_DIR_SAMPLESEDIT				1258
#define IDC_DIR_SAMPLESBROWSE			1259
#define IDC_DIR_SAMPLESDEFAULT			1260
#define IDC_DIR_SCREENSHOTSEDIT			1261
#define IDC_DIR_SCREENSHOTSBROWSE		1262
#define IDC_DIR_SCREENSHOTSDEFAULT		1263
#define IDC_DIR_SRAMEDIT				1264
#define IDC_DIR_SRAMBROWSE				1265
#define IDC_DIR_SRAMDEFAULT				1266
#define IDC_DIR_STATESEDIT				1267
#define IDC_DIR_STATESBROWSE			1268
#define IDC_DIR_STATESDEFAULT			1269

/* timing */
#define IDD_TIMING						1291
#define IDC_TIMING_REVERSE_GROUP		1292
#define IDC_TIMING_REVERSE_SLIDER		1293
#define IDC_TIMING_REVERSE_INFO			1294
#define IDC_TIMING_REVERSE_TIME			1295
#define IDC_TIMING_SPEED_GROUP			1296
#define IDC_TIMING_SLOWDOWN_EDIT		1297
#define IDC_TIMING_SLOWDOWN_SPIN		1298
#define IDC_TIMING_SPEEDUP_EDIT			1299
#define IDC_TIMING_SPEEDUP_SPIN			1300
#define IDC_TIMING_Z80_GROUP			1301
#define IDC_TIMING_Z80_TEXT				1302
#define IDC_TIMING_Z80_EDIT				1303
#define IDC_TIMING_Z80_SPIN				1304
#define IDC_TIMING_Z80_INFO				1305

/* tile viewer */
#define IDD_TILEVIEW					1351
#define IDC_TILEVIEW_SOURCE				1352
#define IDC_TILEVIEW_BLANK				1353
#define IDC_TILEVIEW_M1					1354
#define IDC_TILEVIEW_M2					1355
#define IDC_TILEVIEW_M3					1356
#define IDC_TILEVIEW_NT					1357
#define IDC_TILEVIEW_PT					1358
#define IDC_TILEVIEW_CT					1359
#define IDC_TILEVIEW_B0					1360
#define IDC_TILEVIEW_B1					1361
#define IDC_TILEVIEW_B2					1362
#define IDC_TILEVIEW_TILES				1363
#define IDC_TILEVIEW_XY					1364
#define IDC_TILEVIEW_HTILE				1365
#define IDC_TILEVIEW_HPATT				1366
#define IDC_TILEVIEW_HTEXTT				1367
#define IDC_TILEVIEW_HTEXTAT			1368
#define IDC_TILEVIEW_HTEXTA				1369
#define IDC_TILEVIEW_HTEXTNA			1370
#define IDC_TILEVIEW_FCHECK				1371
#define IDC_TILEVIEW_FEDIT				1372
#define IDC_TILEVIEW_FSPIN				1373
#define IDC_TILEVIEW_FTEXT				1374
#define IDC_TILEVIEW_SEPARATOR			1375
#define IDC_TILEVIEW_DETAILST			1376
#define IDC_TILEVIEW_DETAILSC			1377
#define IDC_TILEVIEW_SDETAILS			1378
#define IDC_TILEVIEW_OPEN				1379
#define IDC_TILEVIEW_SAVE				1380
/* tile viewer open template */
#define IDD_TILEVIEW_OPEN				1411
#define IDC_TILEVIEWOPEN_NT				1412
#define IDC_TILEVIEWOPEN_PGT			1413
#define IDC_TILEVIEWOPEN_CT				1414
#define IDC_TILEVIEWOPEN_SIM			1415
#define IDC_TILEVIEWOPEN_INFO			1416
/* tile viewer save template */
#define IDD_TILEVIEW_SAVE				1417
#define IDC_TILEVIEWSAVE_MASK			1418
#define IDC_TILEVIEWSAVE_HEADER			1419
#define IDC_TILEVIEWSAVE_HELP			1420
#define IDC_TILEVIEWSAVE_INFO			1421
/* tile viewer save screen info */
#define IDD_TILEVIEW_SINFO				1422
#define IDC_TILEVIEW_SINFO_TEXT			1423
/* tile viewer popup menu */
#define IDM_TILEVIEW_CUT				1424
#define IDM_TILEVIEW_COPY				1425
#define IDM_TILEVIEW_PASTE				1426
#define IDM_TILEVIEW_HL					1427
#define IDM_TILEVIEW_SHOWHL				1428
#define IDM_TILEVIEW_NTHL				1429
#define IDM_TILEVIEW_NTCHANGE			1430
#define IDM_TILEVIEW_EDIT				1431
/* tileviewer change nametable byte */
#define IDD_TILEVIEW_CHANGENT			1432
#define IDC_TILEVIEW_CHANGENT_A			1433
#define IDC_TILEVIEW_CHANGENT_V			1434
#define IDC_TILEVIEW_CHANGENT_S			1435
#define IDC_TILEVIEW_CHANGENT_VX		1436
/* tile editor */
#define IDD_TILE0EDIT					1471
#define IDD_TILE3EDIT					1472
#define IDC_TILEEDIT_APPLY				1473
#define IDC_TILEEDIT_OPEN				1474
#define IDC_TILEEDIT_SAVE				1475
#define IDC_TILEEDIT_ALLBLOCKS			1476
#define IDC_TILEEDIT_ZOOMT				1477
#define IDC_TILEEDIT_ZOOM				1478
#define IDC_TILEEDIT_FG					1479
#define IDC_TILEEDIT_BG					1480
#define IDC_TILEEDIT_XYT				1481
#define IDC_TILEEDIT_XY					1482
#define IDC_TILEEDIT_CT					1483
#define IDC_TILEEDIT_C					1484

/* sprite viewer */
#define IDD_SPRITEVIEW					1501
#define IDC_SPRITEVIEW_BLANK			1502
#define IDC_SPRITEVIEW_M1				1503
#define IDC_SPRITEVIEW_ACTIVE			1504
#define IDC_SPRITEVIEW_SIZE				1505
#define IDC_SPRITEVIEW_MAG				1506
#define IDC_SPRITEVIEW_STATUS			1507
#define IDC_SPRITEVIEW_C				1508
#define IDC_SPRITEVIEW_5S				1509
#define IDC_SPRITEVIEW_PATA				1510
#define IDC_SPRITEVIEW_SEPARATOR1		1511
#define IDC_SPRITEVIEW_TILES			1512
#define IDC_SPRITEVIEW_HL				1513
#define IDC_SPRITEVIEW_HLINFO			1514
#define IDC_SPRITEVIEW_HLPT				1515
#define IDC_SPRITEVIEW_HLP				1516
#define IDC_SPRITEVIEW_HLAT				1517
#define IDC_SPRITEVIEW_HLA				1518
#define IDC_SPRITEVIEW_HLST				1519
#define IDC_SPRITEVIEW_HLS				1520
#define IDC_SPRITEVIEW_SPATA			1521
#define IDC_SPRITEVIEW_SEPARATOR2		1522
#define IDC_SPRITEVIEW_SPAT				1523
#define IDC_SPRITEVIEW_SPATT			1524
#define IDC_SPRITEVIEW_SPATS			1525
#define IDC_SPRITEVIEW_SPATSAT			1526
#define IDC_SPRITEVIEW_SPATSA			1527
#define IDC_SPRITEVIEW_SPATXY			1528
#define IDC_SPRITEVIEW_SPATXYH			1529
#define IDC_SPRITEVIEW_SPATXYD			1530
#define IDC_SPRITEVIEW_SPATCT			1531
#define IDC_SPRITEVIEW_SPATC			1532
#define IDC_SPRITEVIEW_DETAILST			1533
#define IDC_SPRITEVIEW_SDETAILS			1534
#define IDC_SPRITEVIEW_OPEN				1535
#define IDC_SPRITEVIEW_SAVE				1536
/* sprite viewer popup menu */
#define IDM_SPRITEVIEW_SPAT				1561
#define IDM_SPRITEVIEW_CUT				1562
#define IDM_SPRITEVIEW_COPY				1563
#define IDM_SPRITEVIEW_PASTE			1564
#define IDM_SPRITEVIEW_EDIT				1565
/* sprite viewer change spat */
#define IDD_SPRITEVIEW_CHANGESPAT		1566
#define IDC_SPRITEVIEW_CS_GROUP			1567
#define IDC_SPRITEVIEW_CS_PATTERN		1568
#define IDC_SPRITEVIEW_CS_PATTERNS		1569
#define IDC_SPRITEVIEW_CS_PATTERNX		1570
#define IDC_SPRITEVIEW_CS_POSX			1571
#define IDC_SPRITEVIEW_CS_POSY			1572
#define IDC_SPRITEVIEW_CS_COLOUR		1573
#define IDC_SPRITEVIEW_CS_EC			1574
/* sprite editor */
#define IDD_SPRITEEDIT					1601
#define IDC_SPRITEEDIT_APPLY			1602
#define IDC_SPRITEEDIT_OPEN				1603
#define IDC_SPRITEEDIT_SAVE				1604
#define IDC_SPRITEEDIT_ZOOMT			1605
#define IDC_SPRITEEDIT_ZOOM				1606
#define IDC_SPRITEEDIT_XY				1607

/* psg toy */
#define IDD_PSGTOY						1641
#define IDC_PSGTOY_PIANO				1642
#define IDC_PSGTOY_ACTAT				1643
#define IDC_PSGTOY_ACTBT				1644
#define IDC_PSGTOY_ACTCT				1645
#define IDC_PSGTOY_ACTAS				1646
#define IDC_PSGTOY_ACTBS				1647
#define IDC_PSGTOY_ACTCS				1648
#define IDC_PSGTOY_ACTES				1649
#define IDC_PSGTOY_ACTAV				1650
#define IDC_PSGTOY_ACTBV				1651
#define IDC_PSGTOY_ACTCV				1652
#define IDC_PSGTOY_ACTEV				1653
#define IDC_PSGTOY_ACTAD				1654
#define IDC_PSGTOY_ACTBD				1655
#define IDC_PSGTOY_ACTCD				1656
#define IDC_PSGTOY_ACTND				1657
#define IDC_PSGTOY_ACTED				1658
#define IDC_PSGTOY_LOGSTART				1659
#define IDC_PSGTOY_LOGSTOP				1660
#define IDC_PSGTOY_LOGINFO				1661
#define IDC_PSGTOY_CUSTOM				1662
#define IDC_PSGTOY_CAR					1663
#define IDC_PSGTOY_CBR					1664
#define IDC_PSGTOY_CCR					1665
#define IDC_PSGTOY_PRESETT				1666
#define IDC_PSGTOY_PRESET				1667
#define IDC_PSGTOY_WAVE					1668
#define IDC_PSGTOY_OPEN					1669
#define IDC_PSGTOY_SAVE					1670
#define IDC_PSGTOY_VTEXT				1671
#define IDC_PSGTOY_VEDIT				1672
#define IDC_PSGTOY_VSPIN				1673
#define IDC_PSGTOY_VPER					1674
#define IDC_PSGTOY_XY					1675
#define IDC_PSGTOY_XYS					1676
/* piano popup menu */
#define IDM_PSGTOYPIANO_A				1677
#define IDM_PSGTOYPIANO_B				1678
#define IDM_PSGTOYPIANO_C				1679
#define IDM_PSGTOYPIANO_CENT			1680
/* change cent offset dialog */
#define IDD_PSGTOY_CENT					1681
#define IDC_PSGTOYCENT_S				1682
#define IDC_PSGTOYCENT_T				1683
/* psglogger save */
#define IDD_PSGLOG_SAVE					1684
#define IDC_YMLOGSAVE_SONG				1685
#define IDC_YMLOGSAVE_AUTHOR			1686
#define IDC_YMLOGSAVE_COMMENT			1687
#define IDC_YMLOGSAVE_INTERLEAVED		1688
#define IDC_YMLOGSAVE_TRIMSILENCE		1689


/* shared */
/* colour pick */
#define IDC_PALEDIT_00					3501
#define IDC_PALEDIT_01					3502
#define IDC_PALEDIT_02					3503
#define IDC_PALEDIT_03					3504
#define IDC_PALEDIT_04					3505
#define IDC_PALEDIT_05					3506
#define IDC_PALEDIT_06					3507
#define IDC_PALEDIT_07					3508
#define IDC_PALEDIT_08					3509
#define IDC_PALEDIT_09					3510
#define IDC_PALEDIT_10					3511
#define IDC_PALEDIT_11					3512
#define IDC_PALEDIT_12					3513
#define IDC_PALEDIT_13					3514
#define IDC_PALEDIT_14					3515
#define IDC_PALEDIT_15					3516
/* ticks */
/* horizontal up */
#define IDC_TICKS_HU00					3517
#define IDC_TICKS_HU01					3518
#define IDC_TICKS_HU02					3519
#define IDC_TICKS_HU03					3520
#define IDC_TICKS_HU04					3521
#define IDC_TICKS_HU05					3522
#define IDC_TICKS_HU06					3523
#define IDC_TICKS_HU07					3524
#define IDC_TICKS_HU08					3525
#define IDC_TICKS_HU09					3526
#define IDC_TICKS_HU10					3527
#define IDC_TICKS_HU11					3528
#define IDC_TICKS_HU12					3529
#define IDC_TICKS_HU13					3530
#define IDC_TICKS_HU14					3531
#define IDC_TICKS_HU15					3532
/* horizontal down */
#define IDC_TICKS_HD00					3533
#define IDC_TICKS_HD01					3534
#define IDC_TICKS_HD02					3535
#define IDC_TICKS_HD03					3536
#define IDC_TICKS_HD04					3537
#define IDC_TICKS_HD05					3538
#define IDC_TICKS_HD06					3539
#define IDC_TICKS_HD07					3540
#define IDC_TICKS_HD08					3541
#define IDC_TICKS_HD09					3542
#define IDC_TICKS_HD10					3543
#define IDC_TICKS_HD11					3544
#define IDC_TICKS_HD12					3545
#define IDC_TICKS_HD13					3546
#define IDC_TICKS_HD14					3547
#define IDC_TICKS_HD15					3548
/* vertical left */
#define IDC_TICKS_VL00					3549
#define IDC_TICKS_VL01					3550
#define IDC_TICKS_VL02					3551
#define IDC_TICKS_VL03					3552
#define IDC_TICKS_VL04					3553
#define IDC_TICKS_VL05					3554
#define IDC_TICKS_VL06					3555
#define IDC_TICKS_VL07					3556
#define IDC_TICKS_VL08					3557
#define IDC_TICKS_VL09					3558
#define IDC_TICKS_VL10					3559
#define IDC_TICKS_VL11					3560
#define IDC_TICKS_VL12					3561
#define IDC_TICKS_VL13					3562
#define IDC_TICKS_VL14					3563
#define IDC_TICKS_VL15					3564
/* vertical right */
#define IDC_TICKS_VR00					3565
#define IDC_TICKS_VR01					3566
#define IDC_TICKS_VR02					3567
#define IDC_TICKS_VR03					3568
#define IDC_TICKS_VR04					3569
#define IDC_TICKS_VR05					3570
#define IDC_TICKS_VR06					3571
#define IDC_TICKS_VR07					3572
#define IDC_TICKS_VR08					3573
#define IDC_TICKS_VR09					3574
#define IDC_TICKS_VR10					3575
#define IDC_TICKS_VR11					3576
#define IDC_TICKS_VR12					3577
#define IDC_TICKS_VR13					3578
#define IDC_TICKS_VR14					3579
#define IDC_TICKS_VR15					3580

/* menu */
#define IDM_MEDIA						4001
#define IDM_NETPLAY						4002
#define IDM_EXITEMU						4003
#define IDM_HARDRESET					4004
#define IDM_SOFTRESET					4005
#define IDM_PASTE						4006
#define IDM_VIDEOFORMAT					4007
#define IDM_PAUSEEMU					4008
#define IDM_SOUND						4009
#define IDM_FULLSCREEN					4010
#define IDM_RENDERD3D					4011
#define IDM_RENDERDD					4012
#define IDM_SOFTRENDER					4013
#define IDM_BFILTER						4014
#define IDM_VSYNC						4015
#define IDM_CORRECTASPECT				4016
#define IDM_STRETCHFIT					4017
#define IDM_VSRGB						4018
#define IDM_VSMONOCHROME				4019
#define IDM_VSCOMPOSITE					4020
#define IDM_VSSVIDEO					4021
#define IDM_VSCUSTOM					4022
#define IDM_PALETTE						4023
#define IDM_ABOUT						4024
#define IDM_HELP						4025
#define IDM_STATE_LOAD					4026
#define IDM_STATE_SAVE					4027
#define IDM_DIRECTORIES					4028
#define IDM_TIMING						4029
#define IDM_FLIP_X						4030
#define IDM_SHELLMSG					4031
#define IDM_TIMECODE					4032
#define IDM_SCREENSHOT					4033
#define IDM_LUMINOISE					4034
#define IDM_UPDATE						4035
#define IDM_ZOOM1						4036
#define IDM_ZOOM2						4037
#define IDM_ZOOM3						4038
#define IDM_ZOOM4						4039
#define IDM_ZOOM5						4040
#define IDM_ZOOM6						4041

#define IDM_TILEVIEW					4066
#define IDM_SPRITEVIEW					4067
#define IDM_PSGTOY						4068

#define IDM_STATE_CHANGE				4101
#define IDM_STATE_0						4102
#define IDM_STATE_1						4103
#define IDM_STATE_2						4104
#define IDM_STATE_3						4105
#define IDM_STATE_4						4106
#define IDM_STATE_5						4107
#define IDM_STATE_6						4108
#define IDM_STATE_7						4109
#define IDM_STATE_8						4110
#define IDM_STATE_9						4111
#define IDM_STATE_A						4112
#define IDM_STATE_B						4113
#define IDM_STATE_C						4114
#define IDM_STATE_D						4115
#define IDM_STATE_E						4116
#define IDM_STATE_F						4117
#define IDM_STATE_CUSTOM				4118
#define IDM_STATE_M						4119

#define IDM_MOVIE_RECORD				4151
#define IDM_MOVIE_PLAY					4152
#define IDM_MOVIE_STOP					4153

#define IDM_LAYERB						4161
#define IDM_LAYERS						4162
#define IDM_LAYERUNL					4163

#define IDM_PORT1						4201
#define IDM_PORT2						4202
#define IDM_PORT1_J						4203
#define IDM_PORT1_M						4204
#define IDM_PORT1_T						4205
#define IDM_PORT1_HS					4206
#define IDM_PORT1_MAGIC					4207
#define IDM_PORT1_ARK					4208
#define IDM_PORT1_NO					4209
#define IDM_PORT2_J						4210
#define IDM_PORT2_M						4211
#define IDM_PORT2_T						4212
#define IDM_PORT2_HS					4213
#define IDM_PORT2_MAGIC					4214
#define IDM_PORT2_ARK					4215
#define IDM_PORT2_NO					4216

#define IDM_PORT_FIRST					IDM_PORT1_J
#define IDM_PORT_SIZE					(IDM_PORT2_J-IDM_PORT_FIRST)

#define IDM_CPUSPEEDMIN					4901
#define IDM_CPUSPEEDPLUS				4902
#define IDM_CPUSPEEDNORMAL				4903
#define IDM_PREVSLOT					4904
#define IDM_NEXTSLOT					4905
#define IDM_TAPEREWIND					4906
#define IDM_GRABMOUSE					4907
#define IDM_PASTEBOOT					4908

#endif /* RESOURCE_H */
