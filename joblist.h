#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#define EXIT 0
#define RUNNING 1
#define STOPPED 2
#define KILLED 3

extern char *status_str[];

struct joblist{ // Double linked list used for storing jobs. O(1) in adding and listing
    struct joblist *prev;
    int jobnumber;
    char *name;
    int pid;
    int status;
    int retval;
    char stdout[1000];
    char stderr[1000];
    struct rusage usage;
    struct joblist *next;
};

struct joblist *initList();
struct joblist *addToList(struct joblist *head, char *name, int pid, int status);
int removeFromList(struct joblist *head, int pid);
struct joblist *getJob(struct joblist *head, int job_id);
void freeList(struct joblist *head);
char *formatList(struct joblist *node);
char *formatResource(struct joblist *node);

int prlimit(pid_t pid, int resource, const struct rlimit *new_limit,struct rlimit *old_limit);

int MAX_JOB_NUM;    // Maximum job number, default is 10
volatile int cur_job_num; // Volatile to avoid race condition