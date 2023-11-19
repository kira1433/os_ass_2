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

struct graph{
    int n;
    int** adj;
    int st;
    struct queue* q;
};

int msgq_id;

struct queue {
    int arr[N];
    int front, back;
    pthread_mutex_t lock;
};
void init(struct queue *q) {
    q->front = -1;
    q->back = -1;
    if (pthread_mutex_init(&q->lock, NULL) != 0) {
        perror("mutex initialization");
        exit(-1);
    }
}
int len(struct queue *q) {
    pthread_mutex_lock(&q->lock);
    int length = (q->back - q->front) + 1;
    if(q->front == -1) length = 0;
    pthread_mutex_unlock(&q->lock);
    return length;
}
void enqueue(struct queue *q, int x) {
    pthread_mutex_lock(&q->lock);
    if(q->front == -1)q->front = 0;
    q->back++;
    q->arr[q->back] = x;
    pthread_mutex_unlock(&q->lock);
}
int dequeue(struct queue *q) {
    pthread_mutex_lock(&q->lock);
    int x = q->arr[q->front];
    if(q->front >= q->back){
        q->front = -1;
        q->back = -1;
    }else{
        q->front++;
    }
    pthread_mutex_unlock(&q->lock);
    return x;
}

void* dfs(void* arg){
    struct graph* graph = (struct graph*)arg;
    pthread_t thread_ids[graph->n];
    int flag = 1;
    for(int i=0;i<graph->n;i++){
        thread_ids[i] = -1;
        if(graph->adj[graph->st][i] == 1){
            graph->adj[graph->st][i] = 0;
            graph->adj[i][graph->st] = 0;
            struct graph* new_graph = (struct graph*)malloc(sizeof(struct graph));
            *new_graph = *graph;
            new_graph->st = i;
            flag = 0;
            if (pthread_create(&thread_ids[i], NULL, dfs, (void *)new_graph) != 0)
            {
                perror("pthread_create");
            }
        }
    }
    for(int i=0;i<graph->n;i++){
        if(thread_ids[i] != -1){
            pthread_join(thread_ids[i], NULL);
        }
    }
    if(flag)enqueue(graph->q, graph->st+1);
    free(graph);
    return NULL;
}

void* process_node(void* arg){
    struct graph* graph = (struct graph*)arg;
    for(int i=0;i<graph->n;i++){
        if(graph->adj[graph->st][i] == 1){
            graph->adj[graph->st][i] = 0;
            graph->adj[i][graph->st] = 0;
            enqueue(graph->q, i);
        }
    }
    free(graph);
    return NULL;
}

struct queue*  bfs(struct graph* graph){
    struct queue* q = (struct queue*)malloc(sizeof(struct queue));
    init(q);
    enqueue(graph->q, graph->st);
    while(len(graph->q)){
        int n = len(graph->q);
        pthread_t thread_ids[n];
        for(int i=0;i<n;i++){
            int x = dequeue(graph->q);
            enqueue(q, x+1);
            struct graph* new_graph = (struct graph*)malloc(sizeof(struct graph));
            *new_graph = *graph;
            new_graph->st = x;
            if(pthread_create(&thread_ids[i], NULL, process_node , (void *)new_graph) != 0){
                perror("pthread_create");
            }    
        }
        for(int i=0;i<n;i++){
            pthread_join(thread_ids[i], NULL);
        }
    }
    pthread_mutex_destroy(&graph->q->lock);
    free(graph->q);
    free(graph);
    return q;
}

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
    struct graph* graph = (struct graph*)malloc(sizeof(struct graph));
    graph->st = shm_ptr->n - 1;
    graph->adj = (int**)malloc(sizeof(int*)*N);
    graph->q = (struct queue*)malloc(sizeof(struct queue));
    init(graph->q);
    struct queue* q = graph->q;
    int** adj = graph->adj;
    for(int i=0;i<N;i++){
        graph->adj[i] = (int*)malloc(sizeof(int)*N);
    }

    int file_desc = open(msg->fname, O_RDONLY | O_CREAT , PERMS);
    if(flock(file_desc,LOCK_SH) == -1){ // SHARED LOCK FOR READING
        perror("flock"); 
        pthread_exit(NULL);
    }

    FILE *file = fdopen(file_desc,"r");
    if (file == NULL)
    {
        perror("fopen");
        pthread_exit(NULL);
    }
    fscanf(file, "%d", &graph->n);
    for (int i = 0; i < graph->n ; i++)
    {
        for (int j = 0; j < graph->n ; j++)
        {
            fscanf(file, "%d ", &graph->adj[i][j]);
        }
    }
    fclose(file);
    flock(file_desc,LOCK_UN);
    close(file_desc);

    if(msg->op == 4)q = bfs(graph);
    else{
        pthread_t thread_id;
        if(pthread_create(&thread_id, NULL, dfs , (void *)graph) != 0){
            perror("pthread_create");
            pthread_exit(NULL);
        }
        pthread_join(thread_id, NULL);
    }
    struct rtn_buf* rtn = (struct rtn_buf*)malloc(sizeof(struct rtn_buf));
    rtn->len = 0;
    while(len(q)){
        rtn->trav[rtn->len++] = dequeue(q);
    }
    if (msg->op==3) strcpy(rtn->msg,"DFS traversal(Leaves):");
    else strcpy(rtn->msg,"BFS traversal:");
    rtn->mtype=msg->seq + 100;
    if (msgsnd(msgq_id, rtn, sizeof(struct rtn_buf) - sizeof(long), 0) == -1)
    {
        perror("msgsnd");
    }

    printf("Served request: Sequence Number %d, Operation Number %d, File Name %s\n", msg->seq, msg->op, msg->fname);
    for(int i=0;i<N;i++){
        free(adj[i]);
    }
    free(adj);
    pthread_mutex_destroy(&q->lock);
    free(q);
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

    sem_t *sem = sem_open("sem", O_EXCL, SHM_PERMS, 2);
    if(sem == SEM_FAILED){
        perror("sem_open");
        return -1;
    }
    if(sem_wait(sem) == -1){
        perror("sem_wait");
        return -1;
    }
    int sem_value;
    if(sem_getvalue(sem, &sem_value) == -1){
        perror("sem_getvalue");
        return -1;
    }
    sem_value = 2-sem_value;
    printf("Secondary Server %d started.\n", sem_value);
    
    pthread_t thread_ids[100];
    int index = 0;

    while(1){   
        struct msg_buf* msg = (struct msg_buf*)malloc(sizeof(struct msg_buf));
        if(msgrcv(msgq_id, msg, sizeof(struct msg_buf) - sizeof(long), 2 + sem_value, 0) == -1){
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

    if(sem_post(sem) == -1){
        perror("sem_post");
        return -1;
    }
    if(sem_close(sem) == -1){
        perror("sem_close");
        return -1;
    }
    return 0;
}