//  DBC.cpp: DBC/MF DragonBASIC compiler
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


#ifdef __WIN32__
#include <windows.h>
#endif

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "DBC.h"
#include "os.h"

char dir_stack[200][256];
char err_buf[256];
int dir_stack_idx;
int my_errno;

bool option_multiboot;
bool option_debug;
bool option_optimize;
bool option_mod;
bool option_sym;

void GLB_error(enum error_t error_code, ...);
void GLB_failWithMbox(const char *msg, const char *func, int line);
void GLB_pushDir(const char *lpPathName);
void GLB_popDir();

Subroutine::Subroutine(const char *ident, bool is_function,
		       enum vartype_t rtype)
{
	strcpy(this->ident, ident);
	this->is_function = is_function;
	ret_vtype = rtype;
	num_args = 0;
	num_locals = 0;
	next = 0;
	can_be_naked = true;
	tin_start = NULL;
	is_inline = false;
}

Subroutine::~Subroutine()
{
	Subroutine *sub;

	if (next) {
		sub = next;
		if (sub)
			delete sub;
	}
}

Subroutine *Subroutine::findByIdent(const char *ident)
{
	Subroutine *result;

	if (!this)
		return NULL;

	if (strcasecmp(ident, this->ident))
		result = next->findByIdent(ident);
	else
		result = this;
	return result;
}

void Subroutine::addArgument(const char *name, enum vartype_t vtype)
{
	int idx;

	idx = num_args;
	++num_args;
	if (num_args > 15)
		GLB_error(ERR_TOO_MANY_PROTO_ARGS, name);
	arg_vtype[idx] = vtype;
	if (name)
		strcpy(arg_name[idx], name);
	else
		arg_name[idx][0] = 0;
}

// This is a copy of Subroutine::addArgument(), with the difference that it
// acts on locals and does not allow anonymous variables.
void Subroutine::addLocal(const char *name, enum vartype_t vtype)
{
	int idx;

	idx = num_locals;
	++num_locals;
	if (num_locals > 31)
		GLB_error(ERR_TOO_MANY_PROTO_ARGS, name);
	local_vtype[idx] = vtype;
	if (name)
		strcpy(local_name[idx], name);
	else
		GLB_error(ERR_SYNTAX);
}

TIN *Subroutine::declareTinLocals(TIN *tin)
{
	char buf[256];
	int argc;

	// MF assigns addresses incrementally from 0.  The arguments are
	// already on the stack and must therefore have higher addresses
	// than the BASIC local variables, so the latter must be declared
	// first.
	argc = num_locals;
	while (true) {
		--argc;
		if (argc < 0)
			break;
		sprintf(buf, "LOCAL: %s::%s\n", ident, local_name[argc]);
		tin = tin->addCode(buf);
	}

	argc = num_args;
	while (true) {
		--argc;
		if (argc < 0)
			break;
		sprintf(buf, "LOCAL: %s::%s\n", ident, arg_name[argc]);
		tin = tin->addCode(buf);
	}

	return tin;
}

static const char *error_strings[] = {
	[ERR_NONE] = "",
	[ERR_WOT] = "%s?",
	[ERR_UNKNOWN_CFLAG] = "Unknown compiler flag? %s",
	[ERR_NO_SOURCE] = "No source file to compile?",
	[ERR_OPEN_FAIL] = "Failed to open file? %s",
	[ERR_WRITE_OBJ] = "Failed to write object file? %s",
	[ERR_UNK_DIR] = "Unknown directive? '%s'",
	[ERR_UNK_TOK] = "Unknown token? %c",
	[ERR_SYNTAX] = "Syntax error?",
	[ERR_NOTOKEN] = "Expected token? %s",
	[ERR_TOO_MANY_PROTO_ARGS] = "Too many arguments in prototype? %s",
	[ERR_DUP_IDENT] = "Duplicate identifier? %s",
	[ERR_UNK_IDENT] = "Unknown identifier? %s",
	[ERR_NEED_LVAL] = "Expected L-value? %s",
	[ERR_NEED_RVAL] = "Expected R-value? %s",
	[ERR_TOO_FEW_ARGS] = "Too few arguments? %s",
	[ERR_TOO_MANY_ARGS] = "Too many arguments? %s",
	[ERR_STACK_OVER] = "Stack overflow? %s",
	[ERR_STACK_UNDER] = "Stack underflow? %s",
	[ERR_UNTERM_CONTROL] = "Unterminated control on line %d?",
	[ERR_TERM_MISMATCH] = "Terminal does not match control on line %d? %s",
	[ERR_EXPR] = "Error in expression?",
	[ERR_SEGMENT] = "Wrong segment? %s",
	[ERR_NOT_ARRAY] = "Variable is not an array? %s",
	[ERR_ARRAY] = "Variable is an array? %s",
	[ERR_TYPE_MISMATCH] = "Type mis-match?",
	[ERR_TYPED_SUB] = "Subroutines cannot have types? %s",
	[ERR_ILLEGAL_OP] = "Operation not supported for this type?",
	[ERR_RETVAL] = "Return value expected",
	[ERR_UNREACHABLE] = "Unreachable code?",
	[ERR_WITHOUT] = "%s without %s?",
	[ERR_NOT_FOUND] = "%s not found?",
	[ERR_SET_FOLDER] = "Failed to set folder? %s",
	[ERR_LAUNCH] = "%s failed to launch?",
};

static Compiler *gCompiler = 0;
void GLB_error(enum error_t error_code, ...)
{
	va_list va;

	va_start(va, error_code);
	my_errno = error_code;
	vsprintf(err_buf, error_strings[error_code], va);
	GLB_failWithMbox(err_buf, gCompiler->parser->filename,
			 gCompiler->line_no);
}

void GLB_failWithMbox(const char *msg, const char *func, int line)
{
	char Text[256];
	char line_str[168];

	memset(line_str, 0, 168);

	if (line > 0)
		sprintf(line_str, " line %d", line);
	if (func)
		sprintf(Text, "%s%s: %s", func, line_str, msg);
	else
		sprintf(Text, "%s", msg);
#ifdef __WIN32__
	MessageBoxA(0, Text, "Dragon BASIC", 0x10u);
#else
	fprintf(stderr, "Error: %s\n", Text);
#endif
	exit(-my_errno);
}

void GLB_exitWithMbox(const char *msg, ...)
{
	char Text[256];
	va_list va;

	va_start(va, msg);
	vsprintf(Text, msg, va);
#ifdef __WIN32__
	MessageBoxA(0, Text, "Dragon BASIC", 0x40u);
#else
	fprintf(stderr, "%s\n", Text);
#endif
	exit(0);
}

void GLB_getAppDir(char *lpFilename)
{
	OS_getAppDir(lpFilename);
}

void GLB_checkFileInAppdir(const char *filename)
{
	char appdir[256];
	char buf[256];
	FILE *fp;

	GLB_getAppDir(appdir);
	sprintf(buf, "%s" PATHSEP "%s", appdir, filename);
	fp = fopen(buf, "r");
	if (!fp)
		GLB_error(ERR_NOT_FOUND, filename);
	fclose(fp);
}

void GLB_runProgram(const char *lpApplicationName, const char *args,
		    bool show_window)
{
	char Dirname[256];
	char CommandLine[1024];

#ifdef __WIN32__
	struct _STARTUPINFOA StartupInfo;
	struct _PROCESS_INFORMATION ProcessInformation;
#endif

	GLB_checkFileInAppdir(lpApplicationName);
	GLB_getAppDir(Dirname);
	sprintf(CommandLine, "\"%s" PATHSEP "%s\" %s", Dirname,
		lpApplicationName, args);
#ifdef __WIN32__
	/* I do not know why Win32 needs this pushDir(); the binary and all file
	 * arguments are given as absolute paths, but CreateProcessA() still fails
	 * unless we change into the directory containing mf.exe first... */
	GLB_pushDir(Dirname);
	memset(&StartupInfo, 0, 68u);
	StartupInfo.cb = 68;
	StartupInfo.dwFlags = STARTF_USESHOWWINDOW;
	StartupInfo.wShowWindow = show_window ? SW_SHOW : 0;
	if (!CreateProcessA(
		    lpApplicationName,
		    CommandLine,
		    0,
		    0,
		    false,
		    NORMAL_PRIORITY_CLASS,
		    0,
		    dir_stack[dir_stack_idx - 1],
		    &StartupInfo,
		    &ProcessInformation))
		GLB_error(ERR_LAUNCH, lpApplicationName);
	WaitForSingleObject(ProcessInformation.hProcess, 0xFFFFFFFFu);
	CloseHandle(ProcessInformation.hProcess);
	CloseHandle(ProcessInformation.hThread);
	GLB_popDir();
#else
	if (system(CommandLine))
		GLB_error(ERR_LAUNCH, lpApplicationName);
#endif
}

void GLB_runProgramWithArgs(const char *lpApplicationName, const char *options,
			    const char *lpFileName, const char *outfile,
			    bool del_output)
{
	char buf[256];
	char args[256];

	getcwd(buf, 256);
	sprintf(args, "%s\"%s" PATHSEP "%s\" \"%s" PATHSEP "%s\"", options, buf,
		lpFileName, buf, outfile);
	GLB_runProgram(lpApplicationName, args, !del_output);
	if (del_output)
		unlink(lpFileName);
}

void GLB_pushDir(const char *lpPathName)
{
	char *dir_buf;

	dir_buf = dir_stack[dir_stack_idx++];
	getcwd(dir_buf, 256);
	if (chdir(lpPathName))
		GLB_error(ERR_SET_FOLDER, lpPathName);
}

void GLB_popDir()
{
	--dir_stack_idx;
	if (dir_stack_idx < 0)
		GLB_error(ERR_STACK_UNDER, "#INCLUDE");
	if (chdir(dir_stack[dir_stack_idx]))
		GLB_error(ERR_SET_FOLDER, dir_stack[dir_stack_idx]);
}

Compiler::Compiler()
{
	TIN *tin;

	tin = new TIN(NULL);
	tin_head = tin;
	tin_tail = tin_head;

	sub_head = NULL;
	var_head = NULL;
	sub_args_head = NULL;
	sub_locals_head = NULL;
	parser = NULL;

	cur_seg = SEG_TOP;
}

Compiler::~Compiler()
{
	if (sub_head)
		delete sub_head;
	if (var_head)
		delete var_head;
	if (tin_head)
		delete tin_head;
	if (parser)
		delete parser;
}

void Compiler::parseFile(const char *filename)
{
	line_no = 1;
	if (parser)
		parser = new Parser(filename, parser->symbol_head);
	else
		parser = new Parser(filename, NULL);
	emitTin("#FILE\" %s\" ", filename);
	parser->parseAll();
}

int Compiler::writeOutput(const char *filename)
{
	FILE *fp;

	line_no = 1;
	fp = fopen(filename, "w+t");
	if (!fp)
		GLB_error(ERR_WRITE_OBJ, filename);
	tin_head->writeCodeToFile(fp);
	return fclose(fp);
}

void Compiler::emitTin(const char *fmt, ...)
{
	TIN *tail;
	char buf[256];
	va_list va;

	va_start(va, fmt);
	vsprintf(buf, fmt, va);
	tail = tin_tail->addCode(buf);
	tin_tail = tail;
}

void Compiler::doSubroutine(BasicObject *bobj, bool is_function, bool emit_code)
{
	bool iwram = false;

	if (bobj->otype == OBJ_CMD && bobj->val.numeric == CMD_IWRAM) {
		iwram = true;
		bobj = parser->consumeNextBasicObj();
	}
	if (bobj->otype && bobj->otype != OBJ_SUB)
		GLB_error(ERR_NOTOKEN, "Identifier");
	if (!is_function && bobj->vtype)
		GLB_error(ERR_TYPED_SUB, bobj->val.symbolic);
	addNewSub(bobj->val.symbolic, is_function, bobj->vtype);
	if (bobj->otype == OBJ_SUB)
		doArgs(emit_code);

	// Manually check if there are locals declared at the start of the
	// function.  We need to do this here because the local declarations
	// have to be emitted before the actual function body.
	while (parser->checkNextBasicObjType(OBJ_EOL)) {
		// Consume any additional EOLs.  This is necessary so the
		// compiler won't stumble over empty lines or comments before
		// the local declarations.
		while (parser->retrieveNextBasicObject()->next->otype == OBJ_EOL) {
			parser->consumeNextBasicObj();
		}
		BasicObject *loc = parser->retrieveNextBasicObject()->next;
		assert(loc);
		if (loc->otype == OBJ_CMD && loc->val.numeric == CMD_LOCAL) {
			parser->consumeNextBasicObj();	// EOL
			parser->consumeNextBasicObj();	// LOCAL
			compileBasicObject(loc);
		} else
			break;
	}

	if (emit_code) {
		tin_tail = sub_head->declareTinLocals(tin_tail);
		emitTin(":  %s%s ", iwram ? "IWRAM " : "", bobj->val.symbolic);
		sub_head->tin_start = tin_tail;
		if (sub_head->num_args > 0 || sub_head->num_locals > 0) {
			// Code the local variable prolog.  Arguments
			// are assumed to have been placed on the stack
			// already, so we only have to reserve space for
			// local variables.
			emitTin("%d LPROLOG ", 4 * sub_head->num_locals);
			sub_head->can_be_naked = false;
		}
	}
}

void Compiler::doArgs(bool emit_code)
{
	BasicObject *bobj;

	if (!parser->requireRop(ROP_CPAREN)) {
		do {
			bobj =
				parser->getObjectWithType(OBJ_IDENT,
							  "Identifier");
			sub_head->addArgument(bobj->val.symbolic, bobj->vtype);
			if (emit_code)
				addSubArgument(bobj->val.symbolic, bobj->vtype);

		} while (parser->requireRop(ROP_COMMA));
		if (!parser->requireRop(ROP_CPAREN))
			GLB_error(ERR_NOTOKEN, ")");
	}
}

void Compiler::addSubArgument(const char *ident, enum vartype_t vtype)
{
	if (sub_locals_head->findByIdent(ident) ||
	    sub_args_head->findByIdent(ident))
		GLB_error(ERR_DUP_IDENT, ident);

	sub_args_head = sub_args_head->prependNew(ident, vtype);
}

void Compiler::addSubLocal(const char *ident, enum vartype_t vtype)
{
	if (sub_locals_head->findByIdent(ident) ||
	    sub_args_head->findByIdent(ident))
		GLB_error(ERR_DUP_IDENT, ident);

	sub_locals_head = sub_locals_head->prependNew(ident, vtype);
}

void Compiler::addVariable(const char *ident, enum vartype_t vtype)
{
	if (var_head->findByIdent(ident))
		GLB_error(ERR_DUP_IDENT, ident);
	var_head = var_head->prependNew(ident, vtype);
}

void Compiler::addNewSub(const char *ident, bool is_function,
			 enum vartype_t vtype)
{
	Subroutine *sub;

	sub = new Subroutine(ident, is_function, vtype);
	if (sub_head) {
		if (sub_head->findByIdent(ident))
			GLB_error(ERR_DUP_IDENT, ident);
		sub->next = sub_head;
	}
	sub_head = sub;
	if (sub_args_head) {
		delete sub_args_head;
		sub_args_head = 0;
	}
	if (sub_locals_head) {
		delete sub_locals_head;
		sub_locals_head = 0;
	}
}

void Compiler::checkNotSegment(int seg_mask, const char *human_seg)
{
	if ((seg_mask & cur_seg) > 0)
		GLB_error(ERR_SEGMENT, human_seg);
}

void Compiler::compile()
{
	BasicObject *bobj;

restart:
	while (true) {
		bobj = parser->consumeNextBasicObj();
		if (!bobj)
			break;
		line_no = bobj->line_no;
		if (bobj->otype != OBJ_EOL) {
			if (bobj->otype != OBJ_EOT) {
				while (true) {
					emitTin("\n#LINE %4d  ", bobj->line_no);
					compileBasicObject(bobj);
					if (parser->checkNextBasicObjType(
						    OBJ_EOT))
						break;
					if (!parser->requireRop(ROP_COLON)) {
						parser->getObjectWithType(
							OBJ_EOL, "EOL");
						goto restart;
					}
					bobj = parser->consumeNextBasicObj();
				}
			}
			break;
		}
	}
	loop_stack.checkEmpty();
}

void Compiler::compileBasicObject(BasicObject *bobj)
{
	switch (bobj->otype) {
	case OBJ_EOL:
		return;
	case OBJ_DIR:
		doDirective(bobj);
		break;
	case OBJ_CMD:
		doCommand(bobj);
		break;
	case OBJ_IDENT:
		doLval(bobj);
		break;
	case OBJ_ARR:
		doLvalNotSub(bobj, true);
		break;
	default:
		GLB_error(ERR_SYNTAX);
		return;
	}
}

void Compiler::doDirective(BasicObject *bobj)
{
	checkNotSegment(SEG_INTR | SEG_SUB, "# Directive");
	switch (bobj->val.numeric) {
	case DIR_REGISTER:
		doDirRegister();
		break;
	case DIR_TITLE:
		doDirTitle();
		break;
	case DIR_REQUIRES:
		doDirRequires();
		break;
	case DIR_INCLUDE:
		doDirInclude();
		break;
	case DIR_IMPORT:
		doDirImport();
		break;
	case DIR_BITMAP:
		doDirBitmap();
		break;
	case DIR_MAP:
		doDirMap();
		break;
	case DIR_PALETTE:
		doDirPalette();
		break;
	case DIR_SOUND:
		doDirSound();
		break;
	case DIR_MUSIC:
		doDirMusic();
		break;
	case DIR_CONSTANT:
		doDirConstant();
		break;
	default:
		GLB_error(ERR_SYNTAX);
		return;
	}
}

void Compiler::doDirRegister()
{
	parser->getObjectWithType(OBJ_STR, "Registration code");
}

void Compiler::doDirTitle()
{
	BasicObject *bobj;

	bobj = parser->getObjectWithType(OBJ_STR, "\"title\"");
	emitTin("TITLE\" %s\" ", bobj->val.symbolic);
}

void Compiler::doDirRequires()
{
	BasicObject *bobj;

	bobj = parser->getObjectWithType(OBJ_STR, "\"filename\"");
	emitTin("REQUIRES\" %s\" ", bobj->val.symbolic);
}

void Compiler::doDirInclude()
{
	char *lpPathName;
	BasicObject *bobj;
	Parser *parser_saved;
	char *basename;
	const char *filename;
	int line_no_saved;
	bool change_dir;
	char appdir_path[256];

	if (parser->checkNextBasicObjType(OBJ_PATH))
		bobj = parser->getObjectWithType(OBJ_PATH, "<include file name>");
	else
		bobj = parser->getObjectWithType(OBJ_STR, "\"filename\"");
	line_no_saved = line_no;
	parser_saved = parser;
	lpPathName = bobj->val.symbolic;
	for (basename = bobj->val.symbolic + strlen(bobj->val.symbolic) - 1;
	     *basename != '\\' && *basename != '/' && basename >= lpPathName;
	     --basename)
		;
	change_dir = basename >= lpPathName;
	if (bobj->otype == OBJ_PATH) {
		GLB_getAppDir(appdir_path);
		strcat(appdir_path, PATHSEP "include" PATHSEP);
		if (change_dir) {
			*basename = 0;
			strcat(appdir_path, lpPathName);
			filename = basename + 1;
		} else
			filename = lpPathName;
		lpPathName = appdir_path;
		change_dir = true;
	} else if (!change_dir) {
		filename = bobj->val.symbolic;
	} else {
		*basename = 0;
		filename = basename + 1;
		if (!strncasecmp(lpPathName, "c:\\db\\", 6) ||
		    !strncasecmp(lpPathName, "$INC", 4)) {
			GLB_getAppDir(appdir_path);
			strcat(appdir_path, PATHSEP);
			if (lpPathName[0] == '$') {
				strcat(appdir_path, "include" PATHSEP);
				strcat(appdir_path, lpPathName + 4);
			} else
				strcat(appdir_path, lpPathName + 6);
			lpPathName = appdir_path;
		}
	}
	if (change_dir)
		GLB_pushDir(lpPathName);
	parseFile(filename);
	compile();

	parser_saved->symbol_head = parser->symbol_head;

	if (parser)
		delete parser;

	line_no = line_no_saved;
	parser = parser_saved;

	if (change_dir)
		GLB_popDir();
	emitTin("#FILE\" %s\" ", parser->filename);
}

void Compiler::doDirImport()
{
	BasicObject *bobj;

	checkNotSegment(SEG_INTR | SEG_SUB, "#IMPORT");
	bobj = parser->getObjectWithType(OBJ_STR, "\"filename\"");
	emitTin("IMPORT\" %s\" ", bobj->val.symbolic);
}

void Compiler::doDirBitmap()
{
	BasicObject *bobj;

	bobj = parser->getObjectWithType(OBJ_STR, "\"filename\"");
	checkNotSegment(SEG_INTR | SEG_SUB, "#BITMAP");
	emitTin("BITMAP\" %s\" ", bobj->val.symbolic);
}

void Compiler::doDirMap()
{
	BasicObject *bobj;

	bobj = parser->getObjectWithType(OBJ_STR, "\"filename\"");
	checkNotSegment(SEG_INTR | SEG_SUB, "#MAP");
	emitTin("MAP\" %s\" ", bobj->val.symbolic);
}

void Compiler::doDirPalette()
{
	BasicObject *bobj;

	checkNotSegment(SEG_INTR | SEG_SUB, "#PALETTE");
	bobj = parser->getObjectWithType(OBJ_STR, "\"filename\"");
	emitTin("PALETTE\" %s\" ", bobj->val.symbolic);
}

void Compiler::doDirSound()
{
	BasicObject *bobj;

	checkNotSegment(SEG_INTR | SEG_SUB, "#SOUND");
	bobj = parser->getObjectWithType(OBJ_STR, "\"filename\"");
	emitTin("SOUND\" %s\" ", bobj->val.symbolic);
}

void Compiler::doDirMusic()
{
	BasicObject *bobj;

	option_mod = true;

	checkNotSegment(SEG_INTR | SEG_SUB, "#MUSIC");
	bobj = parser->getObjectWithType(OBJ_STR, "\"filename\"");
	emitTin("MUSIC\" %s\" ", bobj->val.symbolic);
}

void Compiler::doDirConstant()
{
	BasicObject *i;
	BasicObject *bobj;

	bobj = parser->getObjectWithType(OBJ_IDENT, "Identifier");
	checkNotSegment(SEG_INTR | SEG_SUB, "#SOUND"); // XXX: #SOUND?
	if (sub_head->findByIdent(bobj->val.symbolic)
	    || var_head->findByIdent(bobj->val.symbolic))
		GLB_error(ERR_DUP_IDENT, bobj->val.symbolic);
	parser->addNewSymbol(bobj->val.symbolic);
	for (i = parser->retrieveNextBasicObject();
	     i->otype != OBJ_EOL && i->otype != OBJ_EOT;
	     i = parser->retrieveNextBasicObject())
		parser->appendNextBasicObjCopyToSymbolList();
}

void Compiler::doCommand(BasicObject *bobj)
{
	switch (bobj->val.numeric) {
	case CMD_PROTOTYPE:
		doCmdPrototype();
		break;
	case CMD_WHILE:
		doCmdWhile();
		break;
	case CMD_LOOP:
		doCmdLoop();
		break;
	case CMD_REPEAT:
		doCmdRepeat();
		break;
	case CMD_UNTIL:
		doCmdUntil();
		break;
	case CMD_FOR:
		doCmdFor();
		break;
	case CMD_NEXT:
		doCmdNext();
		break;
	case CMD_DIM:
		doCmdDim();
		break;
	case CMD_LOCAL:
		doCmdLocal();
		break;
	case CMD_RETURN:
		doCmdReturn(false);
		break;
	case CMD_MAP:
		doCmdMap();
		break;
	case CMD_DATA:
		doCmdData(4);
		break;
	case CMD_DATAB:
		doCmdData(1);
		break;
	case CMD_DATAH:
		doCmdData(2);
		break;
	case CMD_READ:
		doCmdRead(4);
		break;
	case CMD_READB:
		doCmdRead(1);
		break;
	case CMD_READH:
		doCmdRead(2);
		break;
	case CMD_RESTORE:
		doCmdRestore();
		break;
	case CMD_FUNCTION:
		doCmdFunction();
		break;
	case CMD_SUB:
		doCmdSub();
		break;
	case CMD_IF:
		doCmdIf();
		break;
	case CMD_ELSE:
		doCmdElse();
		break;
	case CMD_SELECT:
		doCmdSelect();
		break;
	case CMD_CASE:
		doCmdCase();
		break;
	case CMD_DEFAULT:
		doCmdDefault();
		break;
	case CMD_END:
		doCmdEnd();
		break;
	case CMD_RESET:
		doCmdReset();
		break;
	case CMD_INTERRUPT:
		doCmdInterrupt();
		break;
	case CMD_EXIT:
		doCmdExit();
		break;
	case CMD_INC:
		doCmdInc();
		break;
	case CMD_DEC:
		doCmdDec();
		break;
	case CMD_GOTO:
		doCmdGoto();
		break;
	default:
		GLB_error(ERR_SYNTAX);
		return;
	}
}

void Compiler::doCmdPrototype()
{
	bool is_function;
	bool is_inline = false;
	BasicObject *next_bobj;
	BasicObject *bobj;

	bobj = parser->getObjectWithType(OBJ_CMD, "SUB or FUNCTION");
	if (bobj->val.numeric == CMD_INLINE) {
		is_inline = true;
		bobj = parser->getObjectWithType(OBJ_CMD, "SUB or FUNCTION");
	}
	checkNotSegment(SEG_INTR | SEG_SUB, "PROTOTYPE");
	if (bobj->val.numeric != CMD_SUB && bobj->val.numeric != CMD_FUNCTION)
		GLB_error(ERR_NOTOKEN, "SUB or FUNCTION");
	is_function = bobj->val.numeric == CMD_FUNCTION;
	next_bobj = parser->consumeNextBasicObj();
	doSubroutine(next_bobj, is_function, false);
	sub_head->is_inline = is_inline;
}

void Compiler::doCmdWhile()
{
	BasicObject *bobj;

	bobj = parser->retrieveNextBasicObject();
	checkNotSegment(SEG_TOP, "WHILE");
	emitTin("BEGIN ");
	if (bobj->otype == OBJ_EOL) {
		if (!loop_stack.addEntry(CMD_LOOP, line_no))
			GLB_error(ERR_STACK_OVER, "WHILE");
	} else {
		if (compileExpression())
			GLB_error(ERR_TYPE_MISMATCH);
		emitTin("WHILE ");
		loop_stack.addEntry(CMD_WHILE, line_no);
	}
}

void Compiler::doCmdLoop()
{
	int line;
	enum cmd_t cmd;

	checkNotSegment(SEG_TOP, "LOOP");
	if (!loop_stack.popEntry(&cmd, &line))
		GLB_error(ERR_WITHOUT, "LOOP", "WHILE");
	if (cmd == CMD_WHILE) {
		emitTin("REPEAT ");
	} else {
		if (cmd != CMD_LOOP)
			GLB_error(ERR_TERM_MISMATCH, line, "LOOP");
		emitTin("AGAIN ");
		is_top_level = true;
	}
}

void Compiler::doCmdRepeat()
{
	checkNotSegment(SEG_TOP, "REPEAT");
	if (!loop_stack.addEntry(CMD_REPEAT, line_no))
		GLB_error(ERR_STACK_OVER, "REPEAT");
	emitTin("BEGIN ");
}

void Compiler::doCmdUntil()
{
	int line;
	enum cmd_t cmd;

	checkNotSegment(SEG_TOP, "UNTIL");
	if (!loop_stack.popEntry(&cmd, &line))
		GLB_error(ERR_WITHOUT, "UNTIL", "REPEAT");
	if (cmd != CMD_REPEAT)
		GLB_error(ERR_TERM_MISMATCH, line, "UNTIL");
	if (compileExpression())
		GLB_error(ERR_TYPE_MISMATCH);
	emitTin("UNTIL ");
}

void Compiler::doCmdFor()
{
	char buf[12];
	int direction;
	BasicObject *bobj;

	checkNotSegment(SEG_TOP, "FOR");
	if (!loop_stack.addEntry(CMD_FOR, line_no))
		GLB_error(ERR_STACK_OVER, "FOR");
	bobj = parser->consumeNextBasicObj();
	if (bobj->vtype != VAR_SCALAR)
		GLB_error(ERR_TYPE_MISMATCH);
	doAssign(bobj);
	emitTin("OVER ! ");
	if (parser->getNextCmd(CMD_TO)) {
		direction = 1;
		strcpy(buf, "<=");
	} else {
		if (!parser->getNextCmd(CMD_DOWNTO))
			GLB_error(ERR_NOTOKEN, "TO or DOWNTO");
		direction = -1;
		strcpy(buf, ">=");
	}
	emitTin("BEGIN DUP @ ");
	if (compileExpression())
		GLB_error(ERR_TYPE_MISMATCH);
	emitTin("%s WHILE ", buf);
	if (parser->getNextCmd(CMD_STEP)) {
		if (compileExpression())
			GLB_error(ERR_TYPE_MISMATCH);
	} else {
		emitTin("%d # ", direction);
	}
}

void Compiler::doCmdNext()
{
	int line;
	enum cmd_t cmd;

	checkNotSegment(SEG_TOP, "NEXT");
	if (!loop_stack.popEntry(&cmd, &line))
		GLB_error(ERR_WITHOUT, "NEXT", "FOR");
	if (cmd != CMD_FOR)
		GLB_error(ERR_TERM_MISMATCH, line, "NEXT");
	emitTin("OVER @ + OVER ! REPEAT DROP ");
}

void Compiler::doCmdDim()
{
	const char *tin_type;
	int args[16];
	int argc;
	int total_size;
	bool is_string;
	char accessor_ident[168];
	BasicObject *bobj;
	char buf[168];
	int i, j;

	checkNotSegment(SEG_INTR | SEG_SUB, "DIM");
	do {
		if (parser->checkNextBasicObjType(OBJ_IDENT)) {
			bobj = parser->consumeNextBasicObj();
			if (bobj->vtype == VAR_ARRAY)
				emitTin("CREATE %s 1 STRINGS RESERVE\n",
					bobj->val.symbolic);
			else
				emitTin("VARIABLE %s\n", bobj->val.symbolic);
			addVariable(bobj->val.symbolic, VAR_SCALAR);
		} else {
			if (!parser->checkNextBasicObjType(OBJ_SUB))
				GLB_error(ERR_NOTOKEN, "DIM");
			bobj = parser->consumeNextBasicObj();
			argc = 0;
			total_size = 1;
			is_string = bobj->vtype == VAR_ARRAY;
			strcpy(buf, bobj->val.symbolic);
			sprintf(accessor_ident, "%s[]", buf);
			emitTin("CREATE %s ", buf);
			addVariable(buf, VAR_ARRAY);
			do {
				bobj = parser->getObjectWithType(OBJ_NUM,
								 "Literal");
				args[argc++] = bobj->val.numeric;
				total_size *= bobj->val.numeric;
			} while (parser->requireRop(ROP_COMMA));
			if (!parser->requireRop(ROP_CPAREN))
				GLB_error(ERR_NOTOKEN, ")");
			if (is_string)
				tin_type = "STRINGS";
			else
				tin_type = "CELLS";
			emitTin("%d %s RESERVE\n", total_size, tin_type);
			addNewSub(accessor_ident, true, VAR_SCALAR);
			sub_head->addArgument(0, VAR_SCALAR);
			if (argc > 1)
				emitTin(":n %s ( i*x -- a ) ", accessor_ident);
			for (i = 0; i < argc - 1; ++i) {
				sub_head->addArgument(0, VAR_SCALAR);
				total_size = 1;
				for (j = argc - 1 - i; j < argc; ++j)
					total_size *= args[j];
				emitTin("SWAP %d # * + ", total_size);
			}
			if (argc > 1)
				emitTin("%d # N* %s + ;\n", is_string != false ? 8 : 2,
					buf);
		}
	} while (parser->requireRop(ROP_COMMA));
}

void Compiler::doCmdLocal()
{
	BasicObject *bobj;

	// Locals can only be declared at the start of a subroutine.
	checkNotSegment(SEG_INTR | SEG_SUB, "LOCAL");

	do {
		bobj = parser->getObjectWithType(OBJ_IDENT, "Identifier");
		if (bobj->vtype == VAR_ARRAY)
			GLB_error(ERR_TYPE_MISMATCH);
		sub_head->addLocal(bobj->val.symbolic, bobj->vtype);
		addSubLocal(bobj->val.symbolic, bobj->vtype);
	} while (parser->requireRop(ROP_COMMA));
}

void Compiler::doCmdReturn(bool eof)
{
	checkNotSegment(SEG_INTR | SEG_TOP, "RETURN");
	if (sub_head->is_function && compileExpression() != sub_head->ret_vtype)
		GLB_error(ERR_TYPE_MISMATCH);
	if (sub_head->num_args > 0 || sub_head->num_locals > 0) {
		// Local variable epilog.  Cleans locals off the stack,
		// including parameters.  The argument is the number of
		// bytes to drop off the stack.
		// A different word ("FLEPILOG") is emitted for functions
		// because the return value at TOS must be preserved.
		emitTin("%d %sEPILOG ", 4 * (sub_head->num_args + sub_head->num_locals),
			sub_head->is_function ? "FL" : "L");
	}

	if (eof) {
		emitTin("; ");
		if (sub_head->can_be_naked) {
			sub_head->tin_start->text[1] = 'n';
		}
	} else
		emitTin(";r ");

	if (!loop_stack.getStackPtr())
		is_top_level = true;
}

void Compiler::doCmdMap()
{
	BasicObject *bobj;

	checkNotSegment(SEG_INTR | SEG_SUB, "MAP");
	do {
		bobj = parser->consumeNextBasicObj();
		if (bobj->otype != OBJ_NUM)
			GLB_error(ERR_SYNTAX);
		emitTin("$%x H, ", bobj->val.numeric);
	} while (parser->requireRop(ROP_COMMA));
}

void Compiler::doCmdData(unsigned int size)
{
	const char *minus;
	BasicObject *bobj;
	bool had_unary_minus;

	had_unary_minus = false;
	checkNotSegment(SEG_INTR | SEG_SUB, "DATA");
	const char *comma;
	switch (size) {
	case 1: comma = "B,"; break;
	case 2: comma = "H,"; break;
	case 4: comma = ","; break;
	default: abort();
	}
	do {
		bobj = parser->consumeNextBasicObj();
		if (bobj->otype == OBJ_NUM) {
			if (had_unary_minus)
				minus = "-";
			else
				minus = "";
			had_unary_minus = false;
			emitTin("%s$%x %s ", minus, bobj->val.numeric, comma);
		} else {
			if (had_unary_minus || bobj->otype != OBJ_OP ||
			    bobj->val.numeric != OP_MINUS)
				GLB_error(ERR_SYNTAX);
			had_unary_minus = true;
		}
	} while (had_unary_minus || parser->requireRop(ROP_COMMA));
}

void Compiler::doCmdRead(unsigned int size)
{
	BasicObject *bobj;

	checkNotSegment(SEG_TOP, "READ");
	emitTin(">A ");
	do {
		switch (size) {
		case 4: emitTin("@A ");	break;
		case 2: emitTin("H@A "); break;
		case 1: emitTin("C@A "); break;
		default: abort();
		}
		bobj = parser->consumeNextBasicObj();
		doRval(bobj);
		emitTin("! ");
	} while (parser->requireRop(ROP_COMMA));
	emitTin("A> ");
}

void Compiler::doCmdRestore()
{
	checkNotSegment(SEG_TOP, "RESTORE");
	if (compileExpression())
		GLB_error(ERR_TYPE_MISMATCH);
	emitTin(".IDATA ! ");
}

void Compiler::doCmdSub()
{
	BasicObject *bobj;

	checkNotSegment(SEG_SUB, "SUB");
	bobj = parser->consumeNextBasicObj();
	doSubroutine(bobj, false, true);
	cur_seg = SEG_SUB;
	is_top_level = false;
}

void Compiler::doCmdFunction()
{
	BasicObject *bobj;

	checkNotSegment(SEG_INTR | SEG_SUB, "FUNCTION");
	bobj = parser->consumeNextBasicObj();
	doSubroutine(bobj, true, true);
	cur_seg = SEG_SUB;
	is_top_level = false;
}

void Compiler::doCmdIf()
{
	BasicObject *bobj;
	int line;
	enum cmd_t cmd;

	checkNotSegment(SEG_TOP, "IF");
	if (compileExpression())
		GLB_error(ERR_TYPE_MISMATCH);
	emitTin("IF ");
	if (!loop_stack.addEntry(CMD_IF, line_no))
		GLB_error(ERR_STACK_OVER, "IF");
	if (parser->getNextCmd(CMD_THEN)) {
		bobj = parser->consumeNextBasicObj();
		compileBasicObject(bobj);
		emitTin("THEN ");
		loop_stack.popEntry(&cmd, &line);
	}
}

void Compiler::doCmdElse()
{
	int line;
	enum cmd_t cmd;

	checkNotSegment(SEG_TOP, "ELSE");
	if (!loop_stack.popEntry(&cmd, &line))
		GLB_error(ERR_WITHOUT, "ELSE", "IF");
	if (cmd != CMD_IF && cmd != CMD_ELSE)
		GLB_error(ERR_TERM_MISMATCH, line, "ELSE");
	emitTin("ELSE ");
	loop_stack.addEntry(cmd, line_no);
	if (parser->getNextCmd(CMD_IF)) {
		if (compileExpression())
			GLB_error(ERR_TYPE_MISMATCH);
		emitTin("IF ");
		loop_stack.addEntry(CMD_ELSE, line_no);
	}
}

void Compiler::doCmdEndIf()
{
	int line;
	enum cmd_t cmd;

	checkNotSegment(SEG_TOP, "END IF");
	if (!loop_stack.popEntry(&cmd, &line))
		GLB_error(ERR_WITHOUT, "END", "IF");
	while (cmd == CMD_ELSE) {
		emitTin("THEN ");
		if (!loop_stack.popEntry(&cmd, &line))
			GLB_error(ERR_WITHOUT, "END", "IF");
	}
	if (cmd != CMD_IF)
		GLB_error(ERR_TERM_MISMATCH, line, "END IF");
	emitTin("THEN ");
}

void Compiler::doCmdSelect()
{
	checkNotSegment(SEG_TOP, "SELECT");
	if (compileExpression())
		GLB_error(ERR_TYPE_MISMATCH);
	if (!loop_stack.addEntry(CMD_SELECT, line_no))
		GLB_error(ERR_STACK_OVER, "SELECT");
}

void Compiler::doCmdCase()
{
	int line;
	enum cmd_t cmd;

	checkNotSegment(SEG_TOP, "CASE");
	if (!loop_stack.popEntry(&cmd, &line))
		GLB_error(ERR_STACK_UNDER, "CASE");
	if (cmd == CMD_CASE)
		emitTin("ELSE ");
	else
		if (cmd != CMD_SELECT)
			GLB_error(ERR_TERM_MISMATCH, line, "CASE");
	loop_stack.addEntry(cmd, line);
	emitTin("DUP ");
	if (compileExpression())
		GLB_error(ERR_TYPE_MISMATCH);
	emitTin("= IF ");
	if (!loop_stack.addEntry(CMD_CASE, line_no))
		GLB_error(ERR_WITHOUT, "CASE", "SELECT");
}

void Compiler::doCmdDefault()
{
	int line;
	enum cmd_t cmd;

	checkNotSegment(SEG_TOP, "DEFAULT");
	if (!loop_stack.popEntry(&cmd, &line))
		GLB_error(ERR_WITHOUT, "DEFAULT", "SELECT");
	if (cmd != CMD_CASE)
		GLB_error(ERR_WITHOUT, "DEFAULT", "CASE");
	emitTin("ELSE ");
	if (!loop_stack.addEntry(CMD_DEFAULT, line_no))
		GLB_error(ERR_STACK_OVER, "DEFAULT");
}

void Compiler::doCmdEnd()
{
	int line;
	enum cmd_t cmd;
	BasicObject *bobj;

	bobj = parser->getObjectWithType(OBJ_CMD, "END");
	checkNotSegment(SEG_TOP, "END");
	switch (bobj->val.numeric) {
	case CMD_IF:
		doCmdEndIf();
		break;
	case CMD_SELECT:
		if (!loop_stack.popEntry(&cmd, &line))
			GLB_error(ERR_WITHOUT, "END", "SELECT");
		while (cmd != CMD_SELECT) {
			emitTin("THEN ");
			loop_stack.popEntry(&cmd, &line);
		}
		emitTin("DROP ");
		break;
	case CMD_SUB:
		if (sub_head->is_function)
			GLB_error(ERR_NOTOKEN, "FUNCTION");
		if (!is_top_level)
			doCmdReturn(true);
		cur_seg = SEG_TOP;
		break;
	case CMD_FUNCTION:
		if (!sub_head->is_function)
			GLB_error(ERR_NOTOKEN, "SUB");
		if (!is_top_level)
			GLB_error(ERR_RETVAL);
		emitTin(";l ");
		if (sub_head->can_be_naked)
			sub_head->tin_start->text[1] = 'n';
		cur_seg = SEG_TOP;
		break;
	default:
		if (bobj->val.numeric != CMD_INTERRUPT)
			GLB_error(ERR_SYNTAX);
		emitTin("EXIT ");
		cur_seg = SEG_TOP;
		break;
	}
}

void Compiler::doCmdReset()
{
	emitTin("[ASM RESET B, ASM] ");
}

void Compiler::doCmdInterrupt()
{
	BasicObject *bobj;
	bool iwram = false;

	if (parser->checkNextBasicObjType(OBJ_CMD)) {
		parser->getNextCmd(CMD_IWRAM);
		iwram = true;
	}
	bobj = parser->getObjectWithType(OBJ_IDENT, "Identifier");
	checkNotSegment(SEG_INTR | SEG_SUB, "INTERRUPT");
	if (bobj->vtype)
		GLB_error(ERR_TYPE_MISMATCH);
	addNewSub(bobj->val.symbolic, true, VAR_SCALAR);
	emitTin("INTERRUPT %s%s ", iwram ? "IWRAM " : "", bobj->val.symbolic);
	cur_seg = SEG_INTR;
	is_top_level = false;
}

void Compiler::doCmdExit()
{
	checkNotSegment(SEG_SUB | SEG_TOP, "EXIT");
	emitTin("EXIT ");
	cur_seg = SEG_TOP;
}

void Compiler::doCmdInc()
{
	BasicObject *bobj;

	bobj = parser->consumeNextBasicObj();
	if (bobj->vtype && bobj->vtype != VAR_FIXEDP)
		GLB_error(ERR_TYPE_MISMATCH);
	doRval(bobj);
	emitTin("DUP @ ");
	if (parser->requireRop(ROP_COMMA)) {
		if (compileExpression() != bobj->vtype)
			GLB_error(ERR_TYPE_MISMATCH);
		emitTin("+ SWAP ! ");
	} else {
		if (bobj->vtype)
			emitTin("$100 # SWAP ! ");
		else
			emitTin("1+ SWAP ! ");
	}
}

void Compiler::doCmdDec()
{
	BasicObject *bobj;

	bobj = parser->consumeNextBasicObj();
	if (bobj->vtype && bobj->vtype != VAR_FIXEDP)
		GLB_error(ERR_TYPE_MISMATCH);
	doRval(bobj);
	emitTin("DUP @ ");
	if (parser->requireRop(ROP_COMMA)) {
		if (compileExpression() != bobj->vtype)
			GLB_error(ERR_TYPE_MISMATCH);
	} else {
		emitTin("$%x # ", bobj->vtype != VAR_SCALAR ? 256 : 1);
	}
	emitTin("- SWAP ! ");
}

void Compiler::doCmdGoto()
{
	BasicObject *bobj = parser->consumeNextBasicObj();
	if (bobj->otype != OBJ_IDENT)
		GLB_error(ERR_TYPE_MISMATCH);
	emitTin("GOTO %s::%s ", sub_head->ident, bobj->val.symbolic);
}

void Compiler::doLabel(BasicObject *bobj)
{
	BasicObject *same_line_bobj;

	if (cur_seg & SEG_SUB) {
		emitTin("CLABEL %s::%s ", sub_head->ident, bobj->val.symbolic);
		is_top_level = false;
	} else if (!strcasecmp(bobj->val.symbolic, "start")) {
		emitTin(":  START ");
		addNewSub(bobj->val.symbolic, false, VAR_SCALAR);
		is_top_level = false;
		cur_seg = SEG_SUB;
	} else {
		emitTin("DATA: %s ", bobj->val.symbolic);
		if (var_head->findByIdent(bobj->val.symbolic))
			GLB_error(ERR_DUP_IDENT, bobj->val.symbolic);
		addVariable(bobj->val.symbolic, VAR_FIXEDP); // XXX: why fixed point?
	}
	if (parser->retrieveNextBasicObject()->otype != OBJ_EOL) {
		same_line_bobj = parser->consumeNextBasicObj();
		compileBasicObject(same_line_bobj);
	}
}

void Compiler::doLval(BasicObject *bobj)
{
	Subroutine *sub;

	sub = sub_head->findByIdent(bobj->val.symbolic);
	if (sub) {
		checkNotSegment(SEG_TOP, bobj->val.symbolic);
		if (is_top_level == true)
			GLB_error(ERR_UNREACHABLE);
		if (sub->is_function == true)
			GLB_error(ERR_NEED_LVAL, bobj->val.symbolic);
		callSubroutine(sub);
	} else {
		doLvalNotSub(bobj, false);
	}
}

void Compiler::doLvalNotSub(BasicObject *bobj, bool array)
{
	if (parser->requireRop(ROP_COLON)) {
		doLabel(bobj);
	} else {
		if (!var_head->findByIdent(bobj->val.symbolic) &&
		    !sub_args_head->findByIdent(bobj->val.symbolic) &&
		    !sub_locals_head->findByIdent(bobj->val.symbolic))
			GLB_error(ERR_UNK_IDENT, bobj->val.symbolic);
		checkNotSegment(SEG_TOP, bobj->val.symbolic);
		if (is_top_level == true)
			GLB_error(ERR_UNREACHABLE);
		if (array || bobj->vtype == VAR_ARRAY) {
			// Use unoptimized path for array/string lvalues.
			doAssign(bobj);
			if (bobj->vtype == VAR_ARRAY)
				emitTin("MOVE ");
			else
				emitTin("SWAP ! ");
		} else {
			if (parser->getObjectWithType(OBJ_OP, "=")->val.numeric != OP_EQ)
				GLB_error(ERR_NOTOKEN, "=");
			if (compileExpression() != bobj->vtype)
				GLB_error(ERR_TYPE_MISMATCH);
			doRval(bobj);
			emitTin("! ");
		}
	}
}

void Compiler::doRvalFunction(BasicObject *bobj)
{
	Subroutine *sub;

	sub = sub_head->findByIdent(bobj->val.symbolic);
	if (!sub)
		GLB_error(ERR_UNK_IDENT, bobj->val.symbolic);
	if (!sub->is_function)
		GLB_error(ERR_NEED_RVAL, bobj->val.symbolic);
	callSubroutine(sub);
	if (!parser->requireRop(ROP_CPAREN))
		GLB_error(ERR_TOO_MANY_ARGS, sub->ident);
}

void Compiler::doRvalArray(BasicObject *bobj)
{
	Subroutine *sub;
	char buf[256];

	sprintf(buf, "%s[]", bobj->val.symbolic);
	sub = sub_head->findByIdent(buf);
	if (!sub)
		GLB_error(ERR_UNK_IDENT, bobj->val.symbolic);
	if (!sub->is_function)
		GLB_error(ERR_NEED_RVAL, bobj->val.symbolic);
	if (sub->num_args == 1) {
		emitTin("%s ", bobj->val.symbolic);
		compileExpression();
		if (bobj->vtype == VAR_ARRAY)
			emitTin("8 # n* + ");
		else
			emitTin("2 # n* + ");
	} else
		callSubroutine(sub);
	if (!parser->requireRop(ROP_CBRACK))
		GLB_error(ERR_TOO_MANY_ARGS, sub->ident);
}

void Compiler::doAssign(BasicObject *bobj)
{
	doRval(bobj);
	if (parser->getObjectWithType(OBJ_OP, "=")->val.numeric != OP_EQ)
		GLB_error(ERR_NOTOKEN, "=");
	if (compileExpression() != bobj->vtype)
		GLB_error(ERR_TYPE_MISMATCH);
}

void Compiler::doRval(BasicObject *bobj)
{
	Variable *arr;
	Variable *global;

	if (bobj->otype == OBJ_ARR) {
		arr = var_head->findByIdent(bobj->val.symbolic);
		if (!arr)
			GLB_error(ERR_UNK_IDENT, bobj->val.symbolic);
		if (!(arr->vtype & VAR_ARRAY))  // XXX: Is this "&" instead of "==" a weird optimization artifact or a bug?
			GLB_error(ERR_NOT_ARRAY, bobj->val.symbolic);
		doRvalArray(bobj);
	} else {
		if (bobj->otype)
			GLB_error(ERR_NOTOKEN, "L-value");
		if (sub_args_head->findByIdent(bobj->val.symbolic)) {
			emitTin("%s::%s # +R ", sub_head->ident,
				bobj->val.symbolic);
		} else if (sub_locals_head->findByIdent(bobj->val.symbolic)) {
			emitTin("%s::%s # +R ", sub_head->ident,
				bobj->val.symbolic);
		} else {
			global = var_head->findByIdent(bobj->val.symbolic);
			if (!global)
				GLB_error(ERR_UNK_IDENT, bobj->val.symbolic);
			if (global->vtype & VAR_ARRAY)
				GLB_error(ERR_ARRAY, bobj->val.symbolic);
			emitTin("%s ", bobj->val.symbolic);
		}
	}
}

enum vartype_t Compiler::compileExpression()
{
	BasicObject *bobj4;
	BasicObject *bobj3;
	enum vartype_t vtype;
	BasicObject *bobj2;
	BasicObject *bobj1;
	BasicObject *bobj;
	BasicObject *bobj5;
	enum vartype_t ret;
	BasicObject *first_bobj;
	BasicObject *next_bobj;
	Expression expr(tin_tail);

	first_bobj = parser->cur_basic_obj;
	while (true) {
		next_bobj = parser->retrieveNextBasicObject();
		if (!next_bobj)
			break;
		if (next_bobj->otype) {
			switch (next_bobj->otype) {
			case OBJ_SUB:
				expr.addOperand(next_bobj->vtype);
				bobj = parser->consumeNextBasicObj();
				doRvalFunction(bobj);
				break;
			case OBJ_ARR:
				expr.addOperand(next_bobj->vtype);
				bobj1 = parser->consumeNextBasicObj();
				doRvalArray(bobj1);
				if (next_bobj->vtype != VAR_ARRAY)
					emitTin("@ ");
				break;
			case OBJ_OP:
				bobj2 = parser->consumeNextBasicObj();
				next_bobj = bobj2;
				if (bobj2->val.numeric) {
					expr.enqueueOperation(
						(enum op_t)next_bobj->val.numeric);
				} else {
					vtype = compileExpression();
					expr.addOperand(vtype);
					if (!parser->requireRop(ROP_CPAREN))
						GLB_error(ERR_NOTOKEN, ")");
				}
				break;
			case OBJ_NUM:
				expr.addOperand(next_bobj->vtype);
				bobj3 = parser->consumeNextBasicObj();
				emitTin("$%x # ", bobj3->val.numeric);
				break;
			default:
				if (next_bobj->otype != OBJ_STR)
					goto done;
				expr.addOperand(next_bobj->vtype);
				bobj4 = parser->consumeNextBasicObj();
				emitTin("c\" %s\" ", bobj4->val.symbolic);
				break;
			}
		} else {
			expr.addOperand(next_bobj->vtype);
			bobj5 = parser->consumeNextBasicObj();
			doOperand(bobj5);
		}
	}
done:
	if (first_bobj == parser->cur_basic_obj)
		GLB_error(ERR_NOTOKEN, "Expression");
	ret = expr.compileAllOps();
	return ret;
}

void Compiler::doOperand(BasicObject *bobj)
{
	Subroutine *sub;
	Variable *var;

	if (sub_args_head->findByIdent(bobj->val.symbolic)) {
		emitTin("%s::%s # +R @ ", sub_head->ident, bobj->val.symbolic);
	} else if (sub_locals_head->findByIdent(bobj->val.symbolic)) {
		emitTin("%s::%s # +R @ ", sub_head->ident, bobj->val.symbolic);
	} else {
		var = var_head->findByIdent(bobj->val.symbolic);
		if (var) {
			if (var->vtype == VAR_FIXEDP) {
				emitTin("%s ", bobj->val.symbolic);
			} else {
				if (bobj->vtype == VAR_ARRAY)
					emitTin("%s ", bobj->val.symbolic);
				else
					emitTin("%s @ ", bobj->val.symbolic);
			}
		} else {
			sub = sub_head->findByIdent(bobj->val.symbolic);
			if (!sub)
				GLB_error(ERR_UNK_IDENT, bobj->val.symbolic);
			if (!sub->is_function)
				GLB_error(ERR_NEED_RVAL, bobj->val.symbolic);
			if (sub->num_args > 0)
				GLB_error(ERR_TOO_FEW_ARGS, bobj->val.symbolic);

			emitTin("%s ", bobj->val.symbolic);
			if (!sub->is_inline)
				sub_head->can_be_naked = false;
		}
	}
}

void Compiler::callSubroutine(Subroutine *sub)
{
	bool rop;
	int i;

	for (i = 0; i < sub->num_args; ++i) {
		if (compileExpression() != sub->arg_vtype[i])
			GLB_error(ERR_TYPE_MISMATCH);
		rop = parser->requireRop(ROP_COMMA);
		if (rop && i >= sub->num_args - 1)
			GLB_error(ERR_TOO_MANY_ARGS, sub->ident);
		if (!rop)
			if (i < sub->num_args - 1)
				GLB_error(ERR_TOO_FEW_ARGS, sub->ident);
	}
	emitTin("%s ", sub->ident);
	if (!sub->is_inline)
		sub_head->can_be_naked = false;
}

int LoopStack::getStackPtr()
{
	return stack_ptr;
}

Expression::Expression(TIN *tin_head)
{
	this->tin_head = tin_head;
	operation_idx = 0;
	operand_idx = 0;
}

void Expression::compileOperation(int num_ops, const char *tin_op,
				  enum vartype_t vtype)
{
	enum vartype_t vt;

	--operand_idx;
	vt = operand[operand_idx];
	while (true) {
		--num_ops;
		if (num_ops <= 0)
			break;
		if (!operand_idx)
			GLB_error(ERR_EXPR);
		--operand_idx;
		if (operand[operand_idx] != vt)
			GLB_error(ERR_TYPE_MISMATCH);
	}
	tin_head->addCode(tin_op);
	addOperand(vtype);
}

void Expression::addOperand(enum vartype_t vtype)
{
	operand[operand_idx] = vtype;
	++operand_idx;
}

void Expression::enqueueOperation(enum op_t op)
{
	Expression *i;

	// Emit enqueued operations from the top until one is
	// reached that has a lower priority than op.

	for (i = this;
	     i->operation_idx > 0 && i->operation[i->operation_idx - 1] >= op;
	     i->compileNextOperation())
		;
	i->operation[i->operation_idx] = op;
	++i->operation_idx;
}

void Expression::compileNextOperation()
{
	enum vartype_t vtype;

	if (!operand_idx)
		GLB_error(ERR_EXPR);
	vtype = operand[operand_idx - 1];
	if (vtype) {
		if (vtype == VAR_ARRAY) {
			compileArrayOp();
		} else {
			if (vtype != VAR_FIXEDP)
				GLB_error(ERR_TYPE_MISMATCH);
			compileFixedPointOp();
		}
	} else {
		compileScalarOp();
	}
}

void Expression::compileScalarOp()
{
	--operation_idx;
	switch (operation[operation_idx]) {
	case OP_PLUS:
		compileOperation(2, "+ ", VAR_SCALAR);
		break;
	case OP_MULT:
		compileOperation(2, "* ", VAR_SCALAR);
		break;
	case OP_SL:
		compileOperation(2, "N* ", VAR_SCALAR);
		break;
	case OP_SR:
		compileOperation(2, "N/ ", VAR_SCALAR);
		break;
	case OP_AND:
		compileOperation(2, "AND ", VAR_SCALAR);
		break;
	case OP_OR:
		compileOperation(2, "OR ", VAR_SCALAR);
		break;
	case OP_NAND:
		compileOperation(2, "NAND ", VAR_SCALAR);
		break;
	case OP_NOR:
		compileOperation(2, "NOR ", VAR_SCALAR);
		break;
	case OP_XOR:
		compileOperation(2, "XOR ", VAR_SCALAR);
		break;
	case OP_LNOT:
		compileOperation(1, "0= ", VAR_SCALAR);
		break;
	case OP_BNOT:
		compileOperation(1, "COM ", VAR_SCALAR);
		break;
	case OP_DIV:
		compileOperation(2, "/ ", VAR_SCALAR);
		break;
	case OP_MOD:
		compileOperation(2, "MOD ", VAR_SCALAR);
		break;
	case OP_EQ:
		compileOperation(2, "= ", VAR_SCALAR);
		break;
	case OP_NE:
		compileOperation(2, "<> ", VAR_SCALAR);
		break;
	case OP_LT:
		compileOperation(2, "< ", VAR_SCALAR);
		break;
	case OP_GT:
		compileOperation(2, "> ", VAR_SCALAR);
		break;
	case OP_LE:
		compileOperation(2, "<= ", VAR_SCALAR);
		break;
	case OP_GE:
		compileOperation(2, ">= ", VAR_SCALAR);
		break;
	case OP_MINUS:
		if (operand_idx <= 1)
			compileOperation(1, "COM 1+ ", VAR_SCALAR);
		else
			compileOperation(2, "- ", VAR_SCALAR);
		break;
	case OP_UMINUS:
		compileOperation(1, "COM 1+ ", VAR_SCALAR);
		break;
	default:
		GLB_error(ERR_ILLEGAL_OP);
		return;
	}
}

void Expression::compileArrayOp()
{
	--operation_idx;
	switch (operation[operation_idx]) {
	case OP_PLUS:
		compileOperation(2, "APPEND$ ", VAR_ARRAY);
		break;
	case OP_EQ:
		compileOperation(2, "COMPARE 0= ", VAR_SCALAR);
		break;
	case OP_NE:
		compileOperation(2, "COMPARE 0= COM ", VAR_SCALAR);
		break;
	case OP_LT:
		compileOperation(2, "COMPARE 0< ", VAR_SCALAR);
		break;
	case OP_GT:
		compileOperation(2, "SWAP COMPARE 0< ", VAR_SCALAR);
		break;
	case OP_LE:
		compileOperation(2, "SWAP COMPARE 0< COM ", VAR_SCALAR);
		break;
	case OP_GE:
		compileOperation(2, "COMPARE 0< COM ", VAR_SCALAR);
		break;
	default:
		GLB_error(ERR_ILLEGAL_OP);
		return;
	}
}

void Expression::compileFixedPointOp()
{
	--operation_idx;
	switch (operation[operation_idx]) {
	case OP_PLUS:
		compileOperation(2, "+ ", VAR_FIXEDP);
		break;
	case OP_AND:
		compileOperation(2, "AND ", VAR_SCALAR);
		break;
	case OP_OR:
		compileOperation(2, "OR ", VAR_SCALAR);
		break;
	case OP_LNOT:
		compileOperation(1, "0= ", VAR_SCALAR);
		break;
	case OP_DIV:
		compileOperation(2, "F/ ", VAR_FIXEDP);
		break;
	case OP_MULT:
		compileOperation(2, "F* ", VAR_FIXEDP);
		break;
	case OP_EQ:
		compileOperation(2, "= ", VAR_SCALAR);
		break;
	case OP_NE:
		compileOperation(2, "<> ", VAR_SCALAR);
		break;
	case OP_LT:
		compileOperation(2, "< ", VAR_SCALAR);
		break;
	case OP_GT:
		compileOperation(2, "> ", VAR_SCALAR);
		break;
	case OP_LE:
		compileOperation(2, "<= ", VAR_SCALAR);
		break;
	case OP_GE:
		compileOperation(2, ">= ", VAR_SCALAR);
		break;
	case OP_MINUS:
		if (operand_idx <= 1)
			compileOperation(1, "COM 1+ ", VAR_FIXEDP);
		else
			compileOperation(2, "- ", VAR_FIXEDP);
		break;
	case OP_UMINUS:
		compileOperation(1, "COM 1+ ", VAR_FIXEDP);
		break;
	default:
		GLB_error(ERR_ILLEGAL_OP);
		return;
	}
}

enum vartype_t Expression::compileAllOps()
{
	Expression *i;

	for (i = this; i->operation_idx > 0; i->compileNextOperation())
		;
	if (i->operand_idx != 1)
		GLB_error(ERR_EXPR);
	return i->operand[0];
}

LoopStack::LoopStack()
{
	stack_ptr = 0;
}

bool LoopStack::addEntry(enum cmd_t cmd, int line_no)
{
	int stackp;

	stackp = stack_ptr;
	++stack_ptr;
	if (stack_ptr < 16) {
		stack[stackp].line_no = line_no;
		stack[stackp].cmd = cmd;
		return true;
	} else
		return false;
}

char LoopStack::popEntry(enum cmd_t *cmd_p, int *line_no_p)
{
	char ret;
	int stp;

	--stack_ptr;
	stp = stack_ptr;
	if (stack_ptr >= 0) {
		*line_no_p = stack[stp].line_no;
		*cmd_p = stack[stp].cmd;
		ret = true;
	} else {
		ret = false;
	}
	return ret;
}

void LoopStack::checkEmpty()
{
	if (stack_ptr > 0)
		GLB_error(ERR_UNTERM_CONTROL, stack[stack_ptr - 1].line_no);
}

Variable::Variable(const char *ident, enum vartype_t vtype)
{
	strcpy(this->ident, ident);
	this->vtype = vtype;
	next = 0;
}

Variable::~Variable()
{
	if (next)
		delete next;
}

Variable *Variable::findByIdent(const char *ident)
{
	Variable *v;

	for (v = this; v; v = v->next)
		if (!strcasecmp(v->ident, ident))
			return v;
	return 0;
}

Variable *Variable::prependNew(const char *ident, enum vartype_t vtype)
{
	Variable *var = new Variable(ident, vtype);

	var->next = this;
	return var;
}

Symbol *Symbol::newInitialSymbolList()
{
	BasicObject *bobj_true;
	BasicObject *bobj_false;
	Symbol *sym_true;
	Symbol *sym_false;

	sym_false = new Symbol("false", 0);
	sym_true = new Symbol("true", sym_false);
	bobj_false = new BasicObject(0, 1);
	sym_false->appendBasicObj(bobj_false);
	bobj_true = new BasicObject(-1, 1);
	sym_true->appendBasicObj(bobj_true);
	return sym_true;
}

Symbol::Symbol(const char *ident, Symbol *next)
{
	strcpy(this->ident, ident);
	basic_obj_head = 0;
	this->next = next;
}

Symbol::~Symbol()
{
	if (basic_obj_head)
		delete basic_obj_head;
}

void Symbol::appendBasicObj(const BasicObject *bobj)
{
	BasicObject *new_bobj;

	if (basic_obj_head) {
		new_bobj = new BasicObject(*bobj);
		basic_obj_head->append(new_bobj);
	} else {
		new_bobj = new BasicObject(*bobj);
		basic_obj_head = new_bobj;
		basic_obj_head->next = 0;
	}
}

BasicObject *Symbol::getBasicObj(const char *ident)
{
	Symbol *sym;

	for (sym = this; sym; sym = sym->next)
		if (!strcasecmp(sym->ident, ident))
			return sym->basic_obj_head;
	return 0;
}

int GLB_parseOneOption(int optind, char **argv)
{
	int next;

	if (*argv[optind] != '-' && *argv[optind] != '/') {
		next = 0;
	} else {
		if (!strcasecmp(argv[optind] + 1, "mb")) {
			option_multiboot = true;
			next = optind + 1;
		} else if (!strcasecmp(argv[optind] + 1, "mod")) {
			option_mod = true;
			next = optind + 1;
		} else if (!strcasecmp(argv[optind] + 1, "sym")) {
			option_sym = true;
			next = optind + 1;
		} else {
			if (!strcasecmp(argv[optind] + 1, "debug")) {
				option_debug = true;
				next = optind + 1;
			} else {
				if (strcasecmp(argv[optind] + 1, "o")) {
					if (!strcasecmp(argv[optind] + 1, "v"))
						GLB_exitWithMbox(
							"DBC/MF version %s",
							DB_VERSION);
					GLB_error(ERR_UNKNOWN_CFLAG,
						  argv[optind]);
				}
				option_optimize = true;
				next = optind + 1;
			}
		}
	}
	return next;
}

static void GLB_usage()
{
	GLB_failWithMbox(
		"Usage: DBC.EXE [-o|-mb|-debug] <sourcefile> [<binary>]\n", 0,
		0);
}

int GLB_parseAllOptions(int *optind, char **argv)
{
	int next;
	int i;

	for (i = 0;; ++i) {
		if (i < *optind) {
			next = GLB_parseOneOption(i, argv);
			if (next)
				continue;
		}
		break;
	}
	if (i == *optind)
		GLB_usage();
	return i;
}

void GLB_runMF(const char *lpFileName, const char *outfile)
{
	const char *omb;
	const char *odbg;
	const char *oopt;
	const char *omod;
	const char *osym;
	char options[256];

	if (option_optimize)
		oopt = "-o ";
	else
		oopt = (char *)"";
	if (option_debug)
		odbg = "-debug ";
	else
		odbg = (char *)"";
	if (option_multiboot)
		omb = "-mb ";
	else
		omb = (char *)"";
	if (option_mod)
		omod = "-mod ";
	else
		omod = "";
	if (option_sym)
		osym = "-sym ";
	else
		osym = "";
	sprintf(options, "%s%s%s%s%s", omod, omb, odbg, oopt, osym);
	GLB_runProgramWithArgs("mf" EXE, options, lpFileName, outfile,
			       !option_debug);
}

void GLB_main(int argc, char **argv)
{
	Compiler compiler;
	char FileName[256];
	char outfile[256];
	int num_opts;
	int binary_idx;

	gCompiler = &compiler;
	num_opts = GLB_parseAllOptions(&argc, argv);
	binary_idx = num_opts + 1;
	if (binary_idx < argc)
		strcpy(outfile, argv[binary_idx]);
	else {
		strcpy(outfile, argv[num_opts]);
		char *dot = strrchr(outfile, '.');
		if (dot)
			*dot = 0;
		strcat(outfile, ".gba");
	}

	if (num_opts + 1 > argc)
		GLB_usage();

	compiler.parseFile(argv[num_opts]);
	compiler.emitTin("#LINE    1 ");
	compiler.compile();
	compiler.emitTin("\nENTRY START\n");
	compiler.emitTin("PROGRAM\" %s\"\n\n", outfile);
	sprintf(FileName, "~a.out");
	compiler.writeOutput(FileName);
	GLB_runMF(FileName, outfile);
}

#ifdef __WIN32__
void WIN_splitCommandLine(const char *cmdline)
{
	char *arg;
	char *arg_p;
	char *argv[33];
	int argc;
	const char *cmd_p;

	argc = 0;
	while (*cmdline) {
		while (*cmdline <= ' ' && *cmdline)
			++cmdline;
		if (*cmdline) {
			arg = (char *)malloc(0x100u);
			argv[argc] = arg;
			arg_p = arg;
			++argc;
			if (*cmdline == '"') {
				for (cmd_p = cmdline + 1;
				     *cmd_p && *cmd_p != '"'; ++cmd_p)
					*arg_p++ = *cmd_p;
				cmdline = cmd_p + 1;
			} else {
				while (*cmdline > ' ')
					*arg_p++ = *cmdline++;
			}
			*arg_p = 0;
		}
	}
	GLB_main(argc, argv);
	while (true) {
		--argc;
		if (argc < 0)
			break;
		free(argv[argc]);
	}
}
#endif

#ifdef __WIN32__
int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
		      char *lpCmdLine, int nShowCmd)
{
	WIN_splitCommandLine(lpCmdLine);
	return 0;
}
#else
int main(int argc, char **argv)
{
	GLB_main(argc - 1, argv + 1);
	return 0;
}
#endif

Parser::Parser(const char *filename, Symbol *symbol_list)
{
	long size;
	FILE *fp;

	strcpy(this->filename, filename);

	fp = fopen(filename, "r+t");
	if (!fp) {
		fp = fopen(stolower(filename), "r+t");
		if (!fp)
			GLB_error(ERR_OPEN_FAIL, filename);
	}

	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	text = new char[size + 1];
	rewind(fp);

	text[fread(this->text, 1, size, fp)] = 0;
	text_ptr = this->text;

	cur_line = 1;
	basic_obj_head = NULL;

	if (symbol_list) {
		symbol_head = symbol_list;
		is_subparser = true;
	} else {
		symbol_head = Symbol::newInitialSymbolList();
		is_subparser = false;
	}

	fclose(fp);
}

Parser::~Parser()
{
	Symbol *sym;
	BasicObject *bobj;

	delete[] text;

	if (basic_obj_head) {
		bobj = basic_obj_head;
		if (bobj)
			delete bobj;
	}
	if (symbol_head && !is_subparser) {
		sym = symbol_head;
		if (sym)
			delete sym;
	}
}

char Parser::getChar()
{
	while (*text_ptr == ' ' || *text_ptr == '\t' || *text_ptr == '\r')
		++text_ptr;
	return *text_ptr;
}

char Parser::skipNewline()
{
	int c;

	if (getChar() != '\n')
		GLB_error(ERR_SYNTAX);
	while (*text_ptr <= ' ') {
		c = *text_ptr++;
		if (c == '\n')
			++cur_line;
	}
	return *text_ptr;
}

BasicObject *Parser::parseNext()
{
	BasicObject *result;
	BasicObject *bo_eol;
	BasicObject *bo_eot;
	char c;
	static int last_otype = 0;

	do {
		if (*text_ptr > ' ') {
			if (isalpha(*text_ptr) ||
			    (*text_ptr == '_' && text_ptr[1] != '\n' &&
			     text_ptr[1] != '\r')) {
				result = parseToken();
			} else {
				if (isdigit(*text_ptr)) {
					result = parseDecimal();
				} else {
					switch (*text_ptr) {
					case '#':
						result = parseDirective();
						break;
					case ';':
						result = parseComment();
						break;
					case '"':
						result = parseString('"');
						break;
					case '<':
						if (last_otype != OBJ_DIR)
							goto parse_op;
						result = parseString('>');
						result->otype = OBJ_PATH;
						break;
					case '&':
						result = parseHex();
						break;
					case '%':
						result = parseBinary();
						break;
					default:
parse_op:
						result = parseOperator();
						if (result->otype == OBJ_OP &&
						    result->val.numeric ==
						    OP_MINUS
						    && last_otype == OBJ_OP)
							result->val.numeric =
								OP_UMINUS;
						break;
					}
				}
			}
			last_otype = result->otype;
			return result;
		}
		c = *text_ptr++;
		if (!c) {
			bo_eot = new BasicObject(OBJ_EOT, cur_line);
			return bo_eot;
		}
	} while (c != '\n');
	++cur_line;
	bo_eol = new BasicObject(OBJ_EOL, cur_line);
	return bo_eol;
}

BasicObject *Parser::parseToken()
{
	int tmp;
	BasicObject *bobj;
	char token_name[256];
	char *tp;

	tp = token_name;
	// Get token name, convert to upper case.
	while (isdigit(*text_ptr) || isalpha(*text_ptr) || *text_ptr == '_') {
		tmp = *text_ptr++;
		*tp++ = toupper(tmp);
	}
	*tp = 0;
	if (!strcasecmp(token_name, "sl")) {
		bobj = new BasicObject(OP_SL, cur_line);
	} else if (!strcasecmp(token_name, "sr")) {
		bobj = new BasicObject(OP_SR, cur_line);
	} else if (!strcasecmp(token_name, "mod")) {
		bobj = new BasicObject(OP_MOD, cur_line);
	} else if (!strcasecmp(token_name, "not")) {
		bobj = new BasicObject(OP_LNOT, cur_line);
	} else if (!strcasecmp(token_name, "nand")) {
		bobj = new BasicObject(OP_NAND, cur_line);
	} else if (!strcasecmp(token_name, "nor")) {
		bobj = new BasicObject(OP_NOR, cur_line);
	} else if (!strcasecmp(token_name, "or")) {
		bobj = new BasicObject(OP_OR, cur_line);
	} else if (!strcasecmp(token_name, "xor")) {
		bobj = new BasicObject(OP_XOR, cur_line);
	} else if (!strcasecmp(token_name, "and")) {
		bobj = new BasicObject(OP_AND, cur_line);
	} else if (!strcasecmp(token_name, "reset")) {
		bobj = new BasicObject(CMD_RESET, cur_line);
	} else if (!strcasecmp(token_name, "leave")) {
		bobj = new BasicObject(CMD_LEAVE, cur_line);
	} else if (!strcasecmp(token_name, "while")) {
		bobj = new BasicObject(CMD_WHILE, cur_line);
	} else if (!strcasecmp(token_name, "loop")) {
		bobj = new BasicObject(CMD_LOOP, cur_line);
	} else if (!strcasecmp(token_name, "repeat")) {
		bobj = new BasicObject(CMD_REPEAT, cur_line);
	} else if (!strcasecmp(token_name, "until")) {
		bobj = new BasicObject(CMD_UNTIL, cur_line);
	} else if (!strcasecmp(token_name, "for")) {
		bobj = new BasicObject(CMD_FOR, cur_line);
	} else if (!strcasecmp(token_name, "to")) {
		bobj = new BasicObject(CMD_TO, cur_line);
	} else if (!strcasecmp(token_name, "downto")) {
		bobj = new BasicObject(CMD_DOWNTO, cur_line);
	} else if (!strcasecmp(token_name, "next")) {
		bobj = new BasicObject(CMD_NEXT, cur_line);
	} else if (!strcasecmp(token_name, "step")) {
		bobj = new BasicObject(CMD_STEP, cur_line);
	} else if (!strcasecmp(token_name, "if")) {
		bobj = new BasicObject(CMD_IF, cur_line);
	} else if (!strcasecmp(token_name, "then")) {
		bobj = new BasicObject(CMD_THEN, cur_line);
	} else if (!strcasecmp(token_name, "else")) {
		bobj = new BasicObject(CMD_ELSE, cur_line);
	} else if (!strcasecmp(token_name, "select")) {
		bobj = new BasicObject(CMD_SELECT, cur_line);
	} else if (!strcasecmp(token_name, "case")) {
		bobj = new BasicObject(CMD_CASE, cur_line);
	} else if (!strcasecmp(token_name, "default")) {
		bobj = new BasicObject(CMD_DEFAULT, cur_line);
	} else if (!strcasecmp(token_name, "end")) {
		bobj = new BasicObject(CMD_END, cur_line);
	} else if (!strcasecmp(token_name, "return")) {
		bobj = new BasicObject(CMD_RETURN, cur_line);
	} else if (!strcasecmp(token_name, "function")) {
		bobj = new BasicObject(CMD_FUNCTION, cur_line);
	} else if (!strcasecmp(token_name, "sub")) {
		bobj = new BasicObject(CMD_SUB, cur_line);
	} else if (!strcasecmp(token_name, "prototype")) {
		bobj = new BasicObject(CMD_PROTOTYPE, cur_line);
	} else if (!strcasecmp(token_name, "inline")) {
		bobj = new BasicObject(CMD_INLINE, cur_line);
	} else if (!strcasecmp(token_name, "dim") ||
		   !strcasecmp(token_name, "global")) {
		bobj = new BasicObject(CMD_DIM, cur_line);
	} else if (!strcasecmp(token_name, "local")) {
		bobj = new BasicObject(CMD_LOCAL, cur_line);
	} else if (!strcasecmp(token_name, "as")) {
		bobj = new BasicObject(CMD_AS, cur_line);
	} else if (!strcasecmp(token_name, "type")) {
		bobj = new BasicObject(CMD_TYPE, cur_line);
	} else if (!strcasecmp(token_name, "read")) {
		bobj = new BasicObject(CMD_READ, cur_line);
	} else if (!strcasecmp(token_name, "readb")) {
		bobj = new BasicObject(CMD_READB, cur_line);
	} else if (!strcasecmp(token_name, "readh")) {
		bobj = new BasicObject(CMD_READH, cur_line);
	} else if (!strcasecmp(token_name, "restore")) {
		bobj = new BasicObject(CMD_RESTORE, cur_line);
	} else if (!strcasecmp(token_name, "data")) {
		bobj = new BasicObject(CMD_DATA, cur_line);
	} else if (!strcasecmp(token_name, "datab")) {
		bobj = new BasicObject(CMD_DATAB, cur_line);
	} else if (!strcasecmp(token_name, "datah")) {
		bobj = new BasicObject(CMD_DATAH, cur_line);
	} else if (!strcasecmp(token_name, "map")) {
		bobj = new BasicObject(CMD_MAP, cur_line);
	} else if (!strcasecmp(token_name, "interrupt")) {
		bobj = new BasicObject(CMD_INTERRUPT, cur_line);
	} else if (!strcasecmp(token_name, "exit")) {
		bobj = new BasicObject(CMD_EXIT, cur_line);
	} else if (!strcasecmp(token_name, "inc")) {
		bobj = new BasicObject(CMD_INC, cur_line);
	} else if (!strcasecmp(token_name, "dec")) {
		bobj = new BasicObject(CMD_DEC, cur_line);
	} else if (!strcasecmp(token_name, "iwram")) {
		bobj = new BasicObject(CMD_IWRAM, cur_line);
	} else if (!strcasecmp(token_name, "goto")) {
		bobj = new BasicObject(CMD_GOTO, cur_line);
	} else {
		// non-keyword
		bobj = new BasicObject(OBJ_IDENT, cur_line);
		strcpy(
			bobj->val.symbolic,
			token_name);
		if (*text_ptr == '$') {
			// Strings are treated as arrays in TIN.
			bobj->vtype = VAR_ARRAY;
			strcat(
				bobj->val.symbolic,
				"$");
			++text_ptr;
		} else if (*text_ptr == '#') {
			bobj->vtype = VAR_FIXEDP;
			strcat(
				bobj->val.symbolic,
				"#");
			++text_ptr;
		}
		if (*text_ptr == '(') {
			bobj->otype = OBJ_SUB;
			++text_ptr;
		} else if (*text_ptr == '[') {
			bobj->otype = OBJ_ARR;
			++text_ptr;
		}
	}
	return bobj;
}

BasicObject *Parser::parseDecimal()
{
	BasicObject *bobj;
	double fval;
	signed int power;

	bobj = new BasicObject(OBJ_NUM, cur_line);
	bobj->val.numeric = 0;
	while (isdigit(*text_ptr)) {     // integer
		bobj->val.numeric *= 10;
		bobj->val.numeric = *text_ptr++ + bobj->val.numeric - '0';
	}
	if (*text_ptr == '.') {          // floating point, convert to fixed point
		power = 10;
		fval = 0.0;
		++text_ptr;
		bobj->vtype = VAR_FIXEDP;
		while (isdigit(*text_ptr)) {
			fval =
				(double)((signed int)*text_ptr++ -
					 '0') / (double)power + fval;
			power *= 10;
		}
		bobj->val.numeric <<= 8;
		bobj->val.numeric |= (int)(fval * 256.0);
	} else {
		if (*text_ptr == '#') {  // fixed point
			bobj->val.numeric <<= 8;
			bobj->vtype = VAR_FIXEDP;
			++text_ptr;
		}
	}
	return bobj;
}

BasicObject *Parser::parseDirective()
{
	int c;
	BasicObject *bobj;
	char dir_name[256];
	char *dp;

	dp = dir_name;
	++text_ptr;                       // skip hash
	while (isdigit(*text_ptr) || isalpha(*text_ptr) || *text_ptr == '_') {
		c = *text_ptr++;
		*dp++ = toupper(c);
	}
	*dp = 0;

	if (!strcasecmp(dir_name, "register")) {
		bobj = new BasicObject(DIR_REGISTER, cur_line);
	} else if (!strcasecmp(dir_name, "title")) {
		bobj = new BasicObject(DIR_TITLE, cur_line);
	} else if (!strcasecmp(dir_name, "include")) {
		bobj = new BasicObject(DIR_INCLUDE, cur_line);
	} else if (!strcasecmp(dir_name, "import")) {
		bobj = new BasicObject(DIR_IMPORT, cur_line);
	} else if (!strcasecmp(dir_name, "bitmap")) {
		bobj = new BasicObject(DIR_BITMAP, cur_line);
	} else if (!strcasecmp(dir_name, "map")) {
		bobj = new BasicObject(DIR_MAP, cur_line);
	} else if (!strcasecmp(dir_name, "palette")) {
		bobj = new BasicObject(DIR_PALETTE, cur_line);
	} else if (!strcasecmp(dir_name, "sound")) {
		bobj = new BasicObject(DIR_SOUND, cur_line);
	} else if (!strcasecmp(dir_name, "music")) {
		bobj = new BasicObject(DIR_MUSIC, cur_line);
	} else if (!strcasecmp(dir_name, "constant")) {
		bobj = new BasicObject(DIR_CONSTANT, cur_line);
	} else {
		if (strcasecmp(dir_name, "requires"))
			GLB_error(ERR_UNK_DIR, dp);
		bobj = new BasicObject(DIR_REQUIRES, cur_line);
	}

	return bobj;
}

BasicObject *Parser::parseComment()
{
	while (*text_ptr != '\n' && *text_ptr)
		++text_ptr;
	return parseNext();
}

BasicObject *Parser::parseString(const char delim)
{
	BasicObject *bobj;
	char *c;

	bobj = new BasicObject(OBJ_STR, cur_line);
	c = bobj->val.symbolic;
	++text_ptr;
	bobj->vtype = VAR_ARRAY;
	while (*text_ptr != delim && *text_ptr) {
		*c++ = *text_ptr++;
		if (*text_ptr == PATHSEP[0])
			*c++ = *text_ptr++;
	}
	*c = 0;
	++text_ptr;
	return bobj;
}

BasicObject *Parser::parseHex()
{
	int c;
	BasicObject *bobj;

	bobj = new BasicObject(OBJ_NUM, cur_line);
	bobj->val.numeric = 0;
	++text_ptr;
	while (true) {
		c = toupper(*text_ptr);
		if (!isdigit(c))
			if (toupper(*text_ptr) < 'A' || toupper(*text_ptr) >
			    'F')
				break;
		bobj->val.numeric *= 16;
		if (isdigit(*text_ptr)) {
			bobj->val.numeric = *text_ptr++ + bobj->val.numeric -
					    '0';
		} else {
			c = *text_ptr++;
			bobj->val.numeric = toupper(c) + bobj->val.numeric -
					    '7';
		}
	}
	return bobj;
}

BasicObject *Parser::parseBinary()
{
	BasicObject *bobj;

	bobj = new BasicObject(OBJ_NUM, cur_line);
	bobj->val.numeric = 0;
	++text_ptr;
	while (*text_ptr >= '0' && *text_ptr <= '1') {
		bobj->val.numeric *= 2;
		bobj->val.numeric = *text_ptr++ + bobj->val.numeric - '0';
	}
	return bobj;
}

BasicObject *Parser::parseOperator()
{
	int c;
	BasicObject *result;
	char c0;

	c = *text_ptr++;
	switch (c) {
	case '(':
		result = new BasicObject(OP_OPAREN, cur_line);
		break;
	case '~':
		result = new BasicObject(OP_BNOT, cur_line);
		break;
	case '=':
		result = new BasicObject(OP_EQ, cur_line);
		break;
	case '+':
		result = new BasicObject(OP_PLUS, cur_line);
		break;
	case '-':
		result = new BasicObject(OP_MINUS, cur_line);
		break;
	case '*':
		result = new BasicObject(OP_MULT, cur_line);
		break;
	case '/':
		result = new BasicObject(OP_DIV, cur_line);
		break;
	case '>':
		if (*text_ptr == '=') {   // greater or equal
			++text_ptr;
			result = new BasicObject(OP_GE, cur_line);
		} else {                        // greater
			result = new BasicObject(OP_GT, cur_line);
		}
		break;
	case '<':
		c0 = *text_ptr;
		if (c0 == '=') {                // less or equal
			++text_ptr;
			result = new BasicObject(OP_LE, cur_line);
		} else if (c0 == '>') {         // not equal
			++text_ptr;
			result = new BasicObject(OP_NE, cur_line);
		} else {                        // less
			result = new BasicObject(OP_LT, cur_line);
		}
		break;
	case ')':
		result = new BasicObject(ROP_CPAREN, cur_line);
		break;
	case ']':
		result = new BasicObject(ROP_CBRACK, cur_line);
		break;
	case '.':
		result = new BasicObject(ROP_PERIOD, cur_line);
		break;
	case ':':
		result = new BasicObject(ROP_COLON, cur_line);
		break;
	case ',':
		result = new BasicObject(ROP_COMMA, cur_line);
		break;
	case '!':
		result = new BasicObject(ROP_BANG, cur_line);
		break;
	case '_':                               // line continuation
		skipNewline();
		result = parseNext();
		break;
	default:
		GLB_error(ERR_UNK_TOK, *text_ptr);
	}
	return result;
}

void Parser::parseAll()
{
	BasicObject *bobj;

	bobj = parseNext();
	basic_obj_head = bobj;
	while (bobj->otype != OBJ_EOT) {
		bobj->next = parseNext();
		bobj = bobj->next;
	}
	cur_basic_obj = 0;
}

BasicObject *Parser::retrieveNextBasicObject()
{
	BasicObject *bobj_sym;
	BasicObject *bobj;

	if (cur_basic_obj)
		bobj = cur_basic_obj->next;
	else
		bobj = basic_obj_head;
	if (bobj && !bobj->otype) {
		bobj_sym = symbol_head->getBasicObj(bobj->val.symbolic);
		if (bobj_sym) {
			insertBasicObjectList(bobj_sym);
			bobj = cur_basic_obj->next;
		}
	}
	return bobj;
}

BasicObject *Parser::consumeNextBasicObj()
{
	cur_basic_obj = retrieveNextBasicObject();
	return cur_basic_obj;
}

BasicObject *Parser::getObjectWithType(enum obj_t otype, const char *human_type)
{
	BasicObject *bobj;

	bobj = consumeNextBasicObj();
	if (bobj->otype != otype)
		GLB_error(ERR_NOTOKEN, human_type);
	return bobj;
}

bool Parser::checkNextBasicObjType(enum obj_t otype)
{
	return retrieveNextBasicObject()->otype == otype;
}

bool Parser::requireRop(enum rop_t rop)
{
	BasicObject *bobj;

	bobj = retrieveNextBasicObject();
	if (bobj && bobj->otype == OBJ_ROP) {
		if (bobj->val.numeric == rop) {
			cur_basic_obj = bobj;
			return true;
		} else
			return false;
	} else
		return false;
}

bool Parser::getNextCmd(enum cmd_t expected)
{
	BasicObject *bobj;

	bobj = retrieveNextBasicObject();
	if (bobj->otype == OBJ_CMD) {
		if (bobj->val.numeric == expected) {
			cur_basic_obj = bobj;
			return true;
		} else
			return false;
	} else
		return false;
}

void Parser::insertBasicObjectList(BasicObject *bobj)
{
	BasicObject *new_bobj;
	BasicObject *new_bobj1;
	BasicObject *next_bobj;

	next_bobj = cur_basic_obj->next;
	new_bobj = new BasicObject(*bobj);
	new_bobj->line_no = next_bobj->line_no;
	while (bobj->next) {
		new_bobj1 = new BasicObject(*bobj->next);
		new_bobj1->line_no = next_bobj->line_no;
		new_bobj->append(new_bobj1);
		bobj = bobj->next;
	}
	new_bobj->append(next_bobj->next);
	cur_basic_obj->next = new_bobj;
	next_bobj->next = 0;
	if (next_bobj)
		delete next_bobj;
}

void Parser::addNewSymbol(const char *name)
{
	Symbol *sym;

	sym = new Symbol(name, symbol_head);
	symbol_head = sym;
}

void Parser::appendNextBasicObjCopyToSymbolList()
{
	BasicObject *bobj;

	bobj = consumeNextBasicObj();
	symbol_head->appendBasicObj(bobj);
}

TIN::TIN(const char *text)
{
	if (text)
		strcpy(this->text, text);
	else
		this->text[0] = 0;
	this->vunused = 0;
	this->next = 0;
}

TIN::~TIN()
{
	TIN *tin;

	if (this->vunused) {
		tin = this->vunused;
		if (tin)
			delete tin;
	}
	if (this->next) {
		tin = this->next;
		if (tin)
			delete tin;
	}
}

TIN *TIN::unused(char *text)
{
	TIN *result;

	if (vunused) {
		result = vunused->unused(text);
	} else {
		vunused = new TIN(text);
		result = this;
	}
	return result;
}

TIN *TIN::addCode(const char *code)
{
	TIN *result;

	if (next) {
		result = next->addCode(code);
	} else {
		next = new TIN(code);
		result = next;
	}
	return result;
}

void TIN::writeCodeToFile(FILE *fp)
{
	size_t len;
	size_t len0;
	bool prev_not_nl;
	TIN *line;

	line = next;
	prev_not_nl = false;
	if (vunused)
		vunused->writeCodeToFile(fp);
	len = strlen(text);
	fwrite(text, len, 1, fp);
	while (line) {
		if (strcmp(line->text, "\n") || prev_not_nl) {
			len0 = strlen(line->text);
			fwrite(line->text, len0, 1, fp);
			prev_not_nl = strcmp(line->text, "\n") != 0;
		}
		line = line->next;
	}
}

BasicObject::BasicObject(const BasicObject &src)
{
	*this = src;
	this->next = 0;
}

BasicObject::BasicObject(enum obj_t otype, int line_no)
{
	this->line_no = line_no;
	this->otype = otype;
	this->vtype = VAR_SCALAR;
	this->next = 0;
}

BasicObject::BasicObject(enum directive_t dir, int obj_idx)
{
	this->line_no = obj_idx;
	this->otype = OBJ_DIR;
	this->val.numeric = dir;
	this->next = 0;
}

BasicObject::BasicObject(enum cmd_t cmd, int obj_idx)
{
	this->line_no = obj_idx;
	this->otype = OBJ_CMD;
	this->val.numeric = cmd;
	this->next = 0;
}

BasicObject::BasicObject(enum rop_t op, int obj_idx)
{
	this->line_no = obj_idx;
	this->otype = OBJ_ROP;
	this->val.numeric = op;
	this->next = 0;
}

BasicObject::BasicObject(enum op_t op, int obj_idx)
{
	this->line_no = obj_idx;
	this->otype = OBJ_OP;
	this->val.numeric = op;
	this->next = 0;
}

BasicObject::BasicObject(int val, int line_no)
{
	this->line_no = line_no;
	this->otype = OBJ_NUM;
	this->val.numeric = val;
	this->vtype = VAR_SCALAR;
	this->next = 0;
}

BasicObject::~BasicObject()
{
	BasicObject *next;
	BasicObject *bobj;

	for (bobj = this->next; bobj; bobj = next) {
		next = bobj->next;
		bobj->next = 0;
		if (bobj)
			delete bobj;
	}
}

void BasicObject::append(BasicObject *new_bobj)
{
	if (this->next)
		this->next->append(new_bobj);
	else
		this->next = new_bobj;
}
