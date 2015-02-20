//  MF.cpp: MF/TIN compiler
//
//  Copyright (C) 2015 Ulrich Hecht
//  Modeled after MF/TIN 1.4.3, Copyright (C) 2003-2004 Jeff Massung
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
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>
#ifdef __WIN32__
#include <windows.h>
#endif
#include <FreeImage.h>

#include "os.h"

#include "runtime_syms.h"

bool option_debug = false;

#ifndef NDEBUG
#define DEBUG(x ...) do { if (option_debug) fprintf(stderr, "DEBUG " x); \
} while (0)
#else
#define DEBUG(x ...)
#endif

class Icode {
public:
	Icode();
	Icode(const char *word);
	Icode *appendNew(const char *word);

	const char *word;
	char *code;
	char *cp;
	int len;
	Icode *next;
};

Icode::Icode()
{
	word = NULL;
	cp = code = NULL;
	len = 0;
	next = NULL;
}

Icode::Icode(const char *word)
{
	this->word = strdup(word);
	cp = code = new char[1 << 16];
	len = 0;
	next = NULL;
}

Icode *Icode::appendNew(const char *word)
{
	Icode *icode = this;

	while (icode->next)
		icode = icode->next;
	icode->next = new Icode(word);
	return icode->next;
}

class Symbol {
public:
	Symbol();
	Symbol(unsigned int addr, const char *word);
	~Symbol();
	Symbol *appendNew(unsigned int addr, const char *word);

	const char *word;
	unsigned int addr;
	Symbol *next;
	bool has_prolog;
	bool is_addr;
	unsigned int lit_addr;
};

Symbol::Symbol(unsigned int addr, const char *word)
{
	this->addr = addr;
	this->word = strdup(word);
	next = NULL;
	has_prolog = false;
	is_addr = false;
	lit_addr = 0;
}

Symbol::Symbol()
{
	addr = 0;
	next = NULL;
	word = NULL;
	has_prolog = false;
	is_addr = false;
	lit_addr = 0;
}

Symbol::~Symbol()
{
	if (word)
		delete word;
}

Symbol *Symbol::appendNew(unsigned int addr, const char *word)
{
	Symbol *sym = this;

	while (sym->next)
		sym = sym->next;
	sym->next = new Symbol(addr, word);
	return sym->next;
}

class Literal;
class Output;

class Output {
public:
	Output(unsigned int code_addr, unsigned int var_addr);
	void setTitle(const char *title);
	void setEntry(unsigned int addr);
	void emitByte(const unsigned char byte);
	void emitDword(const unsigned int dword);
	void emitString(const char *str, int len);
	void emitBitmap(const char *bmp);
	void emitPalette(const char *bmp);
	void patch32(unsigned int addr, unsigned int val);
	void reloc12(Literal *lit);
	void emitSound(const char *wav);
	void emitMusic(const char *mod);
	void fixCartHeader();
	void openOutFile(const char *name);
	void closeOutFile(void);

	FILE *fp;
	unsigned int addr;
	unsigned int vaddr;
	bool use_pimp;
};

class Literal {
public:
	Literal();
	Literal(unsigned int val, unsigned int reloc);
	Literal *prependNew(unsigned int val, unsigned int reloc);
	void code(Output *out);

	unsigned int val;
	unsigned int reloc;
	unsigned int coded_at;
	Literal *next;
};

Literal::Literal()
{
	val = reloc = coded_at = 0;
	next = NULL;
}

Literal::Literal(unsigned int val, unsigned int reloc)
{
	this->val = val;
	this->reloc = reloc;
	coded_at = 0;
	next = NULL;
}

// XXX: prepending literals is crap because it increases the chance of
// exceeding the maximum displacement by putting the literals used by
// the first instruction last.
Literal *Literal::prependNew(unsigned int val, unsigned int reloc)
{
	Literal *lit = new Literal(val, reloc);

	lit->next = next;
	next = lit;
	return lit;
}

void Literal::code(Output *out)
{
	Literal *lit = next;
	while (lit) {
		// Check if this value has already been encoded earlier.
		bool already_coded = false;
		Literal* lit2 = next;
		// XXX: So this is quadratic. Sue me.
		while (lit2 && lit2 != lit) {
			if (lit2->coded_at && lit2->val == lit->val) {
				already_coded = true;
				break;
			}
			lit2 = lit2->next;
		}

		if (already_coded) {
			DEBUG("literal for 0x%x already coded at 0x%x\n",
			      lit->reloc, lit2->coded_at);
			// Overwrite earlier literal's relocatee with
			// current one's.  This doesn't cause problems
			// because we have already processed lit2.
			lit2->reloc = lit->reloc;
			out->reloc12(lit2);
		} else {
			out->reloc12(lit);
			lit->coded_at = out->addr;
			out->emitDword(lit->val);
		}

		lit = lit->next;
	}

	while (next) {
		lit = next->next;
		delete next;
		next = lit;
	}
}

class Parser {
public:
	Parser();
	void setOutput(Output *out);
	const char *getNextWord();
	const char *peekWordN(int n);
	bool isWordN(int n, const char *word);
	const char *peekNextWord();
	bool getNextWordIf(const char *word);
	void pushText(const char *filename);
	void popText(void);
	void code(unsigned int insn);
	Symbol *getSymbol(const char *word);
	Icode *getIcode(const char *word);
	void parseAsm(const char *word);
	void parseAll();
	void codeAsm(const char *w0, const char *w1 = NULL,
		     const char *w2 = NULL, const char *w3 = NULL,
		     const char *w4 = NULL, const char *w5 = NULL,
		     const char *w6 = NULL);
	void codeAsm(unsigned int w0, const char *w1 = NULL,
		     const char *w2 = NULL, const char *w3 = NULL,
		     const char *w4 = NULL);
	void codeAsm(const char *w0, unsigned int w1,
		     const char *w2 = NULL, const char *w3 = NULL,
		     const char *w4 = NULL);
	void codeBranch(unsigned int dest, const char *mnem);
	void codeCallThumb(unsigned int dest);

private:
	// These numbers reflect the instruction groups as defined in the
	// GBATEK document, which in turn derives them from the chapter
	// numbers in the ARM.
	unsigned int arm5Code3Ops();
	unsigned int arm5Code2Ops(unsigned int regbit);
	unsigned int arm5CodeS();
	unsigned int armCodeCond();
	unsigned int arm5CodeOp2();

	unsigned int arm9CodeAddr();
	unsigned int arm10CodeAddr();

public:
	Output *out;
	char *text;
	char *tptr;
	char *stack[128][2];
	int sp;
	const char *title;
	Icode icodes;
	Icode *cur_icode;
	Symbol symbols;
	Literal literals;
	bool asm_mode;
	char *prev_tptr;
	struct {
		const char *	label;
		unsigned int	addr;
	} asm_labels[32];
	int lsp;
	unsigned int asm_stack[32][2];
	int asp;
	unsigned int loop_stack[32];
	int lpsp;
};

#define GLB_error(x ...) do { fprintf(stderr, x); exit(1); } while (0)

void Parser::setOutput(Output *out)
{
	this->out = out;
}

const char *Parser::getNextWord()
{
	static int text_mode = 0;
	static char buf[256];
	int idx = 0;

	prev_tptr = tptr;

	if (text_mode) {
		while (*tptr && *tptr != '"')
			buf[idx++] = *tptr++;
		tptr++;
	} else {
		while (*tptr && !isspace(*tptr))
			buf[idx++] = tolower(*tptr++);
	}

	while (isspace(*tptr))
		tptr++;

	if (idx == 0)
		return NULL;

	if (buf[idx - 1] == '"')
		text_mode = 1;
	else
		text_mode = 0;
	buf[idx] = 0;

	return buf;
}

const char *Parser::peekWordN(int n)
{
	char *save_tptr = tptr;
	const char *pw;

	do {
		pw = getNextWord();
		n--;
	} while (n >= 0);
	tptr = save_tptr;
	return pw;
}

bool Parser::isWordN(int n, const char *word)
{
	return !strcmp(word, peekWordN(n));
}

const char *Parser::peekNextWord()
{
	return peekWordN(0);
}

bool Parser::getNextWordIf(const char *word)
{
	if (!strcmp(peekNextWord(), word)) {
		getNextWord();
		return true;
	}
	return false;
}

void Parser::pushText(const char *filename)
{
	char buf[256];

	if (text || tptr) {
		stack[sp][0] = text;
		stack[sp++][1] = tptr;
	}
	text = (char *)malloc(1 << 20);
	tptr = text;
	FILE *in = fopen(filename, "rt");
	if (!in) {
		in = fopen(stolower(filename), "rt");
		if (!in) {
			char appdir[256];
			OS_getAppDir(appdir);
			sprintf(buf, "%s" PATHSEP "optional" PATHSEP "%s",
				appdir, filename);
			in = fopen(buf, "rt");
			if (!in) {
				sprintf(buf,
					"%s" PATHSEP "optional" PATHSEP "%s",
					appdir, stolower(filename));
				in = fopen(buf, "rt");
				if (!in)
					GLB_error(
						"unable to open input file %s\n",
						filename);
			}
		}
	}
	int len = fread(text, 1, 1 << 20, in);
	text[len] = 0;
	fclose(in);
}

void Parser::popText()
{
	free(text);
	text = stack[--sp][0];
	tptr = stack[sp][1];
}

void Output::setTitle(const char *title)
{
	long cur = ftell(fp);

	fseek(fp, 0xa0, SEEK_SET);
#ifdef BUG_FOR_BUG
#define MAX_TITLE 11
#else
#define MAX_TITLE 12
#endif
	fwrite(title, 1,
	       strlen(title) > MAX_TITLE ? MAX_TITLE : strlen(title),
	       fp);
	fseek(fp, cur, SEEK_SET);
}

void Output::setEntry(unsigned int addr)
{
	long cur = ftell(fp);

	fseek(fp, 0xfc, SEEK_SET);
	fwrite(&addr, 1, 4, fp);
	fseek(fp, cur, SEEK_SET);
}

void Output::fixCartHeader()
{
	unsigned char bytes[0x1c];
	unsigned char check_byte = 0;
	long saved_pos = ftell(fp);

#ifdef BUG_FOR_BUG
	/* MF.EXE puts the Maker Code and Game Code one byte too early. */
	const unsigned char broken_hdr[8] = {
		'A', 'D', 'B', 'E', '0', '0', 0x96, 0
	};
	fseek(fp, 0xab, SEEK_SET);
	fwrite(broken_hdr, 1, 8, fp);
#endif

	fseek(fp, 0xa0, SEEK_SET);
	fread(bytes, 1, 0x1c, fp);

	for (int i = 0; i < 0x1c; i++)
		check_byte -= bytes[i];
	check_byte -= 0x19;

	fseek(fp, 0xbd, SEEK_SET);
	fwrite(&check_byte, 1, 1, fp);

	fseek(fp, saved_pos, SEEK_SET);
}

unsigned int TIN_parseNum(const char *word)
{
	if (word[0] == '$')
		return strtoul(&word[1], NULL, 16);
	else if (word[0] == '-' && word[1] == '$')
		return -strtoul(&word[2], NULL, 16);
	else if (isdigit(word[0]) ||
		 (word[0] == '-' && isdigit(word[1]))
		 )
		return strtoul(word, NULL, 10);
	else
		GLB_error("unknown number format %s\n", word);
}

bool isNum(const char *word)
{
	return isdigit(word[0]) ||
	       word[0] == '$' ||
	       (word[0] == '-' && (isdigit(word[1]) || word[1] == '$'));
}

void Output::emitByte(const unsigned char byte)
{
	fputc(byte, fp);
	addr++;
}

void Output::emitDword(const unsigned int dword)
{
	fputc(dword & 0xff, fp);
	fputc((dword >> 8) & 0xff, fp);
	fputc((dword >> 16) & 0xff, fp);
	fputc((dword >> 24), fp);
	addr += 4;
}

void Output::emitString(const char *str, int len)
{
	fwrite(str, 1, len, fp);
	addr += len;
}

#ifdef BUG_FOR_BUG
const unsigned short rgb888to555_table[256] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x09, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
	0x0a, 0x0a, 0x0a, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
	0x0c, 0x0c, 0x0c, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0d, 0x0d, 0x0d, 0x0e, 0x0e, 0x0e, 0x0e,
	0x0e, 0x0e, 0x0e, 0x0e, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x10, 0x10, 0x11, 0x11, 0x11, 0x11,
	0x11, 0x11, 0x11, 0x11, 0x11, 0x12, 0x12, 0x12,
	0x12, 0x12, 0x12, 0x12, 0x12, 0x13, 0x13, 0x13,
	0x13, 0x13, 0x13, 0x13, 0x13, 0x14, 0x14, 0x14,
	0x14, 0x14, 0x14, 0x14, 0x14, 0x15, 0x15, 0x15,
	0x15, 0x15, 0x15, 0x15, 0x15, 0x16, 0x16, 0x16,
	0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x17, 0x17,
	0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x18, 0x18,
	0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x19, 0x19,
	0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x1a, 0x1a,
	0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1b,
	0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1c,
	0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1d,
	0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1e,
	0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1f,
};

#define RGB8TO5(c) (rgb888to555_table[c])
#else
#define RGB8TO5(c) ((c) >> 3)
#endif

static unsigned short bgra2gba(unsigned const char *bgra)
{
	unsigned int r, g, b;
	unsigned short gbapal;

	b = *bgra++;
	gbapal = RGB8TO5(b) << 10;
	g = *bgra++;
	gbapal |= RGB8TO5(g) << 5;
	r = *bgra++;
	gbapal |= RGB8TO5(r);

	return gbapal;
}

void Output::emitBitmap(const char *bmp)
{
	FREE_IMAGE_FORMAT fif = FreeImage_GetFileType(bmp, 0);

	if (fif == FIF_UNKNOWN)
		GLB_error("unknown image format in %s\n", bmp);
	FIBITMAP *img = FreeImage_Load(fif, bmp, 0);
	if (!img)
		GLB_error("unable to load bitmap %s\n", bmp);

	int width = FreeImage_GetWidth(img);
	int height = FreeImage_GetHeight(img);
	int bpp = FreeImage_GetBPP(img);
	int bypp = FreeImage_GetLine(img) / FreeImage_GetWidth(img);
	DEBUG("BMP height %d width %d bpp %d\n", height, width, bpp);

	int x = 0;
	int y = 0;
	const unsigned char *p;
	if (bpp == 24) {
		/* produce RGB555 bitmap */
		char line[width * 2];
		for (y = 0; y < height; y++) {
			p = FreeImage_GetScanLine(img, height - y - 1);
			for (x = 0; x < width; x++) {
				/* XXX: endianness! */
				line[x * 2] = bgra2gba(p) & 0xff;
				line[x * 2 + 1] = bgra2gba(p) >> 8;
				p += bypp;
			}
			emitString(line, width * 2);
		}
	} else {
		/* produce tiled indexed bitmap */
		char line[bpp];
		for (;;) {
			int i;
			p = FreeImage_GetScanLine(img, height - y - 1);
			if (bpp == 4) {
				p += x / 2;
				for (i = 0; i < 4; i++)
					line[i] = (p[i] >> 4) |
						  ((p[i] & 0xf) << 4);
			} else if (bpp == 8) {
				p += x;
				for (i = 0; i < 8; i++)
					line[i] = p[i];
			}
			emitString(line, bpp);
			y++;
			if (y % 8 == 0) {
				/* continue with tile to the right */
				y -= 8;
				x += 8;
			}
			if (x >= width) {
				/* continue with next line of tiles */
				x = 0;
				y += 8;
				if (y >= height)
					break;
			}
		}
	}
	FreeImage_Unload(img);
}

void Output::emitPalette(const char *bmp)
{
	int i;
	unsigned short gbapal;
	FREE_IMAGE_FORMAT fif = FreeImage_GetFileType(bmp, 0);

	if (fif == FIF_UNKNOWN) {
		/* Is it a RIFF palette file? */
		FILE *fp = fopen(bmp, "rb");
		if (!fp)
			GLB_error("cannot open %s\n", bmp);
		unsigned char hdr[0x18];
		fread(hdr, 1, 0x18, fp);
		if (!strncmp((char *)hdr, "RIFF", 4) &&
		    !strncmp((char *)hdr + 8, "PAL ", 4)) {
			int entries = ((hdr[0x10]) +
				       (hdr[0x11] << 8) +
				       (hdr[0x12] << 16) +
				       (hdr[0x13] << 24)) / 4 - 2;
			unsigned char pp_rgba[4];
			unsigned char pp_bgr[3];
			for (i = 0; i < entries; i++) {
				fread(pp_rgba, 1, 4, fp);
				/* OF COURSE it has to be different... */
				pp_bgr[0] = pp_rgba[2];
				pp_bgr[1] = pp_rgba[1];
				pp_bgr[2] = pp_rgba[0];
				gbapal = bgra2gba(pp_bgr);
				emitString((char *)&gbapal, 2);
			}
			fclose(fp);
			return;
		} else {
			fclose(fp);
			GLB_error("unknown image format in %s\n", bmp);
		}
	}
	FIBITMAP *img = FreeImage_Load(fif, bmp, 0);
	if (!img)
		GLB_error("unable to load bitmap %s\n", bmp);
	int bpp = FreeImage_GetBPP(img);
	RGBQUAD *pal = FreeImage_GetPalette(img);
	unsigned char pp[3];
	for (i = 0; i < (1 << bpp); i++) {
		pp[0] = pal->rgbBlue;
		pp[1] = pal->rgbGreen;
		pp[2] = pal->rgbRed;
		gbapal = bgra2gba(pp);
		pal++;
		emitString((char *)&gbapal, 2);
	}
	FreeImage_Unload(img);
}

void Output::patch32(unsigned int addr, unsigned int val)
{
	DEBUG("reloc32 addr 0x%x val 0x%x\n", addr, val);
	long cur = ftell(fp);
	fseek(fp, addr - 0x8000000, SEEK_SET);
	fwrite(&val, 1, 4, fp);
	fseek(fp, cur, SEEK_SET);
}

void Output::emitSound(const char *wav)
{
	unsigned size_addr;
	unsigned char c;
	signed char s;

	emitDword(0xfa0e);
	size_addr = addr;
	emitDword(0xcafebabe);
	FILE *fp = fopen(wav, "rb");
	if (!fp)
		GLB_error("cannot open wav file %s\n", wav);

	/* search for data chunk */
	const char *data = "data";
	while (fread(&c, 1, 1, fp)) {
		if (c == *data) {
			data++;
			if (!*data)
				break;
		}
	}
	fseek(fp, 4, SEEK_CUR); /* skip size */

	while (fread(&c, 1, 1, fp)) {
		s = ((int)c) - 0x80;
		emitByte((unsigned char)s);
	}

	patch32(size_addr, ftell(fp) - 0x2c);
	fclose(fp);

	// XXX: looks like BUG_FOR_BUG...
	emitByte(0x80);

	while (addr & 3)
		emitByte(0);
}

void Output::emitMusic(const char *mod)
{
	char buf[4096];
	unsigned int sample_addr = addr;

	emitDword(0); // samples address, will be patched later

	/* convert to pimpmobile format */
	char appdir[256];
	OS_getAppDir(appdir);
	sprintf(buf, "%s" PATHSEP "converter \"%s\"", appdir, mod);
	if (system(buf))
		GLB_error("failed to convert mod %s\n", mod);

	/* copy patterns */
	sprintf(buf, "%s.bin", mod);
	FILE *fp = fopen(buf, "rb");
	if (!fp)
		GLB_error("unable to open converted mod file %s\n", buf);
	unlink(buf);
	for (;;) {
		int bytes = fread(buf, 1, 4096, fp);
		emitString(buf, bytes);
		if (bytes < 4096)
			break;
	}
	fclose(fp);

	while (addr & 3)
		emitByte(0);

	/* copy samples */
	patch32(sample_addr, addr);
	fp = fopen("sample_bank.bin", "rb");
	unlink("sample_bank.bin");
	for (;;) {
		int bytes = fread(buf, 1, 4096, fp);
		emitString(buf, bytes);
		if (bytes < 4096)
			break;
	}
	fclose(fp);

	while (addr & 3)
		emitByte(0);
}

void Output::reloc12(Literal *lit)
{
	unsigned int insn;
	unsigned int target_addr;

	// If the literal has already been coded elsewhere, we use that
	// address. Otherwise we use the current output pointer.
	if (lit->coded_at)
		target_addr = lit->coded_at;
	else
		target_addr = addr;

	DEBUG("reloc12 reloc 0x%x target_addr 0x%x val 0x%x\n", lit->reloc, target_addr,
	      lit->val);
	long cur = ftell(fp);
	fseek(fp, lit->reloc - 0x8000000, SEEK_SET);
	fread(&insn, 1, 4, fp);
	//printf("oinsn 0x%x\n", insn);

	// XXX: add sanity check!
	// XXX: Shouldn't this be abs() or something?
	insn |= target_addr - lit->reloc - 8;
	if (target_addr - 8 > lit->reloc)
		insn |= 1 << 23;

	//printf("ninsn 0x%x\n", insn);
	fseek(fp, lit->reloc - 0x8000000, SEEK_SET);
	fwrite(&insn, 1, 4, fp);
	fseek(fp, cur, SEEK_SET);
}

void Parser::code(unsigned int insn)
{
	if (cur_icode) {
		Icode *i = cur_icode;
		char *p = i->cp;
		*p++ = insn & 0xff;
		*p++ = (insn >> 8) & 0xff;
		*p++ = (insn >> 16) & 0xff;
		*p++ = (insn >> 24);
		i->cp = p;
		i->len += 4;
	} else {
		out->emitDword(insn);
	}
}

#define W(x) !strcmp((x), word)

enum asm_words {
	ASM_REG = 0,
	ASM_OFF = 1,
	ASM_DIR = 2,
	ASM_COND = 3,
	ASM_IMM = 4,
	ASM_AMODE = 5,
};
enum asm_regs {
	REG_TOS = 0,
	REG_A = 1,
	REG_V0 = 2,
	REG_V1 = 3,
	REG_V2 = 4,
	REG_W = 5,
	REG_U = 6,
	REG_RSP = 7,
	REG_V3 = 8,
	REG_V4 = 9,
	REG_V5 = 10,
	REG_V6 = 11,
	REG_V7 = 12,

	REG_R0 = 0,
	REG_R1,
	REG_R2,
	REG_R3,
	REG_R4,
	REG_R5,
	REG_R6,
	REG_R7,
	REG_R8,
	REG_R9,
	REG_R10,
	REG_R11,
	REG_R12,
	REG_SP = 13,
	REG_LR = 14,
	REG_PC = 15,
};
enum asm_dirs {
	DIR_DA =  0x0 << 21,
	DIR_DAW = 0x1 << 21,
	DIR_IA =  0x4 << 21,
	DIR_IAW = 0x5 << 21,
	DIR_DB =  0x8 << 21,
	DIR_DBW = 0x9 << 21,
	DIR_IB =  0xc << 21,
	DIR_IBW = 0xd << 21,
};

enum asm_cond {
	COND_EQ = 0x0 << 28,
	COND_NE = 0x1 << 28,
	COND_CS = 0x2 << 28,
	COND_CC = 0x3 << 28,
	COND_MI = 0x4 << 28,
	COND_PL = 0x5 << 28,
	COND_VS = 0x6 << 28,
	COND_VC = 0x7 << 28,
	COND_HI = 0x8 << 28,
	COND_LS = 0x9 << 28,
	COND_GE = 0xa << 28,
	COND_LT = 0xb << 28,
	COND_GT = 0xc << 28,
	COND_LE = 0xd << 28,
	COND_AL = 0xe << 28,
	COND_NV = 0xf << 28,
};

enum asm_amode {
	AMODE_PREIND  = 1 << 24,
	AMODE_PREINDR = 3 << 24,
	AMODE_POSTIND = (0 << 24) | (1 << 21),
	AMODE_POSTINDR = (2 << 24) | (1 << 21),
	AMODE_IND,
	AMODE_FLAGS = (1 << 20),
	AMODE_IMM   = (1 << 25),
	AMODE_LSLI = 0x00,
	AMODE_LSL  = 0x10,
	AMODE_LSRI = 0x20,
	AMODE_LSR  = 0x30,
	AMODE_ASRI = 0x40,
	AMODE_ASR  = 0x50,
	AMODE_RORI = 0x60,
	AMODE_ROR  = 0x70,
};

enum asm_arm_ops {
	OP_AND = 0x0 << 21,
	OP_EOR = 0x1 << 21,
	OP_SUB = 0x2 << 21,
	OP_RSB = 0x3 << 21,
	OP_ADD = 0x4 << 21,
	OP_ADC = 0x5 << 21,
	OP_SBC = 0x6 << 21,
	OP_RSC = 0x7 << 21,
	OP_TST = (0x8 << 21) | AMODE_FLAGS,
	OP_TEQ = (0x9 << 21) | AMODE_FLAGS,
	OP_CMP = (0xa << 21) | AMODE_FLAGS,
	OP_CMN = (0xb << 21) | AMODE_FLAGS,
	OP_ORR = 0xc << 21,
	OP_MOV = 0xd << 21,
	OP_BIC = 0xe << 21,
	OP_MVN = 0xf << 21,

	OP_LDR = 0x04100000,
	OP_STR = 0x04000000,

	OP_LDM = 0x08100000,
	OP_STM = 0x08000000,

#ifdef BUG_FOR_BUG
	OP_LDRH = 0x00100030,
#else
	OP_LDRH = 0x001000b0,
#endif
	OP_STRH = 0x000000b0,
};

Symbol *Parser::getSymbol(const char *word)
{
	Symbol *sym = symbols.next;

	while (sym) {
		if (!strcmp(sym->word, word))
			return sym;
		sym = sym->next;
	}
	return NULL;
}

Icode *Parser::getIcode(const char *word)
{
	Icode *icode = icodes.next;

	while (icode) {
		if (!strcmp(icode->word, word))
			return icode;
		icode = icode->next;
	}
	return NULL;
}

#define PUSH_ASM(x, y) do { \
	asm_stack[asp][0] = (x); asm_stack[asp++][1] = (y); \
} while (0)

#define ASSERT_REG do { assert(asm_stack[asp][0] == ASM_REG); } while (0)
#define ASSERT_IMM do { assert(asm_stack[asp][0] == ASM_IMM); } while (0)

static unsigned int immrot(unsigned int val, int *err)
{
	int ror = 0;

	if (val > 0xff) while (!(val & 3)) {
			ror--;
			val >>= 2;
	}
	if (err) {
		if (val > 0xff)
			*err = 1;
		else
			*err = 0;
	}
	return ((ror << 8) & 0xf00) | val;
}

static int can_immrot(unsigned int val)
{
	int err;

	immrot(val, &err);
	return !err;
}

unsigned int Parser::arm5CodeOp2()
{
	unsigned int insn = 0;

	if (asm_stack[asp - 1][0] == ASM_AMODE) {
		switch (asm_stack[--asp][1]) {
		case AMODE_IMM:
			/* XXX: check immediate size */
			insn |= AMODE_IMM; /* immediate */
			insn |= immrot(asm_stack[--asp][1], NULL);
			ASSERT_IMM;
			break;
		case AMODE_LSL:
		case AMODE_LSR:
		case AMODE_ASR:
		case AMODE_ROR:
			insn |= asm_stack[asp][1];
			insn |= 1 << 4;                         /* register shift */
			insn |= asm_stack[--asp][1] << 8;       /* Rs */
			ASSERT_REG;
			insn |= asm_stack[--asp][1];            /* Rm */
			ASSERT_REG;
			break;
		case AMODE_LSLI:
		case AMODE_LSRI:
		case AMODE_ASRI:
		case AMODE_RORI:
			insn |= asm_stack[asp][1];
			insn |= asm_stack[--asp][1] << 7;       /* shift_imm */
			ASSERT_IMM;
			insn |= asm_stack[--asp][1];            /* Rm */
			ASSERT_REG;
			break;
		default:
			GLB_error("unimp alu3 amode\n");
		}
	} else {
		/* no addressing mode specified -> RRR */
		insn |= asm_stack[--asp][1]; /* Rm */
		ASSERT_REG;
	}
	return insn;
}

unsigned int Parser::arm5CodeS()
{
	unsigned insn = 0;

	if (asm_stack[asp - 1][0] == ASM_AMODE &&
	    asm_stack[asp - 1][1] == AMODE_FLAGS) {
		insn |= AMODE_FLAGS;    /* S */
		asp--;
	}
	return insn;
}

unsigned int Parser::arm5Code3Ops()
{
	unsigned int insn = 0;

	insn |= armCodeCond();
	insn |= arm5CodeS();

	insn |= asm_stack[--asp][1] << 12;      /* Rd */
	ASSERT_REG;
	insn |= asm_stack[--asp][1] << 16;      /* Rn */
	ASSERT_REG;

	insn |= arm5CodeOp2();
	return insn;
}
#define CODE_ARM5_3OPS do { insn |= arm5Code3Ops(); code(insn); } while (0)

unsigned int Parser::arm5Code2Ops(unsigned int regbit)
{
	unsigned int insn = 0;

	insn |= armCodeCond();
	insn |= arm5CodeS();

	unsigned int rd = asm_stack[--asp][1];
	ASSERT_REG;

	insn |= rd << (regbit);
	insn |= arm5CodeOp2();

	return insn;
}
#define CODE_ARM5_2OPS(r)       do { \
	insn |= arm5Code2Ops(r); code(insn); \
} while (0)

/* insns without Rd */
#define CODE_ARM5_VOID  CODE_ARM5_2OPS(16)
/* insns without Rn (MOV, MVN) */
#define CODE_ARM5_MOV   CODE_ARM5_2OPS(12)

unsigned int Parser::arm9CodeAddr()
{
	unsigned int imm;
	unsigned int insn = 0;

	if (asm_stack[--asp][0] == ASM_AMODE) {
		switch (asm_stack[asp][1]) {
		case AMODE_IND:
			break;

		case AMODE_PREIND:
		case AMODE_POSTIND:
			insn |= asm_stack[asp][1];
			imm = asm_stack[--asp][1];
			if (imm < 0x80000000)
				insn |= 0x00800000; /* upwards */
			else
				imm = -imm;
			ASSERT_IMM;
			insn |= imm;
			break;

		case AMODE_PREINDR:
		case AMODE_POSTINDR:
			insn |= asm_stack[asp][1];
			if (asm_stack[asp - 1][0] == ASM_AMODE &&
			    asm_stack[asp - 1][1] == AMODE_LSLI) {
				--asp;
				insn |= asm_stack[--asp][1] << 7;
				ASSERT_IMM;
				insn |= 0x00800000; /* upwards */
			}
			insn |= asm_stack[--asp][1];
			ASSERT_REG;
			break;

		default:
			GLB_error("unimp ARM.1x addressing mode\n");
		}
		insn |= asm_stack[--asp][1] << 16;
		ASSERT_REG;
	}

	return insn;
}

unsigned int Parser::arm10CodeAddr()
{
	unsigned int imm;
	unsigned int insn = 0;

#ifdef BUG_FOR_BUG
	insn |= 0xb0;
#endif

	assert(asm_stack[asp - 1][0] == ASM_AMODE);
	switch (asm_stack[--asp][1]) {
	case AMODE_PREIND:
		/* Unfortunately the encoding in ARM.10 differs from ARM.9, so we cannot
		 * use the enum directly. */
		insn |= 0x01400000;
do_imm_ind:
		imm = asm_stack[--asp][1];
		if (imm < 0x80000000)
			insn |= 1 << 23; /* upwards */
		else
			imm = -imm;
		ASSERT_IMM;
		insn |= (imm & 0xf);
		insn |= (imm & 0xf0) << 4;
		break;
	case AMODE_POSTIND:
		insn |= 0x00600000;
		goto do_imm_ind;
		break;

	case AMODE_PREINDR:
		insn |= 0x01800000;
do_reg_ind:
		if (asm_stack[asp - 1][0] == ASM_AMODE &&
		    asm_stack[asp - 1][1] == AMODE_LSLI) {
#ifdef BUG_FOR_BUG
			/* Bogus! There is no shifted indexing with halfword memory
			 * accesses, and the specs state bits 8-11 as SBZ and bit
			 * 7 as 1. */
			insn &= ~(1 << 7);
			--asp;
			insn |= asm_stack[--asp][1] << 8;
#else
			asp -= 2;
			/* XXX: Issue a warning. */
#endif
			ASSERT_IMM;
		}
		insn |= asm_stack[--asp][1];
		ASSERT_REG;
		break;
	case AMODE_POSTINDR:
		insn |= 0x00a00000;
		goto do_reg_ind;

	case AMODE_IND:
		insn |= 0x00400000;
		break;
	default:
		GLB_error("internal error\n");
	}
	insn |= asm_stack[--asp][1] << 16;
	ASSERT_REG;
	return insn;
}

unsigned int Parser::armCodeCond()
{
	unsigned int insn = 0;

	if (asm_stack[asp - 1][0] == ASM_COND)
		insn |= asm_stack[--asp][1];
	else
		insn |= COND_AL;

	return insn;
}
#define CODE_COND do { insn |= armCodeCond(); } while (0)

#define CODE_FLAGS do { \
		if (asm_stack[asp - 1][0] == ASM_AMODE && \
		    asm_stack[asp - 1][1] == AMODE_FLAGS) { \
			insn |= 1 << 20; \
			--asp; \
		} \
} while (0)

#define CODE_RD do { \
		insn |= asm_stack[--asp][1] << 12;  \
		ASSERT_REG; \
} while (0)

void Parser::parseAsm(const char *word)
{
	Symbol *sym;

	if (W("r0") || W("tos"))
		PUSH_ASM(ASM_REG, REG_TOS);
	else if (W("r1") || W("a"))
		PUSH_ASM(ASM_REG, REG_R1);
	else if (W("r2") || W("v0"))
		PUSH_ASM(ASM_REG, REG_V0);
	else if (W("r3") || W("v1"))
		PUSH_ASM(ASM_REG, REG_V1);
	else if (W("r4") || W("v2"))
		PUSH_ASM(ASM_REG, REG_V2);
	else if (W("r5") || W("w"))
		PUSH_ASM(ASM_REG, REG_W);
	else if (W("r6") || W("u"))
		PUSH_ASM(ASM_REG, REG_U);
	else if (W("r7") || W("rsp"))
		PUSH_ASM(ASM_REG, REG_RSP);
	else if (W("r8") || W("v3"))
		PUSH_ASM(ASM_REG, REG_V3);
	else if (W("r9") || W("v4"))
		PUSH_ASM(ASM_REG, REG_V4);
	else if (W("r10") || W("v5"))
		PUSH_ASM(ASM_REG, REG_V5);
	else if (W("r11") || W("v6"))
		PUSH_ASM(ASM_REG, REG_V6);
	else if (W("r12") || W("v7"))
		PUSH_ASM(ASM_REG, REG_R12);
	else if (W("r13") || W("sp"))
		PUSH_ASM(ASM_REG, REG_SP);
	else if (W("r14") || W("lr"))
		PUSH_ASM(ASM_REG, REG_LR);
	else if (W("r15") || W("pc"))
		PUSH_ASM(ASM_REG, REG_PC);

	else if (W("da"))
		PUSH_ASM(ASM_DIR, DIR_DA);
	else if (W("da!"))
		PUSH_ASM(ASM_DIR, DIR_DAW);
	else if (W("db"))
		PUSH_ASM(ASM_DIR, DIR_DB);
	else if (W("db!"))
		PUSH_ASM(ASM_DIR, DIR_DBW);
	else if (W("ia"))
		PUSH_ASM(ASM_DIR, DIR_IA);
	else if (W("ia!"))
		PUSH_ASM(ASM_DIR, DIR_IAW);
	else if (W("ib"))
		PUSH_ASM(ASM_DIR, DIR_IB);
	else if (W("ib!"))
		PUSH_ASM(ASM_DIR, DIR_IBW);

	else if (W("0@"))
		PUSH_ASM(ASM_AMODE, AMODE_IND);
	else if (W("##"))
		PUSH_ASM(ASM_AMODE, AMODE_IMM);
	else if (W("s!"))
		PUSH_ASM(ASM_AMODE, AMODE_FLAGS);
	else if (W("#asr"))
		PUSH_ASM(ASM_AMODE, AMODE_ASRI);
	else if (W("#lsl"))
		PUSH_ASM(ASM_AMODE, AMODE_LSLI);
	else if (W("#lsr"))
		PUSH_ASM(ASM_AMODE, AMODE_LSRI);
	else if (W("#ror"))
		PUSH_ASM(ASM_AMODE, AMODE_RORI);
	else if (W("lsl"))
		PUSH_ASM(ASM_AMODE, AMODE_LSL);

	else if (W("eq?"))
		PUSH_ASM(ASM_COND, COND_EQ);
	else if (W("ne?"))
		PUSH_ASM(ASM_COND, COND_NE);
	else if (W("cs?") || W("hs?"))
		PUSH_ASM(ASM_COND, COND_CS);
	else if (W("cc?") || W("lo?"))
		PUSH_ASM(ASM_COND, COND_CC);
	else if (W("mi?"))
		PUSH_ASM(ASM_COND, COND_MI);
	else if (W("pl?"))
		PUSH_ASM(ASM_COND, COND_PL);
	else if (W("vs?"))
		PUSH_ASM(ASM_COND, COND_VS);
	else if (W("vc?"))
		PUSH_ASM(ASM_COND, COND_VC);
	else if (W("hi?"))
		PUSH_ASM(ASM_COND, COND_HI);
	else if (W("ls?"))
		PUSH_ASM(ASM_COND, COND_LS);
	else if (W("ge?"))
		PUSH_ASM(ASM_COND, COND_GE);
	else if (W("lt?"))
		PUSH_ASM(ASM_COND, COND_LT);
	else if (W("gt?"))
		PUSH_ASM(ASM_COND, COND_GT);
	else if (W("le?"))
		PUSH_ASM(ASM_COND, COND_LE);
	else if (W("al?"))
		PUSH_ASM(ASM_COND, COND_AL);
	else if (W("nv?"))
		PUSH_ASM(ASM_COND, COND_NV);

#define ARM53OP(str, op) \
	else if (W(#str ",")) { \
		unsigned int insn = OP_ ## op; \
		CODE_ARM5_3OPS; \
	}

	ARM53OP(and, AND)
	ARM53OP(orr, ORR)
	ARM53OP(eor, EOR)
	ARM53OP(add, ADD)
	ARM53OP(adc, ADC)
	ARM53OP(sub, SUB)
	ARM53OP(sbc, SBC)
	ARM53OP(rsb, RSB)
	ARM53OP(rsc, RSC)
	ARM53OP(bic, BIC)

#define ARM5VOID(str, op) \
	else if (W(#str ",")) { \
		unsigned int insn = OP_ ## op; \
		CODE_ARM5_VOID; \
	}

	ARM5VOID(tst, TST)
	ARM5VOID(teq, TEQ)
	ARM5VOID(cmp, CMP)
	ARM5VOID(cmn, CMN)

	else if (W("b,") || W("bl,")) {
		unsigned insn = 0x0a000000;
		if (word[1] == 'l')
			insn |= 0x01000000;
		CODE_COND;
		unsigned int dest = asm_stack[--asp][1];
		assert(asm_stack[asp][0] == ASM_OFF);
		DEBUG("asm branch from 0x%x to 0x%x\n", out->addr, dest);
		code(insn | (((dest - out->addr - 8) >> 2) & 0x00ffffff));
	} else if (W("bx,")) {
		unsigned insn = 0x012fff10;
		CODE_COND;
		code(insn | asm_stack[--asp][1]);
		ASSERT_REG;
	} else if (W("ldm,") || W("stm,")) {
		/* format ARM.11 */
		unsigned int insn = 0;
		CODE_COND;
		while (asm_stack[--asp][0] == ASM_REG)
			//printf("reg %d\n", asm_stack[asp][1]);
			insn |= 1 << asm_stack[asp][1];
		if (word[0] == 'l')
			insn |= OP_LDM;
		else
			insn |= OP_STM;

		insn |= asm_stack[asp][1];
		assert(asm_stack[asp][0] == ASM_DIR);

		insn |= asm_stack[--asp][1] << 16;
		ASSERT_REG;
		code(insn);
	} else if (W("ldr,") || W("str,") || W("ldrb,") || W("strb,")) {
		unsigned insn;

		if (word[0] == 'l')
			insn = OP_LDR;
		else
			insn = OP_STR;

		if (word[3] == 'b')
			insn |= 1 << 22;

		CODE_COND;
		CODE_RD;
		insn |= arm9CodeAddr();
		code(insn);
	} else if (W("ldrh,")) {
		unsigned int insn = OP_LDRH;
		CODE_COND;
		CODE_RD;
		insn |= arm10CodeAddr();
		code(insn);
	} else if (W("strh,")) {
		unsigned int insn = OP_STRH;
		CODE_COND;
		CODE_RD;
		insn |= arm10CodeAddr();
		code(insn);
	// XXX: mrs, msr, cdp, stc, ldc, mcr, mrc
	} else if (W("link")) {
		unsigned int insn = 0xe9270000;
		insn |= 1 << (asm_stack[--asp][1]);
		ASSERT_REG;
		code(insn);
	} else if (W("mov,")) {
		unsigned int insn = 0x01a00000;
		CODE_ARM5_MOV;
	} else if (W("mvn,")) {
		unsigned int insn = 0x01e00000;
		CODE_ARM5_MOV;
	// XXX: mla, um??l, sm??l
	} else if (W("mul,")) {
		unsigned int insn = 0x00000090;
		CODE_COND;
		insn |= asm_stack[--asp][1] << 16;
		ASSERT_REG;
		insn |= asm_stack[--asp][1];
		ASSERT_REG;
		insn |= asm_stack[--asp][1] << 8;
		ASSERT_REG;
		code(insn);
	} else if (W("ret")) {
		unsigned int insn = 0x012fff1e;
		CODE_COND;
		code(insn);
	} else if (W("swp,")) {
		unsigned int insn = 0x01000090;
		CODE_COND;
		CODE_RD;
		insn |= asm_stack[--asp][1];
		ASSERT_REG;
		insn |= asm_stack[--asp][1] << 16;
		ASSERT_REG;
		code(insn);
	} else if (W("pop")) {
		unsigned int insn = 0x04bd0004;
		CODE_COND;
		CODE_RD;
		code(insn);
	} else if (W("push")) {
		unsigned int insn = 0x092d0000;
		CODE_COND;
		insn |= 1 << asm_stack[--asp][1];
		code(insn);
	} else if (W("swi,")) {
		unsigned int insn = 0x0f000000;
		CODE_COND;
		insn |= asm_stack[--asp][1] << 16;
		ASSERT_IMM;
		code(insn);
	} else if (W("unlink")) {
		unsigned int insn = 0xe4b70004;
		CODE_RD;
		code(insn);
	} else if (W("l:")) {
		asm_labels[lsp].label = strdup(getNextWord());
		asm_labels[lsp++].addr = out->addr;
		DEBUG("asm label %s at 0x%x\n", asm_labels[lsp - 1].label,
		      out->addr);
	} else if (isNum(word)) {
		unsigned int imm = TIN_parseNum(word);
		PUSH_ASM(ASM_IMM, imm);
	} else if (W("#offset")) {
		assert(asm_stack[asp - 1][0] == ASM_IMM);
		asm_stack[asp - 1][0] = ASM_OFF;
		asm_stack[asp - 1][1] += out->addr;

	// XXX: there should be a list of constants
	} else if (W("backbuffer")) {
		PUSH_ASM(ASM_IMM, 0xa000);
	} else if (W("bg_regs")) {
		PUSH_ASM(ASM_IMM, 0x400);
	} else if (W("bg0_sx")) {
		PUSH_ASM(ASM_IMM, 0);
	} else if (W("globals")) {
		PUSH_ASM(ASM_IMM, 0x600);
	} else if (W("int_t0")) {
		PUSH_ASM(ASM_IMM, 0x10);
	} else if (W("int_t1")) {
		PUSH_ASM(ASM_IMM, 0x14);
	} else if (W("int_p1")) {
		PUSH_ASM(ASM_IMM, 0x18);
	} else if (W("int_hb")) {
		PUSH_ASM(ASM_IMM, 0x1c);
	} else if (W("int_vb")) {
		PUSH_ASM(ASM_IMM, 0x20);
	} else if (W("int_vc")) {
		PUSH_ASM(ASM_IMM, 0x24);
	} else if (W("int_t2")) {
		PUSH_ASM(ASM_IMM, 0x28);
	} else if (W("iwram")) {
		PUSH_ASM(ASM_IMM, 0x3000000);
	} else if (W("registers")) {
		PUSH_ASM(ASM_IMM, 0x4000000);
	} else if (W("vram")) {
		PUSH_ASM(ASM_IMM, 0x6000000);

	} else if (W("#(")) {
		PUSH_ASM(ASM_AMODE, AMODE_PREIND);
	} else if (W("(#")) {
		PUSH_ASM(ASM_AMODE, AMODE_POSTIND);
	} else if (W("+(")) {
		PUSH_ASM(ASM_AMODE, AMODE_PREINDR);
	} else if (W("(+")) {
		PUSH_ASM(ASM_AMODE, AMODE_POSTINDR);

	} else if (W("literal")) {
		unsigned int insn = 0x051f0000;
		CODE_COND;
		CODE_RD;
		literals.prependNew(asm_stack[--asp][1], out->addr);
		assert(asm_stack[asp][0] == ASM_IMM ||
		       asm_stack[asp][0] == ASM_OFF);
		code(insn);
	} else if ((sym = getSymbol(word))) {
		DEBUG("asmsym %s 0x%x\n", word, sym->addr);
		PUSH_ASM(ASM_OFF, sym->addr);
	} else {
		int i;
		for (i = lsp - 1; i >= 0; i--) {
			if (!strcmp(asm_labels[i].label, word)) {
				PUSH_ASM(ASM_OFF, asm_labels[i].addr);
				return;
			}
		}
		GLB_error("unknown asm word %s\n", word);
	}
}

void Parser::codeAsm(
	const char *w0,
	const char *w1,
	const char *w2,
	const char *w3,
	const char *w4,
	const char *w5,
	const char *w6)
{
	if (w0) parseAsm(w0);
	if (w1) parseAsm(w1);
	if (w2) parseAsm(w2);
	if (w3) parseAsm(w3);
	if (w4) parseAsm(w4);
	if (w5) parseAsm(w5);
	if (w6) parseAsm(w6);
}

void Parser::codeAsm(
	unsigned int w0,
	const char *w1,
	const char *w2,
	const char *w3,
	const char *w4)
{
	PUSH_ASM(ASM_IMM, w0);
	codeAsm(w1, w2, w3, w4);
}

void Parser::codeAsm(
	const char *w0,
	unsigned int w1,
	const char *w2,
	const char *w3,
	const char *w4)
{
	codeAsm(w0);
	PUSH_ASM(ASM_IMM, w1);
	codeAsm(w2, w3, w4);
}

void Parser::codeBranch(unsigned int dest, const char *mnem)
{
	PUSH_ASM(ASM_OFF, dest);
	codeAsm(mnem);
}

void Parser::codeCallThumb(unsigned int dest)
{
	codeAsm("pc", "4", "#(", "r5", "ldr,"); // load function address
	codeAsm("pc", "lr", "mov,");            // sets LR to skip the address when returning
	codeAsm("r5", "bx,");
	code(dest | 1);                         // address with bit 0 set for Thumb mode
}

void Parser::parseAll()
{
	const char *word;
	Symbol *sym;
	Icode *icode;
	bool currently_naked = false;
#ifdef BUG_FOR_BUG
	bool dont_imm_com = false;
#endif

parse_next:
	word = getNextWord();
	if (!word) {
		while (sp > 0) {
			popText();
			word = getNextWord();
			if (word)
				break;
		}
		if (!word)
			return;
	}

	DEBUG("word -%s- at 0x%x\n", word, out->addr);
	if (W("requires\"")) {
		pushText(getNextWord());
	} else if (W("bitmap\"")) {
		out->emitBitmap(getNextWord());
	} else if (W("palette\"")) {
		out->emitPalette(getNextWord());
	} else if (W("sound\"")) {
		out->emitSound(getNextWord());
	} else if (W("music\"")) {
		out->emitMusic(getNextWord());
	} else if (W("title\"")) {
		out->setTitle(getNextWord());
	} else if (W("entry")) {
		Symbol *sym = getSymbol(getNextWord());
		out->setEntry(sym->addr);
	} else if (W("program\"")) {
		/* ignore */
		// XXX: maybe we should do a rename() here or something...
		getNextWord();
	} else if (W("{")) {
		while ((word = getNextWord()) && word && !W("}")) ;
		if (!word)
			GLB_error("missing }\n");
	} else if (W("(")) {
		while ((word = getNextWord()) && word &&
		       word[strlen(word) - 1] != ')') ;
		if (!word)
			GLB_error("missing )\n");
	} else if (W("\\")) {
		//printf("skipping ==");
		while (*tptr && *tptr != '\n')
			//printf("%c", *tptr);
			tptr++;
		while (isspace(*tptr))
			tptr++;
		//printf("==\n");
	} else if (W("icode")) {
		cur_icode = icodes.appendNew(getNextWord());
		asm_mode = true;
	} else if (W("code")) {
		symbols.appendNew(out->addr, getNextWord());
		asm_mode = true;
	} else if (W("end-code")) {
		if (cur_icode)
			cur_icode = 0;
		asm_mode = false;
		literals.code(out);
		lsp = 0;
	} else if (asm_mode) {
		parseAsm(word);
	} else if (W(":") || W(":n")) {
		currently_naked = word[1] == 'n';
		sym = symbols.appendNew(out->addr, getNextWord());
		if (!currently_naked) {
			sym->has_prolog = true;
			codeAsm("r7", "db!", "r6", "stm,");
			codeAsm("lr", "r6", "mov,");
		}
		DEBUG("===start word %s at 0x%x\n", sym->word, sym->addr);
	} else if (W("label")) {
		sym = symbols.appendNew(out->addr, getNextWord());
		DEBUG("===start label %s at 0x%x\n", sym->word, sym->addr);
	} else if (W(";")) {
		if (currently_naked)
			codeAsm("lr", "bx,");
		else
			codeAsm("r6", "bx,");
		currently_naked = false;
	} else if (W("swap")) {
		if (getNextWordIf("a!")) {
			codeAsm("sp", "4", "(#", "r1", "ldr,");
		} else if (getNextWordIf("-")) {
			codeAsm("r5", "pop");
			codeAsm("r0", "r5", "r0", "rsb,");
		} else if (getNextWordIf("over")) {
			codeAsm("sp", "0@", "r5", "ldr,");
			codeAsm("sp", "db!", "r5", "stm,");
			codeAsm("sp", "4", "#(", "r0", "str,");
		} else if (getNextWordIf("!")) {
			codeAsm("sp", "ia!", "r2", "r3", "ldm,");
			codeAsm("r2", "0@", "r0", "str,");
			codeAsm("r3", "r0", "mov,");
		} else {
			codeAsm("sp", "r0", "r0", "swp,");
		}
	} else if (W("dup")) {
		if (getNextWordIf("a!"))
			codeAsm("r0", "r1", "mov,");
		else
			codeAsm("r0", "push");
	} else if (W("over")) {
		if (getNextWordIf("-")) {
			codeAsm("sp", "0@", "r5", "ldr,");
			codeAsm("r5", "r0", "r0", "sub,");
		} else if (getNextWordIf("!")) {
			codeAsm("sp", "4", "(#", "r5", "ldr,");
			codeAsm("r5", "0@", "r0", "str,");
			codeAsm("r5", "r0", "mov,");
		} else {
			codeAsm("r0", "push");
			codeAsm("sp", "4", "#(", "r0", "ldr,");
		}
	} else if (W("pop")) {
		codeAsm("r0", "push");
		codeAsm("r6", "r0", "mov,");
		codeAsm("r7", "4", "(#", "r6", "ldr,");
	} else if (W("push")) {
		codeAsm("r7", "db!", "r6", "stm,");
		codeAsm("r0", "r6", "mov,");
		codeAsm("sp", "4", "(#", "r0", "ldr,");
	} else if (W("0=")) {
		codeAsm("0", "##", "r0", "cmp,");
		codeAsm("0", "##", "r0", "ne?", "mov,");
		codeAsm("0", "##", "r0", "eq?", "mvn,");
	} else if (W("0<")) {
		codeAsm("0", "##", "r0", "cmp,");
		codeAsm("0", "##", "r0", "pl?", "mov,");
		codeAsm("0", "##", "r0", "mi?", "mvn,");
	} else if (W("1+")) {
		codeAsm("1", "##", "r0", "r0", "add,");
	} else if (W("com")) {
		if (getNextWordIf("1+"))
			codeAsm("0", "##", "r0", "r0", "rsb,");
		else
			codeAsm("r0", "r0", "mvn,");
	} else if (W("and")) {
		codeAsm("sp", "4", "(#", "r5", "ldr,");
		codeAsm("r0", "r5", "r0", "and,");
	} else if (W("or")) {
		codeAsm("sp", "4", "(#", "r5", "ldr,");
		codeAsm("r0", "r5", "r0", "orr,");
	} else if (isNum(word)) {
		unsigned int num = TIN_parseNum(word);
		if (getNextWordIf("swi")) {
			codeAsm(num, "swi,");
		} else if (getNextWordIf("#")) {
			//printf("got imm 0x%x\n", num);
			int have_num = 0;
			for (;;) {
				if (isNum(peekNextWord()) &&
				    isWordN(1, "#")) {
					if (isWordN(2, "-"))
						num -= TIN_parseNum(
							getNextWord());
					else if (isWordN(2, "+"))
						num += TIN_parseNum(
							getNextWord());
					else
						break;
					getNextWord();
					getNextWord();
					have_num = 1;
				} else {
					break;
				}
			}
			if (have_num)
				goto emit_num;
			if (getNextWordIf("n/")) {
				codeAsm("r0");
				codeAsm(num, "#lsr", "r0", "mov,");
			} else if (getNextWordIf("a/")) {
				codeAsm("r0");
				codeAsm(num, "#asr", "r0", "mov,");
			} else if (getNextWordIf("n*")) {
				if (getNextWordIf("+")) {
					codeAsm("r5", "pop");
					codeAsm("r0");
					codeAsm(num, "#lsl", "r5", "r0",
						"add,");
				} else {
					codeAsm("r0");
					codeAsm(num, "#lsl", "r0", "mov,");
				}
			} else if (getNextWordIf("+")) {
				if (num != 0) {
					if (can_immrot(num))
						codeAsm(num, "##", "r0",
							"r0", "add,");
					else
						GLB_error("unimp add1\n");

				}
			} else if (num == 0 && getNextWordIf("or")) {
				/* nop */
			} else if (
#ifdef BUG_FOR_BUG
				   !dont_imm_com &&
#endif
				   getNextWordIf("com")) {
				if (getNextWordIf("and")) {
					codeAsm("r0", "push");
					if (can_immrot(num))
						codeAsm(num, "##", "r0",
							"mov,");
					else
						GLB_error("unimp com/and\n");

					codeAsm("sp", "4", "(#", "r5",
						"ldr,");
					codeAsm("r0", "r5", "r0", "bic,");
				} else if (getNextWordIf("1+")) {
					codeAsm("r0", "push");
					if (can_immrot(num))
						codeAsm(num, "##", "r0",
							"mov,");
					else
						GLB_error("unimp com/1+\n");

					codeAsm("0", "##", "r0", "r0",
						"rsb,");
				} else {
					if (can_immrot(num))
						codeAsm(num, "##", "r0",
							"mvn,");
					else
						GLB_error("unimp com1\n");

				}
			} else if (getNextWordIf("-")) {
				codeAsm(num, "##", "r0", "r0", "sub,");
			} else if (getNextWordIf("*")) {
				if (can_immrot(num)) {
					codeAsm(num, "##", "r5", "mov,");
				} else {
					assert(!currently_naked);
					codeBranch(RT__tin_wlit, "bl,");
					code(num);
				}
				codeAsm("r5", "r0", "r0", "mul,");
			} else if (getNextWordIf("and")) {
				if (can_immrot(num))
					codeAsm(num, "##", "r0", "r0", "and,");
				else
					GLB_error("unimp and1\n");
			} else {
				//printf("misc num\n");
emit_num:
				if (num > 0xff) {
					int err;
					unsigned int imm;
					imm = immrot(num, &err);
					if (err) {
						assert(!currently_naked);
						codeBranch(RT__tin_lit,
							   "bl,");
						code(num);
#ifdef BUG_FOR_BUG
						// Deoptimization to maintain
						// byte-for-byte compatibility.
						dont_imm_com = true;
						goto parse_next;
#endif
					} else {
						codeAsm("r0", "push");
						codeAsm(imm, "##", "r0",
							"mov,");
					}
				} else {
					//printf("small num\n");
					codeAsm("r0", "push");
					codeAsm(num, "##", "r0", "mov,");
				}
			}
		} else if (getNextWordIf(",")) {
			code(num);
		} else {
			GLB_error("unimp num\n");
		}
#ifdef BUG_FOR_BUG
		dont_imm_com = false;
#endif
	} else if (W("c\"")) {
		unsigned int end_str;
		const char *str = getNextWord();
		assert(!currently_naked);
		codeBranch(RT__tin_slit, "bl,");
		end_str = out->addr + 4 + 1 + strlen(str);
#ifdef BUG_FOR_BUG
		// This emulates the behavior of the legacy compiler
		// to add another zero dword even if end_str is
		// already aligned.
		end_str++;
#endif
		end_str = (end_str + 3) & ~3;
		code(end_str);
		out->emitByte(strlen(str));
		out->emitString(str, strlen(str));
		while (out->addr < end_str)
			out->emitByte(0);
	} else if ((sym = getSymbol(word))) {
		DEBUG("syma %s 0x%x oa 0x%x is_addr %d lit_addr 0x%x\n", sym->word, sym->addr,
		      out->addr, sym->is_addr, sym->lit_addr);
		if (sym->is_addr && (sym->lit_addr & 0x00ffffff) < 0x800 && getNextWordIf("@")) {
			DEBUG("litload\n");
			codeAsm("r0", "push");
			codeAsm(sym->lit_addr & 0xff000000, "##", "r5", "mov,");
			codeAsm("r5", sym->lit_addr & 0x00ffffff, "#(", "r0", "ldr,");
		} else if (sym->is_addr && (sym->lit_addr & 0x00ffffff) < 0x800 && getNextWordIf("!")) {
			DEBUG("litstore\n");
			codeAsm(sym->lit_addr & 0xff000000, "##", "r5", "mov,");
			codeAsm("r5", sym->lit_addr & 0x00ffffff, "#(", "r0", "str,");
			codeAsm("r0", "pop");
		} else {
			assert(!currently_naked);
			codeAsm(sym->word, "bl,");
			if (sym->has_prolog)
				codeAsm("r7", "4", "(#", "r6", "ldr,");
		}
	} else if ((icode = getIcode(word))) {
		DEBUG("emit icode %s len %d\n", icode->word,
		      icode->len);
		out->emitString(icode->code, icode->len);
	} else if (W("drop")) {
		if (getNextWordIf("a"))
			codeAsm("r1", "r0", "mov,"); //code(0xe1a00001);
		else
			codeAsm("sp", "4", "(#", "r0", "ldr,");
	} else if (W("+")) {
		codeAsm("r5", "pop");
		codeAsm("r0", "r5", "r0", "add,");
	} else if (W("-")) {
		codeAsm("r5", "pop");
		codeAsm("r0", "r5", "r0", "sub,");
	} else if (W("*")) {
		codeAsm("r5", "pop");
		codeAsm("r0", "r5", "r0", "mul,");
	} else if (W("a!")) {
		codeAsm("r0", "r1", "mov,");
		codeAsm("sp", "4", "(#", "r0", "ldr,");
	} else if (W("a")) {
		codeAsm("r0", "push");
		codeAsm("r1", "r0", "s!", "mov,");
	} else if (W("c!a")) {
		codeAsm("r1", "1", "(#", "r0", "strb,");
		codeAsm("sp", "4", "(#", "r0", "ldr,");
	} else if (W("@")) {
		codeAsm("r0", "0@", "r0", "ldr,");
	} else if (W("@a")) {
		codeAsm("r0", "push");
		codeAsm("r1", "4", "(#", "r0", "ldr,");
	} else if (W("c@a")) {
		codeAsm("r0", "push");
		codeAsm("r1", "1", "(#", "r0", "ldrb,");
	} else if (W("r@")) {
		codeAsm("r0", "push");
		codeAsm("r6", "r0", "mov,");
	} else if (W("!")) {
		codeAsm("r5", "pop");
		codeAsm("r0", "0@", "r5", "str,");
		codeAsm("sp", "4", "(#", "r0", "ldr,");
	} else if (W("aligned")) {
		codeAsm("3", "##", "r0", "tst,");
		codeAsm("3", "##", "r0", "r0", "bic,");
		codeAsm("4", "##", "r0", "r0", "ne?", "add,");
	} else if (W("variable")) {
		sym = symbols.appendNew(out->addr, getNextWord());
		sym->is_addr = true;
		sym->lit_addr = out->vaddr;
		codeAsm("r0", "push");
		codeAsm("4", "##", "pc", "r5", "add,");
		code(0xe4150000);
		code(0xe12fff1e);
		code(out->vaddr);
		out->vaddr += 4;
	} else if (W("create")) {
		symbols.appendNew(out->addr, getNextWord());
		unsigned int size = TIN_parseNum(getNextWord());
		const char *type = getNextWord();
		// XXX: What about CELLS/STRINGS?
		DEBUG("reserve type %s at 0x%x\n", type, out->vaddr);
		if (strcmp(getNextWord(), "reserve"))
			GLB_error("create without reserve\n");
		codeAsm("r0", "push");
		codeAsm("4", "##", "pc", "r5", "add,");
		codeAsm("r5", "0@", "r0", "ldr,");
		codeAsm("lr", "bx,");
		code(out->vaddr);
		out->vaddr += 4 * size;
	} else if (W("data:")) {
		symbols.appendNew(out->addr, getNextWord());
		codeAsm("r0", "push");
		codeAsm("pc", "r0", "mov,");
		codeAsm("lr", "bx,");
	} else if (W("if")) {
		assert(!currently_naked);
		codeBranch(RT__tin_t_eq_0, "bl,");
		loop_stack[lpsp++] = out->addr;
		code(0);
	} else if (W("else")) {
		assert(!currently_naked);
		codeBranch(RT__tin_goto, "bl,");
		code(0);
		out->patch32(loop_stack[--lpsp], out->addr);
		loop_stack[lpsp++] = out->addr - 4;
	} else if (W("then")) {
		out->patch32(loop_stack[--lpsp], out->addr);
	} else if (W("begin")) {
		loop_stack[lpsp++] = out->addr;
	} else if (W("while")) {
		assert(!currently_naked);
		codeBranch(RT__tin_t_eq_0, "bl,");
		loop_stack[lpsp++] = out->addr;
		code(0);
	} else if (W("repeat")) {
		assert(!currently_naked);
		codeBranch(RT__tin_goto, "bl,");
		out->patch32(loop_stack[--lpsp], out->addr + 4);
		code(loop_stack[--lpsp]);
	} else if (W("again")) {
		assert(!currently_naked);
		codeBranch(RT__tin_goto, "bl,");
		code(loop_stack[--lpsp]);
	} else if (W("interrupt")) {
		symbols.appendNew(out->addr, getNextWord());
		codeAsm("r0", "push");
		codeAsm("pc", "r0", "mov,");
		codeAsm("lr", "bx,");
		codeAsm("sp", "db!", "lr", "r10", "r9", "r8", "stm,");
		codeAsm("$3000000", "##", "r7", "mov,");
		codeAsm("$f00", "##", "r7", "r7", "add,");
	} else if (W("exit")) {
		codeAsm("sp", "ia!", "lr", "r10", "r9", "r8", "ldm,");
		codeAsm("lr", "bx,");
	} else if (out->use_pimp && W("modvblank")) {
		codeAsm("r0", "push");
		codeCallThumb(RT_pimp_vblank);
		codeAsm("r0", "pop");
	} else if (out->use_pimp && W("modframe")) {
		codeAsm("r0", "push");
		codeCallThumb(RT_pimp_frame);
		codeAsm("r0", "pop");
	} else if (out->use_pimp && W("modinit")) {
		codeAsm("r0", "4", "(#", "r1", "ldr,");
		codeCallThumb(RT_pimp_init);
		codeAsm("r0", "pop");
	} else {
		GLB_error("unknown word %s at %d\n", word,
			  (int)(tptr - text));
	}
	goto parse_next;
}

Parser::Parser()
{
	sp = 0;
	title = 0;
	text = 0;
	tptr = 0;
	cur_icode = 0;
	asm_mode = false;
	symbols.next = 0;
	literals.next = 0;
	lsp = 0;
	asp = 0;
	lpsp = 0;
}

void Output::openOutFile(const char *name)
{
	fp = fopen(name, "wb+");
	if (!fp)
		GLB_error("unable to open output file %s\n", name);
}

void Output::closeOutFile(void)
{
	fclose(fp);
}

Output::Output(unsigned int code_addr, unsigned int var_addr)
{
	addr = code_addr;
	vaddr = var_addr;
	fp = NULL;
	use_pimp = false;
}

int main(int argc, char **argv)
{
	int rt_size;

	FreeImage_Initialise();
	Parser par;
	Output out(0x8000000, 0x2000000);
	char hdr[131072];

	for (;;) {
		if (!strcmp(argv[1], "-debug")) {
			option_debug = true;
			argc--;
			argv++;
		} else if (!strcmp(argv[1], "-mod")) {
			out.use_pimp = true;
			argc--;
			argv++;
		} else {
			break;
		}
	}

	out.openOutFile(argv[2]);

	/* copy runtime to output */
	OS_getAppDir(hdr);
	if (out.use_pimp)
		strcat(hdr, PATHSEP "runpimp.gba");
	else
		strcat(hdr, PATHSEP "runtime.gba");
	FILE *rt = fopen(hdr, "rb");
	if (!rt)
		GLB_error("unable to open %s", hdr);
	rt_size = fread(hdr, 1, 131072, rt);
	fclose(rt);
	out.emitString(hdr, rt_size);

	par.setOutput(&out);
	par.pushText(argv[1]);
	par.parseAll();

	out.fixCartHeader();
	out.closeOutFile();
	FreeImage_DeInitialise();
	return 0;
}
