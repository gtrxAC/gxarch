const fs = require('fs');

module.exports = function (name, args) {
	function needargs(name, argc) {
		if (args.length < argc)
			throw new Error(`.${name} requires ${argc} arguments but ${args.length} were given`);
	}

	switch (name) {
		case 'include':
			needargs('include', 1);
			return fs.readFileSync(args[0])
				.toString()
				.split(/\r\n|\r|\n/);

		case 'ltj':
			needargs('ltj', 3);
			return [
				`lt ${args[0]} ${args[1]} %29`,
				`cj %29 ${args[2]}`
			]

		case 'gtj':
			needargs('gtj', 3);
			return [
				`gt ${args[0]} ${args[1]} %29`,
				`cj %29 ${args[2]}`
			]

		case 'eqj':
			needargs('eqj', 3);
			return [
				`eq ${args[0]} ${args[1]} %29`,
				`cj %29 ${args[2]}`
			]

		case 'ltjs':
			needargs('ltjs', 3);
			return [
				`lt ${args[0]} ${args[1]} %29`,
				`cjs %29 ${args[2]}`
			]

		case 'gtjs':
			needargs('gtjs', 3);
			return [
				`gt ${args[0]} ${args[1]} %29`,
				`cjs %29 ${args[2]}`
			]

		case 'eqjs':
			needargs('eqjs', 3);
			return [
				`eq ${args[0]} ${args[1]} %29`,
				`cjs %29 ${args[2]}`
			]

		case 'keyj':
			needargs('keyj', 2);
			return [
				`set %29 ${args[0]}`,
				`key %29 %29`,
				`cj %29 ${args[1]}`
			]

		case 'keyjs':
			needargs('keyjs', 2);
			return [
				`set %29 ${args[0]}`,
				`key %29 %29`,
				`cjs %29 ${args[1]}`
			]

		case 'mov':
			needargs('mov', 2);
			return [
				`set %29 0`,
				`add ${args[0]} %29 ${args[1]}`
			]
	}
}