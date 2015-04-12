#!/bin/sh
# 
# File:   init.sh
# Author: Sergio
#
# Created on Apr 12, 2015, 6:16:54 PM
#

gpio mode 0 in
gpio mode 0 up
gpio edge 0 falling
gpio exports
