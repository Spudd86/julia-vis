@ECHO OFF

REM gst-launch-1.0 -v ^
REM filesrc location=Hangarmageddon.ogg ! decodebin ! audioresample ! tee name=t ! queue ! audioconvert ! autoaudiosink ^
REM t. ! queue ! audioconvert ! julia_vis ! queue ! video/x-raw,width=256,height=256,framerate=24/1 ! autovideosink

REM 2k = 2097152

REM --gst-debug-level=2 

gst-launch-1.0 ^
streamsynchronizer name=s ^
filesrc location=Hangarmageddon.ogg ! oggdemux ! queue max-size-bytes=4194304 max-size-time=0 max-size-buffers=0 ! vorbisdec ! tee name=t ^
t.src_0 ! queue ! audioconvert ! spacescope ! video/x-raw,width=384,height=384,framerate=60/1 ! queue ! s.sink_0 ^
t.src_1 ! queue max-size-buffers=0 max-size-time=0 max-size-bytes=0 ! audioconvert ! s.sink_1 ^
s.src_0 ! fakesink max-lateness=20000000 ^
s.src_1 ! directsoundsink can-activate-pull=1 sync=1

REM s.src_0 ! d3dvideosink qos=0 ^
REM s.src_1 ! directsoundsink can-activate-pull=1 sync=1

REM s.src_0 ! queue max-size-buffers=3 max-size-bytes=0 max-size-time=0 ! d3dvideosink qos=1 max-lateness=20000000 ^
REM s.src_1 ! audioconvert ! directsoundsink
