lexer grammar Kodkodi;

@header {
package isabelle.kodkodi;
}
@members {
public void emitErrorMessage(String message) {
    System.err.println(message);
}
}

// $ANTLR src "src/Kodkodi.g" 1698
ATOM_NAME:      'A' NAT;
// $ANTLR src "src/Kodkodi.g" 1699
UNIV_NAME:      'u' NAT;
// $ANTLR src "src/Kodkodi.g" 1700
OFFSET_UNIV_NAME:
                'u' NAT '@' NAT;
// $ANTLR src "src/Kodkodi.g" 1702
TUPLE_NAME:     ('P' | 'T' NAT '_') NAT;
// $ANTLR src "src/Kodkodi.g" 1703
RELATION_NAME:  ('s' | 'r' | 'm' NAT '_') NAT '\''?;
// $ANTLR src "src/Kodkodi.g" 1704
VARIABLE_NAME:  ('S' | 'R' | 'M' NAT '_') NAT '\''?;
// $ANTLR src "src/Kodkodi.g" 1705
TUPLE_SET_REG:  '$' ('a' | 'p' | 't' NAT '_') NAT;
// $ANTLR src "src/Kodkodi.g" 1706
TUPLE_REG:      '$' ('A' | 'P' | 'T' NAT '_') NAT;
// $ANTLR src "src/Kodkodi.g" 1707
FORMULA_REG:    '$f' NAT;
// $ANTLR src "src/Kodkodi.g" 1708
REL_EXPR_REG:   '$e' NAT;
// $ANTLR src "src/Kodkodi.g" 1709
INT_EXPR_REG:   '$i' NAT;

// $ANTLR src "src/Kodkodi.g" 1711
NUM:            (PLUS | MINUS)? '0'..'9'+;
// $ANTLR src "src/Kodkodi.g" 1712
fragment NAT:   '0' | '1'..'9' '0'..'9'*;
// $ANTLR src "src/Kodkodi.g" 1713
STR_LITERAL:    '"' ~('"' | '\n')* '"';
// $ANTLR src "src/Kodkodi.g" 1714
WHITESPACE:     (' ' | '\n' | '\r' | '\t' | '\v')+ { skip(); };
// $ANTLR src "src/Kodkodi.g" 1715
INLINE_COMMENT: '//' ~('\n')* { skip(); };
// $ANTLR src "src/Kodkodi.g" 1716
BLOCK_COMMENT:  '/*' (options { greedy = false; } : .)* '*/' { skip(); };

// $ANTLR src "src/Kodkodi.g" 1718
AMP:            '&';
// $ANTLR src "src/Kodkodi.g" 1719
AND:            '&&';
// $ANTLR src "src/Kodkodi.g" 1720
ARROW:          '->';
// $ANTLR src "src/Kodkodi.g" 1721
COLON_EQ:       ':=';
// $ANTLR src "src/Kodkodi.g" 1722
BAR:            '|';
// $ANTLR src "src/Kodkodi.g" 1723
BRACE_LEFT:     '{';
// $ANTLR src "src/Kodkodi.g" 1724
BRACE_RIGHT:    '}';
// $ANTLR src "src/Kodkodi.g" 1725
BRACKET_LEFT:   '[';
// $ANTLR src "src/Kodkodi.g" 1726
BRACKET_RIGHT:  ']';
// $ANTLR src "src/Kodkodi.g" 1727
COLON:          ':';
// $ANTLR src "src/Kodkodi.g" 1728
COMMA:          ',';
// $ANTLR src "src/Kodkodi.g" 1729
DIVIDE:         '/';
// $ANTLR src "src/Kodkodi.g" 1730
DOT:            '.';
// $ANTLR src "src/Kodkodi.g" 1731
DOT_DOT:        '..';
// $ANTLR src "src/Kodkodi.g" 1732
EQ:             '=';
// $ANTLR src "src/Kodkodi.g" 1733
GE:             '>=';
// $ANTLR src "src/Kodkodi.g" 1734
GT:             '>';
// $ANTLR src "src/Kodkodi.g" 1735
HASH:           '#';
// $ANTLR src "src/Kodkodi.g" 1736
HAT:            '^';
// $ANTLR src "src/Kodkodi.g" 1737
IFF:            '<=>';
// $ANTLR src "src/Kodkodi.g" 1738
IFNO:           '\\';
// $ANTLR src "src/Kodkodi.g" 1739
IMPLIES:        '=>';
// $ANTLR src "src/Kodkodi.g" 1740
LT:             '<';
// $ANTLR src "src/Kodkodi.g" 1741
LE:             '<=';
// $ANTLR src "src/Kodkodi.g" 1742
MINUS :         '-';
// $ANTLR src "src/Kodkodi.g" 1743
MODULO:         '%';
// $ANTLR src "src/Kodkodi.g" 1744
NOT:            '!';
// $ANTLR src "src/Kodkodi.g" 1745
OVERRIDE :      '++';
// $ANTLR src "src/Kodkodi.g" 1746
OR:             '||';
// $ANTLR src "src/Kodkodi.g" 1747
PAREN_LEFT:     '(';
// $ANTLR src "src/Kodkodi.g" 1748
PAREN_RIGHT:    ')';
// $ANTLR src "src/Kodkodi.g" 1749
PLUS:           '+';
// $ANTLR src "src/Kodkodi.g" 1750
SEMICOLON:      ';';
// $ANTLR src "src/Kodkodi.g" 1751
SHA:            '>>';
// $ANTLR src "src/Kodkodi.g" 1752
SHL:            '<<';
// $ANTLR src "src/Kodkodi.g" 1753
SHR:            '>>>';
// $ANTLR src "src/Kodkodi.g" 1754
STAR:           '*';
// $ANTLR src "src/Kodkodi.g" 1755
TILDE:          '~';

// $ANTLR src "src/Kodkodi.g" 1757
ABS:            'abs';
// $ANTLR src "src/Kodkodi.g" 1758
ACYCLIC:        'ACYCLIC';
// $ANTLR src "src/Kodkodi.g" 1759
ALL:            'all';
// $ANTLR src "src/Kodkodi.g" 1760
BITS:           'Bits';
// $ANTLR src "src/Kodkodi.g" 1761
BIT_WIDTH:      'bit_width';
// $ANTLR src "src/Kodkodi.g" 1762
BOUNDS:         'bounds';
// $ANTLR src "src/Kodkodi.g" 1763
DELAY:          'delay';
// $ANTLR src "src/Kodkodi.g" 1764
ELSE:           'else';
// $ANTLR src "src/Kodkodi.g" 1765
FALSE:          'false';
// $ANTLR src "src/Kodkodi.g" 1766
FLATTEN:        'flatten';
// $ANTLR src "src/Kodkodi.g" 1767
FUNCTION:       'FUNCTION';
// $ANTLR src "src/Kodkodi.g" 1768
IDEN:           'iden';
// $ANTLR src "src/Kodkodi.g" 1769
IF:             'if';
// $ANTLR src "src/Kodkodi.g" 1770
IN:             'in';
// $ANTLR src "src/Kodkodi.g" 1771
INT:            'Int';
// $ANTLR src "src/Kodkodi.g" 1772
INT_BOUNDS:     'int_bounds';
// $ANTLR src "src/Kodkodi.g" 1773
INTS:           'ints';
// $ANTLR src "src/Kodkodi.g" 1774
LET:            'let';
// $ANTLR src "src/Kodkodi.g" 1775
LONE:           'lone';
// $ANTLR src "src/Kodkodi.g" 1776
NO:             'no';
// $ANTLR src "src/Kodkodi.g" 1777
NONE:           'none';
// $ANTLR src "src/Kodkodi.g" 1778
ONE:            'one';
// $ANTLR src "src/Kodkodi.g" 1779
RELATION:       'relation';
// $ANTLR src "src/Kodkodi.g" 1780
SET:            'set';
// $ANTLR src "src/Kodkodi.g" 1781
SGN:            'sgn';
// $ANTLR src "src/Kodkodi.g" 1782
SHARING:        'sharing';
// $ANTLR src "src/Kodkodi.g" 1783
SKOLEM_DEPTH:   'skolem_depth';
// $ANTLR src "src/Kodkodi.g" 1784
SOLVE:          'solve';
// $ANTLR src "src/Kodkodi.g" 1785
SOLVER:         'solver';
// $ANTLR src "src/Kodkodi.g" 1786
SOME:           'some';
// $ANTLR src "src/Kodkodi.g" 1787
SUM:            'sum';
// $ANTLR src "src/Kodkodi.g" 1788
SYMMETRY_BREAKING:
                'symmetry_breaking';
// $ANTLR src "src/Kodkodi.g" 1790
THEN:           'then';
// $ANTLR src "src/Kodkodi.g" 1791
TIMEOUT:        'timeout';
// $ANTLR src "src/Kodkodi.g" 1792
TOTAL_ORDERING: 'TOTAL_ORDERING';
// $ANTLR src "src/Kodkodi.g" 1793
TRUE:           'true';
// $ANTLR src "src/Kodkodi.g" 1794
UNIV:           'univ';
