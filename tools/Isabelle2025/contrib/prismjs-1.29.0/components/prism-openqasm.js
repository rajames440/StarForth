/*
 *                                  ***   StarForth   ***
 *
 *  prism-openqasm.js- FORTH-79 Standard and ANSI C99 ONLY
 *  Modified by - rajames
 *  Last modified - 2025-10-27T12:40:04.322-04
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
 *  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/prismjs-1.29.0/components/prism-openqasm.js
 */

// https://qiskit.github.io/openqasm/grammar/index.html

Prism.languages.openqasm = {
	'comment': /\/\*[\s\S]*?\*\/|\/\/.*/,
	'string': {
		pattern: /"[^"\r\n\t]*"|'[^'\r\n\t]*'/,
		greedy: true
	},

	'keyword': /\b(?:CX|OPENQASM|U|barrier|boxas|boxto|break|const|continue|ctrl|def|defcal|defcalgrammar|delay|else|end|for|gate|gphase|if|in|include|inv|kernel|lengthof|let|measure|pow|reset|return|rotary|stretchinf|while)\b|#pragma\b/,
	'class-name': /\b(?:angle|bit|bool|creg|fixed|float|int|length|qreg|qubit|stretch|uint)\b/,
	'function': /\b(?:cos|exp|ln|popcount|rotl|rotr|sin|sqrt|tan)\b(?=\s*\()/,

	'constant': /\b(?:euler|pi|tau)\b|π|𝜏|ℇ/,
	'number': {
		pattern: /(^|[^.\w$])(?:\d+(?:\.\d*)?|\.\d+)(?:e[+-]?\d+)?(?:dt|ns|us|µs|ms|s)?/i,
		lookbehind: true
	},
	'operator': /->|>>=?|<<=?|&&|\|\||\+\+|--|[!=<>&|~^+\-*/%]=?|@/,
	'punctuation': /[(){}\[\];,:.]/
};

Prism.languages.qasm = Prism.languages.openqasm;
