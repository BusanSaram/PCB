#include "pa1616d.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ---------------- helpers ---------------- */

static uint8_t nmea_checksum(const char *s, const char *end)
{
    uint8_t cs = 0;
    while (s < end) cs ^= (uint8_t)*s++;
    return cs;
}

/* Verify "$....*HH". Returns pointer to '*' or NULL if bad. */
static char *verify_checksum(char *line)
{
    if (line[0] != '$') return NULL;
    char *star = strchr(line, '*');
    if (!star || strlen(star) < 3) return NULL;
    uint8_t calc = nmea_checksum(line + 1, star);
    uint8_t recv = (uint8_t)strtoul(star + 1, NULL, 16);
    return (calc == recv) ? star : NULL;
}

/* Split on ',' in place, keeping empty fields. */
static int split_fields(char *s, char *f[], int max)
{
    int n = 0;
    f[n++] = s;
    for (; *s; s++) {
        if (*s == ',' && n < max) {
            *s = '\0';
            f[n++] = s + 1;
        }
    }
    return n;
}

static uint8_t two(const char *p) { return (uint8_t)((p[0]-'0')*10 + (p[1]-'0')); }

static void parse_time(PA1616D_Data_t *d, const char *t)
{
    if (strlen(t) < 6) return;
    d->hour = two(t); d->minute = two(t + 2); d->second = two(t + 4);
}

/* ddmm.mmmm (or dddmm.mmmm) -> decimal degrees */
static double parse_coord(const char *s, char hemi)
{
    if (!*s) return 0.0;
    double v = atof(s);
    int deg = (int)(v / 100.0);
    double val = deg + (v - deg * 100.0) / 60.0;
    return (hemi == 'S' || hemi == 'W') ? -val : val;
}

/* ---------------- sentence parsers ---------------- */

static void parse_gga(PA1616D_Data_t *d, char *f[], int n)
{
    if (n < 10) return;
    parse_time(d, f[1]);
    d->latitude    = parse_coord(f[2], f[3][0]);
    d->longitude   = parse_coord(f[4], f[5][0]);
    d->fix_quality = (uint8_t)atoi(f[6]);
    d->satellites  = (uint8_t)atoi(f[7]);
    d->hdop        = (float)atof(f[8]);
    d->altitude_m  = (float)atof(f[9]);
}

static void parse_rmc(PA1616D_Data_t *d, char *f[], int n)
{
    if (n < 10) return;
    parse_time(d, f[1]);
    d->valid = (f[2][0] == 'A');
    if (d->valid) {
        d->latitude  = parse_coord(f[3], f[4][0]);
        d->longitude = parse_coord(f[5], f[6][0]);
    }
    d->speed_kmh  = (float)atof(f[7]) * 1.852f;   /* knots -> km/h */
    d->course_deg = (float)atof(f[8]);
    if (strlen(f[9]) >= 6) {
        d->day   = two(f[9]);
        d->month = two(f[9] + 2);
        d->year  = 2000 + two(f[9] + 4);
    }
}

/* ---------------- public API ---------------- */

HAL_StatusTypeDef PA1616D_SendCmd(PA1616D_t *gps, const char *body)
{
    char buf[128];
    uint8_t cs = nmea_checksum(body, body + strlen(body));
    int len = snprintf(buf, sizeof(buf), "$%s*%02X\r\n", body, cs);
    return HAL_UART_Transmit(gps->huart, (uint8_t *)buf, (uint16_t)len, 100);
}

HAL_StatusTypeDef PA1616D_Init(PA1616D_t *gps, UART_HandleTypeDef *huart)
{
    memset(gps, 0, sizeof(*gps));
    gps->huart = huart;

    /* Output only RMC and GGA (fields: GLL,RMC,VTG,GGA,GSA,GSV,... 19 total) */
    PA1616D_SendCmd(gps, "PMTK314,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0");
    /* 1 Hz position fix interval (1000 ms) */
    PA1616D_SendCmd(gps, "PMTK220,1000");

    return HAL_UART_Receive_IT(huart, &gps->rx_byte, 1);
}

void PA1616D_RxCallback(PA1616D_t *gps)
{
    char c = (char)gps->rx_byte;

    if (c == '$') {
        gps->idx = 0;                       /* start of a new sentence */
    }
    if (c == '\n') {
        if (gps->idx > 0 && !gps->ready_flag) {
            gps->line[gps->idx] = '\0';
            memcpy(gps->ready, gps->line, gps->idx + 1);
            gps->ready_flag = true;
        }
        gps->idx = 0;
    } else if (c != '\r' && gps->idx < PA1616D_LINE_MAX - 1) {
        gps->line[gps->idx++] = c;
    } else if (gps->idx >= PA1616D_LINE_MAX - 1) {
        gps->idx = 0;                       /* overflow: discard */
    }

    HAL_UART_Receive_IT(gps->huart, &gps->rx_byte, 1);   /* re-arm */
}

bool PA1616D_Process(PA1616D_t *gps)
{
    if (!gps->ready_flag) return false;

    char work[PA1616D_LINE_MAX];
    memcpy(work, gps->ready, sizeof(work));
    gps->ready_flag = false;

    char *star = verify_checksum(work);
    if (!star) return false;
    *star = '\0';

    char *f[20];
    int n = split_fields(work + 1, f, 20);      /* skip '$' */
    if (n < 1 || strlen(f[0]) < 5) return false;

    const char *type = f[0] + 2;                /* skip talker ID (GP/GN/...) */
    if (strcmp(type, "GGA") == 0)      parse_gga(&gps->data, f, n);
    else if (strcmp(type, "RMC") == 0) parse_rmc(&gps->data, f, n);
    else return false;

    return true;
}
