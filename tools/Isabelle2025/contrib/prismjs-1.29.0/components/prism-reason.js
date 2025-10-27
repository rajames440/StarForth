/*
 *                                  ***   StarForth   ***
 *
 *  prism-reason.js- FORTH-79 Standard and ANSI C99 ONLY
 *  Modified by - rajames
 *  Last modified - 2025-10-27T12:40:04.334-04
 *
 *  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.
 *
 *  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
 *  To the extent possible under law, the author(s) have dedicated all copyright and related
 *  and neighboring rights to this software to the public domain worldwide.
 *  This software is distributed without any warranty.
 *
 *  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.
 *
 *  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/prismjs-1.29.0/components/prism-reason.js
 */

Prism.languages.reason = Prism.languages.extend('clike', {
	'string': {
		pattern: /"(?:\\(?:\r\n|[\s\S])|[^\\\r\n"])*"/,
		greedy: true
	},
	// 'class-name' must be matched *after* 'constructor' defined below
	'class-name': /\b[A-Z]\w*/,
	'keyword': /\b(?:and|as|assert|begin|class|constraint|do|done|downto|else|end|exception|external|for|fun|function|functor|if|in|include|inherit|initializer|lazy|let|method|module|mutable|new|nonrec|object|of|open|or|private|rec|sig|struct|switch|then|to|try|type|val|virtual|when|while|with)\b/,
	'operator': /\.{3}|:[:=]|\|>|->|=(?:==?|>)?|<=?|>=?|[|^?'#!~`]|[+\-*\/]\.?|\b(?:asr|land|lor|lsl|lsr|lxor|mod)\b/
});
Prism.languages.insertBefore('reason', 'class-name', {
	'char': {
		pattern: /'(?:\\x[\da-f]{2}|\\o[0-3][0-7][0-7]|\\\d{3}|\\.|[^'\\\r\n])'/,
		greedy: true
	},
	// Negative look-ahead prevents from matching things like String.capitalize
	'constructor': /\b[A-Z]\w*\b(?!\s*\.)/,
	'label': {
		pattern: /\b[a-z]\w*(?=::)/,
		alias: 'symbol'
	}
});

// We can't match functions property, so let's not even try.
delete Prism.languages.reason.function;
