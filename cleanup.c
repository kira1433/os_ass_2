#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>

#define PERMS 0644
#define SHM_PERMS 0666
#define SHM_SIZE 1024
#define N 30

struct msg_buf{
    long mtype;
    int seq;
    int op;
    char fname[200];
};

struct rtn_buf{
    long mtype;
    char msg[200];
    int len;
    int trav[N];
};

struct shm_buf{
    int n;
    int adj[N][N];
};

int main(){
    key_t msgq_key = ftok("load_balancer.c", 'B');
    int msgq_id = msgget(msgq_key, PERMS);
    if(msgq_id == -1){
        perror("msgget");
        return -1;
    }

    while(1){
        char choice;
        printf("Want to terminate the application? Press Y (Yes) or N (No) ");
        scanf(" %c", &choice);
        if(choice == 'Y'){
            struct msg_buf msg;
            msg.mtype = 1;
            msg.seq = 0;
            msg.op = 0;
            msgsnd(msgq_id, &msg, sizeof(msg) - sizeof(long), 0);
            break;
        }
    }
    printf("Sent termination request to load balancer\n");
    return 0;
}