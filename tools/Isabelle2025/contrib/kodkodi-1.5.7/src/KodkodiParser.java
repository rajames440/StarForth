// $ANTLR 3.1.1 src/Kodkodi.g 2021-10-07 20:34:49

package isabelle.kodkodi;

import java.util.Collection;
import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedHashSet;
import java.util.ListIterator;
import java.util.Map;
import java.util.Set;
import java.util.Vector;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import kodkod.ast.BinaryExpression;
import kodkod.ast.BinaryIntExpression;
import kodkod.ast.ComparisonFormula;
import kodkod.ast.Decl;
import kodkod.ast.Decls;
import kodkod.ast.ExprToIntCast;
import kodkod.ast.Expression;
import kodkod.ast.Formula;
import kodkod.ast.IntComparisonFormula;
import kodkod.ast.IntConstant;
import kodkod.ast.IntExpression;
import kodkod.ast.IntToExprCast;
import kodkod.ast.Node;
import kodkod.ast.QuantifiedFormula;
import kodkod.ast.Relation;
import kodkod.ast.UnaryExpression;
import kodkod.ast.UnaryIntExpression;
import kodkod.ast.Variable;
import kodkod.ast.operator.ExprCastOperator;
import kodkod.ast.operator.ExprOperator;
import kodkod.ast.operator.IntCastOperator;
import kodkod.ast.operator.IntCompOperator;
import kodkod.ast.operator.IntOperator;
import kodkod.ast.operator.Multiplicity;
import kodkod.ast.visitor.AbstractReplacer;
import kodkod.engine.Solution;
import kodkod.engine.Solver;
import kodkod.engine.Proof;
import kodkod.engine.config.Options;
import kodkod.engine.fol2sat.HigherOrderDeclException;
import kodkod.engine.fol2sat.UnboundLeafException;
import kodkod.engine.fol2sat.TranslationRecord;
import kodkod.engine.satlab.SATFactory;
import kodkod.engine.ucore.RCEStrategy;
import kodkod.instance.Bounds;
import kodkod.instance.Instance;
import kodkod.instance.Tuple;
import kodkod.instance.TupleFactory;
import kodkod.instance.TupleSet;
import kodkod.instance.Universe;
import kodkod.util.ints.IntSet;
import kodkod.util.ints.SparseSequence;
import org.antlr.runtime.CommonTokenStream;


import org.antlr.runtime.*;
import java.util.Stack;
import java.util.List;
import java.util.ArrayList;

public class KodkodiParser extends Parser {
    public static final String[] tokenNames = new String[] {
        "<invalid>", "<EOR>", "<DOWN>", "<UP>", "SOLVER", "COLON", "STR_LITERAL", "COMMA", "SYMMETRY_BREAKING", "NUM", "SHARING", "BIT_WIDTH", "SKOLEM_DEPTH", "FLATTEN", "TRUE", "FALSE", "TIMEOUT", "DELAY", "UNIV", "UNIV_NAME", "TUPLE_SET_REG", "COLON_EQ", "TUPLE_REG", "BOUNDS", "RELATION_NAME", "BRACKET_LEFT", "BRACKET_RIGHT", "INT_BOUNDS", "FORMULA_REG", "REL_EXPR_REG", "INT_EXPR_REG", "SOLVE", "SEMICOLON", "PLUS", "MINUS", "AND", "ARROW", "OFFSET_UNIV_NAME", "PAREN_LEFT", "PAREN_RIGHT", "BRACE_LEFT", "DOT_DOT", "HASH", "BRACE_RIGHT", "NONE", "ALL", "ATOM_NAME", "TUPLE_NAME", "BAR", "SOME", "SUM", "LET", "IF", "THEN", "ELSE", "OR", "IFF", "IMPLIES", "NOT", "EQ", "LT", "LE", "GT", "GE", "IN", "ACYCLIC", "FUNCTION", "ONE", "LONE", "TOTAL_ORDERING", "SHL", "SHA", "SHR", "STAR", "DIVIDE", "MODULO", "HAT", "AMP", "OVERRIDE", "IFNO", "DOT", "VARIABLE_NAME", "IDEN", "INTS", "TILDE", "ABS", "SGN", "BITS", "INT", "NO", "SET", "NAT", "WHITESPACE", "INLINE_COMMENT", "BLOCK_COMMENT", "RELATION"
    };
    public static final int FUNCTION=66;
    public static final int SHR=72;
    public static final int BITS=87;
    public static final int LT=60;
    public static final int STAR=73;
    public static final int SHL=70;
    public static final int BRACE_RIGHT=43;
    public static final int SHA=71;
    public static final int ONE=67;
    public static final int NOT=58;
    public static final int EOF=-1;
    public static final int TUPLE_REG=22;
    public static final int BRACKET_RIGHT=26;
    public static final int PAREN_LEFT=38;
    public static final int IDEN=82;
    public static final int OFFSET_UNIV_NAME=37;
    public static final int IFNO=79;
    public static final int HAT=76;
    public static final int SKOLEM_DEPTH=12;
    public static final int INLINE_COMMENT=93;
    public static final int SHARING=10;
    public static final int BLOCK_COMMENT=94;
    public static final int EQ=59;
    public static final int DIVIDE=74;
    public static final int TIMEOUT=16;
    public static final int INTS=83;
    public static final int IFF=56;
    public static final int GE=63;
    public static final int IMPLIES=57;
    public static final int BRACKET_LEFT=25;
    public static final int ELSE=54;
    public static final int NO=89;
    public static final int WHITESPACE=92;
    public static final int SEMICOLON=32;
    public static final int INT=88;
    public static final int ACYCLIC=65;
    public static final int TOTAL_ORDERING=69;
    public static final int ABS=85;
    public static final int UNIV=18;
    public static final int STR_LITERAL=6;
    public static final int BOUNDS=23;
    public static final int SYMMETRY_BREAKING=8;
    public static final int NONE=44;
    public static final int BRACE_LEFT=40;
    public static final int OR=55;
    public static final int GT=62;
    public static final int TUPLE_SET_REG=20;
    public static final int FALSE=15;
    public static final int SOLVE=31;
    public static final int DELAY=17;
    public static final int TUPLE_NAME=47;
    public static final int AMP=77;
    public static final int NAT=91;
    public static final int AND=35;
    public static final int SUM=50;
    public static final int BIT_WIDTH=11;
    public static final int IF=52;
    public static final int RELATION_NAME=24;
    public static final int VARIABLE_NAME=81;
    public static final int THEN=53;
    public static final int IN=64;
    public static final int COMMA=7;
    public static final int SOME=49;
    public static final int ALL=45;
    public static final int INT_BOUNDS=27;
    public static final int TILDE=84;
    public static final int PLUS=33;
    public static final int DOT=80;
    public static final int PAREN_RIGHT=39;
    public static final int DOT_DOT=41;
    public static final int MODULO=75;
    public static final int COLON_EQ=21;
    public static final int ATOM_NAME=46;
    public static final int INT_EXPR_REG=30;
    public static final int HASH=42;
    public static final int RELATION=95;
    public static final int SET=90;
    public static final int REL_EXPR_REG=29;
    public static final int MINUS=34;
    public static final int TRUE=14;
    public static final int NUM=9;
    public static final int COLON=5;
    public static final int UNIV_NAME=19;
    public static final int SGN=86;
    public static final int OVERRIDE=78;
    public static final int FLATTEN=13;
    public static final int ARROW=36;
    public static final int SOLVER=4;
    public static final int BAR=48;
    public static final int LET=51;
    public static final int LE=61;
    public static final int FORMULA_REG=28;
    public static final int LONE=68;

    // delegates
    // delegators


        public KodkodiParser(TokenStream input) {
            this(input, new RecognizerSharedState());
        }
        public KodkodiParser(TokenStream input, RecognizerSharedState state) {
            super(input, state);
             
        }
        

    public String[] getTokenNames() { return KodkodiParser.tokenNames; }
    public String getGrammarFileName() { return "src/Kodkodi.g"; }


    Context context;
    boolean verbose;
    int maxSolutions;
    boolean solveAll;
    boolean prove;
    boolean cleanUpInst;
    Lexer lexer;
    int problemNo = 0;
    long startParsingTime;
    Options options;
    int timeout = 0;
    int delay = 0;
    Universe universe;
    TupleFactory factory;
    Bounds bounds;
    ExecutorService executor = null;
    int nextInt;
    HashMap<Integer, Relation> atomRelations;
    HashMap<Integer, Relation> univRelations;
    Relation[] smallSets = new Relation[10];
    Vector<Vector<Relation>> relations = new Vector<Vector<Relation>>();
    Vector<Vector<Variable>> variables = new Vector<Vector<Variable>>();
    Vector<Vector<TupleSet>> tupleSets;
    Vector<Vector<Tuple>> tuples;
    Vector<Expression> exprs;
    Vector<IntExpression> intExprs;
    Vector<Formula> formulas;
    HashMap<Integer, IntConstant> intConstants =
        new HashMap<Integer, IntConstant>();
    StringBuilder bigBuf = null;

    public final void reset() { options = null; }
    public final void ensure() {
        if (options != null)
            return;

        ++problemNo;
        startParsingTime = System.currentTimeMillis();
        ConsoleReporterV2 reporter =
            new ConsoleReporterV2() {
                @Override
                void output(String msg) { context.output(msg); }
            };
        reporter.setProblemNo(problemNo);
        reporter.setPrintsTimestamps(true);
        reporter.println("parsing...");
        options = new Options();
        options.setReporter(reporter);
        timeout = 0;
        delay = 0;
        universe = null;
        factory = null;
        bounds = null;
        nextInt = 0;
        atomRelations = new HashMap<Integer, Relation>(); 
        univRelations = new HashMap<Integer, Relation>();
        tupleSets = new Vector<Vector<TupleSet>>();
        tuples = new Vector<Vector<Tuple>>();
        exprs = new Vector<Expression>();
        intExprs = new Vector<IntExpression>();
        formulas = new Vector<Formula>();
        if (bigBuf == null)
            bigBuf = new StringBuilder();
    }

    public final static KodkodiParser create(Context context, ExecutorService executor,
            boolean verbose, boolean solveAll, boolean prove, int maxSolutions, boolean cleanUpInst,
            Lexer lexer) {
        KodkodiParser parser = new KodkodiParser(new CommonTokenStream(lexer));
        parser.context = context;
        parser.executor = executor;
        parser.verbose = verbose;
        parser.solveAll = solveAll;
        parser.prove = prove;
        parser.maxSolutions = maxSolutions;
        parser.cleanUpInst = cleanUpInst;
        parser.lexer = lexer;
        return parser;
    }

    public final void emitErrorMessage(String message) {
        context.error(message);
    }

    private final static String fixedMessage(Throwable except) {
        StringBuilder buf = new StringBuilder(except.getMessage());
        if (buf.length() > 0 && buf.charAt(buf.length() - 1) == '.')
            buf.deleteCharAt(buf.length() - 1);
        return buf.toString();
    }

    private final void setTuple(Token token, Tuple tuple) {
        final String text = token.getText();
        final int arity = arity(text);
        final int id = id(text);

        if (arity >= tuples.size()) {
            int oldSize = tuples.size();
            tuples.setSize(arity + 1);
            for (int i = oldSize; i <= arity; ++i)
                tuples.setElementAt(new Vector<Tuple>(), i);
        }
        if (id >= tuples.elementAt(arity).size())
            tuples.elementAt(arity).setSize(id + 1);
        tuples.elementAt(arity).setElementAt(tuple, id);
    }
    private final void setTupleSet(Token token, TupleSet tupleSet) {
        final String text = token.getText();
        final int arity = arity(text);
        final int id = id(text);

        if (arity >= tupleSets.size()) {
            int oldSize = tupleSets.size();
            tupleSets.setSize(arity + 1);
            for (int i = oldSize; i <= arity; ++i)
                tupleSets.setElementAt(new Vector<TupleSet>(), i);
        }
        if (id >= tupleSets.elementAt(arity).size())
            tupleSets.elementAt(arity).setSize(id + 1);
        tupleSets.elementAt(arity).setElementAt(tupleSet, id);
    }
    private final void setFormulaReg(Token token, Formula formula) {
        final int id = id(token.getText());
        if (id >= formulas.size())
            formulas.setSize(id + 1);
        formulas.setElementAt(formula, id);
    }
    private final void setExprReg(Token token, Expression expr) {
        final int id = id(token.getText());
        if (id >= exprs.size())
            exprs.setSize(id + 1);
        exprs.setElementAt(expr, id);
    }
    private final void setIntExprReg(Token token, IntExpression intExpr) {
        final int id = id(token.getText());
        if (id >= intExprs.size())
            intExprs.setSize(id + 1);
        intExprs.setElementAt(intExpr, id);
    }
    private final void setReg(Token token, Node node) {
        switch (token.getText().charAt(1)) {
        case 'f':
            setFormulaReg(token, (Formula)node);
            break;
        case 'e':
            setExprReg(token, (Expression)node);
            break;
        case 'i':
            setIntExprReg(token, (IntExpression)node);
            break;
        default:
            panic();
        }
    }

    private final TupleSet univTupleSet(Token token, int id)
    {
        int card = card(id);
        int offset = offset(id);

        if (card == 0) {
            return factory.noneOf(1);
        } else if (card + offset > universe.size()) {
            huh(token, "universe too small for '" + token.getText() + "'");
            return factory.noneOf(1);
        } else {
            return factory.range(factory.tuple(universe.atom(offset)),
                                 factory.tuple(universe.atom(card + offset - 1)));
        }
    }
    private final Object getAtom(Token token) {
        final String text = token.getText();
        try {
            return universe.atom(id(text));
        } catch (IndexOutOfBoundsException except) {
            huh(token, "atom '" + text + "' not in universe");
            return universe.atom(0);
        }
    }
    private final Relation getRelation(Token token) {
        final String text = token.getText();
        int arity = 1;
        int id;
        Relation relation = null;

        if (text.charAt(0) == 'A') {
            id = id(text);
            relation = atomRelations.get(id);
            if (relation == null) {
                relation = Relation.nary(text, 1);
                try {
                    bounds.boundExactly(relation, factory.setOf(universe.atom(id)));
                } catch (IndexOutOfBoundsException except) {
                    huh(token, "atom '" + text + "' not in universe");
                }
                atomRelations.put(id, relation);
            }
            return relation;
        } else if (text.charAt(0) == 'u') {
            id = id(text);
            relation = univRelations.get(id);
            if (relation == null) {
                relation = Relation.nary(text, 1);
                bounds.boundExactly(relation, univTupleSet(token, id));
                univRelations.put(id, relation);
            }
            return relation;
        } else if (text.charAt(0) == 's' && text.length() == 2) {
            id = text.charAt(1) - '0';
            relation = smallSets[id];
            if (relation != null)
                return relation;
        } else {
            arity = arity(text);
            id = id(text);
        }

        if (arity < relations.size() && id < relations.elementAt(arity).size()) {
            relation = relations.elementAt(arity).elementAt(id);
        } else {
            if (arity >= relations.size()) {
                int oldSize = relations.size();
                relations.setSize(arity + 1);
                for (int i = oldSize; i <= arity; ++i)
                    relations.setElementAt(new Vector<Relation>(), i);
            }
            relations.elementAt(arity).setSize(id + 1);
        }
        if (relation == null) {
            relation = Relation.nary(text, arity);
            relations.elementAt(arity).setElementAt(relation, id);
            if (arity == 1 && id < 10)
                smallSets[id] = relation;
        }
        return relation;
    }
    private final Variable getVariable(Token token) {
        final String text = token.getText();
        final int arity = arity(text);
        final int id = id(text);
        Variable variable = null;

        if (arity < variables.size() && id < variables.elementAt(arity).size()) {
            variable = variables.elementAt(arity).elementAt(id);
        } else {
            if (arity >= variables.size()) {
                int oldSize = variables.size();
                variables.setSize(arity + 1);
                for (int i = oldSize; i <= arity; ++i)
                    variables.setElementAt(new Vector<Variable>(), i);
            }
            variables.elementAt(arity).setSize(id + 1);
        }
        if (variable == null) {
            variable = Variable.nary(text, arity);
            variables.elementAt(arity).setElementAt(variable, id);
        }
        return variable;
    }
    private final TupleSet getTupleSet(Token token) {
        final String text = token.getText();
        final int arity = arity(text);
        final int id = id(text);

        if (arity < tupleSets.size() && id < tupleSets.elementAt(arity).size()) {
            return tupleSets.elementAt(arity).elementAt(id);
        } else {
            huh(token, "No such tuple set '" + text + "'");
            return factory.noneOf(arity);
        }
    }
    private final Tuple getTuple(Token token) {
        final String text = token.getText();
        final int arity = arity(text);
        final int id = id(text);

        if (arity < tuples.size() && id < tuples.elementAt(arity).size()) {
            return tuples.elementAt(arity).elementAt(id);
        } else {
            huh(token, "No such tuple '" + text + "'");
            return factory.tuple(1, 0);
        }
    }
    private final Formula getFormulaReg(Token token) {
        final String text = token.getText();
        try {
            Formula formula = formulas.elementAt(id(text));
            if (formula != null) {
                return formula;
            } else {
                huh(token, "No such formula '" + text + "'");
                return Formula.FALSE;
            }
        } catch (IndexOutOfBoundsException except) {
            huh(token, "No such formula '" + text + "'");
            return Formula.FALSE;
        }
    }
    private final Expression getExprReg(Token token) {
        final String text = token.getText();
        try {
            Expression expr = exprs.elementAt(id(text));
            if (expr != null) {
                return expr;
            } else {
                huh(token, "No such relational expression '" + text + "'");
                return Expression.NONE;
            }
        } catch (IndexOutOfBoundsException except) {
            huh(token, "No such relational expression '" + text + "'");
            return Expression.NONE;
        }
    }
    private final IntExpression getIntExprReg(Token token) {
        final String text = token.getText();
        try {
            IntExpression intExpr = intExprs.elementAt(id(token.getText()));
            if (intExpr != null) {
                return intExpr;
            } else {
                huh(token, "No such integer expression '" + text + "'");
                return IntConstant.constant(0);
            }
        } catch (IndexOutOfBoundsException except) {
            huh(token, "No such integer expression '" + text + "'");
            return IntConstant.constant(0);
        }
    }
    private final IntConstant getIntConstant(Token token)
    {
        final int value = getInt(token);
        IntConstant intConstant = intConstants.get(value);
        if (intConstant == null) {
            intConstant = IntConstant.constant(value);
            intConstants.put(value, intConstant);
        }
        return intConstant;
    }
    private final int getInt(String text) {
        try {
            return Integer.parseInt(text);
        } catch (NumberFormatException except) {
            huh(Token.INVALID_TOKEN, "integer '" + text + "' out of range");
            return 0;
        }
    }
    private final int getInt(Token token) {
        return getInt(token, token.getText());
    }
    private final int getInt(Token token, String text) {
        try {
            return Integer.parseInt(text);
        } catch (NumberFormatException except) {
            huh(token, "integer '" + text + "' out of range");
            return 0;
        }
    }

    private final void huh(Token token, String message) {
        SemanticException except = new SemanticException(token, message);
        reportError(except);
    }
    private final void panic() {
        Exception except = new Exception();
        context.error("this cannot happen");
        except.printStackTrace();
        context.exit(1);
    }

    private final int arity(String name) {
        int start = 0;

        if (name.charAt(0) == '$')
            ++start;

        switch (name.charAt(start)) {
        case 'A':
        case 'a':
        case 'S':
        case 's':
        case 'u':
            return 1;
        case 'P':
        case 'p':
        case 'R':
        case 'r':
            return 2;
        case 'M':
        case 'm':
        case 'T':
        case 't':
            {
                final int mid = name.indexOf('_');
                if (mid >= 0) {
                    int nary = getInt(name.substring(start + 1, mid));
                    if (nary >= 3) {
                        return nary;
                    } else {
                        huh(Token.INVALID_TOKEN,
                            "expected arity 3 or more, got " + nary);
                        return 3;
                    }
                }
            }
        }

        panic();
        return 0;
    }
    private final int OFFSET_LIMIT = 65536;
    private final int CARD_LIMIT = 65536;
    private final int universeId(int card, int offset) {
        if (card < 0 || card >= CARD_LIMIT) {
            huh(Token.INVALID_TOKEN,
                "expected reasonable cardinality, got '" + card + "'");
            return 0;
        }
        if (offset < 0 || offset >= OFFSET_LIMIT) {
            huh(Token.INVALID_TOKEN,
                "expected reasonable offset, got '" + offset + "'");
            return 0;
        }
        return offset * CARD_LIMIT + card;
    }
    private final int card(int id) { return id % CARD_LIMIT; }
    private final int offset(int id) { return id / CARD_LIMIT; }
    private final int id(String name) {
        int start = 0;

        if (name.charAt(0) == '$')
            ++start;
        int mid = name.indexOf('_');
        if (mid == -1)
            mid = start;

        switch (name.charAt(start)) {
        case 'A':
        case 'a':
        case 'e':
        case 'f':
        case 'i':
        case 'P':
        case 'p':
        case 'T':
        case 't':
            {
                int id = getInt(name.substring(mid + 1));
                if (id >= 0) {
                    return id;
                } else {
                    huh(Token.INVALID_TOKEN,
                        "expected nonnegative ID, got '" + id + "'");
                    return 0;
                }
            }
        case 'R':
        case 'r':
        case 'S':
        case 's':
        case 'M':
        case 'm':
            {
                int id;
                if (name.endsWith("'")) {
                    int rawId = getInt(name.substring(mid + 1, name.length() - 1));
                    id = 2 * rawId + 1;
                } else {
                    int rawId = getInt(name.substring(mid + 1));
                    id = 2 * rawId;
                }
                if (id >= 0) {
                    return id;
                } else {
                    huh(Token.INVALID_TOKEN,
                        "expected nonnegative ID, got '" + id + "'");
                    return 0;
                }
            }
        case 'u':
            {
                final int at = name.indexOf('@');
                int card;
                int offset = 0;
                if (at >= 0) {
                    card = getInt(name.substring(mid + 1, at));
                    offset = getInt(name.substring(at + 1));
                } else {
                    card = getInt(name.substring(mid + 1));
                }
                return universeId(card, offset);
            }
        }

        panic();
        return 0;
    }
    private final String sstr(Token token) {
        String str = token.getText();
        if (str.length() >= 2) {
            return str.substring(1, str.length() - 1);
        } else {
            panic();
            return "";
        }
    }

    private final Formula F(Token token, Object object) {
        try {
            return (Formula)object;
        } catch (ClassCastException except) {
            huh(token, "expected formula");
            return Formula.FALSE;
        }
    }
    private final Expression E(Token token, Object object) {
        try {
            return (Expression)object;
        } catch (ClassCastException except) {
            huh(token, "expected relational expression");
            return Expression.NONE;
        }
    }
    private final IntExpression I(Token token, Object object) {
        try {
            return (IntExpression)object;
        } catch (ClassCastException except) {
            huh(token, "expected integer expression");
            return IntConstant.constant(0);
        }
    }
    private final static boolean isExpression(Object object) {
        try {
            Expression expr = (Expression)object;
            return true;
        } catch (ClassCastException except) {
            return false;
        }
    }
    private final static boolean isIntExpression(Object object) {
        try {
            IntExpression expr = (IntExpression)object;
            return true;
        } catch (ClassCastException except) {
            return false;
        }
    }

    private final static class SemanticException extends RecognitionException {
        String message;

        public SemanticException(Token token, String message) {
            this.token = token;
            line = token.getLine();
            charPositionInLine = token.getCharPositionInLine();
            this.message = message;
        }

        public final String getMessage() { return message; }
    }

    private final static boolean isRealRelation(Relation relation) {
        switch (relation.name().charAt(0)) {
        case 's':
        case 'r':
        case 'm':
            return true;
        default:
            return false;
        }
    }

    private final static Instance cleanedUpInstance(Instance instance, Bounds bounds,
                                                    boolean aggressive)
    {
        Instance result = new Instance(bounds.universe());
        Set<Relation> relations = instance.relations();

        Map<Relation, TupleSet> lowerBounds = bounds.lowerBounds();
        Map<Relation, TupleSet> upperBounds = bounds.upperBounds();
        Map<Relation, TupleSet> relationTuples = instance.relationTuples();

        for (Relation relation : relations) {
            if ((aggressive
                 && !lowerBounds.get(relation).equals(upperBounds.get(relation)))
                || isRealRelation(relation))
                result.add(relation, relationTuples.get(relation));
        }

        SparseSequence<TupleSet> intBounds = bounds.intBounds();
        SparseSequence<TupleSet> intTuples = instance.intTuples();
        int[] indices = intTuples.indices().toArray();

        for (int index : indices) {
            TupleSet tuples = intTuples.get(index);
            if (!intBounds.get(index).equals(tuples))
                result.add(index, tuples);
        }
        return result;
    }

    private final void solve(Token token, int problemNo, long parsingTime,
                             Options options, int timeout, int delay,
                             Universe universe, Bounds bounds, Formula formula)
    {
        final Solver solver = new Solver(options);
        boolean missingSatSolver = false;

        if (prove) {
            options.setLogTranslation(2);
            options.setCoreGranularity(0);
        }

        try {
            boolean exit = false;

            ((ConsoleReporterV2)options.reporter()).println("kodkoding...");

            final Vector<Solution> solutions = new Vector<Solution>();
            if (solveAll) {
                int k = maxSolutions;
                Iterator<Solution> i = solver.solveAll(formula, bounds);
                if (i.hasNext() && k-- > 0) {
                    solutions.add(i.next());
                    while (i.hasNext()) {
                        Solution solution = i.next();
                        if (solution.outcome() != Solution.Outcome.UNSATISFIABLE
                                && k-- > 0)
                            solutions.add(solution);
                    }
                }
            } else {
                solutions.add(solver.solve(formula, bounds));
            }
            ((ConsoleReporterV2)options.reporter()).println("done kodkoding");

            final StringBuilder buf = new StringBuilder();
            buf.append("*** PROBLEM " + problemNo + " ***\n");
            if (verbose) {
                buf.append("\n---UNIVERSE---\n" + universe.toString() + "\n");
                buf.append("\n---BOUNDS---\n" + bounds.toString() + "\n");
                buf.append("\n---FORMULA---\n" + formula.toString() + "\n");
            }
            for (int i = 0; i < solutions.size(); ++i) {
                Solution solution = solutions.elementAt(i);

                if (solution.outcome() == Solution.Outcome.TRIVIALLY_SATISFIABLE
                        || solution.outcome() == Solution.Outcome.SATISFIABLE) {
                    synchronized (this) {
                        --maxSolutions;
                        exit = (maxSolutions <= 0);
                    }
                }

                buf.append("\n---OUTCOME---\n");
                buf.append(solution.outcome());
                buf.append("\n");

                if (solution.instance() != null) {
                    buf.append("\n---INSTANCE---\n");
                    buf.append(cleanedUpInstance(solution.instance(), bounds,
                                                 cleanUpInst));
                    buf.append("\n");
                }
                if (solution.proof() != null) {
                    try {
                        if (solution.outcome() == Solution.Outcome.UNSATISFIABLE
                                || solution.outcome()
                                   == Solution.Outcome.TRIVIALLY_UNSATISFIABLE) {
                            Proof p = solution.proof();
                            try {
                                p.minimize(new RCEStrategy(p.log()));
                            } catch(Throwable ex) {
                            }
                            Map<Formula, Node> map = p.highLevelCore();
                            LinkedHashSet<Formula> hCore =
                                new LinkedHashSet<Formula>(map.keySet());
                            buf.append("\n---PROOF---\nminimal unsatisfiable core:\n");
                            buf.append(hCore);
                            buf.append("\n");
                        }
                    } catch(Throwable ex) {
                    }
                }
                buf.append("\n---STATS---\n");
                buf.append(solution.stats());

                int pos = buf.lastIndexOf("translation time:");
                if (pos == -1)
                    pos = buf.length();
                buf.insert(pos, "parsing time: " + parsingTime + " ms\n");

                pos = buf.lastIndexOf("ints: []\n");
                if (pos != -1)
                    buf.delete(pos, pos + 9);
                buf.append("\n");

                if (exit)
                    break;
            }

            context.output(buf.toString());

            synchronized (bigBuf) {
                bigBuf.append(buf);
            }

            if (exit) {
                if (delay > 0) {
                    try {
                        Thread.currentThread().sleep(delay);
                    } catch (InterruptedException except) {
                    }
                }
                context.exit(0);
            }
        } catch (UnboundLeafException except) {
            huh(token, "formula contains unbounded leaf expression: "
                        + except.leaf());
        } catch (HigherOrderDeclException except) {
            huh(token, "formula contains unskolemizable higher-order declaration: "
                       + except.decl());
        } catch (NoClassDefFoundError except) {
            missingSatSolver = true;
        } catch (UnsatisfiedLinkError except) {
            missingSatSolver = true;
        } catch (IndexOutOfBoundsException except) {
            huh(token, "out-of-bounds exception");
        } catch (ArithmeticException except) {
            huh(token, "arithmetic exception");
        } catch (OutOfMemoryError except) {
            huh(token, "out of memory");
        }

        if (missingSatSolver) {
            if (options.solver() == SATFactory.DefaultSAT4J) {
                huh(Token.INVALID_TOKEN, "cannot launch SAT solver");
            } else {
                context.error("cannot launch SAT solver, "
                                   + "falling back on \"DefaultSAT4J\"");
                options.setSolver(SATFactory.DefaultSAT4J);
                solve(token, problemNo, parsingTime, options,
                      timeout, delay, universe, bounds, formula);
            }
        }
    }

    public class SingletonArrayList<T> extends ArrayList<T> {
        public SingletonArrayList(T elem) {
            add(elem);
        }
    }



    // $ANTLR start "problems"
    // src/Kodkodi.g:835:1: problems returns [String str] : ( problem )* ;
    public final String problems() throws RecognitionException {
        String str = null;

        try {
            // src/Kodkodi.g:835:30: ( ( problem )* )
            // src/Kodkodi.g:836:9: ( problem )*
            {
            // src/Kodkodi.g:836:9: ( problem )*
            loop1:
            do {
                int alt1=2;
                int LA1_0 = input.LA(1);

                if ( (LA1_0==SOLVER||LA1_0==SYMMETRY_BREAKING||(LA1_0>=SHARING && LA1_0<=FLATTEN)||(LA1_0>=TIMEOUT && LA1_0<=UNIV)) ) {
                    alt1=1;
                }


                switch (alt1) {
            	case 1 :
            	    // src/Kodkodi.g:836:9: problem
            	    {
            	    pushFollow(FOLLOW_problem_in_problems60);
            	    problem();

            	    state._fsp--;


            	    }
            	    break;

            	default :
            	    break loop1;
                }
            } while (true);


                        try {
                            executor.shutdown();
                            executor.awaitTermination(31622400L, TimeUnit.SECONDS);
                        } catch (InterruptedException except) {
                            context.exit(130);
                        }

                        String res = bigBuf.toString();
                        bigBuf = new StringBuilder();
                        return res;
                    

            }

        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return str;
    }
    // $ANTLR end "problems"


    // $ANTLR start "problem"
    // src/Kodkodi.g:848:1: problem : ( option )* univ_spec ( tuple_reg_directive )* ( bound_spec )* ( int_bound_spec )? ( expr_reg_directive )* solve_directive ;
    public final void problem() throws RecognitionException {
        try {
            // src/Kodkodi.g:848:8: ( ( option )* univ_spec ( tuple_reg_directive )* ( bound_spec )* ( int_bound_spec )? ( expr_reg_directive )* solve_directive )
            // src/Kodkodi.g:849:9: ( option )* univ_spec ( tuple_reg_directive )* ( bound_spec )* ( int_bound_spec )? ( expr_reg_directive )* solve_directive
            {
             reset(); 
            // src/Kodkodi.g:850:9: ( option )*
            loop2:
            do {
                int alt2=2;
                int LA2_0 = input.LA(1);

                if ( (LA2_0==SOLVER||LA2_0==SYMMETRY_BREAKING||(LA2_0>=SHARING && LA2_0<=FLATTEN)||(LA2_0>=TIMEOUT && LA2_0<=DELAY)) ) {
                    alt2=1;
                }


                switch (alt2) {
            	case 1 :
            	    // src/Kodkodi.g:850:9: option
            	    {
            	    pushFollow(FOLLOW_option_in_problem87);
            	    option();

            	    state._fsp--;


            	    }
            	    break;

            	default :
            	    break loop2;
                }
            } while (true);

            pushFollow(FOLLOW_univ_spec_in_problem98);
            univ_spec();

            state._fsp--;

            // src/Kodkodi.g:852:9: ( tuple_reg_directive )*
            loop3:
            do {
                int alt3=2;
                int LA3_0 = input.LA(1);

                if ( (LA3_0==TUPLE_SET_REG||LA3_0==TUPLE_REG) ) {
                    alt3=1;
                }


                switch (alt3) {
            	case 1 :
            	    // src/Kodkodi.g:852:9: tuple_reg_directive
            	    {
            	    pushFollow(FOLLOW_tuple_reg_directive_in_problem108);
            	    tuple_reg_directive();

            	    state._fsp--;


            	    }
            	    break;

            	default :
            	    break loop3;
                }
            } while (true);

            // src/Kodkodi.g:853:9: ( bound_spec )*
            loop4:
            do {
                int alt4=2;
                int LA4_0 = input.LA(1);

                if ( (LA4_0==BOUNDS) ) {
                    alt4=1;
                }


                switch (alt4) {
            	case 1 :
            	    // src/Kodkodi.g:853:9: bound_spec
            	    {
            	    pushFollow(FOLLOW_bound_spec_in_problem119);
            	    bound_spec();

            	    state._fsp--;


            	    }
            	    break;

            	default :
            	    break loop4;
                }
            } while (true);

            // src/Kodkodi.g:854:9: ( int_bound_spec )?
            int alt5=2;
            int LA5_0 = input.LA(1);

            if ( (LA5_0==INT_BOUNDS) ) {
                alt5=1;
            }
            switch (alt5) {
                case 1 :
                    // src/Kodkodi.g:854:9: int_bound_spec
                    {
                    pushFollow(FOLLOW_int_bound_spec_in_problem130);
                    int_bound_spec();

                    state._fsp--;


                    }
                    break;

            }

            // src/Kodkodi.g:855:9: ( expr_reg_directive )*
            loop6:
            do {
                int alt6=2;
                int LA6_0 = input.LA(1);

                if ( ((LA6_0>=FORMULA_REG && LA6_0<=INT_EXPR_REG)) ) {
                    alt6=1;
                }


                switch (alt6) {
            	case 1 :
            	    // src/Kodkodi.g:855:9: expr_reg_directive
            	    {
            	    pushFollow(FOLLOW_expr_reg_directive_in_problem141);
            	    expr_reg_directive();

            	    state._fsp--;


            	    }
            	    break;

            	default :
            	    break loop6;
                }
            } while (true);

            pushFollow(FOLLOW_solve_directive_in_problem152);
            solve_directive();

            state._fsp--;


            }

        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return ;
    }
    // $ANTLR end "problem"


    // $ANTLR start "option"
    // src/Kodkodi.g:857:1: option returns [Vector<String> strs = new Vector<String>()] : ( SOLVER COLON s1= STR_LITERAL ( COMMA s2= STR_LITERAL )* | SYMMETRY_BREAKING COLON k= NUM | SHARING COLON k= NUM | BIT_WIDTH COLON k= NUM | SKOLEM_DEPTH COLON k= NUM | FLATTEN COLON t= ( TRUE | FALSE ) | TIMEOUT COLON k= NUM | DELAY COLON k= NUM );
    public final Vector<String> option() throws RecognitionException {
        Vector<String> strs =  new Vector<String>();

        Token s1=null;
        Token s2=null;
        Token k=null;
        Token t=null;

        try {
            // src/Kodkodi.g:857:60: ( SOLVER COLON s1= STR_LITERAL ( COMMA s2= STR_LITERAL )* | SYMMETRY_BREAKING COLON k= NUM | SHARING COLON k= NUM | BIT_WIDTH COLON k= NUM | SKOLEM_DEPTH COLON k= NUM | FLATTEN COLON t= ( TRUE | FALSE ) | TIMEOUT COLON k= NUM | DELAY COLON k= NUM )
            int alt8=8;
            switch ( input.LA(1) ) {
            case SOLVER:
                {
                alt8=1;
                }
                break;
            case SYMMETRY_BREAKING:
                {
                alt8=2;
                }
                break;
            case SHARING:
                {
                alt8=3;
                }
                break;
            case BIT_WIDTH:
                {
                alt8=4;
                }
                break;
            case SKOLEM_DEPTH:
                {
                alt8=5;
                }
                break;
            case FLATTEN:
                {
                alt8=6;
                }
                break;
            case TIMEOUT:
                {
                alt8=7;
                }
                break;
            case DELAY:
                {
                alt8=8;
                }
                break;
            default:
                NoViableAltException nvae =
                    new NoViableAltException("", 8, 0, input);

                throw nvae;
            }

            switch (alt8) {
                case 1 :
                    // src/Kodkodi.g:858:9: SOLVER COLON s1= STR_LITERAL ( COMMA s2= STR_LITERAL )*
                    {
                    match(input,SOLVER,FOLLOW_SOLVER_in_option170); 
                    match(input,COLON,FOLLOW_COLON_in_option172); 
                    s1=(Token)match(input,STR_LITERAL,FOLLOW_STR_LITERAL_in_option178); 
                     strs.add(sstr(s1)); 
                    // src/Kodkodi.g:859:9: ( COMMA s2= STR_LITERAL )*
                    loop7:
                    do {
                        int alt7=2;
                        int LA7_0 = input.LA(1);

                        if ( (LA7_0==COMMA) ) {
                            alt7=1;
                        }


                        switch (alt7) {
                    	case 1 :
                    	    // src/Kodkodi.g:859:10: COMMA s2= STR_LITERAL
                    	    {
                    	    match(input,COMMA,FOLLOW_COMMA_in_option191); 
                    	    s2=(Token)match(input,STR_LITERAL,FOLLOW_STR_LITERAL_in_option197); 
                    	     strs.add(sstr(s2)); 

                    	    }
                    	    break;

                    	default :
                    	    break loop7;
                        }
                    } while (true);


                                ensure();
                                int expected = 1;
                                SATFactory solver = null;
                                if (strs.elementAt(0).equals("CryptoMiniSat")) {
                                    solver = SATFactory.CryptoMiniSat;
                                } else if (strs.elementAt(0).equals("DefaultSAT4J")) {
                                    solver = SATFactory.DefaultSAT4J;
                                } else if (strs.elementAt(0).equals("LightSAT4J")) {
                                    solver = SATFactory.LightSAT4J;
                                } else if (strs.elementAt(0).equals("Lingeling")) {
                                    solver = SATFactory.Lingeling;
                                } else if (strs.elementAt(0).equals("MiniSat")) {
                                    solver = SATFactory.MiniSat;
                                } else if (strs.elementAt(0).equals("MiniSatProver")) {
                                    solver = SATFactory.MiniSatProver;
                                } else if (strs.elementAt(0).equals("ZChaffMincost")) {
                                    solver = SATFactory.ZChaffMincost;
                                } else if (strs.elementAt(0).equals("SAT4J")) {
                                    expected = 2;
                                    if (strs.size() >= 2)
                                        solver = SATFactory.sat4jFactory(strs.elementAt(1));
                                } else if (strs.elementAt(0).equals("External")) {
                                    expected = 3;
                                    if (strs.size() >= expected) {
                                        String[] args = new String[strs.size() - expected];
                                        for (int i = expected; i < strs.size(); ++i)
                                            args[i - expected] = strs.elementAt(i);
                                        solver = SATFactory.externalFactory(strs.elementAt(1),
                                                                            strs.elementAt(2),
                                                                            args);
                                    }
                                } else if (strs.elementAt(0).equals("ExternalV2")) {
                                    expected = 7;
                                    if (strs.size() >= expected) {
                                        String[] args = new String[strs.size() - expected];
                                        for (int i = expected; i < strs.size(); ++i)
                                            args[i - expected] = strs.elementAt(i);
                                        solver = ExternalSolverV2.satFactory(strs.elementAt(1),
                                                                             strs.elementAt(2),
                                                                             strs.elementAt(3),
                                                                             strs.elementAt(4),
                                                                             strs.elementAt(5),
                                                                             strs.elementAt(6),
                                                                             args);
                                    }
                                }

                                if (solver != null) {
                                    options.setSolver(solver);
                                } else {
                                    huh(s1, "unknown SAT solver '" + strs + "'");
                                }

                                if (strs.size() < expected
                                        || (expected <= 2 && strs.size() > expected)) {
                                    huh(s1, "expected " + expected + " strings, got "
                                             + strs.size());
                                }
                            

                    }
                    break;
                case 2 :
                    // src/Kodkodi.g:919:9: SYMMETRY_BREAKING COLON k= NUM
                    {
                    match(input,SYMMETRY_BREAKING,FOLLOW_SYMMETRY_BREAKING_in_option215); 
                    match(input,COLON,FOLLOW_COLON_in_option217); 
                    k=(Token)match(input,NUM,FOLLOW_NUM_in_option223); 

                                try {
                                    options.setSymmetryBreaking(getInt(k));
                                } catch (IllegalArgumentException except) {
                                    huh(k, "symmetry breaking value " + (k!=null?k.getText():null)
                                            + " out of range");
                                }
                            

                    }
                    break;
                case 3 :
                    // src/Kodkodi.g:927:9: SHARING COLON k= NUM
                    {
                    match(input,SHARING,FOLLOW_SHARING_in_option237); 
                    match(input,COLON,FOLLOW_COLON_in_option239); 
                    k=(Token)match(input,NUM,FOLLOW_NUM_in_option245); 

                                try {
                                    options.setSharing(getInt(k));
                                } catch (IllegalArgumentException except) {
                                    huh(k, "sharing value " + (k!=null?k.getText():null) + " out of range");
                                }
                            

                    }
                    break;
                case 4 :
                    // src/Kodkodi.g:934:9: BIT_WIDTH COLON k= NUM
                    {
                    match(input,BIT_WIDTH,FOLLOW_BIT_WIDTH_in_option259); 
                    match(input,COLON,FOLLOW_COLON_in_option261); 
                    k=(Token)match(input,NUM,FOLLOW_NUM_in_option267); 

                                try {
                                    options.setBitwidth(getInt(k));
                                } catch (IllegalArgumentException except) {
                                    huh(k, "bit width value " + (k!=null?k.getText():null) + " out of range");
                                }
                            

                    }
                    break;
                case 5 :
                    // src/Kodkodi.g:941:9: SKOLEM_DEPTH COLON k= NUM
                    {
                    match(input,SKOLEM_DEPTH,FOLLOW_SKOLEM_DEPTH_in_option281); 
                    match(input,COLON,FOLLOW_COLON_in_option283); 
                    k=(Token)match(input,NUM,FOLLOW_NUM_in_option289); 

                                try {
                                    options.setSkolemDepth(getInt(k));
                                } catch (IllegalArgumentException except) {
                                    huh(k, "Skolem depth value " + (k!=null?k.getText():null) + " out of range");
                                }
                            

                    }
                    break;
                case 6 :
                    // src/Kodkodi.g:948:9: FLATTEN COLON t= ( TRUE | FALSE )
                    {
                    match(input,FLATTEN,FOLLOW_FLATTEN_in_option303); 
                    match(input,COLON,FOLLOW_COLON_in_option305); 
                    t=(Token)input.LT(1);
                    if ( (input.LA(1)>=TRUE && input.LA(1)<=FALSE) ) {
                        input.consume();
                        state.errorRecovery=false;
                    }
                    else {
                        MismatchedSetException mse = new MismatchedSetException(null,input);
                        throw mse;
                    }


                                options.setFlatten((t!=null?t.getType():0) == TRUE);
                            

                    }
                    break;
                case 7 :
                    // src/Kodkodi.g:951:9: TIMEOUT COLON k= NUM
                    {
                    match(input,TIMEOUT,FOLLOW_TIMEOUT_in_option331); 
                    match(input,COLON,FOLLOW_COLON_in_option333); 
                    k=(Token)match(input,NUM,FOLLOW_NUM_in_option339); 

                                try {
                                    timeout = getInt(k);
                                } catch (IllegalArgumentException except) {
                                    huh(k, "timeout value " + (k!=null?k.getText():null) + " out of range");
                                }
                            

                    }
                    break;
                case 8 :
                    // src/Kodkodi.g:958:9: DELAY COLON k= NUM
                    {
                    match(input,DELAY,FOLLOW_DELAY_in_option353); 
                    match(input,COLON,FOLLOW_COLON_in_option355); 
                    k=(Token)match(input,NUM,FOLLOW_NUM_in_option361); 

                                try {
                                    delay = getInt(k);
                                } catch (IllegalArgumentException except) {
                                    huh(k, "delay value " + (k!=null?k.getText():null) + " out of range");
                                }
                            

                    }
                    break;

            }
        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return strs;
    }
    // $ANTLR end "option"


    // $ANTLR start "univ_spec"
    // src/Kodkodi.g:965:1: univ_spec : UNIV COLON n= UNIV_NAME ;
    public final void univ_spec() throws RecognitionException {
        Token n=null;

        try {
            // src/Kodkodi.g:965:10: ( UNIV COLON n= UNIV_NAME )
            // src/Kodkodi.g:966:9: UNIV COLON n= UNIV_NAME
            {
            match(input,UNIV,FOLLOW_UNIV_in_univ_spec377); 
            match(input,COLON,FOLLOW_COLON_in_univ_spec379); 
            n=(Token)match(input,UNIV_NAME,FOLLOW_UNIV_NAME_in_univ_spec385); 

                        ensure();
                        int cardinality = id((n!=null?n.getText():null));
                        if (cardinality == 0) {
                            huh(n, "invalid universe '" + (n!=null?n.getText():null) + "'");
                            cardinality = 1;
                        }
                        final List<String> atoms = new ArrayList<String>(cardinality);
                        for (int i = 0; i < cardinality; ++i)
                            atoms.add(new String("A" + i));
                        universe = new Universe(atoms);
                        factory = universe.factory();
                        bounds = new Bounds(universe);
                    

            }

        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return ;
    }
    // $ANTLR end "univ_spec"


    // $ANTLR start "tuple_reg_directive"
    // src/Kodkodi.g:980:1: tuple_reg_directive : (r= TUPLE_SET_REG c= COLON_EQ s= tuple_set[arity($r.text)] | r= TUPLE_REG c= COLON_EQ t= tuple[arity($r.text)] );
    public final void tuple_reg_directive() throws RecognitionException {
        Token r=null;
        Token c=null;
        TupleSet s = null;

        KodkodiParser.tuple_return t = null;


        try {
            // src/Kodkodi.g:980:20: (r= TUPLE_SET_REG c= COLON_EQ s= tuple_set[arity($r.text)] | r= TUPLE_REG c= COLON_EQ t= tuple[arity($r.text)] )
            int alt9=2;
            int LA9_0 = input.LA(1);

            if ( (LA9_0==TUPLE_SET_REG) ) {
                alt9=1;
            }
            else if ( (LA9_0==TUPLE_REG) ) {
                alt9=2;
            }
            else {
                NoViableAltException nvae =
                    new NoViableAltException("", 9, 0, input);

                throw nvae;
            }
            switch (alt9) {
                case 1 :
                    // src/Kodkodi.g:981:9: r= TUPLE_SET_REG c= COLON_EQ s= tuple_set[arity($r.text)]
                    {
                    r=(Token)match(input,TUPLE_SET_REG,FOLLOW_TUPLE_SET_REG_in_tuple_reg_directive405); 
                    c=(Token)match(input,COLON_EQ,FOLLOW_COLON_EQ_in_tuple_reg_directive411); 
                    pushFollow(FOLLOW_tuple_set_in_tuple_reg_directive417);
                    s=tuple_set(arity((r!=null?r.getText():null)));

                    state._fsp--;


                                if (s.arity() == arity((r!=null?r.getText():null))) {
                                    setTupleSet(r, s);
                                } else {
                                    huh(c, "expected " + arity((r!=null?r.getText():null)) + "-tuple set, got "
                                             + s.arity() + "-tuple set");
                                }
                            

                    }
                    break;
                case 2 :
                    // src/Kodkodi.g:989:9: r= TUPLE_REG c= COLON_EQ t= tuple[arity($r.text)]
                    {
                    r=(Token)match(input,TUPLE_REG,FOLLOW_TUPLE_REG_in_tuple_reg_directive436); 
                    c=(Token)match(input,COLON_EQ,FOLLOW_COLON_EQ_in_tuple_reg_directive442); 
                    pushFollow(FOLLOW_tuple_in_tuple_reg_directive448);
                    t=tuple(arity((r!=null?r.getText():null)));

                    state._fsp--;


                                if ((t!=null?t.value:null).arity() == arity((r!=null?r.getText():null))) {
                                    setTuple(r, (t!=null?t.value:null));
                                } else {
                                    huh(c, "expected " + arity((r!=null?r.getText():null)) + "-tuple, got "
                                            + (t!=null?t.value:null).arity() + "-tuple");
                                }
                            

                    }
                    break;

            }
        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return ;
    }
    // $ANTLR end "tuple_reg_directive"


    // $ANTLR start "bound_spec"
    // src/Kodkodi.g:997:1: bound_spec returns [List<Relation> relations = new ArrayList<Relation>()] : BOUNDS n1= RELATION_NAME ( COMMA n2= RELATION_NAME )* c= COLON (s1= tuple_set[$relations.get(0).arity()] | BRACKET_LEFT s1= tuple_set[$relations.get(0).arity()] COMMA s2= tuple_set[$relations.get(0).arity()] BRACKET_RIGHT ) ;
    public final List<Relation> bound_spec() throws RecognitionException {
        List<Relation> relations =  new ArrayList<Relation>();

        Token n1=null;
        Token n2=null;
        Token c=null;
        TupleSet s1 = null;

        TupleSet s2 = null;


        try {
            // src/Kodkodi.g:997:74: ( BOUNDS n1= RELATION_NAME ( COMMA n2= RELATION_NAME )* c= COLON (s1= tuple_set[$relations.get(0).arity()] | BRACKET_LEFT s1= tuple_set[$relations.get(0).arity()] COMMA s2= tuple_set[$relations.get(0).arity()] BRACKET_RIGHT ) )
            // src/Kodkodi.g:998:9: BOUNDS n1= RELATION_NAME ( COMMA n2= RELATION_NAME )* c= COLON (s1= tuple_set[$relations.get(0).arity()] | BRACKET_LEFT s1= tuple_set[$relations.get(0).arity()] COMMA s2= tuple_set[$relations.get(0).arity()] BRACKET_RIGHT )
            {
            match(input,BOUNDS,FOLLOW_BOUNDS_in_bound_spec469); 
            n1=(Token)match(input,RELATION_NAME,FOLLOW_RELATION_NAME_in_bound_spec475); 
             relations.add(getRelation(n1)); 
            // src/Kodkodi.g:999:9: ( COMMA n2= RELATION_NAME )*
            loop10:
            do {
                int alt10=2;
                int LA10_0 = input.LA(1);

                if ( (LA10_0==COMMA) ) {
                    alt10=1;
                }


                switch (alt10) {
            	case 1 :
            	    // src/Kodkodi.g:999:10: COMMA n2= RELATION_NAME
            	    {
            	    match(input,COMMA,FOLLOW_COMMA_in_bound_spec488); 
            	    n2=(Token)match(input,RELATION_NAME,FOLLOW_RELATION_NAME_in_bound_spec494); 
            	     relations.add(getRelation(n2)); 

            	    }
            	    break;

            	default :
            	    break loop10;
                }
            } while (true);

            c=(Token)match(input,COLON,FOLLOW_COLON_in_bound_spec512); 
            // src/Kodkodi.g:1001:9: (s1= tuple_set[$relations.get(0).arity()] | BRACKET_LEFT s1= tuple_set[$relations.get(0).arity()] COMMA s2= tuple_set[$relations.get(0).arity()] BRACKET_RIGHT )
            int alt11=2;
            int LA11_0 = input.LA(1);

            if ( ((LA11_0>=UNIV_NAME && LA11_0<=TUPLE_SET_REG)||(LA11_0>=OFFSET_UNIV_NAME && LA11_0<=PAREN_LEFT)||LA11_0==BRACE_LEFT||(LA11_0>=NONE && LA11_0<=ALL)) ) {
                alt11=1;
            }
            else if ( (LA11_0==BRACKET_LEFT) ) {
                alt11=2;
            }
            else {
                NoViableAltException nvae =
                    new NoViableAltException("", 11, 0, input);

                throw nvae;
            }
            switch (alt11) {
                case 1 :
                    // src/Kodkodi.g:1001:10: s1= tuple_set[$relations.get(0).arity()]
                    {
                    pushFollow(FOLLOW_tuple_set_in_bound_spec527);
                    s1=tuple_set(relations.get(0).arity());

                    state._fsp--;


                    }
                    break;
                case 2 :
                    // src/Kodkodi.g:1002:10: BRACKET_LEFT s1= tuple_set[$relations.get(0).arity()] COMMA s2= tuple_set[$relations.get(0).arity()] BRACKET_RIGHT
                    {
                    match(input,BRACKET_LEFT,FOLLOW_BRACKET_LEFT_in_bound_spec541); 
                    pushFollow(FOLLOW_tuple_set_in_bound_spec547);
                    s1=tuple_set(relations.get(0).arity());

                    state._fsp--;

                    match(input,COMMA,FOLLOW_COMMA_in_bound_spec559); 
                    pushFollow(FOLLOW_tuple_set_in_bound_spec565);
                    s2=tuple_set(relations.get(0).arity());

                    state._fsp--;

                    match(input,BRACKET_RIGHT,FOLLOW_BRACKET_RIGHT_in_bound_spec568); 

                    }
                    break;

            }


                        try {
                            if (s2 == null) {
                                for (Relation relation : relations) {
                                    if (bounds.lowerBound(relation) == null) {
                                        bounds.boundExactly(relation, s1);
                                    } else {
                                        huh(c, "relation already bound");
                                    }
                                }
                            } else {
                                for (Relation relation : relations) {
                                    if (bounds.lowerBound(relation) == null) {
                                        bounds.bound(relation, s1, s2);
                                    } else {
                                        huh(c, "relation already bound");
                                    }
                                }
                            }
                        } catch (IllegalArgumentException except) {
                            huh(c, "invalid bounds: " + fixedMessage(except));
                        }
                    

            }

        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return relations;
    }
    // $ANTLR end "bound_spec"


    // $ANTLR start "int_bound_spec"
    // src/Kodkodi.g:1026:1: int_bound_spec : INT_BOUNDS COLON q= int_bound_seq ( COMMA q= int_bound_seq )* ;
    public final void int_bound_spec() throws RecognitionException {
        try {
            // src/Kodkodi.g:1026:15: ( INT_BOUNDS COLON q= int_bound_seq ( COMMA q= int_bound_seq )* )
            // src/Kodkodi.g:1027:9: INT_BOUNDS COLON q= int_bound_seq ( COMMA q= int_bound_seq )*
            {
            match(input,INT_BOUNDS,FOLLOW_INT_BOUNDS_in_int_bound_spec585); 
            match(input,COLON,FOLLOW_COLON_in_int_bound_spec587); 
            pushFollow(FOLLOW_int_bound_seq_in_int_bound_spec593);
            int_bound_seq();

            state._fsp--;

            // src/Kodkodi.g:1027:44: ( COMMA q= int_bound_seq )*
            loop12:
            do {
                int alt12=2;
                int LA12_0 = input.LA(1);

                if ( (LA12_0==COMMA) ) {
                    alt12=1;
                }


                switch (alt12) {
            	case 1 :
            	    // src/Kodkodi.g:1027:45: COMMA q= int_bound_seq
            	    {
            	    match(input,COMMA,FOLLOW_COMMA_in_int_bound_spec596); 
            	    pushFollow(FOLLOW_int_bound_seq_in_int_bound_spec602);
            	    int_bound_seq();

            	    state._fsp--;


            	    }
            	    break;

            	default :
            	    break loop12;
                }
            } while (true);


            }

        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return ;
    }
    // $ANTLR end "int_bound_spec"


    // $ANTLR start "expr_reg_directive"
    // src/Kodkodi.g:1028:1: expr_reg_directive : (r= FORMULA_REG COLON_EQ e= expr | r= REL_EXPR_REG COLON_EQ e= expr | r= INT_EXPR_REG COLON_EQ e= expr );
    public final void expr_reg_directive() throws RecognitionException {
        Token r=null;
        Object e = null;


        try {
            // src/Kodkodi.g:1028:19: (r= FORMULA_REG COLON_EQ e= expr | r= REL_EXPR_REG COLON_EQ e= expr | r= INT_EXPR_REG COLON_EQ e= expr )
            int alt13=3;
            switch ( input.LA(1) ) {
            case FORMULA_REG:
                {
                alt13=1;
                }
                break;
            case REL_EXPR_REG:
                {
                alt13=2;
                }
                break;
            case INT_EXPR_REG:
                {
                alt13=3;
                }
                break;
            default:
                NoViableAltException nvae =
                    new NoViableAltException("", 13, 0, input);

                throw nvae;
            }

            switch (alt13) {
                case 1 :
                    // src/Kodkodi.g:1029:9: r= FORMULA_REG COLON_EQ e= expr
                    {
                    r=(Token)match(input,FORMULA_REG,FOLLOW_FORMULA_REG_in_expr_reg_directive622); 
                    match(input,COLON_EQ,FOLLOW_COLON_EQ_in_expr_reg_directive624); 
                    pushFollow(FOLLOW_expr_in_expr_reg_directive630);
                    e=expr();

                    state._fsp--;

                     setFormulaReg(r, F(r, e)); 

                    }
                    break;
                case 2 :
                    // src/Kodkodi.g:1030:9: r= REL_EXPR_REG COLON_EQ e= expr
                    {
                    r=(Token)match(input,REL_EXPR_REG,FOLLOW_REL_EXPR_REG_in_expr_reg_directive648); 
                    match(input,COLON_EQ,FOLLOW_COLON_EQ_in_expr_reg_directive650); 
                    pushFollow(FOLLOW_expr_in_expr_reg_directive656);
                    e=expr();

                    state._fsp--;

                     setExprReg(r, E(r, e)); 

                    }
                    break;
                case 3 :
                    // src/Kodkodi.g:1031:9: r= INT_EXPR_REG COLON_EQ e= expr
                    {
                    r=(Token)match(input,INT_EXPR_REG,FOLLOW_INT_EXPR_REG_in_expr_reg_directive674); 
                    match(input,COLON_EQ,FOLLOW_COLON_EQ_in_expr_reg_directive676); 
                    pushFollow(FOLLOW_expr_in_expr_reg_directive682);
                    e=expr();

                    state._fsp--;

                     setIntExprReg(r, I(r, e)); 

                    }
                    break;

            }
        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return ;
    }
    // $ANTLR end "expr_reg_directive"


    // $ANTLR start "solve_directive"
    // src/Kodkodi.g:1032:1: solve_directive : s= SOLVE e= expr SEMICOLON ;
    public final void solve_directive() throws RecognitionException {
        Token s=null;
        Object e = null;


        try {
            // src/Kodkodi.g:1032:16: (s= SOLVE e= expr SEMICOLON )
            // src/Kodkodi.g:1033:9: s= SOLVE e= expr SEMICOLON
            {
            s=(Token)match(input,SOLVE,FOLLOW_SOLVE_in_solve_directive702); 
            pushFollow(FOLLOW_expr_in_solve_directive708);
            e=expr();

            state._fsp--;

            match(input,SEMICOLON,FOLLOW_SEMICOLON_in_solve_directive710); 

                        final long parsingTime = System.currentTimeMillis() - startParsingTime;

                        final Token token = s;
                        final int problemNo = this.problemNo;
                        final Options options = this.options;
                        final Universe universe = this.universe;
                        final Bounds bounds = this.bounds;
                        final Formula formula = F(s, e);

                        Runnable task = new Runnable() {
                            public void run() {
                                try {
                                    solve(token, problemNo, parsingTime, options, timeout,
                                          delay, universe, bounds, formula);
                                } catch (Throwable except) {
                                    String message = except.getMessage();
                                    if (message == null || message.length() == 0)
                                        message = except.toString();
                                    try {
                                        if (Thread.interrupted()) throw new InterruptedException();
                                        context.error("Solve error: " + message);
                                    } catch (InterruptedException exn) {}
                                }
                            }
                        };
                        if (timeout > 0) {
                            Future future = executor.submit(task);
                            try {
                                future.get(timeout, TimeUnit.MILLISECONDS);
                            } catch (ExecutionException e2) {
                                context.error("Error: Java execution error");
                                future.cancel(true);
                            } catch (InterruptedException e2) {
                                context.error("Error: Java interruption error");
                                future.cancel(true);
                            } catch (TimeoutException e2) {
                                context.error("Error: problem timed out");
                                future.cancel(true);
                            }
                        } else {
                            executor.execute(task);
                        }
                    

            }

        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return ;
    }
    // $ANTLR end "solve_directive"


    // $ANTLR start "int_bound_seq"
    // src/Kodkodi.g:1078:1: int_bound_seq : (k= NUM COLON )? q= tuple_set_seq[1] ;
    public final void int_bound_seq() throws RecognitionException {
        Token k=null;
        KodkodiParser.tuple_set_seq_return q = null;


        try {
            // src/Kodkodi.g:1078:14: ( (k= NUM COLON )? q= tuple_set_seq[1] )
            // src/Kodkodi.g:1079:9: (k= NUM COLON )? q= tuple_set_seq[1]
            {
            // src/Kodkodi.g:1079:9: (k= NUM COLON )?
            int alt14=2;
            int LA14_0 = input.LA(1);

            if ( (LA14_0==NUM) ) {
                alt14=1;
            }
            switch (alt14) {
                case 1 :
                    // src/Kodkodi.g:1079:10: k= NUM COLON
                    {
                    k=(Token)match(input,NUM,FOLLOW_NUM_in_int_bound_seq732); 
                    match(input,COLON,FOLLOW_COLON_in_int_bound_seq734); 

                    }
                    break;

            }

            pushFollow(FOLLOW_tuple_set_seq_in_int_bound_seq742);
            q=tuple_set_seq(1);

            state._fsp--;


                        if (k != null)
                            nextInt = getInt(k);
                        for (TupleSet tupleSet : (q!=null?q.value:null)) {
                            try {
                                bounds.boundExactly(nextInt, tupleSet);
                            } catch (IllegalArgumentException except) {
                                huh((q!=null?q.token:null), "invalid bounds: " + fixedMessage(except));
                            }
                            ++nextInt;
                        }
                    

            }

        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return ;
    }
    // $ANTLR end "int_bound_seq"

    public static class tuple_set_seq_return extends ParserRuleReturnScope {
        public Token token;
        public List<TupleSet> value = new ArrayList<TupleSet>();
    };

    // $ANTLR start "tuple_set_seq"
    // src/Kodkodi.g:1091:1: tuple_set_seq[int arity] returns [Token token, List<TupleSet> value = new ArrayList<TupleSet>()] : b= BRACKET_LEFT (s1= tuple_set[$arity] ( COMMA s2= tuple_set[$arity] )* )? BRACKET_RIGHT ;
    public final KodkodiParser.tuple_set_seq_return tuple_set_seq(int arity) throws RecognitionException {
        KodkodiParser.tuple_set_seq_return retval = new KodkodiParser.tuple_set_seq_return();
        retval.start = input.LT(1);

        Token b=null;
        TupleSet s1 = null;

        TupleSet s2 = null;


        try {
            // src/Kodkodi.g:1092:77: (b= BRACKET_LEFT (s1= tuple_set[$arity] ( COMMA s2= tuple_set[$arity] )* )? BRACKET_RIGHT )
            // src/Kodkodi.g:1093:9: b= BRACKET_LEFT (s1= tuple_set[$arity] ( COMMA s2= tuple_set[$arity] )* )? BRACKET_RIGHT
            {
            b=(Token)match(input,BRACKET_LEFT,FOLLOW_BRACKET_LEFT_in_tuple_set_seq773); 
            // src/Kodkodi.g:1093:26: (s1= tuple_set[$arity] ( COMMA s2= tuple_set[$arity] )* )?
            int alt16=2;
            int LA16_0 = input.LA(1);

            if ( ((LA16_0>=UNIV_NAME && LA16_0<=TUPLE_SET_REG)||(LA16_0>=OFFSET_UNIV_NAME && LA16_0<=PAREN_LEFT)||LA16_0==BRACE_LEFT||(LA16_0>=NONE && LA16_0<=ALL)) ) {
                alt16=1;
            }
            switch (alt16) {
                case 1 :
                    // src/Kodkodi.g:1093:27: s1= tuple_set[$arity] ( COMMA s2= tuple_set[$arity] )*
                    {
                    pushFollow(FOLLOW_tuple_set_in_tuple_set_seq780);
                    s1=tuple_set(arity);

                    state._fsp--;


                                retval.token = b;
                                retval.value.add(s1);
                            
                    // src/Kodkodi.g:1096:11: ( COMMA s2= tuple_set[$arity] )*
                    loop15:
                    do {
                        int alt15=2;
                        int LA15_0 = input.LA(1);

                        if ( (LA15_0==COMMA) ) {
                            alt15=1;
                        }


                        switch (alt15) {
                    	case 1 :
                    	    // src/Kodkodi.g:1096:12: COMMA s2= tuple_set[$arity]
                    	    {
                    	    match(input,COMMA,FOLLOW_COMMA_in_tuple_set_seq786); 
                    	    pushFollow(FOLLOW_tuple_set_in_tuple_set_seq792);
                    	    s2=tuple_set(arity);

                    	    state._fsp--;

                    	     retval.value.add(s2); 

                    	    }
                    	    break;

                    	default :
                    	    break loop15;
                        }
                    } while (true);


                    }
                    break;

            }

            match(input,BRACKET_RIGHT,FOLLOW_BRACKET_RIGHT_in_tuple_set_seq808); 

            }

            retval.stop = input.LT(-1);

        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return retval;
    }
    // $ANTLR end "tuple_set_seq"


    // $ANTLR start "tuple_set"
    // src/Kodkodi.g:1099:1: tuple_set[int arity] returns [TupleSet value] : s1= intersect_tuple_set[$arity] (t= ( PLUS | MINUS ) s2= intersect_tuple_set[$arity] )* ;
    public final TupleSet tuple_set(int arity) throws RecognitionException {
        TupleSet value = null;

        Token t=null;
        TupleSet s1 = null;

        TupleSet s2 = null;


        try {
            // src/Kodkodi.g:1099:46: (s1= intersect_tuple_set[$arity] (t= ( PLUS | MINUS ) s2= intersect_tuple_set[$arity] )* )
            // src/Kodkodi.g:1100:9: s1= intersect_tuple_set[$arity] (t= ( PLUS | MINUS ) s2= intersect_tuple_set[$arity] )*
            {
            pushFollow(FOLLOW_intersect_tuple_set_in_tuple_set832);
            s1=intersect_tuple_set(arity);

            state._fsp--;

             value = s1; 
            // src/Kodkodi.g:1101:9: (t= ( PLUS | MINUS ) s2= intersect_tuple_set[$arity] )*
            loop17:
            do {
                int alt17=2;
                int LA17_0 = input.LA(1);

                if ( ((LA17_0>=PLUS && LA17_0<=MINUS)) ) {
                    alt17=1;
                }


                switch (alt17) {
            	case 1 :
            	    // src/Kodkodi.g:1101:10: t= ( PLUS | MINUS ) s2= intersect_tuple_set[$arity]
            	    {
            	    t=(Token)input.LT(1);
            	    if ( (input.LA(1)>=PLUS && input.LA(1)<=MINUS) ) {
            	        input.consume();
            	        state.errorRecovery=false;
            	    }
            	    else {
            	        MismatchedSetException mse = new MismatchedSetException(null,input);
            	        throw mse;
            	    }

            	    pushFollow(FOLLOW_intersect_tuple_set_in_tuple_set862);
            	    s2=intersect_tuple_set(arity);

            	    state._fsp--;


            	                 try {
            	                     if ((t!=null?t.getType():0) == PLUS) {
            	                         value.addAll(s2);
            	                     } else {
            	                         value.removeAll(s2);
            	                     }
            	                 } catch (IllegalArgumentException except) {
            	                      huh(t, "illegal tuple set: " + fixedMessage(except));
            	                 }
            	             

            	    }
            	    break;

            	default :
            	    break loop17;
                }
            } while (true);


            }

        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return value;
    }
    // $ANTLR end "tuple_set"


    // $ANTLR start "intersect_tuple_set"
    // src/Kodkodi.g:1112:1: intersect_tuple_set[int arity] returns [TupleSet value] : s1= product_tuple_set[$arity] (a= AND s2= product_tuple_set[$arity] )* ;
    public final TupleSet intersect_tuple_set(int arity) throws RecognitionException {
        TupleSet value = null;

        Token a=null;
        TupleSet s1 = null;

        TupleSet s2 = null;


        try {
            // src/Kodkodi.g:1112:56: (s1= product_tuple_set[$arity] (a= AND s2= product_tuple_set[$arity] )* )
            // src/Kodkodi.g:1113:9: s1= product_tuple_set[$arity] (a= AND s2= product_tuple_set[$arity] )*
            {
            pushFollow(FOLLOW_product_tuple_set_in_intersect_tuple_set890);
            s1=product_tuple_set(arity);

            state._fsp--;

             value = s1; 
            // src/Kodkodi.g:1114:9: (a= AND s2= product_tuple_set[$arity] )*
            loop18:
            do {
                int alt18=2;
                int LA18_0 = input.LA(1);

                if ( (LA18_0==AND) ) {
                    alt18=1;
                }


                switch (alt18) {
            	case 1 :
            	    // src/Kodkodi.g:1114:10: a= AND s2= product_tuple_set[$arity]
            	    {
            	    a=(Token)match(input,AND,FOLLOW_AND_in_intersect_tuple_set908); 
            	    pushFollow(FOLLOW_product_tuple_set_in_intersect_tuple_set914);
            	    s2=product_tuple_set(arity);

            	    state._fsp--;


            	                 try {
            	                     value.retainAll(s2);
            	                 } catch (IllegalArgumentException except) {
            	                      huh(a, "illegal tuple set: " + fixedMessage(except));
            	                 }
            	             

            	    }
            	    break;

            	default :
            	    break loop18;
                }
            } while (true);


            }

        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return value;
    }
    // $ANTLR end "intersect_tuple_set"


    // $ANTLR start "product_tuple_set"
    // src/Kodkodi.g:1121:1: product_tuple_set[int arity] returns [TupleSet value] : s1= project_tuple_set[$arity] (a= ARROW s2= project_tuple_set[$arity] )* ;
    public final TupleSet product_tuple_set(int arity) throws RecognitionException {
        TupleSet value = null;

        Token a=null;
        TupleSet s1 = null;

        TupleSet s2 = null;


        try {
            // src/Kodkodi.g:1121:54: (s1= project_tuple_set[$arity] (a= ARROW s2= project_tuple_set[$arity] )* )
            // src/Kodkodi.g:1122:9: s1= project_tuple_set[$arity] (a= ARROW s2= project_tuple_set[$arity] )*
            {
            pushFollow(FOLLOW_project_tuple_set_in_product_tuple_set942);
            s1=project_tuple_set(arity);

            state._fsp--;

             value = s1; 
            // src/Kodkodi.g:1123:9: (a= ARROW s2= project_tuple_set[$arity] )*
            loop19:
            do {
                int alt19=2;
                int LA19_0 = input.LA(1);

                if ( (LA19_0==ARROW) ) {
                    alt19=1;
                }


                switch (alt19) {
            	case 1 :
            	    // src/Kodkodi.g:1123:10: a= ARROW s2= project_tuple_set[$arity]
            	    {
            	    a=(Token)match(input,ARROW,FOLLOW_ARROW_in_product_tuple_set960); 
            	    pushFollow(FOLLOW_project_tuple_set_in_product_tuple_set966);
            	    s2=project_tuple_set(arity);

            	    state._fsp--;


            	                 try {
            	                     value = value.product(s2);
            	                 } catch (IllegalArgumentException except) {
            	                      huh(a, "illegal tuple set: " + fixedMessage(except));
            	                 }
            	             

            	    }
            	    break;

            	default :
            	    break loop19;
                }
            } while (true);


            }

        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return value;
    }
    // $ANTLR end "product_tuple_set"


    // $ANTLR start "project_tuple_set"
    // src/Kodkodi.g:1130:1: project_tuple_set[int arity] returns [TupleSet value] : s= basic_tuple_set[$arity] ( BRACKET_LEFT k= NUM BRACKET_RIGHT )* ;
    public final TupleSet project_tuple_set(int arity) throws RecognitionException {
        TupleSet value = null;

        Token k=null;
        KodkodiParser.basic_tuple_set_return s = null;


        try {
            // src/Kodkodi.g:1130:54: (s= basic_tuple_set[$arity] ( BRACKET_LEFT k= NUM BRACKET_RIGHT )* )
            // src/Kodkodi.g:1131:9: s= basic_tuple_set[$arity] ( BRACKET_LEFT k= NUM BRACKET_RIGHT )*
            {
            pushFollow(FOLLOW_basic_tuple_set_in_project_tuple_set994);
            s=basic_tuple_set(arity);

            state._fsp--;

             value = (s!=null?s.value:null); 
            // src/Kodkodi.g:1132:9: ( BRACKET_LEFT k= NUM BRACKET_RIGHT )*
            loop20:
            do {
                int alt20=2;
                int LA20_0 = input.LA(1);

                if ( (LA20_0==BRACKET_LEFT) ) {
                    alt20=1;
                }


                switch (alt20) {
            	case 1 :
            	    // src/Kodkodi.g:1132:10: BRACKET_LEFT k= NUM BRACKET_RIGHT
            	    {
            	    match(input,BRACKET_LEFT,FOLLOW_BRACKET_LEFT_in_project_tuple_set1008); 
            	    k=(Token)match(input,NUM,FOLLOW_NUM_in_project_tuple_set1014); 
            	    match(input,BRACKET_RIGHT,FOLLOW_BRACKET_RIGHT_in_project_tuple_set1016); 

            	                 final int dimension = getInt(k);
            	                 try {
            	                     value = value.project(dimension);
            	                 } catch (IllegalArgumentException except) {
            	                     huh(k, "dimension " + dimension + " out of range");
            	                     value = (s!=null?s.value:null);
            	                 }
            	             

            	    }
            	    break;

            	default :
            	    break loop20;
                }
            } while (true);


            }

        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return value;
    }
    // $ANTLR end "project_tuple_set"

    public static class basic_tuple_set_return extends ParserRuleReturnScope {
        public TupleSet value;
        public List<Tuple> list = new ArrayList<Tuple>();
    };

    // $ANTLR start "basic_tuple_set"
    // src/Kodkodi.g:1141:1: basic_tuple_set[int arity] returns [TupleSet value, List<Tuple> list = new ArrayList<Tuple>()] : (n= ( UNIV_NAME | OFFSET_UNIV_NAME ) | PAREN_LEFT s= tuple_set[$arity] PAREN_RIGHT | b= BRACE_LEFT (t1= tuple[$arity] ( ( COMMA t2= tuple[$arity] )* | dd= DOT_DOT t2= tuple[$arity] | h= HASH t2= tuple[$arity] ) | ) BRACE_RIGHT | n= NONE | a= ALL | r= TUPLE_SET_REG );
    public final KodkodiParser.basic_tuple_set_return basic_tuple_set(int arity) throws RecognitionException {
        KodkodiParser.basic_tuple_set_return retval = new KodkodiParser.basic_tuple_set_return();
        retval.start = input.LT(1);

        Token n=null;
        Token b=null;
        Token dd=null;
        Token h=null;
        Token a=null;
        Token r=null;
        TupleSet s = null;

        KodkodiParser.tuple_return t1 = null;

        KodkodiParser.tuple_return t2 = null;


        try {
            // src/Kodkodi.g:1142:75: (n= ( UNIV_NAME | OFFSET_UNIV_NAME ) | PAREN_LEFT s= tuple_set[$arity] PAREN_RIGHT | b= BRACE_LEFT (t1= tuple[$arity] ( ( COMMA t2= tuple[$arity] )* | dd= DOT_DOT t2= tuple[$arity] | h= HASH t2= tuple[$arity] ) | ) BRACE_RIGHT | n= NONE | a= ALL | r= TUPLE_SET_REG )
            int alt24=6;
            switch ( input.LA(1) ) {
            case UNIV_NAME:
            case OFFSET_UNIV_NAME:
                {
                alt24=1;
                }
                break;
            case PAREN_LEFT:
                {
                alt24=2;
                }
                break;
            case BRACE_LEFT:
                {
                alt24=3;
                }
                break;
            case NONE:
                {
                alt24=4;
                }
                break;
            case ALL:
                {
                alt24=5;
                }
                break;
            case TUPLE_SET_REG:
                {
                alt24=6;
                }
                break;
            default:
                NoViableAltException nvae =
                    new NoViableAltException("", 24, 0, input);

                throw nvae;
            }

            switch (alt24) {
                case 1 :
                    // src/Kodkodi.g:1143:9: n= ( UNIV_NAME | OFFSET_UNIV_NAME )
                    {
                    n=(Token)input.LT(1);
                    if ( input.LA(1)==UNIV_NAME||input.LA(1)==OFFSET_UNIV_NAME ) {
                        input.consume();
                        state.errorRecovery=false;
                    }
                    else {
                        MismatchedSetException mse = new MismatchedSetException(null,input);
                        throw mse;
                    }


                                  retval.value = univTupleSet(n, id((n!=null?n.getText():null)));
                            

                    }
                    break;
                case 2 :
                    // src/Kodkodi.g:1146:9: PAREN_LEFT s= tuple_set[$arity] PAREN_RIGHT
                    {
                    match(input,PAREN_LEFT,FOLLOW_PAREN_LEFT_in_basic_tuple_set1070); 
                    pushFollow(FOLLOW_tuple_set_in_basic_tuple_set1076);
                    s=tuple_set(arity);

                    state._fsp--;

                     retval.value = s; 
                    match(input,PAREN_RIGHT,FOLLOW_PAREN_RIGHT_in_basic_tuple_set1081); 

                    }
                    break;
                case 3 :
                    // src/Kodkodi.g:1147:9: b= BRACE_LEFT (t1= tuple[$arity] ( ( COMMA t2= tuple[$arity] )* | dd= DOT_DOT t2= tuple[$arity] | h= HASH t2= tuple[$arity] ) | ) BRACE_RIGHT
                    {
                    b=(Token)match(input,BRACE_LEFT,FOLLOW_BRACE_LEFT_in_basic_tuple_set1097); 
                    // src/Kodkodi.g:1148:9: (t1= tuple[$arity] ( ( COMMA t2= tuple[$arity] )* | dd= DOT_DOT t2= tuple[$arity] | h= HASH t2= tuple[$arity] ) | )
                    int alt23=2;
                    int LA23_0 = input.LA(1);

                    if ( (LA23_0==TUPLE_REG||LA23_0==BRACKET_LEFT||(LA23_0>=ATOM_NAME && LA23_0<=TUPLE_NAME)) ) {
                        alt23=1;
                    }
                    else if ( (LA23_0==BRACE_RIGHT) ) {
                        alt23=2;
                    }
                    else {
                        NoViableAltException nvae =
                            new NoViableAltException("", 23, 0, input);

                        throw nvae;
                    }
                    switch (alt23) {
                        case 1 :
                            // src/Kodkodi.g:1148:10: t1= tuple[$arity] ( ( COMMA t2= tuple[$arity] )* | dd= DOT_DOT t2= tuple[$arity] | h= HASH t2= tuple[$arity] )
                            {
                            pushFollow(FOLLOW_tuple_in_basic_tuple_set1112);
                            t1=tuple(arity);

                            state._fsp--;

                            // src/Kodkodi.g:1149:10: ( ( COMMA t2= tuple[$arity] )* | dd= DOT_DOT t2= tuple[$arity] | h= HASH t2= tuple[$arity] )
                            int alt22=3;
                            switch ( input.LA(1) ) {
                            case COMMA:
                            case BRACE_RIGHT:
                                {
                                alt22=1;
                                }
                                break;
                            case DOT_DOT:
                                {
                                alt22=2;
                                }
                                break;
                            case HASH:
                                {
                                alt22=3;
                                }
                                break;
                            default:
                                NoViableAltException nvae =
                                    new NoViableAltException("", 22, 0, input);

                                throw nvae;
                            }

                            switch (alt22) {
                                case 1 :
                                    // src/Kodkodi.g:1149:11: ( COMMA t2= tuple[$arity] )*
                                    {

                                                  retval.list.add((t1!=null?t1.value:null));
                                              
                                    // src/Kodkodi.g:1151:13: ( COMMA t2= tuple[$arity] )*
                                    loop21:
                                    do {
                                        int alt21=2;
                                        int LA21_0 = input.LA(1);

                                        if ( (LA21_0==COMMA) ) {
                                            alt21=1;
                                        }


                                        switch (alt21) {
                                    	case 1 :
                                    	    // src/Kodkodi.g:1151:14: COMMA t2= tuple[$arity]
                                    	    {
                                    	    match(input,COMMA,FOLLOW_COMMA_in_basic_tuple_set1128); 
                                    	    pushFollow(FOLLOW_tuple_in_basic_tuple_set1134);
                                    	    t2=tuple(arity);

                                    	    state._fsp--;


                                    	                     retval.list.add((t2!=null?t2.value:null));
                                    	                 

                                    	    }
                                    	    break;

                                    	default :
                                    	    break loop21;
                                        }
                                    } while (true);


                                                  try {
                                                      retval.value = factory.setOf(retval.list);
                                                  } catch (IllegalArgumentException except) {
                                                      huh(b, "illegal tuple set: " + fixedMessage(except));
                                                  }
                                              

                                    }
                                    break;
                                case 2 :
                                    // src/Kodkodi.g:1160:11: dd= DOT_DOT t2= tuple[$arity]
                                    {
                                    dd=(Token)match(input,DOT_DOT,FOLLOW_DOT_DOT_in_basic_tuple_set1159); 
                                    pushFollow(FOLLOW_tuple_in_basic_tuple_set1165);
                                    t2=tuple(arity);

                                    state._fsp--;


                                                  try {
                                                      retval.value = factory.range((t1!=null?t1.value:null), (t2!=null?t2.value:null));
                                                  } catch (IndexOutOfBoundsException except) {
                                                      huh(dd, "invalid range");
                                                      retval.value = factory.setOf((t1!=null?t1.value:null));   
                                                  } catch (IllegalArgumentException except) {
                                                      huh(dd, "invalid range");
                                                      retval.value = factory.setOf((t1!=null?t1.value:null));
                                                  }
                                              

                                    }
                                    break;
                                case 3 :
                                    // src/Kodkodi.g:1171:11: h= HASH t2= tuple[$arity]
                                    {
                                    h=(Token)match(input,HASH,FOLLOW_HASH_in_basic_tuple_set1186); 
                                    pushFollow(FOLLOW_tuple_in_basic_tuple_set1192);
                                    t2=tuple(arity);

                                    state._fsp--;


                                                  try {
                                                      retval.value = factory.area((t1!=null?t1.value:null), (t2!=null?t2.value:null));
                                                  } catch (IndexOutOfBoundsException except) {
                                                      huh(h, "invalid area");
                                                      retval.value = factory.setOf((t1!=null?t1.value:null));
                                                  } catch (IllegalArgumentException except) {
                                                      huh(h, "invalid area");
                                                      retval.value = factory.setOf((t1!=null?t1.value:null));
                                                  }
                                              

                                    }
                                    break;

                            }


                            }
                            break;
                        case 2 :
                            // src/Kodkodi.g:1182:10: 
                            {

                                          try {
                                              retval.value = factory.noneOf(arity);
                                          } catch (IllegalArgumentException except) {
                                              huh(b, "arity " + arity + " too large for universe of " +
                                                      "cardinality " + universe.size());
                                          }
                                     

                            }
                            break;

                    }

                    match(input,BRACE_RIGHT,FOLLOW_BRACE_RIGHT_in_basic_tuple_set1220); 

                    }
                    break;
                case 4 :
                    // src/Kodkodi.g:1191:9: n= NONE
                    {
                    n=(Token)match(input,NONE,FOLLOW_NONE_in_basic_tuple_set1236); 

                                  try {
                                      retval.value = factory.noneOf(arity);
                                  } catch (IllegalArgumentException except) {
                                      huh(n, "arity " + arity + " too large for universe of " +
                                              "cardinality " + universe.size());
                                  }
                            

                    }
                    break;
                case 5 :
                    // src/Kodkodi.g:1199:9: a= ALL
                    {
                    a=(Token)match(input,ALL,FOLLOW_ALL_in_basic_tuple_set1254); 

                                  try {
                                      retval.value = factory.allOf(arity);
                                  } catch (IllegalArgumentException except) {
                                      huh(a, "arity " + arity + " too large for universe of " +
                                              "cardinality " + universe.size());
                                  }
                            

                    }
                    break;
                case 6 :
                    // src/Kodkodi.g:1207:9: r= TUPLE_SET_REG
                    {
                    r=(Token)match(input,TUPLE_SET_REG,FOLLOW_TUPLE_SET_REG_in_basic_tuple_set1272); 
                     retval.value = getTupleSet(r); 

                    }
                    break;

            }
            retval.stop = input.LT(-1);

        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return retval;
    }
    // $ANTLR end "basic_tuple_set"

    public static class tuple_return extends ParserRuleReturnScope {
        public Tuple value;
        public List<Object> atoms;
    };

    // $ANTLR start "tuple"
    // src/Kodkodi.g:1208:1: tuple[int arity] returns [Tuple value, List<Object> atoms] : ( BRACKET_LEFT n1= ATOM_NAME ( COMMA n2= ATOM_NAME )* BRACKET_RIGHT | n= ATOM_NAME | n= TUPLE_NAME | r= TUPLE_REG );
    public final KodkodiParser.tuple_return tuple(int arity) throws RecognitionException {
        KodkodiParser.tuple_return retval = new KodkodiParser.tuple_return();
        retval.start = input.LT(1);

        Token n1=null;
        Token n2=null;
        Token n=null;
        Token r=null;

        try {
            // src/Kodkodi.g:1208:59: ( BRACKET_LEFT n1= ATOM_NAME ( COMMA n2= ATOM_NAME )* BRACKET_RIGHT | n= ATOM_NAME | n= TUPLE_NAME | r= TUPLE_REG )
            int alt26=4;
            switch ( input.LA(1) ) {
            case BRACKET_LEFT:
                {
                alt26=1;
                }
                break;
            case ATOM_NAME:
                {
                alt26=2;
                }
                break;
            case TUPLE_NAME:
                {
                alt26=3;
                }
                break;
            case TUPLE_REG:
                {
                alt26=4;
                }
                break;
            default:
                NoViableAltException nvae =
                    new NoViableAltException("", 26, 0, input);

                throw nvae;
            }

            switch (alt26) {
                case 1 :
                    // src/Kodkodi.g:1209:9: BRACKET_LEFT n1= ATOM_NAME ( COMMA n2= ATOM_NAME )* BRACKET_RIGHT
                    {
                    match(input,BRACKET_LEFT,FOLLOW_BRACKET_LEFT_in_tuple1293); 
                    n1=(Token)match(input,ATOM_NAME,FOLLOW_ATOM_NAME_in_tuple1299); 

                                retval.atoms = new ArrayList<Object>();
                                retval.atoms.add(getAtom(n1));
                            
                    // src/Kodkodi.g:1212:11: ( COMMA n2= ATOM_NAME )*
                    loop25:
                    do {
                        int alt25=2;
                        int LA25_0 = input.LA(1);

                        if ( (LA25_0==COMMA) ) {
                            alt25=1;
                        }


                        switch (alt25) {
                    	case 1 :
                    	    // src/Kodkodi.g:1212:12: COMMA n2= ATOM_NAME
                    	    {
                    	    match(input,COMMA,FOLLOW_COMMA_in_tuple1304); 
                    	    n2=(Token)match(input,ATOM_NAME,FOLLOW_ATOM_NAME_in_tuple1310); 

                    	                   retval.atoms.add(getAtom(n2));
                    	               

                    	    }
                    	    break;

                    	default :
                    	    break loop25;
                        }
                    } while (true);

                    match(input,BRACKET_RIGHT,FOLLOW_BRACKET_RIGHT_in_tuple1316); 
                     retval.value = factory.tuple(retval.atoms); 

                    }
                    break;
                case 2 :
                    // src/Kodkodi.g:1215:9: n= ATOM_NAME
                    {
                    n=(Token)match(input,ATOM_NAME,FOLLOW_ATOM_NAME_in_tuple1334); 

                                retval.value = factory.tuple(getAtom(n));
                            

                    }
                    break;
                case 3 :
                    // src/Kodkodi.g:1218:9: n= TUPLE_NAME
                    {
                    n=(Token)match(input,TUPLE_NAME,FOLLOW_TUPLE_NAME_in_tuple1352); 

                                final String text = (n!=null?n.getText():null);
                                try {
                                    retval.value = factory.tuple(arity(text), id(text));
                                } catch (IllegalArgumentException except) {
                                    huh(n, "tuple index out of range");
                                    retval.value = factory.tuple(arity(text), 0);
                                }
                            

                    }
                    break;
                case 4 :
                    // src/Kodkodi.g:1227:9: r= TUPLE_REG
                    {
                    r=(Token)match(input,TUPLE_REG,FOLLOW_TUPLE_REG_in_tuple1370); 
                     retval.value = getTuple(r); 

                    }
                    break;

            }
            retval.stop = input.LT(-1);

        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return retval;
    }
    // $ANTLR end "tuple"


    // $ANTLR start "expr"
    // src/Kodkodi.g:1229:1: expr returns [Object node] : ( ALL ds= decls b= BAR e= expr | SOME ds= decls b= BAR e= expr | SUM ds= decls b= BAR e= expr | l= LET as= assigns BAR e= expr | i= IF e= expr t= THEN e1= expr u= ELSE e2= expr | e1= iff_formula (o= OR es= or_formula_tail[$o,\n new SingletonArrayList<Formula>(F($o, $node))] )? );
    public final Object expr() throws RecognitionException {
        Object node = null;

        Token b=null;
        Token l=null;
        Token i=null;
        Token t=null;
        Token u=null;
        Token o=null;
        Decls ds = null;

        Object e = null;

        KodkodiParser.assigns_return as = null;

        Object e1 = null;

        Object e2 = null;

        Object es = null;


        try {
            // src/Kodkodi.g:1229:27: ( ALL ds= decls b= BAR e= expr | SOME ds= decls b= BAR e= expr | SUM ds= decls b= BAR e= expr | l= LET as= assigns BAR e= expr | i= IF e= expr t= THEN e1= expr u= ELSE e2= expr | e1= iff_formula (o= OR es= or_formula_tail[$o,\n new SingletonArrayList<Formula>(F($o, $node))] )? )
            int alt28=6;
            switch ( input.LA(1) ) {
            case ALL:
                {
                alt28=1;
                }
                break;
            case SOME:
                {
                int LA28_2 = input.LA(2);

                if ( (LA28_2==BRACKET_LEFT) ) {
                    alt28=2;
                }
                else if ( (LA28_2==NUM||(LA28_2>=TRUE && LA28_2<=FALSE)||(LA28_2>=UNIV && LA28_2<=UNIV_NAME)||LA28_2==RELATION_NAME||(LA28_2>=FORMULA_REG && LA28_2<=INT_EXPR_REG)||LA28_2==MINUS||(LA28_2>=OFFSET_UNIV_NAME && LA28_2<=PAREN_LEFT)||LA28_2==BRACE_LEFT||LA28_2==HASH||LA28_2==NONE||LA28_2==ATOM_NAME||LA28_2==SUM||LA28_2==STAR||LA28_2==HAT||(LA28_2>=VARIABLE_NAME && LA28_2<=INT)) ) {
                    alt28=6;
                }
                else {
                    NoViableAltException nvae =
                        new NoViableAltException("", 28, 2, input);

                    throw nvae;
                }
                }
                break;
            case SUM:
                {
                int LA28_3 = input.LA(2);

                if ( (LA28_3==PAREN_LEFT) ) {
                    alt28=6;
                }
                else if ( (LA28_3==BRACKET_LEFT) ) {
                    alt28=3;
                }
                else {
                    NoViableAltException nvae =
                        new NoViableAltException("", 28, 3, input);

                    throw nvae;
                }
                }
                break;
            case LET:
                {
                alt28=4;
                }
                break;
            case IF:
                {
                alt28=5;
                }
                break;
            case NUM:
            case TRUE:
            case FALSE:
            case UNIV:
            case UNIV_NAME:
            case RELATION_NAME:
            case FORMULA_REG:
            case REL_EXPR_REG:
            case INT_EXPR_REG:
            case MINUS:
            case OFFSET_UNIV_NAME:
            case PAREN_LEFT:
            case BRACE_LEFT:
            case HASH:
            case NONE:
            case ATOM_NAME:
            case NOT:
            case ACYCLIC:
            case FUNCTION:
            case ONE:
            case LONE:
            case TOTAL_ORDERING:
            case STAR:
            case HAT:
            case VARIABLE_NAME:
            case IDEN:
            case INTS:
            case TILDE:
            case ABS:
            case SGN:
            case BITS:
            case INT:
            case NO:
            case SET:
                {
                alt28=6;
                }
                break;
            default:
                NoViableAltException nvae =
                    new NoViableAltException("", 28, 0, input);

                throw nvae;
            }

            switch (alt28) {
                case 1 :
                    // src/Kodkodi.g:1230:9: ALL ds= decls b= BAR e= expr
                    {
                    match(input,ALL,FOLLOW_ALL_in_expr1391); 
                    pushFollow(FOLLOW_decls_in_expr1397);
                    ds=decls();

                    state._fsp--;

                    b=(Token)match(input,BAR,FOLLOW_BAR_in_expr1403); 
                    pushFollow(FOLLOW_expr_in_expr1409);
                    e=expr();

                    state._fsp--;


                                if (ds == null) {
                                    node = F(b, e);
                                } else {
                                    node = F(b, e).forAll(ds);
                                }
                            

                    }
                    break;
                case 2 :
                    // src/Kodkodi.g:1237:9: SOME ds= decls b= BAR e= expr
                    {
                    match(input,SOME,FOLLOW_SOME_in_expr1423); 
                    pushFollow(FOLLOW_decls_in_expr1429);
                    ds=decls();

                    state._fsp--;

                    b=(Token)match(input,BAR,FOLLOW_BAR_in_expr1435); 
                    pushFollow(FOLLOW_expr_in_expr1441);
                    e=expr();

                    state._fsp--;


                                if (ds == null) {
                                    node = F(b, e);
                                } else {
                                    node = F(b, e).forSome(ds);
                                }
                            

                    }
                    break;
                case 3 :
                    // src/Kodkodi.g:1244:9: SUM ds= decls b= BAR e= expr
                    {
                    match(input,SUM,FOLLOW_SUM_in_expr1455); 
                    pushFollow(FOLLOW_decls_in_expr1461);
                    ds=decls();

                    state._fsp--;

                    b=(Token)match(input,BAR,FOLLOW_BAR_in_expr1467); 
                    pushFollow(FOLLOW_expr_in_expr1473);
                    e=expr();

                    state._fsp--;


                                try {
                                    node = I(b, e).sum(ds);
                                } catch (IllegalArgumentException except) {
                                    huh(b, "expected one or more declarations with multiplicity "
                                            + "'one'");
                                }
                            

                    }
                    break;
                case 4 :
                    // src/Kodkodi.g:1252:9: l= LET as= assigns BAR e= expr
                    {
                    l=(Token)match(input,LET,FOLLOW_LET_in_expr1491); 
                    pushFollow(FOLLOW_assigns_in_expr1497);
                    as=assigns();

                    state._fsp--;

                    match(input,BAR,FOLLOW_BAR_in_expr1499); 
                    pushFollow(FOLLOW_expr_in_expr1505);
                    e=expr();

                    state._fsp--;


                                node = e;
                                for (int j = (as!=null?as.oldNodes:null).size() - 1; j >= 0; --j)
                                    setReg((as!=null?as.tokens:null).elementAt(j), (as!=null?as.oldNodes:null).elementAt(j));
                            

                    }
                    break;
                case 5 :
                    // src/Kodkodi.g:1257:9: i= IF e= expr t= THEN e1= expr u= ELSE e2= expr
                    {
                    i=(Token)match(input,IF,FOLLOW_IF_in_expr1523); 
                    pushFollow(FOLLOW_expr_in_expr1529);
                    e=expr();

                    state._fsp--;

                    t=(Token)match(input,THEN,FOLLOW_THEN_in_expr1535); 
                    pushFollow(FOLLOW_expr_in_expr1541);
                    e1=expr();

                    state._fsp--;

                    u=(Token)match(input,ELSE,FOLLOW_ELSE_in_expr1547); 
                    pushFollow(FOLLOW_expr_in_expr1553);
                    e2=expr();

                    state._fsp--;


                                if (isExpression(e1)) {
                                    try {
                                        node = F(i, e)
                                                .thenElse(E(t, e1), E(u, e2));
                                    } catch (IllegalArgumentException except) {
                                        int arity1 = E(t, e1).arity();
                                        int arity2 = E(u, e2).arity();
                                        if (arity1 == arity2) {
                                            panic();
                                        } else {
                                            huh(u, "arity mismatch (" + arity1 + " vs. " + arity2
                                                    + ")");
                                        }
                                    }
                                } else if (isIntExpression(e1)) {
                                    node = F(i, e)
                                            .thenElse(I(t, e1), I(u, e2));
                                } else {
                                    Formula f = F(i, e);
                                    node = (f.and(F(u, e1)))
                                            .or(f.not().and(F(u, e2)));
                                }
                            

                    }
                    break;
                case 6 :
                    // src/Kodkodi.g:1281:9: e1= iff_formula (o= OR es= or_formula_tail[$o,\n new SingletonArrayList<Formula>(F($o, $node))] )?
                    {
                    pushFollow(FOLLOW_iff_formula_in_expr1571);
                    e1=iff_formula();

                    state._fsp--;

                     node = e1; 
                    // src/Kodkodi.g:1282:9: (o= OR es= or_formula_tail[$o,\n new SingletonArrayList<Formula>(F($o, $node))] )?
                    int alt27=2;
                    int LA27_0 = input.LA(1);

                    if ( (LA27_0==OR) ) {
                        alt27=1;
                    }
                    switch (alt27) {
                        case 1 :
                            // src/Kodkodi.g:1282:10: o= OR es= or_formula_tail[$o,\n new SingletonArrayList<Formula>(F($o, $node))]
                            {
                            o=(Token)match(input,OR,FOLLOW_OR_in_expr1588); 
                            pushFollow(FOLLOW_or_formula_tail_in_expr1594);
                            es=or_formula_tail(o, new SingletonArrayList<Formula>(F(o, node)));

                            state._fsp--;

                             node = es; 

                            }
                            break;

                    }


                    }
                    break;

            }
        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return node;
    }
    // $ANTLR end "expr"


    // $ANTLR start "or_formula_tail"
    // src/Kodkodi.g:1285:1: or_formula_tail[Token token, List<Formula> formulas] returns [Object node] : e1= iff_formula ( OR es= or_formula_tail[$token, $formulas] | ) ;
    public final Object or_formula_tail(Token token, List<Formula> formulas) throws RecognitionException {
        Object node = null;

        Object e1 = null;

        Object es = null;


        try {
            // src/Kodkodi.g:1285:75: (e1= iff_formula ( OR es= or_formula_tail[$token, $formulas] | ) )
            // src/Kodkodi.g:1286:9: e1= iff_formula ( OR es= or_formula_tail[$token, $formulas] | )
            {
            pushFollow(FOLLOW_iff_formula_in_or_formula_tail1635);
            e1=iff_formula();

            state._fsp--;

             formulas.add(F(token, e1)); 
            // src/Kodkodi.g:1287:9: ( OR es= or_formula_tail[$token, $formulas] | )
            int alt29=2;
            int LA29_0 = input.LA(1);

            if ( (LA29_0==OR) ) {
                alt29=1;
            }
            else if ( (LA29_0==COMMA||LA29_0==BRACKET_RIGHT||(LA29_0>=FORMULA_REG && LA29_0<=SEMICOLON)||LA29_0==ARROW||LA29_0==PAREN_RIGHT||LA29_0==BRACE_RIGHT||(LA29_0>=THEN && LA29_0<=ELSE)) ) {
                alt29=2;
            }
            else {
                NoViableAltException nvae =
                    new NoViableAltException("", 29, 0, input);

                throw nvae;
            }
            switch (alt29) {
                case 1 :
                    // src/Kodkodi.g:1287:10: OR es= or_formula_tail[$token, $formulas]
                    {
                    match(input,OR,FOLLOW_OR_in_or_formula_tail1648); 
                    pushFollow(FOLLOW_or_formula_tail_in_or_formula_tail1654);
                    es=or_formula_tail(token, formulas);

                    state._fsp--;

                     node = es; 

                    }
                    break;
                case 2 :
                    // src/Kodkodi.g:1288:10: 
                    {
                     node = Formula.or(formulas); 

                    }
                    break;

            }


            }

        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return node;
    }
    // $ANTLR end "or_formula_tail"


    // $ANTLR start "iff_formula"
    // src/Kodkodi.g:1289:1: iff_formula returns [Object node] : e1= implies_formula (i= IFF e2= implies_formula )* ;
    public final Object iff_formula() throws RecognitionException {
        Object node = null;

        Token i=null;
        Object e1 = null;

        Object e2 = null;


        try {
            // src/Kodkodi.g:1289:34: (e1= implies_formula (i= IFF e2= implies_formula )* )
            // src/Kodkodi.g:1290:9: e1= implies_formula (i= IFF e2= implies_formula )*
            {
            pushFollow(FOLLOW_implies_formula_in_iff_formula1693);
            e1=implies_formula();

            state._fsp--;

             node = e1; 
            // src/Kodkodi.g:1291:9: (i= IFF e2= implies_formula )*
            loop30:
            do {
                int alt30=2;
                int LA30_0 = input.LA(1);

                if ( (LA30_0==IFF) ) {
                    alt30=1;
                }


                switch (alt30) {
            	case 1 :
            	    // src/Kodkodi.g:1291:10: i= IFF e2= implies_formula
            	    {
            	    i=(Token)match(input,IFF,FOLLOW_IFF_in_iff_formula1710); 
            	    pushFollow(FOLLOW_implies_formula_in_iff_formula1716);
            	    e2=implies_formula();

            	    state._fsp--;


            	                 node = F(i, node).iff(F(i, e2));
            	             

            	    }
            	    break;

            	default :
            	    break loop30;
                }
            } while (true);


            }

        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return node;
    }
    // $ANTLR end "iff_formula"


    // $ANTLR start "implies_formula"
    // src/Kodkodi.g:1294:1: implies_formula returns [Object node] : e1= and_formula (i= IMPLIES e2= implies_formula )? ;
    public final Object implies_formula() throws RecognitionException {
        Object node = null;

        Token i=null;
        Object e1 = null;

        Object e2 = null;


        try {
            // src/Kodkodi.g:1294:38: (e1= and_formula (i= IMPLIES e2= implies_formula )? )
            // src/Kodkodi.g:1295:9: e1= and_formula (i= IMPLIES e2= implies_formula )?
            {
            pushFollow(FOLLOW_and_formula_in_implies_formula1742);
            e1=and_formula();

            state._fsp--;

             node = e1; 
            // src/Kodkodi.g:1296:9: (i= IMPLIES e2= implies_formula )?
            int alt31=2;
            int LA31_0 = input.LA(1);

            if ( (LA31_0==IMPLIES) ) {
                alt31=1;
            }
            switch (alt31) {
                case 1 :
                    // src/Kodkodi.g:1296:10: i= IMPLIES e2= implies_formula
                    {
                    i=(Token)match(input,IMPLIES,FOLLOW_IMPLIES_in_implies_formula1759); 
                    pushFollow(FOLLOW_implies_formula_in_implies_formula1765);
                    e2=implies_formula();

                    state._fsp--;


                                 node = F(i, node).implies(F(i, e2));
                             

                    }
                    break;

            }


            }

        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return node;
    }
    // $ANTLR end "implies_formula"


    // $ANTLR start "and_formula"
    // src/Kodkodi.g:1299:1: and_formula returns [Object node] : e1= basic_formula (a= AND es= and_formula_tail[$a,\n new SingletonArrayList<Formula>(F($a, $node))] )? ;
    public final Object and_formula() throws RecognitionException {
        Object node = null;

        Token a=null;
        Object e1 = null;

        Object es = null;


        try {
            // src/Kodkodi.g:1299:34: (e1= basic_formula (a= AND es= and_formula_tail[$a,\n new SingletonArrayList<Formula>(F($a, $node))] )? )
            // src/Kodkodi.g:1300:9: e1= basic_formula (a= AND es= and_formula_tail[$a,\n new SingletonArrayList<Formula>(F($a, $node))] )?
            {
            pushFollow(FOLLOW_basic_formula_in_and_formula1791);
            e1=basic_formula();

            state._fsp--;

             node = e1; 
            // src/Kodkodi.g:1301:9: (a= AND es= and_formula_tail[$a,\n new SingletonArrayList<Formula>(F($a, $node))] )?
            int alt32=2;
            int LA32_0 = input.LA(1);

            if ( (LA32_0==AND) ) {
                alt32=1;
            }
            switch (alt32) {
                case 1 :
                    // src/Kodkodi.g:1301:10: a= AND es= and_formula_tail[$a,\n new SingletonArrayList<Formula>(F($a, $node))]
                    {
                    a=(Token)match(input,AND,FOLLOW_AND_in_and_formula1808); 
                    pushFollow(FOLLOW_and_formula_tail_in_and_formula1814);
                    es=and_formula_tail(a, new SingletonArrayList<Formula>(F(a, node)));

                    state._fsp--;

                     node = es; 

                    }
                    break;

            }


            }

        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return node;
    }
    // $ANTLR end "and_formula"


    // $ANTLR start "and_formula_tail"
    // src/Kodkodi.g:1304:1: and_formula_tail[Token token, List<Formula> formulas] returns [Object node] : e1= basic_formula ( AND es= and_formula_tail[$token, $formulas] | ) ;
    public final Object and_formula_tail(Token token, List<Formula> formulas) throws RecognitionException {
        Object node = null;

        Object e1 = null;

        Object es = null;


        try {
            // src/Kodkodi.g:1304:76: (e1= basic_formula ( AND es= and_formula_tail[$token, $formulas] | ) )
            // src/Kodkodi.g:1305:9: e1= basic_formula ( AND es= and_formula_tail[$token, $formulas] | )
            {
            pushFollow(FOLLOW_basic_formula_in_and_formula_tail1855);
            e1=basic_formula();

            state._fsp--;

             formulas.add(F(token, e1)); 
            // src/Kodkodi.g:1306:9: ( AND es= and_formula_tail[$token, $formulas] | )
            int alt33=2;
            int LA33_0 = input.LA(1);

            if ( (LA33_0==AND) ) {
                alt33=1;
            }
            else if ( (LA33_0==COMMA||LA33_0==BRACKET_RIGHT||(LA33_0>=FORMULA_REG && LA33_0<=SEMICOLON)||LA33_0==ARROW||LA33_0==PAREN_RIGHT||LA33_0==BRACE_RIGHT||(LA33_0>=THEN && LA33_0<=IMPLIES)) ) {
                alt33=2;
            }
            else {
                NoViableAltException nvae =
                    new NoViableAltException("", 33, 0, input);

                throw nvae;
            }
            switch (alt33) {
                case 1 :
                    // src/Kodkodi.g:1306:10: AND es= and_formula_tail[$token, $formulas]
                    {
                    match(input,AND,FOLLOW_AND_in_and_formula_tail1868); 
                    pushFollow(FOLLOW_and_formula_tail_in_and_formula_tail1874);
                    es=and_formula_tail(token, formulas);

                    state._fsp--;

                     node = es; 

                    }
                    break;
                case 2 :
                    // src/Kodkodi.g:1307:10: 
                    {
                     node = Formula.and(formulas); 

                    }
                    break;

            }


            }

        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return node;
    }
    // $ANTLR end "and_formula_tail"


    // $ANTLR start "basic_formula"
    // src/Kodkodi.g:1308:1: basic_formula returns [Object node] : (t= NOT e1= basic_formula | e1= predicate_formula | e1= shift_expr (t= ( EQ | LT | LE | GT | GE | IN ) e2= shift_expr | ) | m= multiplicity e1= add_expr );
    public final Object basic_formula() throws RecognitionException {
        Object node = null;

        Token t=null;
        Object e1 = null;

        Object e2 = null;

        KodkodiParser.multiplicity_return m = null;


        try {
            // src/Kodkodi.g:1308:36: (t= NOT e1= basic_formula | e1= predicate_formula | e1= shift_expr (t= ( EQ | LT | LE | GT | GE | IN ) e2= shift_expr | ) | m= multiplicity e1= add_expr )
            int alt35=4;
            switch ( input.LA(1) ) {
            case NOT:
                {
                alt35=1;
                }
                break;
            case ACYCLIC:
            case FUNCTION:
            case TOTAL_ORDERING:
                {
                alt35=2;
                }
                break;
            case NUM:
            case TRUE:
            case FALSE:
            case UNIV:
            case UNIV_NAME:
            case RELATION_NAME:
            case FORMULA_REG:
            case REL_EXPR_REG:
            case INT_EXPR_REG:
            case MINUS:
            case OFFSET_UNIV_NAME:
            case PAREN_LEFT:
            case BRACE_LEFT:
            case HASH:
            case NONE:
            case ATOM_NAME:
            case SUM:
            case STAR:
            case HAT:
            case VARIABLE_NAME:
            case IDEN:
            case INTS:
            case TILDE:
            case ABS:
            case SGN:
            case BITS:
            case INT:
                {
                alt35=3;
                }
                break;
            case SOME:
            case ONE:
            case LONE:
            case NO:
            case SET:
                {
                alt35=4;
                }
                break;
            default:
                NoViableAltException nvae =
                    new NoViableAltException("", 35, 0, input);

                throw nvae;
            }

            switch (alt35) {
                case 1 :
                    // src/Kodkodi.g:1309:9: t= NOT e1= basic_formula
                    {
                    t=(Token)match(input,NOT,FOLLOW_NOT_in_basic_formula1913); 
                    pushFollow(FOLLOW_basic_formula_in_basic_formula1919);
                    e1=basic_formula();

                    state._fsp--;

                     node = F(t, e1).not(); 

                    }
                    break;
                case 2 :
                    // src/Kodkodi.g:1310:9: e1= predicate_formula
                    {
                    pushFollow(FOLLOW_predicate_formula_in_basic_formula1937);
                    e1=predicate_formula();

                    state._fsp--;

                     node = e1; 

                    }
                    break;
                case 3 :
                    // src/Kodkodi.g:1311:9: e1= shift_expr (t= ( EQ | LT | LE | GT | GE | IN ) e2= shift_expr | )
                    {
                    pushFollow(FOLLOW_shift_expr_in_basic_formula1955);
                    e1=shift_expr();

                    state._fsp--;

                    // src/Kodkodi.g:1312:9: (t= ( EQ | LT | LE | GT | GE | IN ) e2= shift_expr | )
                    int alt34=2;
                    int LA34_0 = input.LA(1);

                    if ( ((LA34_0>=EQ && LA34_0<=IN)) ) {
                        alt34=1;
                    }
                    else if ( (LA34_0==COMMA||LA34_0==BRACKET_RIGHT||(LA34_0>=FORMULA_REG && LA34_0<=SEMICOLON)||(LA34_0>=AND && LA34_0<=ARROW)||LA34_0==PAREN_RIGHT||LA34_0==BRACE_RIGHT||(LA34_0>=THEN && LA34_0<=IMPLIES)) ) {
                        alt34=2;
                    }
                    else {
                        NoViableAltException nvae =
                            new NoViableAltException("", 34, 0, input);

                        throw nvae;
                    }
                    switch (alt34) {
                        case 1 :
                            // src/Kodkodi.g:1312:10: t= ( EQ | LT | LE | GT | GE | IN ) e2= shift_expr
                            {
                            t=(Token)input.LT(1);
                            if ( (input.LA(1)>=EQ && input.LA(1)<=IN) ) {
                                input.consume();
                                state.errorRecovery=false;
                            }
                            else {
                                MismatchedSetException mse = new MismatchedSetException(null,input);
                                throw mse;
                            }

                            pushFollow(FOLLOW_shift_expr_in_basic_formula1998);
                            e2=shift_expr();

                            state._fsp--;


                                         if (isExpression(e1)) {
                                             try {
                                                 if ((t!=null?t.getType():0) == EQ) {
                                                     node = E(t, e1).eq(E(t, e2));
                                                 } else if ((t!=null?t.getType():0) == IN) {
                                                     node = E(t, e1).in(E(t, e2));                 
                                                 } else {
                                                     huh(t, "mismatched input '" + (t!=null?t.getText():null)
                                                             + "' expecting EQ or IN");
                                                     node = Formula.FALSE;
                                                 }
                                             } catch (IllegalArgumentException except) {
                                                 int arity1 = E(t, e1).arity();
                                                 int arity2 = E(t, e2).arity();
                                                 if (arity1 == arity2) {
                                                     panic();
                                                 } else {
                                                     huh(t, "arity mismatch (" + arity1 + " vs. " + arity2
                                                             + ")");
                                                 }
                                             }
                                         } else if (isIntExpression(e1)) {
                                             if ((t!=null?t.getType():0) != IN) {
                                                 node = I(t, e1).compare(
                                                             (t!=null?t.getType():0) == EQ
                                                                 ? IntCompOperator.EQ
                                                           : (t!=null?t.getType():0) == LT
                                                                 ? IntCompOperator.LT
                                                           : (t!=null?t.getType():0) == LE
                                                                 ? IntCompOperator.LTE
                                                           : (t!=null?t.getType():0) == GT
                                                                 ? IntCompOperator.GT
                                                           : IntCompOperator.GTE,
                                                             I(t, e2));
                                             } else {
                                                 huh(t, "mismatched input 'in' expecting EQ, LT, GT,"
                                                         + " etc.");
                                                 node = Formula.FALSE;
                                             }
                                         } else {
                                             huh(t, "operands may not be formulas");
                                         }
                                     

                            }
                            break;
                        case 2 :
                            // src/Kodkodi.g:1355:14: 
                            {

                                         node = e1;
                                     

                            }
                            break;

                    }


                    }
                    break;
                case 4 :
                    // src/Kodkodi.g:1358:9: m= multiplicity e1= add_expr
                    {
                    pushFollow(FOLLOW_multiplicity_in_basic_formula2021);
                    m=multiplicity();

                    state._fsp--;

                    pushFollow(FOLLOW_add_expr_in_basic_formula2027);
                    e1=add_expr();

                    state._fsp--;


                                if ((m!=null?m.value:null) != Multiplicity.SET) {
                                    node = E((m!=null?m.token:null), e1).apply((m!=null?m.value:null));
                                } else {
                                    huh((m!=null?m.token:null), "invalid multiplicity 'set'");
                                }
                            

                    }
                    break;

            }
        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return node;
    }
    // $ANTLR end "basic_formula"


    // $ANTLR start "predicate_formula"
    // src/Kodkodi.g:1365:1: predicate_formula returns [Object node] : (a= ACYCLIC PAREN_LEFT n= RELATION_NAME PAREN_RIGHT | f= FUNCTION PAREN_LEFT n= RELATION_NAME c= COMMA e1= expr ARROW t= ( ONE | LONE ) e2= expr PAREN_RIGHT | t= TOTAL_ORDERING PAREN_LEFT n1= RELATION_NAME COMMA n2= ( UNIV_NAME | OFFSET_UNIV_NAME | RELATION_NAME ) COMMA n3= ( ATOM_NAME | RELATION_NAME ) COMMA n4= ( ATOM_NAME | RELATION_NAME ) PAREN_RIGHT );
    public final Object predicate_formula() throws RecognitionException {
        Object node = null;

        Token a=null;
        Token n=null;
        Token f=null;
        Token c=null;
        Token t=null;
        Token n1=null;
        Token n2=null;
        Token n3=null;
        Token n4=null;
        Object e1 = null;

        Object e2 = null;


        try {
            // src/Kodkodi.g:1365:40: (a= ACYCLIC PAREN_LEFT n= RELATION_NAME PAREN_RIGHT | f= FUNCTION PAREN_LEFT n= RELATION_NAME c= COMMA e1= expr ARROW t= ( ONE | LONE ) e2= expr PAREN_RIGHT | t= TOTAL_ORDERING PAREN_LEFT n1= RELATION_NAME COMMA n2= ( UNIV_NAME | OFFSET_UNIV_NAME | RELATION_NAME ) COMMA n3= ( ATOM_NAME | RELATION_NAME ) COMMA n4= ( ATOM_NAME | RELATION_NAME ) PAREN_RIGHT )
            int alt36=3;
            switch ( input.LA(1) ) {
            case ACYCLIC:
                {
                alt36=1;
                }
                break;
            case FUNCTION:
                {
                alt36=2;
                }
                break;
            case TOTAL_ORDERING:
                {
                alt36=3;
                }
                break;
            default:
                NoViableAltException nvae =
                    new NoViableAltException("", 36, 0, input);

                throw nvae;
            }

            switch (alt36) {
                case 1 :
                    // src/Kodkodi.g:1366:9: a= ACYCLIC PAREN_LEFT n= RELATION_NAME PAREN_RIGHT
                    {
                    a=(Token)match(input,ACYCLIC,FOLLOW_ACYCLIC_in_predicate_formula2051); 
                    match(input,PAREN_LEFT,FOLLOW_PAREN_LEFT_in_predicate_formula2053); 
                    n=(Token)match(input,RELATION_NAME,FOLLOW_RELATION_NAME_in_predicate_formula2059); 
                    match(input,PAREN_RIGHT,FOLLOW_PAREN_RIGHT_in_predicate_formula2061); 

                                try {
                                    node = getRelation(n).acyclic();
                                } catch (IllegalArgumentException except) {
                                    huh(a, "invalid arity");
                                }
                            

                    }
                    break;
                case 2 :
                    // src/Kodkodi.g:1373:9: f= FUNCTION PAREN_LEFT n= RELATION_NAME c= COMMA e1= expr ARROW t= ( ONE | LONE ) e2= expr PAREN_RIGHT
                    {
                    f=(Token)match(input,FUNCTION,FOLLOW_FUNCTION_in_predicate_formula2079); 
                    match(input,PAREN_LEFT,FOLLOW_PAREN_LEFT_in_predicate_formula2081); 
                    n=(Token)match(input,RELATION_NAME,FOLLOW_RELATION_NAME_in_predicate_formula2087); 
                    c=(Token)match(input,COMMA,FOLLOW_COMMA_in_predicate_formula2093); 
                    pushFollow(FOLLOW_expr_in_predicate_formula2099);
                    e1=expr();

                    state._fsp--;

                    match(input,ARROW,FOLLOW_ARROW_in_predicate_formula2101); 
                    t=(Token)input.LT(1);
                    if ( (input.LA(1)>=ONE && input.LA(1)<=LONE) ) {
                        input.consume();
                        state.errorRecovery=false;
                    }
                    else {
                        MismatchedSetException mse = new MismatchedSetException(null,input);
                        throw mse;
                    }

                    pushFollow(FOLLOW_expr_in_predicate_formula2127);
                    e2=expr();

                    state._fsp--;

                    match(input,PAREN_RIGHT,FOLLOW_PAREN_RIGHT_in_predicate_formula2129); 

                                try {
                                    Relation relation = getRelation(n);
                                    node = ((t!=null?t.getType():0) == ONE
                                                 ? relation.function(E(c, e1),
                                                                     E(t, e2))
                                                 : relation.partialFunction(E(c, e1),
                                                                            E(t, e2)));
                                } catch (IllegalArgumentException except) {
                                    huh(f, "invalid arity");
                                }
                            

                    }
                    break;
                case 3 :
                    // src/Kodkodi.g:1386:9: t= TOTAL_ORDERING PAREN_LEFT n1= RELATION_NAME COMMA n2= ( UNIV_NAME | OFFSET_UNIV_NAME | RELATION_NAME ) COMMA n3= ( ATOM_NAME | RELATION_NAME ) COMMA n4= ( ATOM_NAME | RELATION_NAME ) PAREN_RIGHT
                    {
                    t=(Token)match(input,TOTAL_ORDERING,FOLLOW_TOTAL_ORDERING_in_predicate_formula2147); 
                    match(input,PAREN_LEFT,FOLLOW_PAREN_LEFT_in_predicate_formula2149); 
                    n1=(Token)match(input,RELATION_NAME,FOLLOW_RELATION_NAME_in_predicate_formula2155); 
                    match(input,COMMA,FOLLOW_COMMA_in_predicate_formula2157); 
                    n2=(Token)input.LT(1);
                    if ( input.LA(1)==UNIV_NAME||input.LA(1)==RELATION_NAME||input.LA(1)==OFFSET_UNIV_NAME ) {
                        input.consume();
                        state.errorRecovery=false;
                    }
                    else {
                        MismatchedSetException mse = new MismatchedSetException(null,input);
                        throw mse;
                    }

                    match(input,COMMA,FOLLOW_COMMA_in_predicate_formula2183); 
                    n3=(Token)input.LT(1);
                    if ( input.LA(1)==RELATION_NAME||input.LA(1)==ATOM_NAME ) {
                        input.consume();
                        state.errorRecovery=false;
                    }
                    else {
                        MismatchedSetException mse = new MismatchedSetException(null,input);
                        throw mse;
                    }

                    match(input,COMMA,FOLLOW_COMMA_in_predicate_formula2205); 
                    n4=(Token)input.LT(1);
                    if ( input.LA(1)==RELATION_NAME||input.LA(1)==ATOM_NAME ) {
                        input.consume();
                        state.errorRecovery=false;
                    }
                    else {
                        MismatchedSetException mse = new MismatchedSetException(null,input);
                        throw mse;
                    }

                    match(input,PAREN_RIGHT,FOLLOW_PAREN_RIGHT_in_predicate_formula2227); 

                                try {
                                    node = getRelation(n1).totalOrder(getRelation(n2),
                                                                        getRelation(n3),
                                                                        getRelation(n4));
                                } catch (IllegalArgumentException except) {
                                    huh(t, "invalid arity");
                                }
                            

                    }
                    break;

            }
        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return node;
    }
    // $ANTLR end "predicate_formula"


    // $ANTLR start "shift_expr"
    // src/Kodkodi.g:1398:1: shift_expr returns [Object node] : e1= add_expr (t= ( SHL | SHA | SHR ) e2= add_expr )* ;
    public final Object shift_expr() throws RecognitionException {
        Object node = null;

        Token t=null;
        Object e1 = null;

        Object e2 = null;


        try {
            // src/Kodkodi.g:1398:33: (e1= add_expr (t= ( SHL | SHA | SHR ) e2= add_expr )* )
            // src/Kodkodi.g:1399:9: e1= add_expr (t= ( SHL | SHA | SHR ) e2= add_expr )*
            {
            pushFollow(FOLLOW_add_expr_in_shift_expr2251);
            e1=add_expr();

            state._fsp--;

             node = e1; 
            // src/Kodkodi.g:1400:9: (t= ( SHL | SHA | SHR ) e2= add_expr )*
            loop37:
            do {
                int alt37=2;
                int LA37_0 = input.LA(1);

                if ( ((LA37_0>=SHL && LA37_0<=SHR)) ) {
                    alt37=1;
                }


                switch (alt37) {
            	case 1 :
            	    // src/Kodkodi.g:1400:10: t= ( SHL | SHA | SHR ) e2= add_expr
            	    {
            	    t=(Token)input.LT(1);
            	    if ( (input.LA(1)>=SHL && input.LA(1)<=SHR) ) {
            	        input.consume();
            	        state.errorRecovery=false;
            	    }
            	    else {
            	        MismatchedSetException mse = new MismatchedSetException(null,input);
            	        throw mse;
            	    }

            	    pushFollow(FOLLOW_add_expr_in_shift_expr2284);
            	    e2=add_expr();

            	    state._fsp--;


            	                 node = I(t, node).compose(
            	                                     (t!=null?t.getType():0) == SHL
            	                                         ? IntOperator.SHL
            	                                   : (t!=null?t.getType():0) == SHA
            	                                         ? IntOperator.SHA
            	                                         : IntOperator.SHR,
            	                                     I(t, e2));
            	             

            	    }
            	    break;

            	default :
            	    break loop37;
                }
            } while (true);


            }

        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return node;
    }
    // $ANTLR end "shift_expr"


    // $ANTLR start "add_expr"
    // src/Kodkodi.g:1409:1: add_expr returns [Object node] : e1= mult_expr (t= ( PLUS | MINUS ) e2= mult_expr )* ;
    public final Object add_expr() throws RecognitionException {
        Object node = null;

        Token t=null;
        Object e1 = null;

        Object e2 = null;


        try {
            // src/Kodkodi.g:1409:31: (e1= mult_expr (t= ( PLUS | MINUS ) e2= mult_expr )* )
            // src/Kodkodi.g:1410:9: e1= mult_expr (t= ( PLUS | MINUS ) e2= mult_expr )*
            {
            pushFollow(FOLLOW_mult_expr_in_add_expr2310);
            e1=mult_expr();

            state._fsp--;

             node = e1; 
            // src/Kodkodi.g:1411:9: (t= ( PLUS | MINUS ) e2= mult_expr )*
            loop38:
            do {
                int alt38=2;
                int LA38_0 = input.LA(1);

                if ( ((LA38_0>=PLUS && LA38_0<=MINUS)) ) {
                    alt38=1;
                }


                switch (alt38) {
            	case 1 :
            	    // src/Kodkodi.g:1411:10: t= ( PLUS | MINUS ) e2= mult_expr
            	    {
            	    t=(Token)input.LT(1);
            	    if ( (input.LA(1)>=PLUS && input.LA(1)<=MINUS) ) {
            	        input.consume();
            	        state.errorRecovery=false;
            	    }
            	    else {
            	        MismatchedSetException mse = new MismatchedSetException(null,input);
            	        throw mse;
            	    }

            	    pushFollow(FOLLOW_mult_expr_in_add_expr2339);
            	    e2=mult_expr();

            	    state._fsp--;


            	                 if (isExpression(node)) {
            	                     try {
            	                         node = E(t, node).compose(
            	                                         (t!=null?t.getType():0) == PLUS
            	                                             ? ExprOperator.UNION
            	                                             : ExprOperator.DIFFERENCE,
            	                                         E(t, e2));
            	                     } catch (IllegalArgumentException except) {
            	                         int arity1 = E(t, node).arity();
            	                         int arity2 = E(t, e2).arity();
            	                         if (arity1 == arity2) {
            	                             panic();
            	                         } else {
            	                             huh(t, "arity mismatch (" + arity1 + " vs. " + arity2
            	                                     + ")");
            	                         }
            	                     }
            	                 } else if (isIntExpression(node)) {
            	                     node = I(t, node).compose(
            	                                     (t!=null?t.getType():0) == PLUS
            	                                         ? IntOperator.PLUS
            	                                         : IntOperator.MINUS,
            	                                     I(t, e2));
            	                 } else {
            	                     huh(t, "operands may not be formulas");
            	                 }
            	             

            	    }
            	    break;

            	default :
            	    break loop38;
                }
            } while (true);


            }

        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return node;
    }
    // $ANTLR end "add_expr"


    // $ANTLR start "mult_expr"
    // src/Kodkodi.g:1439:1: mult_expr returns [Object node] : e1= expr_to_int_cast (t= ( STAR | DIVIDE | MODULO ) e2= expr_to_int_cast )* ;
    public final Object mult_expr() throws RecognitionException {
        Object node = null;

        Token t=null;
        Object e1 = null;

        Object e2 = null;


        try {
            // src/Kodkodi.g:1439:32: (e1= expr_to_int_cast (t= ( STAR | DIVIDE | MODULO ) e2= expr_to_int_cast )* )
            // src/Kodkodi.g:1440:9: e1= expr_to_int_cast (t= ( STAR | DIVIDE | MODULO ) e2= expr_to_int_cast )*
            {
            pushFollow(FOLLOW_expr_to_int_cast_in_mult_expr2365);
            e1=expr_to_int_cast();

            state._fsp--;

             node = e1; 
            // src/Kodkodi.g:1441:9: (t= ( STAR | DIVIDE | MODULO ) e2= expr_to_int_cast )*
            loop39:
            do {
                int alt39=2;
                int LA39_0 = input.LA(1);

                if ( ((LA39_0>=STAR && LA39_0<=MODULO)) ) {
                    alt39=1;
                }


                switch (alt39) {
            	case 1 :
            	    // src/Kodkodi.g:1441:10: t= ( STAR | DIVIDE | MODULO ) e2= expr_to_int_cast
            	    {
            	    t=(Token)input.LT(1);
            	    if ( (input.LA(1)>=STAR && input.LA(1)<=MODULO) ) {
            	        input.consume();
            	        state.errorRecovery=false;
            	    }
            	    else {
            	        MismatchedSetException mse = new MismatchedSetException(null,input);
            	        throw mse;
            	    }

            	    pushFollow(FOLLOW_expr_to_int_cast_in_mult_expr2398);
            	    e2=expr_to_int_cast();

            	    state._fsp--;


            	                 node = I(t, node).compose(
            	                                 (t!=null?t.getType():0) == STAR
            	                                     ? IntOperator.MULTIPLY
            	                               : (t!=null?t.getType():0) == DIVIDE
            	                                     ? IntOperator.DIVIDE
            	                                     : IntOperator.MODULO,
            	                                 I(t, e2));
            	             

            	    }
            	    break;

            	default :
            	    break loop39;
                }
            } while (true);


            }

        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return node;
    }
    // $ANTLR end "mult_expr"


    // $ANTLR start "expr_to_int_cast"
    // src/Kodkodi.g:1450:1: expr_to_int_cast returns [Object node] : (t= ( HASH | SUM ) PAREN_LEFT e= expr PAREN_RIGHT | e1= product_expr (o= ( BAR | HAT | AMP ) e2= product_expr )* (o= OVERRIDE e2= expr_to_int_cast )? );
    public final Object expr_to_int_cast() throws RecognitionException {
        Object node = null;

        Token t=null;
        Token o=null;
        Object e = null;

        Object e1 = null;

        Object e2 = null;


        try {
            // src/Kodkodi.g:1450:39: (t= ( HASH | SUM ) PAREN_LEFT e= expr PAREN_RIGHT | e1= product_expr (o= ( BAR | HAT | AMP ) e2= product_expr )* (o= OVERRIDE e2= expr_to_int_cast )? )
            int alt42=2;
            int LA42_0 = input.LA(1);

            if ( (LA42_0==HASH||LA42_0==SUM) ) {
                alt42=1;
            }
            else if ( (LA42_0==NUM||(LA42_0>=TRUE && LA42_0<=FALSE)||(LA42_0>=UNIV && LA42_0<=UNIV_NAME)||LA42_0==RELATION_NAME||(LA42_0>=FORMULA_REG && LA42_0<=INT_EXPR_REG)||LA42_0==MINUS||(LA42_0>=OFFSET_UNIV_NAME && LA42_0<=PAREN_LEFT)||LA42_0==BRACE_LEFT||LA42_0==NONE||LA42_0==ATOM_NAME||LA42_0==STAR||LA42_0==HAT||(LA42_0>=VARIABLE_NAME && LA42_0<=INT)) ) {
                alt42=2;
            }
            else {
                NoViableAltException nvae =
                    new NoViableAltException("", 42, 0, input);

                throw nvae;
            }
            switch (alt42) {
                case 1 :
                    // src/Kodkodi.g:1451:9: t= ( HASH | SUM ) PAREN_LEFT e= expr PAREN_RIGHT
                    {
                    t=(Token)input.LT(1);
                    if ( input.LA(1)==HASH||input.LA(1)==SUM ) {
                        input.consume();
                        state.errorRecovery=false;
                    }
                    else {
                        MismatchedSetException mse = new MismatchedSetException(null,input);
                        throw mse;
                    }

                    match(input,PAREN_LEFT,FOLLOW_PAREN_LEFT_in_expr_to_int_cast2432); 
                    pushFollow(FOLLOW_expr_in_expr_to_int_cast2438);
                    e=expr();

                    state._fsp--;

                    match(input,PAREN_RIGHT,FOLLOW_PAREN_RIGHT_in_expr_to_int_cast2440); 

                                try {
                                    node = E(t, e).apply((t!=null?t.getType():0) == HASH
                                                               ? ExprCastOperator.CARDINALITY
                                                               : ExprCastOperator.SUM);
                                 } catch (IllegalArgumentException except) {
                                     huh(t, "illegal arity");
                                 }
                            

                    }
                    break;
                case 2 :
                    // src/Kodkodi.g:1460:9: e1= product_expr (o= ( BAR | HAT | AMP ) e2= product_expr )* (o= OVERRIDE e2= expr_to_int_cast )?
                    {
                    pushFollow(FOLLOW_product_expr_in_expr_to_int_cast2458);
                    e1=product_expr();

                    state._fsp--;

                     node = e1; 
                    // src/Kodkodi.g:1461:9: (o= ( BAR | HAT | AMP ) e2= product_expr )*
                    loop40:
                    do {
                        int alt40=2;
                        int LA40_0 = input.LA(1);

                        if ( (LA40_0==BAR||(LA40_0>=HAT && LA40_0<=AMP)) ) {
                            alt40=1;
                        }


                        switch (alt40) {
                    	case 1 :
                    	    // src/Kodkodi.g:1461:10: o= ( BAR | HAT | AMP ) e2= product_expr
                    	    {
                    	    o=(Token)input.LT(1);
                    	    if ( input.LA(1)==BAR||(input.LA(1)>=HAT && input.LA(1)<=AMP) ) {
                    	        input.consume();
                    	        state.errorRecovery=false;
                    	    }
                    	    else {
                    	        MismatchedSetException mse = new MismatchedSetException(null,input);
                    	        throw mse;
                    	    }

                    	    pushFollow(FOLLOW_product_expr_in_expr_to_int_cast2491);
                    	    e2=product_expr();

                    	    state._fsp--;


                    	                 if ((o!=null?o.getType():0) == BAR) {
                    	                     node = I(o, node).or(I(o, e2));
                    	                 } else if ((o!=null?o.getType():0) == HAT) {
                    	                     node = I(o, node).xor(I(o, e2));
                    	                 } else {
                    	                     if (isExpression(node)) {
                    	                         try {
                    	                             node = E(o, node).intersection(E(o, e2));
                    	                         } catch (IllegalArgumentException except) {
                    	                             int arity1 = E(o, node).arity();
                    	                             int arity2 = E(o, e2).arity();
                    	                             if (arity1 == arity2) {
                    	                                 panic();
                    	                             } else {
                    	                                 huh(o, "arity mismatch (" + arity1 + " vs. "
                    	                                         + arity2 + ")");
                    	                             }
                    	                         }
                    	                     } else if (isIntExpression(node)) {
                    	                         node = I(o, node).and(I(o, e2));
                    	                     } else {
                    	                         huh(o, "operands may not be formulas");
                    	                     }
                    	                 }
                    	            

                    	    }
                    	    break;

                    	default :
                    	    break loop40;
                        }
                    } while (true);

                    // src/Kodkodi.g:1486:13: (o= OVERRIDE e2= expr_to_int_cast )?
                    int alt41=2;
                    int LA41_0 = input.LA(1);

                    if ( (LA41_0==OVERRIDE) ) {
                        alt41=1;
                    }
                    switch (alt41) {
                        case 1 :
                            // src/Kodkodi.g:1486:14: o= OVERRIDE e2= expr_to_int_cast
                            {
                            o=(Token)match(input,OVERRIDE,FOLLOW_OVERRIDE_in_expr_to_int_cast2502); 
                            pushFollow(FOLLOW_expr_to_int_cast_in_expr_to_int_cast2508);
                            e2=expr_to_int_cast();

                            state._fsp--;


                                             try {
                                                 node = E(o, node).override(E(o, e2));
                                             } catch (IllegalArgumentException except) {
                                                 int arity1 = E(o, node).arity();
                                                 int arity2 = E(o, e2).arity();
                                                 if (arity1 == arity2) {
                                                     panic();
                                                 } else {
                                                     huh(o, "arity mismatch (" + arity1 + " vs. "
                                                             + arity2 + ")");
                                                 }
                                             }
                                         

                            }
                            break;

                    }


                    }
                    break;

            }
        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return node;
    }
    // $ANTLR end "expr_to_int_cast"


    // $ANTLR start "product_expr"
    // src/Kodkodi.g:1500:1: product_expr returns [Object node] : e1= ifno_expr (a= ARROW es= product_expr_tail[$a,\n new SingletonArrayList<Expression>(E($a, $node))] )? ;
    public final Object product_expr() throws RecognitionException {
        Object node = null;

        Token a=null;
        Object e1 = null;

        Object es = null;


        try {
            // src/Kodkodi.g:1500:35: (e1= ifno_expr (a= ARROW es= product_expr_tail[$a,\n new SingletonArrayList<Expression>(E($a, $node))] )? )
            // src/Kodkodi.g:1501:9: e1= ifno_expr (a= ARROW es= product_expr_tail[$a,\n new SingletonArrayList<Expression>(E($a, $node))] )?
            {
            pushFollow(FOLLOW_ifno_expr_in_product_expr2534);
            e1=ifno_expr();

            state._fsp--;

             node = e1; 
            // src/Kodkodi.g:1502:9: (a= ARROW es= product_expr_tail[$a,\n new SingletonArrayList<Expression>(E($a, $node))] )?
            int alt43=2;
            int LA43_0 = input.LA(1);

            if ( (LA43_0==ARROW) ) {
                int LA43_1 = input.LA(2);

                if ( (LA43_1==NUM||(LA43_1>=TRUE && LA43_1<=FALSE)||(LA43_1>=UNIV && LA43_1<=UNIV_NAME)||LA43_1==RELATION_NAME||(LA43_1>=FORMULA_REG && LA43_1<=INT_EXPR_REG)||LA43_1==MINUS||(LA43_1>=OFFSET_UNIV_NAME && LA43_1<=PAREN_LEFT)||LA43_1==BRACE_LEFT||LA43_1==NONE||LA43_1==ATOM_NAME||LA43_1==STAR||LA43_1==HAT||(LA43_1>=VARIABLE_NAME && LA43_1<=INT)) ) {
                    alt43=1;
                }
            }
            switch (alt43) {
                case 1 :
                    // src/Kodkodi.g:1502:10: a= ARROW es= product_expr_tail[$a,\n new SingletonArrayList<Expression>(E($a, $node))]
                    {
                    a=(Token)match(input,ARROW,FOLLOW_ARROW_in_product_expr2551); 
                    pushFollow(FOLLOW_product_expr_tail_in_product_expr2557);
                    es=product_expr_tail(a, new SingletonArrayList<Expression>(E(a, node)));

                    state._fsp--;

                     node = es; 

                    }
                    break;

            }


            }

        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return node;
    }
    // $ANTLR end "product_expr"


    // $ANTLR start "product_expr_tail"
    // src/Kodkodi.g:1505:1: product_expr_tail[Token token, List<Expression> exprs] returns [Object node] : e1= ifno_expr ( ARROW es= product_expr_tail[$token, $exprs] | ) ;
    public final Object product_expr_tail(Token token, List<Expression> exprs) throws RecognitionException {
        Object node = null;

        Object e1 = null;

        Object es = null;


        try {
            // src/Kodkodi.g:1505:77: (e1= ifno_expr ( ARROW es= product_expr_tail[$token, $exprs] | ) )
            // src/Kodkodi.g:1506:9: e1= ifno_expr ( ARROW es= product_expr_tail[$token, $exprs] | )
            {
            pushFollow(FOLLOW_ifno_expr_in_product_expr_tail2598);
            e1=ifno_expr();

            state._fsp--;

             exprs.add(E(token, e1)); 
            // src/Kodkodi.g:1507:9: ( ARROW es= product_expr_tail[$token, $exprs] | )
            int alt44=2;
            int LA44_0 = input.LA(1);

            if ( (LA44_0==ARROW) ) {
                int LA44_1 = input.LA(2);

                if ( ((LA44_1>=ONE && LA44_1<=LONE)) ) {
                    alt44=2;
                }
                else if ( (LA44_1==NUM||(LA44_1>=TRUE && LA44_1<=FALSE)||(LA44_1>=UNIV && LA44_1<=UNIV_NAME)||LA44_1==RELATION_NAME||(LA44_1>=FORMULA_REG && LA44_1<=INT_EXPR_REG)||LA44_1==MINUS||(LA44_1>=OFFSET_UNIV_NAME && LA44_1<=PAREN_LEFT)||LA44_1==BRACE_LEFT||LA44_1==NONE||LA44_1==ATOM_NAME||LA44_1==STAR||LA44_1==HAT||(LA44_1>=VARIABLE_NAME && LA44_1<=INT)) ) {
                    alt44=1;
                }
                else {
                    NoViableAltException nvae =
                        new NoViableAltException("", 44, 1, input);

                    throw nvae;
                }
            }
            else if ( (LA44_0==COMMA||LA44_0==BRACKET_RIGHT||(LA44_0>=FORMULA_REG && LA44_0<=AND)||LA44_0==PAREN_RIGHT||LA44_0==BRACE_RIGHT||LA44_0==BAR||(LA44_0>=THEN && LA44_0<=IMPLIES)||(LA44_0>=EQ && LA44_0<=IN)||(LA44_0>=SHL && LA44_0<=OVERRIDE)) ) {
                alt44=2;
            }
            else {
                NoViableAltException nvae =
                    new NoViableAltException("", 44, 0, input);

                throw nvae;
            }
            switch (alt44) {
                case 1 :
                    // src/Kodkodi.g:1507:10: ARROW es= product_expr_tail[$token, $exprs]
                    {
                    match(input,ARROW,FOLLOW_ARROW_in_product_expr_tail2611); 
                    pushFollow(FOLLOW_product_expr_tail_in_product_expr_tail2617);
                    es=product_expr_tail(token, exprs);

                    state._fsp--;

                     node = es; 

                    }
                    break;
                case 2 :
                    // src/Kodkodi.g:1508:10: 
                    {
                     node = Expression.product(exprs); 

                    }
                    break;

            }


            }

        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return node;
    }
    // $ANTLR end "product_expr_tail"


    // $ANTLR start "ifno_expr"
    // src/Kodkodi.g:1509:1: ifno_expr returns [Object node] : e1= apply_expr (a= IFNO e2= apply_expr )* ;
    public final Object ifno_expr() throws RecognitionException {
        Object node = null;

        Token a=null;
        Object e1 = null;

        Object e2 = null;


        try {
            // src/Kodkodi.g:1509:32: (e1= apply_expr (a= IFNO e2= apply_expr )* )
            // src/Kodkodi.g:1510:9: e1= apply_expr (a= IFNO e2= apply_expr )*
            {
            pushFollow(FOLLOW_apply_expr_in_ifno_expr2656);
            e1=apply_expr();

            state._fsp--;

             node = e1; 
            // src/Kodkodi.g:1511:9: (a= IFNO e2= apply_expr )*
            loop45:
            do {
                int alt45=2;
                int LA45_0 = input.LA(1);

                if ( (LA45_0==IFNO) ) {
                    alt45=1;
                }


                switch (alt45) {
            	case 1 :
            	    // src/Kodkodi.g:1511:10: a= IFNO e2= apply_expr
            	    {
            	    a=(Token)match(input,IFNO,FOLLOW_IFNO_in_ifno_expr2673); 
            	    pushFollow(FOLLOW_apply_expr_in_ifno_expr2679);
            	    e2=apply_expr();

            	    state._fsp--;


            	                 node = E(a, node).no().thenElse(E(a, e2), E(a, node));
            	             

            	    }
            	    break;

            	default :
            	    break loop45;
                }
            } while (true);


            }

        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return node;
    }
    // $ANTLR end "ifno_expr"


    // $ANTLR start "apply_expr"
    // src/Kodkodi.g:1514:1: apply_expr returns [Object node] : e1= project_expr (p= PAREN_LEFT e2= expr ( COMMA e3= expr )* PAREN_RIGHT )* ;
    public final Object apply_expr() throws RecognitionException {
        Object node = null;

        Token p=null;
        Object e1 = null;

        Object e2 = null;

        Object e3 = null;


        try {
            // src/Kodkodi.g:1514:33: (e1= project_expr (p= PAREN_LEFT e2= expr ( COMMA e3= expr )* PAREN_RIGHT )* )
            // src/Kodkodi.g:1515:9: e1= project_expr (p= PAREN_LEFT e2= expr ( COMMA e3= expr )* PAREN_RIGHT )*
            {
            pushFollow(FOLLOW_project_expr_in_apply_expr2705);
            e1=project_expr();

            state._fsp--;

             node = e1; 
            // src/Kodkodi.g:1516:9: (p= PAREN_LEFT e2= expr ( COMMA e3= expr )* PAREN_RIGHT )*
            loop47:
            do {
                int alt47=2;
                int LA47_0 = input.LA(1);

                if ( (LA47_0==PAREN_LEFT) ) {
                    alt47=1;
                }


                switch (alt47) {
            	case 1 :
            	    // src/Kodkodi.g:1516:10: p= PAREN_LEFT e2= expr ( COMMA e3= expr )* PAREN_RIGHT
            	    {
            	    p=(Token)match(input,PAREN_LEFT,FOLLOW_PAREN_LEFT_in_apply_expr2722); 
            	    pushFollow(FOLLOW_expr_in_apply_expr2728);
            	    e2=expr();

            	    state._fsp--;


            	                 try {
            	                     node = E(p, e2).join(E(p, node));
            	                 } catch (IllegalArgumentException except) {
            	                     int arity1 = E(p, node).arity();
            	                     int arity2 = E(p, e2).arity();
            	                     if (arity1 + arity2 > 2) {
            	                         panic();
            	                     } else {
            	                         huh(p, "illegal arities (1 and 1)");
            	                     }
            	                 }
            	             
            	    // src/Kodkodi.g:1528:12: ( COMMA e3= expr )*
            	    loop46:
            	    do {
            	        int alt46=2;
            	        int LA46_0 = input.LA(1);

            	        if ( (LA46_0==COMMA) ) {
            	            alt46=1;
            	        }


            	        switch (alt46) {
            	    	case 1 :
            	    	    // src/Kodkodi.g:1528:13: COMMA e3= expr
            	    	    {
            	    	    match(input,COMMA,FOLLOW_COMMA_in_apply_expr2733); 
            	    	    pushFollow(FOLLOW_expr_in_apply_expr2739);
            	    	    e3=expr();

            	    	    state._fsp--;


            	    	                 try {
            	    	                     node = E(p, e3).join(E(p, node));
            	    	                 } catch (IllegalArgumentException except) {
            	    	                     int arity1 = E(p, node).arity();
            	    	                     int arity2 = E(p, e3).arity();
            	    	                     if (arity1 + arity2 > 2) {
            	    	                         panic();
            	    	                     } else {
            	    	                         huh(p, "illegal arities (1 and 1)");
            	    	                     }
            	    	                 }
            	    	             

            	    	    }
            	    	    break;

            	    	default :
            	    	    break loop46;
            	        }
            	    } while (true);

            	    match(input,PAREN_RIGHT,FOLLOW_PAREN_RIGHT_in_apply_expr2745); 

            	    }
            	    break;

            	default :
            	    break loop47;
                }
            } while (true);


            }

        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return node;
    }
    // $ANTLR end "apply_expr"


    // $ANTLR start "project_expr"
    // src/Kodkodi.g:1541:1: project_expr returns [Object node] : e1= basic_expr (d= DOT e2= basic_expr )* (cs= project_columns )* ;
    public final Object project_expr() throws RecognitionException {
        Object node = null;

        Token d=null;
        Object e1 = null;

        Object e2 = null;

        KodkodiParser.project_columns_return cs = null;


        try {
            // src/Kodkodi.g:1541:35: (e1= basic_expr (d= DOT e2= basic_expr )* (cs= project_columns )* )
            // src/Kodkodi.g:1542:9: e1= basic_expr (d= DOT e2= basic_expr )* (cs= project_columns )*
            {
            pushFollow(FOLLOW_basic_expr_in_project_expr2769);
            e1=basic_expr();

            state._fsp--;

             node = e1; 
            // src/Kodkodi.g:1543:9: (d= DOT e2= basic_expr )*
            loop48:
            do {
                int alt48=2;
                int LA48_0 = input.LA(1);

                if ( (LA48_0==DOT) ) {
                    alt48=1;
                }


                switch (alt48) {
            	case 1 :
            	    // src/Kodkodi.g:1543:10: d= DOT e2= basic_expr
            	    {
            	    d=(Token)match(input,DOT,FOLLOW_DOT_in_project_expr2786); 
            	    pushFollow(FOLLOW_basic_expr_in_project_expr2792);
            	    e2=basic_expr();

            	    state._fsp--;


            	                 try {
            	                     node = E(d, node).join(E(d, e2));
            	                 } catch (IllegalArgumentException except) {
            	                     int arity1 = E(d, node).arity();
            	                     int arity2 = E(d, e2).arity();
            	                     if (arity1 + arity2 > 2) {
            	                         panic();
            	                     } else {
            	                         huh(d, "illegal arities (1 and 1)");
            	                     }
            	                 }
            	             

            	    }
            	    break;

            	default :
            	    break loop48;
                }
            } while (true);

            // src/Kodkodi.g:1555:14: (cs= project_columns )*
            loop49:
            do {
                int alt49=2;
                int LA49_0 = input.LA(1);

                if ( (LA49_0==BRACKET_LEFT) ) {
                    alt49=1;
                }


                switch (alt49) {
            	case 1 :
            	    // src/Kodkodi.g:1555:15: cs= project_columns
            	    {
            	    pushFollow(FOLLOW_project_columns_in_project_expr2803);
            	    cs=project_columns();

            	    state._fsp--;


            	                      IntExpression[] array = new IntExpression[(cs!=null?cs.nodes:null).size()];
            	                      (cs!=null?cs.nodes:null).toArray(array);
            	                      node = E((cs!=null?cs.token:null), node).project(array);
            	                  

            	    }
            	    break;

            	default :
            	    break loop49;
                }
            } while (true);


            }

        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return node;
    }
    // $ANTLR end "project_expr"


    // $ANTLR start "basic_expr"
    // src/Kodkodi.g:1560:1: basic_expr returns [Object node] : ( PAREN_LEFT e= expr PAREN_RIGHT | n= ( ATOM_NAME | UNIV_NAME | OFFSET_UNIV_NAME | RELATION_NAME ) | n= VARIABLE_NAME | r= FORMULA_REG | r= REL_EXPR_REG | r= INT_EXPR_REG | n= NUM | FALSE | TRUE | IDEN | INTS | NONE | UNIV | t= ( HAT | STAR | TILDE | ABS | SGN | MINUS ) e= basic_expr | BRACE_LEFT ds= decls t= BAR e= expr BRACE_RIGHT | t= ( BITS | INT ) BRACKET_LEFT e= expr BRACKET_RIGHT );
    public final Object basic_expr() throws RecognitionException {
        Object node = null;

        Token n=null;
        Token r=null;
        Token t=null;
        Object e = null;

        Decls ds = null;


        try {
            // src/Kodkodi.g:1560:33: ( PAREN_LEFT e= expr PAREN_RIGHT | n= ( ATOM_NAME | UNIV_NAME | OFFSET_UNIV_NAME | RELATION_NAME ) | n= VARIABLE_NAME | r= FORMULA_REG | r= REL_EXPR_REG | r= INT_EXPR_REG | n= NUM | FALSE | TRUE | IDEN | INTS | NONE | UNIV | t= ( HAT | STAR | TILDE | ABS | SGN | MINUS ) e= basic_expr | BRACE_LEFT ds= decls t= BAR e= expr BRACE_RIGHT | t= ( BITS | INT ) BRACKET_LEFT e= expr BRACKET_RIGHT )
            int alt50=16;
            switch ( input.LA(1) ) {
            case PAREN_LEFT:
                {
                alt50=1;
                }
                break;
            case UNIV_NAME:
            case RELATION_NAME:
            case OFFSET_UNIV_NAME:
            case ATOM_NAME:
                {
                alt50=2;
                }
                break;
            case VARIABLE_NAME:
                {
                alt50=3;
                }
                break;
            case FORMULA_REG:
                {
                alt50=4;
                }
                break;
            case REL_EXPR_REG:
                {
                alt50=5;
                }
                break;
            case INT_EXPR_REG:
                {
                alt50=6;
                }
                break;
            case NUM:
                {
                alt50=7;
                }
                break;
            case FALSE:
                {
                alt50=8;
                }
                break;
            case TRUE:
                {
                alt50=9;
                }
                break;
            case IDEN:
                {
                alt50=10;
                }
                break;
            case INTS:
                {
                alt50=11;
                }
                break;
            case NONE:
                {
                alt50=12;
                }
                break;
            case UNIV:
                {
                alt50=13;
                }
                break;
            case MINUS:
            case STAR:
            case HAT:
            case TILDE:
            case ABS:
            case SGN:
                {
                alt50=14;
                }
                break;
            case BRACE_LEFT:
                {
                alt50=15;
                }
                break;
            case BITS:
            case INT:
                {
                alt50=16;
                }
                break;
            default:
                NoViableAltException nvae =
                    new NoViableAltException("", 50, 0, input);

                throw nvae;
            }

            switch (alt50) {
                case 1 :
                    // src/Kodkodi.g:1561:9: PAREN_LEFT e= expr PAREN_RIGHT
                    {
                    match(input,PAREN_LEFT,FOLLOW_PAREN_LEFT_in_basic_expr2825); 
                    pushFollow(FOLLOW_expr_in_basic_expr2831);
                    e=expr();

                    state._fsp--;

                    match(input,PAREN_RIGHT,FOLLOW_PAREN_RIGHT_in_basic_expr2833); 
                     node = e; 

                    }
                    break;
                case 2 :
                    // src/Kodkodi.g:1562:9: n= ( ATOM_NAME | UNIV_NAME | OFFSET_UNIV_NAME | RELATION_NAME )
                    {
                    n=(Token)input.LT(1);
                    if ( input.LA(1)==UNIV_NAME||input.LA(1)==RELATION_NAME||input.LA(1)==OFFSET_UNIV_NAME||input.LA(1)==ATOM_NAME ) {
                        input.consume();
                        state.errorRecovery=false;
                    }
                    else {
                        MismatchedSetException mse = new MismatchedSetException(null,input);
                        throw mse;
                    }

                     node = getRelation(n); 

                    }
                    break;
                case 3 :
                    // src/Kodkodi.g:1564:9: n= VARIABLE_NAME
                    {
                    n=(Token)match(input,VARIABLE_NAME,FOLLOW_VARIABLE_NAME_in_basic_expr2895); 
                     node = getVariable(n); 

                    }
                    break;
                case 4 :
                    // src/Kodkodi.g:1565:9: r= FORMULA_REG
                    {
                    r=(Token)match(input,FORMULA_REG,FOLLOW_FORMULA_REG_in_basic_expr2913); 
                     node = getFormulaReg(r); 

                    }
                    break;
                case 5 :
                    // src/Kodkodi.g:1566:9: r= REL_EXPR_REG
                    {
                    r=(Token)match(input,REL_EXPR_REG,FOLLOW_REL_EXPR_REG_in_basic_expr2931); 
                     node = getExprReg(r); 

                    }
                    break;
                case 6 :
                    // src/Kodkodi.g:1567:9: r= INT_EXPR_REG
                    {
                    r=(Token)match(input,INT_EXPR_REG,FOLLOW_INT_EXPR_REG_in_basic_expr2949); 
                     node = getIntExprReg(r); 

                    }
                    break;
                case 7 :
                    // src/Kodkodi.g:1568:9: n= NUM
                    {
                    n=(Token)match(input,NUM,FOLLOW_NUM_in_basic_expr2967); 
                     node = getIntConstant(n); 

                    }
                    break;
                case 8 :
                    // src/Kodkodi.g:1569:9: FALSE
                    {
                    match(input,FALSE,FOLLOW_FALSE_in_basic_expr2981); 
                     node = Formula.FALSE; 

                    }
                    break;
                case 9 :
                    // src/Kodkodi.g:1570:9: TRUE
                    {
                    match(input,TRUE,FOLLOW_TRUE_in_basic_expr2995); 
                     node = Formula.TRUE; 

                    }
                    break;
                case 10 :
                    // src/Kodkodi.g:1571:9: IDEN
                    {
                    match(input,IDEN,FOLLOW_IDEN_in_basic_expr3009); 
                     node = Expression.IDEN; 

                    }
                    break;
                case 11 :
                    // src/Kodkodi.g:1572:9: INTS
                    {
                    match(input,INTS,FOLLOW_INTS_in_basic_expr3023); 
                     node = Expression.INTS; 

                    }
                    break;
                case 12 :
                    // src/Kodkodi.g:1573:9: NONE
                    {
                    match(input,NONE,FOLLOW_NONE_in_basic_expr3037); 
                     node = Expression.NONE; 

                    }
                    break;
                case 13 :
                    // src/Kodkodi.g:1574:9: UNIV
                    {
                    match(input,UNIV,FOLLOW_UNIV_in_basic_expr3051); 
                     node = Expression.UNIV; 

                    }
                    break;
                case 14 :
                    // src/Kodkodi.g:1575:9: t= ( HAT | STAR | TILDE | ABS | SGN | MINUS ) e= basic_expr
                    {
                    t=(Token)input.LT(1);
                    if ( input.LA(1)==MINUS||input.LA(1)==STAR||input.LA(1)==HAT||(input.LA(1)>=TILDE && input.LA(1)<=SGN) ) {
                        input.consume();
                        state.errorRecovery=false;
                    }
                    else {
                        MismatchedSetException mse = new MismatchedSetException(null,input);
                        throw mse;
                    }

                    pushFollow(FOLLOW_basic_expr_in_basic_expr3097);
                    e=basic_expr();

                    state._fsp--;


                                if (isExpression(e)) {
                                    try {
                                        if ((t!=null?t.getType():0) == HAT) {
                                            node = E(t, e).closure();
                                        } else if ((t!=null?t.getType():0) == STAR) {
                                            node = E(t, e).reflexiveClosure();
                                        } else if ((t!=null?t.getType():0) == TILDE) {
                                            node = E(t, e).transpose();
                                        } else {
                                            huh(t, "mismatched input '" + (t!=null?t.getText():null)
                                                    + "' expecting HAT, STAR, or TILDE");
                                            node = Formula.FALSE;
                                        }
                                    } catch (IllegalArgumentException except) {
                                        if (E(t, e).arity() == 2) {
                                            panic();
                                        } else {
                                            huh(t, "illegal arity");
                                        }
                                    }
                                } else if (isIntExpression(e)) {
                                    if ((t!=null?t.getType():0) == ABS) {
                                        node = I(t, e).abs();
                                    } else if ((t!=null?t.getType():0) == SGN) {
                                        node = I(t, e).signum();
                                    } else if ((t!=null?t.getType():0) == MINUS) {
                                        node = I(t, e).negate();
                                    } else if ((t!=null?t.getType():0) == TILDE) {
                                        node = I(t, e).not(); 
                                    } else {
                                        huh(t, "mismatched input '" + (t!=null?t.getText():null)
                                                + "' expecting ABS, SGN, MINUS, or TILDE");
                                        node = IntConstant.constant(0);
                                    }
                                } else {
                                    huh(t, "operands may not be formulas");
                                }
                            

                    }
                    break;
                case 15 :
                    // src/Kodkodi.g:1614:9: BRACE_LEFT ds= decls t= BAR e= expr BRACE_RIGHT
                    {
                    match(input,BRACE_LEFT,FOLLOW_BRACE_LEFT_in_basic_expr3111); 
                    pushFollow(FOLLOW_decls_in_basic_expr3117);
                    ds=decls();

                    state._fsp--;

                    t=(Token)match(input,BAR,FOLLOW_BAR_in_basic_expr3123); 
                    pushFollow(FOLLOW_expr_in_basic_expr3129);
                    e=expr();

                    state._fsp--;

                    match(input,BRACE_RIGHT,FOLLOW_BRACE_RIGHT_in_basic_expr3131); 

                                try {
                                    node = F(t, e).comprehension(ds);
                                } catch (IllegalArgumentException except) {
                                    huh(t, "expected declarations with multiplicity 'one'");
                                } catch (NullPointerException except) {
                                    huh(t, "expected at least one declaration");
                                }
                            

                    }
                    break;
                case 16 :
                    // src/Kodkodi.g:1623:9: t= ( BITS | INT ) BRACKET_LEFT e= expr BRACKET_RIGHT
                    {
                    t=(Token)input.LT(1);
                    if ( (input.LA(1)>=BITS && input.LA(1)<=INT) ) {
                        input.consume();
                        state.errorRecovery=false;
                    }
                    else {
                        MismatchedSetException mse = new MismatchedSetException(null,input);
                        throw mse;
                    }

                    match(input,BRACKET_LEFT,FOLLOW_BRACKET_LEFT_in_basic_expr3157); 
                    pushFollow(FOLLOW_expr_in_basic_expr3163);
                    e=expr();

                    state._fsp--;

                    match(input,BRACKET_RIGHT,FOLLOW_BRACKET_RIGHT_in_basic_expr3165); 

                                node = I(t, e).cast((t!=null?t.getType():0) == BITS
                                                          ? IntCastOperator.BITSETCAST
                                                          : IntCastOperator.INTCAST);
                            

                    }
                    break;

            }
        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return node;
    }
    // $ANTLR end "basic_expr"


    // $ANTLR start "decls"
    // src/Kodkodi.g:1629:1: decls returns [Decls nodes = null] : BRACKET_LEFT (d1= decl ( COMMA d2= decl )* )? BRACKET_RIGHT ;
    public final Decls decls() throws RecognitionException {
        Decls nodes =  null;

        Decl d1 = null;

        Decl d2 = null;


        try {
            // src/Kodkodi.g:1629:35: ( BRACKET_LEFT (d1= decl ( COMMA d2= decl )* )? BRACKET_RIGHT )
            // src/Kodkodi.g:1630:9: BRACKET_LEFT (d1= decl ( COMMA d2= decl )* )? BRACKET_RIGHT
            {
            match(input,BRACKET_LEFT,FOLLOW_BRACKET_LEFT_in_decls3186); 
            // src/Kodkodi.g:1631:9: (d1= decl ( COMMA d2= decl )* )?
            int alt52=2;
            int LA52_0 = input.LA(1);

            if ( (LA52_0==VARIABLE_NAME) ) {
                alt52=1;
            }
            switch (alt52) {
                case 1 :
                    // src/Kodkodi.g:1631:10: d1= decl ( COMMA d2= decl )*
                    {
                    pushFollow(FOLLOW_decl_in_decls3201);
                    d1=decl();

                    state._fsp--;

                     nodes = d1; 
                    // src/Kodkodi.g:1632:10: ( COMMA d2= decl )*
                    loop51:
                    do {
                        int alt51=2;
                        int LA51_0 = input.LA(1);

                        if ( (LA51_0==COMMA) ) {
                            alt51=1;
                        }


                        switch (alt51) {
                    	case 1 :
                    	    // src/Kodkodi.g:1632:11: COMMA d2= decl
                    	    {
                    	    match(input,COMMA,FOLLOW_COMMA_in_decls3215); 
                    	    pushFollow(FOLLOW_decl_in_decls3221);
                    	    d2=decl();

                    	    state._fsp--;

                    	     nodes = nodes.and(d2); 

                    	    }
                    	    break;

                    	default :
                    	    break loop51;
                        }
                    } while (true);


                    }
                    break;

            }

            match(input,BRACKET_RIGHT,FOLLOW_BRACKET_RIGHT_in_decls3237); 

            }

        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return nodes;
    }
    // $ANTLR end "decls"


    // $ANTLR start "decl"
    // src/Kodkodi.g:1634:1: decl returns [Decl node] : n= VARIABLE_NAME c= COLON m= multiplicity e= expr ;
    public final Decl decl() throws RecognitionException {
        Decl node = null;

        Token n=null;
        Token c=null;
        KodkodiParser.multiplicity_return m = null;

        Object e = null;


        try {
            // src/Kodkodi.g:1634:25: (n= VARIABLE_NAME c= COLON m= multiplicity e= expr )
            // src/Kodkodi.g:1635:9: n= VARIABLE_NAME c= COLON m= multiplicity e= expr
            {
            n=(Token)match(input,VARIABLE_NAME,FOLLOW_VARIABLE_NAME_in_decl3259); 
            c=(Token)match(input,COLON,FOLLOW_COLON_in_decl3265); 
            pushFollow(FOLLOW_multiplicity_in_decl3271);
            m=multiplicity();

            state._fsp--;

            pushFollow(FOLLOW_expr_in_decl3277);
            e=expr();

            state._fsp--;


                        try {
                            node = getVariable(n).declare((m!=null?m.value:null), E(c, e));
                        } catch (IllegalArgumentException except) {
                            huh(c, "invalid bound: " + fixedMessage(except));
                        }
                    

            }

        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return node;
    }
    // $ANTLR end "decl"

    public static class assigns_return extends ParserRuleReturnScope {
        public Vector<Token> tokens = new Vector<Token>();
        public Vector<Node> oldNodes = new Vector<Node>();
        public Vector<Node> newNodes = new Vector<Node>();
    };

    // $ANTLR start "assigns"
    // src/Kodkodi.g:1642:1: assigns returns [Vector<Token> tokens = new Vector<Token>(),\n Vector<Node> oldNodes = new Vector<Node>(),\n Vector<Node> newNodes = new Vector<Node>()] : BRACKET_LEFT assign[$tokens, $oldNodes, $newNodes] ( COMMA assign[$tokens, $oldNodes, $newNodes] )* BRACKET_RIGHT ;
    public final KodkodiParser.assigns_return assigns() throws RecognitionException {
        KodkodiParser.assigns_return retval = new KodkodiParser.assigns_return();
        retval.start = input.LT(1);

        try {
            // src/Kodkodi.g:1644:61: ( BRACKET_LEFT assign[$tokens, $oldNodes, $newNodes] ( COMMA assign[$tokens, $oldNodes, $newNodes] )* BRACKET_RIGHT )
            // src/Kodkodi.g:1645:9: BRACKET_LEFT assign[$tokens, $oldNodes, $newNodes] ( COMMA assign[$tokens, $oldNodes, $newNodes] )* BRACKET_RIGHT
            {
            match(input,BRACKET_LEFT,FOLLOW_BRACKET_LEFT_in_assigns3297); 
            pushFollow(FOLLOW_assign_in_assigns3299);
            assign(retval.tokens, retval.oldNodes, retval.newNodes);

            state._fsp--;

            // src/Kodkodi.g:1646:9: ( COMMA assign[$tokens, $oldNodes, $newNodes] )*
            loop53:
            do {
                int alt53=2;
                int LA53_0 = input.LA(1);

                if ( (LA53_0==COMMA) ) {
                    alt53=1;
                }


                switch (alt53) {
            	case 1 :
            	    // src/Kodkodi.g:1646:10: COMMA assign[$tokens, $oldNodes, $newNodes]
            	    {
            	    match(input,COMMA,FOLLOW_COMMA_in_assigns3311); 
            	    pushFollow(FOLLOW_assign_in_assigns3313);
            	    assign(retval.tokens, retval.oldNodes, retval.newNodes);

            	    state._fsp--;


            	    }
            	    break;

            	default :
            	    break loop53;
                }
            } while (true);

            match(input,BRACKET_RIGHT,FOLLOW_BRACKET_RIGHT_in_assigns3318); 

                        for (int i = 0; i < retval.tokens.size(); ++i)
                            setReg(retval.tokens.elementAt(i), retval.newNodes.elementAt(i));
                    

            }

            retval.stop = input.LT(-1);

        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return retval;
    }
    // $ANTLR end "assigns"


    // $ANTLR start "assign"
    // src/Kodkodi.g:1650:1: assign[Vector<Token> tokens, Vector<Node> oldNodes, Vector<Node> newNodes] : (r= FORMULA_REG c= COLON_EQ e= expr | r= REL_EXPR_REG c= COLON_EQ e= expr | r= INT_EXPR_REG c= COLON_EQ e= expr );
    public final void assign(Vector<Token> tokens, Vector<Node> oldNodes, Vector<Node> newNodes) throws RecognitionException {
        Token r=null;
        Token c=null;
        Object e = null;


        try {
            // src/Kodkodi.g:1650:75: (r= FORMULA_REG c= COLON_EQ e= expr | r= REL_EXPR_REG c= COLON_EQ e= expr | r= INT_EXPR_REG c= COLON_EQ e= expr )
            int alt54=3;
            switch ( input.LA(1) ) {
            case FORMULA_REG:
                {
                alt54=1;
                }
                break;
            case REL_EXPR_REG:
                {
                alt54=2;
                }
                break;
            case INT_EXPR_REG:
                {
                alt54=3;
                }
                break;
            default:
                NoViableAltException nvae =
                    new NoViableAltException("", 54, 0, input);

                throw nvae;
            }

            switch (alt54) {
                case 1 :
                    // src/Kodkodi.g:1651:9: r= FORMULA_REG c= COLON_EQ e= expr
                    {
                    r=(Token)match(input,FORMULA_REG,FOLLOW_FORMULA_REG_in_assign3339); 
                    c=(Token)match(input,COLON_EQ,FOLLOW_COLON_EQ_in_assign3345); 
                    pushFollow(FOLLOW_expr_in_assign3351);
                    e=expr();

                    state._fsp--;


                                int id = id((r!=null?r.getText():null));
                                tokens.add(r);
                                oldNodes.add(id < formulas.size() ? formulas.elementAt(id) : null);
                                newNodes.add(F(c, e));
                            

                    }
                    break;
                case 2 :
                    // src/Kodkodi.g:1657:9: r= REL_EXPR_REG c= COLON_EQ e= expr
                    {
                    r=(Token)match(input,REL_EXPR_REG,FOLLOW_REL_EXPR_REG_in_assign3369); 
                    c=(Token)match(input,COLON_EQ,FOLLOW_COLON_EQ_in_assign3375); 
                    pushFollow(FOLLOW_expr_in_assign3381);
                    e=expr();

                    state._fsp--;


                                int id = id((r!=null?r.getText():null));
                                tokens.add(r);
                                oldNodes.add(id < exprs.size() ? exprs.elementAt(id) : null);
                                newNodes.add(E(c, e));
                            

                    }
                    break;
                case 3 :
                    // src/Kodkodi.g:1663:9: r= INT_EXPR_REG c= COLON_EQ e= expr
                    {
                    r=(Token)match(input,INT_EXPR_REG,FOLLOW_INT_EXPR_REG_in_assign3399); 
                    c=(Token)match(input,COLON_EQ,FOLLOW_COLON_EQ_in_assign3405); 
                    pushFollow(FOLLOW_expr_in_assign3411);
                    e=expr();

                    state._fsp--;


                                int id = id((r!=null?r.getText():null));
                                tokens.add(r);
                                oldNodes.add(id < intExprs.size() ? intExprs.elementAt(id) : null);
                                newNodes.add(I(c, e));
                            

                    }
                    break;

            }
        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return ;
    }
    // $ANTLR end "assign"

    public static class multiplicity_return extends ParserRuleReturnScope {
        public Token token;
        public Multiplicity value;
    };

    // $ANTLR start "multiplicity"
    // src/Kodkodi.g:1669:1: multiplicity returns [Token token, Multiplicity value] : t= ( NO | LONE | ONE | SOME | SET ) ;
    public final KodkodiParser.multiplicity_return multiplicity() throws RecognitionException {
        KodkodiParser.multiplicity_return retval = new KodkodiParser.multiplicity_return();
        retval.start = input.LT(1);

        Token t=null;

        try {
            // src/Kodkodi.g:1669:55: (t= ( NO | LONE | ONE | SOME | SET ) )
            // src/Kodkodi.g:1670:9: t= ( NO | LONE | ONE | SOME | SET )
            {
            t=(Token)input.LT(1);
            if ( input.LA(1)==SOME||(input.LA(1)>=ONE && input.LA(1)<=LONE)||(input.LA(1)>=NO && input.LA(1)<=SET) ) {
                input.consume();
                state.errorRecovery=false;
            }
            else {
                MismatchedSetException mse = new MismatchedSetException(null,input);
                throw mse;
            }


                        retval.token = t;
                        switch ((t!=null?t.getType():0)) {
                        case NO:
                            retval.value = Multiplicity.NO;
                            break;
                        case LONE:
                            retval.value = Multiplicity.LONE;
                            break;
                        case ONE:
                            retval.value = Multiplicity.ONE;
                            break;
                        case SOME:
                            retval.value = Multiplicity.SOME;
                            break;
                        case SET:
                            retval.value = Multiplicity.SET;
                        }
                    

            }

            retval.stop = input.LT(-1);

        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return retval;
    }
    // $ANTLR end "multiplicity"

    public static class project_columns_return extends ParserRuleReturnScope {
        public Token token;
        public Vector<IntExpression> nodes = new Vector<IntExpression>();
    };

    // $ANTLR start "project_columns"
    // src/Kodkodi.g:1689:1: project_columns returns [Token token,\n Vector<IntExpression> nodes = new Vector<IntExpression>()] : t= BRACKET_LEFT e1= expr (c= COMMA e2= expr )* BRACKET_RIGHT ;
    public final KodkodiParser.project_columns_return project_columns() throws RecognitionException {
        KodkodiParser.project_columns_return retval = new KodkodiParser.project_columns_return();
        retval.start = input.LT(1);

        Token t=null;
        Token c=null;
        Object e1 = null;

        Object e2 = null;


        try {
            // src/Kodkodi.g:1691:75: (t= BRACKET_LEFT e1= expr (c= COMMA e2= expr )* BRACKET_RIGHT )
            // src/Kodkodi.g:1692:9: t= BRACKET_LEFT e1= expr (c= COMMA e2= expr )* BRACKET_RIGHT
            {
            t=(Token)match(input,BRACKET_LEFT,FOLLOW_BRACKET_LEFT_in_project_columns3484); 
            pushFollow(FOLLOW_expr_in_project_columns3490);
            e1=expr();

            state._fsp--;


                        retval.token = t;
                        retval.nodes.add(I(t, e1));
                    
            // src/Kodkodi.g:1695:11: (c= COMMA e2= expr )*
            loop55:
            do {
                int alt55=2;
                int LA55_0 = input.LA(1);

                if ( (LA55_0==COMMA) ) {
                    alt55=1;
                }


                switch (alt55) {
            	case 1 :
            	    // src/Kodkodi.g:1695:12: c= COMMA e2= expr
            	    {
            	    c=(Token)match(input,COMMA,FOLLOW_COMMA_in_project_columns3499); 
            	    pushFollow(FOLLOW_expr_in_project_columns3505);
            	    e2=expr();

            	    state._fsp--;

            	     retval.nodes.add(I(c, e2)); 

            	    }
            	    break;

            	default :
            	    break loop55;
                }
            } while (true);

            match(input,BRACKET_RIGHT,FOLLOW_BRACKET_RIGHT_in_project_columns3519); 

            }

            retval.stop = input.LT(-1);

        }
        catch (RecognitionException re) {
            reportError(re);
            recover(input,re);
        }
        finally {
        }
        return retval;
    }
    // $ANTLR end "project_columns"

    // Delegated rules


 

    public static final BitSet FOLLOW_problem_in_problems60 = new BitSet(new long[]{0x0000000000073D12L});
    public static final BitSet FOLLOW_option_in_problem87 = new BitSet(new long[]{0x00000000F8D73D10L});
    public static final BitSet FOLLOW_univ_spec_in_problem98 = new BitSet(new long[]{0x00000000F8D73D10L});
    public static final BitSet FOLLOW_tuple_reg_directive_in_problem108 = new BitSet(new long[]{0x00000000F8D73D10L});
    public static final BitSet FOLLOW_bound_spec_in_problem119 = new BitSet(new long[]{0x00000000F8D73D10L});
    public static final BitSet FOLLOW_int_bound_spec_in_problem130 = new BitSet(new long[]{0x00000000F8D73D10L});
    public static final BitSet FOLLOW_expr_reg_directive_in_problem141 = new BitSet(new long[]{0x00000000F8D73D10L});
    public static final BitSet FOLLOW_solve_directive_in_problem152 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_SOLVER_in_option170 = new BitSet(new long[]{0x0000000000000020L});
    public static final BitSet FOLLOW_COLON_in_option172 = new BitSet(new long[]{0x0000000000000040L});
    public static final BitSet FOLLOW_STR_LITERAL_in_option178 = new BitSet(new long[]{0x0000000000000082L});
    public static final BitSet FOLLOW_COMMA_in_option191 = new BitSet(new long[]{0x0000000000000040L});
    public static final BitSet FOLLOW_STR_LITERAL_in_option197 = new BitSet(new long[]{0x0000000000000082L});
    public static final BitSet FOLLOW_SYMMETRY_BREAKING_in_option215 = new BitSet(new long[]{0x0000000000000020L});
    public static final BitSet FOLLOW_COLON_in_option217 = new BitSet(new long[]{0x0000000000000200L});
    public static final BitSet FOLLOW_NUM_in_option223 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_SHARING_in_option237 = new BitSet(new long[]{0x0000000000000020L});
    public static final BitSet FOLLOW_COLON_in_option239 = new BitSet(new long[]{0x0000000000000200L});
    public static final BitSet FOLLOW_NUM_in_option245 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_BIT_WIDTH_in_option259 = new BitSet(new long[]{0x0000000000000020L});
    public static final BitSet FOLLOW_COLON_in_option261 = new BitSet(new long[]{0x0000000000000200L});
    public static final BitSet FOLLOW_NUM_in_option267 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_SKOLEM_DEPTH_in_option281 = new BitSet(new long[]{0x0000000000000020L});
    public static final BitSet FOLLOW_COLON_in_option283 = new BitSet(new long[]{0x0000000000000200L});
    public static final BitSet FOLLOW_NUM_in_option289 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_FLATTEN_in_option303 = new BitSet(new long[]{0x0000000000000020L});
    public static final BitSet FOLLOW_COLON_in_option305 = new BitSet(new long[]{0x000000000000C000L});
    public static final BitSet FOLLOW_set_in_option311 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_TIMEOUT_in_option331 = new BitSet(new long[]{0x0000000000000020L});
    public static final BitSet FOLLOW_COLON_in_option333 = new BitSet(new long[]{0x0000000000000200L});
    public static final BitSet FOLLOW_NUM_in_option339 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_DELAY_in_option353 = new BitSet(new long[]{0x0000000000000020L});
    public static final BitSet FOLLOW_COLON_in_option355 = new BitSet(new long[]{0x0000000000000200L});
    public static final BitSet FOLLOW_NUM_in_option361 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_UNIV_in_univ_spec377 = new BitSet(new long[]{0x0000000000000020L});
    public static final BitSet FOLLOW_COLON_in_univ_spec379 = new BitSet(new long[]{0x0000000000080000L});
    public static final BitSet FOLLOW_UNIV_NAME_in_univ_spec385 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_TUPLE_SET_REG_in_tuple_reg_directive405 = new BitSet(new long[]{0x0000000000200000L});
    public static final BitSet FOLLOW_COLON_EQ_in_tuple_reg_directive411 = new BitSet(new long[]{0x0000316000180000L});
    public static final BitSet FOLLOW_tuple_set_in_tuple_reg_directive417 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_TUPLE_REG_in_tuple_reg_directive436 = new BitSet(new long[]{0x0000000000200000L});
    public static final BitSet FOLLOW_COLON_EQ_in_tuple_reg_directive442 = new BitSet(new long[]{0x0000C00002400000L});
    public static final BitSet FOLLOW_tuple_in_tuple_reg_directive448 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_BOUNDS_in_bound_spec469 = new BitSet(new long[]{0x0000000001000000L});
    public static final BitSet FOLLOW_RELATION_NAME_in_bound_spec475 = new BitSet(new long[]{0x00000000000000A0L});
    public static final BitSet FOLLOW_COMMA_in_bound_spec488 = new BitSet(new long[]{0x0000000001000000L});
    public static final BitSet FOLLOW_RELATION_NAME_in_bound_spec494 = new BitSet(new long[]{0x00000000000000A0L});
    public static final BitSet FOLLOW_COLON_in_bound_spec512 = new BitSet(new long[]{0x0000316002180000L});
    public static final BitSet FOLLOW_tuple_set_in_bound_spec527 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_BRACKET_LEFT_in_bound_spec541 = new BitSet(new long[]{0x0000316000180000L});
    public static final BitSet FOLLOW_tuple_set_in_bound_spec547 = new BitSet(new long[]{0x0000000000000080L});
    public static final BitSet FOLLOW_COMMA_in_bound_spec559 = new BitSet(new long[]{0x0000316000180000L});
    public static final BitSet FOLLOW_tuple_set_in_bound_spec565 = new BitSet(new long[]{0x0000000004000000L});
    public static final BitSet FOLLOW_BRACKET_RIGHT_in_bound_spec568 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_INT_BOUNDS_in_int_bound_spec585 = new BitSet(new long[]{0x0000000000000020L});
    public static final BitSet FOLLOW_COLON_in_int_bound_spec587 = new BitSet(new long[]{0x0000000002000200L});
    public static final BitSet FOLLOW_int_bound_seq_in_int_bound_spec593 = new BitSet(new long[]{0x0000000000000082L});
    public static final BitSet FOLLOW_COMMA_in_int_bound_spec596 = new BitSet(new long[]{0x0000000002000200L});
    public static final BitSet FOLLOW_int_bound_seq_in_int_bound_spec602 = new BitSet(new long[]{0x0000000000000082L});
    public static final BitSet FOLLOW_FORMULA_REG_in_expr_reg_directive622 = new BitSet(new long[]{0x0000000000200000L});
    public static final BitSet FOLLOW_COLON_EQ_in_expr_reg_directive624 = new BitSet(new long[]{0x041E7564710CC200L,0x0000000007FE123EL});
    public static final BitSet FOLLOW_expr_in_expr_reg_directive630 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_REL_EXPR_REG_in_expr_reg_directive648 = new BitSet(new long[]{0x0000000000200000L});
    public static final BitSet FOLLOW_COLON_EQ_in_expr_reg_directive650 = new BitSet(new long[]{0x041E7564710CC200L,0x0000000007FE123EL});
    public static final BitSet FOLLOW_expr_in_expr_reg_directive656 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_INT_EXPR_REG_in_expr_reg_directive674 = new BitSet(new long[]{0x0000000000200000L});
    public static final BitSet FOLLOW_COLON_EQ_in_expr_reg_directive676 = new BitSet(new long[]{0x041E7564710CC200L,0x0000000007FE123EL});
    public static final BitSet FOLLOW_expr_in_expr_reg_directive682 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_SOLVE_in_solve_directive702 = new BitSet(new long[]{0x041E7564710CC200L,0x0000000007FE123EL});
    public static final BitSet FOLLOW_expr_in_solve_directive708 = new BitSet(new long[]{0x0000000100000000L});
    public static final BitSet FOLLOW_SEMICOLON_in_solve_directive710 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_NUM_in_int_bound_seq732 = new BitSet(new long[]{0x0000000000000020L});
    public static final BitSet FOLLOW_COLON_in_int_bound_seq734 = new BitSet(new long[]{0x0000000002000200L});
    public static final BitSet FOLLOW_tuple_set_seq_in_int_bound_seq742 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_BRACKET_LEFT_in_tuple_set_seq773 = new BitSet(new long[]{0x0000316004180000L});
    public static final BitSet FOLLOW_tuple_set_in_tuple_set_seq780 = new BitSet(new long[]{0x0000000004000080L});
    public static final BitSet FOLLOW_COMMA_in_tuple_set_seq786 = new BitSet(new long[]{0x0000316000180000L});
    public static final BitSet FOLLOW_tuple_set_in_tuple_set_seq792 = new BitSet(new long[]{0x0000000004000080L});
    public static final BitSet FOLLOW_BRACKET_RIGHT_in_tuple_set_seq808 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_intersect_tuple_set_in_tuple_set832 = new BitSet(new long[]{0x0000000600000002L});
    public static final BitSet FOLLOW_set_in_tuple_set850 = new BitSet(new long[]{0x0000316000180000L});
    public static final BitSet FOLLOW_intersect_tuple_set_in_tuple_set862 = new BitSet(new long[]{0x0000000600000002L});
    public static final BitSet FOLLOW_product_tuple_set_in_intersect_tuple_set890 = new BitSet(new long[]{0x0000000800000002L});
    public static final BitSet FOLLOW_AND_in_intersect_tuple_set908 = new BitSet(new long[]{0x0000316000180000L});
    public static final BitSet FOLLOW_product_tuple_set_in_intersect_tuple_set914 = new BitSet(new long[]{0x0000000800000002L});
    public static final BitSet FOLLOW_project_tuple_set_in_product_tuple_set942 = new BitSet(new long[]{0x0000001000000002L});
    public static final BitSet FOLLOW_ARROW_in_product_tuple_set960 = new BitSet(new long[]{0x0000316000180000L});
    public static final BitSet FOLLOW_project_tuple_set_in_product_tuple_set966 = new BitSet(new long[]{0x0000001000000002L});
    public static final BitSet FOLLOW_basic_tuple_set_in_project_tuple_set994 = new BitSet(new long[]{0x0000000002000002L});
    public static final BitSet FOLLOW_BRACKET_LEFT_in_project_tuple_set1008 = new BitSet(new long[]{0x0000000000000200L});
    public static final BitSet FOLLOW_NUM_in_project_tuple_set1014 = new BitSet(new long[]{0x0000000004000000L});
    public static final BitSet FOLLOW_BRACKET_RIGHT_in_project_tuple_set1016 = new BitSet(new long[]{0x0000000002000002L});
    public static final BitSet FOLLOW_set_in_basic_tuple_set1050 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_PAREN_LEFT_in_basic_tuple_set1070 = new BitSet(new long[]{0x0000316000180000L});
    public static final BitSet FOLLOW_tuple_set_in_basic_tuple_set1076 = new BitSet(new long[]{0x0000008000000000L});
    public static final BitSet FOLLOW_PAREN_RIGHT_in_basic_tuple_set1081 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_BRACE_LEFT_in_basic_tuple_set1097 = new BitSet(new long[]{0x0000C80002400000L});
    public static final BitSet FOLLOW_tuple_in_basic_tuple_set1112 = new BitSet(new long[]{0x00000E0000000080L});
    public static final BitSet FOLLOW_COMMA_in_basic_tuple_set1128 = new BitSet(new long[]{0x0000C00002400000L});
    public static final BitSet FOLLOW_tuple_in_basic_tuple_set1134 = new BitSet(new long[]{0x0000080000000080L});
    public static final BitSet FOLLOW_DOT_DOT_in_basic_tuple_set1159 = new BitSet(new long[]{0x0000C00002400000L});
    public static final BitSet FOLLOW_tuple_in_basic_tuple_set1165 = new BitSet(new long[]{0x0000080000000000L});
    public static final BitSet FOLLOW_HASH_in_basic_tuple_set1186 = new BitSet(new long[]{0x0000C00002400000L});
    public static final BitSet FOLLOW_tuple_in_basic_tuple_set1192 = new BitSet(new long[]{0x0000080000000000L});
    public static final BitSet FOLLOW_BRACE_RIGHT_in_basic_tuple_set1220 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_NONE_in_basic_tuple_set1236 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_ALL_in_basic_tuple_set1254 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_TUPLE_SET_REG_in_basic_tuple_set1272 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_BRACKET_LEFT_in_tuple1293 = new BitSet(new long[]{0x0000400000000000L});
    public static final BitSet FOLLOW_ATOM_NAME_in_tuple1299 = new BitSet(new long[]{0x0000000004000080L});
    public static final BitSet FOLLOW_COMMA_in_tuple1304 = new BitSet(new long[]{0x0000400000000000L});
    public static final BitSet FOLLOW_ATOM_NAME_in_tuple1310 = new BitSet(new long[]{0x0000000004000080L});
    public static final BitSet FOLLOW_BRACKET_RIGHT_in_tuple1316 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_ATOM_NAME_in_tuple1334 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_TUPLE_NAME_in_tuple1352 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_TUPLE_REG_in_tuple1370 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_ALL_in_expr1391 = new BitSet(new long[]{0x0000000002000000L});
    public static final BitSet FOLLOW_decls_in_expr1397 = new BitSet(new long[]{0x0001000000000000L});
    public static final BitSet FOLLOW_BAR_in_expr1403 = new BitSet(new long[]{0x041E7564710CC200L,0x0000000007FE123EL});
    public static final BitSet FOLLOW_expr_in_expr1409 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_SOME_in_expr1423 = new BitSet(new long[]{0x0000000002000000L});
    public static final BitSet FOLLOW_decls_in_expr1429 = new BitSet(new long[]{0x0001000000000000L});
    public static final BitSet FOLLOW_BAR_in_expr1435 = new BitSet(new long[]{0x041E7564710CC200L,0x0000000007FE123EL});
    public static final BitSet FOLLOW_expr_in_expr1441 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_SUM_in_expr1455 = new BitSet(new long[]{0x0000000002000000L});
    public static final BitSet FOLLOW_decls_in_expr1461 = new BitSet(new long[]{0x0001000000000000L});
    public static final BitSet FOLLOW_BAR_in_expr1467 = new BitSet(new long[]{0x041E7564710CC200L,0x0000000007FE123EL});
    public static final BitSet FOLLOW_expr_in_expr1473 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_LET_in_expr1491 = new BitSet(new long[]{0x0000000002000000L});
    public static final BitSet FOLLOW_assigns_in_expr1497 = new BitSet(new long[]{0x0001000000000000L});
    public static final BitSet FOLLOW_BAR_in_expr1499 = new BitSet(new long[]{0x041E7564710CC200L,0x0000000007FE123EL});
    public static final BitSet FOLLOW_expr_in_expr1505 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_IF_in_expr1523 = new BitSet(new long[]{0x041E7564710CC200L,0x0000000007FE123EL});
    public static final BitSet FOLLOW_expr_in_expr1529 = new BitSet(new long[]{0x0020000000000000L});
    public static final BitSet FOLLOW_THEN_in_expr1535 = new BitSet(new long[]{0x041E7564710CC200L,0x0000000007FE123EL});
    public static final BitSet FOLLOW_expr_in_expr1541 = new BitSet(new long[]{0x0040000000000000L});
    public static final BitSet FOLLOW_ELSE_in_expr1547 = new BitSet(new long[]{0x041E7564710CC200L,0x0000000007FE123EL});
    public static final BitSet FOLLOW_expr_in_expr1553 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_iff_formula_in_expr1571 = new BitSet(new long[]{0x0080000000000002L});
    public static final BitSet FOLLOW_OR_in_expr1588 = new BitSet(new long[]{0x041E7564710CC200L,0x0000000007FE123EL});
    public static final BitSet FOLLOW_or_formula_tail_in_expr1594 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_iff_formula_in_or_formula_tail1635 = new BitSet(new long[]{0x0080000000000002L});
    public static final BitSet FOLLOW_OR_in_or_formula_tail1648 = new BitSet(new long[]{0x041E7564710CC200L,0x0000000007FE123EL});
    public static final BitSet FOLLOW_or_formula_tail_in_or_formula_tail1654 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_implies_formula_in_iff_formula1693 = new BitSet(new long[]{0x0100000000000002L});
    public static final BitSet FOLLOW_IFF_in_iff_formula1710 = new BitSet(new long[]{0x041E7564710CC200L,0x0000000007FE123EL});
    public static final BitSet FOLLOW_implies_formula_in_iff_formula1716 = new BitSet(new long[]{0x0100000000000002L});
    public static final BitSet FOLLOW_and_formula_in_implies_formula1742 = new BitSet(new long[]{0x0200000000000002L});
    public static final BitSet FOLLOW_IMPLIES_in_implies_formula1759 = new BitSet(new long[]{0x041E7564710CC200L,0x0000000007FE123EL});
    public static final BitSet FOLLOW_implies_formula_in_implies_formula1765 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_basic_formula_in_and_formula1791 = new BitSet(new long[]{0x0000000800000002L});
    public static final BitSet FOLLOW_AND_in_and_formula1808 = new BitSet(new long[]{0x041E7564710CC200L,0x0000000007FE123EL});
    public static final BitSet FOLLOW_and_formula_tail_in_and_formula1814 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_basic_formula_in_and_formula_tail1855 = new BitSet(new long[]{0x0000000800000002L});
    public static final BitSet FOLLOW_AND_in_and_formula_tail1868 = new BitSet(new long[]{0x041E7564710CC200L,0x0000000007FE123EL});
    public static final BitSet FOLLOW_and_formula_tail_in_and_formula_tail1874 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_NOT_in_basic_formula1913 = new BitSet(new long[]{0x041E7564710CC200L,0x0000000007FE123EL});
    public static final BitSet FOLLOW_basic_formula_in_basic_formula1919 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_predicate_formula_in_basic_formula1937 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_shift_expr_in_basic_formula1955 = new BitSet(new long[]{0xF800000000000002L,0x0000000000000001L});
    public static final BitSet FOLLOW_set_in_basic_formula1970 = new BitSet(new long[]{0x00045564710CC200L,0x0000000001FE1200L});
    public static final BitSet FOLLOW_shift_expr_in_basic_formula1998 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_multiplicity_in_basic_formula2021 = new BitSet(new long[]{0x00045564710CC200L,0x0000000001FE1200L});
    public static final BitSet FOLLOW_add_expr_in_basic_formula2027 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_ACYCLIC_in_predicate_formula2051 = new BitSet(new long[]{0x0000004000000000L});
    public static final BitSet FOLLOW_PAREN_LEFT_in_predicate_formula2053 = new BitSet(new long[]{0x0000000001000000L});
    public static final BitSet FOLLOW_RELATION_NAME_in_predicate_formula2059 = new BitSet(new long[]{0x0000008000000000L});
    public static final BitSet FOLLOW_PAREN_RIGHT_in_predicate_formula2061 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_FUNCTION_in_predicate_formula2079 = new BitSet(new long[]{0x0000004000000000L});
    public static final BitSet FOLLOW_PAREN_LEFT_in_predicate_formula2081 = new BitSet(new long[]{0x0000000001000000L});
    public static final BitSet FOLLOW_RELATION_NAME_in_predicate_formula2087 = new BitSet(new long[]{0x0000000000000080L});
    public static final BitSet FOLLOW_COMMA_in_predicate_formula2093 = new BitSet(new long[]{0x041E7564710CC200L,0x0000000007FE123EL});
    public static final BitSet FOLLOW_expr_in_predicate_formula2099 = new BitSet(new long[]{0x0000001000000000L});
    public static final BitSet FOLLOW_ARROW_in_predicate_formula2101 = new BitSet(new long[]{0x0000000000000000L,0x0000000000000018L});
    public static final BitSet FOLLOW_set_in_predicate_formula2115 = new BitSet(new long[]{0x041E7564710CC200L,0x0000000007FE123EL});
    public static final BitSet FOLLOW_expr_in_predicate_formula2127 = new BitSet(new long[]{0x0000008000000000L});
    public static final BitSet FOLLOW_PAREN_RIGHT_in_predicate_formula2129 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_TOTAL_ORDERING_in_predicate_formula2147 = new BitSet(new long[]{0x0000004000000000L});
    public static final BitSet FOLLOW_PAREN_LEFT_in_predicate_formula2149 = new BitSet(new long[]{0x0000000001000000L});
    public static final BitSet FOLLOW_RELATION_NAME_in_predicate_formula2155 = new BitSet(new long[]{0x0000000000000080L});
    public static final BitSet FOLLOW_COMMA_in_predicate_formula2157 = new BitSet(new long[]{0x0000002001080000L});
    public static final BitSet FOLLOW_set_in_predicate_formula2171 = new BitSet(new long[]{0x0000000000000080L});
    public static final BitSet FOLLOW_COMMA_in_predicate_formula2183 = new BitSet(new long[]{0x0000400001000000L});
    public static final BitSet FOLLOW_set_in_predicate_formula2197 = new BitSet(new long[]{0x0000000000000080L});
    public static final BitSet FOLLOW_COMMA_in_predicate_formula2205 = new BitSet(new long[]{0x0000400001000000L});
    public static final BitSet FOLLOW_set_in_predicate_formula2211 = new BitSet(new long[]{0x0000008000000000L});
    public static final BitSet FOLLOW_PAREN_RIGHT_in_predicate_formula2227 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_add_expr_in_shift_expr2251 = new BitSet(new long[]{0x0000000000000002L,0x00000000000001C0L});
    public static final BitSet FOLLOW_set_in_shift_expr2268 = new BitSet(new long[]{0x00045564710CC200L,0x0000000001FE1200L});
    public static final BitSet FOLLOW_add_expr_in_shift_expr2284 = new BitSet(new long[]{0x0000000000000002L,0x00000000000001C0L});
    public static final BitSet FOLLOW_mult_expr_in_add_expr2310 = new BitSet(new long[]{0x0000000600000002L});
    public static final BitSet FOLLOW_set_in_add_expr2327 = new BitSet(new long[]{0x00045564710CC200L,0x0000000001FE1200L});
    public static final BitSet FOLLOW_mult_expr_in_add_expr2339 = new BitSet(new long[]{0x0000000600000002L});
    public static final BitSet FOLLOW_expr_to_int_cast_in_mult_expr2365 = new BitSet(new long[]{0x0000000000000002L,0x0000000000000E00L});
    public static final BitSet FOLLOW_set_in_mult_expr2382 = new BitSet(new long[]{0x00045564710CC200L,0x0000000001FE1200L});
    public static final BitSet FOLLOW_expr_to_int_cast_in_mult_expr2398 = new BitSet(new long[]{0x0000000000000002L,0x0000000000000E00L});
    public static final BitSet FOLLOW_set_in_expr_to_int_cast2424 = new BitSet(new long[]{0x0000004000000000L});
    public static final BitSet FOLLOW_PAREN_LEFT_in_expr_to_int_cast2432 = new BitSet(new long[]{0x041E7564710CC200L,0x0000000007FE123EL});
    public static final BitSet FOLLOW_expr_in_expr_to_int_cast2438 = new BitSet(new long[]{0x0000008000000000L});
    public static final BitSet FOLLOW_PAREN_RIGHT_in_expr_to_int_cast2440 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_product_expr_in_expr_to_int_cast2458 = new BitSet(new long[]{0x0001000000000002L,0x0000000000007000L});
    public static final BitSet FOLLOW_set_in_expr_to_int_cast2475 = new BitSet(new long[]{0x00045564710CC200L,0x0000000001FE1200L});
    public static final BitSet FOLLOW_product_expr_in_expr_to_int_cast2491 = new BitSet(new long[]{0x0001000000000002L,0x0000000000007000L});
    public static final BitSet FOLLOW_OVERRIDE_in_expr_to_int_cast2502 = new BitSet(new long[]{0x00045564710CC200L,0x0000000001FE1200L});
    public static final BitSet FOLLOW_expr_to_int_cast_in_expr_to_int_cast2508 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_ifno_expr_in_product_expr2534 = new BitSet(new long[]{0x0000001000000002L});
    public static final BitSet FOLLOW_ARROW_in_product_expr2551 = new BitSet(new long[]{0x00045564710CC200L,0x0000000001FE1200L});
    public static final BitSet FOLLOW_product_expr_tail_in_product_expr2557 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_ifno_expr_in_product_expr_tail2598 = new BitSet(new long[]{0x0000001000000002L});
    public static final BitSet FOLLOW_ARROW_in_product_expr_tail2611 = new BitSet(new long[]{0x00045564710CC200L,0x0000000001FE1200L});
    public static final BitSet FOLLOW_product_expr_tail_in_product_expr_tail2617 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_apply_expr_in_ifno_expr2656 = new BitSet(new long[]{0x0000000000000002L,0x0000000000008000L});
    public static final BitSet FOLLOW_IFNO_in_ifno_expr2673 = new BitSet(new long[]{0x00045564710CC200L,0x0000000001FE1200L});
    public static final BitSet FOLLOW_apply_expr_in_ifno_expr2679 = new BitSet(new long[]{0x0000000000000002L,0x0000000000008000L});
    public static final BitSet FOLLOW_project_expr_in_apply_expr2705 = new BitSet(new long[]{0x0000004000000002L});
    public static final BitSet FOLLOW_PAREN_LEFT_in_apply_expr2722 = new BitSet(new long[]{0x041E7564710CC200L,0x0000000007FE123EL});
    public static final BitSet FOLLOW_expr_in_apply_expr2728 = new BitSet(new long[]{0x0000008000000080L});
    public static final BitSet FOLLOW_COMMA_in_apply_expr2733 = new BitSet(new long[]{0x041E7564710CC200L,0x0000000007FE123EL});
    public static final BitSet FOLLOW_expr_in_apply_expr2739 = new BitSet(new long[]{0x0000008000000080L});
    public static final BitSet FOLLOW_PAREN_RIGHT_in_apply_expr2745 = new BitSet(new long[]{0x0000004000000002L});
    public static final BitSet FOLLOW_basic_expr_in_project_expr2769 = new BitSet(new long[]{0x0000000002000002L,0x0000000000010000L});
    public static final BitSet FOLLOW_DOT_in_project_expr2786 = new BitSet(new long[]{0x00045564710CC200L,0x0000000001FE1200L});
    public static final BitSet FOLLOW_basic_expr_in_project_expr2792 = new BitSet(new long[]{0x0000000002000002L,0x0000000000010000L});
    public static final BitSet FOLLOW_project_columns_in_project_expr2803 = new BitSet(new long[]{0x0000000002000002L});
    public static final BitSet FOLLOW_PAREN_LEFT_in_basic_expr2825 = new BitSet(new long[]{0x041E7564710CC200L,0x0000000007FE123EL});
    public static final BitSet FOLLOW_expr_in_basic_expr2831 = new BitSet(new long[]{0x0000008000000000L});
    public static final BitSet FOLLOW_PAREN_RIGHT_in_basic_expr2833 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_set_in_basic_expr2851 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_VARIABLE_NAME_in_basic_expr2895 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_FORMULA_REG_in_basic_expr2913 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_REL_EXPR_REG_in_basic_expr2931 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_INT_EXPR_REG_in_basic_expr2949 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_NUM_in_basic_expr2967 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_FALSE_in_basic_expr2981 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_TRUE_in_basic_expr2995 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_IDEN_in_basic_expr3009 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_INTS_in_basic_expr3023 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_NONE_in_basic_expr3037 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_UNIV_in_basic_expr3051 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_set_in_basic_expr3069 = new BitSet(new long[]{0x00045564710CC200L,0x0000000001FE1200L});
    public static final BitSet FOLLOW_basic_expr_in_basic_expr3097 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_BRACE_LEFT_in_basic_expr3111 = new BitSet(new long[]{0x0000000002000000L});
    public static final BitSet FOLLOW_decls_in_basic_expr3117 = new BitSet(new long[]{0x0001000000000000L});
    public static final BitSet FOLLOW_BAR_in_basic_expr3123 = new BitSet(new long[]{0x041E7564710CC200L,0x0000000007FE123EL});
    public static final BitSet FOLLOW_expr_in_basic_expr3129 = new BitSet(new long[]{0x0000080000000000L});
    public static final BitSet FOLLOW_BRACE_RIGHT_in_basic_expr3131 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_set_in_basic_expr3149 = new BitSet(new long[]{0x0000000002000000L});
    public static final BitSet FOLLOW_BRACKET_LEFT_in_basic_expr3157 = new BitSet(new long[]{0x041E7564710CC200L,0x0000000007FE123EL});
    public static final BitSet FOLLOW_expr_in_basic_expr3163 = new BitSet(new long[]{0x0000000004000000L});
    public static final BitSet FOLLOW_BRACKET_RIGHT_in_basic_expr3165 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_BRACKET_LEFT_in_decls3186 = new BitSet(new long[]{0x0000000004000000L,0x0000000000020000L});
    public static final BitSet FOLLOW_decl_in_decls3201 = new BitSet(new long[]{0x0000000004000080L});
    public static final BitSet FOLLOW_COMMA_in_decls3215 = new BitSet(new long[]{0x0000000000000000L,0x0000000000020000L});
    public static final BitSet FOLLOW_decl_in_decls3221 = new BitSet(new long[]{0x0000000004000080L});
    public static final BitSet FOLLOW_BRACKET_RIGHT_in_decls3237 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_VARIABLE_NAME_in_decl3259 = new BitSet(new long[]{0x0000000000000020L});
    public static final BitSet FOLLOW_COLON_in_decl3265 = new BitSet(new long[]{0x041E7564710CC200L,0x0000000007FE123EL});
    public static final BitSet FOLLOW_multiplicity_in_decl3271 = new BitSet(new long[]{0x041E7564710CC200L,0x0000000007FE123EL});
    public static final BitSet FOLLOW_expr_in_decl3277 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_BRACKET_LEFT_in_assigns3297 = new BitSet(new long[]{0x0000000070000000L});
    public static final BitSet FOLLOW_assign_in_assigns3299 = new BitSet(new long[]{0x0000000004000080L});
    public static final BitSet FOLLOW_COMMA_in_assigns3311 = new BitSet(new long[]{0x0000000070000000L});
    public static final BitSet FOLLOW_assign_in_assigns3313 = new BitSet(new long[]{0x0000000004000080L});
    public static final BitSet FOLLOW_BRACKET_RIGHT_in_assigns3318 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_FORMULA_REG_in_assign3339 = new BitSet(new long[]{0x0000000000200000L});
    public static final BitSet FOLLOW_COLON_EQ_in_assign3345 = new BitSet(new long[]{0x041E7564710CC200L,0x0000000007FE123EL});
    public static final BitSet FOLLOW_expr_in_assign3351 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_REL_EXPR_REG_in_assign3369 = new BitSet(new long[]{0x0000000000200000L});
    public static final BitSet FOLLOW_COLON_EQ_in_assign3375 = new BitSet(new long[]{0x041E7564710CC200L,0x0000000007FE123EL});
    public static final BitSet FOLLOW_expr_in_assign3381 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_INT_EXPR_REG_in_assign3399 = new BitSet(new long[]{0x0000000000200000L});
    public static final BitSet FOLLOW_COLON_EQ_in_assign3405 = new BitSet(new long[]{0x041E7564710CC200L,0x0000000007FE123EL});
    public static final BitSet FOLLOW_expr_in_assign3411 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_set_in_multiplicity3435 = new BitSet(new long[]{0x0000000000000002L});
    public static final BitSet FOLLOW_BRACKET_LEFT_in_project_columns3484 = new BitSet(new long[]{0x041E7564710CC200L,0x0000000007FE123EL});
    public static final BitSet FOLLOW_expr_in_project_columns3490 = new BitSet(new long[]{0x0000000004000080L});
    public static final BitSet FOLLOW_COMMA_in_project_columns3499 = new BitSet(new long[]{0x041E7564710CC200L,0x0000000007FE123EL});
    public static final BitSet FOLLOW_expr_in_project_columns3505 = new BitSet(new long[]{0x0000000004000080L});
    public static final BitSet FOLLOW_BRACKET_RIGHT_in_project_columns3519 = new BitSet(new long[]{0x0000000000000002L});

}