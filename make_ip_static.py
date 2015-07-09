#!/usr/bin/python

# Usage:
#    ./make_ip_static.py [-d] [IP]
#  options:
#    -d   Dry run
#    IP   IP address that will be made static
#         Defaults to the current IP address
#
# Adapted from this tutorial:
#  http://www.modmypi.com/blog/tutorial-how-to-give-your-raspberry-pi-a-static-ip-address

import re
import subprocess as sp
from sys import argv
from os import geteuid

dhcp_line = "iface eth0 inet dhcp"

#TODO: There must be a better way to get this info
ifconfig_re = """eth0[ ]+Link encap:Ethernet  HWaddr ..:..:..:..:..:..[ ]*
[ ]*inet addr:([0-9.]+)  Bcast:([0-9.]+)  Mask:([0-9.]+)"""

netstat_re = """^([0-9.]+)[ ]+([0-9.]+)"""

dry_run = False
if "-d" in argv:
    dry_run = True
    argv.remove("-d")

if not dry_run and geteuid() != 0:
    print("I need to be run with root permissions!.")
    exit(0)

filename = "/etc/network/interfaces"

f = open(filename, "r")
interfaces = f.read()
f.close()

def die(msg, code=1):
    print(msg)
    print("No changes where made.")
    exit(code)

if not re.search(dhcp_line, interfaces):
    die("hmmm...I can't find what I'm looking for")

# get info from 'ifconfig'
ifconfig = sp.Popen('ifconfig', stdout=sp.PIPE, stderr=sp.STDOUT).stdout.read()

m = re.search(ifconfig_re, ifconfig)
if not m:
    die("ifconfig output is unfamiliar. I can't find what I'm looking for")
addr, bcast, mask = m.group(1), m.group(2), m.group(3)

if len(argv) == 2:
    addr = argv[1]

# get info from 'netstat -nr'
destination = gateway = None
netstat = sp.Popen(['netstat', '-nr'], stdout=sp.PIPE, stderr=sp.STDOUT).stdout.readlines()
for line in netstat:
    m = re.search(netstat_re, line)
    if m:
        if m.group(1) != "0.0.0.0":
            if destination:
                die("I'm confused. found two netstat Destination candidates.")
            destination = m.group(1)
        if m.group(2) != "0.0.0.0":
            if gateway:
                die("I'm confused. found two netstat Gateway candidates.")
            gateway = m.group(2)
if not destination:
    die("I could not find the Destination address using netstat")                
if not gateway:
    die("I could not find the Gateway address using netstat")
    
replacement = """iface eth0 inet static
address {}
netmask {}
network {}
broadcast {}
gateway {}""".format(addr, mask, destination, bcast, gateway)

new_interfaces = re.sub(dhcp_line, replacement, interfaces)

if dry_run:
    print("""The static IP address will be: {}

In file '{}' I will replace:

'{}'

  with:

{}""".format(addr, filename, dhcp_line, replacement))
else:
    f = open(filename, "w")
    f.write(new_interfaces)
    f.close()
    print("static IP address is: " + addr)
    f = open("/home/pi/IP_ADDR", "w")
    f.write(addr)
    f.close()

exit(0)

