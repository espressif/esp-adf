/*This is tone file*/

const char* tone_uri[] = {
   "flash://tone/0_linked.mp3",
   "flash://tone/1_not_find.mp3",
   "flash://tone/2_smart_config.mp3",
   "flash://tone/3_unlinked.mp3",
   "flash://tone/4_wakeup.mp3",
};

int get_tone_uri_num()
{
    return sizeof(tone_uri) / sizeof(char *) - 1;
}
