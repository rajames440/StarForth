/*
 *                                  ***   StarForth   ***
 *
 *  custom.js- FORTH-79 Standard and ANSI C99 ONLY
 *  Modified by - rajames
 *  Last modified - 2025-10-23T10:54:00.992-04
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
 *  /home/rajames/CLionProjects/StarForth/docs/js/custom.js
 */

(function () {
    // Example: keyboard shortcut to toggle wide layout
    document.addEventListener('keydown', (e) => {
        if (e.key === 'W' && (e.ctrlKey || e.metaKey)) {
            document.body.classList.toggle('wide');
            e.preventDefault();
        }
    });
    // You can add analytics, TOC tweaks, etc. here.
})();
