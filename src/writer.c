#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <ctype.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <sys/times.h>
#include <sys/time.h>
#include "../include/header.h"

bool is_integer(char* value);

int main(int argc, char* argv[])
{
    srand(time(NULL));

    //timer variables
    struct timeval start, end, start2, end2;
    double time, time2;   //time is total time, time2 is time until we get into the cs
    long sec, msec, sec2, msec2;
    gettimeofday(&start, NULL);
    gettimeofday(&start2, NULL);

    char* filename;
    char* shmpath;
    int recid;   //id of the record that we are going to update
    int value, max_time;

    if (argc != 11){
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
            if(i + 1 < argc){
                if(is_integer(argv[i+1])){
                    recid = atoi(argv[i+1]);
                }
                else{
                    error_exit("Wrong arguments\n");
                }
            }
        }
        else if(strcmp(argv[i], "-v") == 0){
            if(i + 1 < argc){
                value = atoi(argv[i+1]);
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

    //if we have available indexes we "give" one to our process
    int index;
    sem_wait(&shmem->lock);
    
    for(int i = 0; i < PROCESS_MAX; i++){
        if(shmem->available_index[i] == 1){
            index = i;
            shmem->available_index[i] = 0;
            break;
        }
    }
    shmem->Process_data[index].start_id = recid;
    shmem->Process_data[index].is_reader = 2;
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
                if(shmem->Process_data[i].start_id == recid){  
                    //increase our counter
                    shmem->Process_data[index].sem_counter += 1;
                } 
            }
            else if(shmem->Process_data[i].is_reader == 1){    //reader process
                if(recid >= shmem->Process_data[i].start_id && recid <= shmem->Process_data[i].end_id){   //we have to wait
                    //increase our counter
                    shmem->Process_data[index].sem_counter += 1;
                }
            }
        }
    }
    sem_post(&shmem->lock);

    //if the counter is > 0 it means that there are processes with higher priority that block this process
    if(shmem->Process_data[index].sem_counter > 0) sem_wait(&shmem->Process_data[index].proc);
        
    //CS
    sem_wait(&shmem->lock);
    shmem->active_write[index] = getpid();
    sem_post(&shmem->lock);

    //time we waited to get into the critical section
    gettimeofday(&end2, NULL);
    sec2 = end2.tv_sec - start2.tv_sec;
    msec2 = end2.tv_usec - start2.tv_usec;
    time2 = sec2 + msec2/ 1e6;

    sem_wait(&shmem->lock);
    shmem->num_of_recs_processed += 1;
    sem_post(&shmem->lock);

    sem_wait(&shmem->mutex_w);
    shmem->tot_writers += 1;
    sem_post(&shmem->mutex_w);

    int fd1 = open(filename, O_RDWR);
    if(fd1 == -1) error_exit("file opening\n");
    
    off_t entry = (recid-1) * sizeof(struct Entry);
    if(lseek(fd1, entry, SEEK_SET) == -1){
        close(fd1);
        error_exit("lseek error\n");
    }

    //get the entry we want and update its balance
    struct Entry updated_entry;
    read(fd1, &updated_entry, sizeof(struct Entry));

    updated_entry.balance += value;
    printf("\nWRITER %d UPDATED ENTRY: %d %s %s %d\n",getpid(), updated_entry.id, updated_entry.fname, updated_entry.lname, updated_entry.balance);
    fflush(stdout);

    //rewrite the entry on the file
    if(lseek(fd1, entry, SEEK_SET) == -1){
        close(fd1);
        error_exit("lseek error2\n");
    }

    if(write(fd1, &updated_entry, sizeof(struct Entry)) == -1){
        close(fd1);
        error_exit("write error\n");
    }

    close(fd1);

    //finished the update
    //sleep
    int sleep_time;
    if(max_time == 0) sleep_time = 0;
    else sleep_time = (rand() % max_time) + 1;
    sleep(sleep_time);

    //unlock blocked process semaphore
    sem_wait(&shmem->lock);
    for(int i = 0; i < PROCESS_MAX; i++){
        if(shmem->Process_data[i].priority > shmem->Process_data[index].priority){
            //blocked writer
            if(shmem->Process_data[i].is_reader == 2 && shmem->Process_data[i].start_id == recid){ //someone has been waiting so we unlock his process
                shmem->Process_data[i].sem_counter -= 1;
                
                if(shmem->Process_data[i].sem_counter == 0){
                    sem_post(&shmem->Process_data[i].proc);
                }
            }
            
            //blocked reader
            else if(shmem->Process_data[i].is_reader == 1){    //reader process
                if(recid >= shmem->Process_data[i].start_id && recid <= shmem->Process_data[i].end_id){  
                    shmem->Process_data[i].sem_counter -= 1;
        
                    if(shmem->Process_data[i].sem_counter == 0) sem_post(&shmem->Process_data[i].proc);
                }
            }
        }
    }
    
    shmem->Process_data[index].start_id = 0;
    shmem->Process_data[index].sem_counter = 0;
    shmem->Process_data[index].is_reader = 0;
    shmem->Process_data[index].pid = 0;
    shmem->available_index[index] = 1;
    shmem->Process_data[index].priority = 1000000;
    sem_post(&shmem->block);
    sem_post(&shmem->lock);

    sem_wait(&shmem->time);
    if(time2 > shmem->max_time){
        shmem->max_time = time2;
    }
    sem_post(&shmem->time);

    sem_wait(&shmem->lock);
    shmem->active_write[index] = 0;
    sem_post(&shmem->lock);


    //timer for total time
    gettimeofday(&end, NULL);
    sec = end.tv_sec - start.tv_sec;
    msec = end.tv_usec - start.tv_usec;
    //total time
    time = sec + msec/1e6;

    sem_wait(&shmem->mutex_w);
    shmem->avg_wtime += time;
    sem_post(&shmem->mutex_w);

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
