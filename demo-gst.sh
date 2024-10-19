#!/bin/sh
VIS_PLUGIN="julia_vis map-func=normal"

VID_FORMAT="video/x-raw,width=768,height=768,framerate=60/1"

GST_PLUGIN_PATH=bin/.libs $GDB gst-launch-1.0 \
playsink name=s \
uridecodebin3 uri="https://www.newgrounds.com/audio/download/232307" ! audioconvert ! rgvolume ! tee name=t \
t.src_0 ! audioconvert ! audio/x-raw,format=F32LE,channels=1 ! $VIS_PLUGIN ! $VID_FORMAT ! queue2 ! s.video_sink \
t.src_1 ! queue2 ! audioconvert ! audioresample ! s.audio_sink
