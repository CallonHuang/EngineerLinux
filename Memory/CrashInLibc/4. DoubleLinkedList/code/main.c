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
    void *tmp[4]; 
    struct malloc_chunk *test;
    int i;
    for (i = 0; i < 4; i++) {
        tmp[i] = malloc(1040);
        memset(tmp[i], 0, 1040);
    }
    free(tmp[1]);
#ifdef DOUBLE_LINKED_IN_FREE
    test = (struct malloc_chunk *)((char *)tmp[1] - 2*sizeof(size_t));
    test->bk = tmp[3]; //test->fd = tmp[3]; //also get double linked
#endif
    free(tmp[2]);
	return 0;
}
