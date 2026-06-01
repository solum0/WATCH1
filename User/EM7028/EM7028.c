#include "EM7028.h"
#include "HrAlgorythm.h"
#include "../hi2c.h"
#include "../OLED/OLED.h"
#include "../OLED/oled_clock.h"
#include "task.h"
#include "semphr.h"

#define EM7028_ID_RETRY_COUNT 5U

extern SemaphoreHandle_t xI2CMutex;

static uint8_t g_em7028_enabled = 0U;
static uint8_t g_em7028_last_hr = 0U;
static uint8_t g_em7028_invalid_count = 0U;
static TickType_t g_em7028_last_sample_tick = 0U;
static TickType_t g_em7028_avg_window_start_tick = 0U;
static uint32_t g_em7028_avg_sum = 0U;
static uint16_t g_em7028_avg_count = 0U;
static uint8_t g_em7028_avg_min = 0U;
static uint8_t g_em7028_avg_max = 0U;
static uint8_t g_em7028_median_window[EM7028_HR_MEDIAN_WINDOW_SIZE] = {0U};
static uint8_t g_em7028_median_count = 0U;
static uint8_t g_em7028_median_index = 0U;

static void EM7028_DelayMs(uint16_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

uint8_t EM7028_ReadOneReg(unsigned char reg_addr)
{
    uint8_t dat = 0U;

    if (h_I2C_ReadReceives(EM7028_ADDR, (uint8_t)reg_addr, &dat, 1U) != 0U)
    {
        return 0U;
    }

    return dat;
}

void EM7028_WriteOneReg(unsigned char reg_addr, unsigned char dat)
{
    uint8_t bytes_to_send[2];

    bytes_to_send[0] = (uint8_t)reg_addr;
    bytes_to_send[1] = (uint8_t)dat;
    (void)h_I2C_SendBytes((uint8_t)(EM7028_ADDR << 1), bytes_to_send, 2U);
}

uint8_t EM7028_Get_ID(void)
{
    return EM7028_ReadOneReg(ID_REG);
}

uint8_t EM7028_hrs_init(void)
{
    uint8_t retry = EM7028_ID_RETRY_COUNT;

    while ((EM7028_Get_ID() != EM7028_ID) && (retry > 0U))
    {
        EM7028_DelayMs(100U);
        retry--;
    }

    if (retry == 0U)
    {
        return 1U;
    }

    EM7028_WriteOneReg(HRS_CFG, 0x00U);
    EM7028_WriteOneReg(HRS2_DATA_OFFSET, 0x00U);
    EM7028_WriteOneReg(HRS2_GAIN_CTRL, 0x7FU);
    EM7028_WriteOneReg(HRS1_CTRL, 0x47U);
    EM7028_WriteOneReg(INT_CTRL, 0x00U);
    return 0U;
}

uint8_t EM7028_hrs_Enable(void)
{
    uint8_t retry = EM7028_ID_RETRY_COUNT;

    while ((EM7028_Get_ID() != EM7028_ID) && (retry > 0U))
    {
        EM7028_DelayMs(100U);
        retry--;
    }

    if (retry == 0U)
    {
        return 1U;
    }

    EM7028_WriteOneReg(HRS_CFG, 0x08U);
    return 0U;
}

uint8_t EM7028_hrs_DisEnable(void)
{
    uint8_t retry = EM7028_ID_RETRY_COUNT;

    while ((EM7028_Get_ID() != EM7028_ID) && (retry > 0U))
    {
        EM7028_DelayMs(100U);
        retry--;
    }

    if (retry == 0U)
    {
        return 1U;
    }

    EM7028_WriteOneReg(HRS_CFG, 0x00U);
    return 0U;
}

uint16_t EM7028_Get_HRS1(void)
{
    uint16_t dat;

    dat = EM7028_ReadOneReg(HRS1_DATA0_H);
    dat <<= 8;
    dat |= EM7028_ReadOneReg(HRS1_DATA0_L);
    return dat;
}

static void HR_SpO2_ShowHrOnly(uint8_t hr);

static void HR_SpO2_ResetAverageWindow(void)
{
    g_em7028_avg_window_start_tick = 0U;
    g_em7028_avg_sum = 0U;
    g_em7028_avg_count = 0U;
    g_em7028_avg_min = 0U;
    g_em7028_avg_max = 0U;
}

static void HR_SpO2_ResetMedianWindow(void)
{
    uint8_t i;

    g_em7028_median_count = 0U;
    g_em7028_median_index = 0U;
    for (i = 0U; i < EM7028_HR_MEDIAN_WINDOW_SIZE; i++)
    {
        g_em7028_median_window[i] = 0U;
    }
}

static void HR_SpO2_AddAverageSample(uint8_t hr, TickType_t now_tick)
{
    if (g_em7028_avg_count == 0U) {
        g_em7028_avg_window_start_tick = now_tick;
        g_em7028_avg_min = hr;
        g_em7028_avg_max = hr;
    } else {
        if (hr < g_em7028_avg_min) {
            g_em7028_avg_min = hr;
        }
        if (hr > g_em7028_avg_max) {
            g_em7028_avg_max = hr;
        }
    }

    g_em7028_avg_sum += hr;
    g_em7028_avg_count++;
}

static uint8_t HR_SpO2_TryGetAverageHr(TickType_t now_tick, uint8_t *average_hr)
{
    uint32_t filtered_sum;
    uint16_t filtered_count;

    if (g_em7028_avg_count == 0U) {
        return 0U;
    }

    if ((now_tick - g_em7028_avg_window_start_tick) < pdMS_TO_TICKS(EM7028_HR_AVG_WINDOW_MS)) {
        return 0U;
    }

    if (g_em7028_avg_count < EM7028_HR_AVG_MIN_SAMPLES) {
        HR_SpO2_ResetAverageWindow();
        return 0U;
    }

    filtered_sum = g_em7028_avg_sum;
    filtered_count = g_em7028_avg_count;

    if (g_em7028_avg_count > 2U) {
        filtered_sum -= g_em7028_avg_min;
        filtered_sum -= g_em7028_avg_max;
        filtered_count -= 2U;
    }

    if (filtered_count == 0U) {
        HR_SpO2_ResetAverageWindow();
        return 0U;
    }

    *average_hr = (uint8_t)((filtered_sum + (filtered_count / 2U)) / filtered_count);
    HR_SpO2_ResetAverageWindow();
    return 1U;
}

static uint8_t HR_SpO2_ApplyMedianFilter(uint8_t hr)
{
    uint8_t samples[EM7028_HR_MEDIAN_WINDOW_SIZE];
    uint8_t count;
    uint8_t i;
    uint8_t j;
    uint8_t temp;

    g_em7028_median_window[g_em7028_median_index] = hr;
    g_em7028_median_index = (uint8_t)((g_em7028_median_index + 1U) % EM7028_HR_MEDIAN_WINDOW_SIZE);
    if (g_em7028_median_count < EM7028_HR_MEDIAN_WINDOW_SIZE)
    {
        g_em7028_median_count++;
    }

    count = g_em7028_median_count;
    for (i = 0U; i < count; i++)
    {
        samples[i] = g_em7028_median_window[i];
    }

    for (i = 0U; i < (uint8_t)(count - 1U); i++)
    {
        for (j = (uint8_t)(i + 1U); j < count; j++)
        {
            if (samples[i] > samples[j])
            {
                temp = samples[i];
                samples[i] = samples[j];
                samples[j] = temp;
            }
        }
    }

    if ((count & 0x01U) != 0U)
    {
        return samples[count / 2U];
    }

    return (uint8_t)((samples[(count / 2U) - 1U] + samples[count / 2U] + 1U) / 2U);
}

static uint8_t HR_SpO2_ApplyStepLimit(uint8_t hr)
{
    uint8_t limited_hr = hr;

    if (g_em7028_last_hr == 0U)
    {
        return hr;
    }

    if (hr > g_em7028_last_hr)
    {
        if ((uint8_t)(hr - g_em7028_last_hr) > EM7028_HR_STEP_LIMIT)
        {
            limited_hr = (uint8_t)(g_em7028_last_hr + EM7028_HR_STEP_LIMIT);
        }
    }
    else if ((uint8_t)(g_em7028_last_hr - hr) > EM7028_HR_STEP_LIMIT)
    {
        limited_hr = (uint8_t)(g_em7028_last_hr - EM7028_HR_STEP_LIMIT);
    }

    return limited_hr;
}

static void HR_SpO2_UpdateDisplayedHr(uint8_t hr)
{
    hr = HR_SpO2_ApplyMedianFilter(hr);
    hr = HR_SpO2_ApplyStepLimit(hr);
    g_em7028_last_hr = hr;
    g_em7028_invalid_count = 0U;
    HR_SpO2_ShowHrOnly(g_em7028_last_hr);
}

static void HR_SpO2_ResetState(void)
{
    g_em7028_last_hr = 0U;
    g_em7028_invalid_count = 0U;
    g_em7028_last_sample_tick = 0U;

    HR_SpO2_ResetAverageWindow();
    HR_SpO2_ResetMedianWindow();
    HR_AlgoInit();
}

static void HR_SpO2_ShowHrOnly(uint8_t hr)
{
    OLED_ShowNum(60, 20, hr, 2, OLED_8X16);
    OLED_Update();
}

void EM7028_HR_SetEnabled(uint8_t enabled)
{
    g_em7028_enabled = (enabled != 0U) ? 1U : 0U;
    HR_SpO2_ResetState();
}

void HR_SpO2_Hander(void)
{
    uint16_t hrs_raw = 0U;
    uint16_t hr_value = 0U;
    uint8_t averaged_hr = 0U;
    TickType_t now_tick = xTaskGetTickCount();
    uint32_t sample_time_ms = (uint32_t)now_tick * (uint32_t)portTICK_PERIOD_MS;

    if (g_em7028_enabled == 0U) {
        HR_SpO2_showm(0, 0, 0);
        return;
    }

    if ((g_em7028_last_sample_tick != 0U) &&
        ((now_tick - g_em7028_last_sample_tick) < pdMS_TO_TICKS(EM7028_HR_SAMPLE_INTERVAL_MS))) {
        if (g_em7028_last_hr != 0U) {
            HR_SpO2_ShowHrOnly(g_em7028_last_hr);
        } else {
            HR_SpO2_showm(0, 0, 0);
        }
        return;
    }

    g_em7028_last_sample_tick = now_tick;

    if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        if (g_em7028_last_hr != 0U) {
            HR_SpO2_ShowHrOnly(g_em7028_last_hr);
        } else {
            HR_SpO2_showm(0, 0, 0);
        }
        return;
    }

    hrs_raw = EM7028_Get_HRS1();
    xSemaphoreGive(xI2CMutex);

    if ((hrs_raw == 0U) || (hrs_raw == 0xFFFFU)) {
        if (g_em7028_invalid_count < 255U) {
            g_em7028_invalid_count++;
        }
        if (g_em7028_invalid_count >= EM7028_HR_LOST_LIMIT) {
            g_em7028_last_hr = 0U;
            HR_SpO2_ResetAverageWindow();
            HR_SpO2_ResetMedianWindow();
            HR_AlgoInit();
            HR_SpO2_showm(0, 0, 0);
        } else if (g_em7028_last_hr != 0U) {
            HR_SpO2_ShowHrOnly(g_em7028_last_hr);
        } else {
            HR_SpO2_showm(0, 0, 0);
        }
        return;
    }

    hr_value = HR_Calculate(hrs_raw, sample_time_ms);

    if ((hr_value >= EM7028_HR_VALID_MIN) && (hr_value <= EM7028_HR_VALID_MAX)) {
        g_em7028_invalid_count = 0U;
        HR_SpO2_AddAverageSample((uint8_t)hr_value, now_tick);

        if (HR_SpO2_TryGetAverageHr(now_tick, &averaged_hr) != 0U) {
            HR_SpO2_UpdateDisplayedHr(averaged_hr);
        } else if (g_em7028_last_hr != 0U) {
            HR_SpO2_ShowHrOnly(g_em7028_last_hr);
        } else {
            HR_SpO2_showm(0, 0, 0);
        }
        return;
    }

    if (g_em7028_last_hr == 0U) {
        HR_SpO2_showm(0, 0, 0);
        return;
    }

    if (g_em7028_invalid_count < 255U) {
        g_em7028_invalid_count++;
    }

    if (g_em7028_invalid_count >= EM7028_HR_LOST_LIMIT) {
        g_em7028_last_hr = 0U;
        HR_SpO2_ResetAverageWindow();
        HR_SpO2_ResetMedianWindow();
        HR_AlgoInit();
        HR_SpO2_showm(0, 0, 0);
    } else if (g_em7028_last_hr != 0U) {
        HR_SpO2_ShowHrOnly(g_em7028_last_hr);
    } else {
        HR_SpO2_showm(0, 0, 0);
    }
}




