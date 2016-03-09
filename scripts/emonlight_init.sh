#!/bin/sh
# 
# File:   emonlight_init.sh
# Author: Sergio
#
# Created on Apr 12, 2015, 6:16:54 PM
#

#sh -c "echo 2048 > /proc/sys/fs/mqueue/msg_max"
# configure input pin for reading pulse signal
gpio -g mode 17 in
gpio -g mode 17 up
gpio edge 17 falling
# configure output pin for buzzer control
gpio export 3 out
gpio -g mode 3 out
gpio export 3 out
gpio -g write 3 0
#gpio exports
