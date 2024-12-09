#include <sys/types.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MATRIX_SIZE 128
#define CLIENTS 8
#define BLOCK_SIZE 32
#define BLOCKS_PER_SM 8
#define BLOCK_BYTES (BLOCK_SIZE * BLOCK_SIZE)
#define DATA_PER_SM (MATRIX_SIZE * MATRIX_SIZE / CLIENTS)
#define MAX_TEXT 256

struct message {
    long mtype;
    char mtext[MAX_TEXT];
};

void generate_matrix(int matrix[MATRIX_SIZE][MATRIX_SIZE]) {
    int value = 0;
    int i, j;
    for (i = 0; i < MATRIX_SIZE; i++) {
        for (j = 0; j < MATRIX_SIZE; j++) {
            matrix[i][j] = value++;
        }
    }
}

int check_need(int buf, int k) {
    int startNum, endNum;
    startNum = 2048 * k;
    endNum = 2048 * (k + 1) - 1;

    if (startNum <= buf && buf <= endNum) return 1;
    return 0;
}

void selection_sort(int arr[], int n) {
    int i, j, min_idx;
    for (i = 0; i < n - 1; i++) {
        min_idx = i;
        for (j = i + 1; j < n; j++) {
            if (arr[j] < arr[min_idx]) {
                min_idx = j;
            }
        }
        int temp = arr[min_idx];
        arr[min_idx] = arr[i];
        arr[i] = temp;
    }
}

int main(void) {
    pid_t pid;
    int matrix[MATRIX_SIZE][MATRIX_SIZE];
    int pd, sm[8][2];
    int msgid;
    struct message msg;
    pid_t parent_pid = getpid();
    int i, j, k;
    int childSmNum;
    char *fifo_name;

    msgid = msgget((key_t)1505, 0666 | IPC_CREAT);

    if (msgid == -1) {
        perror("메시지 큐 생성 실패");
        exit(EXIT_FAILURE);
    } else {
        printf("메시지 큐 생성 성공: ID = %d\n", msgid);
    }

    char server0_name[40]; // server0 파일 생성
    sprintf(server0_name, "server0.bin");
    FILE* server0 = fopen(server0_name, "wb");
    if (server0 == NULL) {
        perror("fopen");
        exit(1);
    }
    fclose(server0);
    char server1_name[40]; // server1 파일 생성
    sprintf(server1_name, "server1.bin");
    FILE* server1 = fopen(server1_name, "wb");
    if (server1 == NULL) {
        perror("fopen");
        exit(1);
    }
    fclose(server1);

    for (i = 0; i < 8; i++) {
        pipe(sm[i]);
    }

    generate_matrix(matrix);


    for (i = 0; i < 8; i++) {
        switch (pid = fork()) {
        case -1:
            perror("fork");
            exit(1);
        case 0: {
            childSmNum = i;

            close(sm[childSmNum][1]);
            char receive[10];

            char filename[30]; // client_sm(n).bin 생성
            sprintf(filename, "client_sm%d.bin", childSmNum);
            FILE* file = fopen(filename, "wb");
            if (file == NULL) {
                perror("fopen");
                exit(1);
            }

            char filename_updated[40]; // client_sm(n)_updated.bin 생성
            sprintf(filename_updated, "client_sm%d_updated.bin", childSmNum);
            FILE* file_updated = fopen(filename_updated, "wb");
            if (file_updated == NULL) {
                perror("fopen");
                exit(1);
            }
            fclose(file_updated);

            for (j = 0; j < MATRIX_SIZE * MATRIX_SIZE / 8; j++) {
                read(sm[childSmNum][0], &receive, sizeof(char) * 10);
                int value = atoi(receive);
                fwrite(&value, sizeof(int), 1, file);
            }

            fclose(file);
            close(sm[childSmNum][0]);

            printf("\n--- 분할 직후 파일 저장됨: %s ---\n", filename);

            sleep(5);

            // client-client communication start ************************************************** 

            char request[10];
            int target_sm_i;
            for (target_sm_i = 0; target_sm_i < 8; target_sm_i++) {
                sprintf(request, "sm%d", childSmNum);
                strcpy(msg.mtext, request);
                msg.mtype = target_sm_i + 1;
                if (msgsnd(msgid, (void*)&msg, strlen(msg.mtext), 0) == -1) {
                    perror("msgsnd failed");
                    exit(EXIT_FAILURE);
                }
                printf("%s요청보냄 : %s -> sm%d\n", msg.mtext, msg.mtext, (int)msg.mtype - 1);
            }

            sleep(3);

            int rcv_i;
            for (rcv_i = 0; rcv_i < 8; rcv_i++) {
                if (msgrcv(msgid, (void*)&msg, sizeof(msg.mtext), childSmNum + 1, 0) == -1) {
                    perror("메시지 못받음\n");
                    exit(1);
                }
                printf("%s요청받음 : %s -> sm%d\n", msg.mtext, msg.mtext, (int)msg.mtype - 1);

                char rcv_msg[10];
                int k;
                for (k = 0; k < 8; k++) {
                    sprintf(rcv_msg, "sm%d", k);
                    if (strcmp(msg.mtext, rcv_msg) == 0) {

                        char rcv_filename[30]; // client_sm(n).bin 열기
                        sprintf(rcv_filename, "client_sm%d.bin", childSmNum);
                        FILE* rcv_file = fopen(rcv_filename, "rb");
                        if (rcv_file == NULL) {
                            perror("fopen");
                            exit(1);
                        }

                        char snd_filename[30]; // client_sm(n)_updated.bin 열기
                        sprintf(snd_filename, "client_sm%d_updated.bin", k);
                        FILE* snd_file = fopen(snd_filename, "ab");
                        if (snd_file == NULL) {
                            perror("fopen");
                            exit(1);
                        }

                        int buf; // snd_file에 필요한 데이터 rcv_file에서 불러온 뒤 작성
                        while (fread(&buf, sizeof(int), 1, rcv_file)) {
                            if (check_need(buf, k) == 1) {
                                fwrite(&buf, sizeof(int), 1, snd_file);
                            }
                        }

                        fclose(rcv_file);
                        fclose(snd_file);
                    }
                }
            }

            sleep(3);
            char filename_updated1[40]; // client_sm(n)_updated.bin 파일 열기
            sprintf(filename_updated1, "client_sm%d_updated.bin", childSmNum);
            FILE* file_updated1 = fopen(filename_updated1, "rb");
            if (file_updated1 == NULL) {
                perror("fopen updated");
                exit(1);
            }

            int buffer[2048];
            size_t num_elements = fread(buffer, sizeof(int), 2048, file_updated1);
            fclose(file_updated1);

            if (num_elements == 0) {
                printf("sm%d: updated 파일이 비어 있습니다.\n", childSmNum);
                return -1;
            }
            selection_sort(buffer, num_elements); // 두 버퍼에 나누어 저장
            int half_buffer_1[1024];
            int half_buffer_2[1024];
            int buffer_i;
            for (buffer_i = 0; buffer_i < 1024; buffer_i++) {
                half_buffer_1[buffer_i] = buffer[buffer_i];
            }
            for (buffer_i = 1024; buffer_i < 2048; buffer_i++) {
                half_buffer_2[buffer_i - 1024] = buffer[buffer_i];
            }

            char filename_updated2[40];
            sprintf(filename_updated2, "client_sm%d_updated.bin", childSmNum);
            FILE* file_updated2 = fopen(filename_updated2, "wb");
            if (file_updated2 == NULL) {
                perror("fopen updated");
                exit(1);
            }
            fwrite(buffer, sizeof(int), num_elements, file_updated2);
            fclose(file_updated2);

            printf("sm%d: 정렬 완료 후 updated 파일 저장됨: %s\n", childSmNum, filename_updated2);

            // client-client communication finish *************************************************        

            // client-server communication start ************************************************** 
            sprintf(fifo_name, "./server%d_FIFO", childSmNum);

            pd = open(fifo_name, O_WRONLY);

            printf("sm%d : 서버로 파이프를 통해 4KB씩 전송 중\n", childSmNum);
            write(pd, half_buffer_1, sizeof(half_buffer_1));
            sleep(1);
            write(pd, half_buffer_2, sizeof(half_buffer_2));
            printf("sm%d : 서버로 파이프를 통해 4KB씩 전송 완료\n", childSmNum);
            close(pd);

            exit(0);
        }
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
            start_col = (client_id % 4)*BLOCK_SIZE;
            start_row = (client_id / 4)*BLOCK_SIZE;

            for (k = 0; k < 2; k++) {
                for (i = 0; i < BLOCK_SIZE; i++) {
                    for (j = 0; j < BLOCK_SIZE; j++) {
                        int row = start_row + i;
                        int col = start_col + j;
                        char matrixChar[10];
                        sprintf(matrixChar, "%d", matrix[row][col]);
                        write(sm[client_id][1], matrixChar, sizeof(char) * 10);
                    }
                }
                start_row += BLOCK_SIZE * 2;
            }
        }
        for (i = 0; i < 8; i++) {
            close(sm[i][1]);
        }


        for (i = 0; i < CLIENTS; i++) {
            wait(NULL);
        }
    }


    if (msgctl(msgid, IPC_RMID, NULL) == -1) {
        perror("msgctl failed");
        exit(EXIT_FAILURE);
    } else {
        printf("Message queue deleted successfully.\n");
    }

    return 0;
}
                                                       

