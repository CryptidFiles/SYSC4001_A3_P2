#!/bin/bash
for i in {0001..0020}; do
    if [ $i -eq 0020 ]; then
        echo "9999" > "exam_$i.txt"
    else
        student_num=$((10#$i))  # Force base-10 interpretation
        echo "$student_num" > "exam_$i.txt"
    fi
done
echo "Exam files created successfully!"