#include <dlfcn.h>

#include <unwind.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <link.h>	/* required for __ELF_NATIVE_CLASS */
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <ucontext.h>
#include <sys/prctl.h>
#if __ELF_NATIVE_CLASS == 32
#define WORD_WIDTH 8
#else/* We assyme 64bits.  */
#define WORD_WIDTH 16
#endif
# define unwind_backtrace _Unwind_Backtrace
# define unwind_getip _Unwind_GetIP



struct trace_arg
{
  void **array;
  int cnt, size;
};


static _Unwind_Reason_Code
backtrace_helper (struct _Unwind_Context *ctx, void *a)
{
  struct trace_arg *arg = a;

  /* We are first called with address in the __backtrace function.
     Skip it.  */
  if (arg->cnt != -1)
    arg->array[arg->cnt] = (void *) unwind_getip (ctx);
  if (++arg->cnt == arg->size)
    return _URC_END_OF_STACK;
  return _URC_NO_REASON;
}

static int
backtrace (array, size)
     void **array;
     int size;
{
  struct trace_arg arg = { .array = array, .size = size, .cnt = -1 };
#ifdef SHARED
  __libc_once_define (static, once);

  __libc_once (once, init);
  if (unwind_backtrace == NULL)
    return 0;
#endif

  if (size >= 1)
    unwind_backtrace (backtrace_helper, &arg);

  if (arg.cnt > 1 && arg.array[arg.cnt - 1] == NULL)
    --arg.cnt;
  return arg.cnt != -1 ? arg.cnt : 0;
}

static void 
backtrace_symbols (array, size, result, max_len)
     void *const *array;
     int size;
     char **result;
     int max_len;
{
	Dl_info info[size];
	int status[size];
	int cnt;
	size_t total = 0;

	/* Fill in the information we can get from `dladdr'.  */
	for (cnt = 0; cnt < size; ++cnt) {
		status[cnt] = dladdr (array[cnt], &info[cnt]);
		if (status[cnt] && info[cnt].dli_fname &&
			info[cnt].dli_fname[0] != '\0')
		/*
		 * We have some info, compute the length of the string which will be
		 * "<file-name>(<sym-name>) [+offset].
		 */
		total += (strlen (info[cnt].dli_fname ?: "") +
				  (info[cnt].dli_sname ?
				  strlen (info[cnt].dli_sname) + 3 + WORD_WIDTH + 3 : 1)
				  + WORD_WIDTH + 5);
		else
			total += 5 + WORD_WIDTH;
	}

	/* Allocate memory for the result.  */
	if (result != NULL) {
		char *last = (char *) (result + size);
		for (cnt = 0; cnt < size; ++cnt) {
			result[cnt] = last;

			if (status[cnt] && info[cnt].dli_fname
				&& info[cnt].dli_fname[0] != '\0') {

				char buf[20];

				if (array[cnt] >= (void *) info[cnt].dli_saddr)
					sprintf (buf, "+%#lx",
							(unsigned long)(array[cnt] - info[cnt].dli_saddr));
				else
					sprintf (buf, "-%#lx",
					(unsigned long)(info[cnt].dli_saddr - array[cnt]));

				last += 1 + sprintf (last, "%s%s%s%s%s[%p]",
				info[cnt].dli_fname ?: "",
				info[cnt].dli_sname ? "(" : "",
				info[cnt].dli_sname ?: "",
				info[cnt].dli_sname ? buf : "",
				info[cnt].dli_sname ? ") " : " ",
				array[cnt]);
			} else
				last += 1 + sprintf (last, "[%p]", array[cnt]);
		}
		assert (last <= (char *) result + max_len);
	}
	return;
}
#define MAX_SYS_TRACE_LAYER 15
#define MAX_SYS_TRACE_PRINT 1536

void 
watch_backtrace(void)
{
    void* array[MAX_SYS_TRACE_LAYER] = {0};
    char *name[MAX_SYS_TRACE_PRINT] = {0};
    int i = 0, n = 0;
    n = backtrace(array, MAX_SYS_TRACE_LAYER);
    backtrace_symbols(array, n, name, MAX_SYS_TRACE_PRINT);
    for (i = 0; i < n; i++)
    {
        printf("#%02d %s\n", i, name[i]);
    }
    return;
}

static void handler_func(int signalnum, siginfo_t *info, void* rserved)
{
    char pthread_name[120]={0};
    prctl(PR_GET_NAME, pthread_name);
    printf("============Exception Signal %d\tPid:%d\tName:%s=======================\n", signalnum, getpid(), pthread_name);
    printf("========================Start Backtrace======================\n");
    watch_backtrace();
    printf("========================End Backtrace======================\n");
    fflush(stdout);
    signal(signalnum, SIG_DFL);
    raise(signalnum);
}

void backtrace_reg()
{
    struct sigaction handler;
    memset(&handler,0,sizeof(handler));
    handler.sa_sigaction = handler_func;
    handler.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV,&handler,NULL);
    sigaction(SIGABRT,&handler,NULL);
}
