#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<pthread.h>
#include<time.h>
#include<signal.h>
#include<wait.h>
#include<unistd.h>
#include<sys/ipc.h>
#include<sys/shm.h>

#define SHM_KEY_1 0x1289
#define SHM_KEY_2 0x1306
#define NANO_SECONDS_PER_SECOND 1000000000
//shared memory for final matrices, second matrix as Transposed 
int* matrix1, * matrix2;
int flag1 = 1;
int flag2 = 1;

int main(int argc, char* argv[])
{
    int i, j, k;
    i = atoi(argv[1]);
    j = atoi(argv[2]);
    k = atoi(argv[3]);
 
    int shmId1 = shmget(SHM_KEY_1, (i * j) * sizeof(int), 0666 | IPC_CREAT);
    matrix1 = shmat(shmId1, NULL, 0);
    //initializing matrix elements with -1
    for (int ctr = 0; ctr < i * j; ctr++) {
        matrix1[ctr] = -99999;
    }
    //Share memory allocation for matrix2
    int shmId2 = shmget(SHM_KEY_2, (j * k) * sizeof(int), 0666 | IPC_CREAT);
    matrix2 = shmat(shmId2, NULL, 0);
    //initializing matrix elements with -1
    for (int ctr = 0; ctr < j * k; ctr++) {
        matrix2[ctr] = -99999;
    }

    int tq = 0.002;
    int f1 = 0;
    int p1, p2;
    char *args1[] = {"",argv[1], argv[2], argv[3], argv[4], argv[5], "s_p1.txt", "2", "0", 0};
    char *args2[] = {"",argv[1], argv[2], argv[3], argv[6], "s_p2.txt", "2", "0", 0};
    p1 = fork();
    if(p1 == 0)
    {
        // Exec into P1
        execv("./p1.out", args1);
    }
    else
    {
        p2 = fork();
        if(p2 == 0)
        {
            // Exec into P2
            execv("./p2.out", args2);
        } 
        else
        {
            // parent controller for C1 and C2
            // start a timer with 0;
            struct timespec startRT, endRT,
            startPT, endPT,
            startTT, endTT;
            clock_gettime(CLOCK_REALTIME, &startRT);
            clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &startPT);
            clock_gettime(CLOCK_THREAD_CPUTIME_ID, &startTT);
            kill(p1, SIGSTOP);
            kill(p2, SIGSTOP);
            do {
                if(f1 == 0 && flag1)
                {
                    kill(p2, SIGSTOP);
                    kill(p1, SIGCONT);
                    sleep(tq);
                }
                else if(f1 == 1 && flag2)
                {
                    kill(p1, SIGSTOP);
                    kill(p2, SIGCONT);  
                    sleep(tq);
                }   
                f1 = 1 - f1;   
                int status1, status2;
                int w1 = waitpid(p1, &status1, WNOHANG);   
                if(w1 > 0 && WIFEXITED(status1))
                    flag1 = 0;
                int w2 = waitpid(p2, &status2, WNOHANG);   
                if(w2 > 0 && WIFEXITED(status2))
                    flag2 = 0; 
                if(flag1 == 0)
                    f1 = 1;
                if(flag2 == 0)
                    f1 = 0;
            }while(flag1 == 1 || flag2 == 1);

            clock_gettime(CLOCK_REALTIME, &endRT);
            clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &endPT);
            clock_gettime(CLOCK_THREAD_CPUTIME_ID, &endTT);
            shmdt(matrix1);
            shmdt(matrix2);
            // destroy the shared memory
            shmctl(shmId1, IPC_RMID, NULL);
            shmctl(shmId2, IPC_RMID, NULL);
            unsigned long long int sched_start_time = (startTT.tv_sec) * NANO_SECONDS_PER_SECOND + (startTT.tv_nsec);
        }
    }
    return 0;
}
