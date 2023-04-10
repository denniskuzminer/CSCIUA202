#include <pthread.h>
#include <stdio.h>
void *run(void *arg)
{
    int index = (int *)arg;
    printf("My index is %d\n", index);
    pthread_exit(NULL);
}
int main()
{
    pthread_t tid[5];
    for (int i = 0; i < 5; ++i)
    {
        pthread_create(&tid[i], NULL, run, (void *)i);
    }
    for (int i = 0; i < 5; ++i)
    {
        pthread_join(tid[i], NULL);
    }
}
// #include <stdio.h>
// #include <string.h>
// #include <stdlib.h>
// #include <pthread.h>
// #include <math.h>
// #include <unistd.h>
// #include <sys/mman.h>
// #include <fcntl.h>
// #include <sys/types.h>
// #include <sys/stat.h>
// void *PrintHello(void *threadid)
// {
//     long tid;
//     tid = (long)threadid;
//     printf("Hello World! It's me, thread #%ld!\n", tid);
//     pthread_exit(NULL);
// }
// int main()
// {
//     pthread_t threads[5];
//     int rc;
//     long t;
//     for (t = 0; t < 5; t++)
//     {
//         rc = pthread_create(&threads[t], NULL, PrintHello, (void *)t);
//         if (rc)
//         {
//             printf("ERROR; return code from pthread_create() is %d\n", rc);
//             exit(-1);
//         }
//     }
//     pthread_exit(NULL);
// }
