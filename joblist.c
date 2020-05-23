#include "joblist.h"

#define ERROR_MSG(x) fprintf(stderr,"%s error in %s: %s() line: %d\n",x,__FILE__,__func__,__LINE__);

char *status_str[] = {"EXIT","RUNNING","STOPPED","KILLED"}; // For formatting output

// Creates head of the double linked list to store jobs
struct joblist *initList(){
    struct joblist *head = calloc(1,sizeof(struct joblist));
    if(head == NULL){
        ERROR_MSG("calloc")
        exit(1);
    }
    head->prev = head;
    head->next = head;
    return head;
}

// Add job to head->next
// Returns the added job for other processing
struct joblist *addToList(struct joblist *head, char *name, int pid, int status){
    struct joblist *node = calloc(1,sizeof(struct joblist));
    if(node == NULL){
        ERROR_MSG("calloc")
        exit(1);
    }
    node->jobnumber=head->next->jobnumber+1;
    node->name = calloc(1,strlen(name)+1);
    if(node->name == NULL){
        ERROR_MSG("calloc")
        exit(1);
    }
    strncpy(node->name,name,strlen(name));
    node->pid = pid;
    node->status = status;
    
    node->prev = head;
    node->next = head->next;
    head->next->prev = node;
    head->next = node;
    cur_job_num++;
    return node;
}

// Remove job from list
int removeFromList(struct joblist *head, int job_id){
    if(cur_job_num == 0){
        return -1;
    }
    struct joblist *temp = head->next;
    if(job_id == 0){
        while(temp != head){
            if(temp->status == EXIT || temp->status == KILLED){
                temp->next->prev = temp->prev;
                temp->prev->next = temp->next;
                free(temp->name);
                free(temp);
                cur_job_num--;
            }
            temp = temp->next;
        }
        return 0;
    }
    while(temp != head){
        if(temp->jobnumber == job_id){
            if(temp->status == RUNNING || temp->status == STOPPED){
                return -1;
            }
            temp->next->prev = temp->prev;
            temp->prev->next = temp->next;
            free(temp->name);
            free(temp);
            cur_job_num--;
            return 0;
        }
        temp = temp->next;
    }
    return -1;
}

// Get job from list
struct joblist *getJob(struct joblist *head, int job_id){
    struct joblist *temp = head->next;
    while(temp != head){
        if(temp->jobnumber == job_id)
            return temp;
        temp = temp->next;
    }
    return head;
}

// Clean up the entire list and free memory
void freeList(struct joblist *head){
    struct joblist *temp = head->next;
    while(temp != head){
        struct joblist *next = temp->next;
        temp->next->prev = temp->prev;
        temp->prev->next = temp->next;
        free(temp->name);
        free(temp);
        temp = next;
    }
    free(head);
}

// Format the joblist in term of running status, return status
char *formatList(struct joblist *node){
    struct joblist *temp = node->prev;
    char *buf = calloc(1,700);
    if(buf == NULL){
        ERROR_MSG("calloc")
        exit(1);
    }
    sprintf(buf,"|%-10s |%-10s |%-10s |%-10s |%-10s |\n","Jobnumber","Process","Pid","Status","ExitCode");
    if(node->jobnumber != 0){
        if(node->status != EXIT && node->status != KILLED){
            sprintf(buf+strlen(buf),"|%-10d |%-10s |%-10d |%-10s |%-10s |\n",node->jobnumber,node->name,node->pid,status_str[node->status],"");
        }
        else{
            sprintf(buf+strlen(buf),"|%-10d |%-10s |%-10d |%-10s |%-10d |\n",node->jobnumber,node->name,node->pid,status_str[node->status],node->retval);
        }
    }
    else{
        int i = 0;
        while(temp != node && i < 10){
            if(temp->status != EXIT && temp->status != KILLED){
                sprintf(buf+strlen(buf),"|%-10d |%-10s |%-10d |%-10s |%-10s |\n",temp->jobnumber,temp->name,temp->pid,status_str[temp->status],"");
            }
            else{
                sprintf(buf+strlen(buf),"|%-10d |%-10s |%-10d |%-10s |%-10d |\n",temp->jobnumber,temp->name,temp->pid,status_str[temp->status],temp->retval);
            }
            i++;
            temp = temp->prev;
        }
    }
    return buf;
}

// Format joblist in term of resources such as cpu and memory
char *formatResource(struct joblist *node){
    char *buf = calloc(1,900);
    if(buf == NULL){
        ERROR_MSG("calloc")
        exit(1);
    }
    sprintf(buf,"|%-15s |%-15s |%-15s |%-15s |\n","Job id","User CPU Time","Sys. CPU Time","MaxRSS");
    if(node->jobnumber != 0){
        sprintf(buf+strlen(buf),"|%-15d |%-15ld |%-15ld |%-15ld |\n",node->jobnumber,node->usage.ru_utime.tv_usec,node->usage.ru_stime.tv_usec,node->usage.ru_maxrss);
    }
    else{
        struct joblist *temp = node->prev;
        int i = 0;
        while(temp != node && i < 10){
            sprintf(buf+strlen(buf),"|%-15d |%-15ld |%-15ld |%-15ld |\n",temp->jobnumber,temp->usage.ru_utime.tv_usec,temp->usage.ru_stime.tv_usec,temp->usage.ru_maxrss);
            i++;
            temp = temp->prev;
        }
    }
    return buf;
}