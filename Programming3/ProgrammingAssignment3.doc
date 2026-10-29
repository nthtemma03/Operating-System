Programming Assignment 3
CSCE 4600: Introduction to CPU Scheduling and Process Synchronization
Student Name: Thuan Nguyen
Date: 10/28/2026

1. Introduction
The purpose of this assignment is to understand CPU scheduling, process synchronization, and communication between processes. The assignment contains two programming questions.
Question 2 implements four CPU scheduling algorithms: First-Come, First-Served (FCFS), Shortest Job First (SJF), Priority Scheduling, and Round Robin. The program calculates the waiting time, turnaround time, and response time of each process.
Question 3 implements a TCP client and server system with bounded buffers. The client accepts requests from a user and stores them in a local buffer. The server receives requests and stores them in a server buffer until a consumer process can process them. Process synchronization is used to coordinate access to shared memory and prevent conflicting operations.
Both programs are implemented in C. The source files are submitted separately from this report.
2. Question 2: CPU Scheduling
2.1 Process Data Structure
The program uses a process data structure to store information required by the scheduling algorithms. Each process contains its name, arrival time, burst time, latest priority, waiting time, turnaround time, response time, completion time, first start time, remaining execution time, and completion status.
At the beginning of the program, the user enters the number of processes and provides the name, arrival time, and burst time for each process. The program initializes the latest priority to zero and calculates scheduling results using the input information.
Each scheduling algorithm operates on a fresh copy of the process list. This ensures that the results from one algorithm do not affect the results of another algorithm.
2.2 Scheduling Metrics
The program calculates three required metrics for each process.
Waiting Time: The total time a process spends waiting to use the CPU.
Turnaround Time: The total time from when a process arrives until it finishes execution.
Turnaround Time = Completion Time − Arrival Time
Response Time: The time between a process's arrival and the first time it receives the CPU.
Response Time = First Start Time − Arrival Time
For non-preemptive algorithms, a selected process runs until completion. Round Robin is different because a process can be interrupted when its time quantum expires and returned to the ready queue.
2.3 Priority Calculation
The assignment defines the process priority using the following formula:
Priority = α / ta + γ / tb + β × tw
The constants are:
α = 0.15
β = 0.35
γ = 0.25
Here, ta represents arrival time, tb represents burst time, and tw represents the current waiting time.
To avoid division by zero, the arrival time used in the formula is set to 0.1 when a process arrives at time zero. The program initializes each process's latest priority to zero and updates it according to the formula. A larger priority value represents a higher priority.
Priority scheduling is non-preemptive. When the CPU becomes available, the program selects the eligible waiting process with the highest priority. Once selected, that process continues until completion.
2.4 First-Come, First-Served (FCFS)
FCFS schedules processes in arrival order. The process that arrives first is selected first. Once a process begins execution, it continues until it finishes.
The algorithm calculates the start and completion times of each process and uses them to determine waiting time, turnaround time, and response time.
FCFS is simple to implement, but a process with a long burst time can make other processes wait for a long time.
2.5 Shortest Job First (SJF)
SJF selects the process with the shortest burst time among the processes that have already arrived and are waiting for the CPU.
Whenever the CPU becomes available, the program checks the eligible processes and selects the one with the smallest burst time. The selected process runs until completion.
SJF can reduce average waiting time when short processes are scheduled before longer processes. However, a long process may wait while shorter processes are selected.
2.6 Priority Scheduling
Priority scheduling selects the eligible waiting process with the highest calculated priority. The program updates the priority based on the assignment formula and the process's current waiting time.
After selecting a process, the CPU executes it until completion because the algorithm is non-preemptive. When the CPU becomes available again, the program recalculates the priorities of eligible waiting processes before selecting the next process.
This algorithm demonstrates how priority values influence the order in which processes receive CPU time.
2.7 Round Robin Scheduling
Round Robin schedules processes using a ready queue and a fixed time quantum. Each process receives the CPU for up to the specified quantum.
If a process finishes before the quantum expires, it leaves the ready queue. Otherwise, its remaining execution time is reduced, and the process is placed at the back of the queue. Processes that arrive during execution are added to the ready queue when they become eligible.
The process repeats until every process has completed execution.
Round Robin provides each ready process with opportunities to execute. Its behavior depends on the time quantum and the order in which arriving and requeued processes are handled.
2.8 Compilation and Execution
The program is compiled and executed from the terminal in the directory containing the source file.
Compilation command:
gcc -Wall -Wextra -std=c11 CPU_Schedule.c -o CPU_Schedule
Execution command:
./CPU_Schedule
The program prompts the user to enter the number of processes and the details of each process. It then displays the scheduling metrics for FCFS, SJF, Priority Scheduling, and Round Robin.
If the source file has a different name, the compilation command must use that exact filename.
2.9 Testing Procedure
The CPU scheduling program should be tested with multiple processes having different arrival times and burst times. The results should be checked to confirm that each algorithm follows its scheduling rules.
One test case uses the following input:
Process
Arrival Time
Burst Time
P1
0
5
P2
1
3
P3
2
1

For Round Robin, the time quantum is set to 2.
The expected execution order for FCFS is P1, P2, and P3. For SJF, P1 runs first because it is the only process available at time zero. After P1 completes, P3 has the shortest burst time, followed by P2.
For Round Robin, assuming newly arrived processes are added to the ready queue before a process whose quantum just expired is requeued, the expected metrics are:
Process
Waiting Time
Turnaround Time
Response Time
Completion Time
P1
4
9
0
9
P2
4
7
1
8
P3
2
3
2
5

These expected results should be compared with the output from the final program. Priority scheduling results should also be checked against the calculated priority values and the program's tie-breaking rules.
Test results: [Insert a screenshot of the CPU scheduling program and its output here.]
3. Question 3: Process Synchronization and TCP Client/Server
3.1 System Overview
The client represents Sophia, who sends requests to the server. The server represents SkyisSmall, which receives requests and processes them when resources are available.
The client and server communicate through a TCP/IP connection. For local testing, the client connects to the server at 127.0.0.1 on port 5050.
The implementation uses two bounded buffers. The local client buffer stores requests entered by the user, while the server buffer stores requests that have been accepted and are waiting to be processed. Each buffer has a capacity of five requests.
The client and server use shared memory and named POSIX semaphores to coordinate access to their buffers across processes. Named semaphores are used because unnamed process-shared semaphores were not supported by the macOS environment used for testing.
3.2 Client Design
The client has two main processes.
The first process accepts user input. When the user enters a request, the program checks whether space is available in the local buffer. If space is available, the request is added to the buffer. The program does not add the request when the local buffer is full.
The second process handles network communication. It checks whether the local buffer contains pending requests and contacts the server to determine whether server buffer capacity is available. When the server has space, the communication process sends a pending request and waits for the server's response.
The user can enter multiple requests while the communication process works independently. Entering /quit stops the client from accepting additional input.
3.3 Server Design
The server receives requests from the client over the TCP connection. It checks the available capacity of the server buffer before accepting a request.
The server uses a producer-consumer design with separate processes.
The producer receives requests from the client and writes accepted requests into the server buffer when space is available. If the buffer is full, the server must not write another request into it.
The consumer waits until requests are available in the server buffer. It then removes requests from the buffer and processes them. This allows the server to receive requests independently of the consumer's processing speed.
When a request is accepted, the server sends an acknowledgment to the client. An acknowledgment means that the request has been accepted into the server buffer, not necessarily that processing has already finished.
3.4 Process Synchronization
Shared memory allows related processes to access the same buffer data. However, multiple processes accessing the same buffer can cause race conditions if they update the buffer simultaneously.
Named POSIX semaphores coordinate access to shared resources.
The mutex semaphore protects critical sections where processes update shared buffer information, including the buffer's head, tail, and item count. The empty-slot semaphore tracks available buffer positions, while the full-item semaphore tracks queued requests that are ready for processing.
Before a producer writes a request, it must ensure that space is available. A consumer waits until data is available before removing a request. These synchronization mechanisms help prevent buffer overflow and conflicting updates.
The implementation uses named semaphores that can be shared by related processes. The program also includes cleanup operations for semaphore handles and names.
3.5 Client and Server Communication Protocol
The client and server exchange messages using TCP. The implementation uses a simple line-based protocol.
First, the client sends a capacity-check message to ask how much space is available in the server buffer. The server responds with the available capacity as the first line of the response.
If server buffer space is available and the client has pending data, the client sends the request. The server attempts to add the request to its buffer and returns an acknowledgment indicating whether it was accepted.
The response includes the updated available capacity. This allows the client to determine whether the server can accept additional requests.
The client sends pending requests only when the server reports available space. If the server cannot accept a request, the client must keep pending data locally and retry according to the program's communication logic.
3.6 Compilation and Execution
The client and server source files are compiled separately on macOS.
Server compilation:
gcc -Wall -Wextra -std=c11 server.c -o server
Client compilation:
gcc -Wall -Wextra -std=c11 client.c -o client
Open two terminal windows in the directory containing the source files.
In the first terminal, start the server:
./server
Wait until the server reports that it is listening on port 5050.
In the second terminal, start the client:
./client
Enter requests one per line. The client should indicate when requests are queued locally and display the server's capacity information and acknowledgment responses.
Enter /quit to stop accepting new input. The server can remain running for another client connection. When testing is complete, press Control+C in the server terminal to stop it.
3.7 Testing Procedure and Results
The client and server were compiled and run on macOS. The server successfully started and listened on port 5050. The client connected to the server, and two sample requests were entered:
open a file
read my file
Both requests were queued locally and sent to the server. The client received acknowledgment messages indicating that the requests were accepted. The server's consumer process printed messages confirming that it processed both requests.
The observed output showed that the client-server connection, request transmission, acknowledgment mechanism, and consumer processing were working for these sample requests.
The following additional tests should be completed before final submission.
Test
Expected Result
Actual Result
Start server
Server listens on port 5050.
Successful
Connect client
Server accepts the client connection.
Successful
Send a request
Server accepts the request and acknowledges it.
Successful
Process a request
Consumer removes and processes the request.
Successful
Fill the server buffer
The server does not accept more requests than its capacity allows.
To be tested
Fill the local buffer
The client does not write additional requests when its local buffer is full.
To be tested
Quit and reconnect
Client exits cleanly and a new client can reconnect.
To be tested

Test results: [Insert screenshots of the server terminal and client terminal here. Update the table after completing the remaining tests.]
4. Conclusion
This assignment demonstrates CPU scheduling algorithms and process synchronization through two C programs.
The CPU scheduling program implements FCFS, SJF, Priority Scheduling, and Round Robin. It calculates waiting time, turnaround time, and response time to illustrate how different scheduling strategies affect process execution.
The client-server program uses TCP/IP communication, shared memory, named POSIX semaphores, and bounded buffers. The client accepts user requests and sends them when server capacity is available. The server acknowledges accepted requests and uses a consumer process to process requests from its buffer.
Initial testing confirmed that the client and server can connect, exchange requests, return acknowledgments, and process requests. The remaining buffer-capacity and reconnection tests should be completed to verify the system's behavior under additional conditions.
The report is submitted as a PDF, while the C source files are submitted separately as required by the assignment instructions.

