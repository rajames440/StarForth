/*
  --------------------------------------------------------------
  Computing hash keys from datastructures
  --------------------------------------------------------------
*/

#ifndef _HK_H
#define _HK_H

/**
   \brief finalize hash key computation
   \param k original hash key
   \return the hash key */
#define h_inc_end(k) (k += (k << 3), k ^= (k >> 11), k += (k << 15), k)

/**
   \brief incrementally add hash key for unsigned
   \param k original hash key
   \param u unsigned
   \return the hash key
   \remark Use with k = 0 at first, and finalise with h_inc_end function */
#define h_unsigned_inc(k, u) ((((k) + (u)) << 10) ^ (((k) + (u)) >> 6))

/**
   \brief incrementally add h key for string
   \param k original h key
   \param str a string
   \return the hash key
   \remark Use with k = 0 at first, and finalise with h_inc_end function */
static __inline__ unsigned
h_string_inc(unsigned k, char* str)
{
	for (; *str; str++) {
		k += (unsigned)*str;
		k += (k << 10);
		k ^= (k >> 6);
	}
	return k;
}

/**
   \brief computes hash key for unsigned
   \param u an unsigned
   \return the hash key */
static __inline__ unsigned
h_unsigned(unsigned u)
{
	unsigned k = h_unsigned_inc(0, u);
	return h_inc_end(k);
}

/**
   \brief computes h key for string
   \param str a string
   \return the hash key */
static __inline__ unsigned
h_string(char* str)
{
	unsigned k = h_string_inc(0, str);
	return h_inc_end(k);
}

#endif /* _HK_H */
