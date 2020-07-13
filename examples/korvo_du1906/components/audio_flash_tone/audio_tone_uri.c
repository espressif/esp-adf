/*This is tone file*/

const char* tone_uri[] = {
   "flash://tone/0_already_new.mp3",
   "flash://tone/1_boot.mp3",
   "flash://tone/2_bt_connect.mp3",
   "flash://tone/3_bt_disconnect.mp3",
   "flash://tone/4_downloaded.mp3",
   "flash://tone/5_linked.mp3",
   "flash://tone/6_not_find.mp3",
   "flash://tone/7_ota_fail.mp3",
   "flash://tone/8_ota_start.mp3",
   "flash://tone/9_shut_down.mp3",
   "flash://tone/10_smart_config.mp3",
   "flash://tone/11_unlinked.mp3",
   "flash://tone/12_wakeup.mp3",
};

int get_tone_uri_num()
{
    return sizeof(tone_uri) / sizeof(char *) - 1;
}
