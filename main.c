//------------------------------------------------------------------------------
/**
 * @file client.c
 * @author charles-park (charles.park@hardkernel.com)
 * @brief ODROID-M1S JIG Client App.
 * @version 0.2
 * @date 2023-10-23
 *
 * @package apt install iperf3, nmap, ethtool, usbutils, alsa-utils
 *
 * @copyright Copyright (c) 2022
 *
 */
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <linux/fb.h>
#include <getopt.h>

//------------------------------------------------------------------------------
#include "lib_fbui/lib_fb.h"
#include "lib_fbui/lib_ui.h"
#include "lib_dev_check/lib_dev_check.h"
#include "lib_nlp/lib_nlp.h"

//------------------------------------------------------------------------------
//
// JIG Protocol(V2.0)
// https://docs.google.com/spreadsheets/d/1Of7im-2I5m_M-YKswsubrzQAXEGy-japYeH8h_754WA/edit#gid=0
//
//------------------------------------------------------------------------------
#define DEVICE_FB   "/dev/fb0"
#define CONFIG_UI   "ui.cfg"

#define ALIVE_DISPLAY_UI_ID     0
#define ALIVE_DISPLAY_INTERVAL  1000

#define APP_LOOP_DELAY  500

//------------------------------------------------------------------------------
typedef struct client__t {
    // HDMI UI
    fb_info_t   *pfb;
    ui_grp_t    *pui;

    // nlp for mac
    struct nlp_info nlp;

}   client_t;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// 문자열 변경 함수. 입력 포인터는 반드시 메모리가 할당되어진 변수여야 함.
//------------------------------------------------------------------------------
static void tolowerstr (char *p)
{
    int i, c = strlen(p);

    for (i = 0; i < c; i++, p++)
        *p = tolower(*p);
}

//------------------------------------------------------------------------------
static void toupperstr (char *p)
{
    int i, c = strlen(p);

    for (i = 0; i < c; i++, p++)
        *p = toupper(*p);
}

//------------------------------------------------------------------------------
static int run_interval_check (struct timeval *t, double interval_ms)
{
    struct timeval base_time;
    double difftime;

    gettimeofday(&base_time, NULL);

    if (interval_ms) {
        /* 현재 시간이 interval시간보다 크면 양수가 나옴 */
        difftime = (base_time.tv_sec - t->tv_sec) +
                    ((base_time.tv_usec - (t->tv_usec + interval_ms * 1000)) / 1000000);

        if (difftime > 0) {
            t->tv_sec  = base_time.tv_sec;
            t->tv_usec = base_time.tv_usec;
            return 1;
        }
        return 0;
    }
    /* 현재 시간 저장 */
    t->tv_sec  = base_time.tv_sec;
    t->tv_usec = base_time.tv_usec;
    return 1;
}

//------------------------------------------------------------------------------
//
// message discription (PROTOCOL_RX_BYTES)
//
//------------------------------------------------------------------------------
// start | cmd | ui id | grp_id | dev_id | action |extra dat| end (total 19 bytes)
//   @   |  C  |  0000 |    00  |   000  |    0   |  000000 | #
//------------------------------------------------------------------------------
#define PROTOCOL_RX_BYTES   19
#define SIZE_RESP_BYTES     6

static void client_init_data (client_t *p)
{
    int i, status;
    char resp [SIZE_RESP_BYTES +1], msg [PROTOCOL_RX_BYTES +1];

    ui_update (p->pfb, p->pui, -1);
    for (i = 0; i < p->pui->i_item_cnt; i++) {
        memset  (msg, 0, PROTOCOL_RX_BYTES);
        sprintf (msg, "@C%04d%02d%03dI0010#",   p->pui->i_item[i].ui_id,
                                                p->pui->i_item[i].grp_id,
                                                p->pui->i_item[i].dev_id);
        memset (resp, 0, SIZE_RESP_BYTES);
        status = device_check (msg, resp);

        if (!p->pui->i_item[i].is_info) {
            ui_set_ritem (p->pfb, p->pui, p->pui->i_item[i].ui_id,
                            status ? COLOR_GREEN : COLOR_RED, -1);
        }

        /* HDMI만 PASS/FAIL 문자열이고 나머지 Dev는 value값임 */
        if ((p->pui->i_item[i].grp_id != eGROUP_HDMI) &&
          !((p->pui->i_item[i].grp_id == eGROUP_ETHERNET) && (p->pui->i_item[i].dev_id == 1)))
            sprintf (resp, "%d", atoi(resp));

        ui_set_sitem (p->pfb, p->pui, p->pui->i_item[i].ui_id,
                        -1, -1, resp);
    }
    ui_update (p->pfb, p->pui, -1);
}

//------------------------------------------------------------------------------
static int client_setup (client_t *p)
{
    if ((p->pfb = fb_init (DEVICE_FB)) == NULL)         exit(1);
    if ((p->pui = ui_init (p->pfb, CONFIG_UI)) == NULL) exit(1);

    return 1;
}

//------------------------------------------------------------------------------
static void client_alive_display (client_t *p)
{
    static struct timeval i_time;
    static int onoff = 0;

    if (run_interval_check(&i_time, ALIVE_DISPLAY_INTERVAL)) {
        ui_set_ritem (p->pfb, p->pui, ALIVE_DISPLAY_UI_ID,
                    onoff ? COLOR_GREEN : p->pui->bc.uint, -1);
        onoff = !onoff;

        if (onoff)  ui_update (p->pfb, p->pui, -1);
    }
}

//------------------------------------------------------------------------------
int main (void)
{
    client_t client;

    memset (&client, 0, sizeof(client));

    // UI
    client_setup (&client);

    // client device init (lib_dev_check)
    device_setup ();

    // Dispaly device init data
    client_init_data (&client);

    // Get Network printer info
    if (nlp_init (&client.nlp, NULL)) {
        char mac_str[20];

        memset (mac_str, 0, sizeof(mac_str));
        ethernet_mac_str (mac_str);
        nlp_printf (&client.nlp, MSG_TYPE_MAC, mac_str, CH_NONE);
    }

    while (1) {
        client_alive_display (&client);

        usleep (APP_LOOP_DELAY);
    }
    return 0;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
