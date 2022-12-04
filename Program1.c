#include <stdio.h>
#include<string.h>
#include<stdlib.h>
#include<pthread.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <unistd.h>
#include<signal.h>
#define SHM_KEY_1 0x1289
#define SHM_KEY_2 0x1306

#define NANO_SECONDS_PER_SECOND 1000000000
//shared memory for final matrices, second matrix as Transposed 
int* matrix1, * matrix2;

//data element for sendig to thread from main program for processing  
typedef struct {
    int id;             //id of thread 
    int tCount;         //Threads count
    int rows;           //Rows of the matrix to save
    int cols;           //Columns of the matrix to save
    FILE* fp;           //file to Read
    long* positions;    //Array of number positions in the file
    int* matrix;        //Matrix to save after reading from the file 
    sem_t* mutex;
} ThreadData;

//Preprocessing for saving positions of numbers in the file to array for reading later 
void preProcess(char* file, long* outArray, int rows, int cols, int transpose) {
    //printf ("Processing File: %s \n", file);
    FILE* fp = fopen(file, "r");
    int ctr = 0;
    outArray[ctr] = 0;
    ctr++;
    //Reading each character from the file to find spaces/ new line.    
    char ch = fgetc(fp);
    //Stop if end of file
    while (ch != EOF) {
        //Numbers are placed with separator of single space or newline
        if (ch == 32 || ch == '\n') {
      	    char check = fgetc(fp);
      	    if(check == EOF){
      	    	break;
      	    }
      	    else{
      	    	ungetc(check, fp);
      	    }
            //in case of transpose save position of columns to rows 
            int temp = transpose == 0 ? ctr : rows * (ctr % cols) + (ctr / cols);
            outArray[temp] = ftell(fp);
            //printf(" %ld ", outArray[temp]);
            ctr++;
        }
        //printf("%d ", ch);
        ch = fgetc(fp);
    }
    fclose(fp);
}

//Function to be called as thread for reading data from the given file and save to given matrix 
void* saveToMatrix(void* argv) {
    //converting void pointer to ThreadData datatype
    ThreadData* td = argv;
    FILE* fp = td->fp;
    int rem = (td->rows * td->cols) % td->tCount;  // nonequal share count
    int start = ((td->id) * ((td->rows * td->cols) / td->tCount)) + (rem > 0 && rem > td->id ? td->id : rem);
    int end = start + ((td->rows * td->cols) + td->tCount - 1 - td->id) / td->tCount;

    for (int i = start; i < end; i++) { // threads reading adjecent data elements
        //for (int i = td->id; i < td->rows * td->cols; i=i+ td->tCount) {  //Threads reading data elements in round robin
        sem_wait(td->mutex);
        if (fseek(fp, td->positions[i], SEEK_SET) == 0) {
            fscanf(fp, "%d", &td->matrix[i]);
        }
        sem_post(td->mutex);
    }
    // pthread_exit(0);
}


//NOT CORE, USED FOR DATA VERIFICATION: function for testing the data reading from the file
void printMatrixFromFile(int rows, int cols, char* file, long* matrixPos) {
    FILE* fp;
    printf("\n Start of Printing: %s \n", file);
    fp = fopen(file, "r");

    int ctr = 0, num = 0;
    for (int i = 0; i < rows && num >= 0; i++) {
        for (int j = 0; j < cols && num >= 0; j++) {
            fseek(fp, matrixPos[(i * cols) + j], SEEK_SET);
            fscanf(fp, "%d", &num);
            printf("%d ", num);
        }
        printf("\n");
    }
    printf("End of Printing\n");
    fclose(fp);
}

//NOT CORE, USED FOR DATA VERIFICATION: function for testing the final saved matrices data
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
    char file1[100], file2[100], csvfile[100];
    //Taking commandline arguments into variables
    int i = atoi(argv[1]);
    int j = atoi(argv[2]);
    int k = atoi(argv[3]);
    strcpy(file1, argv[4]);
    strcpy(file2, argv[5]);
    strcpy(csvfile, argv[6]);
    int threadsCount = atoi(argv[7]);
    int releaseSharedMemory = atoi(argv[8]);
 //    printf("%d %d %d %s %s %d\n", i, j, k, file1, file2, threadsCount);
   //Memory allocation for holding number positions in the file and for both matrices   
    long* file1Pos = (long*)calloc((i * j), sizeof(long));
    long* file2Pos = (long*)calloc((j * k), sizeof(long));

    // Shared memory allocation for matrix1 
    int shmId1 = shmget(SHM_KEY_1, (i * j) * sizeof(int), 0666 | IPC_CREAT);
    matrix1 = shmat(shmId1, NULL, 0);
    //Share memory allocation for matrix2
    int shmId2 = shmget(SHM_KEY_2, (j * k) * sizeof(int), 0666 | IPC_CREAT);
    matrix2 = shmat(shmId2, NULL, 0);

    //Preprocessing the files to get the positions of matrices numbers in the file 
    preProcess(file1, file1Pos, i, j, 0);
    //    printMatrixFromFile(i, j, file1, file1Pos);
    preProcess(file2, file2Pos, j, k, 1);
    //    printMatrixFromFile(k, j, file2, file2Pos); //Transposed matrix

    //To calculate time taken for Processing
    struct timespec startRT, endRT, 
                    startPT, endPT,
                    startTT, endTT;
    clock_gettime(CLOCK_REALTIME, &startRT);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &startPT);
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &startTT);
    //Opening files to read
    FILE* fp1 = fopen(file1, "r");
    FILE* fp2 = fopen(file2, "r");

    sem_t mutex1, mutex2;
    sem_init(&mutex1, 0, 1);
    sem_init(&mutex2, 0, 1);
    if (threadsCount == 1) {
        //If Single thread to execute, then no threads are created
        //Data for sending to Thread to read from file1 and save to Matrix1
        ThreadData data1;
        data1.id = 0;
        data1.tCount = 1; // Number of threads to process file1 are half and plus one is to take extra one in case of odd number  
        data1.rows = i;
        data1.cols = j;
        data1.fp = fp1;
        data1.positions = file1Pos;
        data1.matrix = matrix1;
        data1.mutex = &mutex1;
        saveToMatrix(&data1);

        ThreadData data2;
        data2.id = 0;
        data2.tCount = 1; // Number of threads to process file1 are half and plus one is to take extra one in case of odd number  
        data2.rows = k;
        data2.cols = j;
        data2.fp = fp2;
        data2.positions = file2Pos;
        data2.matrix = matrix2;
        data2.mutex = &mutex2;
        saveToMatrix(&data2);
    }
    else {
        //Array to hold created thread ids 
        pthread_t threads[threadsCount];
        //To create specified number of threads to process
        int t1 = 0, t2 = 0;
        ThreadData data[threadsCount];
        for (int t = 0; t < threadsCount; t++)
        {
            if (t % 2 == 0) {
                //Reading input file 1
                data[t].id = t1++;
                data[t].tCount = (threadsCount + 1) / 2; // Number of threads to process file1 are half and plus one is to take extra one in case of odd number  
                data[t].rows = i;
                data[t].cols = j;
                data[t].fp = fp1;
                data[t].positions = file1Pos;
                data[t].matrix = matrix1;
                data[t].mutex = &mutex1;
            }
            else {
                //Data for sending to Thread to read from file2 and save to Matrix2
                data[t].id = t2++;
                data[t].tCount = threadsCount / 2; // Number of threads to process by file2 are half  
                data[t].rows = k;
                data[t].cols = j;
                data[t].fp = fp2;
                data[t].positions = file2Pos;
                data[t].matrix = matrix2;
                data[t].mutex = &mutex2;
            }
            pthread_create(&threads[t], NULL, saveToMatrix, &data[t]);
        }
        //Joining all threads after processing is completed
        for (int t = 0; t < threadsCount; t++) {
            pthread_join(threads[t], NULL);
        }
    }
    //Stop the clock after end of execution
    clock_gettime(CLOCK_REALTIME, &endRT);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &endPT);
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &endTT);


    //Print matrices for data verification
    
    //Releasing allocated memory
    fclose(fp1);
    fclose(fp2);
    
    //Releasing the memory created for positions
    free(file1Pos);
    free(file2Pos);

    //Destroy the semaphores
    sem_destroy(&mutex1);
    sem_destroy(&mutex2);

    //Detach shared momory variables 
    shmdt(matrix1);
    shmdt(matrix2);

    //If suggested to release the shared memory
    if (releaseSharedMemory == 1) {
        //Destroy the shared memory 
        shmctl(shmId1, IPC_RMID, NULL);
        shmctl(shmId2, IPC_RMID, NULL);
    }

    //Calculate time taken in seconds and nano seconds
    unsigned long long int p1_endtime = (endTT.tv_sec) * NANO_SECONDS_PER_SECOND + (endTT.tv_nsec);
    exit(1);
}