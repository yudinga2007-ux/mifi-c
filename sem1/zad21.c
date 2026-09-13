#include <stdio.h>

int main(void)
{
    int a, b, c, col;

    if (scanf("%x %x", &a, &b) != 2) {
        fputs("Ошибка ввода\n", stderr);
        return 1;
    }

    printf("..");
    for (col = 0; col < 16; col++)
        printf(" %X", col);
    printf("\n");
    for (col = 0; col < 34; col++)
        putchar('-');
    printf("\n");

    c = a & 0xF0;
    while (c <= b) {
        printf("%Xx", c >> 4);
        for (col = 0; col < 16; col++) {
            if (c + col >= a && c + col <= b)
                printf(" %c", c + col);
            else
                printf("  ");
        }
        printf("\n");
        c += 16;
    }

    for (col = 0; col < 34; col++)
        putchar('-');
    printf("\n");

    return 0;
}