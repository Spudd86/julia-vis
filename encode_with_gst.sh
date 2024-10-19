#!/bin/sh
#FILENAME=megaman.mp3
#FILENAME=232307_Euphoria_Edit.mp3

#OUTFILE=test

#FILENAME=../238913__edtijo__hejda-depressive-guitar.wav; OUTFILE=guitar
#FILENAME=../379875_The_time_portal.mp3; OUTFILE="/media/thomasjones/My Passport/the_time_portal"

#FILENAME=~/Music/Opening.ogg; OUTFILE="/media/thomasjones/My Passport/chrono_cross_op_vis"
FILENAME=~/Music/Desert_Wasteland.mp3.ogg; OUTFILE="/media/thomasjones/My Passport/Desert_Wasteland_rat_vis"

#GDB='gdb --args'

#DEBUG_ARGS="--gst-debug=julia_vis:9 --gst-debug=baseaudiovisualizer-julia:9"
#DEBUG_ARGS="--gst-debug=julia_vis:9"

# NOTE: must end in ! because they don't have it in the launch command string
#TIME_OVERLAY1="timeoverlay time-mode=buffer-time ypos=0 ! timeoverlay time-mode=stream-time ypos=0 deltay=32 !"
#TIME_OVERLAY2="timeoverlay time-mode=stream-time ypos=0 deltay=64 color=0xffffff00 !"
TIME_OVERLAY1=""
TIME_OVERLAY2=""

VIS_PLUGIN="julia_vis map-func=rational"
#VIS_PLUGIN="julia_vis map-func=normal"
#VIS_PLUGIN="julia_vis"

#VID_FORMAT="video/x-raw,width=384,height=384,framerate=30/1"
#VID_FORMAT="video/x-raw,width=1024,height=1024,framerate=60/1"
#VID_FORMAT="video/x-raw,width=8192,height=8192,framerate=60/1"
VID_FORMAT="video/x-raw,width=12288,height=12288,framerate=60/1"
#VID_FORMAT="video/x-raw,width=12960,height=12960,framerate=60/1"
#VID_FORMAT="video/x-raw,width=16384,height=16384,framerate=60/1"

#VID_POST_PROC="identity"
#1080p
#VID_POST_PROC="videoscale n-threads=8 gamma-decode=1 method=lanczos dither=true ! video/x-raw,width=1080,height=1080"
#2k/1440p video
#VID_POST_PROC="videoscale n-threads=8 gamma-decode=1 method=lanczos dither=true ! video/x-raw,width=1440,height=1440"
#4k video
VID_POST_PROC="videoscale n-threads=8 gamma-decode=1 method=lanczos dither=true ! video/x-raw,width=2160,height=2160"
#8k video
#VID_POST_PROC="videoscale n-threads=8 gamma-decode=1 method=lanczos dither=true ! video/x-raw,width=4320,height=4320"

#ENCODER="x264enc vbv-buf-capacity=8388608 bitrate=40000 pass=5 speed-preset=veryslow quantizer=14 qp-min=5"
ENCODER="x264enc vbv-buf-capacity=8192 bitrate=100000 pass=5 speed-preset=veryslow quantizer=12 qp-min=1"
#ENCODER="av1enc target-bitrate=40000"

GST_PLUGIN_PATH=bin/.libs $GDB gst-launch-1.0 $DEBUG_ARGS \
matroskamux name=mux ! filesink location="$OUTFILE.mkv" \
streamsynchronizer name=s \
filesrc location="$FILENAME" ! decodebin ! progressreport update-freq=1 ! audioconvert ! tee name=t \
t.src_0 ! queue ! $VIS_PLUGIN ! $VID_FORMAT ! $TIME_OVERLAY1 s.sink_0 \
t.src_1 ! queue max-size-bytes=0 max-size-buffers=0 max-size-time=10000000000 ! audioconvert ! audioresample ! s.sink_1 \
s.src_0 ! $VID_POST_PROC ! videoconvert ! $TIME_OVERLAY2 $ENCODER ! mux. \
s.src_1 ! audio/x-raw,format=S16LE,channels=2 ! mux. 
