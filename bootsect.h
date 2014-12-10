/* $FreeBSD: src/sys/fs/msdosfs/bootsect.h,v 1.11 2003/12/26 17:24:37 trhodes Exp $ */
/*	$NetBSD: bootsect.h,v 1.9 1997/11/17 15:36:17 ws Exp $	*/

#ifndef __BOOTSECT_H__
#define __BOOTSECT_H__

/*
 * Written by Paul Popelka (paulp@uts.amdahl.com)
 *
 * You can do anything you want with this software, just don't say you wrote
 * it, and don't remove this notice.
 *
 * This software is provided "as is".
 *
 * The author supplies this software to be publicly redistributed on the
 * understanding that the author is not responsible for the correct
 * functioning of this software in any circumstances and is not liable for
 * any damages caused by this software.
 *
 * October 1992
 */

/*
 * Format of a boot sector.  This is the first sector on a DOS floppy disk
 * or the fist sector of a partition on a hard disk.  But, it is not the
 * first sector of a partitioned hard disk.
 */
struct bootsector33 {
	u_int8_t	bsJump[3];		/* jump inst E9xxxx or EBxx90 */
	int8_t		bsOemName[8];		/* OEM name and version */
	int8_t		bsBPB[19];		/* BIOS parameter block */
	int8_t		bsDriveNumber;		/* drive number (0x80) */
	int8_t		bsBootCode[479];	/* pad so struct is 512b */
	u_int8_t	bsBootSectSig0;
	u_int8_t	bsBootSectSig1;
#define	BOOTSIG0	0x55
#define	BOOTSIG1	0xaa
};

struct extboot {
	int8_t		exDriveNumber;		/* drive number (0x80) */
	int8_t		exReserved1;		/* reserved */
	int8_t		exBootSignature;	/* ext. boot signature (0x29) */
#define	EXBOOTSIG	0x29
	int8_t		exVolumeID[4];		/* volume ID number */
	int8_t		exVolumeLabel[11];	/* volume label */
	int8_t		exFileSysType[8];	/* fs type (FAT12 or FAT16) */
};

struct bootsector50 {
	u_int8_t	bsJump[3];		/* jump inst E9xxxx or EBxx90 */
	int8_t		bsOemName[8];		/* OEM name and version */
	int8_t		bsBPB[25];		/* BIOS parameter block */
	int8_t		bsExt[26];		/* Bootsector Extension */
	int8_t		bsBootCode[448];	/* pad so structure is 512b */
	u_int8_t	bsBootSectSig0;
	u_int8_t	bsBootSectSig1;
#define	BOOTSIG0	0x55
#define	BOOTSIG1	0xaa
};

struct bootsector710 {
	u_int8_t	bsJump[3];		/* jump inst E9xxxx or EBxx90 */
	int8_t		bsOEMName[8];		/* OEM name and version */
	int8_t		bsBPB[53];		/* BIOS parameter block */
	int8_t		bsExt[26];		/* Bootsector Extension */
	int8_t		bsBootCode[418];	/* pad so structure is 512b */
	u_int8_t	bsBootSectSig2;		/* 2 & 3 are only defined for FAT32? */
	u_int8_t	bsBootSectSig3;
	u_int8_t	bsBootSectSig0;
	u_int8_t	bsBootSectSig1;
#define	BOOTSIG0	0x55
#define	BOOTSIG1	0xaa
#define	BOOTSIG2	0
#define	BOOTSIG3	0
};

union bootsector {
	struct bootsector33 bs33;
	struct bootsector50 bs50;
	struct bootsector710 bs710;
};


#endif // __BOOTSECT_H__