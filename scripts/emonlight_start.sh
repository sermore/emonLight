#!/bin/sh
# 
# File:   emonlight_start.sh
# Author: Sergio
#
# Created on Apr 12, 2015, 6:16:54 PM
#

msg_max=$(cat /proc/sys/fs/mqueue/msg_max)
emonlight -r -v -d -q $msg_max
emonlight -s -v -d -q $msg_max
