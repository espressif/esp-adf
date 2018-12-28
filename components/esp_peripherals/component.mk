#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

COMPONENT_ADD_INCLUDEDIRS := ./include ./lib/adc_button
COMPONENT_SRCDIRS :=  . ./lib ./lib/sdcard ./lib/button ./lib/touch ./lib/blufi ./lib/adc_button
COMPONENT_PRIV_INCLUDEDIRS := ./lib/sdcard ./lib/button ./lib/touch ./lib/blufi

CFLAGS+=-D__FILENAME__=\"$(<F)\"
