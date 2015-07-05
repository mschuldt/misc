import socket
import time
import random

#s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
#s.connect("/tmp/ga144_socket")

def make_command_string(type_, args):
    #message format: <type> <num-arg> <args...>
    return "{}:{}:".format(type_,
                             ":".join(map(str,[len(args)]+args)))

STEP_N = 2
SET_REG = 5

def send_command(cmd):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect("/tmp/ga144_socket")

    time.sleep(.5)
    print("\n[python] sending: {}".format(cmd))
    s.send(cmd)

    s.close()

while True:
    send_command(make_command_string(STEP_N, [4]))
    send_command(make_command_string(SET_REG, [111, 222, 333]))
    send_command(make_command_string(STEP_N, [1234]))
#s.recv(1024)
