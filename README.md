## DQueue

DQueue is a multi-producer-single-consumer queue that is wait-free and processor-friendly. It outperforms previous queues in three different architectures (x86, Power8, and ARMv8) by utilizing novel explicit optimization techniques, including adding local buffers, reducing the use of branches, atomic operations and memory fences in program. DQueue is released under GPL v2. DQueue includes the following files:

- dqueue.c: Source code of DQueue.
- dqueue.h: Header file of dqueue.c.
- mpsc.c: The multi-producer-single-consumer testbed.
- verify.py: The python script to verify safety of each queue.

## Framework

DQueue reuses the framework from Yang's WFQueue [1]. So most files in these directory belong to Yang. Copyright of these files is LICENSE.wfqueue. For details about this framework, please check README.wfqueue.md.

## Compile and Run

On most 64-bits machines running Linux, command "make" is enough to build testbed and DQueue executable file.

Usage:
```
./dqueue 16   (To create one consumer thread and 15 producer threads)
```

## Affinity setting files

Upon start, DQueue first loads the specified affinity settings in cpumap.h. Users must check and revise the configuration before running DQueue on their machines.

## Verify correctness

Our testbed contains a brute force yet very helpful tool to verify the correctness of queues. To use this tool, you should compile the project by using command:
```
VERIFY=1 make
```
Then, each time you run DQueue (and others), each thread (including the consumer and producers) will log the data it inserts into or reads from the queue. These log files will be written into directory ``long-<timestamp>'', which can be verified by using the python script **verify.py**.


## Contact

If you have any questions or suggestions regarding DQueue, please send email to junchangwang@gmail.com


--


[1] C. Yang and J. M. Mellor-Crummey, “A wait-free queue as fast as fetch-and-add,” in PPOPP, 2016.
