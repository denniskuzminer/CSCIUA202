#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "thread_pool.h"

#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>

#define block_size 4096  // can be modify to how many bytes for one process
// #define GB_size 1073741824 //1G
#define GB_size 1073741824  //500Mx2
#define MAX_BLOCK_NUM 256000 //500Mx2
//#define GB_size 536870912  //500M
//#define MAX_BLOCK_NUM 128000 //500M

static unsigned char g_inputData[GB_size] = {0};
static unsigned long int g_inputIndex = 0;
static unsigned char g_outputData[GB_size] = {0};
static unsigned long int g_outputIndex[MAX_BLOCK_NUM] = {0};

void * encode(void * arg)
{
    // printf("---[%ld]---\n", (long unsigned int)arg);
    long unsigned int offset = (long unsigned int)arg;
    long unsigned int startIndex = offset * block_size;
    long unsigned int endIndex = (offset + 1) * block_size;
    long unsigned int outputIndex = offset * block_size;
    unsigned char prec = 0;
    int newFlag = 1;
    unsigned char counter = 0;

    // output buffer is full
    if(offset >= MAX_BLOCK_NUM)
    {
        printf("ERROR: ioutput buffer full.\n");
        return NULL;
    }

    // printf("tid:%ld-DEBUG: input==offset:%ld==start:%ld==end:%ld==\n", pthread_self(), offset, startIndex, endIndex);

    for (unsigned long int i = startIndex; i < endIndex; i++)
    {
        if (offset >= 48)
        {
            // printf("-%ld-1-%ld-[%02x]-[%02x]\n", offset, i, g_inputData[i], g_inputData[i+1]);
        }
        // exit with end
        if(i >= g_inputIndex)
        {
            // printf("==debug: input end===\n");
            break;
        }
        if (offset >= 45)
        {
            // printf("-%ld-2-%ld-[%02x]-[%02x]\n", offset, i, g_inputData[i], g_inputData[i+1]);
        }

        // first loop
        if(newFlag == 1)
        {
            prec = g_inputData[i];
            counter += 1;
            newFlag = 0;
        }
        // same char
        else if(g_inputData[i] == prec)
        {
            counter += 1;
        }
        // dif char
        else
        {
            g_outputData[outputIndex] = prec;
            outputIndex += 1;
            g_outputData[outputIndex] = counter;
            outputIndex += 1;

            counter = 1;
            prec = g_inputData[i];
        }
        if (offset >= 45)
        {
            // printf("-%ld-3-%ld-[%02x]-[%02x]\n", offset, i, g_inputData[i], g_inputData[i+1]);
        }
    }
    // if (offset >= 45)
        // printf("-%ld-4-%ld-[%02x]-[%02x]\n", offset, endIndex, g_inputData[endIndex], g_inputData[endIndex+1]);
    // for last char
    g_outputData[outputIndex] = prec;
    outputIndex += 1;
    g_outputData[outputIndex] = counter;
    outputIndex += 1;

    // update output inex
    // if (offset >= 45)
        // printf("-%ld-5-%ld-[%02x]-[%02x]\n", offset, endIndex, g_inputData[endIndex], g_inputData[endIndex+1]);
    g_outputIndex[offset] = outputIndex - offset * block_size;
    // printf("-%ld-6-%ld-%ld-[%02x]\n", offset, startIndex, outputIndex, g_inputData[endIndex]);

    return NULL;
}

int main(int argc, const char * argv[])
{
    thread_pool_t pool;
    unsigned long int fileNum = 0;
    unsigned long int jobs = 0;
    unsigned long int taskNum = 0;
    unsigned char prec = 0;
    unsigned char counter = 0;
    int fd = -1;
    unsigned char *p_map = NULL;
    unsigned long int file_size = 0;
    struct stat file_stat;
    unsigned char buffer[16] = {0};
    int newFlag = 1;

    // check para
    if (argc < 2)
    {
        printf("ERROR: invalid para1.\n");
        return -1;
    }

    // in parall
    if (strcmp(argv[1], "-j") == 0)
    {
        // check para
        if (argc < 4)
        {
            printf("ERROR: invalid para2.\n");
            return -1;
        }

        //get process num and filenum
        jobs = strtol(argv[2], NULL, 10);
        if(jobs < 2)
        {
            printf("ERROR: invalid para3.\n");
            return -1;
        }
        // printf("==DEBUG: process num %d==\n", jobs);
        fileNum = argc - 3;

        // read file into inputData
        for (unsigned long int i = 0; i < fileNum; i++)
        {
            // read file
            fd = open(argv[i + 3], O_RDONLY);
            if(fd == -1)
            {
                printf("ERROR: read file fail.\n");
                return -1;
            }
            fstat(fd, &file_stat);
            file_size = file_stat.st_size;
            p_map = (unsigned char*)mmap(NULL, file_size, PROT_READ, MAP_SHARED, fd, 0);
            if(p_map == MAP_FAILED)
            {
                printf("ERROR: mmap file fail.\n");
                return -1;
            }
            close(fd);
            memcpy(g_inputData + g_inputIndex, p_map, file_size);
            g_inputIndex += file_size;
            // release
            munmap(p_map, file_size);
        }

        // add task for pool:
        if(g_inputIndex % block_size == 0)
        {
            taskNum = g_inputIndex / block_size;
        }
        else
        {
            taskNum = g_inputIndex / block_size + 1;
        }

        // printf("==DEBUG: ==g_inputIndex %ld==taskNum %d=\n", g_inputIndex, taskNum);
        pool = thread_pool_create(jobs);
        // each block for each process
        for (unsigned long int i = 0; i < taskNum; i++)
        {
            // printf("==DEBUG: ==add_task %ld=\n", i);
            thread_pool_add_task(pool, encode, (void*)i);
        }
        // release pool and join, wait for sub process
        thread_pool_destroy(pool);


        //debug
        /*
        sleep(2);
        printf("-------g_inputData %ld---------\n", g_inputIndex);
        for(long int i = 0; i < g_inputIndex; i++)
        {
            if(i % block_size == 0)
            {
                printf("-");
            }
            printf("%c", g_inputData[i]);
        }*/
        // printf("\n-------g_outputData---------\n");
        // for (unsigned long int i = 0; i < taskNum ; i++)
        // {
        //     printf("\n-------taskNum %ld = %ld---------\n, i, taskNum");
        //     fwrite(g_outputData + i * block_size, sizeof(char), g_outputIndex[i], stdout);
        //     fflush(stdout);
        //     printf("\n\n");
        // }
        // printf("\n\n");


        // fix the gap between blocks
        sleep(2);
        // printf("\n-------taskNum %ld---\n", taskNum);
        for (unsigned long int i = 0; i < taskNum -1 ; i++)
        {
            if(g_outputData[i * block_size + g_outputIndex[i] - 2] == g_outputData[(i+1) * block_size])
            {
                g_outputData[(i + 1) * block_size + 1] += g_outputData[i * block_size + g_outputIndex[i] - 1];
                g_outputIndex[i] -= 2;
            }
            fwrite(g_outputData + i * block_size, sizeof(char), g_outputIndex[i], stdout);
            fflush(stdout);
        }

        // for last block
        fwrite(g_outputData + (taskNum -1) * block_size, sizeof(char), g_outputIndex[taskNum -1], stdout);
        fflush(stdout);
    }

    // in single
    else
    {
        fileNum = argc - 1;
        for (unsigned long int i = 0; i < fileNum; i++)
        {
            // read file
            fd = open(argv[i + 1], O_RDONLY);
            if(fd == -1)
            {
                printf("ERROR: read file fail.\n");
                return -1;
            }
            fstat(fd, &file_stat);
            file_size = file_stat.st_size;
            p_map = (unsigned char*)mmap(NULL, file_size, PROT_READ, MAP_SHARED, fd, 0);
            if(p_map == MAP_FAILED)
            {
                printf("ERROR: mmap file fail.\n");
                return -1;
            }
            close(fd);

            // get char
            for (unsigned long int j = 0; j < file_size; j++)
            {
                // printf("==DEBUG: =%d=%c=%02x=\n", j, p_map[j], prec);
                if(newFlag == 1)
                {
                    // printf("-1-\n");
                    prec = p_map[j];
                    counter += 1;
                    newFlag = 0;
                }
                else if(p_map[j] == prec)
                {
                    // printf("-2-\n");
                    counter += 1;
                }
                else
                {
                    // printf("%c%c", prec, counter);
                    buffer[0] = prec;
                    buffer[1] = counter;
                    fwrite(buffer, sizeof(char), 2, stdout);
                    fflush(stdout);
                    counter = 1;
                    prec = p_map[j];
                }
            }
            //release
            munmap(p_map, file_size);
        }

        // for last char of last file
        // printf("%c%c", prec, counter);
        buffer[0] = prec;
        buffer[1] = counter;
        fwrite(buffer, sizeof(char), 2, stdout);
        fflush(stdout);
    }

    return 0;
}

