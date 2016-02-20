#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#define NS_PER_DOT	140
#define MADR(_bnk,_adr)	((_bnk) << 14) + (_adr)

#include "video.h"

int vidFlag = 0;
// float brdsize = 1.0;

unsigned char inkTab[] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
};

unsigned char papTab[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
  0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
  0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
  0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
  0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
  0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
  0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
  0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
};

// zx screen adr
unsigned short scrAdrs[8192];
unsigned short atrAdrs[8192];

void vidInitAdrs() {
	unsigned short sadr = 0x0000;
	unsigned short aadr = 0x1800;
	int idx = 0;
	int a,b,c,d;
	for (a = 0; a < 4; a++) {	// parts (4th for profi)
		for (b = 0; b < 8; b++) {	// box lines
			for (c = 0; c < 8; c++) {	// pixel lines
				for (d = 0; d < 32; d++) {	// x bytes
					scrAdrs[idx] = sadr + d;
					atrAdrs[idx] = aadr + d;
					idx++;
				}
				sadr += 0x100;
			}
			sadr -= 0x7e0;
			aadr += 0x20;
		}
		sadr += 0x700;
	}
}

Video* vidCreate(Memory* me) {
	Video* vid = (Video*)malloc(sizeof(Video));
	memset(vid,0x00,sizeof(Video));
	vid->mem = me;
	vid->full.h = 448;
	vid->full.v = 320;
	vid->bord.h = 136;
	vid->bord.v = 80;
	vid->sync.h = 80;
	vid->sync.v = 32;
	vid->intpos.h = 0;
	vid->intpos.v = 0;
	vid->intSize = 64;
	vid->frmsz = vid->full.h * vid->full.v;
	vid->intMask = 0x01;		// FRAME INT for all
	vid->ula = ulaCreate();
	vidUpdate(vid, 0.5);

	vidSetMode(vid,VID_NORMAL);

	vid->nextbrd = 0;
	vid->curscr = 5;
	vid->fcnt = 0;

	vid->nsDraw = 0;
	vid->x = 0;
	vid->y = 0;
	vid->idx = 0;

	// vid->scrimg = (unsigned char*)malloc(1024 * 1024 * 3);
	vid->scrptr = vid->scrimg;
	vid->linptr = vid->scrimg;

	return vid;
}

void vidDestroy(Video* vid) {
	ulaDestroy(vid->ula);
	free(vid);
}

void vidUpdate(Video* vid, float brdsize) {
	if (brdsize < 0.0) brdsize = 0.0;
	if (brdsize > 1.0) brdsize = 1.0;
	vid->lcut.h = (int)floor(vid->sync.h + ((vid->bord.h - vid->sync.h) * (1.0 - brdsize))) & 0xfffc;
	vid->lcut.v = (int)floor(vid->sync.v + ((vid->bord.v - vid->sync.v) * (1.0 - brdsize))) & 0xfffc;
	vid->rcut.h = (int)floor(vid->full.h - ((1.0 - brdsize) * (vid->full.h - vid->bord.h - 256))) & 0xfffc;
	vid->rcut.v = (int)floor(vid->full.v - ((1.0 - brdsize) * (vid->full.v - vid->bord.v - 192))) & 0xfffc;
	vid->vsze.h = vid->rcut.h - vid->lcut.h;
	vid->vsze.v = vid->rcut.v - vid->lcut.v;
	vid->vBytes = vid->vsze.h * vid->vsze.h * 6;	// real size of image buffer (3 bytes/dot x2:x1)
}

int xscr = 0;
int yscr = 0;
int adr = 0;
unsigned char col = 0;
unsigned char ink = 0;
unsigned char pap = 0;
unsigned char scrbyte = 0;
unsigned char atrbyte = 0;
unsigned char nxtbyte = 0;

void vidDarkTail(Video* vid) {
	xscr = vid->x;
	yscr = vid->y;
	unsigned char* ptr = vid->scrptr;
	do {
		if ((yscr >= vid->lcut.v) && (yscr < vid->rcut.v) && (xscr >= vid->lcut.h) && (xscr < vid->rcut.h)) {
			*(ptr++) >>= 1;
			*(ptr++) >>= 1;
			*(ptr++) >>= 1;
			*(ptr++) >>= 1;
			*(ptr++) >>= 1;
			*(ptr++) >>= 1;
		}
		if (++xscr >= vid->full.h) {
			xscr = 0;
			if (++yscr >= vid->full.v)
				ptr = NULL;
		}
	} while (ptr);
}

void vidGetScreen(Video* vid, unsigned char* dst, int bank, int shift, int watr) {
	unsigned char* pixs = vid->mem->ramData + MADR(bank, shift & 0x2000);
	unsigned char* atrs = pixs + 0x1800;
	unsigned char sbyte, abyte, aink, apap;
	int prt, lin, row, xpos, bitn, cidx;
	int sadr, aadr;
	for (prt = 0; prt < 3; prt++) {
		for (lin = 0; lin < 8; lin++) {
			for (row = 0; row < 8; row++) {
				for (xpos = 0; xpos < 32; xpos++) {
					sadr = (prt << 11) | (lin << 5) | (row << 8) | xpos;
					aadr = (prt << 8) | (lin << 5) | xpos;
					sbyte = *(pixs + sadr);
					abyte = watr ? *(atrs + aadr) : 0x47;
					aink = inkTab[abyte & 0x7f];
					apap = papTab[abyte & 0x7f];
					for (bitn = 0; bitn < 8; bitn++) {
						cidx = (sbyte & (128 >> bitn)) ? aink : apap;
						*(dst++) = vid->pal[cidx].r;
						*(dst++) = vid->pal[cidx].g;
						*(dst++) = vid->pal[cidx].b;
					}
				}
			}
		}
	}
}

/*
waits for 128K, +2
	--wwwwww wwwwww-- : 16-dots cycle, start on border 8 dots before screen
waits for +2a, +3
	ww--wwww wwwwwwww : same
unsigned char waitsTab_A[16] = {5,5,4,4,3,3,2,2,1,1,0,0,0,0,6,6};	// 48K
unsigned char waitsTab_B[16] = {1,1,0,0,7,7,6,6,5,5,4,4,3,3,2,2};	// +2A,+3
*/

int contTabA[] = {12,12,10,10,8,8,6,6,4,4,2,2,0,0,0,0};		// 48K 128K +2 (bank 1,3,5,7)
int contTabB[] = {2,1,0,0,14,13,12,11,10,9,8,7,6,5,4,3};	// +2A +3 (bank 4,5,6,7)

void vidWait(Video* vid) {
	if (vid->y < vid->bord.v) return;		// above screen
	if (vid->y > (vid->bord.v + 191)) return;	// below screen
	xscr = vid->x - vid->bord.h; // + 2;
	if (xscr < 0) return;
	if (xscr > 253) return;
	vidSync(vid, contTabA[xscr & 0x0f] * NS_PER_DOT);
}

void vidSetFont(Video* vid, char* src) {
	memcpy(vid->font,src,0x800);
}

inline void vidSingleDot(Video* vid, unsigned char idx) {
	*(vid->scrptr++) = vid->pal[idx].r;
	*(vid->scrptr++) = vid->pal[idx].g;
	*(vid->scrptr++) = vid->pal[idx].b;
}

inline void vidPutDot(Video* vid, unsigned char idx) {
	vidSingleDot(vid, idx);
	vidSingleDot(vid, idx);
}

// video drawing

void vidDrawBorder(Video* vid) {
	vidPutDot(vid,vid->brdcol);
}

// common 256 x 192
void vidDrawNormal(Video* vid) {
	yscr = vid->y - vid->bord.v;
	if ((yscr < 0) || (yscr > 191)) {
		col = vid->brdcol;
		if (vid->ula->active) col |= 8;
		vid->atrbyte = 0xff;
	} else {
		xscr = vid->x - vid->bord.h;
		if ((xscr & 7) == 4) {
			nxtbyte = vid->mem->ramData[MADR(vid->curscr, scrAdrs[vid->idx])];
			//adr = ((yscr & 0xc0) << 5) | ((yscr & 7) << 8) | ((yscr & 0x38) << 2) | (((xscr + 4) & 0xf8) >> 3);
			//nxtbyte = vid->mem->ram[vid->curscr ? 7 :5].data[adr];
		}
		if ((vid->x < vid->bord.h) || (vid->x > vid->bord.h + 255)) {
			col = vid->brdcol;
			if (vid->ula->active) col |= 8;
			vid->atrbyte = 0xff;
		} else {
			if ((xscr & 7) == 0) {
				scrbyte = nxtbyte;
				vid->atrbyte = vid->mem->ramData[MADR(vid->curscr, atrAdrs[vid->idx])];
				if (vid->idx < 0x1b00) vid->idx++;
				//adr = 0x1800 | ((yscr & 0xc0) << 2) | ((yscr & 0x38) << 2) | (((xscr + 4) & 0xf8) >> 3);
				//vid->atrbyte = vid->mem->ram[vid->curscr ? 7 :5].data[adr];
				if (vid->ula->active) {
					ink = ((vid->atrbyte & 0xc0) >> 2) | (vid->atrbyte & 7);
					pap = ((vid->atrbyte & 0xc0) >> 2) | ((vid->atrbyte & 0x38) >> 3) | 8;
				} else {
					if ((vid->atrbyte & 0x80) && vid->flash) scrbyte ^= 0xff;
					ink = inkTab[vid->atrbyte & 0x7f];
					pap = papTab[vid->atrbyte & 0x7f];
				}
			}
			col = (scrbyte & 0x80) ? ink : pap;
			scrbyte <<= 1;
		}
	}
	vidPutDot(vid,col);
}

// alco 16col
void vidDrawAlco(Video* vid) {
	yscr = vid->y - vid->bord.v;
	if ((yscr < 0) || (yscr > 191)) {
		col = vid->brdcol;
	} else {
		xscr = vid->x - vid->bord.h;
		if ((xscr < 0) || (xscr > 255)) {
			col = vid->brdcol;
		} else {
			adr = ((yscr & 0xc0) << 5) | ((yscr & 7) << 8) | ((yscr & 0x38) << 2) | ((xscr & 0xf8) >> 3);
			switch (xscr & 7) {
				case 0:
					scrbyte = vid->mem->ramData[MADR(vid->curscr ^ 1, adr)];
					col = inkTab[scrbyte & 0x7f];
					break;
				case 2:
					scrbyte = vid->mem->ramData[MADR(vid->curscr, adr)];
					col = inkTab[scrbyte & 0x7f];
					break;
				case 4:
					scrbyte = vid->mem->ramData[MADR(vid->curscr ^ 1, adr + 0x2000)];
					col = inkTab[scrbyte & 0x7f];
					break;
				case 6:
					scrbyte = vid->mem->ramData[MADR(vid->curscr, adr + 0x2000)];
					col = inkTab[scrbyte & 0x7f];
					break;
				default:
					col = ((scrbyte & 0x38)>>3) | ((scrbyte & 0x80)>>4);
					break;

			}
		}
	}
	vidPutDot(vid,col);
}

// hardware multicolor
void vidDrawHwmc(Video* vid) {
	yscr = vid->y - vid->bord.v;
	if ((yscr < 0) || (yscr > 191)) {
		col = vid->brdcol;
	} else {
		xscr = vid->x - vid->bord.h;
		if ((xscr & 7) == 4) {
			adr = ((yscr & 0xc0) << 5) | ((yscr & 7) << 8) | ((yscr & 0x38) << 2) | (((xscr + 4) & 0xf8) >> 3);
			nxtbyte = vid->mem->ramData[MADR(vid->curscr, adr)];
		}
		if ((xscr < 0) || (xscr > 255)) {
			col = vid->brdcol;
		} else {
			if ((xscr & 7) == 0) {
				scrbyte = nxtbyte;
				adr = ((yscr & 0xc0) << 5) | ((yscr & 7) << 8) | ((yscr & 0x38) << 2) | ((xscr & 0xf8) >> 3);
				vid->atrbyte = vid->mem->ramData[MADR(vid->curscr, adr)];
				if ((vid->atrbyte & 0x80) && vid->flash) scrbyte ^= 0xff;
				ink = inkTab[vid->atrbyte & 0x7f];
				pap = papTab[vid->atrbyte & 0x7f];
			}
			col = (scrbyte & 0x80) ? ink : pap;
			scrbyte <<= 1;
		}
	}
	vidPutDot(vid,col);
}

// atm ega
void vidDrawATMega(Video* vid) {
	yscr = vid->y - 76;
	xscr = vid->x - 96;
	if ((yscr < 0) || (yscr > 199) || (xscr < 0) || (xscr > 319)) {
		col = vid->brdcol;
	} else {
		adr = (yscr * 40) + (xscr >> 3);
		switch (xscr & 7) {
			case 0:
				scrbyte = vid->mem->ramData[MADR(vid->curscr ^ 4, adr)];
				col = inkTab[scrbyte & 0x7f];
				break;
			case 2:
				scrbyte = vid->mem->ramData[MADR(vid->curscr, adr)];
				col = inkTab[scrbyte & 0x7f];
				break;
			case 4:
				scrbyte = vid->mem->ramData[MADR(vid->curscr ^ 4, adr + 0x2000)];
				col = inkTab[scrbyte & 0x7f];
				break;
			case 6:
				scrbyte = vid->mem->ramData[MADR(vid->curscr, adr + 0x2000)];
				col = inkTab[scrbyte & 0x7f];
				break;
			default:
				col = ((scrbyte & 0x38)>>3) | ((scrbyte & 0x80)>>4);
				break;
		}
	}
	vidPutDot(vid,col);
}

// atm text
void vidDoubleDot(Video* vid) {
	for (int i = 0x80; i > 0; i >>= 1) {
		vidSingleDot(vid, (scrbyte & i) ? ink : pap);
	}
}

void vidATMDoubleDot(Video* vid,unsigned char colr) {
	ink = inkTab[colr & 0x7f];
	pap = papTab[colr & 0x3f] | ((colr & 0x80) >> 4);
	vidDoubleDot(vid);
}

void vidDrawATMtext(Video* vid) {
	yscr = vid->y - 76;
	xscr = vid->x - 96;
	if ((yscr < 0) || (yscr > 199) || (xscr < 0) || (xscr > 319)) {
		vidPutDot(vid,vid->brdcol);
	} else {
		adr = 0x1c0 + ((yscr & 0xf8) << 3) + (xscr >> 3);
		if ((xscr & 3) == 0) {
			if ((xscr & 7) == 0) {
				scrbyte = vid->mem->ramData[MADR(vid->curscr, adr)];
				col = vid->mem->ramData[MADR(vid->curscr ^ 4, adr + 0x2000)];
			} else {
				scrbyte = vid->mem->ramData[MADR(vid->curscr, adr + 0x2000)];
				col = vid->mem->ramData[MADR(vid->curscr ^ 4, adr + 1)];
			}
			scrbyte = vid->font[(scrbyte << 3) | (yscr & 7)];
			vidATMDoubleDot(vid,col);
		}
	}
}

// atm hardware multicolor
void vidDrawATMhwmc(Video* vid) {
	yscr = vid->y - 76;
	xscr = vid->x - 96;
	if ((yscr < 0) || (yscr > 199) || (xscr < 0) || (xscr > 319)) {
		vidPutDot(vid,vid->brdcol);
	} else {
		xscr = vid->x - 96;
		yscr = vid->y - 76;
		adr = (yscr * 40) + (xscr >> 3);
		if ((xscr & 3) == 0) {
			if ((xscr & 7) == 0) {
				scrbyte = vid->mem->ramData[MADR(vid->curscr, adr)];
				col = vid->mem->ramData[MADR(vid->curscr ^ 4, adr)];
			} else {
				scrbyte = vid->mem->ramData[MADR(vid->curscr, adr + 0x2000)];
				col = vid->mem->ramData[MADR(vid->curscr ^ 4, adr + 0x2000)];
			}
			vidATMDoubleDot(vid,col);
		}
		//vid->scrptr++;
		//if (vidFlag & VF_DOUBLE) vid->scrptr++;
	}
}

// tsconf sprites & tiles

void vidTSPut(Video* vid, unsigned char* ptr) {
	int ofs = 0;
	adr = vid->tsconf.xPos - vid->lcut.h;
	// printf("%i - %i = %i\n",vid->tsconf.xPos, vid->lcut.h, adr);
	if (adr > 0) {
		ptr += adr * 6; // ((vidFlag & VF_DOUBLE) ? 6 : 3);
	}
	for (xscr = 0; xscr < vid->tsconf.xSize; xscr++) {
		if ((adr >= 0) && (adr < vid->vsze.h)) {			// visible
			col = vid->tsconf.line[ofs];
			if (col & 0x0f) {					// not transparent
				*(ptr++) = vid->pal[col].r;
				*(ptr++) = vid->pal[col].g;
				*(ptr++) = vid->pal[col].b;
//				if (vidFlag & VF_DOUBLE) {
				*(ptr++) = vid->pal[col].r;
				*(ptr++) = vid->pal[col].g;
				*(ptr++) = vid->pal[col].b;
//				}
			} else {
				ptr += 6; // ((vidFlag & VF_DOUBLE) ? 6 : 3);
			}
		}
		adr++;
		ofs++;
	}
}

int fadr;
int tile;
int sadr;	// adr in sprites dsc
int xadr;	// = pos with XFlip
unsigned char* mptr;

#include <assert.h>

void vidTSTiles(Video* vid, int lay, unsigned short yoffs, unsigned short xoffs, unsigned char gpage, unsigned char palhi) {
	int j;
	yscr = vid->y - vid->tsconf.yPos + yoffs;						// line in TMap
	adr = (vid->tsconf.TMPage << 14) | ((yscr & 0x1f8) << 5) | (lay ? 0x80 : 0x00);		// start of TMap line (full.adr)
	xscr = (0x200 - xoffs) & 0x1ff;									// pos in line buf
	xadr = vid->tsconf.tconfig & (lay ? 8 : 4);
	do {								// 64 tiles in row
		tile = vid->mem->ramData[adr] | (vid->mem->ramData[adr + 1] << 8);		// tile dsc
		adr += 2;

		if ((tile & 0xfff) || xadr) {			// !0 or (0 enabled)
			fadr = gpage << 14;
			fadr += ((tile & 0xfc0) << 5) | ((yscr & 7) << 8) | ((tile & 0x3f) << 2);	// full addr of row of this tile
			if (tile & 0x8000) fadr ^= 0x0700;						// YFlip

			col = palhi | ((tile >> 8) & 0x30);					// palette (b7..4 of color)
			if (tile & 0x4000) {							// XFlip
				xscr += 8;
				for (j = 0; j < 4; j++) {
					col &= 0xf0;
					col |= (vid->mem->ramData[fadr] & 0xf0) >> 4;				// left pixel
					xscr--;
					if (col & 0x0f) vid->tsconf.line[xscr & 0x1ff] = col;
					col &= 0xf0;
					col |= vid->mem->ramData[fadr] & 0x0f;					// right pixel
					xscr--;
					if (col & 0x0f) vid->tsconf.line[xscr & 0x1ff] = col;
					fadr++;
				}
				xscr += 8;
			} else {								// no XFlip
				for (j = 0; j < 4; j++) {
					col &= 0xf0;
					col |= (vid->mem->ramData[fadr] & 0xf0) >> 4;				// left pixel
					if (col & 0x0f) {
						vid->tsconf.line[xscr & 0x1ff] = col;
					}
					xscr++;
					col &= 0xf0;
					col |= vid->mem->ramData[fadr] & 0x0f;					// right pixel
					if (col & 0x0f) {
						vid->tsconf.line[xscr & 0x1ff] = col;
					}
					xscr++;
					fadr++;
				}
			}
		} else {
			xscr += 8;
		}
	} while (adr & 0x7f);
}

typedef struct {
	unsigned y:9;		// 0[0:7], 1:0
	unsigned ys:3;		// 1[1:3]
	unsigned res1:1;	// 1:4
	unsigned act:1;		// 1:5
	unsigned leap:1;	// 1:6
	unsigned yf:1;		// 1:7
	unsigned x:9;		// 2[0:7], 3:0
	unsigned xs:3;		// 3[1:3]
	unsigned res2:3;	// 3[4:6]
	unsigned xf:1;		// 3:7
	unsigned tnum:12;	// 4[0:7], 4[0:3]
	unsigned pal:4;		// 4[4:7]
} TSpr;

void vidTSSprites(Video* vid) {
	unsigned char* ptr;
	TSpr spr;
	while (sadr < (0x200 - 6)) {
		ptr = &vid->tsconf.sfile[sadr];
		spr.y = *ptr;
		ptr++;
		spr.y |= (*ptr & 1) << 8;
		spr.ys = (*ptr & 0x0e) >> 1;
		spr.res1 = (*ptr & 0x10) ? 1 : 0;
		spr.act = (*ptr & 0x20) ? 1 : 0;
		spr.leap = (*ptr & 0x40) ? 1 : 0;
		spr.yf = (*ptr & 0x80) ? 1 : 0;
		ptr++;
		spr.x = *ptr;
		ptr++;
		spr.x |= (*ptr & 1) << 8;
		spr.xs = (*ptr & 0x0e) >> 1;
		spr.res2 = (*ptr & 0x70) >> 4;
		spr.xf = (*ptr & 0x80) ? 1 : 0;
		ptr++;
		spr.tnum = *ptr;
		ptr++;
		spr.tnum |= (*ptr & 0x0f) << 8;
		spr.pal = (*ptr & 0xf0) >> 4;
		if (spr.act) {
			adr = spr.y;
			xscr = (spr.ys + 1) << 3;		// Ysize - 000:8; 001:16; 010:24; ...
			yscr = vid->y - vid->tsconf.yPos;	// line on screen
			if (((yscr - adr) & 0x1ff) < xscr) {	// if sprite visible on current line
				yscr -= adr;			// line inside sprite;
				if (spr.yf) yscr = xscr - yscr - 1;	// YFlip (Ysize - norm.pos - 1)
				tile = spr.tnum + ((yscr & 0x1f8) << 3);	// shift to current tile line

				fadr = vid->tsconf.SGPage << 14;
				fadr += ((tile & 0xfc0) << 5) | ((yscr & 7) << 8) | ((tile & 0x3f) << 2);	// fadr = adr of pix line to put in buf

				col = spr.pal << 4;
				xadr = (spr.xs + 1) << 3;	// xsoze
				adr = spr.x;			// xpos
				if (spr.xf) adr += xadr - 1;	// xpos of right pixel (xflip)
				for (xscr = xadr; xscr > 0; xscr -= 2) {
					col &= 0xf0;
					col |= ((vid->mem->ramData[fadr] & 0xf0) >> 4);		// left pixel;
					if (col & 0x0f) vid->tsconf.line[adr & 0x1ff] = col;
					if (spr.xf) adr--; else adr++;
					col &= 0xf0;
					col |= (vid->mem->ramData[fadr] & 0x0f);		// right pixel
					if (col & 0x0f) vid->tsconf.line[adr & 0x1ff] = col;
					if (spr.xf) adr--; else adr++;
					fadr++;
				}
			}
		}
		sadr += 6;
		if (spr.leap) break;		// LEAP
	}
}

void vidTSRender(Video* vid, unsigned char* ptr) {
	if (vid->y < vid->tsconf.yPos) return;
	if (vid->y >= (vid->tsconf.yPos + vid->tsconf.ySize)) return;
// prepare layers in VID->LINE & out visible part to PTR
	sadr = 0x000;					// adr inside SFILE
	memset(vid->tsconf.line,0x00,0x200);		// clear tile-sprite line
// S0 ?
	if (vid->tsconf.tconfig & 0x80) {
		vidTSSprites(vid);
	}
// T0
	if (vid->tsconf.tconfig & 0x20) {
		vidTSTiles(vid,0,vid->tsconf.T0YOffset,vid->tsconf.T0XOffset,vid->tsconf.T0GPage,vid->tsconf.T0Pal76);
	}
// S1
	if (vid->tsconf.tconfig & 0x80) {
		vidTSSprites(vid);
	}
// T1
	if (vid->tsconf.tconfig & 0x40) {
		vidTSTiles(vid,1,vid->tsconf.T1YOffset,vid->tsconf.T1XOffset,vid->tsconf.T1GPage,vid->tsconf.T1Pal76);
	}
// S2
	if (vid->tsconf.tconfig & 0x80) {
		vidTSSprites(vid);
	}
	if (vid->tsconf.tconfig & 0xe0) {
		vidTSPut(vid,ptr);		// if tile or sprites visible, put line over screen
//		vid->change = 1;
	}
	vid->tsconf.scrLine++;
}

// tsconf normal screen (separated 'cuz of palette)

void vidDrawTSLNormal(Video* vid) {
	xscr = vid->x - vid->bord.h;
	yscr = vid->y - vid->bord.v;
	if ((yscr < 0) || (yscr > 191) || (vid->nogfx)) {
		col = vid->brdcol;
	} else {
		xadr = vid->tsconf.vidPage ^ (vid->curscr & 2);	// TODO : ORLY? Current video page
		if ((xscr & 7) == 4) {
			adr = ((yscr & 0xc0) << 5) | ((yscr & 7) << 8) | ((yscr & 0x38) << 2) | (((xscr + 4) & 0xf8) >> 3);
			nxtbyte = vid->mem->ramData[MADR(xadr, adr)];
		}
		if ((xscr < 0) || (xscr > 255)) {
			col = vid->brdcol;
		} else {
			if ((xscr & 7) == 0) {
				scrbyte = nxtbyte;
				adr = 0x1800 | ((yscr & 0xc0) << 2) | ((yscr & 0x38) << 2) | (((xscr + 4) & 0xf8) >> 3);
				vid->atrbyte = vid->mem->ramData[MADR(xadr, adr)];
				if ((vid->atrbyte & 0x80) && vid->flash) scrbyte ^= 0xff;
				ink = inkTab[vid->atrbyte & 0x7f];
				pap = papTab[vid->atrbyte & 0x7f];
			}
			col = vid->tsconf.scrPal | ((scrbyte & 0x80) ? ink : pap);
			scrbyte <<= 1;
		}
	}
	vidPutDot(vid,col);
//	*(vid->scrptr++) =  col;
//	if (vidFlag & VF_DOUBLE) *(vid->scrptr++) = col;
}

// tsconf 4bpp

void vidDrawTSL16(Video* vid) {
	xscr = vid->x - vid->tsconf.xPos;
	yscr = vid->y - vid->tsconf.yPos;
	if ((xscr < 0) || (xscr >= vid->tsconf.xSize) || (yscr < 0) || (yscr >= vid->tsconf.ySize) || vid->nogfx) {
		col = vid->brdcol;
	} else {
		xscr += vid->tsconf.xOffset;
		yscr = vid->tsconf.scrLine;
		xscr &= 0x1ff;
		yscr &= 0x1ff;
		adr = ((vid->tsconf.vidPage & 0xf8) << 14) + (yscr << 8) + (xscr >> 1);
		scrbyte = vid->mem->ramData[adr];
		if (xscr & 1) {
			col = scrbyte & 0x0f;			// right pixel
		} else {
			col = (scrbyte & 0xf0) >> 4 ;		// left pixel
		}
		col |= vid->tsconf.scrPal;
	}
	vidPutDot(vid,col);
//	*(vid->scrptr++) = col;
//	if (vidFlag & VF_DOUBLE) *(vid->scrptr++) = col;
}

// tsconf 8bpp

void vidDrawTSL256(Video* vid) {
	xscr = vid->x - vid->tsconf.xPos;
	yscr = vid->y - vid->tsconf.yPos;
	if ((xscr < 0) || (xscr >= vid->tsconf.xSize) || (yscr < 0) || (yscr >= vid->tsconf.ySize) || vid->nogfx) {
		col = vid->brdcol;
	} else {
		xscr += vid->tsconf.xOffset;
		yscr = vid->tsconf.scrLine;
		xscr &= 0x1ff;
		yscr &= 0x1ff;
		adr = ((vid->tsconf.vidPage & 0xf0) << 14) + (yscr << 9) + xscr;
		col = vid->mem->ramData[adr];
	}
	vidPutDot(vid,col);
//	*(vid->scrptr++) = col;
//	if (vidFlag & VF_DOUBLE) *(vid->scrptr++) = col;
}

// tsconf text

void vidDrawTSLText(Video* vid) {
	xscr = vid->x - vid->tsconf.xPos;
	yscr = vid->y - vid->tsconf.yPos;
	if ((xscr < 0) || (xscr >= vid->tsconf.xSize) || (yscr < 0) || (yscr >= vid->tsconf.ySize)) {
		vidPutDot(vid, vid->brdcol);
	} else {
		if ((xscr & 3) == 0) {
			xscr += vid->tsconf.xOffset;
			yscr += vid->tsconf.yOffset;
			xscr &= 0x1ff;
			yscr &= 0x1ff;
			adr = (vid->tsconf.vidPage << 14) + ((yscr & 0x1f8) << 5) + (xscr >> 2);	// 256 bytes in row
			scrbyte = vid->mem->ramData[adr];
			col = vid->mem->ramData[adr | 0x80];
			ink = (col & 0x0f) | (vid->tsconf.scrPal);
			pap = ((col & 0xf0) >> 4)  | (vid->tsconf.scrPal);
			scrbyte = vid->mem->ramData[MADR(vid->tsconf.vidPage ^ 1, (scrbyte << 3) | (yscr & 7))];
			// scrbyte = vid->font[(scrbyte << 3) | (yscr & 7)];
			vidDoubleDot(vid);
		}
		//vid->scrptr++;
		//if (vidFlag & VF_DOUBLE) vid->scrptr++;
	}
}

// baseconf text

void vidDrawEvoText(Video* vid) {
	yscr = vid->y - 76;
	xscr = vid->x - 96;
	if ((yscr < 0) || (yscr > 199) || (xscr < 0) || (xscr > 319)) {
		vidPutDot(vid,vid->brdcol);
	} else {
		if ((xscr & 3) == 0) {
			adr = 0x1c0 + ((yscr & 0xf8) << 3) + (xscr >> 3);
			if ((xscr & 7) == 0) {
				scrbyte = vid->mem->ramData[MADR(vid->curscr + 3, adr)];
				col = vid->mem->ramData[MADR(vid->curscr + 3, adr + 0x3000)];
			} else {
				scrbyte = vid->mem->ramData[MADR(vid->curscr + 3, adr + 0x1000)];
				col = vid->mem->ramData[MADR(vid->curscr + 3, adr + 0x2001)];
			}
			scrbyte = vid->font[(scrbyte << 3) | (yscr & 7)];
			vidATMDoubleDot(vid,col);
		}
		// vid->scrptr++;
		// if (vidFlag & VF_DOUBLE) vid->scrptr++;
	}
}

// profi 512x240

void vidProfiScr(Video* vid) {
	yscr = vid->y - vid->bord.v + 24;
	if ((yscr < 0) || (yscr > 239)) {
		vidPutDot(vid, vid->brdcol);
	} else {
		xscr = vid->x - vid->bord.h;
		if ((xscr < 0) || (xscr > 255)) {
			vidPutDot(vid, vid->brdcol);
		} else {
			if ((xscr & 3) == 0) {
				adr = scrAdrs[vid->idx & 0x1fff] & 0x1fff;
				if (xscr & 4) {
					vid->idx++;
				} else {
					adr |= 0x2000;
				}
				if (vid->curscr == 7) {
					scrbyte = vid->mem->ramData[MADR(6, adr)];
					col = vid->mem->ramData[MADR(0x3a, adr)];		// b0..2 ink, b3..5 pap, b6 inkBR, b7 papBR
				} else {
					scrbyte = vid->mem->ramData[MADR(4, adr)];
					col = vid->mem->ramData[MADR(0x38, adr)];
				}
				ink = inkTab[col & 0x47];
				pap = papTab[(col & 0x3f) | ((col >> 1) & 0x40)];
				vidDoubleDot(vid);
			}
		}
	}
}

// fill MSX foreground sprites image

#define PUTDADOT(ad) if (!vid->v9918.sprImg[ad]) vid->v9918.sprImg[ad] = atr;

void msxPutTile(Video* vid, int xpos, int ypos, int pat, int atr) {
	unsigned char src;
	int i,j,adr,xsav;
	for (i = 0; i < 8; i++) {
		src = vid->v9918.ram[vid->v9918.OBJTiles + (pat << 3) + i];		// tile byte
		if ((ypos >= 0) && (ypos < 192)) {					// if line onscreen
			xsav = xpos;
			for (j = 0; j < 8; j++) {
				if ((xpos >= 0) && (xpos < 256) && (src & 0x80)) {	// if sprite has dot onscreen
						adr = (ypos << 8) + xpos;
						PUTDADOT(adr);
						if (vid->v9918.reg[1] & 1) {
							PUTDADOT(adr+1);
							PUTDADOT(adr+256);
							PUTDADOT(adr+257);
							xpos++;
						}
				}
				src <<= 1;
				xpos++;
			}
			xpos = xsav;
		}
		ypos += (vid->v9918.reg[1] & 1) ? 2 : 1;
	}
}

void msxFillSprites(Video* vid) {
	memset(vid->v9918.sprImg, 0x00, 0xc000);
	int i,sh;
	int adr = vid->v9918.OBJAttr;
	int ypos, xpos, pat, atr;
	for (i = 0; i < 32; i++) {
		if (vid->v9918.ram[adr] == 0xd0) break;
		ypos = (vid->v9918.ram[adr] + 1) & 0xff;
		xpos = vid->v9918.ram[adr+1] & 0xff;
		pat = vid->v9918.ram[adr+2] & 0xff;
		atr = vid->v9918.ram[adr+3] & 0xff;
		if (atr & 0x80)			// early clock
			xpos -= 32;
		atr &= 0x0f;
		if (vid->v9918.reg[1] & 2) {
			pat &= 0xfc;
			sh = (vid->v9918.reg[1] & 1) ? 16 : 8;
			msxPutTile(vid, xpos, ypos, pat, atr);
			msxPutTile(vid, xpos, ypos+sh, pat+1, atr);
			msxPutTile(vid, xpos+sh, ypos, pat+2, atr);
			msxPutTile(vid, xpos+sh, ypos+sh, pat+3, atr);
		} else {
			msxPutTile(vid, xpos, ypos, pat, atr);
		}
		adr += 4;
	}
}

// v9918 scr 0 (40 x 24 text)

void vidMsxScr0(Video* vid) {
	yscr = vid->y - vid->bord.v;
	if ((yscr < 0) || (yscr > 191)) {
		col = vid->brdcol;
	} else {
		xscr = vid->x - vid->bord.h;
		if ((xscr < 0) || (xscr > 239)) {
			col = vid->brdcol;
		} else {
			if ((xscr % 6) == 0) {
				adr = vid->v9918.ram[((yscr & 0xf8) * 5) + (xscr / 6)];
				scrbyte = vid->v9918.ram[0x800 + ((adr << 3) | (yscr & 7))];
				ink = (vid->v9918.reg[7] & 0xf0) >> 4;
				pap = vid->v9918.reg[7] & 0x0f;
			}
			col = (scrbyte & 0x80) ? ink : pap;
			scrbyte <<= 1;
		}
	}
	vidPutDot(vid, col);
}



// v9918 scr 1 (32 x 24 text)

void vidMsxScr1(Video* vid) {
	yscr = vid->y - vid->bord.v;
	if ((yscr < 0) || (yscr > 191)) {
		col = vid->brdcol;
	} else {
		xscr = vid->x - vid->bord.h;
		if ((xscr < 0) || (xscr > 255)) {
			col = vid->brdcol;
		} else {
			if ((xscr & 7) == 0) {
				adr = vid->v9918.ram[vid->v9918.BGMap + (xscr >> 3) + ((yscr & 0xf8) << 2)];
				scrbyte = vid->v9918.ram[vid->v9918.BGTiles + (adr << 3) + (yscr & 7)];
				atrbyte = vid->v9918.ram[vid->v9918.BGColors + (adr >> 3)];
				ink = (atrbyte & 0xf0) >> 4;
				pap = atrbyte & 0x0f;
			}
			col = vid->v9918.sprImg[(yscr << 8) | xscr];
			if (col == 0) {
				col = (scrbyte & 0x80) ? ink : pap;
			}
			scrbyte <<= 1;
		}
	}
	vidPutDot(vid, col);
}

// v9918 scr 2 (256 x 192)

void vidMsxScr2(Video* vid) {
	yscr = vid->y - vid->bord.v;
	if ((yscr < 0) || (yscr > 191)) {
		col = vid->brdcol;
	} else {
		xscr = vid->x - vid->bord.h;
		if ((xscr < 0) || (xscr > 255)) {
			col = vid->brdcol;
		} else {
			if ((xscr & 7) == 0) {
				adr = vid->v9918.ram[vid->v9918.BGMap + (xscr >> 3) + ((yscr & 0xf8) << 2)] | ((yscr & 0xc0) << 2);	// tile nr
				scrbyte = vid->v9918.ram[(vid->v9918.BGTiles & ~0x1fff) + (adr << 3) + (yscr & 7)];
				atrbyte = vid->v9918.ram[(vid->v9918.BGColors & ~0x1fff) + (adr << 3) + (yscr & 7)];
				ink = (atrbyte & 0xf0) >> 4;
				pap = atrbyte & 0x0f;
			}
			col = vid->v9918.sprImg[(yscr << 8) | xscr];
			if (col == 0) {
				col = (scrbyte & 0x80) ? ink : pap;
			}
			scrbyte <<= 1;
		}
	}
	vidPutDot(vid, col);
}

// v9918 scr 3 (multicolor)

void vidMsxScr3(Video* vid) {
	yscr = vid->y - vid->bord.v;
	if ((yscr < 0) || (yscr > 191)) {
		col = vid->brdcol;
	} else {
		xscr = vid->x - vid->bord.h;
		if ((xscr < 0) || (xscr > 255)) {
			col = vid->brdcol;
		} else {
			adr = vid->v9918.ram[vid->v9918.BGMap + (xscr >> 3) + ((yscr & 0xf8) << 2)];		// color index
			adr = vid->v9918.BGTiles + (adr << 3) + ((yscr & 0x18) >> 2) + ((yscr & 4) >> 2);	// color adr
			col = vid->v9918.ram[adr];								// color for 2 dots
			if (!(xscr & 4)) {
				col >>= 4;
			}
			col &= 0x0f;
		}
	}
	vidPutDot(vid, col);
}

// weiter

typedef struct {
	int id;
	void(*callback)(Video*);
} xVideoMode;

xVideoMode vidModeTab[] = {
	{VID_NORMAL, vidDrawNormal},
	{VID_ALCO, vidDrawAlco},
	{VID_HWMC, vidDrawHwmc},
	{VID_ATM_EGA, vidDrawATMega},
	{VID_ATM_TEXT, vidDrawATMtext},
	{VID_ATM_HWM, vidDrawATMhwmc},
	{VID_EVO_TEXT, vidDrawEvoText},
	{VID_TSL_NORMAL, vidDrawTSLNormal},
	{VID_TSL_16, vidDrawTSL16},
	{VID_TSL_256, vidDrawTSL256},
	{VID_TSL_TEXT, vidDrawTSLText},
	{VID_PRF_MC, vidProfiScr},
	{VID_MSX_SCR0, vidMsxScr0},
	{VID_MSX_SCR1, vidMsxScr1},
	{VID_MSX_SCR2, vidMsxScr2},
	{VID_MSX_SCR3, vidMsxScr3},
	{VID_UNKNOWN, vidDrawBorder}
};

void vidSetMode(Video* vid, int mode) {
	if (mode == VID_CURRENT) {
		mode = vid->vmode;
	} else {
		vid->vmode = mode;
	}
	if (vid->noScreen) {
		vid->callback = &vidDrawBorder;
	} else {
		int i = 0;
		do {
			if ((vidModeTab[i].id == VID_UNKNOWN) || (vidModeTab[i].id == mode)) {
				vid->callback = vidModeTab[i].callback;
				break;
			}
			i++;
		} while (1);
	}
}

void vidSync(Video* vid, int ns) {
	vid->nsDraw += ns;
	while (vid->nsDraw >= NS_PER_DOT) {
		if ((vid->y >= vid->lcut.v) && (vid->y < vid->rcut.v)) {
			if ((vid->x >= vid->lcut.h) && (vid->x < vid->rcut.h)) {
				if (vid->x & 8) vid->brdcol = vid->nextbrd;
				vid->callback(vid);		// put dot
			}
		}
		if ((vid->intMask & 1) && (vid->y == vid->intpos.v) && (vid->x == vid->intpos.h))
			vid->intFRAME = 1;
			vid->v9918.sr0 |= 0x80;
		if (vid->intFRAME && (vid->x >= vid->intpos.h + vid->intSize))
			vid->intFRAME = 0;
		if (++vid->x >= vid->full.h) {
			vid->x = 0;
			vid->nextrow = 1;
			vid->intFRAME = 0;
			if (vid->istsconf) {
				vidTSRender(vid, vid->linptr);
				vid->linptr = vid->scrptr;
				if (vid->intMask & 0x02) {
						vid->intLINE = 1;
				}
			}
			if (++vid->y >= vid->full.v) {
				vid->y = 0;
				vid->scrptr = vid->scrimg;
				vid->linptr = vid->scrimg;
				vid->fcnt++;
				vid->flash = vid->fcnt & 0x20;
				vid->tsconf.scrLine = vid->tsconf.yOffset;
				vid->idx = 0;
				vid->newFrame = 1;
				if (vid->debug) vidDarkTail(vid);
				if (vid->ismsx) {
					msxFillSprites(vid);
				}
			}
		}
		vid->nsDraw -= NS_PER_DOT;
	}
}
