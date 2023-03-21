/****************************************************************************
 * vproc_common.c - Hal functions for the VPROC API
 *
 *
 ****************************************************************************
 * Copyright Microsemi Inc, 2018. All rights reserved.
 * Licensed under the MIT License. See LICENSE.txt in the project
 * root for license information.
 *
 ***************************************************************************/

#include "vproc_common.h"
#include "esp_codec_dev_os.h"
#include "audio_codec_ctrl_if.h"

/*Note - These functions are PLATFORM SPECIFIC- They must be modified
 *       accordingly
 **********************************************************************/

static audio_codec_ctrl_if_t *vproc_ctrl_if;

void VprocSetCtrlIf(void *ctrl_if)
{
    vproc_ctrl_if = (audio_codec_ctrl_if_t *) ctrl_if;
}

static uint16_t convert_edian(uint16_t v)
{
    return (v >> 8) | ((v & 0xFF) << 8);
}

void VprocHALcleanup(void)
{
}

int VprocHALInit(void)
{
    if (vproc_ctrl_if) {
        return 0;
    }
    return -1;
}

void Vproc_msDelay(unsigned short time)
{
    esp_codec_dev_sleep(time);
}

/* VprocWait(): use this function to
 *     force a delay of specified time in resolution of 125 micro-Seconds
 *
 * Input Argument: time in unsigned 32-bit
 * Return: none
 */
void VprocWait(unsigned long int time)
{
    esp_codec_dev_sleep(time);
}

/* This is the platform dependent low level spi
 * function to write 16-bit data to the ZL380xx device
 */
int VprocHALWrite(unsigned short val)
{
    int ret = 0;
    if (vproc_ctrl_if) {
        val = convert_edian(val);
        ret = vproc_ctrl_if->write_reg(vproc_ctrl_if, 0, 0, &val, sizeof(val));
    }
    return ret;
}

/* This is the platform dependent low level spi
 * function to read 16-bit data from the ZL380xx device
 */
int VprocHALRead(unsigned short *pVal)
{
    unsigned short data = 0;
    int ret = 0;
    if (vproc_ctrl_if) {
        ret = vproc_ctrl_if->read_reg(vproc_ctrl_if, 0, 0, &data, sizeof(data));
        *pVal = convert_edian(data);
    }
    return ret;
}
