#include <unistd.h>
#include <glib.h>
#include <timer.h>
#include <sys/timerfd.h>

static GThread *thread = NULL;


static gpointer run_points()
{
	int fd = timerfd_create(CLOCK_MONOTONIC, 0);
	
	struct itimerspec tm = { {0,1}, {0, 33333333} }; // 1/30 of a second
	struct itimerspec tmp_tmspc;
	timerfd_settime(fd, 0, &tm, &tmp_tmspc);
	
	guint64 expires; // hold # of times timer expired since last read
	
	while(read(fd,&expires, sizeof(guint64) != -1) {
		//update points
	}
	
	return NULL;
}

void setup_points()
{
	//TODO: error checking
	g_thread_create((GThreadFunc)run_points, NULL, FALSE, NULL);
}
