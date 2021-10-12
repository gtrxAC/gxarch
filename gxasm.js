const ohm = require('ohm-js');
const fs = require('fs');
const contents = fs.readFileSync('grammar.ohm');
const grammar = ohm.grammar(contents);
const semantics = grammar.createSemantics();

const output = [];
const labels = new Map();
const labelrefs = new Map();

/**
 * Parses and pushes a number node to the output.
 * @param {Node} digits The node containing the digits.
 * @param {number} radix The number base, 10 for decimal, 16 for hex, etc.
 * @param {boolean} double If true, a 16-bit value (address) is pushed.
 */
function pushnum(digits, radix = 10, double = false) {
	const num = parseInt(digits.sourceString, radix);
	if (num > 65535) throw new Error(`Number too large, ${num} > 65535`);

	if (double) {
		output.push((num & 0xFF00) >> 8);
		output.push(num & 0xFF);
	} else {
		if (num > 255) throw new Error(`Number too large for one byte, ${num} > 255`);
		output.push(num);
	}
}

semantics.addOperation('parse', {
	value_hex(_, digits) { return parseInt(digits.sourceString, 16) },
	value_bin(_, digits) { return parseInt(digits.sourceString, 2) },
	value_dec(digits) { return parseInt(digits.sourceString, 10) },

	address_hex(_, digits) { return parseInt(digits.sourceString, 16) },
	address_bin(_, digits) { return parseInt(digits.sourceString, 2) },
	address_dec(digits) { return parseInt(digits.sourceString, 10) },
})

semantics.addOperation('eval', {
	value_hex(_, digits) { pushnum(digits, 16) },
	value_bin(_, digits) { pushnum(digits, 2) },
	value_dec(digits) { pushnum(digits, 10) },

	address_hex(_, digits) { pushnum(digits, 16, true) },
	address_bin(_, digits) { pushnum(digits, 2, true) },
	address_dec(digits) { pushnum(digits, 10, true) },

	address_label(ident) {
		let addr = 0xFFFF;
		ident = ident.sourceString;
		if (labels.has(ident)) {
			addr = labels.get(ident);
		} else {
			labelrefs.set(output.length, ident);
		}
		output.push((addr & 0xFF00) >> 8);
		output.push(addr & 0xFF);
	},

	register(_, num) { pushnum(num); },

	Instruction_label(ident, _) {
		ident = ident.sourceString;
		labels.set(ident, output.length);
	},
	Instruction_dat(_, data) { data.asIteration().eval(); }, // ???
	Instruction_val(_, ident, val) {
		labels.set(ident.sourceString, val.parse());
	},
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
	Instruction_dw(_, reg1, reg2, reg3) {
		output.push(21); reg1.eval(); reg2.eval(); reg3.eval();
	},
	Instruction_at(_, reg1, reg2) {
		output.push(22); reg1.eval(); reg2.eval();
	},
	Instruction_key(_, reg) { output.push(23); reg.eval(); },
	// snd
	Instruction_end(_) { output.push(25); },

	string(_, parts, __) { parts.eval().forEach(output.push); },
	stringpart(c) { return c.sourceString },
	stringpart_lf(_) { return "\n" },
	stringpart_cr(_) { return "\r" },
	stringpart_tab(_) { return "\t" },
	stringpart_nul(_) { return "\0" },
	stringpart_bs(_) { return "\\" },
})

if (process.argv.length < 3) {
	console.log("gxasm: gxarch assembler\n");
	console.log("Usage: node gxasm.js [file]");
	console.log("Output file is output.gxa");
}

for (let arg of process.argv.slice(2)) {
	const src = fs.readFileSync(arg);
	const match = grammar.match(src);

	if (match.succeeded()) {
		semantics(match).eval();
		
		const outfile = new Uint8Array(output);
		if (outfile.length > 0xFF00)
			throw new Error(`Output file too large, 0x${outfile.length.toString(16)} > 0xFF00`);

		labelrefs.forEach((label, addr) => {
			if (!labels.has(label))
				throw new Error(`Label ${label} not found`);

			const val = labels.get(label);
			outfile.set([(val & 0xFF00) >> 8, val & 0xFF], addr);
		})

		fs.writeFileSync('output.gxa', outfile);
	} else {
		console.error(match.message);
	}
}