//  DBC.h: DBC/MF DragonBASIC compiler header file
//
//  Copyright (C) 2015 Ulrich Hecht
//  Source code reconstructed with permission from DBC/MF executable version
//  1.4.5, Copyright (C) Jeff Massung
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


#ifndef _DBCF_H
#define _DBCF_H

#include <stdio.h>

enum obj_t {
	OBJ_IDENT = 0,
	OBJ_SUB = 1,
	OBJ_ARR = 2,
	OBJ_CMD = 3,
	OBJ_ROP = 4,
	OBJ_OP = 5,
	OBJ_NUM = 6,
	OBJ_DIR = 7,
	OBJ_STR = 8,
	OBJ_EOL = 10,
	OBJ_EOT = 11,
	OBJ_PATH = 12,
};

enum vartype_t {
	VAR_SCALAR = 0,
	VAR_ARRAY = 1,
	VAR_FIXEDP = 2,
};

enum error_t {
	ERR_NONE = 0,
	ERR_WOT = 1,
	ERR_UNKNOWN_CFLAG = 2,
	ERR_NO_SOURCE = 3,
	ERR_OPEN_FAIL = 4,
	ERR_WRITE_OBJ = 5,
	ERR_UNK_DIR = 6,
	ERR_UNK_TOK = 7,
	ERR_SYNTAX = 8,
	ERR_NOTOKEN = 9,
	ERR_TOO_MANY_PROTO_ARGS = 10,
	ERR_DUP_IDENT = 11,
	ERR_UNK_IDENT = 12,
	ERR_NEED_LVAL = 13,
	ERR_NEED_RVAL = 14,
	ERR_TOO_FEW_ARGS = 15,
	ERR_TOO_MANY_ARGS = 16,
	ERR_STACK_OVER = 17,
	ERR_STACK_UNDER = 18,
	ERR_UNTERM_CONTROL = 19,
	ERR_TERM_MISMATCH = 20,
	ERR_EXPR = 21,
	ERR_SEGMENT = 22,
	ERR_NOT_ARRAY = 23,
	ERR_ARRAY = 24,
	ERR_TYPE_MISMATCH = 25,
	ERR_TYPED_SUB = 26,
	ERR_ILLEGAL_OP = 27,
	ERR_RETVAL = 28,
	ERR_UNREACHABLE = 29,
	ERR_WITHOUT = 30,
	ERR_NOT_FOUND = 31,
	ERR_SET_FOLDER = 32,
	ERR_LAUNCH = 33,
};

class TIN
{
public:
	~TIN();
	TIN(const char *text);

	TIN *unused(char *text);
	TIN *addCode(const char *code);
	void writeCodeToFile(FILE *fp);

	TIN *vunused;
	TIN *next;
	char text[256];
};

class Subroutine
{
public:
	Subroutine(const char *ident, bool is_function, enum vartype_t rtype);
	~Subroutine();
	Subroutine *findByIdent(const char *ident);
	void addArgument(const char *name, enum vartype_t vtype);
	void addLocal(const char *name, enum vartype_t vtype);
	TIN *declareTinLocals(TIN *tin);

	char ident[168];
	bool is_function;
	enum vartype_t ret_vtype;
	int num_args;
	int arg_vtype[16];
	Subroutine *next;
	char arg_name[16][168];
	int num_locals;
	int local_vtype[16];
	char local_name[16][168];
};

enum segment_t {
	SEG_TOP = 0x1,
	SEG_SUB = 0x2,
	SEG_INTR = 0x4,
};

enum cmd_t {
	CMD_RESET = 0,
	CMD_LEAVE = 1,
	CMD_WHILE = 2,
	CMD_LOOP = 3,
	CMD_REPEAT = 4,
	CMD_UNTIL = 5,
	CMD_FOR = 6,
	CMD_NEXT = 7,
	CMD_IF = 8,
	CMD_THEN = 9,
	CMD_ELSE = 10,
	CMD_SELECT = 11,
	CMD_CASE = 12,
	CMD_DEFAULT = 13,
	CMD_END = 14,
	CMD_TO = 15,
	CMD_DOWNTO = 16,
	CMD_STEP = 17,
	CMD_RETURN = 18,
	CMD_FUNCTION = 19,
	CMD_SUB = 20,
	CMD_PROTOTYPE = 21,
	CMD_DIM = 22,
	CMD_AS = 23,
	CMD_TYPE = 24,
	CMD_READ = 25,
	CMD_RESTORE = 26,
	CMD_DATA = 27,
	CMD_MAP = 28,
	CMD_INTERRUPT = 29,
	CMD_EXIT = 30,
	CMD_INC = 31,
	CMD_DEC = 32,
	CMD_LOCAL = 33,
};

struct LoopStackEntry {
	int		line_no;
	enum cmd_t	cmd;
};

class LoopStack
{
public:
	LoopStack();
	int getStackPtr();
	bool addEntry(enum cmd_t cmd, int line_no);
	char popEntry(enum cmd_t *cmd_p, int *line_no_p);
	void checkEmpty();

private:
	struct LoopStackEntry stack[16];
	int stack_ptr;
};

class Variable
{
public:
	Variable(const char *ident, enum vartype_t vtype);
	~Variable();
	Variable *findByIdent(const char *ident);
	Variable *prependNew(const char *ident, enum vartype_t vtype);

	char ident[256];
	int vtype;
	Variable *next;
};

enum op_t {
	OP_OPAREN = 0,
	OP_LNOT = 1,
	OP_NAND = 2,
	OP_OR = 3,
	OP_AND = 4,
	OP_NOR = 5,
	OP_XOR = 6,
	OP_BNOT = 7,
	OP_EQ = 8,
	OP_NE = 9,
	OP_LT = 10,
	OP_GT = 11,
	OP_LE = 12,
	OP_GE = 13,
	OP_SL = 14,
	OP_SR = 15,
	OP_PLUS = 16,
	OP_MINUS = 17,
	OP_MULT = 18,
	OP_DIV = 19,
	OP_MOD = 20,
	OP_UMINUS = 21,
};

class Expression
{
public:
	Expression(TIN *tin_head);
	void compileOperation(int num_ops, const char *tin_op,
			      enum vartype_t vtype);
	void addOperand(enum vartype_t vtype);
	void enqueueOperation(enum op_t op);
	void compileNextOperation();
	void compileScalarOp();
	void compileArrayOp();
	void compileFixedPointOp();
	enum vartype_t compileAllOps();

private:
	op_t operation[64];
	vartype_t operand[64];
	int operation_idx;
	int operand_idx;
	TIN *tin_head;
};

enum directive_t {
	DIR_REGISTER = 0,
	DIR_TITLE = 1,
	DIR_INCLUDE = 2,
	DIR_IMPORT = 3,
	DIR_BITMAP = 4,
	DIR_PALETTE = 5,
	DIR_SOUND = 6,
	DIR_MUSIC = 7,
	DIR_CONSTANT = 8,
	DIR_REQUIRES = 9,
	DIR_MAP = 10,
};

enum rop_t {
	ROP_CPAREN = 0,
	ROP_CBRACK = 1,
	ROP_PERIOD = 2,
	ROP_COLON = 3,
	ROP_COMMA = 4,
	ROP_BANG = 5,
};

class BasicObject
{
public:
	BasicObject(enum obj_t otype, int line_no);
	BasicObject(enum directive_t dir, int obj_idx);
	BasicObject(enum cmd_t cmd, int obj_idx);
	BasicObject(enum rop_t rop, int obj_idx);
	BasicObject(enum op_t op, int obj_idx);
	BasicObject(int val, int line_no);
	BasicObject(const BasicObject &bobj);
	~BasicObject();

	void append(BasicObject *new_bobj);

	enum obj_t otype;
	enum vartype_t vtype;
	union {
		int	numeric;
		char	symbolic[256];
	} val;
	int line_no;
	BasicObject *next;
};

class Symbol
{
public:
	static Symbol *newInitialSymbolList();
	Symbol(const char *ident, Symbol *next);
	~Symbol();
	void appendBasicObj(const BasicObject *bobj);
	BasicObject *getBasicObj(const char *ident);

private:
	char ident[168];
	BasicObject *basic_obj_head;
	Symbol *next;
};

class Parser
{
public:
	Parser(const char *filename, Symbol *symbol_list);
	~Parser();
	char getChar();
	char skipNewline();
	BasicObject *parseNext();
	BasicObject *parseToken();
	BasicObject *parseDecimal();
	BasicObject *parseDirective();
	BasicObject *parseComment();
	BasicObject *parseString(const char delim);
	BasicObject *parseHex();
	BasicObject *parseBinary();
	BasicObject *parseOperator();
	void parseAll();
	BasicObject *retrieveNextBasicObject();
	BasicObject *consumeNextBasicObj();
	BasicObject *getObjectWithType(enum obj_t	otype,
				       const char *	human_type);
	bool checkNextBasicObjType(enum obj_t otype);
	bool requireRop(enum rop_t rop);
	bool getNextCmd(enum cmd_t expected);
	void insertBasicObjectList(BasicObject *bobj);
	void addNewSymbol(const char *name);
	void appendNextBasicObjCopyToSymbolList();

	char filename[256];
	BasicObject *cur_basic_obj;
	Symbol *symbol_head;

private:
	int cur_line;
	char *text;
	char *text_ptr;
	BasicObject *basic_obj_head;
	bool is_subparser;
};

class Compiler
{
public:
	Compiler();
	~Compiler();
	void parseFile(const char *filename);
	int writeOutput(const char *filename);
	void emitTin(const char *fmt, ...);
	void doSubroutine(BasicObject *bobj, bool is_function, bool emit_code);
	void doArgs(bool emit_code);
	void addSubArgument(const char *ident, enum vartype_t vtype);
	void addSubLocal(const char *ident, enum vartype_t vtype);
	void addVariable(const char *ident, enum vartype_t vtype);
	void addNewSub(const char *ident, bool is_function,
		       enum vartype_t vtype);
	void checkNotSegment(int seg_mask, const char *human_seg);
	void compile();
	void compileBasicObject(BasicObject *bobj);
	void doDirective(BasicObject *bobj);
	void doDirRegister();
	void doDirTitle();
	void doDirRequires();
	void doDirInclude();
	void doDirImport();
	void doDirBitmap();
	void doDirMap();
	void doDirPalette();
	void doDirSound();
	void doDirMusic();
	void doDirConstant();
	void doCommand(BasicObject *bobj);
	void doCmdPrototype();
	void doCmdWhile();
	void doCmdLoop();
	void doCmdRepeat();
	void doCmdUntil();
	void doCmdFor();
	void doCmdNext();
	void doCmdDim();
	void doCmdLocal();
	void doCmdReturn();
	void doCmdMap();
	void doCmdData();
	void doCmdRead();
	void doCmdRestore();
	void doCmdSub();
	void doCmdFunction();
	void doCmdIf();
	void doCmdElse();
	void doCmdEndIf();
	void doCmdSelect();
	void doCmdCase();
	void doCmdDefault();
	void doCmdEnd();
	void doCmdReset();
	void doCmdInterrupt();
	void doCmdExit();
	void doCmdInc();
	void doCmdDec();
	void doLabel(BasicObject *bobj);
	void doLval(BasicObject *bobj);
	void doLvalNotSub(BasicObject *bobj, bool array);
	void doRvalFunction(BasicObject *bobj);
	void doRvalArray(BasicObject *bobj);
	void doAssign(BasicObject *bobj);
	void doRval(BasicObject *bobj);
	enum vartype_t compileExpression();
	void doOperand(BasicObject *bobj);
	void callSubroutine(Subroutine *sub);

	Parser *parser;
	int line_no;

private:
	TIN *tin_head;
	TIN *tin_tail;
	Subroutine *sub_head;
	Variable *var_head;
	Variable *sub_args_head;
	Variable *sub_locals_head;
	enum segment_t cur_seg;
	LoopStack loop_stack;
	bool is_top_level;
};

#endif
