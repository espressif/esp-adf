SECTIONS
{
    .esp_compact_font_codec : ALIGN(8)
    {
        . = ALIGN(8);
        _esp_compact_font_codec_array_start = .;
        KEEP(*(.esp_compact_font_codec))
        _esp_compact_font_codec_array_end = .;
    }
}
INSERT AFTER .rodata;
