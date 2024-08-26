# Readers-Writers Synchronization Problem

This program implements an efficient solution to the classic readers-writers problem by allowing fine-grained control over access to shared resources. Unlike the typical approach, where the entire file is locked when a writer modifies a record, this implementation only locks the specific record/records being accessed. This allows multiple readers and writers to operate simultaneously on different records, significantly improving concurrency and performance. The program manages access using an array to track active and blocked processes, ensuring that the critical section is protected while allowing for greater parallelism and reducing unnecessary blocking.

## How to Compile and Run

- Use the command `make` to create the executable files.
- Use the command `make clean` to delete the object files.
- Use the command `make run` to execute the program with default values. The output will be written to the `output.txt` file:
  ```bash
  ./myprog -dr 2 -dw 2 -f accounts5000.bin -r ./reader -w ./writer > output.txt
  ```
- Writing is always done randomly.
- I have set the program to randomly create between 10 and 500 processes (you can modify this if needed).
- Regarding active readers/writers, I maintain two arrays to store the current readers/writers that are in the critical section. However, the `Process_Data` array stores both these active processes and those that are blocked and waiting to be activated.

## Readers-Writers

- The `reader.c` and `writer.c` programs are synchronized in such a way that multiple readers and writers can work simultaneously, provided they are working on different records. Multiple readers can read the same record, but when a writer is writing to a record, no one else is allowed to work on that same record.
- In the `header.h` file, I have defined a maximum number of active processes (`PROCESS_MAX`) that can work simultaneously. However, you can change this to allow more processes.
- The basic idea behind the algorithm is that I have an array `Process_Data`, where I store data such as the record IDs (`recids`) that each active or blocked process wants to access, a priority number, etc. Whenever a reader or writer process starts, if the array isn't full, I assign it a position in the array (index). Then, through a for loop, I check if there is another process in the array with a lower priority (the smaller the priority number, the earlier it arrived) that needs those records. If yes, the semaphore counter of my process increases (this variable essentially shows how many processes are blocking the current one). Finally, I perform a `wait` on its semaphore.
- The index assignment is done with the help of an auxiliary array (`available_index`), which has `PROCESS_MAX` positions. I store `1` if the position in the `Process_Data` array is empty and `0` if it is full. I place the process in the first available position found in the array.
- After a process enters the critical section and completes its read/write operation, it sleeps for a random number of seconds. Before exiting, it checks the entire `Process_Data` array again for any processes that may have been blocked because of it. If it finds any, it decreases their semaphore counter. If the counter reaches zero (meaning no other process before it is blocking it), it unblocks them by posting their semaphore.
- Through this algorithm, the critical section problem is resolved, satisfying the three key conditions:
  1. **Mutual Exclusion:** If a writer is working on a specific record, no other process is allowed to work on the same record. Similarly, if a reader is working on a set of records, no writer can enter to update any of those records.
  2. **Progress:** If no process is in the critical section and there is a process that wants to enter, it can enter immediately without waiting.
  3. **Bounded Waiting:** If a process is in the array and is blocked by processes that arrived before it, it will be unlocked and work as soon as those processes finish. If other processes needing the same records arrive while it is blocked, they will be locked

## Additional Files

- The testing_files folder contains files with records that you can use to test the functionality of the program.
