## DQueue

DQueue is an efficient and wait-free multi-producer-single-consumer queue.
It outperforms prior queues on three different architectures (x86, Power8, 
and ARMv8) by utilizing novel explicit optimization techniques, 
including (1) exploiting local buffers, and (2) reducing the use of branches, 
atomic operations and memory fences in program. 
DQueue includes the following files:

- dqueue.c: Source code of DQueue.
- dqueue.h: Header file of dqueue.c.
- mpsc.c: The multi-producer-single-consumer testbed.
- verify.py: The python script to verify safety of each queue.

## Framework

For a fair comparison, DQueue integrates into the framework of Yang's WFQueue [1]. 
The details of this framework can be found on README.wfqueue.md.
Copyright of these files is LICENSE.wfqueue. 

## Compile and Run

On most 64-bits machines running Linux, 
command "make" is enough to build the testbed and DQueue executable file.

Usage:
```
./dqueue 16   (To create one consumer thread and 15 producer threads)
```

## Affinity setting files

Upon start, DQueue loads the specified affinity settings in cpumap.h. 
Users must check and revise the configuration before running DQueue on their machines.

## Verify correctness

Our testbed contains a brute force yet helpful tool to verify the correctness of the queues. 
To use this tool, you should compile the project by using command:
```
VERIFY=1 make
```
Then, each time you run DQueue (and others), each thread (including the consumer and producers) will log the data it inserts into or reads from the queue. These log files will be written into directory ``log-<timestamp>'', which can be verified by using the python script **verify.py**.


## Contact

If you have any questions or suggestions regarding DQueue, please send email to junchangwang@gmail.com


--


[1] C. Yang and J. M. Mellor-Crummey, “A wait-free queue as fast as fetch-and-add,” in PPOPP, 2016.
