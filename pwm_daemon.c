#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <wiringPi.h>

// Path for the Unix domain socket
#define SOCKET_PATH "/tmp/pwm_service.sock"
// Maximum message size
#define MAX_MSG_LEN 256
// PWM base clock frequency
#define PWM_BASE_HZ 19200000
// PWM range (WiringPi default)
#define PWM_RANGE 1024

// Structure for PWM request
struct pwm_request {
    int pin;          // GPIO pin number (WiringPi numbering)
    int duty_cycle;   // Duty cycle (0 to PWM_RANGE)
    int frequency;    // PWM frequency in Hz
};

/**
 * For the process. Runs child copy in background and exits parent.
 */
void daemonize(void) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS); // Parent exits
    }
    // Child continues
    if (setsid() < 0) {
        perror("setsid");
        exit(EXIT_FAILURE);
    }
    // Close standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

int main(void) {
    // Daemonize the process
    //daemonize();

    // Initialize WiringPi
    if (wiringPiSetup() == -1) {
        // Logging to syslog or file could be added here
        exit(EXIT_FAILURE);
    }

    // Create Unix domain socket
    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Set up socket address
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    unlink(SOCKET_PATH); // Remove existing socket file
    
    // Bind socket
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    // Set socket permissions
    if (chmod(SOCKET_PATH, 
        S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH 
    ) < 0) {
        perror("chmod");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_fd, 5) < 0) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Main loop: accept client connections
    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        // Receive PWM request
        struct pwm_request req;
        ssize_t bytes = recv(client_fd, &req, sizeof(req), 0);
        if (bytes <= 0) {
            close(client_fd);
            continue;
        }

        // Validate request
        if (req.pin < 0 || req.duty_cycle < 0 || req.duty_cycle > PWM_RANGE || req.frequency <= 0) {
            char *err_msg = "Invalid PWM parameters\n";
            send(client_fd, err_msg, strlen(err_msg), 0);
            close(client_fd);
            continue;
        }

        // Set up PWM on the specified pin
        pinMode(req.pin, PWM_OUTPUT);
        pwmSetMode(PWM_MODE_MS); // Mark-Space mode
        pwmSetRange(PWM_RANGE);
        pwmSetClock(PWM_BASE_HZ / (req.frequency * PWM_RANGE)); // Adjust clock for frequency
        pwmWrite(req.pin, req.duty_cycle);

        // Send response
        char *success_msg = "PWM set successfully\n";
        send(client_fd, success_msg, strlen(success_msg), 0);
        close(client_fd);
    }

    // Cleanup (unreachable due to infinite loop)
    close(server_fd);
    unlink(SOCKET_PATH);
    return 0;
}
