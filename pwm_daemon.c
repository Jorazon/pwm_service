#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <wiringPi.h>
#include <pthread.h>

// Path for the Unix domain socket
#define SOCKET_PATH "/tmp/pwm_service.sock"
// Maximum number of threads
#define MAX_THREADS 100
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

struct ThreadSlot {
    pthread_t thread;
    int client_fd;
    int in_use;
};

struct ThreadPool {
    struct ThreadSlot *thread_pool;
    pthread_mutex_t pool_mutex;
    pthread_cond_t pool_available;
    int active_threads;
};

int initThreadPool(struct ThreadPool *pool) {
    pool->thread_pool = malloc(sizeof(struct ThreadSlot) * MAX_THREADS);
    if (!pool->thread_pool) return -1;
    pthread_mutex_init(&pool->pool_mutex, NULL);
    pthread_cond_init(&pool->pool_available, NULL);
    pool->active_threads = 0;
    return 0;
}

struct ThreadSlot * getThread(struct ThreadPool *pool) {
    pthread_mutex_lock(&pool->pool_mutex);
    while (pool->active_threads >= MAX_THREADS) {
        printf("Max threads (%d) reached, waiting for free slot...\n", MAX_THREADS);
        pthread_cond_wait(&pool->pool_available, &pool->pool_mutex);
    }
    int slot_index;
    for (int i = 0; i < MAX_THREADS; i++) {
        if (!pool->thread_pool[i].in_use) {
            slot_index = i;
            break;
        }
    }
    pthread_mutex_unlock(&pool->pool_mutex);
    return &pool->thread_pool[slot_index];
}

void removeThread(struct ThreadPool *pool, struct ThreadSlot *slot) {
    pthread_mutex_lock(&pool->pool_mutex);
    slot->in_use = 0;
    pool->active_threads--;
    pthread_cond_signal(&pool->pool_available);
    pthread_mutex_unlock(&pool->pool_mutex);
}

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

#define TIMEOUT_SEC 60

struct ThreadPool threads;
fd_set connections;

void *connection_task(void *arg) {
    struct ThreadSlot client_slot = *(struct ThreadSlot *)arg;
    int client_fd = client_slot.client_fd;
    
    printf("Client %d connected.\n", client_fd);
    
    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_SEC;
    timeout.tv_usec = 0;
    
    FD_SET(client_fd, &connections);
    
    while (1) {
        int ret = select(client_fd + 1, &connections, NULL, NULL, &timeout);
        if (ret < 0) {
            perror("select");
            break;
        } else if (ret == 0) {
            printf("No message from client %d within %d seconds.\n", client_fd, TIMEOUT_SEC);
            break;
        } else {
            struct pwm_request req;
            ssize_t bytes = recv(client_fd, &req, sizeof(req), 0);
            if (bytes < 0) {
                perror("recv");
                break;
            }
            
            if (bytes == 0) {
                printf("Client %d disconnected.\n", client_fd);
                break;
            }
            
            // Validate request
            if (req.pin < 0 || req.duty_cycle < 0 || req.duty_cycle > PWM_RANGE || req.frequency <= 0) {
                char *err_msg = "Invalid PWM parameters\n";
                send(client_fd, err_msg, strlen(err_msg), 0);
                break;
            }
            
            printf("Client %d: [Pin: %2d, Duty: %4d, Freq: %4d]\n", client_fd, req.pin, req.duty_cycle, req.frequency);
            
            // Set up PWM on the specified pin
            pinMode(req.pin, PWM_OUTPUT);
            pwmSetMode(PWM_MODE_MS); // Mark-Space mode
            pwmSetRange(PWM_RANGE);
            pwmSetClock(PWM_BASE_HZ / (req.frequency * PWM_RANGE)); // Adjust clock for frequency
            pwmWrite(req.pin, req.duty_cycle);
            
            // Send response
            char *success_msg = "PWM set successfully\n";
            send(client_fd, success_msg, strlen(success_msg), 0);
        }
    }
    FD_CLR(client_fd, &connections);
    close(client_fd);
    removeThread(&threads, &client_slot);
    printf("Closed client %d.\n", client_fd);
    return 0;
}
    
int main(void) {
    // Daemonize the process
    //daemonize();
    
    // Initialize WiringPi in physical pin numbering mode
    if (wiringPiSetupPhys() == -1) {
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

    initThreadPool(&threads);
    FD_ZERO(&connections);
    
    // Main loop: accept client connections
    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }
        
        struct ThreadSlot *connection = getThread(&threads);
        connection->client_fd = client_fd;
        pthread_create( &connection->thread, NULL, connection_task, (void*)connection);
    }

    // Cleanup (unreachable due to infinite loop)
    close(server_fd);
    unlink(SOCKET_PATH);
    return 0;
}
