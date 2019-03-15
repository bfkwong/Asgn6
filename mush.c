#include "mush.h"

int numOfChild = 0;
pid_t children[MAX_PIPE];

int main(int argc, char *argv[]) {
    int numTotStages, inputMode=0, rdStatus = 0, i = 0;
    size_t numRead;
    char buffer[MAX_BUFFER + 1];
    char *batchBuf = NULL;
    FILE *batch;
    Stage *stageList, *tmpStg;

    /* Make sure everything in buffer is 0 */
    memset(buffer, 0, 513);

    /* Check to see if it is interative or batch mode */
    if (argc == 1) {
        /* If no arguments were given, it is interative */
        inputMode = 0;
    } else if (argc == 2) {
        /* If one argument was given */
        inputMode = 1;
        if ((batch = fopen(argv[1], "r")) == NULL) {
            perror("fopen");
            exit(EXIT_FAILURE);
        }
    } else {
        /* If more than one argument was given */
        fprintf(stderr, "usage: %s [ scriptfile ]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* Install the signal handler to catch SIGINT and check for error */
    if (installSignals() < 0)
        triggerError("installSignal", 0);

    while (1) {
        if (inputMode == 0) {
            /* If in interactive mode, print prompt and wait for input */
            if (write(STDOUT_FILENO, "8-P ", 4) < 0) {
                perror("write 8-P");
                exit(EXIT_FAILURE);
            }
            if ((rdStatus = read(STDIN_FILENO, buffer, MAX_BUFFER)) <= 0) {
                /* Check if EOF was entered */
                if (strlen(buffer) == 0 && rdStatus == 0) {
                    printf("\n");
                    exit(0);
                }
                if (errno != EINTR) {
                    perror("read");
                    exit(EXIT_FAILURE);
                }
            }

            /* Parse the interactive inputs */
            numTotStages = parseline(buffer, &stageList);
        } else {
            /* If in batch mode, read from FILE stream */
            if (getline(&batchBuf, &numRead, batch) < 0) {
                /* If end of file has been reached, exit */
                if (feof(batch)) {
                    free(batchBuf);
                    if (fclose(batch) != 0) {
                        perror("fclose");
                        exit(EXIT_FAILURE);
                    }
                    exit(0);
                } else {
                    perror("getline");
                    exit(0);
                }
            }

            /* Parse the input from the file */
            numTotStages = parseline(batchBuf, &stageList);

            /* Free the char * created by getline() */
            free(batchBuf);
            batchBuf = NULL;
        }

        /* If parseline was successful, execute the commands */
        if (numTotStages >= 0)
            executeCmds(stageList, numTotStages);

        /* Free the linked list of stages and its elements */
        while (stageList != NULL) {
            tmpStg = stageList;
            for (i = 0; i < stageList->argc; i++) {
                free((stageList->argvPtr)[i]);
            }
            stageList = stageList->next;
            free(tmpStg);
        }

        /* reset the input buffer */
        memset(buffer, 0, 513);
        /* Flush the file streams */
        fflush(stdout);
        fflush(stdin);
    }
    return 0;
}

int executeCmds(Stage *stageList, int numTotStages) {
    Stage *head = stageList;
    Stage *stgAry[MAX_PIPE];
    int redirOut, redirIn, status, j, i=0;
    int numPipes = numTotStages - 1;
    int npipe[numPipes][2];

    /* Convert linked list to array */
    while (stageList != NULL) {
        stgAry[i++] = stageList;
        stageList = stageList->next;
    }

    /* Create necessary file redirect outputs */
    stageList = head;

    for (i = 0; i<numTotStages; i++) {
        children[numTotStages] = 0;
    }

    /* Create pipes */
    for (i = 0; i<numPipes; i++) {
        /* If there is only 1 stage, no pipes created */
        if (pipe(npipe[i]) < 0)
            triggerError("pipe", 0);
    }

    /* Fork children */
    for (i=0; i<numTotStages; i++) {

        if (strcmp((stgAry[i]->argvPtr)[0], "cd") == 0) {
            /* cd cannot be forked */
            if (chdir((stgAry[i]->argvPtr)[1]) < 0) {
                perror((stgAry[i]->argvPtr)[1]);
                return 1;
            }
        } else {

            numOfChild += 1;
            /* Fork off the process and check for error */
            if ((children[i] = fork())<0)
                triggerError("fork", 0);

            if (children[i] == 0)  { /* Child stuff */

                /* Make sure that the child doesn't ignore the SIGNAL */
                signal(SIGINT, SIG_DFL);

                /* If the input is not original stdin nor a pipe, we would
                 want to open the file and read from it */
                if (strcmp("original stdin", stgAry[0]->input) != 0) {
                    redirIn = open(stgAry[0]->input, O_RDONLY);
                    if (redirIn < 0) {
                        perror(stgAry[0]->input);
                        return -1;
                    }
                } else {
                    if ((redirIn = dup(STDIN_FILENO)) < 0)
                        triggerError("Dup stdin", 0);
                }

                /* If the output is not original stdour nor a pipe, we would
                 want to open or create the file and write to it */
                if (strcmp("original stdout", stgAry[numPipes]->output) != 0) {
                    redirOut = open(stgAry[0]->output, O_RDWR|O_CREAT|O_TRUNC, 0644);
                    if (redirOut < 0) {
                        perror(stgAry[0]->output);
                        return 1;
                    }
                } else {
                    if ((redirOut = dup(STDOUT_FILENO)) < 0)
                        triggerError("Dup stdout", 0);
                }

                /* Do plumbing */
                if (i == 0 && i == numPipes) {
                    /* If there is only one stage */
                    dup2(redirIn, STDIN_FILENO);
                    dup2(redirOut, STDOUT_FILENO);
                } else if (i == 0) {
                    /* If it is the first stage, we want input
                     from a pipe */
                    dup2(redirIn, STDIN_FILENO);
                    if (numPipes > 0) {
                        dup2(npipe[i][1], STDOUT_FILENO);
                    }
                } else if (i == numPipes) {
                    /* If it is the last stage, we don't want output
                     to a pipe */
                    dup2(npipe[i-1][0], STDIN_FILENO);
                    dup2(redirOut, STDOUT_FILENO);
                } else {
                    /* All the ones in between */
                    dup2(npipe[i-1][0], STDIN_FILENO);
                    dup2(npipe[i][1], STDOUT_FILENO);
                }

                /* Close pipes to induce gurgling */
                for (j=0; j<numPipes; j++) {
                    close(npipe[j][0]);
                    close(npipe[j][1]);
                }

                /* Execute the commands */
                execvp(stgAry[i]->argvPtr[0], stgAry[i]->argvPtr);
                perror(stgAry[i]->argvPtr[0]);
                exit(EXIT_FAILURE);
            }
        }
    }

    /* Close parent's pipes */
    for (i=0; i<numPipes; i++) {
        close(npipe[i][0]);
        close(npipe[i][1]);
    }

    /* Wait for children to finish process */
    for (i = 0; i<numTotStages; i++) {
        if (children[i] != 0) {
            if (waitpid(children[i], &status, 0) < 0) {
                if (errno != EINTR) {
                    perror("Parent wait");
                    exit(EXIT_FAILURE);
                }
            }
            children[i] = 0;
        } else {
            break;
        }
    }

    return 0;
}

int installSignals() {

    /* Set up the sigactions truct */
    struct sigaction sa;
    /* Set the handler function */
    sa.sa_handler = ctrlCHandler;

    /* Set an empty set but add SIGINT, we want to make sure
    our signal handler doesn't get stopped by a SIGINT */
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGINT);
    sa.sa_flags = 0;

    /* Install the signal handler */
    if (sigaction(SIGINT, &sa, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    return 0;
}

void ctrlCHandler(int signum) {
    /* Signal handler for SIGINT */
    int status, i;

    /* When we receive a SIGINT, we should wait for
     all children to finish processing */
    for (i = 0; i<MAX_PIPE; i++) {
        /* Loop through the array of children */
        if (children[i] != 0) {
            /* wait for specific child */
            if (waitpid(children[i], &status, 0) < 0) {
                if (errno != EINTR) {
                    perror("Handler wait");
                    exit(EXIT_FAILURE);
                }
            }
            /* Set to 0 after it has been processed */
            children[i] = 0;
        } else {
            break;
        }
    }

    /* Print newline so that the prompt will be shown on the newline */
    printf("\n");
}
 