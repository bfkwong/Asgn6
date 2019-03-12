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
        printStages(stageList);
//        executeCmds(stageList, numTotStages);
        memset(buffer, 0, 512);
    }
    return 0;
}

int executeCmds(Stage *stageList, int numTotStages) {
    pid_t child; 
    Stage *curStg;
    int redirOut, redirIn, status, prevStgStdout, i;
    int npipe[numTotStages-1][2];
    int children[numTotStages];
    
    
    
    /* Create pipes */
    for (i = 0; i<numTotStages-1; i++) {
        /* If there is only 1 stage, no pipes created */
        if (pipe(npipe[i]) < 0)
            triggerError("pipe", 0);
    }
    
    /* Fork children */
    for (i=0; i<numTotStages; i++) {
        if ((children[i] = fork())<0)
            triggerError("fork", 0);
    }
    
    /* Do plumbing */
    
    
    for (curStg = stageList; curStg != NULL; curStg = curStg->next) {
        
        /* Set up output redirects */
        if (strcmp(curStg->output, "original stdout") != 0) {
            if (strncmp(curStg->output, "pipe to stage", 13) == 0) {
                if (pipe(npipe[i]) < 0)
                    triggerError("pipe", 0);
            } else if ((redirOut = open(curStg->output,
                                        O_RDWR|O_CREAT|O_TRUNC, 0644)) < 0) {
                triggerError("open", 0);
            }
        }
        
        if (strcmp((curStg->argvPtr)[0], "cd") == 0) {
            if (chdir((curStg->argvPtr)[1]) < 0)
                triggerError("chdir", 0);
        } else { /* Not change directory */
            if ((child = fork()) < 0)
                triggerError("fork", 0);
            if (child != 0) {
                /*parent*/
            } else { /*child*/
                if (strcmp(curStg->input, "original stdin")) {
                    if (strncmp(curStg->input, "pipe from stage", 15) == 0) {
                        dup2(npipe[i-1][0], STDIN_FILENO);
                    } else if ((redirIn = open(curStg->input, O_RDWR, 0644)) < 0)
                        triggerError("open", 0);
                }
                
                if (curStg->next != NULL) {
                    dup2(npipe[i][1], STDOUT_FILENO);
                    close(npipe[i][0]);
                }
                numOfChild += 1;
                execvp((curStg->argvPtr)[0], curStg->argvPtr);
                perror("execvp");
                exit(EXIT_FAILURE);
            }
        }
    
        for (i = 0; i<numOfChild; i++) {
            if (wait(&status) < 0)
                triggerError("wait", 0);
        }
        
        i += 1;
    }
    
    return 0;
}

int installSignals() {

    struct sigaction sa;
    sa.sa_handler = ctrlCHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) < 0)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    return 0;
}

void ctrlCHandler(int signum)
{
    printf("\n");
}
