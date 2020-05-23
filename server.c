#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "joblist.h"
#include "message.h"

#define SOCK_PATH "socket_pipe"

#define DEBUG_MSG(x,...) if(dflag){\
                            fprintf(stderr,x,##__VA_ARGS__);\
                        }
#define ERROR_MSG(x) fprintf(stderr,"%s error in %s: %s() line: %d\n",x,__FILE__,__func__,__LINE__);
#define USAGE() printf("Usage: ./server -[idh] -[cjm] [optarg]\n\
-i  Interactive mode\n\
    Can enter commands runnable by execvp\n\
    Special commands:\n\
        'exit' exit the server\n\
        'list' list all jobs\n\
        'resource' list resource of all jobs\n\
        'clearlist' clear exited or killed jobs\n\
-j [Max_Job_Number] \n\
    Allow Max_Job_Number of jobs running at the same time\n\
    Exited and Killed jobs are still considered running if they are not cleared\n\
    Therefore, to add new job. Client has to clear job list with action_code 2\n\
    The threshhold of Max_Job_Number is 10. Anything above 10 will be set to 10\n\
-m [mem_limit]\n\
    Specify mem_limit for each job\n\
-c [cpu_limit]\n\
    Specify cpu_limit for each job\n\
-h Print Usage\n\
-d Print Debug Message\n");

int dflag = 0;
int iflag = 0;

long cpu_limit = RLIM_INFINITY;
long mem_limit = RLIM_INFINITY;

struct joblist *head;

void *handleClient(void *arg);
void *handleUser(void *arg);

int main(int argc, char **argv, char **envp)
{   
    MAX_JOB_NUM = 10; // Default value and maximum number of allowed connection
    cur_job_num = 0;

    int opt;
	while((opt=getopt(argc,argv,"ihdm:c:j:"))!=-1){
		switch(opt){
            case 'i':
                iflag = 1;
                break;
			case 'd':
				dflag = 1;
				break;
            case 'm':
                mem_limit = atoi(optarg);
                break;
            case 'c':
                cpu_limit = atoi(optarg);
                break;
            case 'j':
                MAX_JOB_NUM = atoi(optarg);
                if(MAX_JOB_NUM > 10){
                    MAX_JOB_NUM = 10;
                }
                break;
            case 'h':
                USAGE()
                exit(0);
                break;
			case '?':
                USAGE()
                exit(1);
				break;
		}
	}

    head = initList();  // Creates joblist head to store job

    int sock, fd, len;
    socklen_t remote_len;
    struct sockaddr_un local, remote;

    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        ERROR_MSG("socket")
        exit(1);
    }

    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, SOCK_PATH);
    len = strlen(local.sun_path) + sizeof(local.sun_family);
    if(unlink(local.sun_path)==-1){
        DEBUG_MSG("No such pipe. Creating new one\n");
    }
    if (bind(sock, (struct sockaddr *)&local, len) == -1) { // Binding socket file to the fd
        ERROR_MSG("bind")
        exit(1);
    }

    if (listen(sock, 5) == -1) {
        ERROR_MSG("listen")
        exit(1);
    }

    if(iflag){// If interactive mode is turned on, spawn a separate thread to handle stdin
        pthread_t main_tid;
        if(pthread_create(&main_tid,NULL,handleUser,NULL)!=0){  
            ERROR_MSG("pthread_create")
            exit(1);
        }
        if(pthread_detach(main_tid)!=0){
            ERROR_MSG("pthread_detach")
            exit(1);
        }
    }

    while(1) {  // Looping and listening for connections
        DEBUG_MSG("Waiting for a connection...\n")
        if ((fd = accept(sock, (struct sockaddr *)&remote, &remote_len)) == -1) {
            ERROR_MSG("accept")
            exit(1);
        }

        pthread_t tid;
        void *args[] = {&fd};
        if(pthread_create(&tid,NULL,handleClient,args)!=0){  // Spawn a separate thread to handle request
            ERROR_MSG("pthread_create")
            exit(1);
        }
        if(pthread_detach(tid)!=0){
            ERROR_MSG("pthread_detach")
            exit(1);
        }
        DEBUG_MSG("Connected. Client tid=%lx, fd=%d\n",tid,fd)
    }

    freeList(head); // Cleanup list
    return 0;
}

void *handleClient(void *arg){
    void **args = arg;
    int fd  = *((int*)args[0]);
    
    char buf[PACKET_SIZE];
    int n;
    n = recv(fd, buf, PACKET_SIZE, 0); // Receive the packet from client
    if (n <= 0) {
        if (n < 0) {
            ERROR_MSG("recv")
            exit(1);
        }
        else{
            fprintf(stderr,"Recv Nothing\n");
            return NULL;
        }
    }

    struct request r;
    struct request *tr = getRequest(buf);   // Unpack received packet
    memcpy(&r,tr,sizeof(struct request));
    free(tr);

    struct joblist *job = getJob(head,r.job_id); // If job_id is provided, find corresponding job node in the list

    if(r.action_code == SUBMIT){ // If client wants to submit a new job
        if(cur_job_num == MAX_JOB_NUM){ // Check whether joblist is full
            if(send(fd,"Job is full\n",12,0)<0){
                ERROR_MSG("send")
                exit(1);
            }
            DEBUG_MSG("Connection closed due to full joblist.\n")
            if(close(fd)<0){
                ERROR_MSG("close")
            }
            return NULL;
        }

        if(send(fd,"Submit Success\n",strlen("Submit Success\n"),0)<0){ // Notify client that server received the request
            ERROR_MSG("send")
            exit(1);
        }

        int stdout_fds[2]; // For storing stdout of child process
        int stderr_fds[2]; // same as above, for stderr
        if(pipe(stdout_fds)<0){
            ERROR_MSG("pipe")
            exit(1);
        }
        if(pipe(stderr_fds)<0){
            ERROR_MSG("pipe")
            exit(1);
        }

        char *arg_buf[100] = {};
        int i = 0;
        char *b = strtok(r.command," ");

        while(b){
            arg_buf[i] = b;
            i++;
            b = strtok(NULL," ");
        }
        if(i<100){
            arg_buf[i] = NULL;
        }

        int pid;
        if((pid=fork())==0){ // Forking child to run command
            if(close(stdout_fds[0])<0){
                ERROR_MSG("close")
            }
            if(close(stderr_fds[0])<0){
                ERROR_MSG("close")
            }
            if(dup2(stdout_fds[1],fileno(stdout))<0){
                ERROR_MSG("dup2")
                exit(1);
            }
            if(dup2(stderr_fds[1],fileno(stderr))<0){
                ERROR_MSG("dup2")
                exit(1);
            }

            struct rlimit climit,mlimit; // Set resource limit
            climit.rlim_cur = cpu_limit;
            climit.rlim_max = -1;
            mlimit.rlim_cur = mem_limit;
            mlimit.rlim_max = -1;
            if(setrlimit(RLIMIT_CPU,&climit)==-1){
                ERROR_MSG("setrlimit")
                exit(1);
            }
            if(setrlimit(RLIMIT_AS,&mlimit)==-1){
                ERROR_MSG("setrlimit")
                exit(1);
            }

            if(execvp(arg_buf[0],arg_buf)==-1){
                fprintf(stderr,"%s: command not found",arg_buf[0]);
                exit(127);
            }
        }
        else if(pid < 0){
            ERROR_MSG("fork")
            exit(1);
        }
        else{ // Parent process
            struct joblist *node = addToList(head,arg_buf[0],pid,RUNNING); //Add job to the list
            int status;
            struct rusage usage;
            do{                     // Loop till child to either exit or be killed
                if(wait4(pid,&status,WUNTRACED|WCONTINUED,&usage)==-1){
                    ERROR_MSG("wait4")
                    exit(1);
                }
                if(WIFSTOPPED(status)){
                    DEBUG_MSG("Process %d stopped\n",pid)
                    node->status = STOPPED;
                    memcpy(&node->usage,&usage,sizeof(node->usage));
                }
                if(WIFCONTINUED(status)){
                    DEBUG_MSG("Process %d continued\n",pid)
                    node->status = RUNNING;
                    memcpy(&node->usage,&usage,sizeof(node->usage));
                }
            }while(!WIFSIGNALED(status) && !WIFEXITED(status));
            memcpy(&node->usage,&usage,sizeof(node->usage));
            if(WIFEXITED(status)){
                DEBUG_MSG("Process %d exited with status %d\n",pid,WEXITSTATUS(status))
                node->status = EXIT;
                node->retval = WEXITSTATUS(status);

                char outbuf[1000] = {};
                char errbuf[1000] = {};
                if(fcntl(stdout_fds[0],F_SETFL,O_NONBLOCK)==-1){
                    ERROR_MSG("fcntl")
                    exit(1);
                }
                if(fcntl(stderr_fds[0],F_SETFL,O_NONBLOCK)==-1){
                    ERROR_MSG("fcntl")
                    exit(1);
                }

                int ret2;
                if((ret2=read(stdout_fds[0],outbuf,1000))>0){
                    memcpy(node->stdout,outbuf,1000);
                }
                else{
                    DEBUG_MSG("No stdout output\n")
                }
                if((ret2=read(stderr_fds[0],errbuf,1000)>0)){
                    memcpy(node->stderr,errbuf,1000);
                }
                else{
                    DEBUG_MSG("No stderr output\n")
                }
            }
            else if(WIFSIGNALED(status)){
                DEBUG_MSG("Process %d killed with signal %d\n",pid,WTERMSIG(status))
                node->status = KILLED;
                node->retval = WTERMSIG(status) + 128;
            }
            if(close(stdout_fds[0])==-1){
                ERROR_MSG("close")
            }
            if(close(stdout_fds[1])==-1){
                ERROR_MSG("close")
            }
            if(close(stderr_fds[0])==-1){
                ERROR_MSG("close")
            }
            if(close(stderr_fds[1])==-1){
                ERROR_MSG("close")
            }
        }
    }
    else if(r.action_code == DELETE){ // Client wants to clear a job or entire list
        int ret = removeFromList(head,r.job_id);
        if(ret == 0){
            if(send(fd,"Delete Success\n",strlen("Delete Success\n"),0)==-1){
                ERROR_MSG("send")
                exit(1);
            }
        }
        else{
            if(send(fd,"Delete Failed\n",strlen("Delete Failed\n"),0)==-1){
                ERROR_MSG("send")
                exit(1);
            }
        }
    }
    else if(r.action_code == LIST){ // Client wants to list the jobs
        char buf[1000] = {};
        char *out = formatList(job);
        sprintf(buf,"%s",out);
        if(send(fd,buf,strlen(buf),0)==-1){
            ERROR_MSG("send")
            exit(1);
        }
        free(out);
    }
    else if(r.action_code == GETSTDOUT){ // Client wants stdout of a job
        char buf[1000] = {};
        sprintf(buf,"%s",job->stdout);
        if(send(fd,buf,1000,0)==-1){
            ERROR_MSG("send")
            exit(1);
        }
    }
    else if(r.action_code == GETSTDERR){// Client wants stderr of a job
        char buf[1000] = {};
        sprintf(buf,"%s",job->stderr);
        if(send(fd,buf,1000,0)==-1){
            ERROR_MSG("send")
            exit(1);
        }
    }
    else if(r.action_code == GETRESRC){// Client wants to list resources of jobs
        char buf[1000] = {};
        if(job != head){
            if(job->status == RUNNING){    // For running process, pause to get resources and resume
                DEBUG_MSG("Sending sigstop to %d\n",job->pid)
                if(kill(job->pid,SIGSTOP)==-1){
                    ERROR_MSG("kill")
                }
                if(kill(job->pid,SIGCONT)==-1){
                    ERROR_MSG("kill")
                }
            }
        }
        else{
            struct joblist *temp = head->next;
            while(temp != head){
                if(temp->status == RUNNING){
                    DEBUG_MSG("Sending sigstop to %d\n",temp->pid)
                    if(kill(temp->pid,SIGSTOP)==-1){
                        ERROR_MSG("kill")
                    }
                    if(kill(temp->pid,SIGCONT)==-1){
                        ERROR_MSG("kill")
                    }
                }
                temp = temp->next;
            }
        }
        char *out2 = formatResource(job);
        sprintf(buf,"%s",out2);
        if(send(fd,buf,strlen(buf),0)==-1){
            ERROR_MSG("send")
            exit(1);
        }
        free(out2);
    }
    else{
        if(send(fd,"No Action\n",strlen("No Action\n"),0)==-1){
            ERROR_MSG("send")
            exit(1);
        }
    }

    DEBUG_MSG("Connection %lx closed.\n",pthread_self())
    if(close(fd)==-1){
        ERROR_MSG("close")
    }
    return NULL;
}

void *handleUser(void *arg){ // This is similar to HW3, an interactive shell that interacts with server side manager
    while(1){
        char buf[100];
        printf(">>");
        if(fgets(buf,100,stdin)==NULL){
            ERROR_MSG("fgets")
            exit(1);
        }
        
        if(strcmp(buf,"\n")==0){
			continue;
		}

        if(strtok(buf, "\n")==NULL){
            continue;
        } 

        char *arg_buf[50] = {};
        int i = 0;
        char *b = strtok(buf," ");

        while(b){
            arg_buf[i] = b;
            i++;
            b = strtok(NULL," ");
        }
        if(i<100){
            arg_buf[i] = NULL;
        }

        if(strcmp(arg_buf[0],"exit")==0){ // Special commands
            exit(0);
        }
        else if(strcmp(arg_buf[0],"list")==0){
            char *out3 = formatList(head);
            printf("%s\n",out3);
            free(out3);
        }
        else if(strcmp(arg_buf[0],"resource")==0){
            char *out4 = formatResource(head);
            printf("%s\n",out4);
            free(out4);
        }
        else if(strcmp(arg_buf[0],"clearlist")==0){
            if(removeFromList(head,0)==-1){
                DEBUG_MSG("Nothing to Clear\n")
            }
        }
        else{ // For non special commands, run with execvp
            int pid;
            if((pid=fork())==0){
                if(execvp(arg_buf[0],arg_buf)==-1){
                    fprintf(stderr,"%s: command not found",arg_buf[0]);
                    exit(127);
                }
            }
            else if(pid < 0){
                ERROR_MSG("fork")
                exit(1);
            }
            else{
                int status;
                if(waitpid(pid,&status,WUNTRACED)==-1){
                    ERROR_MSG("waitpid")
                    exit(1);
                }
            }
        }
    }
}