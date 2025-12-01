#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAX_EXAMS 100
#define RUBRIC_SIZE 5
#define MAX_LINE_LENGTH 100

// Shared memory structure
typedef struct {
    char rubric[RUBRIC_SIZE][MAX_LINE_LENGTH];  // Shared rubric data
    char current_exam[MAX_LINE_LENGTH];         // Current exam content
    int current_student_id;                     // Current student number
    int questions_marked[RUBRIC_SIZE];          // Marking status (0 for non marked and available / 1 for marked and should not be )
    int exam_finished;                          // Termination flag
    int current_exam_index;                     // Current exam position
    int total_exams;                            // Total exams (20)
} shared_data_t;

// Function to load rubric from file to shared memory
void load_rubric(shared_data_t *shared_data) {
    FILE *file = fopen("rubric.txt", "r");
    if (file == NULL) {
        perror("Failed to open rubric file");
        exit(1);
    }
    
    for (int i = 0; i < RUBRIC_SIZE; i++) {
        if (fgets(shared_data->rubric[i], MAX_LINE_LENGTH, file) == NULL) {
            break;
        }
        // Remove newline character if present
        shared_data->rubric[i][strcspn(shared_data->rubric[i], "\n")] = 0;
    }
    fclose(file);
}

// Function to load exam file
void load_exam_file(shared_data_t *shared_data, int exam_index) {
    char filename[25];  // Increased buffer size to prevent truncation
    snprintf(filename, sizeof(filename), "exam_%04d.txt", exam_index + 1);
    
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Failed to open exam file");
        exit(1);
    }
    
    if (fgets(shared_data->current_exam, MAX_LINE_LENGTH, file) == NULL) {
        perror("Failed to read exam file");
        fclose(file);
        exit(1);
    }
    fclose(file);
    
    // Extract student ID string and convert into integer
    shared_data->current_student_id = atoi(shared_data->current_exam);
    
    // Reset questions marked for new exam
    for (int i = 0; i < RUBRIC_SIZE; i++) {
        shared_data->questions_marked[i] = 0;
    }
    
    printf("\nLoaded exam: %s (Student ID: %d)\n\n", filename, shared_data->current_student_id);
}

// Function to save rubric back to file
void save_rubric(shared_data_t *shared_data) {
    FILE *file = fopen("rubric.txt", "w");
    if (file == NULL) {
        perror("Failed to open rubric file for writing");
        return;
    }
    
    for (int i = 0; i < RUBRIC_SIZE; i++) {
        fprintf(file, "%s\n", shared_data->rubric[i]);
    }
    fclose(file);
}

// Function to check and potentially correct rubric
void check_rubric(shared_data_t *shared_data, int ta_id) {
    printf("TA %d: Checking rubric...\n", ta_id);
    
    for (int i = 0; i < RUBRIC_SIZE; i++) {
        // Random delay between 0.5-1.0 seconds using usleep
        long delay_us = 500000 + (rand() % 500001);  // 500,000 to 1,000,000 microseconds
        usleep(delay_us);
        
        // Calculate thinking time in seconds for output
        double think_time = delay_us / 1000000.0;
        
        // Random decision to correct (30% chance fixed)
        int should_correct = (rand() % 100 < 30);
        
        if (should_correct) {
            // Correction needed
            char *comma_pos = strchr(shared_data->rubric[i], ',');
            //printf(*(comma_pos + 2));
            if (comma_pos != NULL && *(comma_pos + 2) != '\0') {
                char current_char = *(comma_pos + 2);
                
                // Use modulus to wrap from Z back to A
                char new_char;
                if (current_char >= 'A' && current_char <= 'Z') {
                    // Wrap around using modulus: A=65, Z=90
                    new_char = 'A' + ((current_char - 'A' + 1) % 26);
                } else {
                    // Fallback if character is outside A-Z range
                    new_char = current_char + 1;
                }
                *(comma_pos + 2) = new_char;
                
                //printf("TA %d:  Q%d: Thinks for %.1fs → Corrects! %c→%c\n", ta_id, i + 1, think_time, current_char, new_char);
                printf("TA %d: thinks for %.1fs on Q%d → Needs Correction: %c→%c\n", ta_id, think_time, i+1, current_char, new_char);

                // Save the updated rubric
                save_rubric(shared_data);
            }
        } else {
            // No correction
                printf("TA %d: thinks for %.1fs on Q%d → No Need for Correction (%d%% chance)\n", ta_id, think_time, i+1, 70);

        }
    }
}

// Function to mark questions (no synchronization - race conditions expected)
void mark_questions(shared_data_t *shared_data, int ta_id) {
    printf("TA %d: Starting to mark exam for student %d\n", ta_id, shared_data->current_student_id);
    
    // Find a question to mark (simple approach - race conditions expected)
    for (int i = 0; i < RUBRIC_SIZE; i++) {
        
        
        if (shared_data->questions_marked[i] == 0) {
            // Mark this question (race condition: multiple TAs might pick same question)
            printf("TA %d: Marking question %d for student %d\n", 
                ta_id, i + 1, shared_data->current_student_id);
            
            // Marking takes 1.0-2.0 seconds using usleep
            usleep(1000000 + (rand() % 1000001));  // 1,000,000 to 2,000,000 microseconds
            
            // Mark as completed (race condition: might overwrite other TA's work)
            shared_data->questions_marked[i] = 1;
            //questions_to_mark--;
            
            printf("TA %d: Finished marking question %d for student %d\n", 
                ta_id, i + 1, shared_data->current_student_id);
            
            break;
        }

        usleep(100000);  // 0.1 seconds = 100,000 microseconds
    }

    printf("TA %d: Completed marking all questions for student %d\n", ta_id, shared_data->current_student_id);
}

// TA process function
void ta_process(shared_data_t *shared_data, int ta_id) {  // Removed unused num_tas parameter
    srand(time(NULL) + ta_id); // Different seed for each TA
    
    while (1) {
        // Check if we should exit
        if (shared_data->exam_finished) {
            printf("TA %d: Exiting - all exams completed\n", ta_id);
            break;
        }
        
        // Check if current exam is finished by all TAs
        int all_questions_marked = 1;
        for (int i = 0; i < RUBRIC_SIZE; i++) {
            if (shared_data->questions_marked[i] == 0) {
                all_questions_marked = 0;
                break;
            }
        }
        
        if (!all_questions_marked) {
            // Check rubric
            check_rubric(shared_data, ta_id);
            
            // Mark questions
            mark_questions(shared_data, ta_id);
        }
        
        // Try to load next exam (race condition: multiple TAs might try to load next exam)
        if (shared_data->current_student_id == 9999) {
            shared_data->exam_finished = 1;  // Exam marking as concluded
            printf("TA %d: Found termination exam (9999)\n", ta_id);
            break;
        }
        
        if (all_questions_marked) {
            // Move to next exam 
            shared_data->current_exam_index++;
            if (shared_data->current_exam_index < shared_data->total_exams) {
                load_exam_file(shared_data, shared_data->current_exam_index);
            } else {
                shared_data->exam_finished = 1;
                printf("TA %d: No more exams to mark\n", ta_id);
                break;
            }
        }
        
        // Small delay to prevent tight loop
        usleep(100000);  // 0.1 seconds
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <number_of_TAs>\n", argv[0]);
        exit(1);
    }
    
    int num_tas = atoi(argv[1]);
    if (num_tas < 2) {
        printf("Number of TAs must be at least 2\n");
        exit(1);
    }
    
    printf("Starting marking system with %d TAs\n", num_tas);
    printf("NOTE: Race conditions are expected in Part A - this is normal behavior\n");
    
    // Create shared memory
    key_t key = 1234;
    int shmid = shmget(key, sizeof(shared_data_t), 0666 | IPC_CREAT);
    if (shmid == -1) {
        perror("shmget failed");
        exit(1);
    }
    
    // Attach shared memory
    shared_data_t *shared_data = (shared_data_t *)shmat(shmid, (char *)0, 0);
    if (shared_data == (void *)-1) {
        perror("shmat failed");
        exit(1);
    }
    
    // Initialize shared data
    shared_data->current_exam_index = 0;
    shared_data->total_exams = 20; // We created 20 exam files
    shared_data->exam_finished = 0;
    
    // Load initial rubric and exam
    load_rubric(shared_data);
    load_exam_file(shared_data, 0);
    
    // Create TA processes
    pid_t pids[num_tas];
    
    for (int i = 0; i < num_tas; i++) {
        pids[i] = fork();
        
        if (pids[i] == 0) {
            // Child process (TA)
            ta_process(shared_data, i + 1);
            shmdt(shared_data);
            exit(0);
        } else if (pids[i] < 0) {
            perror("fork failed");
            exit(1);
        }
    }
    
    // Parent process waits for all TAs to finish
    for (int i = 0; i < num_tas; i++) {
        waitpid(pids[i], NULL, 0);
    }
    
    // Cleanup
    shmdt(shared_data);
    shmctl(shmid, IPC_RMID, NULL);
    
    printf("All TAs have finished marking. Program completed.\n");
    return 0;
}