/*This is tone file*/

const char* tone_uri[] = {
   "flash://tone/0_Bt_Reconnect.mp3",
   "flash://tone/1_Wechat.mp3",
   "flash://tone/2_Welcome_To_Wifi.mp3",
   "flash://tone/3_New_Version_Available.mp3",
   "flash://tone/4_Bt_Success.mp3",
   "flash://tone/5_Freetalk.mp3",
   "flash://tone/6_Upgrade_Done.mp3",
   "flash://tone/7_shutdown.mp3",
   "flash://tone/8_Alarm.mp3",
   "flash://tone/9_Wifi_Success.mp3",
};

int get_tone_uri_num()
{
    return sizeof(tone_uri) / sizeof(char *) - 1;
}