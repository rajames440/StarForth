/*
 *                                  ***   StarForth   ***
 *
 *  prism-bro.js- FORTH-79 Standard and ANSI C99 ONLY
 *  Modified by - rajames
 *  Last modified - 2025-10-27T12:40:04.265-04
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
 *  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/prismjs-1.29.0/components/prism-bro.js
 */

Prism.languages.bro = {

	'comment': {
		pattern: /(^|[^\\$])#.*/,
		lookbehind: true,
		inside: {
			'italic': /\b(?:FIXME|TODO|XXX)\b/
		}
	},

	'string': {
		pattern: /(["'])(?:\\(?:\r\n|[\s\S])|(?!\1)[^\\\r\n])*\1/,
		greedy: true
	},

	'boolean': /\b[TF]\b/,

	'function': {
		pattern: /(\b(?:event|function|hook)[ \t]+)\w+(?:::\w+)?/,
		lookbehind: true
	},

	'builtin': /(?:@(?:load(?:-(?:plugin|sigs))?|unload|prefixes|ifn?def|else|(?:end)?if|DIR|FILENAME))|(?:&?(?:add_func|create_expire|default|delete_func|encrypt|error_handler|expire_func|group|log|mergeable|optional|persistent|priority|raw_output|read_expire|redef|rotate_interval|rotate_size|synchronized|type_column|write_expire))/,

	'constant': {
		pattern: /(\bconst[ \t]+)\w+/i,
		lookbehind: true
	},

	'keyword': /\b(?:add|addr|alarm|any|bool|break|const|continue|count|delete|double|else|enum|event|export|file|for|function|global|hook|if|in|int|interval|local|module|next|of|opaque|pattern|port|print|record|return|schedule|set|string|subnet|table|time|timeout|using|vector|when)\b/,

	'operator': /--?|\+\+?|!=?=?|<=?|>=?|==?=?|&&|\|\|?|\?|\*|\/|~|\^|%/,

	'number': /\b0x[\da-f]+\b|(?:\b\d+(?:\.\d*)?|\B\.\d+)(?:e[+-]?\d+)?/i,

	'punctuation': /[{}[\];(),.:]/
};
