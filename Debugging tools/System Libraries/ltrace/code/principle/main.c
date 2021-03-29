// gcc -m32 -no-pie -g -o plt main.c
// use "gcc -m64 -no-pie -g -o plt main.c", if failed
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    puts("Hello world!");
    puts("Say hello again!");
    exit(0);
}