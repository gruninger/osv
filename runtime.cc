#include <cstdlib>
#include <cstring>
#include <string.h>
#include <exception>
#include <libintl.h>
#include <cxxabi.h>
#include <sys/mman.h>
#include <unistd.h>
#include <libunwind.h>
#include <link.h>
#include <stdio.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/uio.h>
#include <wchar.h>
#include <locale.h>
#include <libintl.h>
#include <ctype.h>
#include <wctype.h>
#include <langinfo.h>
#include <stdarg.h>
#include <xlocale.h>
#include <cassert>
#include "arch/x64/processor.hh"
#include "debug.hh"
#include <boost/format.hpp>

#define __LC_LAST 13

typedef unsigned char __guard;

#define __alias(x) __attribute__((alias(x)))

extern "C" {
    void __cxa_pure_virtual(void);
    void abort(void);
    void _Unwind_Resume(void);
    void *malloc(size_t size);
    void free(void *);
    int tdep_get_elf_image(struct elf_image *ei, pid_t pid, unw_word_t ip,
                           unsigned long *segbase, unsigned long *mapoff,
                           char *path, size_t pathlen);
    int _Uelf64_get_proc_name(unw_addr_space_t as, int pid, unw_word_t ip,
                              char *buf, size_t buf_len, unw_word_t *offp);
    int __fxstat(int ver, int fd, struct stat *buf);
    int __fxstat64(int ver, int fd, struct stat64 *buf);
    void __stack_chk_fail(void);
    void __assert_fail(const char * assertion, const char * file, unsigned int line, const char * function);
    void __freelocale(__locale_t __dataset) __THROW;
    __locale_t __duplocale(__locale_t __dataset) __THROW;
    __locale_t __newlocale(int __category_mask, __const char *__locale,
			   __locale_t __base) __THROW;
    __locale_t __uselocale(__locale_t __dataset) __THROW;
    char *__setlocale(int category, const char *locale);
    char *setlocale(int category, const char *locale) __alias("__setlocale");
    double __strtod_l(__const char *__restrict __nptr,
		      char **__restrict __endptr, __locale_t __loc)
	__THROW __nonnull ((1, 3));
    int __iswctype_l(wint_t __wc, wctype_t __desc, __locale_t __locale)
	__THROW;
    wctype_t __wctype_l(__const char *__property, __locale_t __locale)
	__THROW;
    float __strtof_l(__const char *__restrict __nptr,
		     char **__restrict __endptr, __locale_t __loc)
	__THROW __nonnull((1, 3)) __wur;
    char *__nl_langinfo_l(nl_item __item, __locale_t __l);
    wint_t __towlower_l(wint_t __wc, __locale_t __locale) __THROW;
    wint_t __towupper_l(wint_t __wc, __locale_t __locale) __THROW;
    int __wcscoll_l(__const wchar_t *__s1, __const wchar_t *__s2,
		    __locale_t __loc) __THROW;
    int __strcoll_l(__const char *__s1, __const char *__s2, __locale_t __l)
	__THROW __attribute_pure__ __nonnull((1, 2, 3));
    size_t __wcsftime_l(wchar_t *__restrict __s, size_t __maxsize,
			__const wchar_t *__restrict __format,
			__const struct tm *__restrict __tp,
			__locale_t __loc) __THROW;
    size_t __strftime_l(char *__restrict __s, size_t __maxsize,
			__const char *__restrict __format,
			__const struct tm *__restrict __tp,
			__locale_t __loc) __THROW;
    size_t __strxfrm_l(char *__dest, __const char *__src, size_t __n,
		       __locale_t __l) __THROW __nonnull ((2, 4));
    size_t __wcsxfrm_l(wchar_t *__s1, __const wchar_t *__s2,
			 size_t __n, __locale_t __loc) __THROW;

}

void *__dso_handle;

void ignore_debug_write(const char *msg)
{
}

void (*debug_write)(const char *msg) = ignore_debug_write; //replace w/ 'debug'

#define WARN(msg) do {					\
        static bool _x;					\
	if (!_x) {					\
	    _x = true;					\
	    debug("WARNING: unimplemented " msg);	\
	}						\
    } while (0)

#define UNIMPLEMENTED(msg) do {				\
	WARN(msg);					\
	abort();					\
    } while (0)


#define UNIMPL(decl) decl { UNIMPLEMENTED(#decl); }
#define IGN(decl) decl { WARN(#decl " (continuing)"); }

void abort()
{
    while (true)
	processor::x86::halt_no_interrupts();
}

void operator delete(void *)
{
    WARN("operator delete");
}

void __cxa_pure_virtual(void)
{
    abort();
}

void *memcpy(void *dest, const void *src, size_t n)
{
    char* p = reinterpret_cast<char*>(dest);
    const char* q = reinterpret_cast<const char*>(src);

    while (n--) {
	*p++ = *q++;
    }
    return dest;
}

void *memmove(void *dest, const void *src, size_t n)
{
    char* p = reinterpret_cast<char*>(dest);
    const char* q = reinterpret_cast<const char*>(src);

    if (p < q) {
	while (n--) {
	    *p++ = *q++;
	}
    } else {
	p += n;
	q += n;
	while (n--) {
	    *--p = *--q;
	}
    }
    return dest;
}

void *memset(void *s, int c, size_t n)
{
    char* p = reinterpret_cast<char*>(s);

    while (n--) {
	*p++ = c;
    }
    return s;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
    const unsigned char* p1 = reinterpret_cast<const unsigned char*>(s1);
    const unsigned char* p2 = reinterpret_cast<const unsigned char*>(s2);

    while (n) {
	if (*p1 != *p2) {
	    return int(*p1) - int(*p2);
	}
	++p1;
	++p2;
	--n;
    }

    return 0;
}

const void* memchr(const void *s, int c, size_t n)
{
    const unsigned char* p = reinterpret_cast<const unsigned char*>(s);

    while (n--) {
	if (*p == c) {
	    return p;
	}
	++p;
    }

    return NULL;
}

size_t strlen(const char *s)
{
    size_t ret = 0;
    while (*s++) {
	++ret;
    }
    return ret;
}

char* strncpy(char* dest, const char* src, size_t n)
{
    char* p = dest;

    while (n--) {
        *p = *src;
        if (!*src) {
            break;
        }
        ++p;
        ++src;
    }
    return dest;
}

char* gettext (const char* msgid)
{
    return const_cast<char*>(msgid);
}

char* strerror(int err)
{
    return const_cast<char*>("strerror");
}

namespace __cxxabiv1 {
    std::terminate_handler __terminate_handler = abort;

    int __cxa_guard_acquire(__guard*)
    {
	return 0;
    }

    void __cxa_guard_release(__guard*) _GLIBCXX_NOTHROW
    {
    }

    void __cxa_guard_abort(__guard*) _GLIBCXX_NOTHROW
    {
	abort();
    }

    int __cxa_atexit(void (*destructor)(void *), void *arg, void *dso)
    {
	return 0;
    }

    int __cxa_finalize(void *f)
    {
	return 0;
    }

}


void *mmap(void *addr, size_t length, int prot, int flags,
	   int fd, off_t offset)
{
    if (fd != -1) {
	abort();
    }

    return malloc(length);
}

int munmap(void *addr, size_t length)
{
    return 0;
}

int getpagesize()
{
    return 4096;
}

int
tdep_get_elf_image (struct elf_image *ei, pid_t pid, unw_word_t ip,
		    unsigned long *segbase, unsigned long *mapoff,
		    char *path, size_t pathlen)
{
    return 0;
}

int getpid()
{
    return 0;
}

int mincore(void *addr, size_t length, unsigned char *vec)
{
    memset(vec, 0x01, (length + getpagesize() - 1) / getpagesize());
    return 0;
}

int dl_iterate_phdr(int (*callback)(struct dl_phdr_info *info,
                                    size_t size, void *data),
                    void *data)
{
    return 0;
}

int _Uelf64_get_proc_name(unw_addr_space_t as, int pid, unw_word_t ip,
                          char *buf, size_t buf_len, unw_word_t *offp)
{
    return 0;
}

FILE* stdin;
FILE* stdout;
FILE* stderr;

//    WCTDEF(alnum), WCTDEF(alpha), WCTDEF(blank), WCTDEF(cntrl),
//    WCTDEF(digit), WCTDEF(graph), WCTDEF(lower), WCTDEF(print),
//    WCTDEF(punct), WCTDEF(space), WCTDEF(upper), WCTDEF(xdigit),

static unsigned short c_locale_array[384] = {
#include "ctype-data.h"
};

static struct __locale_struct c_locale = {
    { }, // __locales_data
    c_locale_array + 128, // __ctype_b
};

const wchar_t* wmemchr(const wchar_t *s, wchar_t c, size_t n)
{
    UNIMPLEMENTED("wmemchr");
}

int ioctl(int fd, unsigned long request, ...)
{
    UNIMPLEMENTED("ioctl");
}

off64_t lseek64(int fd, off64_t offset, int whence)
{
    UNIMPLEMENTED("lseek64");
}

int poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
    UNIMPLEMENTED("poll");
}

int __fxstat(int ver, int fd, struct stat *buf)
{
    UNIMPLEMENTED("__fxstat");
}

int __fxstat64(int ver, int fd, struct stat64 *buf)
{
    UNIMPLEMENTED("__fxstat");
}

int *__errno_location()
{
    UNIMPLEMENTED("__errno_location");
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt)
{
    UNIMPLEMENTED("readv");
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt)
{
    UNIMPLEMENTED("writev");
}

ssize_t preadv(int fd, const struct iovec *iov, int iovcnt, off_t offset)
{
    UNIMPLEMENTED("preadv");
}

ssize_t pwritev(int fd, const struct iovec *iov, int iovcnt, off_t offset)
{
    UNIMPLEMENTED("pwritev");
}

ssize_t read(int fd, void *buf, size_t count)
{
    UNIMPLEMENTED("read");
}

int fclose(FILE *fp)
{
    UNIMPLEMENTED("fclose");
}

int fileno(FILE *fp)
{
    UNIMPLEMENTED("fileno");
}

FILE *fdopen(int fd, const char *mode)
{
    UNIMPLEMENTED("fdopen");
}

int fflush(FILE *fp)
{
    UNIMPLEMENTED("fflush");
}

ssize_t write(int fd, const void *buf, size_t count)
{
    UNIMPLEMENTED("write");
}

int fgetc(FILE *stream)
{
    UNIMPLEMENTED("fgetc");
}

char *fgets(char *s, int size, FILE *stream)
{
    UNIMPLEMENTED("fgets");
}

#undef getc
int getc(FILE *stream)
{
    UNIMPLEMENTED("getc");
}

int getchar(void)
{
    UNIMPLEMENTED("getchar");
}

char *gets(char *s)
{
    UNIMPLEMENTED("gets");
}

int ungetc(int c, FILE *stream)
{
    UNIMPLEMENTED("ungetc");
}

UNIMPL(int fputc(int c, FILE *stream))
UNIMPL(int fputs(const char *s, FILE *stream))
#undef putc
UNIMPL(int putc(int c, FILE *stream))
UNIMPL(int putchar(int c))
UNIMPL(int puts(const char *s))
UNIMPL(size_t wcslen(const wchar_t *s))
UNIMPL(int wmemcmp(const wchar_t *s1, const wchar_t *s2, size_t n))
UNIMPL(wchar_t *wmemcpy(wchar_t *dest, const wchar_t *src, size_t n))
UNIMPL(int setvbuf(FILE *stream, char *buf, int mode, size_t size))
UNIMPL(size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream))
UNIMPL(size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream))
UNIMPL(wint_t fgetwc(FILE *stream))
UNIMPL(wint_t getwc(FILE *stream))
UNIMPL(wint_t ungetwc(wint_t wc, FILE *stream))

UNIMPL(int fseeko64(FILE *stream, off64_t offset, int whence))
UNIMPL(wchar_t *wmemmove(wchar_t *dest, const wchar_t *src, size_t n))
UNIMPL(size_t mbrtowc(wchar_t *pwc, const char *s, size_t n, mbstate_t *ps))
UNIMPL(wchar_t *wmemset(wchar_t *wcs, wchar_t wc, size_t n))
UNIMPL(off64_t ftell(FILE *stream))
UNIMPL(FILE *fopen64(const char *path, const char *mode))
UNIMPL(off64_t ftello64(FILE *stream))
UNIMPL(wint_t fputwc(wchar_t wc, FILE *stream))
UNIMPL(wint_t putwc(wchar_t wc, FILE *stream))
int wctob(wint_t c)
{
    WARN("trivial wctob");
    return c <= 0x7f ? c : EOF;
}

UNIMPL(size_t wcsnrtombs(char *dest, const wchar_t **src, size_t nwc,
                         size_t len, mbstate_t *ps))
wint_t btowc(int c)
{
    WARN("trivial btowc");
    return c;
}

UNIMPL(size_t wcrtomb(char *s, wchar_t wc, mbstate_t *ps))
UNIMPL(void __stack_chk_fail(void))
UNIMPL(void __assert_fail(const char * assertion, const char * file, unsigned int line, const char * function))
UNIMPL(void __freelocale(__locale_t __dataset) __THROW)
UNIMPL(__locale_t __duplocale(__locale_t __dataset) __THROW)

namespace {
    bool all_categories(int category_mask)
    {
	return (category_mask | (1 << LC_ALL)) == (1 << __LC_LAST) - 1;
    }
}

struct __locale_data
{
  const char *name;
  const char *filedata;		/* Region mapping the file data.  */
  off_t filesize;		/* Size of the file (and the region).  */
  enum				/* Flavor of storage used for those.  */
  {
    ld_malloced,		/* Both are malloc'd.  */
    ld_mapped,			/* name is malloc'd, filedata mmap'd */
    ld_archive			/* Both point into mmap'd archive regions.  */
  } alloc;

  /* This provides a slot for category-specific code to cache data computed
     about this locale.  That code can set a cleanup function to deallocate
     the data.  */
  struct
  {
    void (*cleanup) (struct __locale_data *);
    union
    {
      void *data;
      struct lc_time_data *time;
      const struct gconv_fcts *ctype;
    };
  } __private;

  unsigned int usage_count;	/* Counter for users.  */

  int use_translit;		/* Nonzero if the mb*towv*() and wc*tomb()
				   functions should use transliteration.  */

  unsigned int nstrings;	/* Number of strings below.  */
  union locale_data_value
  {
    const uint32_t *wstr;
    const char *string;
    unsigned int word;		/* Note endian issues vs 64-bit pointers.  */
  }
  values __flexarr;	/* Items, usually pointers into `filedata'.  */
};

__locale_t __newlocale(int category_mask, const char *locale, locale_t base)
    __THROW
{
    if (category_mask == 1 << LC_ALL) {
	category_mask = ((1 << __LC_LAST) - 1) & ~(1 << LC_ALL);
    }
    assert(locale);
    if (base == &c_locale) {
	base = NULL;
    }
    if ((base == NULL || all_categories(category_mask))
	&& (category_mask == 0 || strcmp(locale, "C") == 0)) {
	return &c_locale;
    }
    struct __locale_struct result = base ? *base : c_locale;
    if (category_mask == 0) {
	auto result_ptr = new __locale_struct;
	*result_ptr = result;
	auto ctypes = result_ptr->__locales[LC_CTYPE]->values;
	result_ptr->__ctype_b = (const unsigned short *)
	    ctypes[_NL_ITEM_INDEX(_NL_CTYPE_CLASS)].string + 128;
	result_ptr->__ctype_tolower = (const int *)
	    ctypes[_NL_ITEM_INDEX(_NL_CTYPE_TOLOWER)].string + 128;
	result_ptr->__ctype_toupper = (const int *)
	    ctypes[_NL_ITEM_INDEX(_NL_CTYPE_TOUPPER)].string + 128;
	return result_ptr;
    }
    abort();
}

UNIMPL(long double strtold_l(__const char *__restrict __nptr,
			     char **__restrict __endptr, __locale_t __loc))
UNIMPL(double __strtod_l(__const char *__restrict __nptr,
			 char **__restrict __endptr, __locale_t __loc)
			 __THROW)
UNIMPL(size_t mbsnrtowcs(wchar_t *dest, const char **src,
                         size_t nms, size_t len, mbstate_t *ps))
UNIMPL(size_t mbsrtowcs(wchar_t *dest, const char **src,
			size_t len, mbstate_t *ps))
__locale_t __uselocale (__locale_t __dataset) __THROW
{
    WARN("__uselocale unimplemented");
    return (__locale_t)__dataset;
}

UNIMPL(char *__setlocale(int category, const char *locale))
UNIMPL(char* textdomain (const char* domainname))
UNIMPL(char* bindtextdomain (const char * domainname, const char * dirname))
UNIMPL(int __iswctype_l(wint_t __wc, wctype_t __desc, __locale_t __locale)
       __THROW)
UNIMPL(float __strtof_l(__const char *__restrict __nptr,
			char **__restrict __endptr, __locale_t __loc)
       __THROW)
UNIMPL(char *__nl_langinfo_l(nl_item __item, __locale_t __l))

#define WCTDEF(x) { _IS##x, #x }

static const struct { wctype_t mask; const char *name; } wct_names[] = {
    WCTDEF(alnum), WCTDEF(alpha), WCTDEF(blank), WCTDEF(cntrl),
    WCTDEF(digit), WCTDEF(graph), WCTDEF(lower), WCTDEF(print),
    WCTDEF(punct), WCTDEF(space), WCTDEF(upper), WCTDEF(xdigit),
    {}
};

wctype_t __wctype_l(__const char *__property, __locale_t __locale) __THROW
{
    WARN("trivial __wctype_l");
    debug_write(__property);
    for (auto n = wct_names; n->mask; ++n) {
	if (strcmp(__property, n->name) == 0) {
	    return n->mask;
	}
    }
    return 0;
}

UNIMPL(size_t __ctype_get_mb_cur_max (void) __THROW)
UNIMPL(wint_t __towlower_l(wint_t __wc, __locale_t __locale) __THROW)
UNIMPL(int __wcscoll_l(__const wchar_t *__s1, __const wchar_t *__s2,
		       __locale_t __loc) __THROW)
UNIMPL(wint_t __towupper_l(wint_t __wc, __locale_t __locale) __THROW)

UNIMPL(int __strcoll_l(__const char *__s1, __const char *__s2, __locale_t __l)
       __THROW)
UNIMPL(size_t __wcsftime_l (wchar_t *__restrict __s, size_t __maxsize,
			    __const wchar_t *__restrict __format,
			    __const struct tm *__restrict __tp,
			    __locale_t __loc) __THROW)
UNIMPL(size_t __strftime_l (char *__restrict __s, size_t __maxsize,
			    __const char *__restrict __format,
			    __const struct tm *__restrict __tp,
			    __locale_t __loc) __THROW)
UNIMPL(size_t __strxfrm_l (char *__dest, __const char *__src, size_t __n,
			   __locale_t __l) __THROW)
UNIMPL(size_t __wcsxfrm_l(wchar_t *__s1, __const wchar_t *__s2,
			    size_t __n, __locale_t __loc) __THROW)
UNIMPL(int wcscmp(const wchar_t *s1, const wchar_t *s2))

long sysconf(int name)
{
    switch (name) {
    case _SC_CLK_TCK: return CLOCKS_PER_SEC;
    case _SC_PAGESIZE: return 4096; // FIXME
    case _SC_THREAD_PROCESS_SHARED: return true;
    case _SC_NPROCESSORS_ONLN: return 1; // FIXME
    case _SC_NPROCESSORS_CONF: return 1; // FIXME
    }
    debug(fmt("sysconf: unknown parameter %1%") % name);
    abort();
}

long timezone;

char* __environ_array[1];
char** environ = __environ_array;

#undef errno
int __thread errno;
