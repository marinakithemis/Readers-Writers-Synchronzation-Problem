#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/times.h>
#include <sys/time.h>
#include "../include/header.h"

bool is_integer(char* value);

int main(int argc, char* argv[])
{
    srand(time(NULL));
    //timer variables
    struct timeval start, end, start2, end2;
    double time, time2;
    long sec, msec, sec2, msec2;
    gettimeofday(&start, NULL);
    gettimeofday(&start2, NULL);

    char* filename;
    char* shmpath;
    int recid_from, recid_to;   
    int max_time;

    if (argc != 9){
        error_exit("Wrong number of arguments\n");
    }

    //check the flags in the arguments//
    for(int i = 1; i < argc; i += 2){
        if(strcmp(argv[i], "-f") == 0){
            if(i + 1 < argc){
                filename = argv[i+1];
            }
        }
        else if(strcmp(argv[i], "-l") == 0){
            if(i + 2 < argc){
                recid_from = atoi(strtok(argv[i+1], ","));
                recid_to = atoi(strtok(NULL,","));
            }
        }
        else if(strcmp(argv[i], "-d") == 0){
            if(i+1 < argc){
                if(is_integer(argv[i+1])){
                    max_time = atoi(argv[i+1]);
                }
                else{
                    error_exit("Wrong arguments\n");
                }
            }
        }
        else if(strcmp(argv[i], "-s") == 0){
            if(i+1 < argc){
                shmpath = argv[i+1];
            }
        }
        else{
            error_exit("Wrong flags\n");
        }
    }

    //open the shared memory segment and map it 
    int fd = shm_open(shmpath, O_RDWR, 0);
    if(fd == -1){
        error_exit("shm_open\n");
    }

    struct shm *shmem = mmap(NULL, sizeof(*shmem), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(shmem == MAP_FAILED){
        error_exit("mmap\n");
    }

    //we can't have > PROCESS_MAX processes active 
    sem_wait(&shmem->block);

    //get an index from the available indexes
    int index;
    sem_wait(&shmem->lock);
    for(int i = 0; i < PROCESS_MAX; i++){
        if(shmem->available_index[i] == 1){
            index = i;
            shmem->available_index[i] = 0;
            break;
        }
    }
    shmem->Process_data[index].start_id = recid_from;
    shmem->Process_data[index].end_id = recid_to;
    shmem->Process_data[index].is_reader = 1;
    shmem->Process_data[index].pid = getpid();
    shmem->processes += 1;
    shmem->Process_data[index].priority = shmem->processes;
    sem_post(&shmem->lock);

    //check if we need to block our process
    sem_wait(&shmem->lock);
    for(int i = 0; i < PROCESS_MAX; i++){
        if(shmem->Process_data[i].priority < shmem->Process_data[index].priority){
            if(shmem->Process_data[i].is_reader == 2){   //writer process
                //someone else is updating the record we want, we have to wait
                if(shmem->Process_data[i].start_id >= recid_from && shmem->Process_data[i].start_id <= recid_to){ 
                    //increase the counter
                    shmem->Process_data[index].sem_counter += 1;
                }
            }
        }
    }
    sem_post(&shmem->lock);

    //if the counter is > 0 it means that there are processes with higher priority that block this process
    if(shmem->Process_data[index].sem_counter > 0) sem_wait(&shmem->Process_data[index].proc); 
    
    //we are into the CS

    //some updates for the active readers matrix
    sem_wait(&shmem->lock);
    shmem->active_read[index] = getpid();
    sem_post(&shmem->lock);

    gettimeofday(&end2, NULL);
    sec2 = end2.tv_sec - start2.tv_sec;
    msec2 = end2.tv_usec - start2.tv_usec;
    time2 = sec2 + msec2 / 1e6;

    //do the reading
    int fd1 = open(filename, O_RDWR);
    if(fd1 == -1) error_exit("file opening\n");
    
    int it = recid_to - recid_from;
    int sum = 0, mid = 0;
    
    for(int i = 0; i <= it; i++){
        struct Entry en;
        off_t entry = (recid_from + i - 1) * sizeof(struct Entry);
        if(lseek(fd1, entry, SEEK_SET) == -1){
            close(fd1);
            error_exit("lseek error\n");
        }
        read(fd1, &en, sizeof(struct Entry));
        printf("\nREADER:%d JUST READ: %d %s %s %d",getpid(), en.id, en.fname, en.lname, en.balance);
        fflush(stdout);
        sum += en.balance;
    }
    
    close(fd1);

    //calculate average balance
    if(it != 0) mid = sum / it;  
    else mid = sum;
    
    printf("\nProcess %d Average Balance: %d", index, mid);
    fflush(stdout);

    //done with reading 
    //sleep
    int sleep_time;
    if(max_time == 0) sleep_time = 0;
    else sleep_time = (rand() % max_time) + 1;
    sleep(sleep_time);

    //unlock the blocked process semaphore
    sem_wait(&shmem->lock);
    for(int i = 0; i < PROCESS_MAX; i++){
        if(shmem->Process_data[i].is_reader == 2 && shmem->Process_data[i].priority > shmem->Process_data[index].priority){
            //someone has been waiting so we unlock his process
            if(shmem->Process_data[i].start_id >= recid_from && shmem->Process_data[i].start_id <= recid_to){ 
                shmem->Process_data[i].sem_counter -= 1;
                //no more processes block process i
                if(shmem->Process_data[i].sem_counter == 0) sem_post(&shmem->Process_data[i].proc);
            } 
        }
    }

    //reinitialize values
    shmem->Process_data[index].end_id = 0;
    shmem->Process_data[index].start_id = 0;
    shmem->Process_data[index].pid = 0;
    shmem->Process_data[index].sem_counter = 0;
    shmem->Process_data[index].is_reader = 0;
    shmem->Process_data[index].priority = 1000000;
    shmem->available_index[index] = 1;
    shmem->num_of_recs_processed += it + 1;
    sem_post(&shmem->block);
    sem_post(&shmem->lock);
    
    sem_wait(&shmem->mutex_r);
    shmem->tot_readers += 1;
    sem_post(&shmem->mutex_r);

    sem_wait(&shmem->time);
    if(time2 > shmem->max_time){
        shmem->max_time = time2;
    }
    sem_post(&shmem->time);

    sem_wait(&shmem->lock);
    shmem->active_read[index] = 0;
    sem_post(&shmem->lock);

    //timer
    gettimeofday(&end, NULL);
    sec = end.tv_sec - start.tv_sec;
    msec = end.tv_usec - start.tv_usec;
    //total time
    time = sec + msec / 1e6;  

    sem_wait(&shmem->mutex_r);
    shmem->avg_rtime += time;
    sem_post(&shmem->mutex_r);

    if(munmap(shmem, sizeof(*shmem)) == -1){
        error_exit("munmap");
    }
    
}

bool is_integer(char* value) 
{
    int i = 0;
    while(value[i] != '\0'){
        if(isdigit(value[i]) == 0){
            return false;
        }
        i++;
    }
    return true;
}