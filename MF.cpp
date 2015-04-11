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
#include <sndfile.hh>

#include "os.h"

#include "runtime_syms.h"
#include "MF.h"

bool option_debug = false;
bool debug_words = false;


Icode::Icode()
{
	word = NULL;
	cp = code = NULL;
	len = 0;
	next = NULL;
	thumb = false;
}

Icode::Icode(const char *word)
{
	this->word = strdup(word);
	cp = code = new char[1 << 16];
	len = 0;
	next = NULL;
	thumb = false;
}

Icode *Icode::appendNew(const char *word)
{
	Icode *icode = this;

	while (icode->next)
		icode = icode->next;
	icode->next = new Icode(word);
	return icode->next;
}

Symbol::Symbol(unsigned int addr, const char *word)
{
	this->addr = addr;
	this->word = strdup(word);
	next = NULL;
	has_prolog = false;
	is_addr = false;
	is_const = false;
	lit_addr = 0;
	thumb = false;
}

Symbol::Symbol()
{
	addr = 0;
	next = NULL;
	word = NULL;
	has_prolog = false;
	is_addr = false;
	is_const = false;
	lit_addr = 0;
	thumb = false;
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

Literal::Literal()
{
	val = reloc = coded_at = 0;
	thumb = false;
	next = NULL;
	prev = NULL;
}

Literal::Literal(unsigned int val, unsigned int reloc, bool thumb)
{
	this->val = val;
	this->reloc = reloc;
	this->thumb = thumb;
	coded_at = 0;
	next = NULL;
	prev = NULL;
}

// XXX: prepending literals is crap because it increases the chance of
// exceeding the maximum displacement by putting the literals used by
// the first instruction last.
Literal *Literal::prependNew(unsigned int val, unsigned int reloc, bool thumb)
{
	Literal *lit = new Literal(val, reloc, thumb);

	lit->next = next;
	if (next)
		next->prev = lit;
	next = lit;
	return lit;
}

void Literal::code(Output *out)
{
	out->alignDword();
	Literal *tail = next;
	while (tail && tail->next)
		tail = tail->next;
	Literal *lit = tail;
	if (lit)
		out->addSym(".pool");
	unsigned int start_addr = out->addr;
	while (lit) {
		// Check if this value has already been encoded earlier.
		bool already_coded = false;
		Literal* lit2 = tail;
		// XXX: So this is quadratic. Sue me.
		while (lit2 && lit2 != lit) {
			if (lit2->coded_at && lit2->val == lit->val) {
				already_coded = true;
				break;
			}
			lit2 = lit2->prev;
		}

		if (already_coded) {
			DEBUG("literal for 0x%x already coded at 0x%x\n",
			      lit->reloc, lit2->coded_at);
			// Overwrite earlier literal's relocatee with
			// current one's.  This doesn't cause problems
			// because we have already processed lit2.
			lit2->reloc = lit->reloc;
			if (lit->thumb)
				out->reloc8(lit2);
			else
				out->reloc12(lit2);
		} else {
			if (lit->thumb)
				out->reloc8(lit);
			else
				out->reloc12(lit);
			lit->coded_at = out->addr;
			out->emitDword(lit->val);
		}

		lit = lit->prev;
	}

	if (out->addr > start_addr) {
		char dbgsym[10];
		sprintf(dbgsym, ".dbl:%04X", out->addr - start_addr);
		out->addSym(dbgsym, start_addr);
	}

	while (next) {
		lit = next->next;
		delete next;
		next = lit;
	}
}

void Parser::setOutput(Output *out)
{
	this->out = out;
}

const char *Parser::_getNextWord()
{
	static int text_mode = 0;
	static char bufs[8][256];
	static int buf_ptr = 0;
	char *buf;
	int idx = 0;

	buf = bufs[buf_ptr];
	buf_ptr = (buf_ptr + 1) % 8;

	prev_tptr = tptr;

	if (text_mode) {
		while (*tptr && *tptr != '"')
			buf[idx++] = *tptr++;
		tptr++;
	} else {
		while (*tptr && !isspace(*tptr))
			buf[idx++] = tolower(*tptr++);
	}

	if (idx == 0)
		return NULL;

	if (buf[idx - 1] == '"') {
		text_mode = 1;
		assert(isspace(*tptr));
		tptr++;
	}
	else {
		text_mode = 0;
		while (isspace(*tptr))
			tptr++;
	}
	buf[idx] = 0;

	return buf;
}

const char *Parser::getNextWord()
{
	const char *word = _getNextWord();
	if (!debug_words)
		DEBUG("\"%s", word);
	else
		DEBUGN(" %s", word);
	debug_words = true;
	return word;
}

const char *Parser::peekWordN(int n)
{
	char *save_tptr = tptr;
	const char *pw;

	do {
		pw = _getNextWord();
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
	fwrite(title, 1,
	       strlen(title) > 12 ? 12 : strlen(title),
	       fp);
	fseek(fp, cur, SEEK_SET);
}

void Output::setEntry(unsigned int addr)
{
	long cur = ftell(fp);

	fseek(fp, RT__tin_entry_p - 0x8000000, SEEK_SET);
	fwrite(&addr, 1, 4, fp);
	fseek(fp, cur, SEEK_SET);
}

void Output::setIwramTable(unsigned int addr)
{
	long cur = ftell(fp);

	fseek(fp, RT__tin_iwram_table_p - 0x8000000, SEEK_SET);
	fwrite(&addr, 1, 4, fp);
	fseek(fp, cur, SEEK_SET);
}

unsigned int Output::addIwram(unsigned int from, int size)
{
	unsigned int start = start_iwram;
	iwram[iwptr].from = from;
	iwram[iwptr].size = size;
	iwram[iwptr++].to = start;
	DEBUG("addiwram from 0x%x to 0x%x size %d now 0x%x\n",
		from, start, size, iwaddr);
	start_iwram += size;
	return start;
}

void Output::codeIwramTable()
{
	if (!iwptr)
		return;

	alignDword();
	patch32(RT__tin_entry_p + 4, addr);
	for (unsigned int i = 0; i < iwptr; i++) {
		emitDword(iwram[i].from);
		emitDword(iwram[i].to);
		emitDword(iwram[i].size);
	}
	emitDword(0);
}

unsigned int Output::ta()
{
	if (!currently_iwram)
		return addr;
	else
		return iwaddr;
}


void Output::fixCartHeader()
{
	unsigned char bytes[0x1c];
	unsigned char check_byte = 0;
	long saved_pos = ftell(fp);

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

static bool ishex(char c)
{
	return (c >= 'a' && c <= 'f') ||
	       (c >= 'A' && c <= 'F');
}

bool isNum(const char *word)
{
	char last_digit = word[strlen(word) - 1];
	return (isdigit(word[0]) ||
	       word[0] == '$' ||
	       (word[0] == '-' && (isdigit(word[1]) || word[1] == '$')))
	       &&
	       (isdigit(last_digit) || ishex(last_digit));
}

bool isPow2(unsigned int num)
{
	while (num && !(num & 1))
		num >>= 1;
	return num == 1;
}

unsigned int log2(unsigned int num)
{
	unsigned int pow = 0;
	while (num && !(num & 1)) {
		num >>= 1;
		pow++;
	}
	return pow;
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
	if (currently_iwram)
		iwaddr += 4;
}

void Output::alignDword()
{
	while(addr & 3)
		emitByte(0);
}

void Output::emitWord(const unsigned short word)
{
	fputc(word & 0xff, fp);
	fputc((word >> 8) & 0xff, fp);
	addr += 2;
	if (currently_iwram)
		iwaddr += 2;
}

void Output::emitString(const char *str, int len)
{
	fwrite(str, 1, len, fp);
	addr += len;
	if (currently_iwram)
		iwaddr += len;
}

#define RGB8TO5(c) ((c) >> 3)

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

void Output::emitBinary(const char *file)
{
	FILE* fp = fopen(file, "rb");
	if (!fp)
		GLB_error("failed to open %s\n", file);
	char buf[4096];
	int size;
	while ((size = fread(buf, 1, 4096, fp))) {
		emitString(buf, size);
	}
	fclose(fp);
}

void Output::emitBitmap(const char *bmp)
{
	FREE_IMAGE_FORMAT fif = FreeImage_GetFileType(bmp, 0);

	if (fif == FIF_UNKNOWN)
		GLB_error("%s: file not found or unknown image format\n", bmp);
	FIBITMAP *img = FreeImage_Load(fif, bmp, 0);
	if (!img)
		GLB_error("%s: failed to load bitmap\n", bmp);

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

void Output::emitMap(const char *fmp)
{
	Decode(fmp, this);
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
	DEBUG("patch32 addr 0x%x val 0x%x\n", addr, val);
	long cur = ftell(fp);
	fseek(fp, addr - 0x8000000, SEEK_SET);
	fwrite(&val, 1, 4, fp);
	fseek(fp, cur, SEEK_SET);
}

void Output::patch16(unsigned int addr, unsigned short val)
{
	DEBUG("patch16 addr 0x%x val 0x%x\n", addr, val);
	long cur = ftell(fp);
	fseek(fp, addr - 0x8000000, SEEK_SET);
	fwrite(&val, 1, 2, fp);
	fseek(fp, cur, SEEK_SET);
}

void Output::emitSound(const char *wav)
{
	unsigned size_addr;

	SndfileHandle infile(wav);
	if (infile.error())
		GLB_error("%s: %s\n", wav, infile.strError());
	DEBUG("sndfile %s fmt %d chans %d rate %d\n", wav, infile.format(),
	      infile.channels(), infile.samplerate());

	int timer_reload = -16780000 / infile.samplerate();
	emitDword(timer_reload & 0xffff);

	size_addr = addr;
	emitDword(0xcafebabe);

	int count = 0;
	short sample;
	while (infile.read(&sample, 1)) {
		emitByte(sample / 256);
		count++;
	}

	patch32(size_addr, count);

	// XXX: looks like BUG_FOR_BUG...
	emitByte(0x80);

	alignDword();
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

	alignDword();

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

	alignDword();
}

void Output::reloc8(Literal *lit)
{
	unsigned int insn;
	unsigned int target_addr;

	// If the literal has already been coded elsewhere, we use that
	// address. Otherwise we use the current output pointer.
	if (lit->coded_at)
		target_addr = lit->coded_at;
	else
		target_addr = addr;

	DEBUG("reloc8 reloc 0x%x target_addr 0x%x val 0x%x\n", lit->reloc, target_addr,
	      lit->val);
	long cur = ftell(fp);
	fseek(fp, lit->reloc - 0x8000000, SEEK_SET);
	fread(&insn, 1, 2, fp);
	//printf("oinsn 0x%x\n", insn);

	unsigned int pc = (lit->reloc + 4) & (~3);
	DEBUG("reloc8 offset 0x%x\n", (target_addr - pc) >> 2);

	assert((target_addr - pc) >> 2 < 256);
	insn |= (target_addr - pc) >> 2;

	//printf("ninsn 0x%x\n", insn);
	fseek(fp, lit->reloc - 0x8000000, SEEK_SET);
	fwrite(&insn, 1, 2, fp);
	fseek(fp, cur, SEEK_SET);
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

void Output::reloc24(unsigned int addr, unsigned int target)
{
	unsigned int insn;
	long cur = ftell(fp);
	fseek(fp, addr - 0x8000000, SEEK_SET);
	fread(&insn, 1, 4, fp);

	insn &= 0xff000000;
	insn |= ((target - addr - 8) >> 2) & 0x00ffffff;

	fseek(fp, addr - 0x8000000, SEEK_SET);
	fwrite(&insn, 1, 4, fp);
	fseek(fp, cur, SEEK_SET);
}

void Output::reloc10(unsigned int addr, unsigned int target)
{
	unsigned short insn;
	long cur = ftell(fp);
	if (addr < 0x8000000) {
		// Translate from IWRAM to ROM address.
		addr += this->addr - iwaddr;
		target += this->addr - iwaddr;
	}
	fseek(fp, addr - 0x8000000, SEEK_SET);
	fread(&insn, 1, 2, fp);

	insn &= 0xf800;
	int off = (target - addr - 4);
	assert(abs(off) < 2048);
	insn |= (off >> 1) & 0x7ff;

	fseek(fp, addr - 0x8000000, SEEK_SET);
	fwrite(&insn, 1, 2, fp);
	fseek(fp, cur, SEEK_SET);
}

void Output::reloc8(unsigned int addr, unsigned int target)
{
	unsigned short insn;
	long cur = ftell(fp);
	if (addr < 0x8000000) {
		// Translate from IWRAM to ROM address.
		addr += this->addr - iwaddr;
		target += this->addr - iwaddr;
	}
	fseek(fp, addr - 0x8000000, SEEK_SET);
	fread(&insn, 1, 2, fp);

	insn &= 0xff00;
	int off = target - addr - 4;
	insn |= (off >> 1) & 0xff;

	DEBUG("reloc8 from 0x%x to 0x%x insn 0x%x\n", addr, target, insn);
	assert(abs(off) < 256);

	fseek(fp, addr - 0x8000000, SEEK_SET);
	fwrite(&insn, 1, 2, fp);
	fseek(fp, cur, SEEK_SET);
}

void Output::registerIwram(Symbol *iwsym)
{
        if (currently_iwram) {
                // Add this word to IWRAM table and get the effective
                // address in return.  This is the address that we
                // will use to call this word.
                iwsym->addr = addIwram(iwsym->addr,
                                            addr - iwsym->addr);

                clearIwram();

                char id[strlen(iwsym->word) + 7];
                sprintf(id, "%s_iwram", iwsym->word);
                addSym(id, iwsym->addr);
        }
}

void Parser::code(unsigned int insn)
{
	assert(!thumb);
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
	last_insn = insn;
}

void Parser::code16(unsigned short insn)
{
	assert(thumb);
	if (cur_icode) {
		Icode *i = cur_icode;
		char *p = i->cp;
		*p++ = insn & 0xff;
		*p++ = (insn >> 8) & 0xff;
		i->cp = p;
		i->len += 2;
	} else {
		out->emitWord(insn);
	}
	last_insn = insn;
}

#define W(x) !strcmp((x), word)

enum asm_words {
	ASM_REG = 0,
	ASM_OFF = 1,
	ASM_DIR = 2,
	ASM_COND = 3,
	ASM_IMM = 4,
	ASM_AMODE = 5,
	ASM_RELOC = 6,
};

enum asm_relocs {
	RELOC_8 = 0,
	RELOC_10 = 1,
	RELOC_24 = 2,
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

	OP_LDRH  = 0x001000b0,
	OP_LDRSH = 0x001000f0,
	OP_STRH  = 0x000000b0,
};

enum asm_thumb_ops {
	TOP_AND = 0x0 << 6,
	TOP_EOR = 0x1 << 6,
	TOP_LSL = 0x2 << 6,
	TOP_LSR = 0x3 << 6,
	TOP_ASR = 0x4 << 6,
	TOP_ADC = 0x5 << 6,
	TOP_SBC = 0x6 << 6,
	TOP_ROR = 0x7 << 6,
	TOP_TST = 0x8 << 6,
	TOP_NEG = 0x9 << 6,
	TOP_CMP = 0xa << 6,
	TOP_CMN = 0xb << 6,
	TOP_ORR = 0xc << 6,
	TOP_MUL = 0xd << 6,
	TOP_BIC = 0xe << 6,
	TOP_MVN = 0xf << 6,
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
	strcpy(asm_text[asp], word); \
	asm_stack[asp][0] = (x); asm_stack[asp++][1] = (y); \
} while (0)

// next-of-stack type/value macros
#define NOS_TYPE (asm_stack[asp - 1][0])
#define NOS_VAL (asm_stack[asp - 1][1])
// top-of-stack type/value macros
#define TOS_TYPE (asm_stack[asp][0])
#define TOS_VAL (asm_stack[asp][1])
// popped type/value macros
#define POP_TYPE (asm_stack[--asp][0])
#define POP_VAL (asm_stack[--asp][1])

// pop with type checking
#define POP_VAL_TYPE(x) (--asp, assert(TOS_TYPE == (x)), TOS_VAL)
#define POP_REG POP_VAL_TYPE(ASM_REG)
#define POP_TREG (--asp, assert(TOS_TYPE == ASM_REG && TOS_VAL < 8), TOS_VAL)
#define POP_IMM POP_VAL_TYPE(ASM_IMM)

#define ASSERT_REG do { assert(TOS_TYPE == ASM_REG); } while (0)
#define ASSERT_TREG do { assert(TOS_TYPE == ASM_REG && TOS_VAL < 8); } while (0)
#define ASSERT_IMM do { assert(TOS_TYPE == ASM_IMM); } while (0)

static unsigned int immrot(unsigned int val, int *err)
{
	int ror = 0;

	if (val > 0xff) while (!(val & 3)) {
			ror--;
			val >>= 2;
	}
	if (err) {
		if (val > 0xff) {
			*err = 1;
		}
		else
			*err = 0;
	}
	return ((ror << 8) & 0xf00) | val;
}

static void immshift(unsigned int *val, int *lsl)
{
	*lsl = 0;
	if (*val > 0xff) {
		while (!(*val & 1)) {
			(*lsl)++;
			*val >>= 1;
		}
	}

	if (*val > 0xff)
		GLB_error("immediate 0x%x too complex\n", *val);
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
	int err;

	if (NOS_TYPE == ASM_AMODE) {
		switch (POP_VAL) {
		case AMODE_IMM:
			insn |= AMODE_IMM; /* immediate */
			insn |= immrot(POP_VAL_TYPE(ASM_IMM), &err);
			assert(!err);
			break;
		case AMODE_LSL:
		case AMODE_LSR:
		case AMODE_ASR:
		case AMODE_ROR:
			insn |= TOS_VAL;
			insn |= 1 << 4;	     /* register shift */
			insn |= POP_REG << 8;       /* Rs */
			insn |= POP_REG;	    /* Rm */
			break;
		case AMODE_LSLI:
		case AMODE_LSRI:
		case AMODE_ASRI:
		case AMODE_RORI:
			insn |= TOS_VAL;
			insn |= POP_IMM << 7;       /* shift_imm */
			insn |= POP_REG;	    /* Rm */
			break;
		default:
			GLB_error("unimp alu3 amode\n");
		}
	} else {
		/* no addressing mode specified -> RRR */
		insn |= POP_REG; /* Rm */
	}
	return insn;
}

unsigned int Parser::arm5CodeS()
{
	unsigned insn = 0;

	if (NOS_TYPE == ASM_AMODE &&
	    NOS_VAL == AMODE_FLAGS) {
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

	insn |= POP_REG << 12;      /* Rd */
	insn |= POP_REG << 16;      /* Rn */

	insn |= arm5CodeOp2();
	return insn;
}
#define CODE_ARM5_3OPS do { insn |= arm5Code3Ops(); code(insn); } while (0)

unsigned int Parser::arm5Code2Ops(unsigned int regbit)
{
	unsigned int insn = 0;

	insn |= armCodeCond();
	insn |= arm5CodeS();

	unsigned int rd = POP_REG;

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

	if (POP_TYPE == ASM_AMODE) {
		switch (TOS_VAL) {
		case AMODE_IND:
			break;

		case AMODE_PREIND:
		case AMODE_POSTIND:
			insn |= TOS_VAL;
			imm = POP_IMM;
			if (imm < 0x80000000)
				insn |= 0x00800000; /* upwards */
			else
				imm = -imm;
			insn |= imm;
			break;

		case AMODE_PREINDR:
		case AMODE_POSTINDR:
			insn |= TOS_VAL;
			insn |= 0x00800000;	/* upwards */
			if (NOS_TYPE == ASM_AMODE &&
			    NOS_VAL == AMODE_LSLI) {
				--asp;
				insn |= POP_IMM << 7;
			}
			insn |= POP_REG;
			break;

		default:
			GLB_error("unimp ARM.1x addressing mode\n");
		}
		insn |= POP_REG << 16;
	}

	return insn;
}

unsigned int Parser::arm10CodeAddr()
{
	unsigned int imm;
	unsigned int insn = 0;

	assert(NOS_TYPE == ASM_AMODE);
	switch (POP_VAL) {
	case AMODE_PREIND:
		/* Unfortunately the encoding in ARM.10 differs from ARM.9, so we cannot
		 * use the enum directly. */
		insn |= 0x01400000;
do_imm_ind:
		imm = POP_IMM;
		if (imm < 0x80000000)
			insn |= 1 << 23; /* upwards */
		else
			imm = -imm;
		insn |= (imm & 0xf);
		insn |= (imm & 0xf0) << 4;
		break;
	case AMODE_POSTIND:
		insn |= 0x00600000;
		goto do_imm_ind;
		break;
	case AMODE_PREINDR:
		insn |= 0x01800000;
		insn |= POP_REG;
		break;
	case AMODE_POSTINDR:
		insn |= 0x00a00000;
		insn |= POP_REG;
		break;
	case AMODE_IND:
		insn |= 0x00400000;
		break;
	default:
		GLB_error("internal error\n");
	}
	insn |= POP_REG << 16;
	return insn;
}

unsigned int Parser::armCodeCond()
{
	unsigned int insn = 0;

	if (NOS_TYPE == ASM_COND)
		insn |= POP_VAL;
	else
		insn |= COND_AL;

	return insn;
}
#define CODE_COND do { insn |= armCodeCond(); } while (0)
#define CODE_TCOND do { insn |= armCodeCond() >> 20; } while (0)

#define CODE_FLAGS do { \
		if (NOS_TYPE == ASM_AMODE && \
		    NOS_VAL == AMODE_FLAGS) { \
			insn |= 1 << 20; \
			--asp; \
		} \
} while (0)

#define CODE_RD do { \
		insn |= POP_REG << 12;  \
} while (0)

#ifndef NDEBUG
static bool can_branch(unsigned int from, unsigned int to)
{
	return abs(to - from) < (32 << 20);
}
static bool can_branch_thumb(unsigned int from, unsigned int to)
{
	return abs(to - from) < (4 << 20);
}
#endif

bool Parser::parseArm(const char *word)
{
	if (thumb)
		return false;
#define ARM53OP(str, op) \
	else if (!thumb && W(#str ",")) { \
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
		unsigned int dest = POP_VAL_TYPE(ASM_OFF);
		DEBUG("asm branch from 0x%x to 0x%x\n", out->addr, dest);
		assert(can_branch(out->addr, dest));
		code(insn | (((dest - out->addr - 8) >> 2) & 0x00ffffff));
	} else if (W("bx,")) {
		unsigned insn = 0x012fff10;
		CODE_COND;
		code(insn | POP_REG);
	} else if (W("ldm,") || W("stm,")) {
		/* format ARM.11 */
		unsigned int insn = 0;
		CODE_COND;
		while (POP_TYPE == ASM_REG)
			//printf("reg %d\n", TOS_VAL);
			insn |= 1 << TOS_VAL;
		if (word[0] == 'l')
			insn |= OP_LDM;
		else
			insn |= OP_STM;

		insn |= TOS_VAL;
		assert(TOS_TYPE == ASM_DIR);

		insn |= POP_REG << 16;
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
	} else if (W("ldrsh,")) {
		unsigned int insn = OP_LDRSH;
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
		insn |= 1 << POP_REG;
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
		insn |= POP_REG << 16;
		insn |= POP_REG;
		insn |= POP_REG << 8;
		code(insn);
	} else if (W("ret")) {
		unsigned int insn = 0x012fff1e;
		CODE_COND;
		code(insn);
	} else if (W("swp,")) {
		unsigned int insn = 0x01000090;
		CODE_COND;
		CODE_RD;
		insn |= POP_REG;
		insn |= POP_REG << 16;
		code(insn);
	} else if (W("pop")) {
		unsigned int insn = 0x08bd0000;
		CODE_COND;
		while (asp)
			insn |= 1 << POP_REG;
		code(insn);
	} else if (W("push")) {
		unsigned int insn = 0x092d0000;
		CODE_COND;
		while (asp)
			insn |= 1 << POP_REG;
		code(insn);
	} else if (W("swi,")) {
		unsigned int insn = 0x0f000000;
		CODE_COND;
		insn |= POP_IMM << 16;
		code(insn);
	} else if (W("unlink")) {
		unsigned int insn = 0xe4b70004;
		CODE_RD;
		code(insn);
	} else if (W("literal")) {
		unsigned int insn = 0x051f0000;
		CODE_COND;
		CODE_RD;
		literals.prependNew(POP_VAL, out->addr);
		assert(TOS_TYPE == ASM_IMM ||
		       TOS_TYPE == ASM_OFF);
		code(insn);
	} else if (W("movi")) {
		unsigned int rd = POP_REG;
		unsigned int val = POP_IMM;
		codeAsm(val, "##"); PUSH_ASM(ASM_REG, rd); codeAsm("mov,");
	} else
		return false;

	return true;
}

bool Parser::parseThumb(const char *word)
{
	if (!thumb)
		return false;

#define THUMBSHIFT(str, op) \
	else if (W(#str ",") && asp > 3 && \
		 asm_stack[asp - 3][0] == ASM_AMODE && \
		 asm_stack[asp - 3][1] == AMODE_IMM) { \
		unsigned short insn = op << 11; \
		insn |= POP_TREG << 0; \
		insn |= POP_TREG << 3; \
		--asp; \
		insn |= POP_IMM << 6; \
		assert(TOS_VAL < 32); \
		code16(insn); \
	}

	THUMBSHIFT(lsl, 0)
	THUMBSHIFT(lsr, 1)
	THUMBSHIFT(asr, 2)

#define THUMB4(str, op) \
	else if (W(#str ",")) { \
		unsigned short insn = 0x4000 | TOP_ ## op; \
		insn |= POP_TREG << 0; \
		insn |= POP_TREG << 3; \
		code16(insn); \
	}

	THUMB4(and, AND)
	THUMB4(eor, EOR)
	THUMB4(lsl, LSL)
	THUMB4(lsr, LSR)
	THUMB4(asr, ASR)
	THUMB4(adc, ADC)
	THUMB4(sbc, SBC)
	THUMB4(ror, ROR)
	THUMB4(tst, TST)
	THUMB4(neg, NEG)
	THUMB4(cmn, CMN)
	THUMB4(orr, ORR)
	THUMB4(mul, MUL)
	THUMB4(bic, BIC)
	THUMB4(mvn, MVN)

	else if (W("cmp,")) {
		unsigned short insn;
		unsigned int rd = POP_REG;
		if (POP_TYPE == ASM_AMODE &&
		    TOS_VAL == AMODE_IMM) {
			insn = 0x2800;
			insn |= POP_VAL << 0;
			assert(TOS_VAL < 256);
			insn |= rd << 8;
		} else {
			unsigned int rs = TOS_VAL;
			ASSERT_REG;
			if (rd < 8 && rs < 8) {
				insn = 0x4000 | TOP_CMP;
				insn |= rd << 0;
				insn |= rs << 3;
			} else {
				insn = 0x4500;
				insn |= !!(rd & 8) << 7;
				insn |= !!(rs & 8) << 6;
				insn |= (rd & 7) << 0;
				insn |= (rs & 7) << 3;
			}
		}
		code16(insn);
	} else if ((W("sub,") || W("add,"))) {
		unsigned short insn;
		unsigned int rd = POP_REG;
		if (POP_TYPE == ASM_AMODE &&
		    TOS_VAL == AMODE_IMM) {
			assert(rd < REG_R8 || rd == REG_SP);
			if (rd == REG_SP) {
				insn = 0xb000;
				if (word[0] == 's')
					insn |= 0x80;
				insn |= POP_IMM >> 2;
				assert(TOS_VAL < 512);
			} else {
				insn = 0x3000;
				if (word[0] == 's')
					insn |= 0x800;
				insn |= rd << 8;
				insn |= POP_IMM;
				assert(TOS_VAL < 256);
			}
		} else if (word[0] == 'a' && TOS_TYPE == ASM_REG &&
			   (rd >= REG_R8 || TOS_VAL >= REG_R8)) {
			if (!asp) {
				insn = 0x4400;
				insn |= (rd & 7) << 0;
				insn |= (TOS_VAL & 7) << 3;
				insn |= !!(rd & 8) << 7;
				insn |= !!(TOS_VAL & 8) << 6;
			} else {
				assert(TOS_VAL == REG_SP || TOS_VAL == REG_PC);
				insn = 0xa000;
				if (TOS_VAL == REG_SP)
					insn |= 0x800;
				insn |= POP_IMM >> 2;
				assert(TOS_VAL < 1024);
			}
		} else {
			assert(rd < REG_R8);
			insn = 0x1800 | (rd << 0);
			if (word[0] == 's')
				insn |= 1 << 9;
			insn |= TOS_VAL << 3;
			ASSERT_TREG;
			if (POP_TYPE == ASM_AMODE) {
				assert(TOS_VAL == AMODE_IMM);
				insn |= 1 << 10;
				insn |= POP_VAL << 6;
				assert(TOS_VAL < 8);
			} else {
				insn |= TOS_VAL << 6;
				ASSERT_TREG;
			}
		}
		code16(insn);
	} else if (W("ldr,") || W("str,") || W("ldrb,") || W("strb,")) {
		unsigned short insn;
		unsigned int offset;
		unsigned int rd = POP_TREG;
		--asp;
		assert(TOS_TYPE == ASM_AMODE);
		switch (TOS_VAL) {
			case AMODE_IND:
				--asp;
				ASSERT_REG;
				if (TOS_VAL == REG_SP) {
					assert(word[3] != 'b');
					// XXX: This is rarely used and pretty much untested.
					insn = 0x9000;
					if (word[0] == 'l')
						insn |= 1 << 11;
					insn |= rd << 8;
				} else {
					ASSERT_TREG;
					insn = 0x6000;
					if (word[0] == 'l')
						insn |= 0x800;
					if (word[3] == 'b')
						insn |= 0x1000;
					insn |= rd << 0;
					insn |= TOS_VAL << 3;
				}
				break;

			case AMODE_PREIND:
				--asp;
				ASSERT_IMM;
				assert(word[3] == 'b' || !(TOS_VAL & 3));
				offset = TOS_VAL;
				if (word[3] != 'b')
					offset >>= 2;
				if (POP_VAL == REG_SP) {
					assert(word[3] != 'b');
					assert(offset < 256);
					insn = 0x9000;
					if (word[0] == 'l')
						insn |= 1 << 11;
					insn |= rd << 8;
					insn |= offset;
				} else if (TOS_VAL == REG_PC) {
					assert(word[3] != 'b');
					assert(offset < 256);
					insn = 0x4800;
					// only exists as load
					assert(word[0] != 's');
					insn |= rd << 8;
					insn |= offset;
				} else {
					assert(offset < 32);
					insn = 0x6000;
					if (word[0] == 'l')
						insn |= 0x800;
					if (word[3] == 'b')
						insn |= 0x1000;
					insn |= TOS_VAL << 3;
					ASSERT_TREG;
					insn |= rd << 0;
					insn |= offset << 6;
				}
				break;

			case AMODE_PREINDR:
				--asp;
				ASSERT_TREG;
				insn = 0x5000;
				if (word[0] == 'l')
					insn |= 0x800;
				if (word[3] == 'b')
					insn |= 0x400;
				insn |= TOS_VAL << 6;	// Ro
				insn |= POP_TREG << 3;	// Rb
				insn |= rd << 0;
				break;

			default:
				GLB_error("internal error thumb ldst\n");
		}
		code16(insn);
	} else if (W("ldrh,") || W("strh,")) {
		unsigned short insn;
		unsigned int rd = POP_TREG;
		--asp;
		assert(TOS_TYPE == ASM_AMODE);
		if (TOS_VAL == AMODE_PREIND ||
		    TOS_VAL == AMODE_IND) {
			insn = 0x8000;
			if (word[0] == 'l')
				insn |= 0x800;
			if (TOS_VAL == AMODE_PREIND) {
				insn |= (POP_VAL >> 1) << 6;
				assert(TOS_VAL < 64);
			}
			insn |= POP_TREG << 3;
		} else if (TOS_VAL == AMODE_PREINDR) {
			insn = 0x5200;
			if (word[0] == 'l')
				insn |= 0x800;
			insn |= POP_TREG << 6;	// offset
			insn |= POP_TREG << 3;	// base
		} else
			GLB_error("invalid addressing mode on %s\n", word);
		insn |= rd << 0;
		code16(insn);
	} else if ((W("ldm,") || W("stm,"))) {
		// XXX: unused and thus entirely untested
		unsigned short insn = 0xc000;
		while (POP_TYPE == ASM_REG) {
			//printf("reg %d\n", TOS_VAL);
			insn |= 1 << TOS_VAL;
			ASSERT_TREG;
		}

		if (word[0] == 'l')
			insn |= 1 << 11;

		insn |= TOS_VAL;
		assert(TOS_TYPE == ASM_DIR &&
		       TOS_VAL == DIR_IAW);

		insn |= POP_TREG << 8;
		code16(insn);
	} else if (W("bl,")) {
		unsigned short insn1 = 0xf000;
		unsigned short insn2 = 0xf800;
		unsigned int dest = POP_VAL;
		assert(TOS_TYPE == ASM_OFF);
		DEBUG("asm thumb branch from 0x%x to 0x%x\n", out->ta(), dest);
		assert(can_branch_thumb(out->ta(), dest));
		int soff = (dest - out->ta() - 4) >> 1;
		unsigned int off1 = (soff >> 11) & ((1 << 11) -1);
		unsigned int off2 = soff & ((1 << 11) - 1);
		DEBUG("off1 0x%x off2 0x%x\n", off1, off2);
		code16(insn1 | off1);
		code16(insn2 | off2);
	} else if (W("b,")) {
		unsigned short insn;
		if (NOS_TYPE == ASM_COND) {
			insn = 0xd000;
			CODE_TCOND;
			unsigned int dest = POP_VAL;
			assert(TOS_TYPE == ASM_RELOC || TOS_TYPE == ASM_OFF);
			if (TOS_TYPE == ASM_RELOC) {
				asm_relocs[dest].reloc = RELOC_8;
				dest = out->ta();
			}
			DEBUG("asm thumb cond branch from 0x%x to 0x%x\n", out->ta(), dest);
			int soff = dest - out->ta() - 4;
			assert(soff >= -256 && soff < 256);
			insn |= (soff >> 1) & 0xff;
		} else {
short_branch:
			insn = 0xe000;
			unsigned int dest = POP_VAL;
			assert(TOS_TYPE == ASM_RELOC || TOS_TYPE == ASM_OFF);
			if (TOS_TYPE == ASM_RELOC) {
				asm_relocs[dest].reloc = RELOC_10;
				dest = out->ta();
			}
			DEBUG("asm thumb uncond branch from 0x%x to 0x%x\n", out->ta(), dest);
			int soff = dest - out->ta() - 4;
			assert(soff >= -2048 && soff < 2048);
			insn |= (soff >> 1) & 0x7ff;
		}
		code16(insn);
	} else if (W("jmp")) {
		/* unconditional branch of arbitrary distance */
		assert(NOS_TYPE == ASM_OFF);
		int soff = NOS_VAL - out->addr - 4;
		if (soff >= -2048 && soff < 2048)
			goto short_branch;
		codeAsm(POP_VAL + 1, "r2", "literal");
		codeAsm("r2", "bx,");
	} else if (W("lcallt")) {
		unsigned int dest = POP_VAL_TYPE(ASM_OFF);

		codeAsm(dest+1, "r2", "literal");
		if ((out->ta() & 0xff000000) == 0x03000000)
		        codeBranch(RT__thumbthunk_iwram, "bl,");
                else
			codeBranch(RT__thumbthunk, "bl,");
	} else if (W("bx,")) {
		unsigned short insn = 0x4700;
		code16(insn | (POP_REG << 3));
	} else if (W("mov,")) {
		unsigned short insn;
		unsigned int rd = POP_REG;
		if (POP_TYPE == ASM_REG) {
			unsigned int rs = TOS_VAL;
			if (rs > 7 || rd > 7) {
				insn = 0x4600;
				insn |= rs << 3;
				insn |= rd & 7;
				insn |= (!!(rd & 8)) << 7;
			} else {
				// So gas assembles "mov r0, r1" to a
				// Thumb.2 insn, but objdump disassembles
				// that as "adds r0, r1, #0", whereas it
				// disassembles Thumb.1 insn "lsls r0, r1,
				// #0" as "mov r0, r1", which NO$GBA
				// disassembles as lsls.  The ARM ARM
				// doesn't even comment on what a canonical
				// Thumb movs might be...
				insn = 0x1c00;
				insn |= rs << 3;
				insn |= rd << 0;
			}
		} else if (TOS_TYPE == ASM_AMODE &&
			   POP_TYPE == ASM_IMM) {
			assert(rd < REG_R8);
			insn = 0x2000;
			unsigned int imm = TOS_VAL;
			assert(imm < 256);
			insn |= rd << 8;
			insn |= imm;
		} else
			GLB_error("invalid thumb mov");
		code16(insn);
	} else if (W("pop")) {
		unsigned short insn = 0xbc00;
		while (asp) {
			insn |= 1 << POP_TREG;
		}
		code16(insn);
	} else if (W("push")) {
		unsigned short insn = 0xb400;
		while (asp) {
			if (POP_VAL_TYPE(ASM_REG) == REG_LR)
				insn |= 0x100;
			else {
				insn |= 1 << TOS_VAL;
				ASSERT_TREG;
			}
		}
		code16(insn);
	} else if (W("swi,")) {
		unsigned short insn = 0xdf00;
		insn |= POP_IMM;
		assert(TOS_VAL < 256);
		code16(insn);
	} else if (W("literal")) {
		unsigned short insn = 0x4800;
		insn |= POP_TREG << 8;
		literals.prependNew(POP_VAL, out->addr, true);
		assert(TOS_TYPE == ASM_IMM ||
		       TOS_TYPE == ASM_OFF);
		code16(insn);
	} else if (W("movi")) {
		unsigned int rd = POP_TREG;
		unsigned int val = POP_IMM;
		int lsl;
		immshift(&val, &lsl);
		codeAsm(val, "##"); PUSH_ASM(ASM_REG, rd); codeAsm("mov,");
		if (lsl) {
			codeAsm(lsl, "##"); PUSH_ASM(ASM_REG, rd);
			PUSH_ASM(ASM_REG, rd); codeAsm("lsl,");
		}
	} else if (W("tothumb")) {
		// We have been called as ARM, but want to be Thumb.
		// Used by assembly language interrupt handlers.
		assert(!(out->addr & 3));
		thumb = false;
		codeAsm("1", "##", "pc", "r0", "add,");
		codeAsm("r0", "bx,");
		thumb = true;
	} else if (W("ret")) {
		/* BX LR */
		code16(0x4700 | (REG_LR << 3));
	} else
		return false;

	return true;
}

void Parser::parseAsm(const char *word)
{
	Symbol *sym;
#ifndef NDEBUG
	int old_asp = asp;
	unsigned int old_addr = out->addr;
#endif

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

	else if (parseThumb(word) || parseArm(word)) {
		assert(asp == 0);
#ifndef NDEBUG
		unsigned int addr;
		if (cur_icode)
			addr = cur_icode->cp - cur_icode->code;
		else
			addr = old_addr;
		DEBUG("%07X  ", addr);
		if (thumb)
			DEBUGN("    %04x", last_insn);
		else
			DEBUGN("%08x", last_insn);
		DEBUGN("  %s", word);
		for (int i = old_asp - 1; i >= 0; i--) {
			DEBUGN(" %s", asm_text[i]);
		}
		DEBUGN("\n");
#endif
	} else if (W("l:")) {
		const char *label = getNextWord();
		asm_labels[lsp].label = strdup(label);
		asm_labels[lsp++].addr = out->ta();
		DEBUG("asm label %s at 0x%x\n", asm_labels[lsp - 1].label,
		      out->ta());

		// Resolve forward references to this label, if any.
		for (int i = 0; i < rsp; i++) {
			if (asm_relocs[i].label &&
			    !strcmp(asm_relocs[i].label, label)) {
				switch (asm_relocs[i].reloc) {
				case RELOC_8:
					out->reloc8(asm_relocs[i].addr, out->ta());
					break;
				case RELOC_10:
					out->reloc10(asm_relocs[i].addr, out->ta());
					break;
				case RELOC_24:
					out->reloc24(asm_relocs[i].addr, out->ta());
					break;
				default:
					abort();
				}
				delete asm_relocs[i].label;
				asm_relocs[i].label = NULL;
			}
		}
	} else if (isNum(word)) {
		unsigned int imm = TIN_parseNum(word);
		PUSH_ASM(ASM_IMM, imm);
	} else if (W("#offset")) {
		assert(NOS_TYPE == ASM_IMM);
		NOS_TYPE = ASM_OFF;
		NOS_VAL += out->ta();

	// XXX: there should be a list of constants
	} else if (W("backbuffer")) {
		PUSH_ASM(ASM_IMM, 0xa000);
	} else if (W("bg_regs")) {
		PUSH_ASM(ASM_IMM, 0x400);
	} else if (W("bg0_sx")) {
		PUSH_ASM(ASM_IMM, 0);
	} else if (W("globals")) {
		PUSH_ASM(ASM_IMM, 0x600);
	} else if (W("iwram_globals")) {
		PUSH_ASM(ASM_IMM, 0x3000600);
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
	} else if (W("string_ring")) {
		PUSH_ASM(ASM_IMM, RT___ewram_start - 4096);
	} else if (W("string_ring_size")) {
		PUSH_ASM(ASM_IMM, 4096);
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

	} else if ((sym = getSymbol(word))) {
		DEBUG("asmsym %s 0x%x thumb %d\n", word, sym->addr, sym->thumb);
		if (sym->is_addr)
			PUSH_ASM(ASM_OFF, sym->lit_addr);
		else
			PUSH_ASM(ASM_OFF, sym->addr);
	} else {
		int i;
		for (i = lsp - 1; i >= 0; i--) {
			if (!strcmp(asm_labels[i].label, word)) {
				PUSH_ASM(ASM_OFF, asm_labels[i].addr);
				return;
			}
		}

		// No label found.  We expect it to be resolved later.

		// We push an ASM_RELOC on the stack.  It contains the
		// relocation index that allows the instruction implementation
		// to set the appropriate reloc type.
		PUSH_ASM(ASM_RELOC, rsp);

		// Remember that there will be an instruction at out->ta()
		// that needs to be relocated to "word" once the latter is
		// defined.
		asm_relocs[rsp].label = strdup(word);
		asm_relocs[rsp].addr = out->ta();
		rsp++;
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
	char word[64];
	sprintf(word, "0x%x", w0);
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
	char word[64];
	sprintf(word, "0x%x", w1);
	PUSH_ASM(ASM_IMM, w1);
	codeAsm(w2, w3, w4);
}

void Parser::codeBranch(unsigned int dest, const char *mnem)
{
	char word[64];
	sprintf(word, "0x%x", dest);
	PUSH_ASM(ASM_OFF, dest);
	codeAsm(mnem);
}

void Parser::codeBranch(unsigned int dest, const char *cond, const char *mnem)
{
	char word[64];
	sprintf(word, "0x%x", dest);
	PUSH_ASM(ASM_OFF, dest);
	codeAsm(cond, mnem);
}

void Parser::codeToThumb(bool save_lr)
{
	if (!thumb) {
		out->addSym(".arm");
		if (save_lr) {
			codeAsm("r0", "push");
			codeAsm("1", "##", "pc", "r0", "add,");
			codeAsm("r0", "bx,");
			thumb = true;
			out->addSym(".thumb");
			codeAsm("r0", "pop");
		} else {
			codeAsm("1", "##", "pc", "lr", "add,");
			codeAsm("lr", "bx,");
			out->addSym(".thumb");
			thumb = true;
		}
	}
}

void Parser::codeToArm()
{
	if (thumb) {
		// According to specs, this is UNPREDICTABLE when not
		// 32-bit aligned, but it seems to work fine on the GBA...
		//if (out->addr & 3)
		//	code16(0x46c0);	// NOP
		codeAsm("pc", "bx,");
		thumb = false;
		out->alignDword();
		out->addSym(".thumb");
	}
}

void Parser::codeCallThumb(unsigned int dest, const char *ident)
{
	if (thumb) {
		if ((out->ta() & 0xff000000) != (dest & 0xff000000)) {
		        if ((out->ta() & 0xff000000) == 0x03000000)
		                GLB_warning("call from IWRAM to %s in ROM\n", ident);
			codeBranch(dest, "lcallt");
		} else
			codeBranch(dest, "bl,");
	} else {
		codeAsm("pc", "4", "#(", "r5", "ldr,"); // load function address
		codeAsm("pc", "lr", "mov,");	    // sets LR to skip the address when returning
		codeAsm("r5", "bx,");
		code(dest | 1);			 // address with bit 0 set for Thumb mode
	}
}

void Parser::codeCallArm(unsigned int dest)
{
	if (thumb) {
		// Worst. Instruction set. Ever.
		codeToArm();
		codeAsm("1", "##", "pc", "lr", "add,");
		codeBranch(dest, "b,");
		thumb = true;
	} else {
		codeBranch(dest, "bl,");
	}
}

const char *op2cond(const char *word, bool invert)
{
	if (W(">"))
		return !invert ? "gt?" : "le?";
	else if (W(">="))
		return !invert ? "ge?" : "lt?";
	else if (W("<"))
		return !invert ? "lt?" : "ge?";
	else if (W("<="))
		return !invert ? "le?" : "gt?";
	else if (W("<>"))
		return !invert ? "ne?" : "eq?";
	else if (W("="))
		return !invert ? "eq?" : "ne?";
	abort();
}

void Parser::loadR5(unsigned int num)
{

	if (!r5_const || r5 != num) {
		if (r5_const && num > r5 && num - r5 < 0x100) {
			DEBUG("loaded r5 with 0x%x by adding 0x%x\n", num, num - r5);
			codeAsm(num - r5, "##", "r5", "add,");
		} else if (r5_const && num < r5 && r5 - num < 0x100) {
			DEBUG("loaded r5 with 0x%x by subbing 0x%x\n", num, r5 - num);
			codeAsm(r5 - num, "##", "r5", "sub,");
		} else if (can_immrot(num)) {
			DEBUG("loaded r5 with 0x%x using movi\n", num);
			codeAsm(num, "r5", "movi");
		} else if (can_immrot(num & 0xffffff00)) {
			DEBUG("loaded r5 with 0x%x using movi/add\n", num);
			codeAsm(num & 0xffffff00, "r5", "movi");
			codeAsm(num & 0xff, "##", "r5", "add,");
		} else {
			DEBUG("loaded r5 with 0x%x using ldr pc\n", num);
			literals.prependNew(num, out->addr, thumb);
			codeAsm("pc", "0", "#(", "r5", "ldr,");
		}
		setR5(num);
	} else {
		DEBUG("loaded r5 NOT, already at 0x%x\n", num);
	}
}

void Parser::parseAll()
{
	const char *word;
	Symbol *sym;
	Icode *icode;
	bool currently_naked = false;
	Symbol *iwsym;
	// R5 is often repeatedly loaded with the same constant.  We keep
	// track of its current value to avoid unnecessary reloading.
	invalR5();
	unsigned int word_start = out->addr;
	unsigned int local_idx = 0;
	unsigned int num;
	bool skip_push = false;
	const char *cond;

parse_next:
	if (thumb && out->addr - word_start > 0x380 && literals.next) {
		unsigned int branch = out->addr;
		code16(0);
		literals.code(out);
		out->patch16(branch, 0xe000 | ((out->addr - branch - 4) >> 1));
		word_start = out->addr;
	}

	word = getNextWord();
	if (!word) {
		while (sp > 0) {
			popText();
			word = getNextWord();
			if (word)
				break;
		}
		if (!word) {
			// DBC does not end "start" with a semicolon :/
			literals.code(out);
			return;
		}
	}

	if (W("requires\"")) {
		pushText(getNextWord());
	} else if (W("import\"")) {
		out->emitBinary(getNextWord());
	} else if (W("bitmap\"")) {
		out->emitBitmap(getNextWord());
	} else if (W("map\"")) {
		out->emitMap(getNextWord());
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
	} else if (W("icode") || W("icode-thumb") || W(":i")) {
		cur_icode = icodes.appendNew(getNextWord());
		DEBUG("===start icode %s\n", cur_icode->word);
		if (word[0] != ':')
			asm_mode = true;
		if ((word[5] == '-' && word[6] == 't') || word[0] == ':') {
			thumb = true;
			cur_icode->thumb = true;
		} else {
			thumb = false;
			cur_icode->thumb = false;
		}
		invalR5();
	} else if (W("code") || W("code-thumb")) {
		out->alignDword();
		if (word[4] == '-' && word[5] == 't')
			thumb = true;
		else
			thumb = false;

		if (getNextWordIf("iwram"))
			out->setIwram();
		else
			out->clearIwram();

		const char *ident = getNextWord();
		out->addSym(ident);
		sym = symbols.appendNew(out->addr, ident);

		// Save pointer to symbol; we need it later to create the
		// IWRAM table entry.
		if (out->currently_iwram)
			iwsym = sym;
		else
			iwsym = NULL;

		DEBUG("===start %s %s at 0x%x\n", word, sym->word, sym->addr);
		sym->thumb = thumb;
		asm_mode = true;
		invalR5();
		word_start = out->addr;
	} else if (W("end-code")|| (W(";") && cur_icode)) {
		if (cur_icode) {
			cur_icode = 0;
			if (literals.next)
				GLB_error("literals not allowed in inlined code\n");
		}
		asm_mode = false;
		literals.code(out);
		for (int i = 0; i < rsp; i++) {
			if (asm_relocs[i].label)
				GLB_error("unresolved symbol %s\n",
					  asm_relocs[i].label);
		}
		lsp = 0;
		rsp = 0;
		invalR5();
		out->registerIwram(iwsym);
	} else if (asm_mode) {
		parseAsm(word);
	} else if (W(":") || W(":n")) {
		out->alignDword();
		invalR5();
		currently_naked = word[1] == 'n';
		const char *ident = getNextWord();
		sym = symbols.appendNew(out->addr, ident);
		out->addSym(ident);
		DEBUG("===start word %s at 0x%x\n", sym->word, sym->addr);
		if (!strcmp(ident, "start")) {
			// Runtime assumes that "start" is an ARM word, so we
			// have to code a switch to Thumb.
			sym->thumb = false;
			thumb = false;
			codeToThumb();
			if (getSymbol(".init_string_ring"))
				codeAsm(".init_string_ring", "bl,");
		} else {
			thumb = true;
			sym->thumb = true;
			out->addSym(".thumb");
		}
		if (!currently_naked) {
			sym->has_prolog = true;
			codeAsm("4", "##", "r7", "r7", "sub,");
			codeAsm("r7", "0@", "r6", "str,");
			// LPROLOG overwrites R6, no need to set it.
			if (!isWordN(1, "lprolog"))
				codeAsm("lr", "r6", "mov,");
		}
		word_start = out->addr;
		local_idx = 0;
	} else if (W("label")) {
		out->alignDword();
		sym = symbols.appendNew(out->addr, getNextWord());
		DEBUG("===start label %s at 0x%x\n", sym->word, sym->addr);
		thumb = false;
	} else if (W("local:")) {
		sym = symbols.appendNew(local_idx, getNextWord());
		sym->is_const = true;
		sym->lit_addr = local_idx;
		local_idx += 4;
	} else if (W(";r")) {
		// return from inside a routine's body
		assert(!cur_icode);
		if (currently_naked)
			codeAsm("lr", "bx,");
		else
			codeAsm("r6", "bx,");
	} else if (W(";l")) {
		// end of function without return
		assert(!cur_icode);
		invalR5();
		currently_naked = false;
		thumb = false;
		literals.code(out);
	} else if (W(";")) {
		assert(!cur_icode);
		invalR5();
		if (currently_naked)
			codeAsm("lr", "bx,");
		else
			codeAsm("r6", "bx,");
		currently_naked = false;
		thumb = false;
		literals.code(out);
	} else if (W("swap")) {
		if (getNextWordIf("a!")) {
			codeAsm("r1", "pop");
		} else if (getNextWordIf("-")) {
			codeAsm("r2", "pop");
			codeAsm("r2", "r0", "r0", "sub,");
		} else if (getNextWordIf("over")) {
			codeAsm("sp", "0@", "r2", "ldr,");
			codeAsm("r2", "push");
			codeAsm("sp", "4", "#(", "r0", "str,");
		} else if (getNextWordIf("!")) {
			codeAsm("r2", "r3", "pop");
			codeAsm("r2", "0@", "r0", "str,");
			codeAsm("r3", "r0", "mov,");
		} else {
			if (!thumb)
				codeAsm("sp", "r0", "r0", "swp,");
			else {
				codeAsm("r2", "pop");
				codeAsm("r0", "push");
				codeAsm("r2", "r0", "mov,");
			}
		}
	} else if (W("dup")) {
		if (getNextWordIf("a!")) {
			codeAsm("r0", "r1", "mov,");
		} else if (isWordN(0, "@") &&
			   isWordN(1, "1+") &&
			   isWordN(2, "swap") &&
			   isWordN(3, "!")) {
			// DBC INC x,1 idiom
			getNextWord();
			getNextWord();
			getNextWord();
			getNextWord();
			codeAsm("r0", "0@", "r2", "ldr,");
			codeAsm(1, "##", "r2", "add,");
			codeAsm("r0", "0@", "r2", "str,");
			codeAsm("r0", "pop");
		} else if (isWordN(0, "@") &&
			   isNum(peekWordN(1)) &&
			   isWordN(2, "#") &&
			   (isWordN(3, "-") || isWordN(3, "+")) &&
			   isWordN(4, "swap") &&
			   isWordN(5, "!")) {
			// DBC INC/DEC x,n idiom
			getNextWord();
			unsigned int num = TIN_parseNum(getNextWord());
			getNextWord();
			const char *op = getNextWord();
			getNextWord();
			getNextWord();
			codeAsm("r0", "0@", "r2", "ldr,");
			codeAsm(num, "##", "r2", op[0] == '+' ? "add," : "sub,");
			codeAsm("r0", "0@", "r2", "str,");
			codeAsm("r0", "pop");
		} else {
			codeAsm("r0", "push");
		}
	} else if (W("over")) {
		if (getNextWordIf("-")) {
			codeAsm("sp", "0@", "r2", "ldr,");
			codeAsm("r2", "r0", "r0", "sub,");
		} else if (getNextWordIf("!")) {
			codeAsm("r2", "pop");
			codeAsm("r2", "0@", "r0", "str,");
			codeAsm("r2", "r0", "mov,");
		} else if (isWordN(0, "@") && isWordN(1, "+")) {
			getNextWord();
			getNextWord();
			if (isWordN(0, "over") && isWordN(1, "!")) {
				getNextWord();
				getNextWord();
				codeAsm("r3", "pop");
				codeAsm("r3", "0@", "r2", "ldr,");
				codeAsm("r2", "r0", "r0", "add,");
				codeAsm("r3", "0@", "r0", "str,");
				codeAsm("r3", "r0", "mov,");
			} else {
				codeAsm("sp", "0@", "r2", "ldr,");
				codeAsm("r2", "0@", "r2", "ldr,");
				codeAsm("r2", "r0", "r0", "add,");
			}
		} else {
			codeAsm("r0", "push");
			codeAsm("sp", "4", "#(", "r0", "ldr,");
		}
	} else if (W("pop")) {
		codeAsm("r0", "push");
		codeAsm("r6", "r0", "mov,");
		codeAsm("r7", "ia!", "r6", "ldm,");
	} else if (W("push")) {
		if (!thumb)
			codeAsm("r7", "db!", "r6", "stm,");
		else {
			codeAsm("4", "##", "r7", "sub,");
			codeAsm("r7", "0@", "r6", "str,");
		}
		codeAsm("r0", "r6", "mov,");
		codeAsm("r0", "pop");
	} else if (W("0<")) {
		if (!thumb)
			codeAsm("r0", "31", "#asr", "r0", "mov,");
		else
			codeAsm("31", "##", "r0", "r0", "asr,");
	} else if (W("1+")) {
		codeAsm("1", "##", "r0", "r0", "add,");
	} else if (W("com")) {
		if (!thumb && getNextWordIf("1+")) {
			codeAsm("0", "##", "r0", "r0", "rsb,");
		} else {
			codeAsm("r0", "r0", "mvn,");
		}
	} else if (W("and")) {
		codeAsm("r2", "pop");
		codeAsm("r2", "r0", "and,");
	} else if (W("or")) {
		codeAsm("r2", "pop");
		codeAsm("r2", "r0", "orr,");
	} else if ((sym = getSymbol(word)) && sym->is_const) {
		num = sym->lit_addr;
		goto handle_const;
	} else if (isNum(word)) {
		num = TIN_parseNum(word);
handle_const:
		if (getNextWordIf("swi")) {
			codeAsm(num, "swi,");
		} else if (getNextWordIf("#")) {
			//printf("got imm 0x%x\n", num);
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
				} else {
					break;
				}
			}

			if (getNextWordIf("n/")) {
				if (!thumb) {
					codeAsm("r0");
					codeAsm(num, "#lsr", "r0", "mov,");
				} else
					codeAsm(num, "##", "r0", "r0", "lsr,");
			} else if (getNextWordIf("a/")) {
				if (!thumb) {
					codeAsm("r0");
					codeAsm(num, "#asr", "r0", "mov,");
				}
				else
					codeAsm(num, "##", "r0", "r0", "asr,");
			} else if (getNextWordIf("n*")) {
				if (!thumb && getNextWordIf("+")) {
					codeAsm("r2", "pop");
					codeAsm("r0");
					codeAsm(num, "#lsl", "r2", "r0",
						"add,");
				} else {
					if (!thumb) {
						codeAsm("r0");
						codeAsm(num, "#lsl", "r0", "mov,");
					} else
						codeAsm(num, "##", "r0", "r0", "lsl,");
				}
			} else if (num < 64 && isWordN(0, "+") && isWordN(1, "peek")) {
				getNextWord();
				getNextWord();
				codeAsm("r0", num, "#(", "r0", "ldrh,");
			} else if (num < 128 && isWordN(0, "+") && isWordN(1, "peekw")) {
				getNextWord();
				getNextWord();
				codeAsm("r0", num, "#(", "r0", "ldr,");
			} else if (num < 32 && isWordN(0, "+") && isWordN(1, "peekb")) {
				getNextWord();
				getNextWord();
				codeAsm("r0", num, "#(", "r0", "ldrb,");
			} else if (isWordN(0, "+") ||
				   isWordN(0, "-")) {
				const char *op;
				if (getNextWordIf("+"))
					op = "add,";
				else if (getNextWordIf("-"))
					op = "sub,";
				else
					abort();

				if (num != 0) {
					if (thumb && num < 256) {
						codeAsm(num, "##", "r0", op);
					} else if (!thumb && can_immrot(num)) {
						codeAsm(num, "##", "r0",
							"r0", op);
					} else {
						loadR5(num);
						codeAsm("r5", "r0", "r0", op);
					}

				}
			} else if (isPow2(num) && getNextWordIf("/")) {
				codeAsm(log2(num), "##", "r0", "r0", "asr,");
			} else if (isPow2(num) && getNextWordIf("*")) {
				codeAsm(log2(num), "##", "r0", "r0", "lsl,");
			} else if (num == 0 && getNextWordIf("or")) {
				/* nop */
			} else if (getNextWordIf("com")) {
				if (getNextWordIf("and")) {
					loadR5(num);
					codeAsm("r5", "r0", "bic,");
				} else if (getNextWordIf("1+")) {
					codeAsm("r0", "push");
					if (!thumb && can_immrot(num)) {
						codeAsm(num, "##", "r0",
							"mov,");
						codeAsm("0", "##", "r0", "r0",
							"rsb,");
					} else {
						literals.prependNew(-num, out->addr, thumb);
						codeAsm("pc", "0", "#(", "r0", "ldr,");
					}
				} else {
					codeAsm("r0", "push");
					if (!thumb && can_immrot(num)) {
						codeAsm(num, "##", "r0",
							"mvn,");
					} else {
						literals.prependNew(~num, out->addr, thumb);
						codeAsm("pc", "0", "#(", "r0", "ldr,");
					}
				}
			} else if (isWordN(0, "*") ||
				   isWordN(0, "and")) {
				const char *word = getNextWord();
				loadR5(num);
				if (W("*"))
					codeAsm("r5", "r0", "mul,");
				else if (W("and"))
					codeAsm("r5", "r0", "and,");
			} else if (!thumb && can_immrot(num) && getNextWordIf("and")) {
				codeAsm(num, "##", "r0", "r0", "and,");
			} else {
				//printf("misc num\n");
				if (isWordN(0, "+r") &&
				    (isWordN(1, "@") || isWordN(1, "!"))) {
					getNextWord();
					if (getNextWordIf("@")) {
						if (!skip_push && getNextWordIf("+")) {
							codeAsm("r6", num, "#(", "r2", "ldr,");
							codeAsm("r2", "r0", "r0", "add,");
						} else {
							// If a local store precedes,
							// we don't have to push R0.
							if (!skip_push)
								codeAsm("r0", "push");
							skip_push = false;
							codeAsm("r6", num, "#(", "r0", "ldr,");
						}
					} else if (getNextWordIf("!")) {
						codeAsm("r6", num, "#(", "r0", "str,");
						// If a local load follows, we
						// don't have to pop R0.
						if (isWordN(1, "#") &&
						    isWordN(2, "+r") &&
						    isWordN(3, "@"))
							skip_push = true;
						else
							codeAsm("r0", "pop");
					} else
						GLB_error("internal error\n");
				} else if (num > 0xff) {
					if (!can_immrot(num)) {
						codeAsm("r0", "push");
						literals.prependNew(num, out->addr, thumb);
						codeAsm("pc", "0", "#(", "r0", "ldr,");
					} else {
						codeAsm("r0", "push");
						codeAsm(num, "r0", "movi");
					}
				} else {
					DEBUG("small num 0x%x\n", num);
					if (isWordN(0, "over") && isWordN(1, "!")) {
						getNextWord();
						getNextWord();
						loadR5(num);
						codeAsm("r0", "0@", "r5", "str,");
					} else if ((isWordN(0, "=") ||
						    isWordN(0, "<=") ||
						    isWordN(0, ">=") ||
						    isWordN(0, "<") ||
						    isWordN(0, ">") ||
						    isWordN(0, "<>")) &&
						   (isWordN(1, "if") || isWordN(1, "while"))) {
						const char *op = getNextWord();
						getNextWord();
						cond = op2cond(op, !thumb);
						codeAsm(num, "##", "r0", "cmp,");
						goto do_ifwhile;
					} else {
						codeAsm("r0", "push");
						codeAsm(num, "##", "r0", "mov,");
					}
				}
			}
		} else if (getNextWordIf(",")) {
			code(num);
		} else if (getNextWordIf("h,")) {
			out->emitWord(num);
		} else if (getNextWordIf("b,")) {
			out->emitByte(num);
		} else if (getNextWordIf("lprolog")) {
			assert(!currently_naked);
			codeAsm("r0", "push");	// last param to stack
			// Reserve space for additional local variables.
			if (num)
				codeAsm(num, "##", "sp", "sub,");
			// Set up R6 as base pointer.
			codeAsm("sp", "r6", "mov,");
			// Save return address.
			codeAsm("lr", "push");
			// NB: We now have a bogus value in TOS that we
			// have to discard in the epilog!
		} else if (getNextWordIf("lepilog")) {
			// Restore return address.
			codeAsm("r6", "pop");
			// Purge locals from stack...
			codeAsm(num, "##", "sp", "add,");
			// ...including bogus TOS.
			codeAsm("r0", "pop");
		} else if (getNextWordIf("flepilog")) {
			// Here, the user stack looks like this:
			// TOS		return value
			// SP+0		bogus TOS
			// SP+4		return address
			// SP+8 etc.	locals/arguments

			// Dispose of bogus TOS and restore return address.
			codeAsm("r2", "r6", "pop");
			// Purge locals and arguments from stack...
			codeAsm(num, "##", "sp", "add,");
			// ...and keep TOS because it contains the return
			// value.
		} else {
			GLB_error("unimp num\n");
		}
	} else if (W("+r")) {
		codeAsm("r0", "r6", "r0", "add,");
	} else if (W("c\"")) {
		unsigned int end_str;
		const char *str = getNextWord();
		codeAsm("r0", "push");
		codeAsm("pc", "r0", "mov,");
		unsigned int skip = out->addr;
		codeBranch(out->addr, "b,");
		end_str = out->addr + 4 + 1 + strlen(str);
#ifdef BUG_FOR_BUG
		// This emulates the behavior of the legacy compiler
		// to add another zero dword even if end_str is
		// already aligned.
		end_str++;
#endif
		end_str = (end_str + 3) & ~3;
		out->addSym(".byt:0001");
		out->emitByte(strlen(str));

		char dbgsym[10];
		sprintf(dbgsym, ".asc:%04X", (unsigned int)strlen(str));
		out->addSym(dbgsym);

		out->emitString(str, strlen(str));

		unsigned int pad_start = out->addr;
		while (out->addr < end_str)
			out->emitByte(0);
		if (out->addr > pad_start) {
			sprintf(dbgsym, ".byt:%04X", out->addr - pad_start);
			out->addSym(dbgsym, pad_start);
		}

		if (thumb)
			out->reloc10(skip, out->addr);
		else
			out->reloc24(skip, out->addr);
	} else if (W("drop")) {
		if (getNextWordIf("a@")) {
			codeAsm("r1", "r0", "mov,");
		} else if (getNextWordIf("over")) {
			codeAsm("sp", "4", "#(", "r0", "ldr,");
		} else {
			codeAsm("r0", "pop");
		}
	} else if (W("+")) {
		codeAsm("r2", "pop");
		codeAsm("r0", "r2", "r0", "add,");
	} else if (W("-")) {
		codeAsm("r2", "pop");
		codeAsm("r0", "r2", "r0", "sub,");
	} else if (W("*")) {
		codeAsm("r2", "pop");
		codeAsm("r2", "r0", "mul,");
	} else if (W("a!")) {
		codeAsm("r0", "r1", "mov,");
		codeAsm("r0", "pop");
	} else if (W("a@")) {
		codeAsm("r0", "push");
		// Thumb substitute for "movs"
		codeAsm("0", "##", "r1", "r0", "add,");
	} else if (W("c!a")) {
		if (!thumb)
			codeAsm("r1", "1", "(#", "r0", "strb,");
		else {
			codeAsm("r1", "0@", "r0", "strb,");
			codeAsm("1", "##", "r1", "add,");
		}
		codeAsm("r0", "pop");
	} else if (W("@")) {
		codeAsm("r0", "0@", "r0", "ldr,");
	} else if (W("@a")) {
		codeAsm("r0", "push");
		if (thumb) {
			codeAsm("r1", "0@", "r0", "ldr,");
			codeAsm("4", "##", "r1", "add,");
		} else {
			codeAsm("r1", "4", "(#", "r0", "ldr,");
		}
	} else if (W("c@a")) {
		codeAsm("r0", "push");
		if (!thumb)
			codeAsm("r1", "1", "(#", "r0", "ldrb,");
		else {
			codeAsm("r1", "0@", "r0", "ldrb,");
			codeAsm("1", "##", "r1", "add,");
		}
	} else if (W("h@a")) {
		codeAsm("r0", "push");
		if (!thumb)
			codeAsm("r1", "2", "(#", "r0", "ldrh,");
		else {
			codeAsm("r1", "0@", "r0", "ldrh,");
			codeAsm("2", "##", "r1", "add,");
		}
	} else if (W("r@")) {
		codeAsm("r0", "push");
		codeAsm("r6", "r0", "mov,");
	} else if (W("!")) {
		codeAsm("r2", "pop");
		codeAsm("r0", "0@", "r2", "str,");
		codeAsm("r0", "pop");
	} else if (W("variable")) {
		const char *ident = getNextWord();

		out->addSym(ident, out->vaddr);
		out->addSym(".dbl:0004", out->vaddr);

		sym = symbols.appendNew(out->addr, ident);
		sym->is_addr = true;
		sym->lit_addr = out->vaddr;

		out->vaddr += 4;
	} else if (W("create")) {
		out->alignDword();
		sym = symbols.appendNew(out->addr, getNextWord());
		thumb = false;
		unsigned int size = TIN_parseNum(getNextWord());
		const char *type = getNextWord();
		(void)type;
		// XXX: What about CELLS/STRINGS?
		DEBUG("reserve type %s at 0x%x\n", type, out->vaddr);
		if (strcmp(getNextWord(), "reserve"))
			GLB_error("create without reserve\n");
		sym->is_addr = true;
		sym->lit_addr = out->vaddr;

		out->addSym(sym->word, out->vaddr);
		char dbgsym[10];
		sprintf(dbgsym, ".dbl:%04X", 4 * size);
		out->addSym(dbgsym, out->vaddr);

		out->vaddr += 4 * size;
	} else if (W("data:")) {
		out->alignDword();
		thumb = false;
		sym = symbols.appendNew(out->addr, getNextWord());
		sym->is_addr = true;
		sym->lit_addr = out->addr;

	// Loop stack entries for Thumb point to the (unconditional) branch
	// insn to relocate, so if they are used as a branch target, we must
	// jump to "entry - 2" (the conditional branch).  If there is no
	// branch insn to relocate, the entry must be encoded as "address +
	// 2" to make any jumps to it end up in the right place.
	} else if (W("if") || W("while")) {
		invalR5();
		codeAsm("r0", "r0", "tst,");
		codeAsm("r0", "pop");
		if (thumb) {
			codeBranch(out->addr + 4, "ne?", "b,");
			loop_stack[lpsp++] = out->addr;
			codeBranch(out->addr, "b,");
		} else {
			loop_stack[lpsp++] = out->addr;
			codeBranch(out->addr, "eq?", "b,");
		}
	} else if (W("else")) {
		invalR5();
		codeBranch(out->addr, "jmp");
		if (thumb) {
			out->reloc10(loop_stack[--lpsp], out->ta());
			loop_stack[lpsp++] = out->addr - 2;
		} else {
			out->reloc24(loop_stack[--lpsp], out->ta());
			loop_stack[lpsp++] = out->addr - 4;
		}
	} else if (W("then")) {
		invalR5();
		if (thumb)
			out->reloc10(loop_stack[--lpsp], out->ta());
		else
			out->reloc24(loop_stack[--lpsp], out->ta());
	} else if (W("begin")) {
		invalR5();
		if (thumb)
			loop_stack[lpsp++] = out->ta() + 2;
		else
			loop_stack[lpsp++] = out->ta();
	} else if (W("0=") && (getNextWordIf("if") || getNextWordIf("while"))) {
		cond = thumb ? "eq?" : "ne?";
		codeAsm("r0", "r0", "tst,");
		goto do_ifwhile;
	} else if ((W("<=") ||
		    W("<") ||
		    W(">") ||
		    W(">=") ||
		    W("<>") ||
		    W("=")) &&
		   (getNextWordIf("while") || getNextWordIf("if"))) {
		cond = op2cond(word, !thumb);
		codeAsm("r2", "pop");
		codeAsm("r0", "r2", "cmp,");
do_ifwhile:
		codeAsm("r0", "pop");
		if (thumb) {
			codeBranch(out->ta() + 4, cond, "b,");
			loop_stack[lpsp++] = out->ta();
			codeBranch(out->ta(), "b,");
		} else {
			loop_stack[lpsp++] = out->ta();
			codeBranch(out->ta(), cond, "b,");
		}
	} else if (W("repeat")) {
		invalR5();
		if (thumb) {
			out->reloc10(loop_stack[--lpsp], out->ta() + 2);
			codeBranch(loop_stack[--lpsp] - 2, "jmp");
		} else {
			out->reloc24(loop_stack[--lpsp], out->ta() + 4);
			codeBranch(loop_stack[--lpsp], "b,");
		}
	} else if (W("again")) {
		invalR5();
		if (thumb)
			codeBranch(loop_stack[--lpsp] - 2, "jmp");
		else
			codeBranch(loop_stack[--lpsp], "b,");
	} else if (W("0=")) {
		codeAsm("1", "##", "r0", "sub,");
		// If there is a borrow (ie. carry clear), we had 0 and are
		// now at 0xffffffff -> mission accomplished.
		codeBranch(out->ta() + 4, "cc?", "b,");
		codeAsm("0", "##", "r0", "mov,");
	} else if (W("interrupt")) {
		out->alignDword();
		invalR5();
		const char *ident = getNextWord();
		out->addSym(ident);
		sym = symbols.appendNew(out->addr, ident);
		sym->is_addr = true;
		sym->lit_addr = out->addr;
		thumb = false;
		codeAsm("sp", "db!", "lr", "r10", "r9", "r8", "stm,");
		codeToThumb(false);
	} else if (W("exit")) {
		invalR5();
		codeToArm();
		codeAsm("sp", "ia!", "lr", "r10", "r9", "r8", "ldm,");
		codeAsm("lr", "bx,");
	} else if (out->use_pimp && W("modvblank")) {
		codeAsm("r0", "push");
		invalR5();
		codeCallThumb(RT_pimp_vblank, "pimp_vblank");
		codeAsm("r0", "pop");
	} else if (out->use_pimp && W("modframe")) {
		codeAsm("r0", "push");
		invalR5();
		codeCallThumb(RT_pimp_frame, "pimp_frame");
		codeAsm("r0", "pop");
	} else if (out->use_pimp && W("modclose")) {
		codeAsm("r0", "push");
		invalR5();
		codeCallThumb(RT_pimp_close, "pimp_close");
		codeAsm("r0", "pop");
	} else if (out->use_pimp && W("modinit")) {
		if (!thumb)
			codeAsm("r0", "4", "(#", "r1", "ldr,");
		else {
			codeAsm("r0", "0@", "r1", "ldr,");
			codeAsm("4", "##", "r0", "add,");
		}
		invalR5();
		codeCallThumb(RT_pimp_init, "pimp_init");
		codeAsm("r0", "pop");
	} else if (out->use_pimp && W("modsetpos")) {
		codeAsm("r1", "pop");
		invalR5();
		codeCallThumb(RT_pimp_set_pos, "pimp_set_pos");
		codeAsm("r0", "pop");
	} else if (out->use_pimp && W("modgetrow")) {
		codeAsm("r0", "push");
		invalR5();
		codeCallThumb(RT_pimp_get_row, "pimp_get_row");
	} else if (out->use_pimp && W("modgetorder")) {
		codeAsm("r0", "push");
		invalR5();
		codeCallThumb(RT_pimp_get_order, "pimp_get_order");
	} else if ((sym = getSymbol(word))) {
		//DEBUG("syma %s 0x%x oa 0x%x is_addr %d lit_addr 0x%x thumb %d\n", sym->word, sym->addr,
		//      out->addr, sym->is_addr, sym->lit_addr, sym->thumb);
		// Can it be addressed by an _ARM_ PC-relative load?
		bool small_offset = (sym->lit_addr & 0x00ffffff) < 0x800;
		if (sym->is_addr) {
			if (getNextWordIf("@")) {
				codeAsm("r0", "push");
				if (!thumb && small_offset) {
					loadR5(sym->lit_addr & 0xff000000);
					codeAsm("r5", sym->lit_addr & 0x00ffffff, "#(", "r0", "ldr,");
				} else {
					loadR5(sym->lit_addr);
					codeAsm("r5", "0@", "r0", "ldr,");
				}
			} else if (getNextWordIf("!")) {
				if (!thumb && small_offset) {
					loadR5(sym->lit_addr & 0xff000000);
					codeAsm("r5", sym->lit_addr & 0x00ffffff, "#(", "r0", "str,");
				} else {
					loadR5(sym->lit_addr);
					codeAsm("r5", "0@", "r0", "str,");
				}
				codeAsm("r0", "pop");
			} else {
				// load from literal pool
				codeAsm("r0", "push");
				literals.prependNew(sym->lit_addr, out->addr, thumb);
				codeAsm("pc", "0", "#(", "r0", "ldr,");
			}
		} else {
			assert(!currently_naked);
			assert(sym->addr != 0);
			invalR5();
			if (sym->thumb)
				codeCallThumb(sym->addr, sym->word);
			else {
				DEBUG("ARM call to %s\n", sym->word);
				codeCallArm(sym->addr);
			}
			if (sym->has_prolog) {
				if (!thumb)
					codeAsm("r7", "4", "(#", "r6", "ldr,");
				else
					codeAsm("r7", "ia!", "r6", "ldm,");
			}
		}
	} else if ((icode = getIcode(word))) {
		DEBUG("emit icode %s len %d\n", icode->word,
		      icode->len);
		bool change = false;
		if (thumb != icode->thumb) {
			change = true;
			if (thumb)
				codeToArm();
			else
				codeToThumb();
		}

		char isym[strlen(icode->word) + 9 + 1];
		sprintf(isym, "__inline_%s", icode->word);
		out->addSym(isym);

		if (cur_icode) {
			memcpy(cur_icode->cp, icode->code, icode->len);
			cur_icode->cp += icode->len;
			cur_icode->len += icode->len;
		} else
			out->emitString(icode->code, icode->len);

		if (change) {
			if (icode->thumb)
				codeToArm();
			else
				codeToThumb();
		}
		invalR5();
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
	rsp = 0;
	asp = 0;
	lpsp = 0;
	thumb = false;
	invalR5();
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
	sym_fp = NULL;
	iwptr = 0;
	iwaddr = 0x3001000;
}

void Output::openSymFile(const char *name)
{
	sym_fp = fopen(name, "w");
	if (!sym_fp)
		GLB_error("failed to create SYM file %s\n", name);
}

void Output::closeSymFile(void)
{
	if (sym_fp)
		fclose(sym_fp);
}

void Output::addSym(const char *sym, unsigned int addr)
{
	if (addr == (unsigned int)-1)
		addr = this->addr;
	if (sym_fp)
		fprintf(sym_fp, "%08X %s\n", addr, sym);
}

bool clean_exit = false;
static const char *outfile;

static void clean_up(void)
{
	if (!option_debug && !clean_exit)
		unlink(outfile);
	delete outfile;
}

int main(int argc, char **argv)
{
	int rt_size;
	bool option_symfile = false;

	FreeImage_Initialise();
	Parser par;
	Output out(0x8000000, 0x2000000);
	char hdr[131072];

	if (argc < 2) {
		fprintf(stderr, "Usage: %s [-debug|-mod] <sourcefile> <binary>\n",
			argv[0]);
		exit(1);
	}

	for (;;) {
		if (!strcmp(argv[1], "-debug")) {
			option_debug = true;
			argc--;
			argv++;
		} else if (!strcmp(argv[1], "-mod")) {
			out.use_pimp = true;
			argc--;
			argv++;
		} else if (!strcmp(argv[1], "-sym")) {
			option_symfile = true;
			argc--;
			argv++;
		} else {
			break;
		}
	}

	outfile = strdup(argv[2]);
	atexit(clean_up);
	out.openOutFile(argv[2]);

	if (option_symfile) {
		char *sym_name = strdup(argv[2]);
		char *ext = strrchr(sym_name, '.');
		if (!ext)
			GLB_error("could not find extension of %s\n", argv[2]);
		sprintf(ext, ".sym");
		out.openSymFile(sym_name);
	}

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

	// Move IRQ handler to IWRAM.
	out.patch32(RT_irq_handler_p,
		    out.addIwram(RT_irq_handler,
				 RT__end_irq_handler - RT_irq_handler));

	out.codeIwramTable();

	out.fixCartHeader();
	out.closeOutFile();
	out.closeSymFile();
	clean_exit = true;
	FreeImage_DeInitialise();
	return 0;
}
