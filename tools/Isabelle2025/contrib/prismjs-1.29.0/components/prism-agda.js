/*
 *                                  ***   StarForth   ***
 *
 *  prism-agda.js- FORTH-79 Standard and ANSI C99 ONLY
 *  Modified by - rajames
 *  Last modified - 2025-10-27T12:40:04.261-04
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
 *  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/prismjs-1.29.0/components/prism-agda.js
 */

(function (Prism) {

	Prism.languages.agda = {
		'comment': /\{-[\s\S]*?(?:-\}|$)|--.*/,
		'string': {
			pattern: /"(?:\\(?:\r\n|[\s\S])|[^\\\r\n"])*"/,
			greedy: true,
		},
		'punctuation': /[(){}⦃⦄.;@]/,
		'class-name': {
			pattern: /((?:data|record) +)\S+/,
			lookbehind: true,
		},
		'function': {
			pattern: /(^[ \t]*)(?!\s)[^:\r\n]+(?=:)/m,
			lookbehind: true,
		},
		'operator': {
			pattern: /(^\s*|\s)(?:[=|:∀→λ\\?_]|->)(?=\s)/,
			lookbehind: true,
		},
		'keyword': /\b(?:Set|abstract|constructor|data|eta-equality|field|forall|hiding|import|in|inductive|infix|infixl|infixr|instance|let|macro|module|mutual|no-eta-equality|open|overlap|pattern|postulate|primitive|private|public|quote|quoteContext|quoteGoal|quoteTerm|record|renaming|rewrite|syntax|tactic|unquote|unquoteDecl|unquoteDef|using|variable|where|with)\b/,
	};
}(Prism));
