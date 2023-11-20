#!/bin/bash

# list of playback device
# aplay -l

# mixer control path
# amixer -c 1

# mixer control path set
amixer -c 1 sset 'Playback Path' 'HP'

while true
do
    aplay -Dhw:1,0 /root/JIG.m1s.self/service/piano.wav
    sleep 1
done