#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <string.h>
#include <time.h>

#define MAX_EXAMS 100
#define RUBRIC_SIZE 5
#define MAX_LINE_LENGTH 100

// Semaphore operations
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

// Semaphore indices
#define SEM_RUBRIC    0  // Controls rubric modification
#define SEM_QUESTIONS 1  // Controls question marking 
#define SEM_SHARED    2 // Controls general shared data access
#define NUM_SEMAPHORES 3

// Shared memory structure
typedef struct {
    char rubric[RUBRIC_SIZE][MAX_LINE_LENGTH];  // Shared rubric data
    char current_exam[MAX_LINE_LENGTH];         // Current exam content
    int current_student_id;                     // Current student number
    int questions_marked[RUBRIC_SIZE];          // Marking status (0 for non marked and available / 1 for marked and should not be )
    int exams_finished;                          // Termination flag that all exams have been completed
    int current_exam_index;                     // Current exam position
    int total_exams;                            // Total exams (20)
} shared_data_t;

// Semaphore operations
void sem_wait(int semid, int sem_num) {
    struct sembuf sb = {sem_num, -1, 0};
    semop(semid, &sb, 1);
}

void sem_signal(int semid, int sem_num) {
    struct sembuf sb = {sem_num, 1, 0};
    semop(semid, &sb, 1);
}

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
        shared_data->rubric[i][strcspn(shared_data->rubric[i], "\n")] = 0;
    }
    fclose(file);
}

// Function to load exam file - IMPROVED
void load_exam_file(shared_data_t *shared_data, int exam_index) {
    // Note: Caller should hold SEM_SHARED lock when calling this function!
    
    char filename[25];
    snprintf(filename, sizeof(filename), "exam_%04d.txt", exam_index + 1);
    
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Failed to open exam file");
        return;
    }
    
    if (fgets(shared_data->current_exam, MAX_LINE_LENGTH, file) == NULL) {
        perror("Failed to read exam file");
        fclose(file);
        return;
    }
    fclose(file);
    
    shared_data->current_student_id = atoi(shared_data->current_exam);
    
    // Reset questions marked for new exam
    for (int i = 0; i < RUBRIC_SIZE; i++) {
        shared_data->questions_marked[i] = 0;
    }
    
    printf("\nTA loaded exam: %s (Student ID: %d)\n\n", filename, shared_data->current_student_id);
}

// Function to save rubric back to file
void save_rubric(shared_data_t *shared_data, int semid) {
    sem_wait(semid, SEM_RUBRIC);  // Only one TA modifies rubric file
    
    FILE *file = fopen("rubric.txt", "w");
    if (file == NULL) {
        perror("Failed to open rubric file for writing");
        sem_signal(semid, SEM_RUBRIC);
        return;
    }
    
    for (int i = 0; i < RUBRIC_SIZE; i++) {
        fprintf(file, "%s\n", shared_data->rubric[i]);
    }
    fclose(file);
    
    sem_signal(semid, SEM_RUBRIC);
}

// Function to check and potentially correct rubric
void check_rubric(shared_data_t *shared_data, int ta_id, int semid) {
    printf("TA %d: Checking rubric...\n", ta_id);
    
    for (int i = 0; i < RUBRIC_SIZE; i++) {
        // Random delay between 0.5-1.0 seconds using sleep
        double delay_seconds = 0.5 + (rand() % 501) / 1000.0;  // 0.5 to 1.0 seconds
        sleep(delay_seconds);  // Use sleep instead of usleep

        // Calculate thinking time in seconds for output (already have it!)
        double think_time = delay_seconds;
        
        // Random decision to correct (30% chance fixed)
        int should_correct = (rand() % 100 < 30);
        
        if (should_correct) {
            sem_wait(semid, SEM_QUESTIONS);  // Lock rubric for modification
            
            char *comma_pos = strchr(shared_data->rubric[i], ',');
            if (comma_pos != NULL && *(comma_pos + 2) != '\0') {
                char current_char = *(comma_pos + 2);
                
                char new_char;
                if (current_char >= 'A' && current_char <= 'Z') {
                    new_char = 'A' + ((current_char - 'A' + 1) % 26);
                } else {
                    new_char = current_char + 1;
                }
                *(comma_pos + 2) = new_char;
                
                printf("TA %d: thinks for %.1fs on Q%d → Corrects: %c→%c\n", 
                       ta_id, think_time, i+1, current_char, new_char);

                save_rubric(shared_data, semid);
            }
            
            sem_signal(semid, SEM_QUESTIONS);  // Release rubric lock
        } else {
            printf("TA %d: thinks for %.1fs on Q%d → No Correction Needed\n", 
                   ta_id, think_time, i+1);
        }
    }
}

// Function to mark questions with synchronization 
void mark_questions(shared_data_t *shared_data, int ta_id, int semid) {
    
    // CAPTURE student ID at start
    sem_wait(semid, SEM_SHARED);
    int captured_student_id = shared_data->current_student_id;  // Save it!
    sem_signal(semid, SEM_SHARED);

    printf("TA %d: Starting to mark exam for student %d\n", ta_id, captured_student_id);
    
    int marked_any = 0;
    
    for (int attempt = 0; attempt < RUBRIC_SIZE; attempt++) {  // Limit attempts to prevent infinite loop
        sem_wait(semid, SEM_SHARED);
        
        // Find an unmarked question
        int question_to_mark = -1;
        for (int i = 0; i < RUBRIC_SIZE; i++) {
            if (shared_data->questions_marked[i] == 0) {
                question_to_mark = i;
                shared_data->questions_marked[i] = 1;  // Mark as in progress
                marked_any = 1;
                break;
            }
        }
        
        sem_signal(semid, SEM_SHARED);
        
        if (question_to_mark == -1) {
            // No more questions to mark
            if (!marked_any) {
                printf("TA %d: No questions available to mark for student %d\n", 
                       ta_id, captured_student_id);
            }
            break;
        }
        
        // Mark the question
        printf("TA %d: Marking question %d for student %d\n", 
               ta_id, question_to_mark + 1, captured_student_id);  // Use captured ID
        
        //usleep(1000000 + (rand() % 1000001));  // 1.0-2.0 seconds for marking
        sleep(1 + rand() % 2);
        
        printf("TA %d: Finished marking question %d for student %d\n", 
               ta_id, question_to_mark + 1, captured_student_id);  // Use captured ID
        
        usleep(100000);  // Small delay
    }
    
    if (marked_any) {
        printf("TA %d: Completed marking questions for student %d\n", ta_id, captured_student_id);
    }
}

// TA process function - PROPERLY FIXED
void ta_process(shared_data_t *shared_data, int ta_id, int semid) {
    srand(time(NULL) + ta_id);
    
    while (1) {       
        // Brief check for termination
        sem_wait(semid, SEM_SHARED);
        if (shared_data->exams_finished) {
            sem_signal(semid, SEM_SHARED);
            printf("TA %d: Exiting - all exams completed\n", ta_id);
            break;
        }
        sem_signal(semid, SEM_SHARED);

        
        // Check if we need to load next exam if all questions have been marked
        // Ensures TAs arent loading next exam txt file at the same time and corrupting it
        sem_wait(semid, SEM_SHARED);

        // Before even performing any activities on the exam, check if we need have reached
        if (shared_data->current_student_id == 9999) {
            shared_data->exams_finished = 1;  // Exam marking as concluded
            printf("TA %d: Found termination exam (9999)\n", ta_id);
            sem_signal(semid, SEM_SHARED);
            break;
        }

        // Re-check termination inside critical section if break fails?
        if (shared_data->exams_finished) {
            sem_signal(semid, SEM_SHARED);
            break;
        }

       // Check if current exam still needs questions to be marked
        int all_questions_marked = 1;
        for (int i = 0; i < RUBRIC_SIZE; i++) {
            if (shared_data->questions_marked[i] == 0) {
                all_questions_marked = 0;
                break;
            }
        }

        // If all questions answered, check if we move to the next exam or break since we are finished
        if (all_questions_marked) {
            // Move to next exam 
            shared_data->current_exam_index++;
            if (shared_data->current_exam_index < shared_data->total_exams) {
                load_exam_file(shared_data, shared_data->current_exam_index);
                sem_signal(semid, SEM_SHARED);
                continue;
            } else {
                shared_data->exams_finished = 1;
                printf("TA %d: No more exams to mark\n", ta_id);
                sem_signal(semid, SEM_SHARED);
                break;
            }
        }
        sem_signal(semid, SEM_SHARED); // release lock for actual work

        // Check conditions AFTER releasing lock
        if (shared_data->exams_finished || shared_data->current_student_id == 9999) {
            break;
        }


        // Check rubric
        check_rubric(shared_data, ta_id, semid);
            
        // Mark questions
        mark_questions(shared_data, ta_id, semid);
        
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
    
    printf("Starting synchronized marking system with %d TAs\n", num_tas);
    
    // Create semaphores
    key_t sem_key = 1235;
    int semid = semget(sem_key, NUM_SEMAPHORES, 0666 | IPC_CREAT);
    if (semid == -1) {
        perror("semget failed");
        exit(1);
    }
    
    // Initialize semaphores
    union semun arg;
    unsigned short values[NUM_SEMAPHORES] = {1, 1, 1};  // All binary semaphores
    arg.array = values;
    if (semctl(semid, 0, SETALL, arg) == -1) {
        perror("semctl SETALL failed");
        exit(1);
    }
    
    // Create shared memory
    key_t shm_key = 1234;
    int shmid = shmget(shm_key, sizeof(shared_data_t), 0666 | IPC_CREAT);
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
    shared_data->total_exams = 20;
    shared_data->exams_finished = 0;
    
    // Load initial rubric and exam
    load_rubric(shared_data);
    load_exam_file(shared_data, 0);
    
    // Create TA processes
    pid_t pids[num_tas];
    
    for (int i = 0; i < num_tas; i++) {
        pids[i] = fork();
        
        if (pids[i] == 0) {
            // Child process (TA)
            ta_process(shared_data, i + 1, semid);
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
    
    // Remove semaphores
    semctl(semid, 0, IPC_RMID);
    
    printf("All TAs have finished marking. Program completed.\n");
    return 0;
}