#include "../include/os.h"
#include "../include/memory.h"
#include "../include/parser.h"
#include "../include/semaphore.h"
#include "../include/scheduler_interface.h"
#include "../include/fcfs_scheduler.h"
#include "../include/round_robin_scheduler.h"
#include "../include/mlfq_scheduler.h"
#include "../include/gui.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>

// Global Variables
Scheduler* scheduler = NULL;
pcb_t processes[3];
int num_processes = 0;
int clock_tick = 0;
int simulation_running = 1; // 0 = stopped, 1 = running
int auto_mode = 0;           // 0 = step-by-step, 1 = auto-run
pcb_t* ready_queue[3];
pcb_t* blocked_queue[3];
pcb_t* running_process = NULL;
pcb_t* current = NULL; // Current process being executed

// Function Prototypes
//void load_programs();
void choose_scheduler();
void simulation_step();
void add_process();
void load_program();
//void update_gui();
int extractFirstInt( char *str) {
    int num = 0;
    int found = 0;

    // Traverse the string
    while (*str) {
        if  (isdigit(*str)) {
            num = num * 10 + (*str - '0');
            found = 1;
        } else if (found) {
            // If an integer was found and we hit a non-digit, break
            break;
        }
        str++;
    }

    // If no number was found, you can decide what to return (e.g., 0 or an error code)
    return found ? num : -1; // Returns -1 if no integer was found
}
void get_ready_queue () {
    // This function should return the ready queue from the scheduler
    
        pcb_t* sourceArray = scheduler->queue(scheduler);
        for(int i = 0; i<scheduler->queue_size(scheduler); i++){
            if(i >= 3){
                printf("Error: Ready queue size exceeds maximum limit.\n");
                break;
            }
            ready_queue[i] = &sourceArray[i];
        }
    
}

void get_blocked_queue () {
    // This function should return the blocked queue from the scheduler
    int j=0;
    for (int i = 0; i < 3; i++) {
        if (processes[i].state == BLOCKED ) {
            blocked_queue[j++] = &processes[i];
        }
    }
} 

void get_running_process () {
    // This function should return the running process from the scheduler
    if(!current || current->state != RUNNING){
        printf("Error: No running process.\n");
        return;
    }
    else{
        running_process = current;
    }
}


void add_process() {
    if (num_processes >= 3) {
        printf("Maximum number of processes reached.\n");
        return;
    }
    
    char filename[MAX_LINE_LEN];
    int arrival_time;
    printf("Adding process from %s at time %d\n", filename, arrival_time);
    // Read from temporary file
    FILE* temp_file = fopen("temp_input.txt", "r");
    if (!temp_file) {
        printf("Error: Could not read input file.\n");
        return;
    }

    if (fscanf(temp_file, "%s\n%d", filename, &arrival_time) != 2) {
        printf("Error: Invalid input format.\n");
        fclose(temp_file);
        return;
    }
    fclose(temp_file);

    FILE* f = fopen(filename, "r");
    if (!f) {
        printf("Failed to open %s\n", filename);
        return;
    }

    // --- Refined Memory Allocation ---
    // 1. Count instructions first
    int instruction_count = 0;
    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), f)) {
        instruction_count++;
    }
    rewind(f); // Go back to the start of the file

    // 2. Calculate total size needed
    int num_vars = 3;
    int pcb_fields_count = 9; // pid, state, prio, pc, low, high, pcb_idx, time_in_queue, arrival_time
    int total_size = num_vars + instruction_count + pcb_fields_count;

    int mem_start_index = mem_alloc(total_size);
    if (mem_start_index < 0) {
        printf("Memory allocation failed for process (need %d words)\n", total_size);
        fclose(f);
        return;
    }

    // 3. Define memory regions
    int var_start = mem_start_index;
    int code_mem_start = var_start + num_vars;
    int pcb_start = code_mem_start + instruction_count;
    int current_idx = code_mem_start; // Start writing code instructions

    // 4. Load instructions into memory
    while (fgets(line, sizeof(line), f)) {
        if (current_idx >= pcb_start) {
            printf("Memory overflow loading %s\n", filename);
            fclose(f);
            return;
        }

        // Save each instruction as text inside memory
        mem_write(current_idx, "instruction", line); 
        current_idx++;
    }

    // 5. Setup PCB structure
    processes[num_processes].pid = extractFirstInt(filename); // Extract PID from filename
    if (processes[num_processes].pid < 0) {
        printf("Error: Invalid PID extracted from filename.\n");
        fclose(f);
        return;
    }
    processes[num_processes].state = NEW;
    processes[num_processes].priority = 0;
    processes[num_processes].pc = code_mem_start;
    processes[num_processes].mem_low = var_start;
    processes[num_processes].mem_high = var_start + total_size - 1;
    processes[num_processes].pcb_index = pcb_start;
    processes[num_processes].time_in_queue = 0;

    // 6. Write PCB and arrival time to memory
    current_idx = pcb_start;
    char *str = malloc(32);
    if (!str) {
        printf("Failed to allocate buffer for PCB writing.\n");
        exit(1);
    }

    // Write PCB fields
    snprintf(str, 32, "%d", processes[num_processes].pid);
    mem_write(current_idx++, "pid", str);

    mem_write(current_idx++, "state", state_type_to_string(processes[num_processes].state));

    memset(str, 0, 32);
    snprintf(str, 32, "%d", processes[num_processes].priority);
    mem_write(current_idx++, "priority", str);

    memset(str, 0, 32);
    snprintf(str, 32, "%d", processes[num_processes].pc);
    mem_write(current_idx++, "pc", str);

    memset(str, 0, 32);
    snprintf(str, 32, "%d", processes[num_processes].mem_low);
    mem_write(current_idx++, "mem_low", str);

    memset(str, 0, 32);
    snprintf(str, 32, "%d", processes[num_processes].mem_high);
    mem_write(current_idx++, "mem_high", str);

    memset(str, 0, 32);
    snprintf(str, 32, "%d", processes[num_processes].pcb_index);
    mem_write(current_idx++, "pcb_index", str);

    memset(str, 0, 32);
    snprintf(str, 32, "%d", processes[num_processes].time_in_queue);
    mem_write(current_idx++, "time_in_queue", str);

    memset(str, 0, 32);
    snprintf(str, 32, "%d", arrival_time);
    mem_write(current_idx++, "arrival_time", str);

    free(str);
    num_processes++;
    fclose(f);
    printf("Process added successfully.\n");
}

void choose_scheduler() {
    //should be from gui
    
    printf("Choose scheduler:\n1. FCFS\n2. RR\n3. MLFQ\nChoice: ");
    int choice, quantum;
    scanf("%d", &choice);
    while (getchar() != '\n'); // Consume trailing newline

    switch (choice) {
        case 1:
            scheduler = create_fcfs_scheduler();
            break;
        case 2:
            printf("Enter quantum: ");
            scanf("%d", &quantum);
            while (getchar() != '\n'); // Consume trailing newline
            scheduler = create_rr_scheduler(quantum);
            break;
        case 3:
            scheduler = create_mlfq_scheduler();
            break;
        default:
            printf("Invalid choice.\n");
            exit(1);
    }
    
    if (!scheduler) {
        printf("Failed to create scheduler. Out of memory.\n");
        exit(1);
    }
}

void load_program() {
    for (int i = 0; i < num_processes; i++) {
        if (processes[i].state == NEW) {
            int arrival_time = atoi(mem_read(processes[i].mem_low,processes[i].mem_high, "arrival_time"));
            if (clock_tick >= arrival_time) {
                processes[i].state = READY;
                scheduler->scheduler_enqueue(scheduler, &processes[i]);
                printf("Process %d arrived at time %d\n", processes[i].pid, clock_tick);
                char buffer[256];
                snprintf(buffer, sizeof(buffer), "Process %d arrived at time %d\n", processes[i].pid, clock_tick);
                log_message(buffer);
            }
        }
    }
}
// void load_programs() {
//     const char* filenames[] = {
//         "programs/Program_1.txt",
//         "programs/Program_2.txt",
//         "programs/Program_3.txt"
//     };

//     num_processes = 3;

//     for (int i = 0; i < num_processes; i++) {
//         FILE* f = fopen(filenames[i], "r");
//         if (!f) {
//             printf("Failed to open %s\n", filenames[i]);
//             exit(1);
//         }

//         // Estimate number of instructions first
//         int code_start = mem_alloc(20); // allocate big enough block: code + 3 vars
//         if (code_start < 0) {
//             printf("Memory allocation failed for process %d\n", i + 1);
//             exit(1);
//         }

//         int var_start = code_start;         // variables will be at the start
//         int code_mem_start = code_start + 3; // instructions start after vars

//         int current_idx = code_mem_start;
//         char line[MAX_LINE_LEN];

//         int instruction_count = 0;
//         while (fgets(line, sizeof(line), f)) {
//             if (current_idx >= MAX_MEM_WORDS) {
//                 printf("Memory overflow loading %s\n", filenames[i]);
//                 exit(1);
//             }

//             // Save each instruction as text inside memory
//             mem_write(current_idx, "instruction", line); 
//             current_idx++;
//             instruction_count++;
//         }

//         // Setup PCB
//         processes[i].pid = i + 1;
//         processes[i].state = READY;
//         processes[i].priority = 0; // MLFQ start at top level
//         processes[i].pc = code_mem_start;
//         processes[i].mem_low = var_start;
//         processes[i].pcb_index = current_idx; // where the PCB starts in memory

        

//         scheduler->scheduler_enqueue(scheduler, &processes[i]);
//         fclose(f);
//     }
// }
void simulation_step() {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "\n--- Clock Tick: %d ---\n", clock_tick);
    printf("%s", buffer);
    log_message(buffer);
    
    // First check for new processes
    load_program();
    current = scheduler->next(scheduler);
    if (!current) {
        // Check if there are any processes that are not terminated
        int has_active_processes = 0;
        for (int i = 0; i < num_processes; i++) {
            if (processes[i].state != TERMINATED) {
                has_active_processes = 1;
                break;
            }
        }
        
        if (!has_active_processes) {
            printf("All processes finished.\n");
            log_message("All processes finished.\n");
            simulation_running = 0;
        }
        clock_tick++;  // Increment clock even when no process is running
        return;
    }

    // Check if the process selected by the scheduler is actually ready/running
    if (current->state == BLOCKED || current->state == TERMINATED) {
        snprintf(buffer, sizeof(buffer), "Scheduler returned non-runnable process %d (%s). Skipping tick.\n", 
                current->pid, state_type_to_string(current->state));
        printf("%s", buffer);
        log_message(buffer);
        clock_tick++;  // Increment clock even when skipping
        return;
    }

    current->state = RUNNING;
    update_pcb_in_memory(current); // Update PCB in memory

    // --- Add PC Validity Check ---
    // Calculate code boundaries based on memory layout (assuming 3 vars)
    int code_start_index = current->mem_low + 3; 
    int code_end_index = current->pcb_index;     // Code ends strictly *before* PCB begins
    if (current->pc < code_start_index || current->pc >= code_end_index) {
        snprintf(buffer, sizeof(buffer), "Error: PC (%d) is outside valid code range [%d, %d) for PID %d. Terminating process.\n", 
                current->pc, code_start_index, code_end_index, current->pid);
        printf("%s", buffer);
        log_message(buffer);
        current->state = TERMINATED;
        update_pcb_in_memory(current); // Update state in memory
        clock_tick++;  // Increment clock even when process is terminated
        return;
    }
    
    snprintf(buffer, sizeof(buffer), "Running process %d (Priority: %d, PC: %d, State: %s)\n", 
            current->pid, current->priority, current->pc, state_type_to_string(current->state));
    printf("%s", buffer);
    log_message(buffer);
    
    // Fetch and execute one instruction
    char* instruction_string = mem_read(current->pc, current->pc, "instruction");
    if (!instruction_string) {
        snprintf(buffer, sizeof(buffer), "Error: Failed to read instruction for PID %d at PC %d. Terminating process.\n", 
                current->pid, current->pc);
        printf("%s", buffer);
        log_message(buffer);
        current->state = TERMINATED; // Mark process as terminated
        update_pcb_in_memory(current); // Update its status in memory
        clock_tick++;  // Increment clock even when process is terminated
        return;
    }
    
    snprintf(buffer, sizeof(buffer), "  Fetching instruction at mem[%d]: %s", current->pc, instruction_string);
    printf("%s", buffer);
    log_message(buffer);
    
    char instruction_copy[MAX_LINE_LEN];
    strncpy(instruction_copy, instruction_string, sizeof(instruction_copy) - 1);
    instruction_copy[sizeof(instruction_copy) - 1] = '\0';

    instruction_t* inst = parse_program(instruction_copy);
    if (!inst) {
        snprintf(buffer, sizeof(buffer), "Error: Failed to parse instruction for PID %d at PC %d: '%s'. Terminating process.\n", 
                current->pid, current->pc, instruction_copy);
        printf("%s", buffer);
        log_message(buffer);
        current->state = TERMINATED;
        update_pcb_in_memory(current);
        clock_tick++;  // Increment clock even when process is terminated
        return;
    }
    // Dispatch based on instruction type
    switch (inst->type) {
        case INST_ASSIGN: exec_assign(current, inst); break;
        case INST_PRINT: exec_print(current, inst); break;
        case INST_PRINT_FROM_TO: exec_print_from_to(current, inst); break;
        case INST_WRITE_FILE: exec_write_file(current, inst); break;
        case INST_READ_FILE: exec_read_file(current, inst); break;
        case INST_SEM_WAIT: sem_wait(inst->arg1,current,scheduler); break;
        case INST_SEM_SIGNAL: sem_signal(inst->arg1,scheduler); break;
    }

    free(inst);

    current->pc++;
    update_pcb_in_memory(current); // Update PCB in memory

    // Check if process finished
    if (current->state == BLOCKED) {
        snprintf(buffer, sizeof(buffer), "Process %d is now BLOCKED.\n", current->pid);
        printf("%s", buffer);
        log_message(buffer);
        clock_tick++;
        return;
    }
    
    // Check for process termination
    int code_end = current->pcb_index; // PCB starts right after the last instruction index
    if (current->pc >= code_end) {
        current->state = TERMINATED;
        snprintf(buffer, sizeof(buffer), "Process %d finished execution (PC %d >= Code End %d).\n", 
                current->pid, current->pc, code_end_index);
        printf("%s", buffer);
        log_message(buffer);
        update_pcb_in_memory(current); // Update state in memory
    } else {
        scheduler->preempt(scheduler, current); // If not terminated or blocked
    }
    
    clock_tick++;
}


int main(int argc, char *argv[]) {
    // Initialize subsystems
    mem_init();
    sem_init_all();

    // Initialize and run GUI
    GtkWidget *window = init_gui(argc, argv);
    run_gui();

    // Cleanup
    if (scheduler) scheduler->destroy(scheduler);
    return 0;
}