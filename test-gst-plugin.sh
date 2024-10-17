#!/bin/sh

#TODO: command line arguments


#GDB='gdb --args'

#DEBUG_ARGS="--gst-debug=julia_vis:9 --gst-debug=baseaudiovisualizer-julia:9"
#DEBUG_ARGS="--gst-debug=julia_vis:9"

#TIME_OVERLAY1="timeoverlay time-mode=buffer-time ypos=0 ! timeoverlay time-mode=stream-time ypos=0 deltay=32"
#TIME_OVERLAY2="timeoverlay time-mode=stream-time ypos=0 deltay=64 color=0xffffff00"
TIME_OVERLAY1="identity"
TIME_OVERLAY2="identity"

#VIS_PLUGIN="julia_vis map-func=rational-interp"
#VIS_PLUGIN="julia_vis map-func=normal"
VIS_PLUGIN="julia_vis"


VID_FORMAT="video/x-raw,width=512,height=512,framerate=60/1"
VID_FORMAT="video/x-raw,width=1024,height=1024,framerate=60/1"
#VID_FORMAT="video/x-raw,width=512,height=512,framerate=30/1"
#VID_FORMAT="video/x-raw,format=RGB15,width=512,height=512,framerate=30/1"


VID_SINK="fpsdisplaysink video-sink=xvimagesink"
#VID_SINK="fpsdisplaysink"

#GDB='valgrind'
#VID_SINK="fakesink"
#VID_FORMAT="video/x-raw,width=128,height=128,framerate=10/1"

VID_FRAME_RATE=60
VID_WIDTH=1024
VID_HEIGHT=1024

INPUT_AUDIO="test.mid"

POSITIONAL=()
while [[ $# -gt 0 ]]; do
  key="$1"

  case $key in
    -h|--help)
      echo "Usage:\n"
      #TODO
      shift # past argument
      ;;
    -g|--gdb)
      GDB='gdb --args'
      shift # past argument
      ;;
    -v|--valgrind)
      GDB='valgrind'
      shift # past argument
      ;;
    -l|--log)
      DEBUG_ARGS="--gst-debug=julia_vis:9 --gst-debug=baseaudiovisualizer-julia:9"
      shift # past argument
      ;;
    -i|--input)
      INPUT_AUDIO="$2"
      shift # past argument
      shift # past value
      ;;

    -r|--resolution)
      VID_WIDTH=${2%%x}
      VID_HEIGHT=${2#x}
      shift # past argument
      shift # past value
      ;;
    
    --default)
      DEFAULT=YES
      shift # past argument
      ;;
    *)    # unknown option
      POSITIONAL+=("$1") # save it in an array for later
      shift # past argument
      ;;
  esac
done

GST_PLUGIN_PATH=bin/.libs $GDB gst-launch-1.0 $DEBUG_ARGS \
streamsynchronizer name=s \
filesrc location=$INPUT_AUDIO ! decodebin ! audioconvert  ! rgvolume ! audioconvert ! tee name=t \
t.src_0 ! queue ! $VIS_PLUGIN ! $VID_FORMAT ! $TIME_OVERLAY1 ! queue max-size-buffers=2 ! s.sink_0 \
t.src_1 ! queue ! audioconvert ! audioresample ! s.sink_1 \
s.src_0 !  $TIME_OVERLAY2 ! videoconvert ! $VID_SINK \
s.src_1 ! autoaudiosink


