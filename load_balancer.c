#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <semaphore.h>
#include <fcntl.h>

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
    int msgq_id = msgget(msgq_key, PERMS | IPC_CREAT); 
    if(msgq_id == -1){
        perror("msgget");
        return -1;
    }
    sem_t *sem = sem_open("sem", O_CREAT, SHM_PERMS, 2);
    if(sem == SEM_FAILED){
        perror("sem_open");
        return -1;
    }
    printf("Load balancer started.\n");

    while(1){
        struct msg_buf msg;
        if(msgrcv(msgq_id, &msg, sizeof(msg) - sizeof(long), 1, 0) == -1){
            perror("msgrcv");
            continue;
        }
        
        if(msg.op == 0){
            printf("Received exit request\n");
            msg.mtype = 2;
            if(msgsnd(msgq_id, &msg, sizeof(msg) - sizeof(long), 0) == -1){
                perror("msgsnd");
            }
            msg.mtype = 3;
            if(msgsnd(msgq_id, &msg, sizeof(msg) - sizeof(long), 0) == -1){
                perror("msgsnd");
            }
            msg.mtype = 4;
            if(msgsnd(msgq_id, &msg, sizeof(msg) - sizeof(long), 0) == -1){
                perror("msgsnd");
            }
            sleep(5);
            printf("Exiting...\n");
            break;
        }
        
        printf("Received request: Sequence Number %d, Operation Number %d, File Name %s\n", msg.seq, msg.op, msg.fname);
        if(msg.op == 1 || msg.op == 2){
            msg.mtype = 2;
            printf("Sent request to Primary Server\n");    
        }
        else if(msg.op == 3 || msg.op == 4){
            if(msg.seq % 2){
                msg.mtype = 3;
                printf("Sent request to Secondary Server 1\n");    
            }
            else{
                msg.mtype = 4;
                printf("Sent request to Secondary Server 2\n");    
            }
        }
        if(msgsnd(msgq_id, &msg, sizeof(msg) - sizeof(long), 0) == -1){
            perror("msgsnd");
            continue;
        }
    }
    
    if(msgctl(msgq_id, IPC_RMID, NULL) == -1){
        perror("msgctl");
        return -1;
    }
    if(sem_unlink("sem") == -1){
        perror("sem_unlink");
        return -1;
    }
    return 0;
}