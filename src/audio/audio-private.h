/**
 * audio-private.h
 *
 */

#ifndef AUDIOPRIVATE_H_
#define AUDIOPRIVATE_H_

int audio_setup_pa(const opt_data *od);
void audio_stop_pa(void);
int jack_setup(const opt_data *);
void jack_shutdown(void);
int pulse_setup(const opt_data *);
void pulse_shutdown(void);
int filedecode_setup(const opt_data *od);
void filedecode_shutdown(void);

/**
 * set up interal audio stuff, should be called by driver
 * @arg sr sample rate
 */
int audio_setup(int sr);

#endif /* include guard */
