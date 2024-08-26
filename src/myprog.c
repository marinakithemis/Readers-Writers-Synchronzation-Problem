#include <ctype.h>
#include "header.h"
#include <string.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <time.h>

bool is_integer(char* value);

int main(int argc, char* argv[])
{
    srand(time(NULL));
    struct stat st;   //we need this to find the size of the file
    char* filename, *reader, *writer;
    int time_r, time_w;  //max time a reader can read (time_r), max time a writer can write(time_w)

    if (argc != 11){
        error_exit("Wrong number of arguments\n");
    }

    ////check the flags in the arguments////
    for(int i = 1; i < argc ; i+=2){
        if(strcmp(argv[i], "-dr") == 0){
            if(i+1 < argc){
                if(is_integer(argv[i+1])){       //we have to check if after flag -dr we have an integer 
                    time_r = atoi(argv[i+1]);
                }
                else{
                    error_exit("wrong arguments\n");
                }
            }
        }
        else if(strcmp(argv[i], "-dw") == 0){
            if(i+1 < argc){
                if(is_integer(argv[i+1])){       //we have to check if after flag -dw we have an integer 
                    time_w = atoi(argv[i+1]);
                }
                else{
                    error_exit("wrong arguments\n");
                }
            }
        }
        else if(strcmp(argv[i], "-f") == 0){
            if(i + 1 < argc){
                filename = argv[i+1];
            }
        }
        else if(strcmp(argv[i], "-r") == 0){
            if(i + 1 < argc){
                reader = argv[i+1];
            }
        }
        else if(strcmp(argv[i], "-w") == 0){
            if(i + 1 < argc){
                writer = argv[i+1];
            }
        }
        else{
            error_exit("Wrong flags\n");
        }
    }

    int fd1 = open(filename, O_RDONLY);

    if(fd1 == -1){
        error_exit("file opening11\n");
    }

    stat(filename, &st);
    int size = st.st_size;    //size of file
    int num_of_entries = size / 52;   //each entry is 52 bytes, so dividing the size of the file with 52 we get the number of entries

    close(fd1);

    //create the shared memory segment 
    const char *shmpath = "shared_mem";

    int fd = shm_open(shmpath, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
    if(fd == -1){
        error_exit("shared mem111\n");
    }
    
    if(ftruncate(fd,sizeof(struct shm)) == -1){
        error_exit("ftruncate\n");
    }

    //map the object
    struct shm *shmem = mmap(NULL, sizeof(*shmem), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if(shmem == MAP_FAILED){
        error_exit("map\n");
    }

    //initialize all semaphores to 1
    if(sem_init(&shmem->lock, 1, 1) == -1){
        error_exit("Semaphore Initialization\n");
    }
    if(sem_init(&shmem->mutex_r, 1, 1) == -1){
        error_exit("Semaphore Initialization\n");
    }
    if(sem_init(&shmem->mutex_w, 1, 1) == -1){
        error_exit("Semaphore Initialization\n");
    }
    if(sem_init(&shmem->time, 1, 1) == -1){
        error_exit("Semaphore Initialization\n");
    }
    if(sem_init(&shmem->block, 1, PROCESS_MAX) == -1){
        error_exit("Semaphore Initialization\n");
    }
    for(int i = 0; i < PROCESS_MAX; i++){
        if(sem_init(&shmem->Process_data[i].proc, 1, 0) == -1){
            error_exit("Semaphore initialization\n");
        }
        shmem->Process_data[i].start_id = 0;
        shmem->Process_data[i].end_id = 0;
        shmem->Process_data[i].sem_counter = 0;
        shmem->Process_data[i].is_reader = 0;
        shmem->Process_data[i].pid = 0;
        shmem->Process_data[i].priority = 10000000;
        shmem->available_index[i] = 1;
        
    }

    //initialize the rest of the variables in the shared memory struct
    shmem->processes = 0;
    shmem->tot_readers = 0;
    shmem->tot_writers = 0;
    shmem->num_of_recs_processed = 0;
    shmem->max_time = 0;
    shmem->avg_rtime = 0;
    shmem->avg_wtime = 0;
    shmem->processes = 0;

    //generate random number of readers and random number of writers
    int num;
    num = (rand() % 500) + 10;
    
    int root_pid = getpid();

    for(int i = 0; i < 20; i++){
        srand(time(NULL) + 20*i);
        int pid = fork(); 
        if(pid == -1) error_exit("Error during fork\n");
        
        char *recid_str = malloc(BUFFER_SIZE*sizeof(char));
        char *time_str = malloc(BUFFER_SIZE*sizeof(char));
        char *value_str = malloc(BUFFER_SIZE*sizeof(char));
        char *recid_fstr = malloc(BUFFER_SIZE*sizeof(char));
        char *recid_tstr = malloc(BUFFER_SIZE*sizeof(char));
        char *range = malloc(BUFFER_SIZE*sizeof(char));
        
        if(pid == 0){
            if(i % 3 != 0){   //generate writers
                //initialize arguments and strings to put into execlp's arguments
                int recid = (rand() % num_of_entries) + 1;
                int value = (rand() % 101) - 50;
                
                snprintf(recid_str, BUFFER_SIZE, "%d", recid);
                snprintf(time_str, BUFFER_SIZE, "%d", time_w);
                snprintf(value_str, BUFFER_SIZE, "%d", value);
                //call execlp
                int res = execlp(writer, writer, "-f", filename, "-l", recid_str, "-v", value_str, "-d", time_str, "-s", shmpath, NULL);
                if(res == -1){
                    error_exit("Error during exec\n");
                }            
            }
            else{             //generate readers
                //initialize arguments and strings to put into execlp's arguments
                int recid_from = (rand() % num_of_entries + 1);
                int recid_to;
                if(recid_from == num_of_entries){
                    recid_to = recid_from;
                }
                else{
                    recid_to = recid_from + (rand() % (num_of_entries-recid_from) + 1);
                }
                
                snprintf(time_str, BUFFER_SIZE, "%d", time_r);
                snprintf(recid_fstr, BUFFER_SIZE, "%d", recid_from);
                snprintf(recid_tstr, BUFFER_SIZE, "%d", recid_to);
                sprintf(range, "%s,%s", recid_fstr, recid_tstr);
                //call execlp
                int res = execlp(reader, reader, "-f", filename, "-l", range, "-d", time_str, "-s", shmpath, NULL);
                if(res == -1){
                    error_exit("Error during exec\n");
                } 
            } 
        }
        else{
            free(recid_str);
            free(recid_fstr);
            free(recid_tstr);
            free(time_str);
            free(value_str);
            free(range);
        }
    }
    for(int i = 0; i < num; i++){
        wait(NULL);
    }
    int paidi = getpid();

    if(paidi == root_pid){

        //print the statistics 
        printf("\nTotal number of readers: %d\n", shmem->tot_readers);
        printf("Total number of writers: %d\n", shmem->tot_writers);
        double avg_rtime = (float)shmem->avg_rtime / (float) shmem->tot_readers;
        double avg_wtime = (float)shmem->avg_wtime / (float) shmem->tot_writers;
        printf("Average reader time: %f\n", avg_rtime);
        printf("Average writer time: %f\n", avg_wtime);
        printf("Total entries we worked on: %d\n", shmem->num_of_recs_processed);
        printf("Maximum waiting time: %f\n", shmem->max_time);

        //unmap the shared memory segment
        if(munmap(shmem, sizeof(*shmem)) == -1) error_exit("munmap");

        if(close(fd) == -1) error_exit("close fd\n");
        
        if(shm_unlink(shmpath) == -1) error_exit("shm_unlink\n"); 
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
