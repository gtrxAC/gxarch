#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>
#include <stdint.h>

#include "mpc.h"
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"
#include "util.h"

#define u8 uint8_t
#define u16 uint16_t

typedef enum VarType {
	VAR_VALUE,    // 0-255
	VAR_ADDRESS,  // 0-65535
	VAR_REGISTER  // 0-31
} VarType;

char *typenames[] = {"value", "address", "register"};

typedef struct Variable {
	VarType type;
	u16 value;
} Variable;

typedef enum Opcode {
	OP_NOP, OP_SET, OP_LD, OP_LDI, OP_ST, OP_STI,
	OP_ADD, OP_SUB, OP_MUL, OP_DIV,
	OP_AND, OP_OR, OP_XOR,
	OP_EQ, OP_LT, OP_GT,
	OP_JMP, OP_CJ, OP_JS, OP_CJS, OP_RET,
	OP_DW, OP_AT, OP_KEY, OP_SND, OP_END,
	OP_COUNT
} Opcode;

typedef struct ForwardRef {
	mpc_ast_t *ast;
	char file[512];
} ForwardRef;

char *opnames[] = {
	"nop", "set", "ld", "ldi", "st", "sti",
	"add", "sub", "mul", "div",
	"and", "or", "xor",
	"eq", "lt", "gt",
	"jmp", "cj", "js", "cjs", "ret",
	"dw", "at", "key", "snd", "end"
};

mpc_parser_t *program;

u8 *output = NULL;
struct {char *key; Variable value;} *vars = NULL;
struct {u16 key; ForwardRef value;} *forwardrefs = NULL;
char **filenames = NULL;
mpc_ast_t **files = NULL;

u16 lastins = 0;

// _____________________________________________________________________________
//
//  Utilities
// _____________________________________________________________________________
//
void err(mpc_ast_t *t, const char *fmt, ...) {
	char buf[256];
	va_list args;
	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);

	if (arrlen(filenames)) fprintf(stderr, "%s:", arrlast(filenames));
	if (t) fprintf(stderr, "%ld:%ld:", t->state.row + 1, t->state.col + 1);

	if (arrlen(filenames) || t) fprintf(stderr, " ");
	fprintf(stderr, "error: %s\n", buf);

	for (int i = 0; i < shlen(vars); i++) {
		printf("'%s' %s = %d\n", vars[i].key, typenames[vars[i].value.type], vars[i].value.value);
	}

	exit(EXIT_FAILURE);
}

void push(mpc_ast_t *t, u8 val) {
	if (arrlen(output) > 0xF000) err(t, "File size exceeded 0xF000 (61440) bytes");
	arrput(output, val);
}

void push16(mpc_ast_t *t, u16 addr) {
	push(t, (addr & 0xFF00) >> 8);
	push(t, addr & 0xFF);
}

void assemble(char *filename, bool ismain);

// _____________________________________________________________________________
//
//  Evaluation - Numbers
// _____________________________________________________________________________
//
u16 eval_number(mpc_ast_t *t, VarType type) {
	if (strstr(t->tag, "val_lohi")) {
		u16 addr = eval_number(t->children[2], VAR_ADDRESS);
		if (!strcmp(t->children[0]->contents, "lo")) {
			return addr & 0xFF;
		} else {
			return (addr & 0xFF00) >> 8;
		}
	}
	else if (strstr(t->tag, "number")) {
		unsigned long res;

		if (t->children_num == 2) {
			if (!strcmp(t->children[0]->contents, "0x")) {
				res = strtoul(t->children[1]->contents, NULL, 16);
			} else {
				res = strtoul(t->children[1]->contents, NULL, 2);
			}
		} else {
			res = strtoul(t->contents, NULL, 10);
		}

		if (errno == ERANGE) err(t, "Value out of range");

		switch (type) {
			case VAR_VALUE:
				if (res > 255) err(t, "Value out of range, %d > 255", res);
				break;

			case VAR_ADDRESS:
				if (res > 65535) err(t, "Value out of range for address, %d > 65535", res);
				break;

			case VAR_REGISTER:
				if (res > 31) err(t, "Invalid register, %d > 31", res);
				break;

			default:
				err(t, "Invalid number type %d", type);
				break;
		}

		return (u16) res;
	} else {
		if (shgeti(vars, t->contents) == -1) {
			if (type == VAR_ADDRESS) {
				hmput(forwardrefs, arrlen(output), (ForwardRef) {t});
				strcpy(hmget(forwardrefs, arrlen(output)).file, arrlast(filenames));
				return 0xFFFF;
			} else {
				err(t, "Variable '%s' not found", t->contents);
			}
		}

		Variable var = shget(vars, t->contents);
		if (var.type != type) err(
			t, "Variable '%s' is of type '%s', expected '%s'",
			t->contents, typenames[var.type], typenames[type]
		);

		return var.value;
	}
}

// _____________________________________________________________________________
//
//  Evaluation - Registers
// _____________________________________________________________________________
//
u8 eval_reg(mpc_ast_t *t) {
	if (strstr(t->tag, "ident")) {
		if (shgeti(vars, t->contents) == -1)
			err(t, "Variable '%s' not found", t->contents);

		Variable var = shget(vars, t->contents);
		if (var.type != VAR_REGISTER) err(
			t, "Variable '%s' is of type '%s', expected 'register'",
			t->contents, typenames[var.type]
		);

		return (u8) var.value;
	}
	else if (strstr(t->tag, "reg_val")) {
		u8 res = eval_reg(t->children[1]);
		arrins(output, lastins, OP_SET);
		arrins(output, lastins + 1, res);
		arrins(output, lastins + 2, eval_number(t->children[3], VAR_VALUE));
		return res;
	}
	else if (strstr(t->tag, "reg_addr")) {
		u8 res = eval_reg(t->children[1]);
		u16 addr = eval_number(t->children[3], VAR_ADDRESS);
		arrins(output, lastins, OP_LD);
		arrins(output, lastins + 1, res);
		arrins(output, lastins + 2, (addr & 0xFF00) >> 8);
		arrins(output, lastins + 3, addr & 0xFF);
		return res;
	}
	else {
		switch (t->children[1]->contents[0]) {
			case 'h': case 'H': return 30; break;
			case 'r': case 'R': return 31; break;
			default: return eval_number(t->children[1], VAR_REGISTER); break;
		}
	}
}

// _____________________________________________________________________________
//
//  Evaluation
// _____________________________________________________________________________
//
void eval(mpc_ast_t *t) {
	#define TAG(a) else if (strstr(t->tag, a))

	if (!strcmp(t->tag, ">")) {
		for (int i = 0; i < t->children_num; i++) {
			eval(t->children[i]);
		}
	}

	TAG("label") shput(
		vars, t->children[0]->contents,
		((Variable) {VAR_ADDRESS, arrlen(output)})
	);

	TAG("block") {
		for (int i = 1; i < t->children_num - 1; i++) {
			eval(t->children[i]);
		}
	}

	TAG("macro") {
		#define MACRO(m) if (!strcmp(t->children[0]->contents, m))

		MACRO("include") {
			char *filename = NULL;

			// Remove quotes from string
			for (int i = 1; i < strlen(t->children[1]->contents) - 1; i++) {
				arrput(filename, t->children[1]->contents[i]);
			}
			arrput(filename, 0);
			assemble(filename, false);
		}

		#define CJ_MACRO(cmp, op) { \
			push(t, cmp); \
			push(t, eval_reg(t->children[1])); \
			push(t, eval_reg(t->children[2])); \
			push(t, 29); \
			push(t, op); \
			push(t, 29); \
			push16(t, eval_number(t->children[3], VAR_ADDRESS)); \
			lastins = arrlen(output); \
		}

		MACRO("ltj") CJ_MACRO(OP_LT, OP_CJ);
		MACRO("gtj") CJ_MACRO(OP_GT, OP_CJ);
		MACRO("eqj") CJ_MACRO(OP_EQ, OP_CJ);
		MACRO("ltjs") CJ_MACRO(OP_LT, OP_CJS);
		MACRO("gtjs") CJ_MACRO(OP_GT, OP_CJS);
		MACRO("eqjs") CJ_MACRO(OP_EQ, OP_CJS);

		#define KEYJ_MACRO(op) { \
			push(t, OP_SET); \
			push(t, 29); \
			push(t, eval_number(t->children[1], VAR_VALUE)); \
			push(t, OP_KEY); \
			push(t, 29); \
			push(t, 29); \
			push(t, op); \
			push(t, 29); \
			push16(t, eval_number(t->children[2], VAR_ADDRESS)); \
			lastins = arrlen(output); \
		}

		MACRO("keyj") KEYJ_MACRO(OP_CJ)
		MACRO("keyjs") KEYJ_MACRO(OP_CJS)

		MACRO("mov") {
			push(t, OP_SET);
			push(t, 29);
			push(t, 0);
			push(t, OP_ADD);
			push(t, eval_reg(t->children[1]));
			push(t, 29);
			push(t, eval_reg(t->children[2]));
			lastins = arrlen(output);
		}
	}

	// Instruction which doesn't take any args
	TAG("instruction|regex") {
		for (int i = 0; i < OP_COUNT; i++) {
			if (!strcmp(opnames[i], t->contents)) {
				push(t, i); break;
			}
		}
		lastins = arrlen(output);
	}

	TAG("instruction") {
		for (int i = 0; i < OP_COUNT; i++) {
			if (!strcmp(opnames[i], t->children[0]->contents)) {
				push(t->children[0], i); break;
			}
		}

		for (int i = 1; i < t->children_num; i++) {
			mpc_ast_t *c = t->children[i];
			if (strstr(c->tag, "register")) {
				push(c, eval_reg(c));
			}
			else if (strstr(c->tag, "address")) {
				push16(c, eval_number(c, VAR_ADDRESS));
			}
			else if (strstr(c->tag, "value")) {
				push(c, eval_number(c, VAR_VALUE));
			}
		}
		lastins = arrlen(output);
	}

	TAG("ins_datl") {
		for (int i = 1; i < t->children_num; i += 2) {
			push16(t->children[i], eval_number(t->children[i], VAR_ADDRESS));
		}
		lastins = arrlen(output);
	}

	TAG("ins_dat") {
		for (int i = 1; i < t->children_num; i += 2) {
			mpc_ast_t *c = t->children[i];

			if (strstr(c->tag, "string")) {
				bool escape = false;
				for (int i = 1; i < strlen(c->contents) - 1; i++) {
					if (escape) {
						switch (c->contents[i]) {
							case '"': push(c, '"'); break;
							case 'r': push(c, '\r'); break;
							case 'n': push(c, '\n'); break;
							case 't': push(c, '\t'); break;
							case '0': push(c, 0); break;
							case '\\': push(c, '\\'); break;
							default: err(c, "Invalid string escape '\\%c'", c->contents[i]);
						}
						escape = false;
					} else {
						if (c->contents[i] == '\\') escape = true;
						else push(c, c->contents[i]);
					}
				}
				push(c, 0);
			} else {
				push(c, eval_number(c, VAR_VALUE));
			}
		}
		lastins = arrlen(output);
	}

	TAG("ins_val") {
		shput(
			vars, t->children[1]->contents,
			((Variable) {VAR_VALUE, eval_number(t->children[2], VAR_VALUE)})
		);
		lastins = arrlen(output);
	}

	TAG("ins_addr") {
		shput(
			vars, t->children[1]->contents,
			((Variable) {VAR_ADDRESS, eval_number(t->children[2], VAR_ADDRESS)})
		);
		lastins = arrlen(output);
	}

	TAG("ins_reg") {
		shput(
			vars, t->children[1]->contents,
			((Variable) {VAR_REGISTER, eval_reg(t->children[2])})
		);
		lastins = arrlen(output);
	}
}

// _____________________________________________________________________________
//
//  Assemble
// _____________________________________________________________________________
//
void assemble(char *filename, bool ismain) {
	arrput(filenames, filename);

	mpc_result_t res;
	if (!mpc_parse_contents(filename, program, &res)) {
		mpc_err_print(res.error);
		mpc_err_delete(res.error);
		exit(EXIT_FAILURE);
	}

	arrput(files, res.output);
	eval(res.output);

	if (ismain) {
		// Check forward references
		for (int i = 0; i < hmlen(forwardrefs); i++) {
			char *ref = forwardrefs[i].value.ast->contents;

			if (shgeti(vars, ref) == -1) {
				arrput(filenames, forwardrefs[i].value.file);
				err(forwardrefs[i].value.ast, "Variable '%s' not found", ref);
			}

			Variable var = shget(vars, ref);

			if (var.type != VAR_ADDRESS) {
				arrput(filenames, forwardrefs[i].value.file);
				err(
					forwardrefs[i].value.ast, "Variable '%s' is of type '%s', expected 'address'",
					ref, typenames[var.type]
				);
			}

			output[forwardrefs[i].key] = (var.value & 0xFF00) >> 8;
			output[forwardrefs[i].key + 1] = var.value & 0xFF;
		}
	}

	arrpop(filenames);
}

// _____________________________________________________________________________
//
//  Main
// _____________________________________________________________________________
//
void cleanup(void) {
	for (int i = 0; i < arrlen(files); i++) mpc_ast_delete(files[i]);
	arrfree(output);
	shfree(vars);
	hmfree(forwardrefs);
	arrfree(filenames);
	arrfree(files);
	mpc_cleanup(1, program);
}

void help(int exitcode) {
	puts("gxasm: gxarch assembler\n");
	puts("Usage: gxasm [options] [file]");
	puts("-h, --help   Show this message");
	puts("-r, --run    Run the output, gxvm must be in the same directory");
	puts("-d, --debug  Enable debugging if used with --run");
	exit(exitcode);
}

int main(int argc, char **argv) {
	char *mainfile = NULL;
	bool run = false;
	bool debug = false;

	if (argc == 1) help(1);

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--run") || !strcmp(argv[i], "-r")) {
			run = true;
		}
		else if (!strcmp(argv[i], "--debug") || !strcmp(argv[i], "-d")) {
			debug = true;
		}
		else if (!strcmp(argv[i], "-rd") || !strcmp(argv[i], "-dr")) {
			run = true; debug = true;
		}
		else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
			help(0);
		}
		else {
			if (mainfile) {
				err(NULL, "Only one source file allowed, use include macro to use multiple files");
			} else {
				mainfile = argv[i];
			}
		}
	}

	if (!mainfile) err(NULL, "No input file specified");

// _____________________________________________________________________________
//
//  Parser Variables
// _____________________________________________________________________________
//
	#define DEFTAG(tag) mpc_parser_t *tag = mpc_new(#tag);

	// program is global so assemble() can use it
	program = mpc_new("program");

	DEFTAG(stmt);
	DEFTAG(comment);
	DEFTAG(label);
	DEFTAG(block);
	DEFTAG(instruction);
	DEFTAG(macro);
	DEFTAG(ins_dat);
	DEFTAG(ins_datl);
	DEFTAG(ins_val);
	DEFTAG(ins_addr);
	DEFTAG(ins_reg);
	DEFTAG(data);
	DEFTAG(string);
	DEFTAG(stringpart);
	DEFTAG(number);
	DEFTAG(ident);

	// register is a c keyword...
	mpc_parser_t *reg = mpc_new("register");

	DEFTAG(reg_val);
	DEFTAG(reg_addr);
	DEFTAG(value);
	DEFTAG(val_lohi);
	DEFTAG(address);

// _____________________________________________________________________________
//
//  Create Parser and Assemble
// _____________________________________________________________________________
//
	mpc_err_t *e = mpca_lang(
		MPCA_LANG_DEFAULT,
		" \
			program: /^/ <stmt>* /$/; \n \
			\n \
			stmt: <comment>  | <block>   | <macro>    | <instruction> | <ins_dat> \n \
			    | <ins_datl> | <ins_val> | <ins_addr> | <ins_reg>     | <label>; \n \
			\n \
			comment: /;[^\\r\\n]*/; \n \
			label: <ident> ':'; \n \
			block: '{' <stmt>* '}'; \n \
			instruction: \n \
			  /nop\\b/ \n \
			| /set\\b/ <register> <value> \n \
			| /ld\\b/ <register> <address> \n \
			| /ldi\\b/ <register> <register> \n \
			| /st\\b/ <register> <address> \n \
			| /sti\\b/ <register> <register> \n \
			| /add\\b/ <register> <register> <register> \n \
			| /sub\\b/ <register> <register> <register> \n \
			| /mul\\b/ <register> <register> <register> \n \
			| /div\\b/ <register> <register> <register> \n \
			| /and\\b/ <register> <register> <register> \n \
			| /or\\b/ <register> <register> <register> \n \
			| /xor\\b/ <register> <register> <register> \n \
			| /eq\\b/ <register> <register> <register> \n \
			| /lt\\b/ <register> <register> <register> \n \
			| /gt\\b/ <register> <register> <register> \n \
			| /jmp\\b/ <address> \n \
			| /cj\\b/ <register> <address> \n \
			| /js\\b/ <address> \n \
			| /cjs\\b/ <register> <address> \n \
			| /ret\\b/ \n \
			| /dw\\b/ <register> <register> <register> <register> \n \
			| /at\\b/ <register> <register> \n \
			| /key\\b/ <register> <register> \n \
			| /snd\\b/ <register> <register> <register> <register> \n \
			| /end\\b/; \n \
			\n \
			macro: \n \
			  /include\\b/ <string> \n \
			| /ltj\\b/ <register> <register> <address> \n \
			| /gtj\\b/ <register> <register> <address> \n \
			| /eqj\\b/ <register> <register> <address> \n \
			| /ltjs\\b/ <register> <register> <address> \n \
			| /gtjs\\b/ <register> <register> <address> \n \
			| /eqjs\\b/ <register> <register> <address> \n \
			| /keyj\\b/ <value> <address> \n \
			| /keyjs\\b/ <value> <address> \n \
			| /mov\\b/ <register> <register>; \n \
			\n \
			ins_dat: /dat\\b/ <data> (',' <data>)*; \n \
			ins_datl: /datl\\b/ <address> (',' <address>)*; \n \
			ins_val: /val\\b/ <ident> <value>; \n \
			ins_addr: /addr\\b/ <ident> <address>; \n \
			ins_reg: /reg\\b/ <ident> <register>; \n \
			\n \
			data: <value> | <string>; \n \
			string: /\"(\\\\.|[^\"])*\"/; \n \
			\n \
			number: (\"0x\" /[0-9a-fA-F]+/ | \"0b\" /[01]+/ | /[0-9]+/ ); \n \
			ident: /\\b[a-zA-Z_][a-zA-Z0-9_]*\\b/ ; \n \
			\n \
			register: '%' (<number> | /[hrHR]/) | <ident> | <reg_val> | <reg_addr>; \n \
			reg_val: '(' <register> ':' <value> ')'; \n \
			reg_addr: '[' <register> ':' <address> ']'; \n \
			\n \
			value: <number> | <val_lohi> | <ident>; \n \
			val_lohi: /lo|hi/ '(' <address> ')'; \n \
			address: <number> | <ident>; \n \
			\
		",
		program, stmt, comment, label, block, instruction, macro,
		ins_dat, ins_datl, ins_val, ins_addr, ins_reg,
		data, string, number, ident, reg, reg_val, reg_addr,
		value, val_lohi, address
	);

	if (e != NULL) {
		mpc_err_print(e);
		mpc_err_delete(e);
		exit(1);
	}

	assemble(mainfile, true);

// _____________________________________________________________________________
//
//  Exit
// _____________________________________________________________________________
//
	mpc_cleanup(
		20, stmt, comment, label, block, instruction,
		ins_dat, ins_datl, ins_val, ins_addr, ins_reg,
		data, string, number, ident, reg, reg_val, reg_addr,
		value, val_lohi, address
	);

	char *outname = TextReplace(mainfile, ".gxs", ".gxa");
	SaveFileData(outname, output, arrlen(output));

	if (run) {
		#ifdef WIN32
			#define RUNCMD "gxvm.exe \"%s\"%s"
		#else
			#define RUNCMD "\"./gxvm\" \"%s\"%s"
		#endif

		char command[512];
		sprintf(command, RUNCMD, outname, debug ? " -d" : "");
		int unused = system(command);
	}

	free(outname);
}