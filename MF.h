//  MF.h: MF/TIN compiler
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

extern bool option_debug;
extern bool debug_words;

#ifndef NDEBUG
#define DEBUGN(x ...) do { if (option_debug) fprintf(stderr, x); \
} while (0)
#define DEBUG(x ...) do { \
	if (debug_words) { \
		DEBUGN("\"\n"); \
		debug_words = false; \
	} \
	DEBUGN("DEBUG " x); \
} while (0)

#else
#define DEBUG(x ...) do {} while(0)
#define DEBUGN(x ...) do {} while(0)
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
	bool thumb;
	Icode *next;
	int sp_offset;
};

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
	bool is_const;
	unsigned int lit_addr;
	bool thumb;
};

class Literal;
class Output;

class Output {
public:
	Output(unsigned int code_addr, unsigned int var_addr);
	void setTitle(const char *title);
	void setEntry(unsigned int addr);
	void setIwramTable(unsigned int addr);
	unsigned int addIwram(unsigned int from, int size);
	void codeIwramTable();
	void registerIwram(Symbol *iwsym);
	void emitByte(const unsigned char byte);
	void emitDword(const unsigned int dword);
	void emitWord(const unsigned short word);
	void emitString(const char *str, int len);
	void emitBinary(const char *file);
	void emitBitmap(const char *bmp);
	void emitMap(const char *fmp);
	void emitPalette(const char *bmp);
	void patch16(unsigned int addr, unsigned short val);
	void patch32(unsigned int addr, unsigned int val);
	// XXX: These two do completely different things...
	void reloc8(Literal *lit);
	void reloc8(unsigned int addr, unsigned int target);
	void reloc10(unsigned int addr, unsigned int target);
	void reloc12(Literal *lit);
	void reloc24(unsigned int addr, unsigned int target);
	void emitSound(const char *wav);
	void emitMusic(const char *mod);
	void fixCartHeader();
	void openOutFile(const char *name);
	void closeOutFile(void);
	void alignDword();

	void openSymFile(const char *name);
	void closeSymFile(void);
	void addSym(const char *sym, unsigned int addr = -1);

	bool use_pimp;
	unsigned int addr;
	unsigned int vaddr;

	unsigned int ta();	// Translated address of current position.

	void setIwram() {
		currently_iwram = true;
		// Remember where we started when enabling IWRAM mode.
		start_iwram = iwaddr;
	}
	void clearIwram() {
		currently_iwram = false;

		// The runtime copier does not handle unaligned data.
		if (iwaddr & 2)
			iwaddr += 2;
	}

	bool currently_iwram;

private:
	FILE *fp;
	FILE *sym_fp;

	unsigned int iwaddr;	// next IWRAM destination
	unsigned int iwptr;	// IWRAM list index
	unsigned int start_iwram;
	struct {
		unsigned int from;
		unsigned int to;
		int size;
	} iwram[256];
};

class Literal {
public:
	Literal();
	Literal(unsigned int val, unsigned int reloc, bool thumb = false);
	Literal *prependNew(unsigned int val, unsigned int reloc, bool thumb = false);
	void code(Output *out);

	unsigned int val;
	unsigned int reloc;
	unsigned int coded_at;
	bool thumb;
	Literal *next;
	Literal *prev;
};

class Parser {
public:
	Parser();
	void setOutput(Output *out);
	const char *_getNextWord(bool skip_line = true);
	const char *getNextWord(bool skip_line = true);
	const char *peekWordN(int n);
	bool isWordN(int n, const char *word);
	const char *peekNextWord();
	bool getNextWordIf(const char *word);
	void pushText(const char *filename);
	void popText(void);
	void code(unsigned int insn);
	void code16(unsigned short insn);
	Symbol *getSymbol(const char *word);
	Icode *getIcode(const char *word);
	void parseAsm(const char *word);
	void debugAsm(const char *word, bool pseudo = false);
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
	void codeBranch(unsigned int dest, const char *cond, const char *mnem);
	void codeCallArm(unsigned int dest);
	void codeCallThumb(unsigned int dest, const char *ident);
	void codeToThumb(bool save_lr = true);
	void codeToArm();
	void codePush(const char *reg);
	void codePop(const char *reg);

	void codeLocalLoad(int num, const char *reg);
	void codeLocalStore(int num, const char *reg);
	void codeGetLocalAddr(int num, const char *reg);
	void codeAddLocalBase(const char *reg);

	void resolveRelocs(const char *label);
	void checkRelocs();

	void setR5(unsigned int c) {
		r5_const = true;
		r5 = c;
	}
	void invalR5() {
		r5_const = false;
	}
	void loadR5(unsigned int num);
	void codeLoadConst(unsigned int num, const char *reg);

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

	bool parseArm(const char *word);
	bool parseThumb(const char *word);

	unsigned int POP_VAL_TYPE(unsigned int x);
	unsigned int POP_TREG();

	int debug_old_addr, debug_old_asp;

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
	struct {
		const char *label;
		unsigned int addr;
		unsigned int reloc;
	} asm_relocs[32];
	int rsp;
	unsigned int asm_stack[32][2];
	char asm_text[32][64];
	int asp;
	unsigned int loop_stack[32];
	int lpsp;
	bool thumb;
	unsigned int last_insn;
	bool r5_const;
	unsigned int r5;

	bool text_mode;
	int sp_offset;
	bool currently_naked;
	bool skip_push;
	int first_local_in_tos;
	bool underwater;
};

extern const char *current_file;
extern const char *current_word;
extern unsigned int current_line;
extern bool last_eol;

#define GLB_error(x, ...) do { \
	fprintf(stderr, "ERROR: %s:%d: %s: " x, \
		current_file, \
		current_line - (last_eol ? 1 : 0), \
		current_word, ##__VA_ARGS__); \
	exit(1); \
} while (0)

#define GLB_warning(x, ...) do { \
	fprintf(stderr, "WARNING: %s:%d: %s: " x, \
		current_file, \
		current_line - (last_eol ? 1 : 0), \
		current_word, ##__VA_ARGS__); \
} while (0)

int Decode(const char *filename,Output *out);

