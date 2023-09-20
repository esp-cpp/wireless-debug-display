import sys
import socket
import time
import argparse

from zeroconf import ServiceBrowser, ServiceListener, Zeroconf

class MyListener(ServiceListener):
    def __init__(self, message):
        self.message = message
        self.has_sent = False

    def update_service(self, zc: Zeroconf, type_: str, name: str) -> None:
        print(f"Service {name} updated")

    def remove_service(self, zc: Zeroconf, type_: str, name: str) -> None:
        print(f"Service {name} removed")

    def add_service(self, zc: Zeroconf, type_: str, name: str) -> None:
        info = zc.get_service_info(type_, name)
        print(f"Service {name} added, service info: {info}")
        UDP_IP = socket.inet_ntoa(info.addresses[0])
        UDP_PORT = info.port
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM) # UDP
        sock.sendto(self.message.encode(), (UDP_IP, UDP_PORT))
        print(f"Sent to address: {UDP_IP}:{UDP_PORT}")
        self.has_sent = True

def main():
    parser = argparse.ArgumentParser(description='Send a message to the display.')
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--message', type=str, action="append",
                       help='Messages to send to the display')
    group.add_argument('--file', dest='file', type=str,
                       help='the file to read the message from')

    args = parser.parse_args()

    if args.message:
        MESSAGE = '\n'.join(args.message)
    if args.file:
        with open(args.file, 'r') as f:
            MESSAGE = f.read()

    zeroconf = Zeroconf()
    listener = MyListener(MESSAGE)
    browser = ServiceBrowser(zeroconf, "_debugdisplay._udp.local.", listener)

    print("Sending...\n")
    while not listener.has_sent:
        time.sleep(0.1)

if __name__ == '__main__':
    main()
