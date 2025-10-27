/*
 *                                  ***   StarForth   ***
 *
 *  prism-go.js- FORTH-79 Standard and ANSI C99 ONLY
 *  Modified by - rajames
 *  Last modified - 2025-10-27T12:40:04.289-04
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
 *  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/prismjs-1.29.0/components/prism-go.js
 */

Prism.languages.go = Prism.languages.extend('clike', {
	'string': {
		pattern: /(^|[^\\])"(?:\\.|[^"\\\r\n])*"|`[^`]*`/,
		lookbehind: true,
		greedy: true
	},
	'keyword': /\b(?:break|case|chan|const|continue|default|defer|else|fallthrough|for|func|go(?:to)?|if|import|interface|map|package|range|return|select|struct|switch|type|var)\b/,
	'boolean': /\b(?:_|false|iota|nil|true)\b/,
	'number': [
		// binary and octal integers
		/\b0(?:b[01_]+|o[0-7_]+)i?\b/i,
		// hexadecimal integers and floats
		/\b0x(?:[a-f\d_]+(?:\.[a-f\d_]*)?|\.[a-f\d_]+)(?:p[+-]?\d+(?:_\d+)*)?i?(?!\w)/i,
		// decimal integers and floats
		/(?:\b\d[\d_]*(?:\.[\d_]*)?|\B\.\d[\d_]*)(?:e[+-]?[\d_]+)?i?(?!\w)/i
	],
	'operator': /[*\/%^!=]=?|\+[=+]?|-[=-]?|\|[=|]?|&(?:=|&|\^=?)?|>(?:>=?|=)?|<(?:<=?|=|-)?|:=|\.\.\./,
	'builtin': /\b(?:append|bool|byte|cap|close|complex|complex(?:64|128)|copy|delete|error|float(?:32|64)|u?int(?:8|16|32|64)?|imag|len|make|new|panic|print(?:ln)?|real|recover|rune|string|uintptr)\b/
});

Prism.languages.insertBefore('go', 'string', {
	'char': {
		pattern: /'(?:\\.|[^'\\\r\n]){0,10}'/,
		greedy: true
	}
});

delete Prism.languages.go['class-name'];
