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
   "flash://tone/10_Under_Smartconfig.mp3",
   "flash://tone/11_Out_Of_Power.mp3",
   "flash://tone/12_server_connect.mp3",
   "flash://tone/13_hello.mp3",
   "flash://tone/14_new_message.mp3",
   "flash://tone/15_Please_Retry_Wifi.mp3",
   "flash://tone/16_please_setting_wifi.mp3",
   "flash://tone/17_Welcome_To_Bt.mp3",
   "flash://tone/18_Wifi_Time_Out.mp3",
   "flash://tone/19_Wifi_Reconnect.mp3",
   "flash://tone/20_server_disconnect.mp3",
};

int get_tone_uri_num()
{
    return sizeof(tone_uri) / sizeof(char *) - 1;
}
                