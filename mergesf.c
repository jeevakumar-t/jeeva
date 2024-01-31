#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FILES 3
#define MAX_LINE_LENGTH 256

int compareStrings(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

void mergeAndSortFiles(const char *outputFileName, const char *inputFileNames[], int numFiles) {
    FILE *outputFile = fopen(outputFileName, "w");
    if (!outputFile) {
        perror("Error opening output file");
        exit(EXIT_FAILURE);
    }

    char *lines[MAX_FILES * MAX_LINE_LENGTH];
    size_t totalLines = 0;

    for (int i = 0; i < numFiles; i++) {
        FILE *inputFile = fopen(inputFileNames[i], "r");
        if (!inputFile) {
            perror("Error opening input file");
            exit(EXIT_FAILURE);
        }

        char currentLine[MAX_LINE_LENGTH];

        while (fgets(currentLine, sizeof(currentLine), inputFile)) {
            lines[totalLines++] = strdup(currentLine);
        }

        fclose(inputFile);
    }

    qsort(lines, totalLines, sizeof(char *), compareStrings);

    for (size_t i = 0; i < totalLines; i++) {
        fprintf(outputFile, "%s", lines[i]);
        free(lines[i]);
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

