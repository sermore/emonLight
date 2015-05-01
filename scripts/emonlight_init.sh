#!/bin/sh
# 
# File:   emonlight_init.sh
# Author: Sergio
#
# Created on Apr 12, 2015, 6:16:54 PM
#

#sh -c "echo 2048 > /proc/sys/fs/mqueue/msg_max"
gpio -g mode 17 in
gpio -g mode 17 up
gpio edge 17 falling
gpio exports
