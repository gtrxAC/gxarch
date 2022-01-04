const ohm = require('ohm-js');
const fs = require('fs');
const macros = require('./macros');

const contents = fs.readFileSync('grammar.ohm');
const grammar = ohm.grammar(contents);
const semantics = grammar.createSemantics();

const output = [];
const vars = [new Map()];
const labelrefs = new Map();

let run = false;
let debug = false;
let lastlabel;

// _____________________________________________________________________________
//

function err(msg, pos) {
	console.error(`Error ${pos ?? getpos()}: ${msg}`);
	console.error(`Assembled: ${output.length} bytes`);
	if (debug) {
		console.log(output);
		vars.forEach((scope, i) => {
			console.log(`\nscope ${i}:`);
			console.log(scope)
		});
		console.log((new Error()).stack);
	} else {
		console.error(`Tip: use --debug to dump output and variables`);
	}
	process.exit(1);
}

/**
 * Adds spaces after a string to make it a certain length.
 * @param {string} str The string to pad
 * @param {number} len The length to pad to
 * @returns The string padded to len length.
 */
// function rpad(str, len) {
// 	console.log(len, `${str}`.length);
// 	return str + ' '.repeat(len - `${str}`.length);
// }

function getpos() {
	return lastlabel ?
		`at ${lastlabel} + ${output.length - arr2num(vars[0].get(lastlabel).val)}` :
		'before any labels';
}

function arr2num(arr) {
	return arr[0] << 8 | arr[1];
}

/**
 * Parses a number node.
 * @param {Node} digits The node containing the digits.
 * @param {number} radix The number base, 10 for decimal, 16 for hex, etc.
 * @param {boolean} double If true, a 16-bit value (address) is parsed.
 * @param {boolean} push If true, the value is also pushed to the output.
 */
function parsenum(digits, radix = 10, double = false, push) {
	let result;
	const num = parseInt(digits.sourceString, radix);
	if (num > 65535) err(`Number too large, ${num} > 65535`);
	if (!isFinite(num)) err(`Invalid number ${num}`);

	if (double) {
		result = [(num & 0xFF00) >> 8, num & 0xFF];
	} else {
		if (num > 255) err(`Number too large for one byte, ${num} > 255`);
		result = [num];
	}

	if (push) output.push(...result);
	return result;
}

function getvar(name, type, error = true, pos) {
	if (name.sourceString) name = name.sourceString;
	let value;
	for (let i = vars.length - 1; i >= 0; i--) {
		if (!vars[i].has(name)) continue;
		value = vars[i].get(name);
		break;
	}

	if (value) {
		if (value.type !== type)
			err(`Variable ${name} is of type '${value.type}', expected '${type}'`, pos);

		if (type === 'register' && value.val[0] > 31)
			err(`Invalid register, ${value.val[0]} > 31`, pos)

		return value.val;
	} else if (error) {
		err(`Variable ${name} not found. Variables must be defined before using them. (looking for type '${type}')`, pos);
	} else {
		return undefined;
	}
}

function setvar(name, val, type) {
	if (name.sourceString) name = name.sourceString;
	vars[vars.length - 1].set(name, {val, type});
}

function setgvar(name, val, type) {
	if (name.sourceString) name = name.sourceString;
	vars[0].set(name, {val, type});
}

// _____________________________________________________________________________
//

semantics.addOperation('parse', {
	value_hex(_, digits) { return parsenum(digits, 16) },
	value_bin(_, digits) { return parsenum(digits, 2) },
	value_dec(digits) { return parsenum(digits) },

	address_hex(_, digits) { return parsenum(digits, 16, true) },
	address_bin(_, digits) { return parsenum(digits, 2, true) },
	address_dec(digits) { return parsenum(digits, 10, true) },
	address_label(ident) { return getvar(ident, 'address') },

	register_reg(_, reg) {
		switch (reg.sourceString.toLowerCase()) {
			case "h": // result high byte
				return [30];

			case "r": // division remainder
				return [31];

			default: {
				const result = parseInt(reg.sourceString);
				if (result > 31)
					err(`Invalid register, ${reg.sourceString} > 31`);

				return [result];
			}
		}
	},

	register_ident(ident) { return getvar(ident, 'register') },

	stringpart_char(c) { return c.sourceString; },
	stringpart_lf(_) { return "\n"; },
	stringpart_cr(_) { return "\r"; },
	stringpart_tab(_) { return "\t"; },
	stringpart_nul(_) { return "\0"; },
	stringpart_bs(_) { return "\\"; },
	stringpart_quot(_) { return '"'; }
})

semantics.addOperation('eval', {
	value_hex(_, digits) { parsenum(digits, 16, false, true) },
	value_bin(_, digits) { parsenum(digits, 2, false, true) },
	value_dec(digits) { parsenum(digits, 10, false, true) },
	value_lo(_, __, addr, ___) { output.push(addr.parse()[1]) },
	value_hi(_, __, addr, ___) { output.push(addr.parse()[0]) },
	value_label(ident) { output.push(...getvar(ident, 'value')) },

	address_hex(_, digits) { parsenum(digits, 16, true, true) },
	address_bin(_, digits) { parsenum(digits, 2, true, true) },
	address_dec(digits) { parsenum(digits, 10, true, true) },

	address_label(ident) {
		let addr = getvar(ident, 'address', false);

		if (addr === undefined) {
			addr = [0xFF, 0xFF];
			labelrefs.set(output.length, {ident: ident.sourceString, pos: getpos()});
		}
		output.push(...addr);
	},

	data_label(ident) {
		let val = getvar(ident, 'value', false);

		if (val === undefined) {
			val = getvar(ident, 'address', false);

			if (val === undefined) {
				val = [0xFF, 0xFF];
				labelrefs.set(output.length, {ident: ident.sourceString, pos: getpos()});
			}
		}
		output.push(...val);
	},

	register_reg(_, reg) {
		switch (reg.sourceString.toLowerCase()) {
			case "h": // result high byte
				output.push(30);
				break;

			case "r": // division remainder
				output.push(31);
				break;

			default: {
				const result = parseInt(reg.sourceString);
				if (result > 31)
					err(`Invalid register, ${reg.sourceString} > 31`);

				output.push(result);
			}
		}
	},

	register_ident(ident) { output.push(...getvar(ident, 'register')) },

	Instruction_block(_, insts, __) {
		vars.push(new Map());  // create a scope for this block
		insts.eval();
		vars.pop();
	},

	Instruction_label(ident, _) {  // currently labels are global to make stuff simpler
		if (output.length < 256) {
			setgvar(ident, [0, output.length], 'address');
		} else {
			setgvar(ident, [(output.length & 0xFF00) >> 8, output.length & 0xFF], 'address');
		}
		lastlabel = ident.sourceString;
	},
	Instruction_dat(_, data) { data.asIteration().eval() },
	Instruction_val(_, ident, val) { setvar(ident, val.parse(), 'value') },
	Instruction_addr(_, ident, addr) { setvar(ident, addr.parse(), 'address') },
	Instruction_reg(_, ident, reg) { setvar(ident, reg.parse(), 'register') },

	Instruction_nop(_) { output.push(0); },
	Instruction_set(_, reg, val) {
		output.push(1); reg.eval(); val.eval();
	},
	Instruction_ld(_, reg, addr) {
		output.push(2); reg.eval(); addr.eval();
	},
	Instruction_ldi(_, reg, addr) {
		output.push(3); reg.eval(); addr.eval();
	},
	Instruction_st(_, reg, addr) {
		output.push(4); reg.eval(); addr.eval();
	},
	Instruction_sti(_, reg, addr) {
		output.push(5); reg.eval(); addr.eval();
	},
	Instruction_add(_, reg1, reg2, reg3) {
		output.push(6); reg1.eval(); reg2.eval(); reg3.eval();
	},
	Instruction_sub(_, reg1, reg2, reg3) {
		output.push(7); reg1.eval(); reg2.eval(); reg3.eval();
	},
	Instruction_mul(_, reg1, reg2, reg3) {
		output.push(8); reg1.eval(); reg2.eval(); reg3.eval();
	},
	Instruction_div(_, reg1, reg2, reg3) {
		output.push(9); reg1.eval(); reg2.eval(); reg3.eval();
	},
	Instruction_and(_, reg1, reg2, reg3) {
		output.push(10); reg1.eval(); reg2.eval(); reg3.eval();
	},
	Instruction_or(_, reg1, reg2, reg3) {
		output.push(11); reg1.eval(); reg2.eval(); reg3.eval();
	},
	Instruction_xor(_, reg1, reg2, reg3) {
		output.push(12); reg1.eval(); reg2.eval(); reg3.eval();
	},
	Instruction_eq(_, reg1, reg2, reg3) {
		output.push(13); reg1.eval(); reg2.eval(); reg3.eval();
	},
	Instruction_lt(_, reg1, reg2, reg3) {
		output.push(14); reg1.eval(); reg2.eval(); reg3.eval();
	},
	Instruction_gt(_, reg1, reg2, reg3) {
		output.push(15); reg1.eval(); reg2.eval(); reg3.eval();
	},
	Instruction_jmp(_, addr) { output.push(16); addr.eval(); },
	Instruction_cj(_, reg, addr) {
		output.push(17); reg.eval(); addr.eval();
	},
	Instruction_js(_, addr) { output.push(18); addr.eval(); },
	Instruction_cjs(_, reg, addr) {
		output.push(19); reg.eval(); addr.eval();
	},
	Instruction_ret(_) { output.push(20); },
	Instruction_dw(_, reg1, reg2, reg3, reg4) {
		output.push(21); reg1.eval(); reg2.eval(); reg3.eval(); reg4.eval();
	},
	Instruction_at(_, reg1, reg2) {
		output.push(22); reg1.eval(); reg2.eval();
	},
	Instruction_key(_, reg1, reg2) {
		output.push(23); reg1.eval(); reg2.eval();
	},
	// snd
	Instruction_end(_) { output.push(25); },

	string(_, parts, __) {
		parts.parse().forEach(c => output.push(c.charCodeAt(0)));
		output.push(0);
	}
})

// _____________________________________________________________________________
//
//  Read command line arguments
// _____________________________________________________________________________
//

function help() {
	console.log("gxasm: gxarch assembler\n");
	console.log("Usage: node gxasm.js [options] [file]");
	console.log("-h, --help  Show this message");
	console.log("-r, --run   Run the output, gxvm must be in the same directory");
	console.log("-d, --debug Enable debugging if used with --run")
	process.exit(0);
}

if (process.argv.length < 3) help();
let file;

for (let arg of process.argv.slice(2)) {
	switch (arg) {
		case '-h': case '--help': help();
		case '-r': case '--run': run = true; break;
		case '-d': case '--debug': debug = true; break;
		case '-rd': case '-dr': run = true; debug = true; break;

		default:
			if (file)
				err("Only one file can be specified (use .include to include other files)");
			file = arg;
			break;
	}
}

// _____________________________________________________________________________
//
//  Expand macros (preprocessor)
// _____________________________________________________________________________
//

let src = fs.readFileSync(file)
	.toString()
	.split(/\r\n|\r|\n/);

// Check each line for macros and run them.
// All macros are stored in macros.js.
let ln = 0;
while (ln < src.length) {
	let line = src[ln].trim().split(/\s+/);
	if (line[0][0] === '.') {
		src.splice(ln, 1, macros(line.shift().slice(1), line));
		src = src.flat();
	} else ln++;
}

// _____________________________________________________________________________
//
//  Parse and assemble
// _____________________________________________________________________________
//

src = src.join('\n');
const match = grammar.match(src);

if (match.succeeded()) {
	semantics(match).eval();

	const outfile = new Uint8Array(output);
	if (outfile.length > 0xFF00)
		err(`Output file too large, 0x${outfile.length.toString(16)} > 0xFF00`);

	labelrefs.forEach((label, addr) => {
		const val = getvar(label.ident, 'address', true, label.pos);
		if (val.length === 1) val.unshift(0x00);
		outfile.set(val, addr);
	})

	// Use same filename as input file, but with gxa extension
	const outname = file.replace(/\.\w*$/, ".gxa");
	fs.writeFileSync(outname, outfile);

	if (debug) console.log(vars[0]);

	// Debug symbols need some work to get working again
	// if (debug) {
	// 	let symbols = [...vars[0].entries()]  // [['key1', 1], ['key2', 2], ...]
	// 		.sort((a, b) => a[1] - b[1])   // sort in increasing value order
	// 		.map(sym => `0x${rpad(sym[1].toString(16), 4)} (${rpad(sym[1], 5)}) ${sym[0]}`); // "0xff   (255  ) label"

	// 	fs.writeFileSync(file.replace(/\.\w*$/, ".sym.txt"), symbols.join('\n'));
	// }

	if (run) {
		const cp = require('child_process');
		cp.spawn(
			process.platform === 'win32' ? 'gxvm.exe' : './gxvm',
			debug ? ['--debug', outname] : [outname],
			{stdio: 'inherit'}
		);
	}
} else {
	console.error(match.message);
}