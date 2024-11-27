#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/shm.h>
#include <string.h>
#include <time.h>
#include <fcntl.h> 

#define MATRIX_SIZE 128
#define CLIENTS 8
#define BLOCK_SIZE 16
#define BLOCKS_PER_SM 8
#define BLOCK_BYTES (BLOCK_SIZE * BLOCK_SIZE)
#define DATA_PER_SM (MATRIX_SIZE * MATRIX_SIZE / CLIENTS)
#define FIFO_TEMPLATE "sm%d_fifo"

void generate_matrix(int matrix[MATRIX_SIZE][MATRIX_SIZE]) {
    int value = 0;
    int i, j;
    for (i = 0; i < MATRIX_SIZE; i++) {
        for (j = 0; j < MATRIX_SIZE; j++) {
            matrix[i][j] = value++;
        }
    }
}

void create_fifo(const char* path) {
    if (unlink(path) == 0) {
        printf("Removed existing FIFO: %s\n", path);
    }

    if (mkfifo(path, 0666) == -1) {
        perror("mkfifo");
        exit(1);
    }
}

void read_fifo(const char* path, char* buffer, size_t size) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        perror("open");
        exit(1);
    }

    read(fd, buffer, size);
    close(fd);
}

void write_fifo(const char* path, const char* message) {
    int fd = open(path, O_WRONLY);
    if (fd == -1) {
        perror("open");
        exit(1);
    }
    write(fd, message, strlen(message) + 1); // Null-terminator 포함
    close(fd);
}



int main(void) {
    pid_t pid;
    int matrix[MATRIX_SIZE][MATRIX_SIZE];
    int i, j, k;
    int sm[8][2];
    int childSmNum;
    pid_t parent_pid;
    parent_pid = getpid();
    char fifo_paths[CLIENTS][20];

    // 이름있는 파이프 생성
    for (i = 0; i < CLIENTS; i++) {
        sprintf(fifo_paths[i], FIFO_TEMPLATE, i);
        create_fifo(fifo_paths[i]);
    }

    for (i = 0; i < 8; i++) {
        pipe(sm[i]);
    }

    generate_matrix(matrix);

    for (i = 0; i < 8; i++) {
        switch (pid = fork()) {
        case -1:
            perror("fork");
            exit(1);

        case 0:
            childSmNum = i;
            sleep(childSmNum);
            close(sm[childSmNum][1]);
            char receive[10];
            char filename[30];
            sprintf(filename, "client_sm_%d.bin", childSmNum);
            FILE* file = fopen(filename, "wb");
            if (file == NULL) {
                perror("fopen");
                exit(1);
            }

            //연속된 형태로 저장하기 위한 새로운 파일 생성
            char filename_updated[40];
            sprintf(filename_updated, "client_sm%d_updated.bin", childSmNum);
            FILE* file_updated = fopen(filename_updated, "wb");
            if (file_updated == NULL) {
                perror("fopen");
                exit(1);
            }

            for (j = 0; j < MATRIX_SIZE * MATRIX_SIZE / 8; j++) {
                read(sm[childSmNum][0], &receive, sizeof(char) * 10);
                int value = atoi(receive);
                fwrite(&value, sizeof(int), 1, file);
            }
            
            fclose(file);
            close(sm[childSmNum][0]);

            
            char command[50];
            sprintf(command, "od -t d4 %s", filename);
            printf("\n--- 분할 직후 파일 저장됨: %s ---\n", filename);
            if (childSmNum == 0) {
                system(command);
            }

            
         
            //sm7까지 생성될때까지 대기
            sleep(8);


            // 다른 SM에게 요청
            int target_sm;
            for (target_sm = 0; target_sm < CLIENTS; target_sm++) {
                if (target_sm == childSmNum) {
                    continue; // 자기 자신에게 요청하지 않음
                }
                
                // 요청 메시지 전송
                char request_message[20];
                sprintf(request_message, "sm%d", childSmNum);
                
                printf("\n--- sm%d이 sm%d에게 원하는 정수 요청중---\n", childSmNum, target_sm);
                write_fifo(fifo_paths[target_sm], request_message);
                printf("\n--- sm%d이 sm%d에게 원하는 정수 요청 완료---\n", childSmNum, target_sm);
                
            }
            
            sleep(8);
            // 요청 수신 및 응답
            int receive_i;
            for (receive_i = 0; receive_i < 7; receive_i++) { 
                
                char request[20];
                read_fifo(fifo_paths[childSmNum], request, sizeof(request));

                if (strncmp(request, "sm", 2) == 0) {
                    int target_sm = atoi(&request[2]);
                    int data_to_send[DATA_PER_SM];
                    int count = 0;
                    int start_range = target_sm * DATA_PER_SM;
                    int end_range = start_range + DATA_PER_SM - 1;

                    // 필요한 데이터 검색
                    file = fopen(filename_updated, "rb");
                    if (file == NULL) {
                        perror("fopen");
                        exit(1);
                    }
                    int value;
                    while (fread(&value, sizeof(int), 1, file)) {
                        if (value >= start_range && value <= end_range) {
                            data_to_send[count++] = value;
                        }
                    }
                    fclose(file);

                    // 요청한 SM의 연속된 데이터 파일 열기
                    char target_filename[40];
                    sprintf(target_filename, "client_sm%d_updated.bin", target_sm);
                    FILE* target_file = fopen(target_filename, "ab");
                    if (target_file == NULL) {
                        perror("fopen");
                        exit(1);
                    }

                    // 데이터 추가
                    fwrite(data_to_send, sizeof(int), count, target_file);
                    fclose(target_file);
                }

            }
            sleep(10);

            exit(0);

        default:
            break;
        }
    }

    if (parent_pid == getpid()) {
        for (i = 0; i < 8; i++) {
            close(sm[i][0]);
        }
        int client_id, start_row, start_col;
        for (client_id = 0; client_id < CLIENTS; client_id++) {
            start_col = client_id * BLOCK_SIZE;
            start_row = 0;

            for (k = 0; k < CLIENTS; k++) {
                for (i = 0; i < BLOCK_SIZE; i++) {
                    for (j = 0; j < BLOCK_SIZE; j++) {
                        int row = start_row + i;
                        int col = start_col + j;
                        char matrixChar[10];
                        sprintf(matrixChar, "%d", matrix[row][col]);
                        write(sm[client_id][1], matrixChar, sizeof(char) * 10);
                    }
                }
                start_row += BLOCK_SIZE;
            }
        }

        for (i = 0; i < 8; i++) {
            close(sm[i][1]);
        }


        for (i = 0; i < CLIENTS; i++) {
            wait(NULL);
        }
    }

    return 0;
}

