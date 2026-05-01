🖥️ Simulated Operating System
A university project built for the Operating Systems course at GUC. The system simulates core OS functionality including process scheduling, memory management, mutex-based resource control, and a basic program interpreter, visualized through a Python-based GUI.
✨ Key Features

Program Interpreter: Reads and executes .txt program files as processes using a custom instruction set (print, assign, writeFile, readFile, printFromTo, semWait, semSignal)
Memory Management: Fixed 40-word memory with PCB storage, variable allocation, and disk swapping when memory is full
Process Control Block (PCB): Tracks process ID, state, program counter, and memory boundaries per process
Mutual Exclusion: Three mutexes controlling access to shared resources: userInput, userOutput, and file
Scheduling Algorithms: HRRN (Highest Response Ratio Next) and Round Robin (2 instructions per time slice)
Multi-Level Feedback Queue (MLFQ): Bonus scheduling algorithm with 4 priority queues
GUI: Python-based visual display of queues, running process, memory contents, disk swapping, and step-by-step clock cycle execution

🗂️ System Components

Code Parser / Interpreter
System Calls
Mutexes
Scheduler
Memory Manager

📋 Process Arrival Order
ProcessArrival TimeProcess 10Process 21Process 34
🛠️ Tech Stack

Core OS Simulation: C
GUI: Python (bonus component)

🚀 Getting Started
Running the OS simulation:
bashgcc -o os_sim *.c
./os_sim
Running the GUI:
bashpip install -r requirements.txt
python main.py
