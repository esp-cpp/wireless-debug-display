# Wireless Debug Display

This repository contains an example application designed for either
ESP32-WROVER-KIT, ESP32-S3-BOX, ESP32-S3-BOX-3, or LILYGO T-DECK (selectable via
menuconfig) which listens on a UDP socket for data. It then parses that data and
if it matches a certain format, it will plot the data in a graph, otherwise it
will print the data to a text log for display.

https://github.com/esp-cpp/wireless-debug-display/assets/213467/f835922f-e17f-4f76-95ee-5d6585e84656

## Configuration

You'll need to configure the build using `idf.py set-target <esp32 or esp32s3>`
and then `idf.py menuconfig` to then set the `Wireless Debug Display
Configuration` which allows you to set which hardware you want to run it on, as
well as the WiFi Access Point connection information (ssid/password). It also
allows customization of the port of the UDP server that the debug display is
running.

## Use

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

## Build and Flash

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
