#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>

#define MAX_COMMAND 1000
#define MAX_JOBS 100

// void sigint_handler(int signum)
// {
//     signal(SIGINT, sigint_handler);
// }
char *jobs[MAX_JOBS];
pid_t jobIds[MAX_JOBS];
int numJobs = 0;
const char inputString[MAX_COMMAND];
pid_t pid;

void printShell()
{
    char cwd[MAX_COMMAND];
    getcwd(cwd, sizeof(cwd));
    char *basename, *token;
    char *fileDiv = strstr(cwd, "\\") != NULL ? "\\" : "/";
    for (token = strtok(cwd, fileDiv); token != NULL; token = strtok(NULL, fileDiv))
        basename = token;
    printf("[nyush %s]$ ", basename);
    fflush(stdout);
}
void sigint_handler(int sig)
{
    // printf("\n");
    // printShell();
}
void sigint_handler_p(int sig)
{
    // printf("\n");
}
void sigint_handler_add(int sig)
{
    // printf("so i made it\n");
    size_t len = strlen(inputString);
    jobs[numJobs] = malloc((1 + len) + sizeof(char));
    strncpy(jobs[numJobs], inputString, len);
    jobIds[numJobs] = pid;
    jobs[numJobs++][len] = '\0';
    jobs[numJobs] = NULL;
    // printf("im down here\n");
}
// https://www.geeksforgeeks.org/making-linux-shell-c/
int getInput(char *str)
{
    char *line;
    str[0] = '\0';
    size_t len = 0;
    ssize_t lineSize = 0;
    lineSize = getline(&line, &len, stdin);
    line[strlen(line) - 1] = '\0';
    if (lineSize == -1)
        exit(0);
    if (strlen(line) != 0)
    {
        strcpy(str, line);
        return 0;
    }
    else
        return 1;
}

int parseSymbols(const char *input, char **inputArgs)
{
    char *other = malloc(1 + strlen(input) * sizeof(char));
    strcpy(other, input);
    char *token = strtok(other, "|");
    int i = 0;
    for (; token != NULL; i++)
    {
        inputArgs[i] = malloc(1 + strlen(token) * sizeof(char));
        strcpy(inputArgs[i], token);
        token = strtok(NULL, "|");
    }
    inputArgs[i] = NULL;
    return i;
}
char *del(char *arr[], int pos)
{
    char *ret = malloc((1 + strlen(arr[pos])) * sizeof(char));
    strcpy(ret, arr[pos]);
    for (int i = pos; arr[i]; i++)
    {
        arr[i] = arr[i + 1];
    }
    return ret;
}
pid_t delPid(pid_t arr[], int pos)
{
    pid_t ret = arr[pos];
    for (int i = pos; arr[i]; i++)
    {
        arr[i] = arr[i + 1];
    }
    return ret;
}
int indexOf(char *arr[], char *str)
{
    int i;
    for (i = 0; arr[i]; ++i)
    {
        if (strcmp(arr[i], str) == 0)
        {
            return i;
        }
    }
    return -1;
}
void parseSpaces(char *inputArgs[MAX_COMMAND], char *splitArgs[MAX_COMMAND][MAX_COMMAND])
{
    int i = 0;
    for (int j = 0; inputArgs[i]; i++, j = 0)
    {
        char *ptr;
        char *curr = malloc((strlen(inputArgs[i]) + 1) * sizeof(char));
        strcpy(curr, inputArgs[i]);
        for (ptr = strtok(curr, " "); ptr != NULL; ptr = strtok(NULL, " "), j++)
        {
            splitArgs[i][j] = malloc((strlen(ptr) + 1) * sizeof(char));
            strcpy(splitArgs[i][j], ptr);
            splitArgs[i][j][strlen(ptr) + 1] = '\0';
        }
        splitArgs[i][j] = NULL;
    }

    splitArgs[i][0] = NULL;
}
void clearOut(char *splitArgs[MAX_COMMAND][MAX_COMMAND])
{
    for (size_t i = 0; splitArgs[i][0]; i++)
    {
        for (size_t j = 0; splitArgs[i][j]; j++)
        {
            splitArgs[i][j] = NULL;
        }
    }
}
int main()
{
    while (1)
    {
        int status;
        signal(SIGINT, sigint_handler);
        // signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        printShell();
        if (getInput(inputString))
            continue;
        // printf("Input command: %s\n", inputString);
        char *inputArgs[MAX_COMMAND];
        char *splitArgs[MAX_COMMAND][MAX_COMMAND];
        clearOut(splitArgs);

        int numCmds = parseSymbols(inputString, inputArgs);
        parseSpaces(inputArgs, splitArgs);
        if (strcmp(inputString, "exit\0") == 0)
        {
            if (!splitArgs[0][2])
            {
                if (!jobs[0])
                {
                    exit(0);
                }
                else
                {
                    fprintf(stderr, "Error: there are suspended jobs\n");
                }
            }
            else
            {
                fprintf(stderr, "Error: invalid command\n");
            }
            continue;
        }
        if (strcmp(splitArgs[0][0], "fg\0") == 0)
        {
            if (splitArgs[0][2] == NULL && splitArgs[0][1] != NULL)
            {
                int job = atoi(splitArgs[0][1]);
                if (job > 0 && job <= numJobs)
                {
                    char *jobName = del(jobs, job - 1);
                    if (kill(delPid(jobIds, job - 1), SIGCONT) < 0)
                    {
                        fprintf(stderr, "Error: failed to restart process\n");
                        continue;
                    }
                    numJobs--;
                    if (waitpid(-1, &status, WUNTRACED) > 0)
                    {
                        if (WIFSTOPPED(status))
                        {
                            size_t len = strlen(jobName);
                            jobs[numJobs] = malloc((1 + len) + sizeof(char));
                            strncpy(jobs[numJobs], jobName, len);
                            jobIds[numJobs] = pid;
                            jobs[numJobs++][len] = '\0';
                            jobs[numJobs] = NULL;
                        }
                    }
                }
                else
                {
                    fprintf(stderr, "Error: invalid job\n");
                }
            }
            else
            {
                fprintf(stderr, "Error: invalid command\n");
            }
            continue;
        }
        if (strcmp(splitArgs[0][0], "jobs\0") == 0)
        {
            if (splitArgs[0][1] == NULL)
            {
                for (int i = 0; jobs[i]; i++)
                {
                    printf("[%d] %s\n", i + 1, jobs[i]);
                }
            }
            else
            {
                fprintf(stderr, "Error: invalid command\n");
            }
            continue;
        }
        if (strcmp(splitArgs[0][0], "cd\0") == 0)
        {
            if (!splitArgs[0][2])
            {
                if (chdir(splitArgs[0][1]) != 0)
                {
                    fprintf(stderr, "Error: invalid directory\n");
                }
            }
            else
            {
                fprintf(stderr, "Error: invalid command\n");
            }
            continue;
        }
        int pipeFd[numCmds][2];
        if (numCmds > 1)
        {
            for (int i = 0; i < numCmds; i++)
            {
                if (pipe(pipeFd[i]) < 0)
                {
                    return 1;
                }
            }
        }
        for (int i = 0; splitArgs[i][0]; i++)
        {
            char binCMD[MAX_COMMAND] = "/usr/bin/";
            int inpos = indexOf(splitArgs[i], "<");
            int isInputRedirect = inpos != -1;
            char *inputFile;
            if (isInputRedirect)
            {
                del(splitArgs[i], inpos);
                inputFile = del(splitArgs[i], inpos);
            }
            int outAppendpos = indexOf(splitArgs[i], ">>");
            int isOutputRedirectAppend = outAppendpos != -1;
            char *outputAppendFile;
            if (isOutputRedirectAppend)
            {
                del(splitArgs[i], outAppendpos);
                outputAppendFile = del(splitArgs[i], outAppendpos);
            }
            int outpos = indexOf(splitArgs[i], ">");
            int isOutputRedirect = outpos != -1;
            char *outputFile;
            if (isOutputRedirect)
            {
                del(splitArgs[i], outpos);
                outputFile = del(splitArgs[i], outpos);
            }

            if (splitArgs[i][0][0] == '/')
            {
                memset(binCMD, '\0', sizeof(splitArgs[i][0]));
                strcpy(binCMD, splitArgs[i][0]);
            }
            else if (strchr(splitArgs[i][0], '/') != NULL)
            {
                if (splitArgs[i][0][0] == '.' && splitArgs[i][0][1] == '/')
                {
                    memset(binCMD, '\0', sizeof(binCMD));
                    strcpy(binCMD, splitArgs[i][0]);
                }
                else
                {
                    memset(binCMD, '\0', sizeof(binCMD));
                    strcpy(binCMD, "./");
                    strcat(binCMD, splitArgs[i][0]);
                }
            }
            else
            {
                strcat(binCMD, splitArgs[i][0]);
            }
            pid = fork();
            if (pid == 0)
            {
                // printf("im in the child\n");
                signal(SIGINT, SIG_DFL);
                signal(SIGQUIT, SIG_DFL);
                signal(SIGTSTP, sigint_handler_add);
                signal(SIGTSTP, SIG_DFL);
                signal(SIGSTOP, sigint_handler_add);
                signal(SIGSTOP, SIG_DFL);
                if (isInputRedirect)
                {
                    int fd1 = open(inputFile, O_RDONLY);
                    if (fd1 < 0)
                    {
                        fprintf(stderr, "Error: invalid file\n");
                        continue;
                    }
                    else
                    {
                        dup2(fd1, 0);
                        close(fd1);
                    }
                }
                if (isOutputRedirectAppend)
                {
                    int fd3 = open(outputAppendFile, O_APPEND | O_CREAT | O_WRONLY, 0777);
                    if (fd3 < 0)
                    {
                        fprintf(stderr, "Error: invalid file\n");
                        continue;
                    }
                    else
                    {
                        dup2(fd3, 1);
                        close(fd3);
                    }
                }
                if (isOutputRedirect)
                {
                    int fd2 = open(outputFile, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR);
                    if (fd2 < 0)
                    {
                        fprintf(stderr, "Error: invalid file\n");
                        continue;
                    }
                    else
                    {
                        dup2(fd2, 1);
                        close(fd2);
                    }
                }
                if (numCmds > 1)
                {
                    if (i != 0)
                    {
                        dup2(pipeFd[i - 1][0], STDIN_FILENO);
                    }
                    // ping -c 5 google.com | grep rtt
                    if (splitArgs[i + 1][0])
                    {
                        dup2(pipeFd[i][1], STDOUT_FILENO);
                    }
                    for (int k = 0; k < numCmds; k++)
                    {
                        close(pipeFd[k][0]);
                        close(pipeFd[k][1]);
                    }
                }
                if (execvp(binCMD, splitArgs[i]) < 0)
                {
                    fprintf(stderr, "Error: invalid command\n");
                    exit(1);
                }
            }
            else
            {
                if (numCmds == 1)
                {
                    // printf("%d\n", pid);
                    if (waitpid(-1, &status, WUNTRACED) > 0)
                    {
                        if (WIFSTOPPED(status))
                        {
                            sigint_handler_add(SIGTSTP);
                        }
                    }
                    // signal(SIGINT, sigint_handler_p);
                    signal(SIGSTOP, SIG_IGN);
                    signal(SIGQUIT, SIG_IGN);
                    // signal(SIGTSTP, SIG_IGN);
                }
            }
        }

        if (numCmds > 1)
        {
            wait(NULL);
            for (int k = 0; k < numCmds; k++)
            {
                close(pipeFd[k][0]);
                close(pipeFd[k][1]);
            }
            for (int k = 0; k < numCmds - 1; k++)
            {
                wait(NULL);
            }
        }
    }
    return 0;
}
// }
// // process
// execFlag = processString(inputString,
//                          parsedArgs, parsedArgsPiped);
// // execflag returns zero if there is no command
// // or it is a builtin command,
// // 1 if it is a simple command
// // 2 if it is including a pipe.

// // execute
// if (execFlag == 1)
//     execArgs(parsedArgs);

// if (execFlag == 2)
//     execArgsPiped(parsedArgs, parsedArgsPiped);

// parse(inputString, inputArgs2);
// char *args[2];
// args[0] = inputArgs2[0];
// args[1] = NULL;
// strcat(binCMD, inputArgs2[0]);
// if (fork() == 0)
// {
//     printf("%s\n", binCMD);
//     for (size_t i = 0; args[i]; i++)
//     {
//         printf("%s\n", args[i]);
//     }

//     if (execvp(binCMD, args) < 0)
//     {
//         printf("Error: invalid program\n");
//     }
// }
// else
// {
//     wait(NULL);
// }
// continue;

// char *is = malloc(strlen(inputString) * sizeof(char));
//             strcpy(is, inputString);
//             char *inputRedirectString = strchr(is, '<');
//             char *inputFile;
//             int hasInputRedirect = inputRedirectString != NULL;
//             if (hasInputRedirect)
//             {
//                 inputFile = strtok(inputRedirectString, " ");
//                 inputFile = strtok(NULL, " ");
//             }

//             char *is3 = malloc(strlen(inputString) * sizeof(char));
//             strcpy(is3, inputString);
//             char *outputRedirectAppendString = strstr(is3, ">>");
//             char *outputAppendFile;
//             int hasOutputRedirectAppend = outputRedirectAppendString != NULL;
//             if (hasOutputRedirectAppend)
//             {
//                 outputAppendFile = strtok(outputRedirectAppendString, " ");
//                 outputAppendFile = strtok(NULL, " ");
//             }

//             char *is2 = malloc(strlen(inputString) * sizeof(char));
//             strcpy(is2, inputString);
//             char *outputRedirectString = strchr(is2, '>');
//             char *outputFile;
//             int hasOutputRedirect = outputRedirectString != NULL && !hasOutputRedirectAppend;
//             if (hasOutputRedirect)
//             {
//                 outputFile = strtok(outputRedirectString, " ");
//                 outputFile = strtok(NULL, " ");
//             }

// int isOutputRedirect = strcmp(splitArgs[i][0], ">") == 0;        // splitArgs[i][0] &&
// int isOutputRedirectAppend = strcmp(splitArgs[i][0], ">>") == 0; // splitArgs[i][0] &&
// int isInputRedirect = strcmp(splitArgs[i][0], "<") == 0;         // splitArgs[i][0] &&

// FILE *file = fopen(outputAppendFile, "a+");
// dup2(fileno(file), fileno(stdout));
// fclose(file);
// open(outputAppendFile, O_CREAT | O_APPEND | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR);

/*
if (numCmds > 1)
                {
                    // for (int k = 0; k < numCmds; k++)
                    // {
                    //     if (k != (i - 1) && k != i)
                    //     {
                    //         // printf("%s closing %d\n", splitArgs[i][0], k);
                    //         // close(pipeFd[k][0]);
                    //         // close(pipeFd[k][1]);
                    //     }
                    // }
                    if (i != 0)
                    {
                        // printf("%s reading from the pipe\n", splitArgs[i][0]);
                        dup2(pipeFd[i - 1][0], STDIN_FILENO);
                        // for (int k = 0; k < numCmds; k++)
                        // {
                        //     close(pipeFd[k][0]);
                        //     close(pipeFd[k][1]);
                        // }
                        // printf("%s closing\n", splitArgs[i][0]);

                        // close(pipeFd[i - 1][0]);
                        // close(pipeFd[i - 1][1]);
                    }
                    // ping -c 5 google.com | grep rtt
                    if (splitArgs[i + 1][0])
                    {

                        // for (int k = 0; k < numCmds; k++)
                        // {
                        //     if (k != i)
                        //     {
                        //         close(pipeFd[k][0]);
                        //         close(pipeFd[k][1]);
                        //     }
                        // }
                        // printf("%s writing to the pipe\n", splitArgs[i][0]);
                        dup2(pipeFd[i][1], STDOUT_FILENO);
                        // for (int k = 0; k < numCmds; k++)
                        // {
                        //     close(pipeFd[k][0]);
                        //     close(pipeFd[k][1]);
                        // }
                        // printf("%s closing write end\n", splitArgs[i][0]);
                        // close(pipeFd[i][0]);
                        // close(pipeFd[i][1]);
                        // for (int k = 0; k < numCmds; k++)
                        // {
                        //     close(pipeFd[k][0]);
                        //     close(pipeFd[k][1]);
                        // }
                    }
                    for (int k = 0; k < numCmds; k++)
                    {
                        close(pipeFd[k][0]);
                        close(pipeFd[k][1]);
                    }
                }
                if (execvp(binCMD, splitArgs[i]) < 0)
                {
                    fprintf(stderr, "Error: invalid command\n");
                    exit(1);
                }
            }
            // else
            // {
            //     // wait(NULL);
            //     wait(NULL);
            //     // while (waitpid(0, 0, 0) < 0)
            //     //     ;

            //     if (numCmds > 1)
            //     {
            //         printf("More than one\n");
            //         wait(NULL);
            //         for (int k = 0; k < numCmds; k++)
            //         {
            //             printf("in parent closing %d\n", k);
            //             close(pipeFd[k][0]);
            //             close(pipeFd[k][1]);
            //         }
            //     }
            // }
            // printf("2 This is the main command: %s\n", binCMD);
*/