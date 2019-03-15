#include "mush.h"

/* Global Error Message for triggerError */
char ERRMSG[MAX_BUFFER];

int parseline(char *input, Stage **stgBuf)
{
    /* Initialize the stuff that I need */
    char *myArgv[MAX_ARG + 1];
    int myArgc, numTotStg, i = 0;
    Stage *stageList;

    if (strlen(input) == 0)
        return -1;
    if (strcmp(input, "\n") == 0)
        return -1;

    /* Allocate memeory for my stagelist object */
    if (!(stageList = (Stage *)calloc(1, sizeof(Stage))))
        triggerError("calloc", 0);
    stageList->next = NULL;
    stageList->prev = NULL;

    /* Split the input buffer string into args separated
     by spaces */
    if ((myArgc = strToArray(myArgv, input)) < 0) {
        return -1;
    }

    /* Turn the array of arguments into a linked list of stages */
    numTotStg = parseArgs(myArgv, myArgc, &stageList);
    if (numTotStg < 0) {
        return -1;
    }

    for (i = 0; i < MAX_ARG + 1; i++) {
        free(myArgv[i]);
    }

    *stgBuf = stageList;
    /* Print the linked list of stages */
    return numTotStg;
}

void triggerError(const char *errSrc, int mode)
{
    /* Allow errors and exits to be triggered with 1 line */
    if (mode == 0) {
        perror(errSrc);
        exit(EXIT_FAILURE);
    } else if (mode == 1) {
        fprintf(stderr, "%s\n", errSrc);
    } else if (mode == 2) {
        fprintf(stderr, "%s: %s\n", ERRMSG, errSrc);
    }
}

int strToArray(char *myArgv[], char *buf)
{
    /* Nondestructive strtok */

    int i = 0, myArgc = 0;
    char *token, *tempStr;
    char *noWS;

    for (i = 0; i < MAX_ARG + 1; i++) {
        if (!(myArgv[i] = (char *)malloc(sizeof(char) * (MAX_BUFFER+1)))) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
    }

    tempStr = (char *)malloc((strlen(buf) + 1)*sizeof(char));
    strcpy(tempStr, buf);

    token = strtok(tempStr, " \n");
    while(token != NULL ) {
        noWS = trimwhitespace(token);
        if (containsNonSpace(noWS) == 1) {
            strcpy(myArgv[myArgc],noWS);
            myArgc += 1;
        }

        token = strtok(NULL, " \n");
    }

    free(tempStr);
    return myArgc;
}

int containsNonSpace(char *str)
{
    int i;

    /* test to make sure that it is more than just white space */
    for (i = 0; i < strlen(str); i++) {
        if (str[i] != ' ' && str[i] != '\t')
            return 1;
    }
    return 0;
}

int parseArgs(char *myArgv[], int myArgc, Stage **stageList)
{
    /* Loop through the array of strings and build a linked
     list of stages */
    int i, numTotStg = 0;
    Stage *stages;
    stages = *stageList;

    /* loop through the array of strings */
    for (i = 0; i < myArgc; i++) {
        if (!strcmp(myArgv[i], "|")) {
            /* If the current argument is a pipe */
            if (!stages->argc) {
                fprintf(stderr, "invalid null command\n");
                return -1;
            }
            /* Check to makesure a pipe isn't the first arg */

            /* Check to make sure the current stage is valid */
            if (isValidStage(stages) >= 0) {
                /* If it is a valid stage, process the current node
                 in the linked list and move onto the next */

                /* Process the input output */
                if (!strlen(stages->input))
                    strcpy(stages->input, "original stdin");
                if (!strlen(stages->output))
                    sprintf(stages->output, "pipe to stage %d",
                            (stages->curStage) + 1);

                /* Make sure the pipe isn't too deep */
                if (stages->curStage > MAX_PIPE){
                    fprintf(stderr, "pipeline too deep\n");
                    return -1;
                }

                /* Create next node in the linked list */
                stages->next = (Stage *)calloc(1, sizeof(Stage));
                if (stages->next == NULL)
                    triggerError("calloc", 0);
                stages->next->curStage = (stages->curStage) + 1;
                stages->next->next = NULL;
                stages->next->prev = stages;
                sprintf(stages->next->input, "pipe from stage %d",
                        stages->curStage);

                stages = stages->next;
                numTotStg += 1;
            } else {
                /* Not a valid stage */
                return -1;
            }
        }
        else if (!strcmp(myArgv[i], ">"))
        {
            /* If the current argument is a redirect out */
            if (stages->argc == 0) {
                fprintf(stderr, "invalid null command\n");
                return -1;
            }
            /* Check to makesure a redirect isn't the first arg */
            if (!strlen(stages->output)) {
                /* Make sure that the argument following is valid */
                if (!strCmpSpecialChar(myArgv[++i])) {
                    fprintf(stderr, "%s: bad input redirection\n", stages->argvPtr[0]);
                    return -1;
                }
                /* Copy the input into the argument list */
                strcpy(stages->output, myArgv[i]);
                /* reformat the full command string */
                sprintf(stages->fullCmd, "%s > %s",
                        stages->fullCmd, myArgv[i]);
            } else {
                /* If another redirect existed earlier in the stage,
                 return an error */
                fprintf(stderr, "%s: bad output redirection\n", stages->argvPtr[0]);
                return -1;
            }
        }
        else if (!strcmp(myArgv[i], "<")) {
            /* If the current argument is a redirect in */
            if (!stages->argc) {
                fprintf(stderr, "invalid null command\n");
                return -1;
            }
            /* Check to makesure a redirect isn't the first arg */

            if (stages->curStage > 0)
            {
                /* Check to makesure we're not trying to redirect an
                 input for a command that is piped in */
                fprintf(stderr, "%s: ambiguous input\n", stages->argvPtr[0]);
                return -1;
            }
            if (!strlen(stages->input))
            {
                /* Make sure that the argument following is valid */
                if (!strCmpSpecialChar(myArgv[++i])) {
                    fprintf(stderr, "%s: bad output redirection\n", stages->argvPtr[0]);
                    return -1;
                }
                /* Copy the input into the argument list */
                strcpy(stages->input, myArgv[i]);
                /* reformat the full command string */
                sprintf(stages->fullCmd, "%s < %s",
                        stages->fullCmd, myArgv[i]);
            }
            else
            {
                /* If another redirect existed earlier in the stage,
                 return an error */
                fprintf(stderr, "%s: bad input redirection\n", stages->argvPtr[0]);
                return -1;
            }
        }
        else
        {
            /* If the current argument is a plain ol' regular command */
            sprintf(stages->fullCmd, "%s %s", stages->fullCmd, myArgv[i]);

            (stages->argvPtr)[(stages->argc)] = (char *) malloc((strlen(myArgv[i])+1) * sizeof(char));
            if ((stages->argvPtr)[(stages->argc)] == 0) {
                perror("malloc");
                exit(EXIT_FAILURE);
            }
            strcpy((stages->argvPtr)[(stages->argc)], myArgv[i]);
            (stages->argc) += 1;
        }
    }

    /* If the ouput and the input of the current stage is still empty,
     it means that the input and the output is stdio */

    if (!strlen(stages->input))
        strcpy(stages->input, "original stdin");
    if (!strlen(stages->output))
        strcpy(stages->output, "original stdout");

    if (stages->argc == 0) {
        fprintf(stderr, "invalid null commands\n");
        return -1;
    }

    return numTotStg + 1;
}

int strCmpSpecialChar(char *str)
{
    /* Test to see if an argument is a <, >, or | */
    if (!strcmp(str, "<"))
    {
        return 0;
    }
    else if (!strcmp(str, ">"))
    {
        return 0;
    }
    else if (!strcmp(str, "|"))
    {
        return 0;
    }
    return 1;
}

int isValidStage(Stage *s)
{
    /* There is a pipe meaning receiving pipe can't have in
     out going pipe can't have stdout */

    if (s->argc == 0) {
        fprintf(stderr, "invalid null command\n");
        return -1;
    /* Not valid because no/too many argument */
    }
    if (s->curStage >= MAX_PIPE) {
        fprintf(stderr, "pipe too deep\n");
        return -1;
    /* Not valid because output conflicts with pipe */
    }
    if (strlen(s->output) != 0)
    {
        fprintf(stderr, "%s: bad input redirection\n", s->argvPtr[0]);
        return -1;
        /* Not valid because output conflicts with pipe */
    }

    return 0;
}

char *trimwhitespace(char *str)
{
    char *end;

    // Trim leading space
    while(isspace((unsigned char)*str)) str++;

    if(*str == 0)  // All spaces?
        return str;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end))
        end--;

    // Write new null terminator character
    end[1] = '\0';

    return str;
}

int printStages(Stage *stages)
{
    int i;

    /* Loop through the linked list of stages */
    while (stages)
    {
        /* Make sure there isn't too many arguments to a command */
        if (stages->argc > MAX_CMD)
        {
            strcpy(ERRMSG, stages->argvPtr[0]);
            triggerError("too many arguments", 2);
        }

        /* Print */
        printf("\n--------\nStage %d: \"%s\"\n--------\n",
               stages->curStage, (stages->fullCmd) + 1);
        printf("%10s: %s\n", "input", stages->input);
        printf("%10s: %s\n", "output", stages->output);
        printf("%10s: %d\n", "argc", stages->argc);
        printf("%10s: ", "argv");

        printf("\"%s\"", stages->argvPtr[0]);
        for (i = 1; i < ((stages->argc)); i++)
            printf(", \"%s\"", stages->argvPtr[i]);
        printf("\n");

        /* Move onto the next element in the list */
        stages = stages->next;
    }
    return 0;
}
