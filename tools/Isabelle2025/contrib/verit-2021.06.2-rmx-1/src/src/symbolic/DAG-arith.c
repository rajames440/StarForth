/**
   Module for representing arithmetic formulas and terms
*/

#include "symbolic/DAG-arith.h"

#include "symbolic/DAG-symb.h"

#include <gmp.h>

TDAG
DAG_new_integer(long value)
{
	return DAG_new_nullary(DAG_symb_integer(value));
}

TDAG
DAG_new_integer_mpz(mpz_t mpz)
{
	return DAG_new_nullary(DAG_symb_integer_mpz(mpz));
}

TDAG
DAG_new_integer_str(char* value)
{
	return DAG_new_nullary(DAG_symb_integer_str(value));
}

TDAG
DAG_new_binary_str(char* value)
{
	return DAG_new_nullary(DAG_symb_binary_str(value));
}

TDAG
DAG_new_hex_str(char* value)
{
	return DAG_new_nullary(DAG_symb_hex_str(value));
}

TDAG
DAG_new_rational(long num, unsigned long den)
{
	return DAG_new_nullary(DAG_symb_rational(num, den));
}

TDAG
DAG_new_rational_mpq(mpq_t mpq)
{
	return DAG_new_nullary(DAG_symb_rational_mpq(mpq));
}

TDAG
DAG_new_rational_str(char* value)
{
	return DAG_new_nullary(DAG_symb_rational_str(value));
}

TDAG
DAG_new_decimal_str(char* value)
{
	return DAG_new_nullary(DAG_symb_decimal_str(value));
}

TDAG
DAG_new_str(char* value)
{
	return DAG_new_nullary(DAG_symb_str(value));
}

bool
DAG_is_rational(TDAG DAG)
{
	return DAG_symb_type(DAG_symb(DAG)) & SYMB_NUMBER;
}

bool
DAG_is_integer(TDAG DAG)
{
	return DAG_symb_type(DAG_symb(DAG)) & SYMB_INTEGER;
}

bool
DAG_is_number(TDAG DAG)
{
	return DAG_is_integer(DAG) || DAG_is_rational(DAG);
}

void
DAG_mpz_set(mpz_t mpz, TDAG DAG)
{
	assert(DAG_is_integer(DAG));
	mpz_set(mpz, DAG_symb_mpz(DAG_symb(DAG)));
}

void
DAG_mpq_set(mpq_t mpq, TDAG DAG)
{
	if (DAG_is_integer(DAG)) {
		mpq_set_z(mpq, DAG_symb_mpz(DAG_symb(DAG)));
		return;
	}
	assert(DAG_is_rational(DAG));
	mpq_set(mpq, DAG_symb_mpq(DAG_symb(DAG)));
}

char*
DAG_number_str(TDAG DAG)
{
	char *str = NULL, *tmp;
	size_t str_len = 0;
	bool neg;
	if (DAG_is_integer(DAG)) {
		neg = (mpz_sgn(DAG_symb_mpz(DAG_symb(DAG))) < 0);
		if (neg) str = strapp(str, &str_len, "(- ");
		tmp = mpz_get_str(NULL, 10, DAG_symb_mpz(DAG_symb(DAG)));
		str = strapp(str, &str_len, tmp + (neg ? 1 : 0));
		free(tmp);
		if (neg) str = strapp(str, &str_len, ")");
		return str;
	}
	assert(DAG_is_rational(DAG));
	neg = (mpz_sgn(mpq_numref(DAG_symb_mpq(DAG_symb(DAG)))) < 0);
	if (neg) str = strapp(str, &str_len, "(- ");
	str = strapp(str, &str_len, "(/ ");
	tmp = mpz_get_str(NULL, 10, mpq_numref(DAG_symb_mpq(DAG_symb(DAG))));
	str = strapp(str, &str_len, tmp + (neg ? 1 : 0));
	free(tmp);
	str = strapp(str, &str_len, " ");
	tmp = mpz_get_str(NULL, 10, mpq_denref(DAG_symb_mpq(DAG_symb(DAG))));
	str = strapp(str, &str_len, tmp);
	free(tmp);
	str = strapp(str, &str_len, neg ? "))" : ")");
	return str;
}
