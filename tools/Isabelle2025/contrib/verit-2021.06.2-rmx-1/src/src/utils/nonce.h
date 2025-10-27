/**
   \brief Provides an API to generate different names with a given prefix.

   \note Usage protocol:

   MAIN(n, prefix) = nonce_init(n, prefix) -> RUN(n)
   RUN(n) = (nonce_next(n) -> {reads/copies n->buffer} -> RUN) | END(n)
   END(n) = nonce_free(n) -> (SKIP | MAIN(n, prefix'))

   After each nonce_next(n) call, a new name is available in n->buffer.
   This string needs to be copied by the client if it must be kept across
   calls. */
#ifndef __NONCE_H
#define __NONCE_H

typedef struct Snonce
{
	char* prefix; /*< name common to all generated names */
	unsigned n; /*< number of names already generated */
	unsigned sz; /*< size of name that has been generated */
	unsigned resize_n; /*< value of n when size of buffer must be increased */
	char* buffer; /*< this is where the last generated name is stored */
} Tnonce;

/**
   \brief Initializes a name generator with the given prefix.
   \note Only call once. */
extern void nonce_init(Tnonce*, const char*);

/**
   \brief Frees the allocated resource.
   \note Only call after nonce_init. Pointed structure can no
   longer be used afterwards */
extern void nonce_free(Tnonce* P);

/**
   \brief Stores a new name in P->buffer */
extern void nonce_next(Tnonce* P);

#endif
