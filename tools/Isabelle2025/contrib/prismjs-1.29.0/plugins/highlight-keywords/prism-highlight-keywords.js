/*
 *                                  ***   StarForth   ***
 *
 *  prism-highlight-keywords.js- FORTH-79 Standard and ANSI C99 ONLY
 *  Modified by - rajames
 *  Last modified - 2025-10-27T12:40:04.293-04
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
 *  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/prismjs-1.29.0/plugins/highlight-keywords/prism-highlight-keywords.js
 */

(function () {

	if (typeof Prism === 'undefined') {
		return;
	}

	Prism.hooks.add('wrap', function (env) {
		if (env.type !== 'keyword') {
			return;
		}
		env.classes.push('keyword-' + env.content);
	});

}());
