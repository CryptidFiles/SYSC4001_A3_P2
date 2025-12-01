# README: Exam Marking System with Process Synchronization

## Overview
This project implements a distributed exam marking system using shared memory and process synchronization. The system simulates multiple Teaching Assistants (TAs) working concurrently to mark student exams, with Part A demonstrating race conditions and Part B implementing proper synchronization using semaphores.

## Project Structure
```
.
├── create_exams.sh              # Script to generate exam files
├── ta_marking_partA_101299776_101287534.c  # Part A: Race condition demo
├── ta_marking_partB_101299776_101287534.c  # Part B: Synchronized version
├── rubric.txt                   # Rubric data file
└── Makefile                     # Build automation
```

## Prerequisites
- GCC compiler
- Linux/Unix environment
- Basic C development tools


### Using Makefile (Recommended)
```bash
# Build both Part A and Part B (compilation only)
make all

# Build only Part A (compilation only)
make partA

# Build only Part B (compilation only)
make partB

# Create exam files (required before running)
make create_exams

# Remove executables and generated files
make clean
```

## Running the Programs

### Part A: Race Condition Demonstration
Part A intentionally demonstrates race conditions where multiple TAs can:
- Mark the same question simultaneously
- Corrupt shared data
- Load exams concurrently

```bash
# Run with n TAs (minimum 2 required)
./ta_partA n
```

### Part B: Synchronized Implementation
Part B uses semaphores to prevent race conditions and ensure proper synchronization:

```bash
# Run with n TAs (minimum 2 required)
./ta_partB n

```

## Test Cases

### Test Case 1: Basic Functionality
```bash
make create_exams
make partA
./ta_partA 2
```

### Test Case 2: Multiple TAs
```bash
make create_exams
make partB
./ta_partB 5
```

### Test Case 3: Edge Cases
```bash
# Invalid number of TAs (should show error message "Number of TAs must be at least 2" and exit)
./ta_partA 1

# Large number of TAs
./ta_partB 10
```

```

For any issues, ensure all IPC resources are cleared and rebuild from scratch.
