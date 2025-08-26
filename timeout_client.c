#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>

#define SOCKET_PATH "/tmp/pwm_service.sock"

int main(int argc, char *argv[]) {
    
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
    
    for(;;){} // loop forever
    
    close(sock);
}