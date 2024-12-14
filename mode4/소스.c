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
#include <sys/time.h>

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

struct timeval tv;
double begin, end;
struct tm* tm_info;

void print_current_time_microseconds(const char* message) {
  
   
    char buffer[64];

  
    gettimeofday(&tv, NULL);
    tm_info = localtime(&tv.tv_sec);

    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    printf("[%s.%06ld] %s\n", buffer, tv.tv_usec, message);
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
    char* fifo_name;
    


    msgid = msgget((key_t)20011266, 0666 | IPC_CREAT);

    if (msgid == -1) {
        perror("메시지 큐 생성 실패");
        exit(EXIT_FAILURE);
    }
    else {
        printf("메시지 큐 생성 성공: ID = %d\n", msgid);
    }

    char server0_name[40]; // server0 파일 생성
    sprintf(server0_name, "server0.bin");
    int server0 = open(server0_name, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (server0 == -1) {
        perror("open");
        exit(1);
    }
    close(server0);

    char server1_name[40]; // server1 파일 생성
    sprintf(server1_name, "server1.bin");
    int server1 = open(server1_name, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (server1 == -1) {
        perror("open");
        exit(1);
    }
    close(server1);

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
            int file = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (file == -1) {
                perror("open");
                exit(1);
            }

            char filename_updated[40]; // client_sm(n)_updated.bin 생성
            sprintf(filename_updated, "client_sm%d_updated.bin", childSmNum);
            int file_updated = open(filename_updated, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (file_updated == -1) {
                perror("open");
                exit(1);
            }
            close(file_updated);

            for (j = 0; j < MATRIX_SIZE * MATRIX_SIZE / 8; j++) {
                read(sm[childSmNum][0], &receive, sizeof(char) * 10);
                int value = atoi(receive);
                write(file, &value, sizeof(int));
            }

            close(file);
            close(sm[childSmNum][0]);

            printf("\n--- 분할 직후 파일 저장됨: %s ---\n", filename);

            sleep(3);

            gettimeofday(&tv, NULL);//시간측정시작
            begin = (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000;
            usleep(1500000);

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
                        int rcv_file = open(rcv_filename, O_RDONLY);
                        if (rcv_file == -1) {
                            perror("open");
                            exit(1);
                        }

                        char snd_filename[30]; // client_sm(n)_updated.bin 열기
                        sprintf(snd_filename, "client_sm%d_updated.bin", k);
                        int snd_file = open(snd_filename, O_WRONLY | O_APPEND, 0666);
                        if (snd_file == -1) {
                            perror("open");
                            exit(1);
                        }

                        int buf; // snd_file에 필요한 데이터 rcv_file에서 불러온 뒤 작성
                        while (read(rcv_file, &buf, sizeof(int)) > 0) {
                            if (check_need(buf, k) == 1) {
                                write(snd_file, &buf, sizeof(int));
                            }
                        }

                        close(rcv_file);
                        close(snd_file);
                    }
                }
            }

            sleep(3);
            char filename_updated1[40]; // client_sm(n)_updated.bin 파일 열기
            sprintf(filename_updated1, "client_sm%d_updated.bin", childSmNum);
            int file_updated1 = open(filename_updated1, O_RDONLY);
            if (file_updated1 == -1) {
                perror("open updated");
                exit(1);
            }

            int buffer[2048];
            size_t num_elements = read(file_updated1, buffer, sizeof(int) * 2048) / sizeof(int);
            close(file_updated1);

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
            int file_updated2 = open(filename_updated2, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (file_updated2 == -1) {
                perror("open updated");
                exit(1);
            }
            write(file_updated2, buffer, sizeof(int) * num_elements);
            close(file_updated2);

            printf("sm%d: 정렬 완료 후 updated 파일 저장됨: %s\n", childSmNum, filename_updated2);

            gettimeofday(&tv, NULL);//시간측정 종료
            end = (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000;
            printf("Execution time sm%d : %f\n", childSmNum, (end - begin) / 1000);



           
            print_current_time_microseconds("클라이언트가 서버로 4KB씩 전송 시작시간\n");

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
            start_col = (client_id % 4) * BLOCK_SIZE;
            start_row = (client_id / 4) * BLOCK_SIZE;

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
    }
    else {
        printf("Message queue deleted successfully.\n");
    }

    return 0;
    
}
