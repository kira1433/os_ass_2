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
    printf("\n1. Add a new graph to the database\
            \n2. Modify an existing graph of the database\
            \n3. Perform DFS on an existing graph of the database\
            \n4. Perform BFS on an existing graph of the database\n");

    while(1){
        printf("\nEnter Sequence Number ");
        int seq;
        scanf("%d", &seq);
        printf("Enter Operation Number ");
        int op;
        scanf("%d", &op);
        printf("Enter Graph File Name ");
        char fname[200];
        scanf("%s", fname);
        printf("\n");
        
        key_t shm_key = ftok("client.c", seq);
        int shm_id = shmget(shm_key, SHM_SIZE, SHM_PERMS | IPC_CREAT);
        if(shm_id == -1){
            perror("shmget");
            return -1;
        }
        struct shm_buf *shm_ptr = (struct shm_buf *)shmat(shm_id, NULL, 0);
        if(shm_ptr == (void*)-1){
            perror("shmat");
            return -1;
        }
        struct msg_buf msg;

        msg.mtype = 1;
        msg.seq = seq;
        msg.op = op;
        strcpy(msg.fname, fname);

        if(op == 1 || op == 2){
            printf("Enter number of nodes of the graph ");
            scanf("%d", &shm_ptr->n);
            printf("Enter adjacency matrix, each row on a separate line and elements of a single row separated by whitespace characters\n");
            for(int i = 0; i < shm_ptr->n; i++){
                for(int j = 0; j < shm_ptr->n; j++){
                    scanf("%d", &shm_ptr->adj[i][j]);
                }
            }
        }
        else{
            printf("Enter starting vertex ");
            scanf("%d", &shm_ptr->n);
        }
        msgsnd(msgq_id, &msg, sizeof(msg) - sizeof(long), 0);
        
        struct rtn_buf rtn;
        int break_condition = 0;
        if(msgrcv(msgq_id, &rtn, sizeof(rtn) - sizeof(long),100 + seq, 0)== -1){
            perror("msgrcv");
            break_condition = 1;
        }
        printf("%s\n", rtn.msg);
        for(int i = 0; i < rtn.len; i++){
            printf("%d ", rtn.trav[i]);
        }
        if(rtn.len)printf("\n");

        if(shmdt(shm_ptr) == -1){
            perror("shmdt");
            return -1;
        }
        if(shmctl(shm_id, IPC_RMID, NULL) == -1){
            perror("shmctl");
            return -1;
        }
        
        if(break_condition)break;
    }
    return 0;
}