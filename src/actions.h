#ifndef A2PM_ACTIONS_H
#define A2PM_ACTIONS_H

int act_do_video(params_t *pr, avs_hnd_t *ah, AVS_Value res);
int act_do_audio(params_t *pr, avs_hnd_t *ah, AVS_Value res);
int act_do_info(params_t *pr, avs_hnd_t *ah);
int act_do_benchmark(params_t *pr, avs_hnd_t *ah, AVS_Value res);
int act_do_x264bd(params_t *pr, avs_hnd_t *ah, AVS_Value res);
int act_do_x264raw(params_t *pr, avs_hnd_t *ah, AVS_Value res);
int act_dump_pix_values_as_txt(params_t *pr, avs_hnd_t *ah, AVS_Value res);

#endif
