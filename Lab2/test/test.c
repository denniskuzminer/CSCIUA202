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
int parse(const char *input, char **inputArgs)
{
    int j = 0, start = 0, i = 0;
    for (; input[i]; i++)
        if (includes((char[2]){(char)input[i], '\0'}, SPEC_CHARS))
        {
            char subbuff[i - start + 1];
            memcpy(subbuff, &input[start], i - start);
            subbuff[i - start] = '\0';
            inputArgs[j] = malloc(strlen(subbuff) * sizeof(char));
            strcpy(inputArgs[j++], subbuff);

            inputArgs[j] = malloc(2 * sizeof(char));
            strcpy(inputArgs[j++], (char[2]){(char)input[i], '\0'});
            start = i + 1;
        }
    char subbuff[i - start + 1];
    memcpy(subbuff, &input[start], i - start);
    subbuff[i - start] = '\0';

    inputArgs[j] = malloc(strlen(subbuff) * sizeof(char));
    strcpy(inputArgs[j++], subbuff);

    inputArgs[j] = NULL;
    return j;
}

void parseSpaces(char *inputArgs[MAX_COMMAND], char *splitArgs[MAX_COMMAND][MAX_COMMAND])
{
    int i = 0;
    for (int j = 0; inputArgs[i]; i++, j = 0)
    {

        printf("whole string %s\n", inputArgs[i]);
        char *ptr;
        char *curr = malloc(strlen(inputArgs[i]) * sizeof(char));
        strcpy(curr, inputArgs[i]);
        for (ptr = strtok(curr, " "); ptr != NULL; ptr = strtok(NULL, " "), j++)
        {
            printf("'%s'\n", ptr);
            splitArgs[i][j] = malloc(strlen(ptr) * sizeof(char));
            strcpy(splitArgs[i][j], ptr);
        }
        splitArgs[i][j] = NULL;
    }

    splitArgs[i][0] = NULL;
}

int main(void)
{

    char *inputArgs[MAX_COMMAND];
    char *splitArgs[MAX_COMMAND][MAX_COMMAND];
    int numCMDs = parse(sentences, inputArgs);
    parseSpaces(inputArgs, splitArgs);
    for (size_t i = 0; splitArgs[i][0]; i++)
    {
        for (size_t j = 0; splitArgs[i][j]; j++)
        {
            printf("processed string %s\n", splitArgs[i][j]);
        }
        printf("new cmd\n");
    }
    printf("%d\n", numCMDs);
}