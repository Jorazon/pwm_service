# PWM Service for Raspberry Pi

Control RPi PWM! (hard) (no soft)

## Why?

I needed a way to control Raspberry Pi's hardware PWM without root privileges usually required due to access needed to `/dev/mem`.

The solution is to have a WiringPi wrapper service that is run as root.

## How?

### Service

To compile, install, enable, and start the service run the following command

```bash
make install
```

To check successful installation run

```bash
systemctl status pwm_daemon
```

and it should show `Active: active (running)` when starting the service was successful.

### Client

This repo also includes [example client code](./pwm_client.c).

#### Example

Compile

```bash
make pwm_client
```

Set GPIO12 (physical 32, WPi 26) to output a 50% duty cycle 390 Hz PWM signal.

```bash
./build/pwm_client 32 512 390
```

## TODO

- add signal handling to close socket on exit

---

┌┐┌┐┌┐┌┐┌┐┌┐┌┐┌┐┌┐┌┐┌┐┌┐┌┐┌┐┌┐┌┐┌┐┌┐┌┐┌┐┌┐┌┐┌┐┌┐┌┐┌┐┌┐┌┐┌┐  
┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└
