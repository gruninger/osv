#include <wchar.h>
#include <wctype.h>
#undef iswxdigit

int iswxdigit(wint_t wc)
{
	return (unsigned)(wc-'0') < 10 || (unsigned)((wc|32)-'a') < 6;
}