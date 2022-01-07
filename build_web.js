'use strict';

const fs = require('fs');

if (process.argv.length < 3)
	throw new Error("Specify the name of a gxa file to convert to HTML");

for (let filename of process.argv.slice(2)) {
	if (filename.endsWith('.gxs'))
		throw new Error(`Specified file ${filename} is an assembly file, it must be assembled into a gxa file first`);

	if (!filename.endsWith('.gxa'))
		throw new Error(`Specified file ${filename} is not a gxa file`);

	let index = fs.readFileSync('gxvm.html')
		.toString()
		.split(/\r\n|\r|\n/)
		.filter(l => !l.includes('gxa_builder_remove'));

	const gxa = fs.readFileSync(filename);
															// examples/hello.gxa
	const filenamenoext = filename.replace(/\..*$/, '');      // -> examples/hello
	const filenamearr = filenamenoext.split(/\/|\\/);         // -> ['examples', 'hello']
	const filenamebase = filenamearr[filenamearr.length - 1]; // -> hello

	let array = `const rom = [`;
	gxa.forEach(byte => array += Number(byte) + ',');
	array += '];'

	const codeline = index.findIndex(l => l.includes('gxa_builder_code_here'));
	const imgline = index.findIndex(l => l.includes('gxa_builder_tileset_here'));

	index.splice(codeline, 1, array);
	index.splice(imgline, 1, `<img id="tileset" src="${filenamebase}.png"></img>`)
	index = index.join('\n').replace('<!-- gxa_builder_filename_here -->', filenamebase);

	fs.writeFileSync(`${filenamenoext}.html`, index);
}