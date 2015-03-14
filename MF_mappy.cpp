//  MF_mappy.cpp: MF/TIN compiler FMP parser
//
//  Copyright (C) 2015 Ulrich Hecht
//  Derived from FMP2GBA 1.0, Copyright (C) Dave Etherton
//
//  This software is provided 'as-is', without any express or implied
//  warranty.  In no event will the authors be held liable for any damages
//  arising from the use of this software.
//
//  Permission is granted to anyone to use this software for any purpose,
//  including commercial applications, and to alter it and redistribute it
//  freely, subject to the following restrictions:
//
//  1. The origin of this software must not be misrepresented; you must not
//     claim that you wrote the original software. If you use this software
//     in a product, an acknowledgment in the product documentation would be
//     appreciated but is not required.
//  2. Altered source versions must be plainly marked as such, and must not be
//     misrepresented as being the original software.
//  3. This notice may not be removed or altered from any source distribution.
//
//  Ulrich Hecht
//  ulrich.hecht@gmail.com



#include <stdio.h>
#include <stdlib.h>
#include "MF.h"

#define FOURCC(a,b,c,d) (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))

// borrowed from mappy:
typedef struct {	/* Structure for data blocks */
	int bgoff, fgoff;	/* offsets from start of graphic blocks */
	int fgoff2, fgoff3; /* more overlay blocks */
	unsigned int user1, user2;	/* user long data */
	unsigned short int user3, user4;	/* user short data */
	unsigned char user5, user6, user7;	/* user byte data */
	unsigned char tl : 1;	/* bits for collision detection */
	unsigned char tr : 1;
	unsigned char bl : 1;
	unsigned char br : 1;
	unsigned char trigger : 1;	/* bit to trigger an event */
	unsigned char unused1 : 1;
	unsigned char unused2 : 1;
	unsigned char unused3 : 1;
} BLKSTR;

typedef struct { /* Animation control structure */
	signed char antype;	/* Type of anim, AN_? */
	signed char andelay;	/* Frames to go before next frame */
	signed char ancount;	/* Counter, decs each frame, till 0, then resets to andelay */
	signed char anuser;	/* User info */
	int ancuroff;	/* Points to current offset in list */
	int anstartoff;	/* Points to start of blkstr offsets list, AFTER ref. blkstr offset */
	int anendoff;	/* Points to end of blkstr offsets list */
} ANISTR;

typedef struct {	/* Map header structure */
	/* char M,P,H,D;	4 byte chunk identification. */
	/* long int mphdsize;	size of map header. */
	char mapverhigh;	/* map version number to left of . (ie X.0). */
	char mapverlow;		/* map version number to right of . (ie 0.X). */
	char lsb;		/* if 1, data stored LSB first, otherwise MSB first. */
	char bodytype;	/* 0 for 32 offset still, -16 offset anim shorts in BODY */
	short int mapwidth;	/* width in blocks. */
	short int mapheight;	/* height in blocks. */
	short int reserved1;
	short int reserved2;
	short int blockwidth;	/* width of a block (tile) in pixels. */
	short int blockheight;	/* height of a block (tile) in pixels. */
	short int blockdepth;	/* depth of a block (tile) in planes (ie. 256 colours is 8) */
	short int blockstrsize;	/* size of a block data structure */
	short int numblockstr;	/* Number of block structures in BKDT */
	short int numblockgfx;	/* Number of 'blocks' in graphics (BODY) */
	unsigned char ckey8bit, ckeyred, ckeygreen, ckeyblue; /* colour key values */
} MPHD;

static MPHD *Header;
static BLKSTR *Blocks;

static void DecodeMPHD(MPHD *block,int tagLen) {
	if (block->mapverhigh != 1 || block->mapverlow != 0)
		GLB_warning("Expecting version 1.0 header, might not work (re-save after editing map properties)\n");
	Header = block;
}

static void DecodeCMAP(unsigned char *rgb,int tagLen, Output *out) {
	int i;
	for (i=0; i<tagLen; i += 3) {
		int p = (rgb[i] >> 3) | ((rgb[i+1]>>3) << 5) | ((rgb[i+2]>>3) << 10);
		out->emitWord(p);
	}
	free(rgb);
}

static void DecodeBKDT(BLKSTR *str,int tagLen) {
	Blocks = str;
}

static void DecodeBGFX(unsigned char *bits,int tagLen, Output *out) {
	int i, j;
	int area = Header->blockwidth * Header->blockheight;

	if (!Header) {
		GLB_warning("got BGFX before header!\n");
		return;
	}

	tagLen /= area;

	while (tagLen) {
		for (i=0; i<Header->blockheight; i+=8) {
			for (j=0; j<Header->blockwidth; j+=8) {
				unsigned char *base = bits + j + i * Header->blockwidth;
				int m, n;
				for (m=0; m<8; m++,base += Header->blockwidth) {
					for (n=0; n<8; n++)
						out->emitByte(base[n]);
				}
			}
		}
		tagLen--;
		bits += area;
	}
}

static int first_nonzero(int a,int b) {
	return a? a : b;
}

static void DecodeBODY(short *body,int tagLen, Output *out) {
	int i;
	if (!Header || !Blocks) {
		GLB_warning("got BODY before header and BKDT!\n");
		return;
	}

	tagLen >>= 1;
	for (i=0; i<tagLen; i++) {
		//char x;
		if (body[i] >= 0) {
			out->emitByte(first_nonzero(Blocks[body[i]].bgoff, Blocks[body[i]].fgoff));
		}
		else {
			out->emitByte(0);
		}
	}
}

static int GetWord(FILE *f) {	// big-endian word
	int w = fgetc(f) << 24;
	w |= fgetc(f) << 16;
	w |= fgetc(f) << 8;
	w |= fgetc(f);
	return w;
}

int Decode(const char *filename,Output *out) {
	FILE *f = fopen(filename,"rb");
	int totalSize;
	int tag;

	if (!f)
		GLB_error("'%s' - file not found\n",filename);

	if (GetWord(f) != FOURCC('F','O','R','M')) {
		fclose(f);
		GLB_error("'%s' - not a IFF format file\n",filename);
	}

	totalSize = GetWord(f) - 4;	// form type is counted here

	if (GetWord(f) != FOURCC('F','M','A','P')) {
		fclose(f);
		GLB_error("'%s' - not an FMAP file\n",filename);
	}

	unsigned int cmap_ptr = out->addr;
	out->emitDword(0);
	unsigned int gfx_ptr = out->addr;
	out->emitDword(0);
	unsigned int map_ptr = out->addr;
	out->emitDword(0);
	unsigned int dim = out->addr;
	out->emitDword(0);

	while ((tag = GetWord(f)) != -1) {
		int tagLen = GetWord(f);
		void *block = malloc(tagLen);
		fread(block,tagLen,1,f);

		totalSize -= 8 + tagLen;
		if (tag == FOURCC('M','P','H','D')) {
			DecodeMPHD((MPHD *)block,tagLen);
			DEBUG("map w %d h %d block w %d h %d\n",
				Header->mapwidth,
				Header->mapheight,
				Header->blockwidth,
				Header->blockheight);
			// XXX: endianness...
			out->patch32(dim,
				(Header->mapheight    << 24) |
				(Header->mapwidth   << 16) |
				(Header->blockheight  <<  8) |
				(Header->blockwidth <<  0));
		} else if (tag == FOURCC('C','M','A','P')) {
			out->patch32(cmap_ptr, out->addr);
			DecodeCMAP((unsigned char *)block,tagLen,out);
		}
		else if (tag == FOURCC('B','K','D','T'))
			DecodeBKDT((BLKSTR *)block,tagLen);
		else if (tag == FOURCC('B','G','F','X')) {
			out->patch32(gfx_ptr, out->addr);
			DecodeBGFX((unsigned char *)block,tagLen,out);
		}
		else if (tag == FOURCC('B','O','D','Y')) {
			out->patch32(map_ptr, out->addr);
			DecodeBODY((short *)block,tagLen,out);
		}
		else {
			GLB_warning("code %x (%d bytes) skipped\n",tag,tagLen);
			free(block);
		}
	}

	if (totalSize)
		GLB_error("'%s' - warning - %d bytes after IFF\n",filename,totalSize);

	fclose(f);
	return 0;
}
