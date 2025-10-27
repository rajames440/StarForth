/*
    Title:      Context.java
    Authors:    Makarius
    License:    See COPYRIGHT for details.
*/

package isabelle.kodkodi;

public class Context
{
    public void output(String msg) {
        System.out.println(msg);
        System.out.flush();
    }
    public void error(String msg) {
        System.err.println(msg);
        System.err.flush();
    }
    public void exit(int rc) {
        System.exit(rc);
    }
}
