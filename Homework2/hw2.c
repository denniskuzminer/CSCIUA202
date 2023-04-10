// #include <stdio.h>
// size_t f(const char *c)
// {
//     return sizeof(c);
// }
// int main()
// {
//     const char a[] = "Hello, CS202!";
//     const char *b = "Hello, CS202!";
//     printf("size of a is %lu\n", sizeof(a));
//     printf("size of b is %lu\n", sizeof(b));
//     printf("size of b is %lu\n", sizeof("b"));
//     printf("f(a) returns %lu\n", f(a));
//     printf("f(b) returns %lu\n", f(b));
// }

#include <stdio.h>
#include <unistd.h>

int main()
{
    printf("one");
    execl("/bin/echo", "/bin/echo", "two", NULL);
    printf("three");
}

int main()
{
    printf("Before system\n");
    if (fork() == 0)
    {
        execl("/bin/sh", "/bin/sh", "-c", "echo", NULL);
        exit(-1);
    }
    wait(NULL);
    printf("After system\n");
}

