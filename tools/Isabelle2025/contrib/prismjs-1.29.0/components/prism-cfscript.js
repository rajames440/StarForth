/*
 *                                  ***   StarForth   ***
 *
 *  prism-cfscript.js- FORTH-79 Standard and ANSI C99 ONLY
 *  Modified by - rajames
 *  Last modified - 2025-10-27T12:40:04.269-04
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
 *  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/prismjs-1.29.0/components/prism-cfscript.js
 */

// https://cfdocs.org/script
Prism.languages.cfscript = Prism.languages.extend('clike', {
	'comment': [
		{
			pattern: /(^|[^\\])\/\*[\s\S]*?(?:\*\/|$)/,
			lookbehind: true,
			inside: {
				'annotation': {
					pattern: /(?:^|[^.])@[\w\.]+/,
					alias: 'punctuation'
				}
			}
		},
		{
			pattern: /(^|[^\\:])\/\/.*/,
			lookbehind: true,
			greedy: true
		}
	],
	'keyword': /\b(?:abstract|break|catch|component|continue|default|do|else|extends|final|finally|for|function|if|in|include|package|private|property|public|remote|required|rethrow|return|static|switch|throw|try|var|while|xml)\b(?!\s*=)/,
	'operator': [
		/\+\+|--|&&|\|\||::|=>|[!=]==|[-+*/%&|^!=<>]=?|\?(?:\.|:)?|:/,
		/\b(?:and|contains|eq|equal|eqv|gt|gte|imp|is|lt|lte|mod|not|or|xor)\b/
	],
	'scope': {
		pattern: /\b(?:application|arguments|cgi|client|cookie|local|session|super|this|variables)\b/,
		alias: 'global'
	},
	'type': {
		pattern: /\b(?:any|array|binary|boolean|date|guid|numeric|query|string|struct|uuid|void|xml)\b/,
		alias: 'builtin'
	}
});

Prism.languages.insertBefore('cfscript', 'keyword', {
	// This must be declared before keyword because we use "function" inside the lookahead
	'function-variable': {
		pattern: /[_$a-zA-Z\xA0-\uFFFF](?:(?!\s)[$\w\xA0-\uFFFF])*(?=\s*[=:]\s*(?:\bfunction\b|(?:\((?:[^()]|\([^()]*\))*\)|(?!\s)[_$a-zA-Z\xA0-\uFFFF](?:(?!\s)[$\w\xA0-\uFFFF])*)\s*=>))/,
		alias: 'function'
	}
});

delete Prism.languages.cfscript['class-name'];
Prism.languages.cfc = Prism.languages['cfscript'];
