const ohm = require('ohm-js');
const fs = require('fs');
const contents = fs.readFileSync('grammar.ohm');
const grammar = ohm.grammar(contents);

const output = [];
const labels = new Map();
const labelrefs = new Map();

/**
 * Adds a value to the output.
 * @param {string | number | object} val The value to push.
 * @param {boolean} force16 If true, the number is 16-bit even if it's under 256.
 */
function push(val, force16) {
	switch (typeof val) {
		case 'string':
			for (let char of val.split('')) {
				push(char.charCodeAt(0));
			}
			push(0);
			break;

		case 'number':
			if (val > 65535) throw new Error("Number too big");
		
			if (val > 255) {
				push((val & 0xFF00) >> 8);
				push(val & 0xFF);
			} else {
				if (force16) output.push(0);
				output.push(val);
			}
			break;

		case 'object':
			if ('num' in val && 'double' in val) {
				push(val.num, val.double);
				break;
			}
			// fall through

		default:
			console.error(val);
			throw new Error(`Invalid data of type ${typeof val}, logged above`);
	}
}

/**
 * Parses a number node.
 * @param {Node} digits The node containing the digits.
 * @param {number} radix The number base, 10 for decimal, 16 for hex, etc.
 */
function parsenum(digits, radix) {
	const double = digits.sourceString.length > (255).toString(radix).length;
	const num = parseInt(digits.sourceString, radix);
	if (num > 65535) throw new Error("Number too big");
	return {num, double};
}

/**
 * Instruction name to opcode mappings.
 */
const instructions = [
	'nop', 'set', 'mov',
	'add', 'sub', 'mul', 'div',
	'and', 'or',  'xor', 'not',
	'equ', 'lt',  'gt',
	'jmp', 'cj',  'js',  'cjs', 'ret',
	'key', 'end',
]

const semantics = grammar.createSemantics().addOperation('eval', {
	Command_inst(inst, args) {
		let instaddr = output.length;
		push(inst.eval());
		args = args.asIteration();

		let args2 = [];
		if (args2[0] = args.child(0)?.eval()) {
			if (args2[0].ptr) output[instaddr] |= 0b10000000;
			push(args2[0].value);
		}

		if (args2[1] = args.child(1)?.eval()) {
			if (args2[1].ptr) output[instaddr] |= 0b01000000;
			push(args2[1].value);
		}
	},

	Command_data(_, val) {
		val.asIteration().eval().forEach(push);
	},

	Command_value(_, ident, __, value) {
		labels.set(ident.eval(), value.eval());
	},

	Command_label(ident, _) {
		labels.set(ident.eval(), {num: output.length, double: true});
	},

	Argument(ptr, value) {
		return {ptr: !!ptr.sourceString.length, value: value.eval()};
	},

	value_hex(_, digits) { return parsenum(digits, 16) },
	value_bin(_, digits) { return parsenum(digits, 2) },
	value_dec(digits) { return parsenum(digits, 10) },

	value_label(ident) {
		ident = ident.eval();
		if (labels.has(ident)) {
			return labels.get(ident);
		} else {
			labelrefs.set(output.length, ident);
			return 0xFFFF;
		}
		// throw new Error(`Label ${ident} not found`);
	},

	string(_, parts, __) { return parts.eval().join('') },

	stringpart(c) { return c.sourceString },
	stringpart_lf(_) { return "\n" },
	stringpart_cr(_) { return "\r" },
	stringpart_tab(_) { return "\t" },
	stringpart_nul(_) { return "\0" },
	stringpart_bs(_) { return "\\" },

	ident(start, parts) {
		return start.sourceString + parts.sourceString;
	},

	inst(name) {
		return instructions.indexOf(name.sourceString);
	}
})

for (let arg of process.argv.slice(2)) {
	switch (arg) {
		case '-v':
		case '--version':
			console.log("1.0.1");
			break;
		
		case '-h':
		case '--help':
			console.log("gxarch assembler\n\n-h, --help: show this screen");
			break;

		default: {
			const src = fs.readFileSync(arg);
			const match = grammar.match(src);

			if (match.succeeded()) {
				semantics(match).eval();
				
				const outfile = new Uint8Array(output);
				if (outfile.length > 0xE000)
					throw new Error(`Output file too large, ${outfile.length} > 56k`);

				labelrefs.forEach((label, addr) => {
					console.log(`${addr}: ${label}`);
					if (!labels.has(label))
						throw new Error(`Label ${label} not found`);

					const val = labels.get(label).num;
					outfile.set([(val & 0xFF00) >> 8, val & 0xFF], addr);
				})

				fs.writeFileSync('output.gxa', outfile);
			} else {
				console.error(match.message);
			}
			break;
		}
	}
}