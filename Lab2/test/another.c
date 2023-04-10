#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define MAX_COMMAND 1000

const char *sentences = "He hi      ho  | poop \0";
const char *SPEC_CHARS[] = {
    ">",
    "<",
    "|",
    ">>",
    "<<", 0};
int includes(char *s, const char *x[6])
{
    for (int i = 0; x[i]; i++)
        if (strcmp(x[i], s) == 0)
            return 1;
    return 0;
}
int parseSymbols(const char *input, char **inputArgs)
{
    size_t totLen = strlen(input);
    int i = 0, start = 0, j = 0, len = NULL, nextStart = NULL;
    char *oneChar, *twoChar;
    for (int numChar = 0; input[i]; i++, start = nextStart)
    {
        oneChar = (char[2]){(char)input[i], '\0'};
        twoChar = (char[3]){(char)input[i], (char)input[i + 1], '\0'};
        numChar = includes(twoChar, SPEC_CHARS) ? 2 : includes(oneChar, SPEC_CHARS) ? 1
                                                                                    : 0;
        if (numChar == 0)
        {
            continue;
        }
        // first
        len = (i - 2) - start + 1;
        inputArgs[j] = malloc(len * sizeof(char));
        printf("%d\n", j);
        strncpy(inputArgs[j], input + start, len);
        inputArgs[j++][len] = '\0';

        // middle
        len = numChar + 1;
        inputArgs[j] = malloc(len * sizeof(char));
        printf("%d\n", j);
        if (numChar == 2)
            strncpy(inputArgs[j++], twoChar, len);
        else if (numChar == 1)
            strncpy(inputArgs[j++], oneChar, len);

        // rest
        nextStart = i + len;
        if (numChar == 2)
        {
            i++;
        }
    }
    // finally
    len = totLen - nextStart + 1;
    inputArgs[j] = malloc(len * sizeof(char));
    printf("%d\n", j);
    strncpy(inputArgs[j], input + nextStart, len);
    inputArgs[j++][len] = '\0';
    inputArgs[j] = NULL;

    for (size_t i = 0; inputArgs[i]; i++)
    {
        printf("%s\n", inputArgs[i]);
    }

    return j;
}
int main(void)
{

    char *inputArgs[MAX_COMMAND];
    char *splitArgs[MAX_COMMAND][MAX_COMMAND];
    char *inputString = "ls > hi.txt >> yo yo yo yo | hey << l | p | w\0";
    int numCMDs = parseSymbols(inputString, inputArgs);
    // parseSpaces(inputArgs, splitArgs);
    // for (size_t i = 0; splitArgs[i][0]; i++)
    // {
    //     for (size_t j = 0; splitArgs[i][j]; j++)
    //     {
    //         printf("processed string %s\n", splitArgs[i][j]);
    //     }
    //     printf("new cmd\n");
    // }
    // printf("%d\n", numCMDs);
}