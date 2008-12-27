#include <gtk/gtk.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <malloc.h>

//the global pixmap that will serve as our buffer
static GdkPixmap *pixmap = NULL;

gboolean on_window_configure_event(GtkWidget * da, GdkEventConfigure * event, gpointer user_data){
    static int oldw = 0;
    static int oldh = 0;
    //make our selves a properly sized pixmap if our window has been resized
    if (oldw != event->width || oldh != event->height){
        //create our new pixmap with the correct size.
        GdkPixmap *tmppixmap = gdk_pixmap_new(da->window, event->width,  event->height, -1);
        //copy the contents of the old pixmap to the new pixmap.  This keeps ugly uninitialized
        //pixmaps from being painted upon resize
        int minw = oldw, minh = oldh;
        if( event->width < minw ){ minw =  event->width; }
        if( event->height < minh ){ minh =  event->height; }
        gdk_draw_drawable(tmppixmap, da->style->fg_gc[GTK_WIDGET_STATE(da)], pixmap, 0, 0, 0, 0, minw, minh);
        //we're done with our old pixmap, so we can get rid of it and replace it with our properly-sized one.
        g_object_unref(pixmap); 
        pixmap = tmppixmap;
    }
    oldw = event->width;
    oldh = event->height;
    return TRUE;
}

gboolean on_window_expose_event(GtkWidget * da, GdkEventExpose * event, gpointer user_data){
    gdk_draw_drawable(da->window,
        da->style->fg_gc[GTK_WIDGET_STATE(da)], pixmap,
        // Only copy the area that was exposed.
        event->area.x, event->area.y,
        event->area.x, event->area.y,
        event->area.width, event->area.height);
    return TRUE;
}


static int currently_drawing = 0;
//do_draw will be executed in a separate thread whenever we would like to update
//our animation

void soft_map_ref(guint8 *out, guint8 *in, int w, int h, float x0, float y0);
void soft_map_ref2(guint16 *out, guint16 *in, int w, int h, float x0, float y0);
void soft_map_bl(guint16 *out, guint16 *in, int w, int h, float x0, float y0);
void pallet_blit(void *dest, int dst_stride, uint16_t *src, int w, int h, uint32_t *pal);
void pallet_blit_cairo(cairo_surface_t *dst, uint16_t *src, int w, int h, uint32_t *pal);
void pallet_blit_cairo_unroll(cairo_surface_t *dst, uint16_t *src, int w, int h, uint32_t *pal);

#define IM_SIZE 512
#define IM_MID  (IM_SIZE/2)

void *do_draw(void *ptr){

    //prepare to trap our SIGALRM so we can draw when we recieve it!
    siginfo_t info;
    sigset_t sigset;

    sigemptyset(&sigset);
    sigaddset(&sigset, SIGALRM);
	
	cairo_surface_t *static_surf = cairo_image_surface_create (CAIRO_FORMAT_A8, IM_SIZE, IM_SIZE);
	guchar *source = cairo_image_surface_get_data(static_surf);
	int src_strd = cairo_image_surface_get_stride(static_surf);
	memset(source, 0, IM_SIZE * src_strd);
	cairo_t *cr = cairo_create(static_surf);
	cairo_pattern_t *pat = cairo_pattern_create_radial (IM_MID, IM_MID, 0, IM_MID,  IM_MID, IM_SIZE/8);
	cairo_pattern_add_color_stop_rgba (pat, 0, 1, 1, 1, 1);
	cairo_pattern_add_color_stop_rgba (pat, 1, 0, 0, 0, 0);
	cairo_set_source (cr, pat);

	cairo_arc(cr, IM_MID, IM_MID, IM_SIZE/8, 0, 2*G_PI);
	cairo_fill(cr);
	cairo_pattern_destroy (pat);
	cairo_destroy(cr);
	
	cairo_surface_t *cst = cairo_image_surface_create (CAIRO_FORMAT_RGB24, IM_SIZE, IM_SIZE);
	//const int stride = cairo_image_surface_get_stride(cst);
	//void *draw_surf = cairo_image_surface_get_data(cst);
	memset(cairo_image_surface_get_data(cst), 0, IM_SIZE * cairo_image_surface_get_stride(cst));
	
	guint16 *map_surf[2];
	
	//~ map_surf[0] = g_malloc0(IM_SIZE * IM_SIZE * sizeof(guint16));
	//~ map_surf[1] = g_malloc0(IM_SIZE * IM_SIZE * sizeof(guint16));
	map_surf[0] = valloc(IM_SIZE * IM_SIZE * sizeof(guint16));
	memset(map_surf[0], 0, IM_SIZE * IM_SIZE * sizeof(guint16));
	map_surf[1] = valloc(IM_SIZE * IM_SIZE * sizeof(guint16));
	memset(map_surf[1], 0, IM_SIZE * IM_SIZE * sizeof(guint16));
	
	guint32 *pal = memalign(32, 257 * sizeof(guint32));
	for(int i = 0; i < 256; i++) pal[i] = (((i+128)%256)<<16) | (i<<8) | ((255-i));
	pal[256] = pal[255];

	int m = 0; // current map surf

    while(1)
	{//wait for our SIGALRM.  Upon receipt, draw our stuff.  Then, do it again!
		float t0 = 0, t1 = 0;
        while (sigwaitinfo(&sigset, &info) > 0) 
		{
            currently_drawing = 1;

            for(int y=0; y < IM_SIZE; y++) {
				for(int x=0; x < IM_SIZE; x++) {
					map_surf[m][y*IM_SIZE + x] = MAX(map_surf[m][y*IM_SIZE + x], source[y*src_strd + x]<<8);
				}
			}
			
			soft_map_bl(map_surf[(m+1)&0x1], map_surf[m], IM_SIZE, IM_SIZE, sin(t0), sin(t1));
			m = (m+1)&0x1; t0+=0.01; t1+=0.07;

            //create a gtk-independant surface to draw on
            pallet_blit_cairo_unroll(cst, map_surf[m], IM_SIZE, IM_SIZE, pal);

            //When dealing with gdkPixmap's, we need to make sure not to
            //access them from outside gtk_main().
            gdk_threads_enter();

            cairo_t *cr_pixmap = gdk_cairo_create(pixmap);
            cairo_set_source_surface (cr_pixmap, cst, 0, 0);
			cairo_paint(cr_pixmap);
            cairo_destroy(cr_pixmap);

            gdk_threads_leave();

            currently_drawing = 0;
        }
    }
	return 0;
}

gboolean timer_exe(GtkWidget * window){
    static int first_time = 1;
    //use a safe function to get the value of currently_drawing so
    //we don't run into the usual multithreading issues
    int drawing_status = g_atomic_int_get(&currently_drawing);

    //if this is the first time, create the drawing thread
    static pthread_t thread_info;
    if(first_time == 1){
        int  iret;
        iret = pthread_create( &thread_info, NULL, do_draw, NULL);
    }

    //if we are not currently drawing anything, send a SIGALRM signal
    //to our thread and tell it to update our pixmap
    if(drawing_status == 0){
        pthread_kill(thread_info, SIGALRM);
    }

    //tell our window it is time to draw our animation.
    int width, height;
    gdk_drawable_get_size(pixmap, &width, &height);
    gtk_widget_queue_draw_area(window, 0, 0, width, height);


    first_time = 0;
    return TRUE;

}


int main (int argc, char *argv[]){

    //Block SIGALRM in the main thread
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGALRM);
    pthread_sigmask(SIG_BLOCK, &sigset, NULL);

    //we need to initialize all these functions so that gtk knows
    //to be thread-aware
    if (!g_thread_supported ()){ g_thread_init(NULL); }
    gdk_threads_init();
    gdk_threads_enter();

    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(G_OBJECT(window), "expose_event", G_CALLBACK(on_window_expose_event), NULL);
    g_signal_connect(G_OBJECT(window), "configure_event", G_CALLBACK(on_window_configure_event), NULL);

    //this must be done before we define our pixmap so that it can reference
    //the colour depth and such
    gtk_widget_show_all(window);

    //set up our pixmap so it is ready for drawing
    pixmap = gdk_pixmap_new(window->window,500,500,-1);
    //because we will be painting our pixmap manually during expose events
    //we can turn off gtk's automatic painting and double buffering routines.
    gtk_widget_set_app_paintable(window, TRUE);
    gtk_widget_set_double_buffered(window, FALSE);

    (void)g_timeout_add(33, (GSourceFunc)timer_exe, window);


    gtk_main();
    gdk_threads_leave();

    return 0;
}
