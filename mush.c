#include "mush.h"

int numOfChild = 0;
pid_t children[MAX_PIPE];

int main(int argc, char *argv[])
{
    int numTotStages, inputMode=0;
    size_t numRead;
    char buffer[MAX_BUFFER + 1];
    char *batchBuf = NULL;
    FILE *batch;
    Stage *stageList;
    
    memset(buffer, 0, 513);
    if (installSignals() < 0)
        triggerError("installSignal", 0);
    
    if (argc == 1) {
        inputMode = 0;
    } else if (argc == 2) {
        inputMode = 1;
        batch = fopen(argv[1], "r");
    } else {
        fprintf(stderr, "usage: %s [ scriptfile ]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    while (1) {
        if (inputMode == 0) {
            if (write(STDOUT_FILENO, "8-P ", 4) < 0) {
                perror("write 8-P");
                exit(EXIT_FAILURE);
            }
            if (read(STDIN_FILENO, buffer, 512) < 0) {
                perror("Read Input");
                exit(EXIT_FAILURE);
            }
            numTotStages = parseline(buffer, &stageList);
        } else {
            if (getline(&batchBuf, &numRead, batch) < 0) {
                if (feof(batch)) {
                    exit(0);
                } else {
                    perror("getline");
                    exit(0);
                }
            }
            numTotStages = parseline(batchBuf, &stageList);
        }
    
        if (numTotStages >= 0)
            executeCmds(stageList, numTotStages);
        memset(buffer, 0, 513);
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
            if (chdir((stgAry[i]->argvPtr)[1]) < 0) {
                perror((stgAry[i]->argvPtr)[1]);
                return 1;
            }
        } else {
            numOfChild += 1;

            if ((children[i] = fork())<0)
                triggerError("fork", 0);

            if (children[i] == 0)  {
                
                signal(SIGINT, SIG_DFL);
                /* Child stuff */
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
                    dup2(redirIn, STDIN_FILENO);
                    if (numPipes > 0) {
                        dup2(npipe[i][1], STDOUT_FILENO);
                    }
                } else if (i == numPipes) {
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

                execvp(stgAry[i]->argvPtr[0], stgAry[i]->argvPtr);
                perror("execvp");
                exit(EXIT_FAILURE);
            }
        }
    }

    for (i=0; i<numPipes; i++) {
        close(npipe[i][0]);
        close(npipe[i][1]);
    }

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

    struct sigaction sa;
    sa.sa_handler = ctrlCHandler;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGINT);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) < 0)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    return 0;
}

void ctrlCHandler(int signum) {

    int status, i;

    for (i = 0; i<MAX_PIPE; i++) {
        if (children[i] != 0) {
            if (waitpid(children[i], &status, 0) < 0) {
                if (errno != EINTR) {
                    perror("Handler wait");
                    exit(EXIT_FAILURE);
                }
            }
            children[i] = 0;
        } else {
            break;
        }
    }

    printf("\n");
}
