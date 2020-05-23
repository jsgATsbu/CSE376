#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "message.h"

#define SOCK_PATH "socket_pipe"

#define USAGE() printf("Usage: ./client -a 1 -c \"command\" | -a [2-6] -[hd] -[j] [optarg]\n\
-a  [0-6]\n\
    0 No Action\n\
    1 Submit\n\
    2 Delete a job/jobs\n\
    3 List a job/jobs \n\
    4 Stdout of a job\n\
    5 Stderr of a job\n\
    6 Resource of a job/jobs \n\
-c \"command\" (Surround with \" \", e.g. \"echo hello\")\n\
    Must be used with -a 1\n\
-j [job number] \n\
    For action 2, 3, 4, 5 and 6 \n\
    For action 2, 3 and 6, if no job/ incorrect job number is provided,\n\
    all jobs will be printed by default. \n\
-h Print Usage                  \n\
-d Print Debug Message        \n");

#define DEBUG_MSG(x,...) if(dflag){\
                            fprintf(stderr,x,##__VA_ARGS__);\
                        }

#define ERROR_MSG(x) fprintf(stderr,"%s error in %s: %s() line: %d\n",x,__FILE__,__func__,__LINE__);

int dflag = 0; // Debug mode flag

int main(int argc, char **argv)
{
    char *command = "";           // Default value for protocol
    int action_code = NO_ACTION;  // Default value 
    int job_id = 0;               // Default value
    int opt;
	while((opt=getopt(argc,argv,"hdc:j:a:"))!=-1){
		switch(opt){
            case 'c':
                command = optarg;
                break;
            case 'j':
                job_id = atoi(optarg);
                break;
            case 'a':
                action_code = atoi(optarg);
                break;
			case 'd':
				dflag = 1;
				break;
            case 'h':
                USAGE()
                exit(0);
                break;
			case '?':
                USAGE()
                exit(0);
				break;
		}
	}

    if(action_code == SUBMIT && strlen(command)==0){  // -a 1 option must be followed by -c "command"
        fprintf(stderr,"Error: Please enter command.\n");
        exit(1);
    }

    int sock, ret, len;
    struct sockaddr_un server;
    char buf[PACKET_SIZE] = {};

    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        ERROR_MSG("socket")
        exit(1);
    }

    DEBUG_MSG("Connecting to Server...\n")

    server.sun_family = AF_UNIX;
    strcpy(server.sun_path, SOCK_PATH);
    len = strlen(server.sun_path) + sizeof(server.sun_family);
    if (connect(sock, (struct sockaddr *)&server, len) == -1) { // Notify Server that client is connecting
        ERROR_MSG("connect")
        exit(1);
    }

    DEBUG_MSG("Connection established.\n")

    char *str = createRequest(action_code,job_id,command); // Generate Protocol packet
    if (send(sock, str, PACKET_SIZE, 0) == -1) { // Send packet
        ERROR_MSG("send")
        exit(1);
    }
    free(str);

    if((ret=recv(sock,buf,PACKET_SIZE,0))<=0){ // Wait for server response
        if(ret == 0){
            DEBUG_MSG("Server closed connection.\n")
        }
        else{
            ERROR_MSG("recv")
        }
        exit(1);
    }
    else{
        printf("%s\n",buf);
    }

    DEBUG_MSG("Closing Connection.\n")
    if(close(sock)==-1){    // Close connection
        ERROR_MSG("close")
    }

    return 0;
}