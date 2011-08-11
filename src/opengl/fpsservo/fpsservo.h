

#ifndef FPSSERVO_H
#define FPSSERVO_H

struct fps_period { 
	int32_t n; ///< numerator
	int32_t d; ///< denominator
};

struct fps_data;

struct fps_data *fps_data_new(struct fps_period rate, uint64_t init_msc, uint64_t now);

int fps_get_hist_len(struct fps_data *self);
void fps_get_worktimes(struct fps_data *self, int *total, const int **worktimes);
void fps_get_frametimes(struct fps_data *self, int *total, const int **frametimes);
void fps_get_delays(struct fps_data *self, int *total, const int **delays);
void fps_get_slacks(struct fps_data *self, int *total, const int **slacktimes);
int fps_get_cur_slack(struct fps_data *self);

/**
 * Call this just before XXXSwapBuffers()
 * 
 * @return target msc (for use with glXSwapBuffersMscOML()
 */ 
int64_t swap_begin(struct fps_data *self, int64_t now);

/**
 * @return delay from now to start drawing next frame
 */
int swap_complete(struct fps_data *self, int64_t now, uint64_t msc, uint64_t sbc);

void fps_get_worktime_array(struct fps_data *self, int *len, int **worktimes);

#endif

