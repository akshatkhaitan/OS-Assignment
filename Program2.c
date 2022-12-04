#include<unistd.h>
#include<stdio.h>
#include<stdlib.h>
#include <string.h>
#include<pthread.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include<signal.h>
// shared memory data
#define SHM_KEY_1 0x1289
#define SHM_KEY_2 0x1306

#define NANO_SECONDS_PER_SECOND 1000000000
//shared memory for final matrices, second matrix as Transposed 
int *matrix1, *matrix2;
long long *matrix3;
pthread_mutex_t mutex;


typedef struct{
    int id;
    int tCount;
    int rows1;
    int cols1;
    int cols2;
    long long sum;
} ThreadData;

void* thread_operations(void* args)
{
    ThreadData* td = args;
    int r1 = td->rows1;
    int c1 = td->cols1;
    int c2 = td->cols2;
    int not = td->tCount;
    int rem = (r1 * c2) % not;  // nonequal share count
    int nop = (r1 * c2) / not;
    int start, end;
    
    if (td->id == 0) {
    start = (td->id) * nop;
    end = (nop * (td->id + 1)) + rem;
  }
  else {
    start = ((td->id) * nop) + rem;
    end = (nop * (td->id + 1)) + rem;
  }
        for (int i = start; i < end; i++) {
            int row = i / c2;
            int col = i % c2;
            for (int ctr1 = 0; ctr1 < c1 ;ctr1++) {
                    while(matrix1[(row * c1) + ctr1] == -99999 || matrix2[col * c1 + ctr1] == -99999);
                        td->sum += matrix1[(row * c1) + ctr1] * matrix2[(col * c1) + ctr1];
            }
            
            matrix3[i] = td->sum;
            td->sum = 0;
        }
}

void printMatrixFromMemory(int rows, int cols, int* matrix) {
    printf("\nMemory Matrix printing Start \n");
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            printf("%d ", matrix[(i * cols) + j]);
        }
        printf("\n");
    }
    printf("Memory Matrix printing End \n");
}


int main(int argc, char* argv[])
{
    pthread_mutex_init(&mutex, NULL);
    int i, j, k, threadsCount;
    char outFile[100], csvFile[100];
    //Taking commandline arguments into variables
    i = atoi(argv[1]);
    j = atoi(argv[2]);
    k = atoi(argv[3]);
    strcpy(outFile, argv[4]);
    strcpy(csvFile, argv[5]);
    threadsCount = atoi(argv[6]);
    int releaseSharedMemory = atoi(argv[7]);
    // printf("Read Arguments");
    //To calculate time taken for Processing
    struct timespec startRT, endRT,
        startPT, endPT,
        startTT, endTT;
    clock_gettime(CLOCK_REALTIME, &startRT);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &startPT);
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &startTT);
    
    matrix3 = (long long *)calloc((i * k), sizeof(long long));
    
    int shmId1 = shmget(SHM_KEY_1, (i * j) * sizeof(int), 0666 | IPC_CREAT);
    matrix1 = shmat(shmId1, NULL, 0);
    //Share memory allocation for matrix2
    int shmId2 = shmget(SHM_KEY_2, (j * k) * sizeof(int), 0666 | IPC_CREAT);
    matrix2 = shmat(shmId2, NULL, 0);
    pthread_t threads[threadsCount];
    for (int t = 0; t < threadsCount; t++)
    {
        ThreadData *data = (ThreadData * )malloc(sizeof(ThreadData));
        data->id = t;
        data->tCount = threadsCount;
        data->rows1 = i;
        data->cols1 = j;
        data->cols2 = k;
        data->sum = 0;
        pthread_create(&threads[t], NULL, thread_operations, data);
    }
    
    for (int t = 0; t < threadsCount; t++)
        pthread_join(threads[t], NULL);
    

    FILE* fp = fopen(outFile, "w");
    for (int row = 0; row < i; row++)
    {
        for (int col = 0; col < k; col++)
            fprintf(fp, "%lld ", matrix3[row * k + col]);
            fflush(fp);
        fprintf(fp,"\n");
    }
    fclose(fp);
    //Stop the clock after end of execution
    clock_gettime(CLOCK_REALTIME, &endRT);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &endPT);
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &endTT);
    //Detach shared momory variables 
    shmdt(matrix1);
    shmdt(matrix2);

    //If suggested to release the shared memory
    if (releaseSharedMemory == 1) {
        //Destroy the shared memory 
        shmctl(shmId1, IPC_RMID, NULL);
        shmctl(shmId2, IPC_RMID, NULL);
    }
    unsigned long long int p2_endtime = (endTT.tv_sec) * NANO_SECONDS_PER_SECOND + (endTT.tv_nsec);
    exit(2);
}