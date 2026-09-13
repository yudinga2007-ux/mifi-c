//  gcc zad221.c -o zad221
//  ./zad221
//  ./zad221 | ./zad222
#include <stdio.h>
#include <time.h>

int main(void)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    if (t == NULL) {
        fputs("Ошибка: localtime\n", stderr);
        return 1;
    }

    char buff[32];
    strftime(buff, sizeof buff, "%Y-%m-%d", t);
    puts(buff);

    return 0;
}