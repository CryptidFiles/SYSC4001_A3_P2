CC = gcc
CFLAGS = -Wall -Wextra -std=c99
TARGET = ta_marking_partA
SOURCES = ta_marking_partA.c

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

create_exams:
	chmod +x create_exams.sh
	./create_exams.sh

clean:
	rm -f $(TARGET) exam_*.txt

.PHONY: all create_exams clean