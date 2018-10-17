
DQueue is a multi-producer-single-consumer queue that is wait-free and processor friendly. It outperforms previous queues in three different architectures by utilizing novel explicit optimization techniques, including adding local buffers, reducing the use of branches, atomic operations and memory fences in program.

This project reuses the framework from Chaoran's WFQueue.
