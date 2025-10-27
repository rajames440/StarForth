/*
 *                                  ***   StarForth   ***
 *
 *  prism-gap.js- FORTH-79 Standard and ANSI C99 ONLY
 *  Modified by - rajames
 *  Last modified - 2025-10-27T12:40:04.285-04
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
 *  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/prismjs-1.29.0/components/prism-gap.js
 */

// https://www.gap-system.org/Manuals/doc/ref/chap4.html
// https://www.gap-system.org/Manuals/doc/ref/chap27.html

Prism.languages.gap = {
	'shell': {
		pattern: /^gap>[\s\S]*?(?=^gap>|$(?![\s\S]))/m,
		greedy: true,
		inside: {
			'gap': {
				pattern: /^(gap>).+(?:(?:\r(?:\n|(?!\n))|\n)>.*)*/,
				lookbehind: true,
				inside: null // see below
			},
			'punctuation': /^gap>/
		}
	},

	'comment': {
		pattern: /#.*/,
		greedy: true
	},
	'string': {
		pattern: /(^|[^\\'"])(?:'(?:[^\r\n\\']|\\.){1,10}'|"(?:[^\r\n\\"]|\\.)*"(?!")|"""[\s\S]*?""")/,
		lookbehind: true,
		greedy: true,
		inside: {
			'continuation': {
				pattern: /([\r\n])>/,
				lookbehind: true,
				alias: 'punctuation'
			}
		}
	},

	'keyword': /\b(?:Assert|Info|IsBound|QUIT|TryNextMethod|Unbind|and|atomic|break|continue|do|elif|else|end|fi|for|function|if|in|local|mod|not|od|or|quit|readonly|readwrite|rec|repeat|return|then|until|while)\b/,
	'boolean': /\b(?:false|true)\b/,

	'function': /\b[a-z_]\w*(?=\s*\()/i,

	'number': {
		pattern: /(^|[^\w.]|\.\.)(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?(?:_[a-z]?)?(?=$|[^\w.]|\.\.)/,
		lookbehind: true
	},

	'continuation': {
		pattern: /([\r\n])>/,
		lookbehind: true,
		alias: 'punctuation'
	},
	'operator': /->|[-+*/^~=!]|<>|[<>]=?|:=|\.\./,
	'punctuation': /[()[\]{},;.:]/
};

Prism.languages.gap.shell.inside.gap.inside = Prism.languages.gap;
