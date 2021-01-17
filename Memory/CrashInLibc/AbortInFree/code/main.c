#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

int main()
{
    void *temp = NULL;
    temp = malloc(128);
#if defined(INVALID_POINTER)/*free(): invalid pointer*/
    *((size_t*)(temp-sizeof(size_t))) = 0;
#elif defined(INVALID_SIZE)/*free(): invalid size*/
    *((size_t*)(temp-sizeof(size_t))) = 0xffff<<3;
#elif defined(MUNMAP_INVALID_POINTER)/*munmap_chunk(): invalid pointer*/
    *((size_t*)(temp-sizeof(size_t))) = 0x2;
#elif defined(DOUBLE_FREE)/*double free or corruption*/
    *((size_t*)(temp-sizeof(size_t))) = 144<<3|0x0;
#endif
    free(temp);
    return 0;
}
