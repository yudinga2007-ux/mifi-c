//  gcc -c main33.c -o main33.o
//  gcc -c check33.c -o check33.o
//  gcc main33.o check33.o -o code_check
//  ./code_check
#include <string.h>

int check_code(const char *code)
{
    if (strlen(code) != 3) {
        return 0;
    }

    return strcmp(code, "123") == 0;
}