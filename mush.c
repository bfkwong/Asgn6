#include "mush.h"

int numOfChild = 0;

int main(int argc, char *argv[])
{
    int numTotStages;
    char buffer[MAX_BUFFER + 1];
    Stage *stageList;

    memset(buffer, 0, 513);
    if (installSignals() < 0)
        triggerError("installSignal", 0);

    while (1) {
        write(STDOUT_FILENO, "8-P ", 4);
        read(STDIN_FILENO, buffer, 512);
        numTotStages = parseline(buffer, &stageList);
        if (numTotStages >= 0) {
            /* Parsing succeeded */
            executeCmds(stageList, numTotStages);
        }
        memset(buffer, 0, 513);
    }
    return 0;
}

int executeCmds(Stage *stageList, int numTotStages) {
    Stage *head = stageList;
    Stage *stgAry[MAX_PIPE];
    int redirOut, redirIn, status, j, i=0;
    int numPipes = numTotStages - 1;
    int npipe[numPipes][2];
    pid_t children[numTotStages];

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
            fprintf(stderr, "Added a child:\t\t%d\n", numOfChild);

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

    for (j=0; j<numPipes; j++) {
        close(npipe[j][0]);
        close(npipe[j][1]);
    }

    while(numOfChild != 0) {
        fprintf(stderr, "Parent start waiting:\t%d\n", numOfChild);
        if(wait(&status) < 0) {
            if (errno != EINTR) {
                perror("handler waiting");
                exit(EXIT_FAILURE);
            }
            numOfChild += 1; /*Counteract the subtract because
                              if it exited due to SIGINT we restart */
        }
        numOfChild -= 1;
        fprintf(stderr, "Parent finish waiting:\t%d\n", numOfChild);

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

    int status;

    while(numOfChild != 0) {
        
        fprintf(stderr, "Handler start waiting:\t%d\n", numOfChild);
        if(wait(&status) < 0) {
            if (errno != EINTR) {
                perror("handler waiting");
                exit(EXIT_FAILURE);
            }
            numOfChild += 1; /*Counteract the subtract because
                              if it exited due to SIGINT we restart */
        }
        numOfChild -= 1;
        fprintf(stderr, "Handler finish waiting:\t%d\n", numOfChild);
    }

    printf("\n");
}
