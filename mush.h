#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

/* Define maximum sizes */
#define MAX_BUFFER 512
#define MAX_ARG 512
#define MAX_CMD 16
#define MAX_PIPE 16

/* Define a Stage node for a linked list */
typedef struct stage Stage;
struct stage
{
    char fullCmd[MAX_BUFFER + 1];
    char input[MAX_BUFFER + 1];
    char output[MAX_BUFFER + 1];
    int curStage;
    int argc;
    char *argvPtr[MAX_ARG];
    struct stage *next;
    struct stage *prev;
};

/* mush.c Functional Declaration */
int installSignals();
void ctrlCHandler(int signum);
int executeCmds(Stage *stageList, int numTotStages);

/* parseline.c Function Declaration */
void triggerError(const char *errSrc, int mode);
int parseline(char *input, Stage **stgBuf);
int strToArray(char *myArgv[], char *buf);
int parseArgs(char *myArgv[], int myArgc, Stage **stageList);
int isValidStage(Stage *s);
int strCmpSpecialChar(char *str);
int printStages(Stage *stages);
int containsNonSpace(char *str);
char *trimwhitespace(char *str);
