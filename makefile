CC = gcc
CFLAGS = -Wall -Wextra -std=c99

# Targets
TARGET_A = ta_marking_partA
TARGET_B = ta_marking_partB

# Sources
SOURCES_A = ta_marking_partA.c
SOURCES_B = ta_marking_partB.c

# Default target
all: $(TARGET_A) $(TARGET_B)

# Part A target
$(TARGET_A): $(SOURCES_A)
	$(CC) $(CFLAGS) -o $(TARGET_A) $(SOURCES_A)

# Part B target  
$(TARGET_B): $(SOURCES_B)
	$(CC) $(CFLAGS) -o $(TARGET_B) $(SOURCES_B)

# Individual build targets
partA: $(TARGET_A)

partB: $(TARGET_B)

# Create exam files
create_exams:
	chmod +x create_exams.sh
	./create_exams.sh

# Run targets (shortcut if I want to test defined number of TAs
run-partA: $(TARGET_A)
	./$(TARGET_A) 3

run-partB: $(TARGET_B)
	./$(TARGET_B) 2

# Clean all
clean:
	rm -f $(TARGET_A) $(TARGET_B) exam_*.txt

.PHONY: all partA partB create_exams run-partA run-partB clean