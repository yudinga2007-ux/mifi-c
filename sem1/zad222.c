//  gcc zad222.c -o zad222
//  ./zad222
//  ./zad221 | ./zad222
#include <stdio.h>
#include <time.h>

int main(void)
{
    int y, m, d;

    if (scanf("%d-%d-%d", &y, &m, &d) != 3) {
        fputs("Ошибка: ожидается дата ГГГГ-ММ-ДД\n", stderr);
        return 1;
    }

    struct tm t = {0};
    t.tm_year  = y - 1900;
    t.tm_mon   = m - 1;
    t.tm_mday  = d + 1;
    t.tm_hour  = 12;
    t.tm_isdst = -1;

    if (mktime(&t) == (time_t)-1) {
        fputs("Ошибка: некорректная дата\n", stderr);
        return 1;
    }

    char buff[32];
    strftime(buff, sizeof buff, "%Y-%m-%d", &t);
    puts(buff);

    return 0;
}