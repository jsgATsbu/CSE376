#include "message.h"

#define ERROR_MSG(x) fprintf(stderr,"%s error in %s: %s() line: %d\n",x,__FILE__,__func__,__LINE__);

// Client creates protocol packet with this method
char *createRequest(int action_code, int job_id, char *command){
    if(strlen(command)>MAX_COMMAND_LEN-1){
        perror("Command Longer than MAX_COMMAND_LEN(1000).");
        exit(1);
    }
    struct request r;
    r.action_code = action_code;
    r.job_id = job_id;
    memcpy(r.command,command,strlen(command));
    r.command[strlen(command)] = '\0';
    char *msg = calloc(1,sizeof(struct request));
    if(msg == NULL){
        ERROR_MSG("calloc")
        exit(1);
    }
    memcpy(msg,&r,sizeof(struct request));
    return msg;
}


// Server parse the packet with this method
struct request *getRequest(char *reqstr){
    struct request *r = calloc(1,sizeof(struct request));
    if(r == NULL){
        ERROR_MSG("calloc")
        exit(1);
    }
    memcpy(r,reqstr,sizeof(struct request));
    return r;
}

// int main(){
//     char *s = createRequest(1,2,"asd");
//     struct request *r = getRequest(s);
//     printf("%d\n",r->action_code);
//     printf("%d\n",r->job_id);
//     printf("%s\n",r->command);
//     return 0;
// }