# Compiler and flags
CC = gcc
CFLAGS = -Wall -O2
LDFLAGS = -lwiringPi

# Target binaries
DAEMON = pwm_daemon
CLIENT = pwm_client

# Source files
DAEMON_SRC = pwm_daemon.c
CLIENT_SRC = pwm_client.c

# Installation path
BUILD_DIR = ./build/
INSTALL_DIR = /usr/local/bin

default: $(DAEMON)

# Build all
all: $(DAEMON) $(CLIENT)

$(BUILD_DIR):
	mkdir $(BUILD_DIR)
	echo "*" > $(BUILD_DIR).gitignore

# Build daemon
$(DAEMON): $(DAEMON_SRC) $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)$@ $< $(LDFLAGS)

# Build client
$(CLIENT): $(CLIENT_SRC) $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)$@ $<

# Install binaries
install: $(DAEMON) $(BUILD_DIR)
	sudo install -m 755 $(BUILD_DIR)$(DAEMON) $(INSTALL_DIR)
	sudo setcap cap_sys_rawio=ep $(INSTALL_DIR)/$(DAEMON)

# Clean build files
clean:
	rm -rf $(BUILD_DIR)

# Uninstall binaries
uninstall:
	sudo rm -f $(INSTALL_DIR)/$(DAEMON)

# Phony targets
.PHONY: all install clean uninstall
