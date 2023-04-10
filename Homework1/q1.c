#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

int *add(int a, int b, int *sum)
{
    *sum = a + b;
    return sum;
}
int main()
{
    int *result = (int *)malloc(sizeof(int));
    result = add(1, 2, result);
    printf("result = %d\n", *result);
    printf("result = %d\n", *result);
}

// int main()
// {
//     int *result = add(1, 2);
//     printf("result = %d\n", *result);
//     printf("result = %d\n", *result);
// }

int *add1(int a, int b)
{
    int c = a + b;
    int *d = &c;
    // printf("%d\n", a);
    // printf("%d\n", b);
    // printf("%d\n", c);
    // printf("%d\n", d);
    // printf("%d\n", *d);
    // printf("%d\n", &a);
    // printf("%d\n", &b);
    // printf("%d\n", &c);
    // printf("%d\n", &d);
    return d;
}