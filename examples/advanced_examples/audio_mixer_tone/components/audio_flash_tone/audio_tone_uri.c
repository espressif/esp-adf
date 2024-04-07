/*This is tone file*/

const char* tone_uri[] = {
   "flash://tone/0_lexin_introduce.mp3",
   "flash://tone/1_wozai.mp3",
};

int get_tone_uri_num()
{
    return sizeof(tone_uri) / sizeof(char *) - 1;
}
