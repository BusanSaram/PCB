#ifndef PA1616D_H
#define PA1616D_H

#include "main.h"      /* or your family HAL header, e.g. stm32f4xx_hal.h */
#include <stdint.h>
#include <stdbool.h>

#define PA1616D_LINE_MAX 100   /* NMEA max is 82 chars; margin included */

typedef struct {
    /* time / date (UTC) */
    uint8_t  hour, minute, second;
    uint8_t  day, month;
    uint16_t year;

    /* position */
    double   latitude;      /* decimal degrees, +N / -S */
    double   longitude;     /* decimal degrees, +E / -W */
    float    altitude_m;    /* above mean sea level */

    /* motion */
    float    speed_kmh;
    float    course_deg;

    /* quality */
    uint8_t  fix_quality;   /* 0 = none, 1 = GPS, 2 = DGPS */
    uint8_t  satellites;
    float    hdop;
    bool     valid;         /* RMC status 'A' */
} PA1616D_Data_t;

typedef struct {
    UART_HandleTypeDef *huart;

    /* ISR-side line assembly */
    uint8_t  rx_byte;
    char     line[PA1616D_LINE_MAX];
    uint8_t  idx;
    char     ready[PA1616D_LINE_MAX];
    volatile bool ready_flag;

    PA1616D_Data_t data;
} PA1616D_t;

/* Start reception; optionally limits output to RMC+GGA at 1 Hz */
HAL_StatusTypeDef PA1616D_Init(PA1616D_t *gps, UART_HandleTypeDef *huart);

/* Call from HAL_UART_RxCpltCallback() */
void PA1616D_RxCallback(PA1616D_t *gps);

/* Call regularly from the main loop. Returns true when data was updated. */
bool PA1616D_Process(PA1616D_t *gps);

/* Send a PMTK command body, e.g. "PMTK220,200" (checksum + CRLF added) */
HAL_StatusTypeDef PA1616D_SendCmd(PA1616D_t *gps, const char *body);

#endif
