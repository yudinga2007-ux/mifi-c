//  gcc -c main33.c -o main33.o
//  gcc -c check33.c -o check33.o
//  gcc main33.o check33.o -o code_check
//  ./code_check
#include <stdio.h>

int check_code(const char *code);

void journal(const char *message)
{
    FILE *f = fopen("journal.txt", "a");

    if (f != NULL) {
        fprintf(f, "%s\n", message);
        fclose(f);
    }
}

int main(void)
{
    char code[8];
    int i;

    for (i = 0; i < 3; i++) {
        printf("Enter code: ");

        if (scanf("%7s", code) != 1) {
            code[0] = '\0';
        }

        if (check_code(code)) {
            puts("GRANTED");
            journal("OK: successful login");
            return 0;
        }

        puts("Warning: wrong code");
        journal("FAIL: wrong code attempt");
    }

    puts("DENIED");
    journal("ERROR: login denied");

    return 1;
}