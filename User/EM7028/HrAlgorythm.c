#include "HrAlgorythm.h"

#define HR_ALGO_MIN_PEAK_INTERVAL_MS 320U
#define HR_ALGO_MIN_VALID_COUNT      3U
#define HR_ALGO_MAX_AVG_COUNT        5U

static Queue datas;
static Queue times;
static Queue hr_list;
static uint32_t peaks_time[2];
static uint8_t hr;

void HR_AlgoInit(void)
{
    initQueue(&datas);
    initQueue(&times);
    initQueue(&hr_list);
    peaks_time[0] = 0U;
    peaks_time[1] = 0U;
    hr = 0U;
}

static uint8_t Hr_Ave_Filter(const uint32_t *hr_values, uint8_t length)
{
    uint32_t ave = 0;
    uint8_t i;

    if (length == 0)
    {
        return 0;
    }

    for (i = 0; i < length; i++)
    {
        ave += hr_values[i];
    }
    ave /= length;

    return (uint8_t)ave;
}

uint16_t HR_Calculate(uint16_t present_dat, uint32_t present_time)
{
    uint8_t filter_length;

    if (isQueueFull(&datas))
    {
        dequeue(&datas);
    }
    if (isQueueFull(&times))
    {
        dequeue(&times);
    }
    if (isQueueFull(&hr_list))
    {
        dequeue(&hr_list);
    }

    enqueue(&datas, present_dat);
    enqueue(&times, present_time);

    if ((datas.data[3] >= datas.data[2]) && (datas.data[3] >= datas.data[1]) &&
        (datas.data[3] > datas.data[0]) && (datas.data[3] >= datas.data[4]) &&
        (datas.data[3] >= datas.data[5]) && (datas.data[3] > datas.data[6]))
    {
        if ((times.data[3] - peaks_time[0]) > HR_ALGO_MIN_PEAK_INTERVAL_MS)
        {
            peaks_time[1] = peaks_time[0];
            peaks_time[0] = times.data[3];

            if ((peaks_time[0] - peaks_time[1]) != 0U)
            {
                enqueue(&hr_list, 60000U / (peaks_time[0] - peaks_time[1]));
            }

            if (hr_list.size >= HR_ALGO_MIN_VALID_COUNT)
            {
                filter_length = (hr_list.size > HR_ALGO_MAX_AVG_COUNT) ? HR_ALGO_MAX_AVG_COUNT : (uint8_t)hr_list.size;
                hr = Hr_Ave_Filter(&hr_list.data[hr_list.size - filter_length], filter_length);
            }
        }
    }

    return hr;
}
