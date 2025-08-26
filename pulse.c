#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <math.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define NSEC 1e9
#define PI 3.14159265358979323846

#define SOCKET_PATH "/tmp/pwm_service.sock"
#define MAX_MSG_LEN 256
#define PWM_RANGE 1024

struct pwm_request {
    int pin;
    int duty_cycle;
    int frequency;
};

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <pin> <min> <max> <period>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    // Parse arguments
    struct pwm_request req;
    req.pin = atoi(argv[1]);
    req.frequency = 3000;
    
    int min = MAX(0, atoi(argv[2]));
    int max = MIN(PWM_RANGE, atoi(argv[3]));
    if (max < min) {
        int temp = max;
        max = min;
        min = temp;
    }
    
    struct timespec now, delay;
    char buffer[MAX_MSG_LEN];
    
    delay.tv_sec = 0;
    delay.tv_nsec = (1e9 / req.frequency);
    
    double period = (double)atoi(argv[4]); // seconds
    
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
        
    // Create socket
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
    
    for(;;) {
        timespec_get(&now, TIME_UTC);
        
        double time = now.tv_sec + (now.tv_nsec / NSEC);
        double angular_freq = 2.0 * PI / period;
        double sine_val = sin(angular_freq * time);
        double normalized = (sine_val + 1.0) / 2.0; // Map [-1, 1] to [0, 1]
        req.duty_cycle = min + (max - min) * normalized;
        
        //printf("%d %d %d", req.pin, req.duty_cycle, req.frequency);
        //printf("\r%4d", req.duty_cycle);
        
        // Send request
        if (send(sock, &req, sizeof(req), 0) < 0) {
            perror("Socket send");
            close(sock);
            exit(EXIT_FAILURE);
        }

        // Receive response
        ssize_t bytes = recv(sock, buffer, MAX_MSG_LEN - 1, 0);
        if (bytes > 0);
        
        nanosleep(&delay, NULL);
    }
    close(sock);
    return 0;
}

// TODO graceful exit on ^C
