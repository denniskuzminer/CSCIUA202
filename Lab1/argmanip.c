// Used a bit of help from this https://stackoverflow.com/a/36804895
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "argmanip.h"

char **manipulate_args(int argc, const char *const *argv, int (*const manip)(int))
{
    char **newArgs = malloc((argc + 1) * sizeof *newArgs);
    for (int i = 0; i < argc; i++)
    {
        size_t s_l = strlen(argv[i]) + 1;
        newArgs[i] = malloc(s_l * sizeof(char));
        char *str = malloc(s_l * sizeof(char));
        strcpy(str, argv[i]);
        int j = 0;
        for (; str[j]; j++)
        {
            str[j] = manip(str[j]);
        }
        str[j] = '\0';
        strcpy(newArgs[i], str);

        free(str);
    }
    newArgs[argc] = NULL;
    return newArgs;
}

void free_copied_args(char **args, ...)
{
    for (size_t i = 0; args[i] != NULL; i++)
    {
        free(args[i]);
    }
    free(args);
    va_list va;
    va_start(va, args);
    while (1)
    {
        char **arr = va_arg(va, char **);
        if (arr == NULL)
            break;
        for (size_t j = 0; arr[j] != NULL; j++)
        {
            free(arr[j]);
        }
        free(arr);
    }
    va_end(va);
}
