// word_counter_windows.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <time.h>
#include <ctype.h>

#define MAX_WORD_LENGTH 100
#define MAX_WORDS 1000
#define MAX_THREADS 4
#define CHUNK_SIZE 1024
#define PIPE_BUFFER_SIZE 4096

// Structure to hold thread arguments
typedef struct {
    FILE* file;
    long start_pos;
    long chunk_size;
    char target_word[MAX_WORD_LENGTH];
    int thread_id;
} ThreadArgs;

// Structure for pipe communication
typedef struct {
    char word[MAX_WORD_LENGTH];
    int count;
} WordCount;

// Global variables (keeping the beginner style)
HANDLE g_mutex;
int total_count = 0;

// Function to count words in a chunk of file
DWORD WINAPI CountWordsInChunk(LPVOID lpParam) {
    ThreadArgs* args = (ThreadArgs*)lpParam;
    char buffer[CHUNK_SIZE];
    char word[MAX_WORD_LENGTH];
    int local_count = 0;
    int i = 0;
    
    // Seek to starting position
    fseek(args->file, args->start_pos, SEEK_SET);
    
    // Read chunk
    size_t bytes_read = fread(buffer, 1, args->chunk_size, args->file);
    buffer[bytes_read] = '\0';
    
    // Count words (simplified approach)
    char* word_start = buffer;
    while (*word_start) {
        if (isalpha(*word_start)) {
            i = 0;
            while (isalpha(*word_start) && i < MAX_WORD_LENGTH - 1) {
                word[i++] = tolower(*word_start++);
            }
            word[i] = '\0';
            
            if (strcmp(word, args->target_word) == 0) {
                local_count++;
            }
        } else {
            word_start++;
        }
    }
    
    // Update global count (with mutex)
    WaitForSingleObject(g_mutex, INFINITE);
    total_count += local_count;
    ReleaseMutex(g_mutex);
    
    return 0;
}

// Process a single file
int ProcessFile(const char* filename, const char* target_word, HANDLE pipe) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error opening file: %s\n", filename);
        return -1;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Create threads
    HANDLE threads[MAX_THREADS];
    ThreadArgs thread_args[MAX_THREADS];
    long chunk_size = file_size / MAX_THREADS;
    
    // Initialize and create threads
    for (int i = 0; i < MAX_THREADS; i++) {
        thread_args[i].file = file;
        thread_args[i].start_pos = i * chunk_size;
        thread_args[i].chunk_size = (i == MAX_THREADS - 1) ? file_size - (i * chunk_size) : chunk_size;
        strcpy(thread_args[i].target_word, target_word);
        thread_args[i].thread_id = i;
        
        threads[i] = CreateThread(
            NULL,                   // default security attributes
            0,                      // default stack size
            CountWordsInChunk,      // thread function
            &thread_args[i],        // argument to thread function
            0,                      // default creation flags
            NULL                    // receive thread identifier
        );
        
        if (threads[i] == NULL) {
            printf("Error creating thread %d\n", i);
            return -1;
        }
    }
    
    // Wait for all threads
    WaitForMultipleObjects(MAX_THREADS, threads, TRUE, INFINITE);
    
    // Close thread handles
    for (int i = 0; i < MAX_THREADS; i++) {
        CloseHandle(threads[i]);
    }
    
    // Send result via pipe
    WordCount result;
    strcpy(result.word, target_word);
    result.count = total_count;
    DWORD bytes_written;
    WriteFile(pipe, &result, sizeof(WordCount), &bytes_written, NULL);
    
    fclose(file);
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage: %s <target_word> <file1> [file2 ...]\n", argv[0]);
        return 1;
    }
    
    char* target_word = argv[1];
    int num_files = argc - 2;
    
    // Create mutex
    g_mutex = CreateMutex(NULL, FALSE, NULL);
    if (g_mutex == NULL) {
        printf("Error creating mutex\n");
        return 1;
    }
    
    clock_t start = clock();
    
    // Process each file
    HANDLE* child_processes = malloc(num_files * sizeof(HANDLE));
    HANDLE* pipes = malloc(num_files * sizeof(HANDLE));
    
    for (int i = 0; i < num_files; i++) {
        // Create pipe
        HANDLE read_pipe, write_pipe;
        SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
        
        if (!CreatePipe(&read_pipe, &write_pipe, &sa, PIPE_BUFFER_SIZE)) {
            printf("Error creating pipe\n");
            return 1;
        }
        
        pipes[i] = read_pipe;
        
        // Create process
        STARTUPINFO si = {sizeof(STARTUPINFO)};
        PROCESS_INFORMATION pi;
        char cmd[MAX_PATH * 2];
        
        sprintf(cmd, "%s %s %s %d", argv[0], target_word, argv[i + 2], (int)write_pipe);
        
        if (!CreateProcess(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
            printf("Error creating process for file %s\n", argv[i + 2]);
            return 1;
        }
        
        child_processes[i] = pi.hProcess;
        CloseHandle(pi.hThread);
        CloseHandle(write_pipe);
    }
    
    // Wait for all processes and collect results
    int grand_total = 0;
    for (int i = 0; i < num_files; i++) {
        WaitForSingleObject(child_processes[i], INFINITE);
        
        WordCount result;
        DWORD bytes_read;
        ReadFile(pipes[i], &result, sizeof(WordCount), &bytes_read, NULL);
        
        grand_total += result.count;
        printf("File %d count for '%s': %d\n", i + 1, result.word, result.count);
        
        CloseHandle(child_processes[i]);
        CloseHandle(pipes[i]);
    }
    
    clock_t end = clock();
    double cpu_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    printf("\nTotal occurrences of '%s': %d\n", target_word, grand_total);
    printf("Total CPU time: %f seconds\n", cpu_time);
    
    // Cleanup
    CloseHandle(g_mutex);
    free(child_processes);
    free(pipes);
    
    return 0;
}