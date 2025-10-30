# Wireless Debug Display

This repository contains an example application designed for either
ESP32-WROVER-KIT, ESP32-S3-BOX, ESP32-S3-BOX-3, or LILYGO T-DECK (selectable via
menuconfig) which listens on a UDP socket for data. It then parses that data and
if it matches a certain format, it will plot the data in a graph, otherwise it
will print the data to a text log for display.

https://github.com/esp-cpp/wireless-debug-display/assets/213467/f835922f-e17f-4f76-95ee-5d6585e84656

<!-- markdown-toc start - Don't edit this section. Run M-x markdown-toc-refresh-toc -->
**Table of Contents**

- [Wireless Debug Display](#wireless-debug-display)
  - [Description](#description)
  - [Use](#use)
    - [Program](#program)
    - [Configure](#configure)
  - [Sending Data to the Display](#sending-data-to-the-display)
    - [Commands](#commands)
    - [Plotting](#plotting)
    - [Logging](#logging)
  - [Development](#development)
    - [Environment](#environment)
    - [Build and Flash](#build-and-flash)
  - [Output](#output)
    - [Console Logs:](#console-logs)
    - [Python script:](#python-script)
    - [ESP32-WROVER-KIT](#esp32-wrover-kit)
    - [LILYGO T-DECK](#lilygo-t-deck)
    - [ESP32-S3-BOX](#esp32-s3-box)
    - [ESP32-S3-BOX-3](#esp32-s3-box-3)

<!-- markdown-toc end -->

## Description

This code receives string data from a UDP server. It will parse that string data
and determine which of the following three types of data it is:

* *Commands*: contain the prefix (`+++`) in the string.
* *Plot data*: contain the delimiter (`::`) in the string followed by a
  single value which can be converted successfully to a number. If the
  conversion fails, the message will be printed as a log.
* *Log / text data*: all data that is not a command and cannot be
  plotted.

They are parsed in that priority order.

Some example data (no commands) can be found in [test_data.txt](./test_data.txt).

A couple python scripts are provided for sending data from a computer to your
logger to showcase simple UDP socket sending, as well as automatic service
discovery using mDNS.

- [./send_to_display.py](./send_to_display.py): Uses simple UDP sockets to send
  messages or a file to the debug display.
- [./send_to_display_mdns.py](./send_to_display_mdns.py): Uses python's
  `zeroconf` package to discover the wireless display on the network and then
  send messages or a file to the debug display. NOTE: zeroconf may not be
  installed / accessible within the python environment used by ESP-IDF.

## Use

You must first program your hardware. Afterwards, you can configure it via a USB
connection using its built-in CLI.

### Program

Download the release `programmer` executable from the latest [releases
page](https://github.com/esp-cpp/wireless-debug-display/releases) for `windows`,
`macos`, or `linux` - depending on which computer you want to use to perform the
one-time programming. There are a few programmers pre-built for the
`ESP-BOX`, the `LilyGo T-Deck`, or the `ESP32-Wrover-Kit`.

1. Download the programmer
2. Unzip it
3. Double click the `exe` (if windows), or open a terminal and execute it from
   the command line `./wireless-debug-display-<hardware>_programmer_v2.0.0_macos.bin`,
   where hardware is one of `esp-box` or `t-deck` or `wrover-kit`.

### Configure

To configure it, simply connect it to your computer via USB and open the serial
port in a terminal (e.g. `screen`, `PuTTY`, etc.) at 115200 baud. Once there,
you can use it as you would any other CLI - and the `help` command will provide
info about the commands available.

Any SSID/Password you set will be securely saved in the board's NVS, which is
managed by the ESP-IDF WiFi subsystem.

![CleanShot 2025-06-25 at 09 42 28](https://github.com/user-attachments/assets/680f2dbc-e75a-4359-8e18-752df31c42bd)

```console
sta> help
Commands available:
 - help
	This help message
 - exit
	Quit the session
 - log <verbosity>
	Set the log verbosity for the wifi sta.
 - connect
	Connect to a WiFi network with the given SSID and password.
 - connect <ssid> <password>
	Connect to a WiFi network with the given SSID and password.
 - disconnect
	Disconnect from the current WiFi network.
 - ssid
	Get the current SSID (Service Set Identifier) of the WiFi connection.
 - rssi
	Get the current RSSI (Received Signal Strength Indicator) of the WiFi connection.
 - ip
	Get the current IP address of the WiFi connection.
 - connected
	Check if the WiFi is connected.
 - mac
	Get the current MAC address of the WiFi connection.
 - bssid
	Get the current BSSID (MAC addressof the access point) of the WiFi connection.
 - channel
	Get the current WiFi channel of the connection.
 - config
	Get the current WiFi configuration.
 - scan <int>
	Scan for available WiFi networks.
 - memory
	Display minimum free memory.
 - switch_tab
	Switch to the next tab in the display.
 - clear_info
	Clear the Info display.
 - clear_plots
	Clear the Plot display.
 - clear_logs
	Clear the Log display.
 - push_data <data>
	Push data to the display.
 - push_info <info>
	Push info to the display.
```

## Sending Data to the Display

This display is designed to receive data from any other device on the network,
though it is primarily intended for other embedded wireless devices such as
other ESP-based systems. However, I have provided some scripts to help show how
data can be sent from computers or other systems if you choose.

Assuming that your computer is also on the network (you'll need to replace the
IP address below with the ip address displayed in the `info` page of the
display if you don't use the mDNS version):

```console
# this python script uses mDNS to automatically find the display on the network
python ./send_to_display_mdns.py --file <file>
python ./send_to_display_mdns.py --message "<message 1>" --message "<message 2>" ...
# e.g.
python ./send_to_display_mdns.py --file test_data.txt
python ./send_to_display_mdns.py --message "Hello world" --message "trace1::0" --message "trace1::1" --message "Goodbye World"

# this python script uses raw UDP sockets to send data to the display on the network
python ./send_to_display.py --ip <IP Address> --port <port, default 5555> --file <file>
python ./send_to_display.py --ip <IP Address> --port <port, default 5555> --message "<message 1>" --message "<message 2>" ... 
# e.g.
python ./send_to_display.py --ip 192.168.1.23 --file additional_data.txt
python ./send_to_display.py --ip 192.168.1.23 --message "Hello world" --message "trace1::0" --message "trace1::1" --message "Goodbye World"
```

### Commands

There are a limited set of commands in the system, which are
determined by a prefix and the command itself. If the prefix is found
_ANYWHERE_ in the string message (where messages are separated by
newlines), then the message is determined to be a command.

**PREFIX:** `+++` - three plus characters in a row

* **Remove Plot:** this command (`RP:` followed by the string plot name) will remove the named plot from the graph.
* **Clear Plots:** this command (`CP`) will remove _all_ plots from the graph.
* **Clear Logs:** this command (`CL`) will remove _all_ logs / text.

### Plotting

Messages which contain the string `::` and which have a value that
successfully and completely converts into a number are determined to
be a plot. Plots are grouped by their name, which is any string
preceding the `::`.

### Logging

All other text is treated as a log and written out to the log
window. Note, we do not wrap lines, so any text that would go off the
edge of the screen is simply not rendered.

## Development

You'll need to configure the build using `idf.py set-target <esp32 or esp32s3>`
and then `idf.py menuconfig` to then set the `Wireless Debug Display
Configuration` which allows you to set which hardware you want to run it on, as
well as the WiFi Access Point connection information (ssid/password). It also
allows customization of the port of the UDP server that the debug display is
running.

### Environment

This project is an ESP-IDF project, currently [ESP-IDF
v.5.5.1](https://github.com/espressif/esp-idf).

For information about setting up `ESP-IDF v5.5.1`, please see [the official
ESP-IDF getting started
documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/get-started/index.html).

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(Replace PORT with the name of the serial port to use.)

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

## Output

### Console Logs:
![initial output](https://github.com/esp-cpp/wireless-debug-display/assets/213467/c20993a7-9873-4c76-bc8e-1b115f63a5e0)
![receiving more info](https://github.com/esp-cpp/wireless-debug-display/assets/213467/0413e79e-018c-497e-b9d7-511481d17385)

### Python script:
![python script](https://github.com/esp-cpp/wireless-debug-display/assets/213467/9d5d4899-3074-47b1-8d57-1ef22aa4bfba)

### ESP32-WROVER-KIT

https://github.com/esp-cpp/wireless-debug-display/assets/213467/395400f6-e677-464c-a258-df06049cc562

### LILYGO T-DECK

![image](https://github.com/esp-cpp/wireless-debug-display/assets/213467/8dde7920-d21f-4565-b0a5-5b5804d2450d)

### ESP32-S3-BOX

![image](https://github.com/esp-cpp/wireless-debug-display/assets/213467/5aa28996-4ad7-4dbc-bc00-756ecd7ec736)
![image](https://github.com/esp-cpp/wireless-debug-display/assets/213467/2c75f6dc-4528-4663-ae12-f894ec2bcdc9)
![image](https://github.com/esp-cpp/wireless-debug-display/assets/213467/e59536a1-da8c-40fb-9f37-fdfdfb2d5b52)

https://github.com/esp-cpp/wireless-debug-display/assets/213467/f835922f-e17f-4f76-95ee-5d6585e84656

### ESP32-S3-BOX-3

![image](https://github.com/esp-cpp/wireless-debug-display/assets/213467/26501803-44bc-47a6-b229-d0b3ca1eb3be)
![image](https://github.com/esp-cpp/wireless-debug-display/assets/213467/a1f34a60-dae2-4264-a85b-a8b28f9bbacf)
