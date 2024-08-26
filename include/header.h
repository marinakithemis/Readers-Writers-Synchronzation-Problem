#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUFFER_SIZE 150   //max string size

#define error_exit(msg)    do { perror(msg); exit(0); \
                            } while (0)

#define READER_MAX 100   //max number of readers

#define WRITER_MAX 100   //max number of writers

#define PROCESS_MAX 100 

struct process{
    sem_t proc;
    int start_id;
    int end_id; //only for readers, for writers it is 0
    int sem_counter;   //how many times have we currently used sem_wait on this semaphore
    int is_reader; //1 if we have a reader 2 if we have a writer
    int priority;
    int pid;
};

//define the struct of the shared memory
struct shm{
    sem_t mutex_r, mutex_w;
    sem_t lock;  
    sem_t time;
    sem_t block;
    struct process Process_data[PROCESS_MAX];
    int processes;   //number of curent processes
    int tot_readers, tot_writers, num_of_recs_processed;
    double avg_rtime, avg_wtime, max_time;
    int active_read[READER_MAX];
    int active_write[WRITER_MAX];
    int available_index[PROCESS_MAX];    //1 if available, 0 if not available
};

struct Entry{
    int id;
    char lname[20];
    char fname[20];
    int balance;
};

