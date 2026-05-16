/* Freestanding stdio.h shim for aarch64 COFF kernel builds.
 * printf/fprintf/snprintf/vfprintf all provided by shim.c (route to console). */
#ifndef FREESTANDING_STDIO_H
#define FREESTANDING_STDIO_H
#include <stddef.h>
#include <stdarg.h>
typedef struct { int _dummy; } FILE;
#define stderr ((FILE *)0)
#define stdout ((FILE *)1)
#define stdin  ((FILE *)2)
int printf(const char *fmt, ...);
int fprintf(FILE *stream, const char *fmt, ...);
int vfprintf(FILE *stream, const char *fmt, va_list args);
int snprintf(char *buf, size_t n, const char *fmt, ...);
int vsnprintf(char *buf, size_t n, const char *fmt, va_list args);
int puts(const char *s);
int fputs(const char *s, FILE *stream);
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
int    fflush(FILE *stream);
int    putchar(int c);
int    fseek(FILE *stream, long offset, int whence);
long   ftell(FILE *stream);
int    sscanf(const char *str, const char *fmt, ...);
int getchar(void);
int fgetc(FILE *stream);
int fputc(int c, FILE *stream);
char *fgets(char *s, int n, FILE *stream);
FILE *fopen(const char *path, const char *mode);
int   fclose(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int    fscanf(FILE *stream, const char *fmt, ...);
void   rewind(FILE *stream);
#endif /* FREESTANDING_STDIO_H */
