#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>
#include "sha1.h"

void* (*sha1)(const char*, size_t, void*);

void test_sha1(const char* buffer, const char* result)
{
  unsigned char digest[20];
  char digest_string[41];
  int i;

  sha1(buffer, strlen(buffer), (void*) digest);

  for (i=0; i<20; i++)
  {
    sprintf(digest_string + 2*i, "%x%x", digest[i] / 16, digest[i] % 16);
  }

  printf("computed: %s\n", digest_string);
  printf("expected: %s\n", result);

  if (strcmp(digest_string, result))
  {
    fprintf(stderr, "ERROR: bad SHA1 sum for '%s'\n", buffer);
    exit(1);
  }
}

int main(int argc, char** argv)
{
  void *lib_handle;
  char *error;

  if (argc < 2)
  {
    fprintf(stderr, "ERROR: missing argument: path of SHA1 library\n");
    exit(1);
  }

  lib_handle = dlopen(argv[1], RTLD_LAZY);
  if (!lib_handle)
  {
    fprintf(stderr, "ERROR: %s\n", dlerror());
    exit(1);
  }

  sha1 = dlsym(lib_handle, "sha1_buffer");
  error = dlerror();
  if (error != NULL)  
  {
    fprintf(stderr, "ERROR: %s\n", error);
    exit(1);
  }

  test_sha1("",
    "da39a3ee5e6b4b0d3255bfef95601890afd80709");

  test_sha1("abc",
    "a9993e364706816aba3e25717850c26c9cd0d89d");

  test_sha1("abcdefghijklmnopqrstuvwxyz",
    "32d10c7b8cf96570ca04ce37f2a19d84240d3a89");

  test_sha1("The quick brown fox jumps over the lazy dog",
    "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12");

  test_sha1("The quick brown fox jumps over the lazy cog",
    "de9f2c7fd25e1b3afad3e85a0bd17d9b100db4b3");

  printf("all tests succeeded\n");

  dlclose(lib_handle);
  return 0;
}

