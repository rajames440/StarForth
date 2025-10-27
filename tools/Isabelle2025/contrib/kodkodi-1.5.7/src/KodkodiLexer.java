// $ANTLR 3.1.1 src/Kodkodi.g 2021-10-07 20:34:49

package isabelle.kodkodi;


import org.antlr.runtime.*;
import java.util.Stack;
import java.util.List;
import java.util.ArrayList;

public class KodkodiLexer extends Lexer {
    public static final int BITS=87;
    public static final int SHR=72;
    public static final int FUNCTION=66;
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
    public static final int IFNO=79;
    public static final int OFFSET_UNIV_NAME=37;
    public static final int HAT=76;
    public static final int SKOLEM_DEPTH=12;
    public static final int INLINE_COMMENT=93;
    public static final int SHARING=10;
    public static final int BLOCK_COMMENT=94;
    public static final int EQ=59;
    public static final int DIVIDE=74;
    public static final int INTS=83;
    public static final int TIMEOUT=16;
    public static final int IFF=56;
    public static final int GE=63;
    public static final int IMPLIES=57;
    public static final int BRACKET_LEFT=25;
    public static final int NO=89;
    public static final int ELSE=54;
    public static final int WHITESPACE=92;
    public static final int INT=88;
    public static final int SEMICOLON=32;
    public static final int ACYCLIC=65;
    public static final int ABS=85;
    public static final int TOTAL_ORDERING=69;
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
    public static final int LE=61;
    public static final int LET=51;
    public static final int BAR=48;
    public static final int FORMULA_REG=28;
    public static final int LONE=68;

    public void emitErrorMessage(String message) {
        System.err.println(message);
    }


    // delegates
    // delegators

    public KodkodiLexer() {;} 
    public KodkodiLexer(CharStream input) {
        this(input, new RecognizerSharedState());
    }
    public KodkodiLexer(CharStream input, RecognizerSharedState state) {
        super(input,state);

    }
    public String getGrammarFileName() { return "src/Kodkodi.g"; }

    // $ANTLR start "ATOM_NAME"
    public final void mATOM_NAME() throws RecognitionException {
        try {
            int _type = ATOM_NAME;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1698:10: ( 'A' NAT )
            // src/Kodkodi.g:1698:17: 'A' NAT
            {
            match('A'); 
            mNAT(); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "ATOM_NAME"

    // $ANTLR start "UNIV_NAME"
    public final void mUNIV_NAME() throws RecognitionException {
        try {
            int _type = UNIV_NAME;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1699:10: ( 'u' NAT )
            // src/Kodkodi.g:1699:17: 'u' NAT
            {
            match('u'); 
            mNAT(); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "UNIV_NAME"

    // $ANTLR start "OFFSET_UNIV_NAME"
    public final void mOFFSET_UNIV_NAME() throws RecognitionException {
        try {
            int _type = OFFSET_UNIV_NAME;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1700:17: ( 'u' NAT '@' NAT )
            // src/Kodkodi.g:1701:17: 'u' NAT '@' NAT
            {
            match('u'); 
            mNAT(); 
            match('@'); 
            mNAT(); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "OFFSET_UNIV_NAME"

    // $ANTLR start "TUPLE_NAME"
    public final void mTUPLE_NAME() throws RecognitionException {
        try {
            int _type = TUPLE_NAME;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1702:11: ( ( 'P' | 'T' NAT '_' ) NAT )
            // src/Kodkodi.g:1702:17: ( 'P' | 'T' NAT '_' ) NAT
            {
            // src/Kodkodi.g:1702:17: ( 'P' | 'T' NAT '_' )
            int alt1=2;
            int LA1_0 = input.LA(1);

            if ( (LA1_0=='P') ) {
                alt1=1;
            }
            else if ( (LA1_0=='T') ) {
                alt1=2;
            }
            else {
                NoViableAltException nvae =
                    new NoViableAltException("", 1, 0, input);

                throw nvae;
            }
            switch (alt1) {
                case 1 :
                    // src/Kodkodi.g:1702:18: 'P'
                    {
                    match('P'); 

                    }
                    break;
                case 2 :
                    // src/Kodkodi.g:1702:24: 'T' NAT '_'
                    {
                    match('T'); 
                    mNAT(); 
                    match('_'); 

                    }
                    break;

            }

            mNAT(); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "TUPLE_NAME"

    // $ANTLR start "RELATION_NAME"
    public final void mRELATION_NAME() throws RecognitionException {
        try {
            int _type = RELATION_NAME;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1703:14: ( ( 's' | 'r' | 'm' NAT '_' ) NAT ( '\\'' )? )
            // src/Kodkodi.g:1703:17: ( 's' | 'r' | 'm' NAT '_' ) NAT ( '\\'' )?
            {
            // src/Kodkodi.g:1703:17: ( 's' | 'r' | 'm' NAT '_' )
            int alt2=3;
            switch ( input.LA(1) ) {
            case 's':
                {
                alt2=1;
                }
                break;
            case 'r':
                {
                alt2=2;
                }
                break;
            case 'm':
                {
                alt2=3;
                }
                break;
            default:
                NoViableAltException nvae =
                    new NoViableAltException("", 2, 0, input);

                throw nvae;
            }

            switch (alt2) {
                case 1 :
                    // src/Kodkodi.g:1703:18: 's'
                    {
                    match('s'); 

                    }
                    break;
                case 2 :
                    // src/Kodkodi.g:1703:24: 'r'
                    {
                    match('r'); 

                    }
                    break;
                case 3 :
                    // src/Kodkodi.g:1703:30: 'm' NAT '_'
                    {
                    match('m'); 
                    mNAT(); 
                    match('_'); 

                    }
                    break;

            }

            mNAT(); 
            // src/Kodkodi.g:1703:47: ( '\\'' )?
            int alt3=2;
            int LA3_0 = input.LA(1);

            if ( (LA3_0=='\'') ) {
                alt3=1;
            }
            switch (alt3) {
                case 1 :
                    // src/Kodkodi.g:1703:47: '\\''
                    {
                    match('\''); 

                    }
                    break;

            }


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "RELATION_NAME"

    // $ANTLR start "VARIABLE_NAME"
    public final void mVARIABLE_NAME() throws RecognitionException {
        try {
            int _type = VARIABLE_NAME;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1704:14: ( ( 'S' | 'R' | 'M' NAT '_' ) NAT ( '\\'' )? )
            // src/Kodkodi.g:1704:17: ( 'S' | 'R' | 'M' NAT '_' ) NAT ( '\\'' )?
            {
            // src/Kodkodi.g:1704:17: ( 'S' | 'R' | 'M' NAT '_' )
            int alt4=3;
            switch ( input.LA(1) ) {
            case 'S':
                {
                alt4=1;
                }
                break;
            case 'R':
                {
                alt4=2;
                }
                break;
            case 'M':
                {
                alt4=3;
                }
                break;
            default:
                NoViableAltException nvae =
                    new NoViableAltException("", 4, 0, input);

                throw nvae;
            }

            switch (alt4) {
                case 1 :
                    // src/Kodkodi.g:1704:18: 'S'
                    {
                    match('S'); 

                    }
                    break;
                case 2 :
                    // src/Kodkodi.g:1704:24: 'R'
                    {
                    match('R'); 

                    }
                    break;
                case 3 :
                    // src/Kodkodi.g:1704:30: 'M' NAT '_'
                    {
                    match('M'); 
                    mNAT(); 
                    match('_'); 

                    }
                    break;

            }

            mNAT(); 
            // src/Kodkodi.g:1704:47: ( '\\'' )?
            int alt5=2;
            int LA5_0 = input.LA(1);

            if ( (LA5_0=='\'') ) {
                alt5=1;
            }
            switch (alt5) {
                case 1 :
                    // src/Kodkodi.g:1704:47: '\\''
                    {
                    match('\''); 

                    }
                    break;

            }


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "VARIABLE_NAME"

    // $ANTLR start "TUPLE_SET_REG"
    public final void mTUPLE_SET_REG() throws RecognitionException {
        try {
            int _type = TUPLE_SET_REG;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1705:14: ( '$' ( 'a' | 'p' | 't' NAT '_' ) NAT )
            // src/Kodkodi.g:1705:17: '$' ( 'a' | 'p' | 't' NAT '_' ) NAT
            {
            match('$'); 
            // src/Kodkodi.g:1705:21: ( 'a' | 'p' | 't' NAT '_' )
            int alt6=3;
            switch ( input.LA(1) ) {
            case 'a':
                {
                alt6=1;
                }
                break;
            case 'p':
                {
                alt6=2;
                }
                break;
            case 't':
                {
                alt6=3;
                }
                break;
            default:
                NoViableAltException nvae =
                    new NoViableAltException("", 6, 0, input);

                throw nvae;
            }

            switch (alt6) {
                case 1 :
                    // src/Kodkodi.g:1705:22: 'a'
                    {
                    match('a'); 

                    }
                    break;
                case 2 :
                    // src/Kodkodi.g:1705:28: 'p'
                    {
                    match('p'); 

                    }
                    break;
                case 3 :
                    // src/Kodkodi.g:1705:34: 't' NAT '_'
                    {
                    match('t'); 
                    mNAT(); 
                    match('_'); 

                    }
                    break;

            }

            mNAT(); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "TUPLE_SET_REG"

    // $ANTLR start "TUPLE_REG"
    public final void mTUPLE_REG() throws RecognitionException {
        try {
            int _type = TUPLE_REG;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1706:10: ( '$' ( 'A' | 'P' | 'T' NAT '_' ) NAT )
            // src/Kodkodi.g:1706:17: '$' ( 'A' | 'P' | 'T' NAT '_' ) NAT
            {
            match('$'); 
            // src/Kodkodi.g:1706:21: ( 'A' | 'P' | 'T' NAT '_' )
            int alt7=3;
            switch ( input.LA(1) ) {
            case 'A':
                {
                alt7=1;
                }
                break;
            case 'P':
                {
                alt7=2;
                }
                break;
            case 'T':
                {
                alt7=3;
                }
                break;
            default:
                NoViableAltException nvae =
                    new NoViableAltException("", 7, 0, input);

                throw nvae;
            }

            switch (alt7) {
                case 1 :
                    // src/Kodkodi.g:1706:22: 'A'
                    {
                    match('A'); 

                    }
                    break;
                case 2 :
                    // src/Kodkodi.g:1706:28: 'P'
                    {
                    match('P'); 

                    }
                    break;
                case 3 :
                    // src/Kodkodi.g:1706:34: 'T' NAT '_'
                    {
                    match('T'); 
                    mNAT(); 
                    match('_'); 

                    }
                    break;

            }

            mNAT(); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "TUPLE_REG"

    // $ANTLR start "FORMULA_REG"
    public final void mFORMULA_REG() throws RecognitionException {
        try {
            int _type = FORMULA_REG;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1707:12: ( '$f' NAT )
            // src/Kodkodi.g:1707:17: '$f' NAT
            {
            match("$f"); 

            mNAT(); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "FORMULA_REG"

    // $ANTLR start "REL_EXPR_REG"
    public final void mREL_EXPR_REG() throws RecognitionException {
        try {
            int _type = REL_EXPR_REG;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1708:13: ( '$e' NAT )
            // src/Kodkodi.g:1708:17: '$e' NAT
            {
            match("$e"); 

            mNAT(); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "REL_EXPR_REG"

    // $ANTLR start "INT_EXPR_REG"
    public final void mINT_EXPR_REG() throws RecognitionException {
        try {
            int _type = INT_EXPR_REG;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1709:13: ( '$i' NAT )
            // src/Kodkodi.g:1709:17: '$i' NAT
            {
            match("$i"); 

            mNAT(); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "INT_EXPR_REG"

    // $ANTLR start "NUM"
    public final void mNUM() throws RecognitionException {
        try {
            int _type = NUM;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1711:4: ( ( PLUS | MINUS )? ( '0' .. '9' )+ )
            // src/Kodkodi.g:1711:17: ( PLUS | MINUS )? ( '0' .. '9' )+
            {
            // src/Kodkodi.g:1711:17: ( PLUS | MINUS )?
            int alt8=2;
            int LA8_0 = input.LA(1);

            if ( (LA8_0=='+'||LA8_0=='-') ) {
                alt8=1;
            }
            switch (alt8) {
                case 1 :
                    // src/Kodkodi.g:
                    {
                    if ( input.LA(1)=='+'||input.LA(1)=='-' ) {
                        input.consume();

                    }
                    else {
                        MismatchedSetException mse = new MismatchedSetException(null,input);
                        recover(mse);
                        throw mse;}


                    }
                    break;

            }

            // src/Kodkodi.g:1711:33: ( '0' .. '9' )+
            int cnt9=0;
            loop9:
            do {
                int alt9=2;
                int LA9_0 = input.LA(1);

                if ( ((LA9_0>='0' && LA9_0<='9')) ) {
                    alt9=1;
                }


                switch (alt9) {
            	case 1 :
            	    // src/Kodkodi.g:1711:33: '0' .. '9'
            	    {
            	    matchRange('0','9'); 

            	    }
            	    break;

            	default :
            	    if ( cnt9 >= 1 ) break loop9;
                        EarlyExitException eee =
                            new EarlyExitException(9, input);
                        throw eee;
                }
                cnt9++;
            } while (true);


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "NUM"

    // $ANTLR start "NAT"
    public final void mNAT() throws RecognitionException {
        try {
            // src/Kodkodi.g:1712:13: ( '0' | '1' .. '9' ( '0' .. '9' )* )
            int alt11=2;
            int LA11_0 = input.LA(1);

            if ( (LA11_0=='0') ) {
                alt11=1;
            }
            else if ( ((LA11_0>='1' && LA11_0<='9')) ) {
                alt11=2;
            }
            else {
                NoViableAltException nvae =
                    new NoViableAltException("", 11, 0, input);

                throw nvae;
            }
            switch (alt11) {
                case 1 :
                    // src/Kodkodi.g:1712:17: '0'
                    {
                    match('0'); 

                    }
                    break;
                case 2 :
                    // src/Kodkodi.g:1712:23: '1' .. '9' ( '0' .. '9' )*
                    {
                    matchRange('1','9'); 
                    // src/Kodkodi.g:1712:32: ( '0' .. '9' )*
                    loop10:
                    do {
                        int alt10=2;
                        int LA10_0 = input.LA(1);

                        if ( ((LA10_0>='0' && LA10_0<='9')) ) {
                            alt10=1;
                        }


                        switch (alt10) {
                    	case 1 :
                    	    // src/Kodkodi.g:1712:32: '0' .. '9'
                    	    {
                    	    matchRange('0','9'); 

                    	    }
                    	    break;

                    	default :
                    	    break loop10;
                        }
                    } while (true);


                    }
                    break;

            }
        }
        finally {
        }
    }
    // $ANTLR end "NAT"

    // $ANTLR start "STR_LITERAL"
    public final void mSTR_LITERAL() throws RecognitionException {
        try {
            int _type = STR_LITERAL;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1713:12: ( '\"' (~ ( '\"' | '\\n' ) )* '\"' )
            // src/Kodkodi.g:1713:17: '\"' (~ ( '\"' | '\\n' ) )* '\"'
            {
            match('\"'); 
            // src/Kodkodi.g:1713:21: (~ ( '\"' | '\\n' ) )*
            loop12:
            do {
                int alt12=2;
                int LA12_0 = input.LA(1);

                if ( ((LA12_0>='\u0000' && LA12_0<='\t')||(LA12_0>='\u000B' && LA12_0<='!')||(LA12_0>='#' && LA12_0<='\uFFFF')) ) {
                    alt12=1;
                }


                switch (alt12) {
            	case 1 :
            	    // src/Kodkodi.g:1713:21: ~ ( '\"' | '\\n' )
            	    {
            	    if ( (input.LA(1)>='\u0000' && input.LA(1)<='\t')||(input.LA(1)>='\u000B' && input.LA(1)<='!')||(input.LA(1)>='#' && input.LA(1)<='\uFFFF') ) {
            	        input.consume();

            	    }
            	    else {
            	        MismatchedSetException mse = new MismatchedSetException(null,input);
            	        recover(mse);
            	        throw mse;}


            	    }
            	    break;

            	default :
            	    break loop12;
                }
            } while (true);

            match('\"'); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "STR_LITERAL"

    // $ANTLR start "WHITESPACE"
    public final void mWHITESPACE() throws RecognitionException {
        try {
            int _type = WHITESPACE;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1714:11: ( ( ' ' | '\\n' | '\\r' | '\\t' | '\\v' )+ )
            // src/Kodkodi.g:1714:17: ( ' ' | '\\n' | '\\r' | '\\t' | '\\v' )+
            {
            // src/Kodkodi.g:1714:17: ( ' ' | '\\n' | '\\r' | '\\t' | '\\v' )+
            int cnt13=0;
            loop13:
            do {
                int alt13=2;
                int LA13_0 = input.LA(1);

                if ( ((LA13_0>='\t' && LA13_0<='\n')||LA13_0=='\r'||LA13_0==' '||LA13_0=='v') ) {
                    alt13=1;
                }


                switch (alt13) {
            	case 1 :
            	    // src/Kodkodi.g:
            	    {
            	    if ( (input.LA(1)>='\t' && input.LA(1)<='\n')||input.LA(1)=='\r'||input.LA(1)==' '||input.LA(1)=='v' ) {
            	        input.consume();

            	    }
            	    else {
            	        MismatchedSetException mse = new MismatchedSetException(null,input);
            	        recover(mse);
            	        throw mse;}


            	    }
            	    break;

            	default :
            	    if ( cnt13 >= 1 ) break loop13;
                        EarlyExitException eee =
                            new EarlyExitException(13, input);
                        throw eee;
                }
                cnt13++;
            } while (true);

             skip(); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "WHITESPACE"

    // $ANTLR start "INLINE_COMMENT"
    public final void mINLINE_COMMENT() throws RecognitionException {
        try {
            int _type = INLINE_COMMENT;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1715:15: ( '//' (~ ( '\\n' ) )* )
            // src/Kodkodi.g:1715:17: '//' (~ ( '\\n' ) )*
            {
            match("//"); 

            // src/Kodkodi.g:1715:22: (~ ( '\\n' ) )*
            loop14:
            do {
                int alt14=2;
                int LA14_0 = input.LA(1);

                if ( ((LA14_0>='\u0000' && LA14_0<='\t')||(LA14_0>='\u000B' && LA14_0<='\uFFFF')) ) {
                    alt14=1;
                }


                switch (alt14) {
            	case 1 :
            	    // src/Kodkodi.g:1715:22: ~ ( '\\n' )
            	    {
            	    if ( (input.LA(1)>='\u0000' && input.LA(1)<='\t')||(input.LA(1)>='\u000B' && input.LA(1)<='\uFFFF') ) {
            	        input.consume();

            	    }
            	    else {
            	        MismatchedSetException mse = new MismatchedSetException(null,input);
            	        recover(mse);
            	        throw mse;}


            	    }
            	    break;

            	default :
            	    break loop14;
                }
            } while (true);

             skip(); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "INLINE_COMMENT"

    // $ANTLR start "BLOCK_COMMENT"
    public final void mBLOCK_COMMENT() throws RecognitionException {
        try {
            int _type = BLOCK_COMMENT;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1716:14: ( '/*' ( options {greedy=false; } : . )* '*/' )
            // src/Kodkodi.g:1716:17: '/*' ( options {greedy=false; } : . )* '*/'
            {
            match("/*"); 

            // src/Kodkodi.g:1716:22: ( options {greedy=false; } : . )*
            loop15:
            do {
                int alt15=2;
                int LA15_0 = input.LA(1);

                if ( (LA15_0=='*') ) {
                    int LA15_1 = input.LA(2);

                    if ( (LA15_1=='/') ) {
                        alt15=2;
                    }
                    else if ( ((LA15_1>='\u0000' && LA15_1<='.')||(LA15_1>='0' && LA15_1<='\uFFFF')) ) {
                        alt15=1;
                    }


                }
                else if ( ((LA15_0>='\u0000' && LA15_0<=')')||(LA15_0>='+' && LA15_0<='\uFFFF')) ) {
                    alt15=1;
                }


                switch (alt15) {
            	case 1 :
            	    // src/Kodkodi.g:1716:53: .
            	    {
            	    matchAny(); 

            	    }
            	    break;

            	default :
            	    break loop15;
                }
            } while (true);

            match("*/"); 

             skip(); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "BLOCK_COMMENT"

    // $ANTLR start "AMP"
    public final void mAMP() throws RecognitionException {
        try {
            int _type = AMP;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1718:4: ( '&' )
            // src/Kodkodi.g:1718:17: '&'
            {
            match('&'); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "AMP"

    // $ANTLR start "AND"
    public final void mAND() throws RecognitionException {
        try {
            int _type = AND;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1719:4: ( '&&' )
            // src/Kodkodi.g:1719:17: '&&'
            {
            match("&&"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "AND"

    // $ANTLR start "ARROW"
    public final void mARROW() throws RecognitionException {
        try {
            int _type = ARROW;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1720:6: ( '->' )
            // src/Kodkodi.g:1720:17: '->'
            {
            match("->"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "ARROW"

    // $ANTLR start "COLON_EQ"
    public final void mCOLON_EQ() throws RecognitionException {
        try {
            int _type = COLON_EQ;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1721:9: ( ':=' )
            // src/Kodkodi.g:1721:17: ':='
            {
            match(":="); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "COLON_EQ"

    // $ANTLR start "BAR"
    public final void mBAR() throws RecognitionException {
        try {
            int _type = BAR;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1722:4: ( '|' )
            // src/Kodkodi.g:1722:17: '|'
            {
            match('|'); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "BAR"

    // $ANTLR start "BRACE_LEFT"
    public final void mBRACE_LEFT() throws RecognitionException {
        try {
            int _type = BRACE_LEFT;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1723:11: ( '{' )
            // src/Kodkodi.g:1723:17: '{'
            {
            match('{'); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "BRACE_LEFT"

    // $ANTLR start "BRACE_RIGHT"
    public final void mBRACE_RIGHT() throws RecognitionException {
        try {
            int _type = BRACE_RIGHT;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1724:12: ( '}' )
            // src/Kodkodi.g:1724:17: '}'
            {
            match('}'); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "BRACE_RIGHT"

    // $ANTLR start "BRACKET_LEFT"
    public final void mBRACKET_LEFT() throws RecognitionException {
        try {
            int _type = BRACKET_LEFT;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1725:13: ( '[' )
            // src/Kodkodi.g:1725:17: '['
            {
            match('['); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "BRACKET_LEFT"

    // $ANTLR start "BRACKET_RIGHT"
    public final void mBRACKET_RIGHT() throws RecognitionException {
        try {
            int _type = BRACKET_RIGHT;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1726:14: ( ']' )
            // src/Kodkodi.g:1726:17: ']'
            {
            match(']'); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "BRACKET_RIGHT"

    // $ANTLR start "COLON"
    public final void mCOLON() throws RecognitionException {
        try {
            int _type = COLON;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1727:6: ( ':' )
            // src/Kodkodi.g:1727:17: ':'
            {
            match(':'); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "COLON"

    // $ANTLR start "COMMA"
    public final void mCOMMA() throws RecognitionException {
        try {
            int _type = COMMA;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1728:6: ( ',' )
            // src/Kodkodi.g:1728:17: ','
            {
            match(','); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "COMMA"

    // $ANTLR start "DIVIDE"
    public final void mDIVIDE() throws RecognitionException {
        try {
            int _type = DIVIDE;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1729:7: ( '/' )
            // src/Kodkodi.g:1729:17: '/'
            {
            match('/'); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "DIVIDE"

    // $ANTLR start "DOT"
    public final void mDOT() throws RecognitionException {
        try {
            int _type = DOT;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1730:4: ( '.' )
            // src/Kodkodi.g:1730:17: '.'
            {
            match('.'); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "DOT"

    // $ANTLR start "DOT_DOT"
    public final void mDOT_DOT() throws RecognitionException {
        try {
            int _type = DOT_DOT;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1731:8: ( '..' )
            // src/Kodkodi.g:1731:17: '..'
            {
            match(".."); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "DOT_DOT"

    // $ANTLR start "EQ"
    public final void mEQ() throws RecognitionException {
        try {
            int _type = EQ;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1732:3: ( '=' )
            // src/Kodkodi.g:1732:17: '='
            {
            match('='); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "EQ"

    // $ANTLR start "GE"
    public final void mGE() throws RecognitionException {
        try {
            int _type = GE;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1733:3: ( '>=' )
            // src/Kodkodi.g:1733:17: '>='
            {
            match(">="); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "GE"

    // $ANTLR start "GT"
    public final void mGT() throws RecognitionException {
        try {
            int _type = GT;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1734:3: ( '>' )
            // src/Kodkodi.g:1734:17: '>'
            {
            match('>'); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "GT"

    // $ANTLR start "HASH"
    public final void mHASH() throws RecognitionException {
        try {
            int _type = HASH;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1735:5: ( '#' )
            // src/Kodkodi.g:1735:17: '#'
            {
            match('#'); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "HASH"

    // $ANTLR start "HAT"
    public final void mHAT() throws RecognitionException {
        try {
            int _type = HAT;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1736:4: ( '^' )
            // src/Kodkodi.g:1736:17: '^'
            {
            match('^'); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "HAT"

    // $ANTLR start "IFF"
    public final void mIFF() throws RecognitionException {
        try {
            int _type = IFF;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1737:4: ( '<=>' )
            // src/Kodkodi.g:1737:17: '<=>'
            {
            match("<=>"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "IFF"

    // $ANTLR start "IFNO"
    public final void mIFNO() throws RecognitionException {
        try {
            int _type = IFNO;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1738:5: ( '\\\\' )
            // src/Kodkodi.g:1738:17: '\\\\'
            {
            match('\\'); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "IFNO"

    // $ANTLR start "IMPLIES"
    public final void mIMPLIES() throws RecognitionException {
        try {
            int _type = IMPLIES;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1739:8: ( '=>' )
            // src/Kodkodi.g:1739:17: '=>'
            {
            match("=>"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "IMPLIES"

    // $ANTLR start "LT"
    public final void mLT() throws RecognitionException {
        try {
            int _type = LT;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1740:3: ( '<' )
            // src/Kodkodi.g:1740:17: '<'
            {
            match('<'); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "LT"

    // $ANTLR start "LE"
    public final void mLE() throws RecognitionException {
        try {
            int _type = LE;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1741:3: ( '<=' )
            // src/Kodkodi.g:1741:17: '<='
            {
            match("<="); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "LE"

    // $ANTLR start "MINUS"
    public final void mMINUS() throws RecognitionException {
        try {
            int _type = MINUS;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1742:7: ( '-' )
            // src/Kodkodi.g:1742:17: '-'
            {
            match('-'); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "MINUS"

    // $ANTLR start "MODULO"
    public final void mMODULO() throws RecognitionException {
        try {
            int _type = MODULO;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1743:7: ( '%' )
            // src/Kodkodi.g:1743:17: '%'
            {
            match('%'); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "MODULO"

    // $ANTLR start "NOT"
    public final void mNOT() throws RecognitionException {
        try {
            int _type = NOT;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1744:4: ( '!' )
            // src/Kodkodi.g:1744:17: '!'
            {
            match('!'); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "NOT"

    // $ANTLR start "OVERRIDE"
    public final void mOVERRIDE() throws RecognitionException {
        try {
            int _type = OVERRIDE;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1745:10: ( '++' )
            // src/Kodkodi.g:1745:17: '++'
            {
            match("++"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "OVERRIDE"

    // $ANTLR start "OR"
    public final void mOR() throws RecognitionException {
        try {
            int _type = OR;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1746:3: ( '||' )
            // src/Kodkodi.g:1746:17: '||'
            {
            match("||"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "OR"

    // $ANTLR start "PAREN_LEFT"
    public final void mPAREN_LEFT() throws RecognitionException {
        try {
            int _type = PAREN_LEFT;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1747:11: ( '(' )
            // src/Kodkodi.g:1747:17: '('
            {
            match('('); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "PAREN_LEFT"

    // $ANTLR start "PAREN_RIGHT"
    public final void mPAREN_RIGHT() throws RecognitionException {
        try {
            int _type = PAREN_RIGHT;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1748:12: ( ')' )
            // src/Kodkodi.g:1748:17: ')'
            {
            match(')'); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "PAREN_RIGHT"

    // $ANTLR start "PLUS"
    public final void mPLUS() throws RecognitionException {
        try {
            int _type = PLUS;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1749:5: ( '+' )
            // src/Kodkodi.g:1749:17: '+'
            {
            match('+'); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "PLUS"

    // $ANTLR start "SEMICOLON"
    public final void mSEMICOLON() throws RecognitionException {
        try {
            int _type = SEMICOLON;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1750:10: ( ';' )
            // src/Kodkodi.g:1750:17: ';'
            {
            match(';'); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "SEMICOLON"

    // $ANTLR start "SHA"
    public final void mSHA() throws RecognitionException {
        try {
            int _type = SHA;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1751:4: ( '>>' )
            // src/Kodkodi.g:1751:17: '>>'
            {
            match(">>"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "SHA"

    // $ANTLR start "SHL"
    public final void mSHL() throws RecognitionException {
        try {
            int _type = SHL;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1752:4: ( '<<' )
            // src/Kodkodi.g:1752:17: '<<'
            {
            match("<<"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "SHL"

    // $ANTLR start "SHR"
    public final void mSHR() throws RecognitionException {
        try {
            int _type = SHR;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1753:4: ( '>>>' )
            // src/Kodkodi.g:1753:17: '>>>'
            {
            match(">>>"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "SHR"

    // $ANTLR start "STAR"
    public final void mSTAR() throws RecognitionException {
        try {
            int _type = STAR;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1754:5: ( '*' )
            // src/Kodkodi.g:1754:17: '*'
            {
            match('*'); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "STAR"

    // $ANTLR start "TILDE"
    public final void mTILDE() throws RecognitionException {
        try {
            int _type = TILDE;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1755:6: ( '~' )
            // src/Kodkodi.g:1755:17: '~'
            {
            match('~'); 

            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "TILDE"

    // $ANTLR start "ABS"
    public final void mABS() throws RecognitionException {
        try {
            int _type = ABS;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1757:4: ( 'abs' )
            // src/Kodkodi.g:1757:17: 'abs'
            {
            match("abs"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "ABS"

    // $ANTLR start "ACYCLIC"
    public final void mACYCLIC() throws RecognitionException {
        try {
            int _type = ACYCLIC;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1758:8: ( 'ACYCLIC' )
            // src/Kodkodi.g:1758:17: 'ACYCLIC'
            {
            match("ACYCLIC"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "ACYCLIC"

    // $ANTLR start "ALL"
    public final void mALL() throws RecognitionException {
        try {
            int _type = ALL;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1759:4: ( 'all' )
            // src/Kodkodi.g:1759:17: 'all'
            {
            match("all"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "ALL"

    // $ANTLR start "BITS"
    public final void mBITS() throws RecognitionException {
        try {
            int _type = BITS;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1760:5: ( 'Bits' )
            // src/Kodkodi.g:1760:17: 'Bits'
            {
            match("Bits"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "BITS"

    // $ANTLR start "BIT_WIDTH"
    public final void mBIT_WIDTH() throws RecognitionException {
        try {
            int _type = BIT_WIDTH;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1761:10: ( 'bit_width' )
            // src/Kodkodi.g:1761:17: 'bit_width'
            {
            match("bit_width"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "BIT_WIDTH"

    // $ANTLR start "BOUNDS"
    public final void mBOUNDS() throws RecognitionException {
        try {
            int _type = BOUNDS;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1762:7: ( 'bounds' )
            // src/Kodkodi.g:1762:17: 'bounds'
            {
            match("bounds"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "BOUNDS"

    // $ANTLR start "DELAY"
    public final void mDELAY() throws RecognitionException {
        try {
            int _type = DELAY;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1763:6: ( 'delay' )
            // src/Kodkodi.g:1763:17: 'delay'
            {
            match("delay"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "DELAY"

    // $ANTLR start "ELSE"
    public final void mELSE() throws RecognitionException {
        try {
            int _type = ELSE;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1764:5: ( 'else' )
            // src/Kodkodi.g:1764:17: 'else'
            {
            match("else"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "ELSE"

    // $ANTLR start "FALSE"
    public final void mFALSE() throws RecognitionException {
        try {
            int _type = FALSE;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1765:6: ( 'false' )
            // src/Kodkodi.g:1765:17: 'false'
            {
            match("false"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "FALSE"

    // $ANTLR start "FLATTEN"
    public final void mFLATTEN() throws RecognitionException {
        try {
            int _type = FLATTEN;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1766:8: ( 'flatten' )
            // src/Kodkodi.g:1766:17: 'flatten'
            {
            match("flatten"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "FLATTEN"

    // $ANTLR start "FUNCTION"
    public final void mFUNCTION() throws RecognitionException {
        try {
            int _type = FUNCTION;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1767:9: ( 'FUNCTION' )
            // src/Kodkodi.g:1767:17: 'FUNCTION'
            {
            match("FUNCTION"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "FUNCTION"

    // $ANTLR start "IDEN"
    public final void mIDEN() throws RecognitionException {
        try {
            int _type = IDEN;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1768:5: ( 'iden' )
            // src/Kodkodi.g:1768:17: 'iden'
            {
            match("iden"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "IDEN"

    // $ANTLR start "IF"
    public final void mIF() throws RecognitionException {
        try {
            int _type = IF;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1769:3: ( 'if' )
            // src/Kodkodi.g:1769:17: 'if'
            {
            match("if"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "IF"

    // $ANTLR start "IN"
    public final void mIN() throws RecognitionException {
        try {
            int _type = IN;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1770:3: ( 'in' )
            // src/Kodkodi.g:1770:17: 'in'
            {
            match("in"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "IN"

    // $ANTLR start "INT"
    public final void mINT() throws RecognitionException {
        try {
            int _type = INT;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1771:4: ( 'Int' )
            // src/Kodkodi.g:1771:17: 'Int'
            {
            match("Int"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "INT"

    // $ANTLR start "INT_BOUNDS"
    public final void mINT_BOUNDS() throws RecognitionException {
        try {
            int _type = INT_BOUNDS;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1772:11: ( 'int_bounds' )
            // src/Kodkodi.g:1772:17: 'int_bounds'
            {
            match("int_bounds"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "INT_BOUNDS"

    // $ANTLR start "INTS"
    public final void mINTS() throws RecognitionException {
        try {
            int _type = INTS;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1773:5: ( 'ints' )
            // src/Kodkodi.g:1773:17: 'ints'
            {
            match("ints"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "INTS"

    // $ANTLR start "LET"
    public final void mLET() throws RecognitionException {
        try {
            int _type = LET;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1774:4: ( 'let' )
            // src/Kodkodi.g:1774:17: 'let'
            {
            match("let"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "LET"

    // $ANTLR start "LONE"
    public final void mLONE() throws RecognitionException {
        try {
            int _type = LONE;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1775:5: ( 'lone' )
            // src/Kodkodi.g:1775:17: 'lone'
            {
            match("lone"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "LONE"

    // $ANTLR start "NO"
    public final void mNO() throws RecognitionException {
        try {
            int _type = NO;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1776:3: ( 'no' )
            // src/Kodkodi.g:1776:17: 'no'
            {
            match("no"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "NO"

    // $ANTLR start "NONE"
    public final void mNONE() throws RecognitionException {
        try {
            int _type = NONE;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1777:5: ( 'none' )
            // src/Kodkodi.g:1777:17: 'none'
            {
            match("none"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "NONE"

    // $ANTLR start "ONE"
    public final void mONE() throws RecognitionException {
        try {
            int _type = ONE;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1778:4: ( 'one' )
            // src/Kodkodi.g:1778:17: 'one'
            {
            match("one"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "ONE"

    // $ANTLR start "RELATION"
    public final void mRELATION() throws RecognitionException {
        try {
            int _type = RELATION;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1779:9: ( 'relation' )
            // src/Kodkodi.g:1779:17: 'relation'
            {
            match("relation"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "RELATION"

    // $ANTLR start "SET"
    public final void mSET() throws RecognitionException {
        try {
            int _type = SET;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1780:4: ( 'set' )
            // src/Kodkodi.g:1780:17: 'set'
            {
            match("set"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "SET"

    // $ANTLR start "SGN"
    public final void mSGN() throws RecognitionException {
        try {
            int _type = SGN;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1781:4: ( 'sgn' )
            // src/Kodkodi.g:1781:17: 'sgn'
            {
            match("sgn"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "SGN"

    // $ANTLR start "SHARING"
    public final void mSHARING() throws RecognitionException {
        try {
            int _type = SHARING;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1782:8: ( 'sharing' )
            // src/Kodkodi.g:1782:17: 'sharing'
            {
            match("sharing"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "SHARING"

    // $ANTLR start "SKOLEM_DEPTH"
    public final void mSKOLEM_DEPTH() throws RecognitionException {
        try {
            int _type = SKOLEM_DEPTH;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1783:13: ( 'skolem_depth' )
            // src/Kodkodi.g:1783:17: 'skolem_depth'
            {
            match("skolem_depth"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "SKOLEM_DEPTH"

    // $ANTLR start "SOLVE"
    public final void mSOLVE() throws RecognitionException {
        try {
            int _type = SOLVE;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1784:6: ( 'solve' )
            // src/Kodkodi.g:1784:17: 'solve'
            {
            match("solve"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "SOLVE"

    // $ANTLR start "SOLVER"
    public final void mSOLVER() throws RecognitionException {
        try {
            int _type = SOLVER;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1785:7: ( 'solver' )
            // src/Kodkodi.g:1785:17: 'solver'
            {
            match("solver"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "SOLVER"

    // $ANTLR start "SOME"
    public final void mSOME() throws RecognitionException {
        try {
            int _type = SOME;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1786:5: ( 'some' )
            // src/Kodkodi.g:1786:17: 'some'
            {
            match("some"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "SOME"

    // $ANTLR start "SUM"
    public final void mSUM() throws RecognitionException {
        try {
            int _type = SUM;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1787:4: ( 'sum' )
            // src/Kodkodi.g:1787:17: 'sum'
            {
            match("sum"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "SUM"

    // $ANTLR start "SYMMETRY_BREAKING"
    public final void mSYMMETRY_BREAKING() throws RecognitionException {
        try {
            int _type = SYMMETRY_BREAKING;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1788:18: ( 'symmetry_breaking' )
            // src/Kodkodi.g:1789:17: 'symmetry_breaking'
            {
            match("symmetry_breaking"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "SYMMETRY_BREAKING"

    // $ANTLR start "THEN"
    public final void mTHEN() throws RecognitionException {
        try {
            int _type = THEN;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1790:5: ( 'then' )
            // src/Kodkodi.g:1790:17: 'then'
            {
            match("then"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "THEN"

    // $ANTLR start "TIMEOUT"
    public final void mTIMEOUT() throws RecognitionException {
        try {
            int _type = TIMEOUT;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1791:8: ( 'timeout' )
            // src/Kodkodi.g:1791:17: 'timeout'
            {
            match("timeout"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "TIMEOUT"

    // $ANTLR start "TOTAL_ORDERING"
    public final void mTOTAL_ORDERING() throws RecognitionException {
        try {
            int _type = TOTAL_ORDERING;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1792:15: ( 'TOTAL_ORDERING' )
            // src/Kodkodi.g:1792:17: 'TOTAL_ORDERING'
            {
            match("TOTAL_ORDERING"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "TOTAL_ORDERING"

    // $ANTLR start "TRUE"
    public final void mTRUE() throws RecognitionException {
        try {
            int _type = TRUE;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1793:5: ( 'true' )
            // src/Kodkodi.g:1793:17: 'true'
            {
            match("true"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "TRUE"

    // $ANTLR start "UNIV"
    public final void mUNIV() throws RecognitionException {
        try {
            int _type = UNIV;
            int _channel = DEFAULT_TOKEN_CHANNEL;
            // src/Kodkodi.g:1794:5: ( 'univ' )
            // src/Kodkodi.g:1794:17: 'univ'
            {
            match("univ"); 


            }

            state.type = _type;
            state.channel = _channel;
        }
        finally {
        }
    }
    // $ANTLR end "UNIV"

    public void mTokens() throws RecognitionException {
        // src/Kodkodi.g:1:8: ( ATOM_NAME | UNIV_NAME | OFFSET_UNIV_NAME | TUPLE_NAME | RELATION_NAME | VARIABLE_NAME | TUPLE_SET_REG | TUPLE_REG | FORMULA_REG | REL_EXPR_REG | INT_EXPR_REG | NUM | STR_LITERAL | WHITESPACE | INLINE_COMMENT | BLOCK_COMMENT | AMP | AND | ARROW | COLON_EQ | BAR | BRACE_LEFT | BRACE_RIGHT | BRACKET_LEFT | BRACKET_RIGHT | COLON | COMMA | DIVIDE | DOT | DOT_DOT | EQ | GE | GT | HASH | HAT | IFF | IFNO | IMPLIES | LT | LE | MINUS | MODULO | NOT | OVERRIDE | OR | PAREN_LEFT | PAREN_RIGHT | PLUS | SEMICOLON | SHA | SHL | SHR | STAR | TILDE | ABS | ACYCLIC | ALL | BITS | BIT_WIDTH | BOUNDS | DELAY | ELSE | FALSE | FLATTEN | FUNCTION | IDEN | IF | IN | INT | INT_BOUNDS | INTS | LET | LONE | NO | NONE | ONE | RELATION | SET | SGN | SHARING | SKOLEM_DEPTH | SOLVE | SOLVER | SOME | SUM | SYMMETRY_BREAKING | THEN | TIMEOUT | TOTAL_ORDERING | TRUE | UNIV )
        int alt16=91;
        alt16 = dfa16.predict(input);
        switch (alt16) {
            case 1 :
                // src/Kodkodi.g:1:10: ATOM_NAME
                {
                mATOM_NAME(); 

                }
                break;
            case 2 :
                // src/Kodkodi.g:1:20: UNIV_NAME
                {
                mUNIV_NAME(); 

                }
                break;
            case 3 :
                // src/Kodkodi.g:1:30: OFFSET_UNIV_NAME
                {
                mOFFSET_UNIV_NAME(); 

                }
                break;
            case 4 :
                // src/Kodkodi.g:1:47: TUPLE_NAME
                {
                mTUPLE_NAME(); 

                }
                break;
            case 5 :
                // src/Kodkodi.g:1:58: RELATION_NAME
                {
                mRELATION_NAME(); 

                }
                break;
            case 6 :
                // src/Kodkodi.g:1:72: VARIABLE_NAME
                {
                mVARIABLE_NAME(); 

                }
                break;
            case 7 :
                // src/Kodkodi.g:1:86: TUPLE_SET_REG
                {
                mTUPLE_SET_REG(); 

                }
                break;
            case 8 :
                // src/Kodkodi.g:1:100: TUPLE_REG
                {
                mTUPLE_REG(); 

                }
                break;
            case 9 :
                // src/Kodkodi.g:1:110: FORMULA_REG
                {
                mFORMULA_REG(); 

                }
                break;
            case 10 :
                // src/Kodkodi.g:1:122: REL_EXPR_REG
                {
                mREL_EXPR_REG(); 

                }
                break;
            case 11 :
                // src/Kodkodi.g:1:135: INT_EXPR_REG
                {
                mINT_EXPR_REG(); 

                }
                break;
            case 12 :
                // src/Kodkodi.g:1:148: NUM
                {
                mNUM(); 

                }
                break;
            case 13 :
                // src/Kodkodi.g:1:152: STR_LITERAL
                {
                mSTR_LITERAL(); 

                }
                break;
            case 14 :
                // src/Kodkodi.g:1:164: WHITESPACE
                {
                mWHITESPACE(); 

                }
                break;
            case 15 :
                // src/Kodkodi.g:1:175: INLINE_COMMENT
                {
                mINLINE_COMMENT(); 

                }
                break;
            case 16 :
                // src/Kodkodi.g:1:190: BLOCK_COMMENT
                {
                mBLOCK_COMMENT(); 

                }
                break;
            case 17 :
                // src/Kodkodi.g:1:204: AMP
                {
                mAMP(); 

                }
                break;
            case 18 :
                // src/Kodkodi.g:1:208: AND
                {
                mAND(); 

                }
                break;
            case 19 :
                // src/Kodkodi.g:1:212: ARROW
                {
                mARROW(); 

                }
                break;
            case 20 :
                // src/Kodkodi.g:1:218: COLON_EQ
                {
                mCOLON_EQ(); 

                }
                break;
            case 21 :
                // src/Kodkodi.g:1:227: BAR
                {
                mBAR(); 

                }
                break;
            case 22 :
                // src/Kodkodi.g:1:231: BRACE_LEFT
                {
                mBRACE_LEFT(); 

                }
                break;
            case 23 :
                // src/Kodkodi.g:1:242: BRACE_RIGHT
                {
                mBRACE_RIGHT(); 

                }
                break;
            case 24 :
                // src/Kodkodi.g:1:254: BRACKET_LEFT
                {
                mBRACKET_LEFT(); 

                }
                break;
            case 25 :
                // src/Kodkodi.g:1:267: BRACKET_RIGHT
                {
                mBRACKET_RIGHT(); 

                }
                break;
            case 26 :
                // src/Kodkodi.g:1:281: COLON
                {
                mCOLON(); 

                }
                break;
            case 27 :
                // src/Kodkodi.g:1:287: COMMA
                {
                mCOMMA(); 

                }
                break;
            case 28 :
                // src/Kodkodi.g:1:293: DIVIDE
                {
                mDIVIDE(); 

                }
                break;
            case 29 :
                // src/Kodkodi.g:1:300: DOT
                {
                mDOT(); 

                }
                break;
            case 30 :
                // src/Kodkodi.g:1:304: DOT_DOT
                {
                mDOT_DOT(); 

                }
                break;
            case 31 :
                // src/Kodkodi.g:1:312: EQ
                {
                mEQ(); 

                }
                break;
            case 32 :
                // src/Kodkodi.g:1:315: GE
                {
                mGE(); 

                }
                break;
            case 33 :
                // src/Kodkodi.g:1:318: GT
                {
                mGT(); 

                }
                break;
            case 34 :
                // src/Kodkodi.g:1:321: HASH
                {
                mHASH(); 

                }
                break;
            case 35 :
                // src/Kodkodi.g:1:326: HAT
                {
                mHAT(); 

                }
                break;
            case 36 :
                // src/Kodkodi.g:1:330: IFF
                {
                mIFF(); 

                }
                break;
            case 37 :
                // src/Kodkodi.g:1:334: IFNO
                {
                mIFNO(); 

                }
                break;
            case 38 :
                // src/Kodkodi.g:1:339: IMPLIES
                {
                mIMPLIES(); 

                }
                break;
            case 39 :
                // src/Kodkodi.g:1:347: LT
                {
                mLT(); 

                }
                break;
            case 40 :
                // src/Kodkodi.g:1:350: LE
                {
                mLE(); 

                }
                break;
            case 41 :
                // src/Kodkodi.g:1:353: MINUS
                {
                mMINUS(); 

                }
                break;
            case 42 :
                // src/Kodkodi.g:1:359: MODULO
                {
                mMODULO(); 

                }
                break;
            case 43 :
                // src/Kodkodi.g:1:366: NOT
                {
                mNOT(); 

                }
                break;
            case 44 :
                // src/Kodkodi.g:1:370: OVERRIDE
                {
                mOVERRIDE(); 

                }
                break;
            case 45 :
                // src/Kodkodi.g:1:379: OR
                {
                mOR(); 

                }
                break;
            case 46 :
                // src/Kodkodi.g:1:382: PAREN_LEFT
                {
                mPAREN_LEFT(); 

                }
                break;
            case 47 :
                // src/Kodkodi.g:1:393: PAREN_RIGHT
                {
                mPAREN_RIGHT(); 

                }
                break;
            case 48 :
                // src/Kodkodi.g:1:405: PLUS
                {
                mPLUS(); 

                }
                break;
            case 49 :
                // src/Kodkodi.g:1:410: SEMICOLON
                {
                mSEMICOLON(); 

                }
                break;
            case 50 :
                // src/Kodkodi.g:1:420: SHA
                {
                mSHA(); 

                }
                break;
            case 51 :
                // src/Kodkodi.g:1:424: SHL
                {
                mSHL(); 

                }
                break;
            case 52 :
                // src/Kodkodi.g:1:428: SHR
                {
                mSHR(); 

                }
                break;
            case 53 :
                // src/Kodkodi.g:1:432: STAR
                {
                mSTAR(); 

                }
                break;
            case 54 :
                // src/Kodkodi.g:1:437: TILDE
                {
                mTILDE(); 

                }
                break;
            case 55 :
                // src/Kodkodi.g:1:443: ABS
                {
                mABS(); 

                }
                break;
            case 56 :
                // src/Kodkodi.g:1:447: ACYCLIC
                {
                mACYCLIC(); 

                }
                break;
            case 57 :
                // src/Kodkodi.g:1:455: ALL
                {
                mALL(); 

                }
                break;
            case 58 :
                // src/Kodkodi.g:1:459: BITS
                {
                mBITS(); 

                }
                break;
            case 59 :
                // src/Kodkodi.g:1:464: BIT_WIDTH
                {
                mBIT_WIDTH(); 

                }
                break;
            case 60 :
                // src/Kodkodi.g:1:474: BOUNDS
                {
                mBOUNDS(); 

                }
                break;
            case 61 :
                // src/Kodkodi.g:1:481: DELAY
                {
                mDELAY(); 

                }
                break;
            case 62 :
                // src/Kodkodi.g:1:487: ELSE
                {
                mELSE(); 

                }
                break;
            case 63 :
                // src/Kodkodi.g:1:492: FALSE
                {
                mFALSE(); 

                }
                break;
            case 64 :
                // src/Kodkodi.g:1:498: FLATTEN
                {
                mFLATTEN(); 

                }
                break;
            case 65 :
                // src/Kodkodi.g:1:506: FUNCTION
                {
                mFUNCTION(); 

                }
                break;
            case 66 :
                // src/Kodkodi.g:1:515: IDEN
                {
                mIDEN(); 

                }
                break;
            case 67 :
                // src/Kodkodi.g:1:520: IF
                {
                mIF(); 

                }
                break;
            case 68 :
                // src/Kodkodi.g:1:523: IN
                {
                mIN(); 

                }
                break;
            case 69 :
                // src/Kodkodi.g:1:526: INT
                {
                mINT(); 

                }
                break;
            case 70 :
                // src/Kodkodi.g:1:530: INT_BOUNDS
                {
                mINT_BOUNDS(); 

                }
                break;
            case 71 :
                // src/Kodkodi.g:1:541: INTS
                {
                mINTS(); 

                }
                break;
            case 72 :
                // src/Kodkodi.g:1:546: LET
                {
                mLET(); 

                }
                break;
            case 73 :
                // src/Kodkodi.g:1:550: LONE
                {
                mLONE(); 

                }
                break;
            case 74 :
                // src/Kodkodi.g:1:555: NO
                {
                mNO(); 

                }
                break;
            case 75 :
                // src/Kodkodi.g:1:558: NONE
                {
                mNONE(); 

                }
                break;
            case 76 :
                // src/Kodkodi.g:1:563: ONE
                {
                mONE(); 

                }
                break;
            case 77 :
                // src/Kodkodi.g:1:567: RELATION
                {
                mRELATION(); 

                }
                break;
            case 78 :
                // src/Kodkodi.g:1:576: SET
                {
                mSET(); 

                }
                break;
            case 79 :
                // src/Kodkodi.g:1:580: SGN
                {
                mSGN(); 

                }
                break;
            case 80 :
                // src/Kodkodi.g:1:584: SHARING
                {
                mSHARING(); 

                }
                break;
            case 81 :
                // src/Kodkodi.g:1:592: SKOLEM_DEPTH
                {
                mSKOLEM_DEPTH(); 

                }
                break;
            case 82 :
                // src/Kodkodi.g:1:605: SOLVE
                {
                mSOLVE(); 

                }
                break;
            case 83 :
                // src/Kodkodi.g:1:611: SOLVER
                {
                mSOLVER(); 

                }
                break;
            case 84 :
                // src/Kodkodi.g:1:618: SOME
                {
                mSOME(); 

                }
                break;
            case 85 :
                // src/Kodkodi.g:1:623: SUM
                {
                mSUM(); 

                }
                break;
            case 86 :
                // src/Kodkodi.g:1:627: SYMMETRY_BREAKING
                {
                mSYMMETRY_BREAKING(); 

                }
                break;
            case 87 :
                // src/Kodkodi.g:1:645: THEN
                {
                mTHEN(); 

                }
                break;
            case 88 :
                // src/Kodkodi.g:1:650: TIMEOUT
                {
                mTIMEOUT(); 

                }
                break;
            case 89 :
                // src/Kodkodi.g:1:658: TOTAL_ORDERING
                {
                mTOTAL_ORDERING(); 

                }
                break;
            case 90 :
                // src/Kodkodi.g:1:673: TRUE
                {
                mTRUE(); 

                }
                break;
            case 91 :
                // src/Kodkodi.g:1:678: UNIV
                {
                mUNIV(); 

                }
                break;

        }

    }


    protected DFA16 dfa16 = new DFA16(this);
    static final String DFA16_eotS =
        "\12\uffff\1\107\3\uffff\1\112\1\114\1\116\1\120\1\122\5\uffff\1"+
        "\124\1\126\1\131\2\uffff\1\134\30\uffff\2\154\40\uffff\1\162\1\uffff"+
        "\1\164\12\uffff\1\166\2\uffff\1\170\5\uffff\1\154\15\uffff\1\176"+
        "\2\uffff";
    static final String DFA16_eofS =
        "\177\uffff";
    static final String DFA16_minS =
        "\1\11\2\60\1\uffff\3\60\2\uffff\1\101\1\60\3\uffff\1\52\1\46\1\53"+
        "\1\75\1\174\5\uffff\1\56\1\76\1\75\2\uffff\1\74\10\uffff\1\142\1"+
        "\uffff\1\151\2\uffff\1\141\1\uffff\1\144\1\uffff\1\145\1\157\1\uffff"+
        "\1\150\3\uffff\1\100\1\60\5\uffff\1\154\32\uffff\1\76\1\uffff\1"+
        "\76\12\uffff\1\164\2\uffff\1\156\5\uffff\1\60\1\166\5\uffff\1\137"+
        "\3\uffff\1\145\2\uffff\1\162\2\uffff";
    static final String DFA16_maxS =
        "\1\176\1\103\1\156\1\uffff\1\117\1\171\1\145\2\uffff\1\164\1\76"+
        "\3\uffff\1\57\1\46\1\71\1\75\1\174\5\uffff\1\56\2\76\2\uffff\1\75"+
        "\10\uffff\1\154\1\uffff\1\157\2\uffff\1\154\1\uffff\1\156\1\uffff"+
        "\2\157\1\uffff\1\162\3\uffff\2\100\5\uffff\1\155\32\uffff\1\76\1"+
        "\uffff\1\76\12\uffff\1\164\2\uffff\1\156\5\uffff\1\100\1\166\5\uffff"+
        "\1\163\3\uffff\1\145\2\uffff\1\162\2\uffff";
    static final String DFA16_acceptS =
        "\3\uffff\1\4\3\uffff\1\5\1\6\2\uffff\1\14\1\15\1\16\5\uffff\1\26"+
        "\1\27\1\30\1\31\1\33\3\uffff\1\42\1\43\1\uffff\1\45\1\52\1\53\1"+
        "\56\1\57\1\61\1\65\1\66\1\uffff\1\72\1\uffff\1\75\1\76\1\uffff\1"+
        "\101\1\uffff\1\105\2\uffff\1\114\1\uffff\1\70\1\1\1\133\2\uffff"+
        "\1\131\1\116\1\117\1\120\1\121\1\uffff\1\125\1\126\1\115\1\11\1"+
        "\12\1\13\1\7\1\10\1\23\1\51\1\17\1\20\1\34\1\22\1\21\1\54\1\60\1"+
        "\24\1\32\1\55\1\25\1\36\1\35\1\46\1\37\1\40\1\uffff\1\41\1\uffff"+
        "\1\63\1\47\1\67\1\71\1\73\1\74\1\77\1\100\1\102\1\103\1\uffff\1"+
        "\110\1\111\1\uffff\1\127\1\130\1\132\1\2\1\3\2\uffff\1\124\1\64"+
        "\1\62\1\44\1\50\1\uffff\1\104\1\113\1\112\1\uffff\1\106\1\107\1"+
        "\uffff\1\123\1\122";
    static final String DFA16_specialS =
        "\177\uffff}>";
    static final String[] DFA16_transitionS = {
            "\2\15\2\uffff\1\15\22\uffff\1\15\1\40\1\14\1\33\1\11\1\37\1"+
            "\17\1\uffff\1\41\1\42\1\44\1\20\1\27\1\12\1\30\1\16\12\13\1"+
            "\21\1\43\1\35\1\31\1\32\2\uffff\1\1\1\47\3\uffff\1\54\2\uffff"+
            "\1\56\3\uffff\1\10\2\uffff\1\3\1\uffff\2\10\1\4\6\uffff\1\25"+
            "\1\36\1\26\1\34\2\uffff\1\46\1\50\1\uffff\1\51\1\52\1\53\2\uffff"+
            "\1\55\2\uffff\1\57\1\7\1\60\1\61\2\uffff\1\6\1\5\1\62\1\2\1"+
            "\15\4\uffff\1\23\1\22\1\24\1\45",
            "\12\64\11\uffff\1\63",
            "\1\66\11\67\64\uffff\1\65",
            "",
            "\12\3\25\uffff\1\70",
            "\12\7\53\uffff\1\71\1\uffff\1\72\1\73\2\uffff\1\74\3\uffff"+
            "\1\75\5\uffff\1\76\3\uffff\1\77",
            "\12\7\53\uffff\1\100",
            "",
            "",
            "\1\105\16\uffff\1\105\3\uffff\1\105\14\uffff\1\104\3\uffff"+
            "\1\102\1\101\2\uffff\1\103\6\uffff\1\104\3\uffff\1\104",
            "\12\13\4\uffff\1\106",
            "",
            "",
            "",
            "\1\111\4\uffff\1\110",
            "\1\113",
            "\1\115\4\uffff\12\13",
            "\1\117",
            "\1\121",
            "",
            "",
            "",
            "",
            "",
            "\1\123",
            "\1\125",
            "\1\127\1\130",
            "",
            "",
            "\1\133\1\132",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "\1\135\11\uffff\1\136",
            "",
            "\1\137\5\uffff\1\140",
            "",
            "",
            "\1\141\12\uffff\1\142",
            "",
            "\1\143\1\uffff\1\144\7\uffff\1\145",
            "",
            "\1\146\11\uffff\1\147",
            "\1\150",
            "",
            "\1\151\1\152\10\uffff\1\153",
            "",
            "",
            "",
            "\1\155",
            "\12\156\6\uffff\1\155",
            "",
            "",
            "",
            "",
            "",
            "\1\157\1\160",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "\1\161",
            "",
            "\1\163",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "\1\165",
            "",
            "",
            "\1\167",
            "",
            "",
            "",
            "",
            "",
            "\12\156\6\uffff\1\155",
            "\1\171",
            "",
            "",
            "",
            "",
            "",
            "\1\172\23\uffff\1\173",
            "",
            "",
            "",
            "\1\174",
            "",
            "",
            "\1\175",
            "",
            ""
    };

    static final short[] DFA16_eot = DFA.unpackEncodedString(DFA16_eotS);
    static final short[] DFA16_eof = DFA.unpackEncodedString(DFA16_eofS);
    static final char[] DFA16_min = DFA.unpackEncodedStringToUnsignedChars(DFA16_minS);
    static final char[] DFA16_max = DFA.unpackEncodedStringToUnsignedChars(DFA16_maxS);
    static final short[] DFA16_accept = DFA.unpackEncodedString(DFA16_acceptS);
    static final short[] DFA16_special = DFA.unpackEncodedString(DFA16_specialS);
    static final short[][] DFA16_transition;

    static {
        int numStates = DFA16_transitionS.length;
        DFA16_transition = new short[numStates][];
        for (int i=0; i<numStates; i++) {
            DFA16_transition[i] = DFA.unpackEncodedString(DFA16_transitionS[i]);
        }
    }

    class DFA16 extends DFA {

        public DFA16(BaseRecognizer recognizer) {
            this.recognizer = recognizer;
            this.decisionNumber = 16;
            this.eot = DFA16_eot;
            this.eof = DFA16_eof;
            this.min = DFA16_min;
            this.max = DFA16_max;
            this.accept = DFA16_accept;
            this.special = DFA16_special;
            this.transition = DFA16_transition;
        }
        public String getDescription() {
            return "1:1: Tokens : ( ATOM_NAME | UNIV_NAME | OFFSET_UNIV_NAME | TUPLE_NAME | RELATION_NAME | VARIABLE_NAME | TUPLE_SET_REG | TUPLE_REG | FORMULA_REG | REL_EXPR_REG | INT_EXPR_REG | NUM | STR_LITERAL | WHITESPACE | INLINE_COMMENT | BLOCK_COMMENT | AMP | AND | ARROW | COLON_EQ | BAR | BRACE_LEFT | BRACE_RIGHT | BRACKET_LEFT | BRACKET_RIGHT | COLON | COMMA | DIVIDE | DOT | DOT_DOT | EQ | GE | GT | HASH | HAT | IFF | IFNO | IMPLIES | LT | LE | MINUS | MODULO | NOT | OVERRIDE | OR | PAREN_LEFT | PAREN_RIGHT | PLUS | SEMICOLON | SHA | SHL | SHR | STAR | TILDE | ABS | ACYCLIC | ALL | BITS | BIT_WIDTH | BOUNDS | DELAY | ELSE | FALSE | FLATTEN | FUNCTION | IDEN | IF | IN | INT | INT_BOUNDS | INTS | LET | LONE | NO | NONE | ONE | RELATION | SET | SGN | SHARING | SKOLEM_DEPTH | SOLVE | SOLVER | SOME | SUM | SYMMETRY_BREAKING | THEN | TIMEOUT | TOTAL_ORDERING | TRUE | UNIV );";
        }
    }
 

}