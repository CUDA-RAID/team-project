#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>

#define NUM_PROCESSES 8
#define BUFFER_SIZE 4096

void prepare_fifo(const char *fifo_name) {
    if (access(fifo_name, F_OK) == 0) {
        printf("%s already exists. Removing it...\n", fifo_name);
        if (unlink(fifo_name) == -1) {
            perror("unlink");
            exit(1);
        }
    }

    if (mkfifo(fifo_name, 0666) == -1) {
        perror("mkfifo");
        exit(1);
    }
}

void handle_fifo_and_write(const char *fifo_name, const char *output_file) {
    int pd, fd, n;
    int buffer[BUFFER_SIZE];

    if ((pd = open(fifo_name, O_RDONLY)) == -1) {
        perror("open FIFO");
        exit(1);
    }

    if ((fd = open(output_file, O_WRONLY | O_CREAT | O_APPEND, 0666)) == -1) {
        perror("open file");
        close(pd);
        exit(1);
    }

    while ((n = read(pd, buffer, sizeof(buffer))) > 0) {
        write(fd, buffer, n);
        printf("Received %d bytes from %s and written to %s.\n", n, fifo_name, output_file);
    }


    close(fd);
    close(pd);
}
int main() {
    char fifo_name[32];
    int i;
    for (i = 0; i < NUM_PROCESSES; i++) {
        sprintf(fifo_name, "./server%d_FIFO", i);
        prepare_fifo(fifo_name);
    }

    switch (fork()) {
        case 0:
            handle_fifo_and_write("./server0_FIFO", "server0.bin");
            handle_fifo_and_write("./server1_FIFO", "server0.bin");
            handle_fifo_and_write("./server4_FIFO", "server0.bin");
            handle_fifo_and_write("./server5_FIFO", "server0.bin");
            break;
        default:
            handle_fifo_and_write("./server2_FIFO", "server0.bin");
            handle_fifo_and_write("./server3_FIFO", "server0.bin");
            handle_fifo_and_write("./server6_FIFO", "server0.bin");
            handle_fifo_and_write("./server7_FIFO", "server0.bin");
            break;
    }

    // client-server communication end ************************************************** 

    return 0;
}

