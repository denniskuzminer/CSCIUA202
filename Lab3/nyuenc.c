// Borrowed ideas from https://www.youtube.com/watch?v=_n2hE2gyPxU
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAX_RLEN 256
#define MAX_LEN 4096
#define MAX_CHUNKS 500000
int THREAD_NUM = 1;

typedef struct Task
{
    char *(*taskFunction)(int, int, int, char *);
    int start, len;
    int order;
    char *fileContents;
    int last;
} Task;

Task taskQueue[MAX_CHUNKS];
char results[MAX_CHUNKS][MAX_RLEN];
int resultLen[MAX_CHUNKS];
int taskCount = 0;
char *fileContents;
long fileLength;

pthread_mutex_t mutexQueue;
pthread_cond_t condQueue;

char *encode(int start, int len, int order, char *fileContents)
{

    unsigned char rLen;
    unsigned char count[MAX_RLEN];
    int i, j = 0, k, l = 0;
    for (i = start; i < start + len; i++)
    {
        for (results[order][j++] = fileContents[i], rLen = 1; i + 1 < start + len && fileContents[i] == fileContents[i + 1]; rLen++,
            i++)
        {
        }
        results[order][j++] = rLen;
    }
    results[order][j] = '\0';
    resultLen[order] = j;
    return results[order];
}

char *encodeSubstring(int start, int len, int order, char *fileContents)
{
    return encode(start, len, order, fileContents);
}
int last = -1;
int order = 0;
int queueStart = 0;

void executeTask(Task *task)
{
    task->taskFunction(task->start, task->len, task->order, task->fileContents);
    if (task->last != -1)
    {
        last = task->last;
    }
}

void submitTask(Task task)
{
    pthread_mutex_lock(&mutexQueue);
    taskQueue[queueStart + taskCount++] = task;
    pthread_cond_signal(&condQueue);
    pthread_mutex_unlock(&mutexQueue);
}
pthread_t th[1000];

void *startThread(void *args)
{
    while (1) 
    {
        Task task;
        pthread_mutex_lock(&mutexQueue);
        while (taskCount == 0)
        {
            while (last != -1)
            {
                pthread_cond_broadcast(&condQueue);
                pthread_mutex_unlock(&mutexQueue);
                pthread_exit(NULL);
            }
            pthread_cond_wait(&condQueue, &mutexQueue);
        }
        task = taskQueue[queueStart];
        queueStart++;
        taskCount--;
        pthread_mutex_unlock(&mutexQueue);
        executeTask(&task);
    }
}
char *pointerArr[MAX_CHUNKS];
int pointerIdx = 0;

int main(int argc, char **argv)
{
    int jobSpec = 0;
    int i = 0;
    int j = 1;
    if (0 == strcmp(argv[j], "-j"))
    {
        THREAD_NUM = atoi(argv[++j]);
        jobSpec = 1;
        j++;
    }
    pthread_mutex_init(&mutexQueue, NULL);
    pthread_cond_init(&condQueue, NULL);
    for (i = 0; i < THREAD_NUM; i++)
    {
        if (pthread_create(&th[i], NULL, &startThread, NULL) != 0)
        {
            perror("There was an error creating the thread");
        }
    }

    for (; j < argc; j++)
    {
        int fd = open(argv[j], O_RDONLY);
        if (fd == -1)
        {
            perror("open");
            exit(1);
        }

        struct stat sb;
        if (fstat(fd, &sb) == -1)
        {
            perror("fstat");
            exit(1);
        }

        fileLength = sb.st_size;

        fileContents = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        pointerArr[pointerIdx++] = fileContents;
        if (fileContents == MAP_FAILED)
        {
            perror("mmap");
            exit(1);
        }

        srand(time(NULL));
        for (i = 0; (i * MAX_LEN) < fileLength; i++, order++)
        {
            int start = (i * MAX_LEN);
            int len = ((start + MAX_LEN) >= fileLength) ? (fileLength - start) : MAX_LEN;
            int last = (((i + 1) * MAX_LEN) >= fileLength && j == argc - 1) ? order : -1;
            Task t = {
                .taskFunction = &encodeSubstring,
                .start = start,
                .len = len,
                .order = order,
                .fileContents = fileContents,
                .last = last,
            };
            submitTask(t);
        }
        close(fd);
    }
    for (i = 0; i < THREAD_NUM; i++)
    {
        if (pthread_join(th[i], NULL) != 0)
        {
            perror("There was an error joining the thread");
        }
    }

    for (i = 0; i <= last; i++)
    {
        if (i + 1 <= last && results[i][resultLen[i] - 2] == results[i + 1][0])
        {
            char lastCount = results[i][resultLen[i] - 1];
            resultLen[i] -= 2;
            results[i + 1][1] += lastCount;
        }
        write(1, results[i], resultLen[i]);
    }

    pthread_mutex_destroy(&mutexQueue);
    pthread_cond_destroy(&condQueue);

    return 0;
}