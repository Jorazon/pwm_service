#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/pwm_service.sock"
#define MAX_MSG_LEN 256

struct pwm_request {
    int pin;
    int duty_cycle;
    int frequency;
};

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <pin> <duty_cycle 0..1023> <frequency>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Parse arguments
    struct pwm_request req;
    req.pin = atoi(argv[1]);
    req.duty_cycle = atoi(argv[2]);
    req.frequency = atoi(argv[3]);

    // Create socket
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket create");
        exit(EXIT_FAILURE);
    }

    // Connect to daemon
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Socket connect");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Send request
    if (send(sock, &req, sizeof(req), 0) < 0) {
        perror("Socket send");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Receive response
    char buffer[MAX_MSG_LEN];
    ssize_t bytes = recv(sock, buffer, MAX_MSG_LEN - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        printf("%s\n", buffer);
    } else {
        perror("nothing received from socket");
    }

    close(sock);
    return 0;
}
