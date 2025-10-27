#ifndef __H_UTIL_H
#define __H_UTIL_H

#define HASH_MACRO

/**
   \brief general purpose hash function for strings
   \param str a string to get a hash for
   \return hash key for string */
unsigned hash_one_at_a_time(char* str);

/**
   \brief general purpose hash function for unsigned
   \param u an unsigned to get a hash for
   \return hash key for unsigned */
unsigned hash_one_at_a_time_u(unsigned u);

/**
   \brief incremental general purpose hash function for string
   \param str a string to get a hash for
   \return hash key for unsigned and previous hash
   \remark use with hash = 0 at first, and finalise with _end function below */
unsigned hash_one_at_a_time_inc(unsigned hash, char* str);

/**
   \brief incremental general purpose hash function for unsigned
   \param u an unsigned to get a hash for
   \return hash key for unsigned and previous hash
   \remark use with hash = 0 at first, and finalise with _end function below */
#ifdef HASH_MACRO
#define hash_one_at_a_time_u_inc(hash, u) \
	((((hash) + (u)) << 10) ^ (((hash) + (u)) >> 6))
#else
unsigned hash_one_at_a_time_u_inc(unsigned hash, unsigned u);
#endif

/**
   \brief finalise hash key
   \param hash a hash key
   \return hash key finalised */
unsigned hash_one_at_a_time_inc_end(unsigned hash);

#endif /* __H_UTIL_H */
