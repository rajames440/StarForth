/*
 *                                  ***   StarForth   ***
 *
 *  prism-supercollider.js- FORTH-79 Standard and ANSI C99 ONLY
 *  Modified by - rajames
 *  Last modified - 2025-10-27T12:40:04.353-04
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
 *  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/prismjs-1.29.0/components/prism-supercollider.js
 */

Prism.languages.supercollider = {
	'comment': {
		pattern: /\/\/.*|\/\*(?:[^*/]|\*(?!\/)|\/(?!\*)|\/\*(?:[^*]|\*(?!\/))*\*\/)*\*\//,
		greedy: true
	},
	'string': {
		pattern: /(^|[^\\])"(?:[^"\\]|\\[\s\S])*"/,
		lookbehind: true,
		greedy: true
	},
	'char': {
		pattern: /\$(?:[^\\\r\n]|\\.)/,
		greedy: true
	},
	'symbol': {
		pattern: /(^|[^\\])'(?:[^'\\]|\\[\s\S])*'|\\\w+/,
		lookbehind: true,
		greedy: true
	},

	'keyword': /\b(?:_|arg|classvar|const|nil|var|while)\b/,
	'boolean': /\b(?:false|true)\b/,

	'label': {
		pattern: /\b[a-z_]\w*(?=\s*:)/,
		alias: 'property'
	},

	'number': /\b(?:inf|pi|0x[0-9a-fA-F]+|\d+(?:\.\d+)?(?:[eE][+-]?\d+)?(?:pi)?|\d+r[0-9a-zA-Z]+(?:\.[0-9a-zA-Z]+)?|\d+[sb]{1,4}\d*)\b/,
	'class-name': /\b[A-Z]\w*\b/,

	'operator': /\.{2,3}|#(?![[{])|&&|[!=]==?|\+>>|\+{1,3}|-[->]|=>|>>|\?\?|@\|?@|\|(?:@|[!=]=)?\||!\?|<[!=>]|\*{1,2}|<{2,3}\*?|[-!%&/<>?@|=`]/,
	'punctuation': /[{}()[\].:,;]|#[[{]/
};

Prism.languages.sclang = Prism.languages.supercollider;
