#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/file.h>
#include <pthread.h>
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

int msgq_id;

void* request_handler(void* arg){
    struct msg_buf* msg = (struct msg_buf*)arg;
    key_t shm_key = ftok("client.c", msg->seq);
    int shm_id = shmget(shm_key, SHM_SIZE, SHM_PERMS);
    if(shm_id == -1){
        perror("shmget");
        pthread_exit(NULL);
    }

    struct shm_buf* shm_ptr = (struct shm_buf*)shmat(shm_id, NULL, 0);
    if(shm_ptr == (void*)-1){
        perror("shmat");
        pthread_exit(NULL);
    }

    int file_desc = open(msg->fname, O_WRONLY | O_CREAT | O_TRUNC , PERMS);
    if(flock(file_desc,LOCK_EX) == -1){ // EXCLUSIVE LOCK FOR WRITING
        perror("flock");
        pthread_exit(NULL);
    }

    FILE* file = fdopen(file_desc, "w");
    if(file == NULL){
        perror("fopen");
        pthread_exit(NULL);
    }
    fprintf(file, "%d\n", shm_ptr->n);
    for(int i = 0; i < shm_ptr->n; i++){
        for(int j = 0; j < shm_ptr->n; j++){
            fprintf(file, "%d ", shm_ptr->adj[i][j]);
        }
        fprintf(file, "\n");
    }
    fclose(file);
    flock(file_desc,LOCK_UN);
    close(file_desc);

    struct rtn_buf rtn;
    if (msg->op==1) strcpy(rtn.msg,"File successfully added");
    else strcpy(rtn.msg,"File successfully modified");
    
    rtn.mtype=msg->seq + 100;
    rtn.len = 0;
    if (msgsnd(msgq_id, &rtn, sizeof(rtn) - sizeof(long), 0) == -1)
    {
        perror("msgsnd");
    }

    printf("Served request: Sequence Number %d, Operation Number %d, File Name %s\n", msg->seq, msg->op, msg->fname);
    shmdt(shm_ptr);
    free(msg);
    pthread_exit(NULL);
}

int main(){
    key_t msgq_key = ftok("load_balancer.c", 'B');
    msgq_id = msgget(msgq_key, PERMS); 
    if(msgq_id == -1){
        perror("msgget");
        return -1;
    }
    printf("Primary Server started.\n");
    
    pthread_t thread_ids[100];
    int index = 0;

    while(1){   
        struct msg_buf* msg = (struct msg_buf*)malloc(sizeof(struct msg_buf));
        if(msgrcv(msgq_id, msg, sizeof(struct msg_buf) - sizeof(long), 2, 0) == -1){
            perror("msgrcv");
            continue;
        }
        
        if(msg->op == 0){
            printf("Exiting...\n");
            free(msg);
            break;
        }

        if (pthread_create(&thread_ids[index], NULL, request_handler, (void*)msg) != 0) {
            perror("pthread_create");
            continue;
        }
        index++;
    }

    for (int i = 0; i < index; i++) {
        pthread_join(thread_ids[i], NULL);
    }

    return 0;
}