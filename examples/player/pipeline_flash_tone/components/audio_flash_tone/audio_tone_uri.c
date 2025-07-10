/*This is tone file*/

const char* tone_uri[] = {
   "flash://tone/0_BT_Connect.mp3",
   "flash://tone/1_Bt_Fail.mp3",
   "flash://tone/2_Bt_Success.mp3",
   "flash://tone/3_Mic_Close.mp3",
   "flash://tone/4_Mic_Open.mp3",
   "flash://tone/5_Server_Connect.mp3",
   "flash://tone/6_Upgrade_Done.mp3",
   "flash://tone/7_Upgrade_Failed.mp3",
   "flash://tone/8_Upgrading.mp3",
   "flash://tone/9_Wifi_Connect.mp3",
};

int get_tone_uri_num()
{
    return sizeof(tone_uri) / sizeof(char *) - 1;
}
