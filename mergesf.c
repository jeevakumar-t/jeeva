#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FILES 3
#define MAX_LINE_LENGTH 256

void mergeAndSortFiles(const char *outputFileName, const char *inputFileNames[], int numFiles) {
    FILE *outputFile = fopen(outputFileName, "w");
    if (!outputFile) {
        perror("Error opening output file");
        exit(EXIT_FAILURE);
    }

    FILE *inputFiles[MAX_FILES];
    for (int i = 0; i < numFiles; i++) {
        inputFiles[i] = fopen(inputFileNames[i], "r");
        if (!inputFiles[i]) {
            perror("Error opening input file");
            exit(EXIT_FAILURE);
        }
    }

    char currentLine[MAX_LINE_LENGTH];
    char *lines[MAX_FILES] = {NULL};

    // Read the initial line from each file
    for (int i = 0; i < numFiles; i++) {
        if (fgets(currentLine, sizeof(currentLine), inputFiles[i])) {
            lines[i] = strdup(currentLine);
        } else if (feof(inputFiles[i])) {
            // If a file is empty, set the line to NULL
            lines[i] = NULL;
        } else {
            perror("Error reading from input file");
            exit(EXIT_FAILURE);
        }
    }

    while (1) {
        int smallestIndex = -1;

        // Find the smallest line
        for (int i = 0; i < numFiles; i++) {
            if (lines[i] && (smallestIndex == -1 || strcmp(lines[i], lines[smallestIndex]) < 0)) {
                smallestIndex = i;
            }
        }

        // If no lines are left, break the loop
        if (smallestIndex == -1) {
            break;
        }

        // Write the smallest line to the output file
        fprintf(outputFile, "%s", lines[smallestIndex]);

        // Read the next line from the file that had the smallest line
        if (fgets(currentLine, sizeof(currentLine), inputFiles[smallestIndex])) {
            free(lines[smallestIndex]);  // Free the memory allocated for the old line
            lines[smallestIndex] = strdup(currentLine);
        } else if (feof(inputFiles[smallestIndex])) {
            // If the file is empty, set the line to NULL
            free(lines[smallestIndex]);
            lines[smallestIndex] = NULL;
        } else {
            perror("Error reading from input file");
            exit(EXIT_FAILURE);
        }
    }

    // Free memory for remaining lines
    for (int i = 0; i < numFiles; i++) {
        free(lines[i]);
    }

    // Close files
    for (int i = 0; i < numFiles; i++) {
        fclose(inputFiles[i]);
    }
    fclose(outputFile);
}

int main() {
    const char *inputFiles[] = {"file1.txt", "file2.txt", "file3.txt"};
    const char *outputFile = "merged.txt";
    int numFiles = sizeof(inputFiles) / sizeof(inputFiles[0]);

    mergeAndSortFiles(outputFile, inputFiles, numFiles);

    printf("Files merged and sorted successfully!\n");

    return 0;
}

