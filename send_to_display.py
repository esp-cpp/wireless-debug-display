import sys
import socket
import argparse

def main():
    parser = argparse.ArgumentParser(description='Send a message to the display.')
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--message', type=str, action="append",
                       help='Messages to send to the display')
    group.add_argument('--file', dest='file', type=str,
                       help='the file to read the message from')
    parser.add_argument('--ip', dest='ip', type=str,
                        help='the ip address of the display')
    parser.add_argument('--port', dest='port', type=int, default=5555,
                        help='the port of the display')

    args = parser.parse_args()

    UDP_IP = args.ip
    UDP_PORT = args.port

    if args.message:
        MESSAGE = '\n'.join(args.message)
    if args.file:
        with open(args.file, 'r') as f:
            MESSAGE = f.read()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM) # UDP
    sock.sendto(MESSAGE.encode(), (UDP_IP, UDP_PORT))
    print(f"Sent to address: {UDP_IP}:{UDP_PORT}")

if __name__ == '__main__':
    main()
