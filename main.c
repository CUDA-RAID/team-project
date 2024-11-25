#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/shm.h>
#include <string.h>
#include <time.h>

#define MATRIX_SIZE 128
#define CLIENTS 8
#define BLOCK_SIZE 16
#define BLOCKS_PER_SM 8
#define BLOCK_BYTES (BLOCK_SIZE * BLOCK_SIZE)

//128 x 128 배열 생성 함수
void generate_matrix(int matrix[MATRIX_SIZE][MATRIX_SIZE]) {
    int value = 0;
    int i, j;
    for (i = 0; i < MATRIX_SIZE; i++) {
        for (j = 0; j < MATRIX_SIZE; j++) {
            matrix[i][j] = value++;
        }
    }
}

int main(void) {
    pid_t pid;
    int matrix[MATRIX_SIZE][MATRIX_SIZE];

    // mkfifo(".PROJECT-FIFO", 0666);
    
    int i, j, k;
    int sm[8][2];
    int childSmNum;
    pid_t parent_pid;
    parent_pid = getpid();

    for ( i = 0; i < 8; i++ ) {
        pipe(sm[i]);
    }

    generate_matrix(matrix);//128 x 128배열 생성
    ///
    //sm0~7생성

    //생성한 sm으로 분배를 해 버리면

    for ( i = 0; i < 8; i++ ) {
        switch(pid = fork()) {
            case -1:
                perror("fork");
                exit(1);

            case 0:
                childSmNum = i;
                
                // pipe usage sm[i][0] or sm[i][1];

                break;


            default:
                break;

        }
    }

    if ( parent_pid == getpid() ) { // 부모 프로세스에서만 동작

        for( i = 0; i < 8; i++ ) {
           close(sm[i][0]); // 모든 자식의 읽기 닫기
        }
        int client_id, block_id, start_row, start_col;
        for ( client_id = 0; client_id < CLIENTS; client_id++ ) {
            

            start_col = client_id * BLOCK_SIZE;
            start_row = 0;

            for ( k = 0; k < CLIENTS; k++ ) {
                

                for (i = 0; i < BLOCK_SIZE; i++) {
                    for (j = 0; j < BLOCK_SIZE; j++) {
                        int row = start_row + i;
                        int col = start_col + j;
                        //fwrite(&matrix[row][col], sizeof(int), 1, client_file);
                        char matrixChar[10];
                        sprintf(matrixChar, "%d", matrix[row][col]);
                        //printf("%s ", matrixChar);
                        write(sm[client_id][1], matrixChar, sizeof(char) * 10);
                    }
                }
                printf("\n");
                start_row += BLOCK_SIZE;
            }
            
        }
    }
     else { // 자식 프로세스에서만 동작
        close(sm[childSmNum][1]); // 모든 자식의 쓰기 닫기 
        char receive[10];
        int resData = 0;
        sleep(childSmNum*3);
        printf("childNum : %d\n", childSmNum);
        for ( i = 0; i < MATRIX_SIZE * MATRIX_SIZE / 8; i++ ) {
            read(sm[childSmNum][0], &receive, sizeof(char) * 10);
            printf(" %s",receive);
            if(i%16 == 0 && i != 0)printf("\n");
        }

        //printf("childNum %d : %d\n", childSmNum, resData);
    }
    
    
    //여기서 분배된 데이터를 저장


    //sm0~7 데이터 출력





    //sm끼리 통신하여 연속된 정수 형태로 저장

    //sm0~7 데이터 출력

    //서버로 데이터 송신

    //서버 데이터 출력


    return 0;
}
