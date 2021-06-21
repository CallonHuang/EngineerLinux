extern void backtrace_reg();

int main()
{
    int *p = 0;
    backtrace_reg();
    *p = 1;
    return 0;
}