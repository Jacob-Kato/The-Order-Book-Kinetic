import socket
import struct
import time
import random

sock = socket.socket(socket.AF_INET,socket.SOCK_DGRAM)

target_ip = "127.0.0.1"
target_port = 8080
target_address = (target_ip, target_port)

for n in range(1,1001):
    timestamp = time.time_ns()
    price = random.randint(10000,11000)
    volume = random.randint(1,100)
    if((random.randint(1,1000))%2==0):
        side = b'B'
    else:
        side = b'S'
        

    packed_data = struct.pack("<QqIc", timestamp, price, volume, side)

    sock.sendto(packed_data, target_address)
    print(f"sent packet({n})")
    time.sleep(0.01)

sock.close()

