#!/usr/bin/env python

"""
fix_prot 0.1

Usage: fixprot.py <config_file> [jobfile]...

Parse a number of E job specifications and protocols, and add the
parameters comment to the protocols.

Options:

-h Print this help.

Copyright 2009 Stephan Schulz, schulz@eprover.org

This code is part of the support structure for the equational
theorem prover E. Visit

 http://www.eprover.org

for more information.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program ; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston,
MA  02111-1307 USA 

The original copyright holder can be contacted as

Stephan Schulz (I4)
Technische Universitaet Muenchen
Institut fuer Informatik
Boltzmannstrasse 3
Garching bei Muenchen
Germany

or via email (address above).
"""

#                                   ***   StarForth   ***
#
#   fix_prot.py- FORTH-79 Standard and ANSI C99 ONLY
#   Modified by - rajames
#   Last modified - 2025-10-27T12:40:03.088-04
#
#   Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.
#
#   This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
#   To the extent possible under law, the author(s) have dedicated all copyright and related
#   and neighboring rights to this software to the public domain worldwide.
#   This software is distributed without any warranty.
#
#   See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.
#
#   /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/e-3.1-1/src/PYTHON/fix_prot.py

import sys
import getopt
import pylib_io
import pylib_generic
import pylib_eprot
import pylib_emconf


if __name__ == '__main__':
    opts, args = getopt.gnu_getopt(sys.argv[1:], "hv", ["Verbose"])

    for option, optarg in opts:
        if option == "-h":
            print __doc__
            sys.exit()
        elif option == "-v" or option =="--verbose":
            pylib_io.Verbose = 1
        else:
            sys.exit("Unknown option "+ option)

    if len(args)<1:
        print __doc__
        sys.exit()

    config = pylib_emconf.e_mconfig(args[0])

    for arg in args[1:]:
        strat = pylib_eprot.estrat_task(arg)
        strat.parse(config.specdir, config.protdir)
        print strat
        strat.set_synced(False)
        strat.sync()
