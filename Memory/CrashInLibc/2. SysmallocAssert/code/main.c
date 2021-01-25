#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

struct malloc_chunk {

  size_t      mchunk_prev_size;  /* Size of previous chunk (if free).  */
  size_t      mchunk_size;       /* Size in bytes, including overhead. */

  struct malloc_chunk* fd;         /* double links -- used only if free. */
  struct malloc_chunk* bk;

  /* Only used for large blocks: pointer to next larger size.  */
  struct malloc_chunk* fd_nextsize; /* double links -- used only if free. */
  struct malloc_chunk* bk_nextsize;
};

int main()
{
    int *ttmp, i;
    struct malloc_chunk *next;
#ifndef SYSMALLOC_ASSERT
    printf("sbrk(0) = %p\n", sbrk(0));
#endif
    char *tmp = (char *)malloc(1024);
#ifdef SYSMALLOC_ASSERT
    next = (struct malloc_chunk*)((char *)tmp + 1024);
    next->mchunk_size = 0;
#else
    printf("sbrk(0) = %p, tmp = %p\n", sbrk(0), tmp);
#endif
    ttmp = (int *)malloc(4);
#ifndef SYSMALLOC_ASSERT
    printf("sbrk(0) = %p, tmp = %p, ttmp = %p\n", sbrk(0), tmp, ttmp);
#endif
	return 0;
}
