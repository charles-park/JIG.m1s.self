#!/bin/bash

# device stable delay
sleep 10 && sync

#--------------------------
# ODROID-M1S audio play
#--------------------------
/root/JIG.m1s.self/service/jig-audio.sh &

#--------------------------
# ODROID-M1S self test enable
#--------------------------
/root/JIG.m1s.self/JIG.m1s.self > /dev/null 2>&1
