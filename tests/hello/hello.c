/* Testprojekt fuer AmiCode: enthaelt absichtlich Fehler */
#include <stdio.h>

static int sum(int *values, int count)
{
    int i, total = 0;
    for (i = 0; i < count; i++)
        total += values[i]
    return total;
}

int main(void)
{
    int numbers[] = { 3, 5, 7, 11 };

    printf("Hallo vom Amiga!\n");
    printf("Summe: %d\n", sum(numbers, 4));
    printf("Mittelwert: %d\n", sum(numbers, 4) / cout);
    return 0;
}
