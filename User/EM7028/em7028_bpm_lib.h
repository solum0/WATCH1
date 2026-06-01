#ifndef __EM7028_BPM_LIB_H__
#define __EM7028_BPM_LIB_H__

#ifdef __cplusplus
extern "C" {
#endif

int em70xx_bpm_dynamic(int in, int qsensor_x, int qsensor_y, int qsensor_z);
int em70xx_reset(int reserved);

#ifdef __cplusplus
}
#endif

#endif
