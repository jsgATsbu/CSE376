#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NO_ACTION 0
#define SUBMIT 1
#define DELETE 2
#define LIST 3
#define GETSTDOUT 4
#define GETSTDERR 5
#define GETRESRC 6

#define MAX_COMMAND_LEN 1000

struct request{ // Self-defined protocol struct for client-server communication
    int action_code;
    int job_id;
    char command[MAX_COMMAND_LEN];
};

#define PACKET_SIZE sizeof(struct request)

char *createRequest(int action_code, int job_id, char *command);
struct request *getRequest(char *reqstr);
