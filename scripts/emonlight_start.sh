#!/bin/sh
# 
# File:   emonlight_start.sh
# Author: Sergio
#
# Created on Apr 12, 2015, 6:16:54 PM
#

msg_max=$(cat /proc/sys/fs/mqueue/msg_max)
emonlight -v -q $msg_max
