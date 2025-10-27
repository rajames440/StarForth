/*
    Title:      ConsoleReporterV2.java
    Authors:    Emina Torlek, MIT
                Jasmin Blanchette, TU Muenchen
    License:    See COPYRIGHT for details.
*/
/*
 * This file was adapted from ConsoleReporter.java distributed with Kodkod
 * (2009-03-17 version) by Jasmin Christian Blanchette. The original
 * copyright notice is reproduced below. The changes to the original file
 * are Copyright 2010 Technische Universitaet Muenchen, like the rest of the
 * Kodkodi software.
 */    
/* 
 * Kodkod -- Copyright (c) 2005-2007, Emina Torlak
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

package isabelle.kodkodi;

import java.util.Calendar;
import java.util.List;
import java.util.Set;
import kodkod.ast.Decl;
import kodkod.ast.Formula;
import kodkod.ast.Relation;
import kodkod.engine.bool.BooleanFormula;
import kodkod.engine.config.Reporter;
import kodkod.instance.Bounds;
import kodkod.util.ints.IntSet;

/**
 * An implementation of the reporter interface that prints messages
 * to the standard output stream.
 * @author Emina Torlak
 */
public class ConsoleReporterV2 implements Reporter {
	void output(String msg) {
		System.out.println(msg);
		System.out.flush();
	}
/*
  SimpleDateFormat is painfully slow. We cannot use it for logging.
*/
/*
    static SimpleDateFormat dateFormat =
        new SimpleDateFormat("HH:mm:ss.SSS");
*/
    static private String num2(int val) {
        char data[] =
          {(char)(val / 10 + (int)'0'), (char)(val % 10 + (int)'0')};
        return new String(data);
    }
    static private String num3(int val) {
        char data[] =
          {(char)(val / 100 + (int)'0'), (char)(val / 10 % 10 + (int)'0'),
           (char)(val % 10 + (int)'0')};
        return new String(data);
    }

    public void printlnWithTimestamp(String msg) {
        Calendar now = Calendar.getInstance();
        output(num2(now.get(Calendar.HOUR_OF_DAY)) + ":" +
						   num2(now.get(Calendar.MINUTE)) + ":" +
                           num2(now.get(Calendar.SECOND)) + "." +
                           num3(now.get(Calendar.MILLISECOND)) + ": " + msg);
/*
        output(dateFormat.format(new Date()) + ": " + msg);
*/
    }

	/**
	 * Constructs a new instance of the ConsoleReporterV2.
	 */
	public ConsoleReporterV2() {}

    int problemNo = -1;
    boolean printsTimestamps = false;

	public void setProblemNo(int no) { problemNo = no; }
	public int problemNo() { return problemNo; }
	public void setPrintsTimestamps(boolean prints) { printsTimestamps = true; }
	public boolean printsTimestamps() { return printsTimestamps; }

    public void println(String msg) {
        String msgWithNo;
        if (problemNo >= 0) {
            msgWithNo = "[" + problemNo + "] " + msg;
        } else {
            msgWithNo = msg;
        }

        if (printsTimestamps)
            printlnWithTimestamp(msgWithNo);
        else
            output(msgWithNo);
    }

	/**
	 * @see kodkod.engine.config.Reporter#generatingSBP()
	 */
	public void generatingSBP() {
	    println("generating lex-leader symmetry breaking predicate...");
	}

	/**
	 * @see kodkod.engine.config.Reporter#flattening(kodkod.engine.bool.BooleanFormula)
	 */
	public void flattening(BooleanFormula circuit) {
		println("flattening...");
	}

	/**
	 * {@inheritDoc}
	 * @see kodkod.engine.config.Reporter#skolemizing(kodkod.ast.Decl, kodkod.ast.Relation, java.util.List)
	 */
	public void skolemizing(Decl decl, Relation skolem, List<Decl> context) {
		println("skolemizing " + decl + ": skolem relation=" + skolem + ", arity=" + skolem.arity());
	}

	/**
	 * @see kodkod.engine.config.Reporter#solvingCNF(int, int, int)
	 */
	public void solvingCNF(int primaryVars, int vars, int clauses) {
		println("solving p cnf " + vars + " " + clauses);
	}


	/**
	 * {@inheritDoc}
	 * @see kodkod.engine.config.Reporter#detectingSymmetries(kodkod.instance.Bounds)
	 */
	public void detectingSymmetries(Bounds bounds){
		println("detecting symmetries...");
	}
	
	/**
	 * {@inheritDoc}
	 * @see kodkod.engine.config.Reporter#detectedSymmetries(java.util.Set)
	 */
	public void detectedSymmetries(Set<IntSet> parts) {
		println("detected symmetries: " + parts);
	}
	
	/**
	 * @see kodkod.engine.config.Reporter#optimizingBoundsAndFormula()
	 */
	public void optimizingBoundsAndFormula() {
		println("optimizing bounds and formula (breaking predicate symmetries, inlining, skolemizing)...");
	}
	
	/**
	 * @see kodkod.engine.config.Reporter#translatingToBoolean(kodkod.ast.Formula, kodkod.instance.Bounds)
	 */
	public void translatingToBoolean(Formula formula, Bounds bounds) {
		println("translating to boolean...");
	}

	/**
	 * @see kodkod.engine.config.Reporter#translatingToCNF(kodkod.engine.bool.BooleanFormula)
	 */
	public void translatingToCNF(BooleanFormula circuit) {
		println("translating to cnf...");
	}
	
	/**
	 * @see java.lang.Object#toString()
	 */
	public String toString() {
		return "ConsoleReporterV2";
	}
}
