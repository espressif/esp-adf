/****************************************************************************
*
* vprocTwolf_access.h - Voice Processor devices high level access module function
*                prototypes, variables
*
****************************************************************************
* Copyright Microsemi Inc, 2018. All rights reserved.
* Licensed under the MIT License. See LICENSE.txt in the project
* root for license information.
*
***************************************************************************/

#ifndef VPROC_TWOLFACCESS_H
#define VPROC_TWOLFACCESS_H

#include "vproc_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TWOLF_MAILBOX_SPINWAIT 1000 /*at least a 1000 to avoid mailbox busy */

/*device HBI command structure*/
typedef struct hbiCmdInfo {
    unsigned char page;
    unsigned char offset;
    unsigned char numwords;
} hbiCmdInfo;

/* external function prototypes */

VprocStatusType VprocTwolfHbiInit(void); /*Use this function to initialize the HBI bus*/

VprocStatusType VprocTwolfHbiRead(unsigned short cmd,     /*the 16-bit register to read from*/
                                  unsigned char numwords, /* The number of 16-bit words to read*/
                                  unsigned short *pData); /* Pointer to the read data buffer*/

VprocStatusType VprocTwolfHbiWrite(unsigned short cmd,     /*the 16-bit register to write to*/
                                   unsigned char numwords, /* The number of 16-bit words to write*/
                                   unsigned short *pData); /*the words (0-255) to write*/

VprocStatusType TwolfHbiNoOp(                         /*send no-op command to the device*/
                             unsigned char numWords); /* The number of no-op (0-255) to write*/

/*An alternative method to loading the firmware into the device
 * USe this method if you have used the provided tool to convert the *.s3 into
 * c code that can be compiled with the application
 */
VprocStatusType
VprocTwolfHbiBoot_alt(/*use this function to boot load the firmware (*.c) from the host to the device RAM*/
                      twFirmware *st_firmware); /*Pointer to the firmware image in host RAM*/

VprocStatusType VprocTwolfLoadConfig(dataArr *pCr2Buf, unsigned short numElements);

VprocStatusType VprocTwolfHbiCleanup(void);
VprocStatusType VprocTwolfHbiBootPrepare(void);
VprocStatusType VprocTwolfHbiBootMoreData(char *dataBlock);
VprocStatusType VprocTwolfHbiBootConclude(void);
VprocStatusType VprocTwolfFirmwareStop(void);   /*Use this function to halt the currently running firmware*/
VprocStatusType VprocTwolfFirmwareStart(void);  /*Use this function to start/restart the firmware currently in RAM*/
VprocStatusType VprocTwolfSaveImgToFlash(void); /*Save current loaded firmware from device RAM to FLASH*/
VprocStatusType VprocTwolfSaveCfgToFlash(void); /*Save current device config from device RAM to FLASH*/
VprocStatusType VprocTwolfReset(VprocResetMode mode);
VprocStatusType VprocTwolfEraseFlash(void);
VprocStatusType VprocTwolfLoadFwrCfgFromFlash(uint16 image_number);
VprocStatusType VprocTwolfSetVolume(uint8 vol);
VprocStatusType VprocTwolfGetVolume(int8_t *vol);
VprocStatusType VprocTwolfGetAppStatus(uint16 *status);

#ifdef __cplusplus
}
#endif
#endif /* VPROCTWOLFACCESS_H */
