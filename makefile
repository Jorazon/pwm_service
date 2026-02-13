# Compiler and flags
CC = gcc
CFLAGS = -Wall

# Target binaries
DAEMON = pwm_daemon
CLIENT = pwm_client
PULSE = pwm_pulse

# Source files
DAEMON_SRC = pwm_daemon.c
CLIENT_SRC = pwm_client.c
PULSE_SRC = pulse.c

# Installation path
BUILD_DIR = ./build/
INSTALL_DIR = /usr/local/bin

default: $(DAEMON)

# Build all
all: $(DAEMON) $(CLIENT) $(PULSE)

$(BUILD_DIR):
	mkdir $(BUILD_DIR)
	echo "*" > $(BUILD_DIR).gitignore

# Build daemon
$(DAEMON): $(DAEMON_SRC) $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)$@ $< -lwiringPi -lpthread -D_GNU_SOURCE

# Build client
$(CLIENT): $(CLIENT_SRC) $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)$@ $<

# Build pulse
$(PULSE): $(PULSE_SRC) $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)$@ $< -lm

# Install binary and create service
install: $(DAEMON) $(BUILD_DIR)
	sudo systemctl stop pwm_daemon || true
	sudo install -m 755 $(BUILD_DIR)$(DAEMON) $(INSTALL_DIR)
	sudo setcap cap_sys_rawio=ep $(INSTALL_DIR)/$(DAEMON)
	sudo cp ./pwm_daemon.service /etc/systemd/system/
	sudo systemctl enable pwm_daemon
	sudo systemctl start pwm_daemon

# Clean build files
clean:
	rm -rf $(BUILD_DIR)

# Remove service and uninstall binary
uninstall:
	sudo systemctl stop pwm_daemon
	sudo systemctl disable pwm_daemon
	sudo rm -f /etc/systemd/system/pwm_daemon.service
	sudo rm -f $(INSTALL_DIR)/$(DAEMON)

# Phony targets
.PHONY: all install clean uninstall
