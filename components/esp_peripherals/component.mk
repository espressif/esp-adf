#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

COMPONENT_ADD_INCLUDEDIRS := ./include ./lib/adc_button
COMPONENT_SRCDIRS :=  . ./lib ./lib/sdcard ./lib/button ./lib/touch ./lib/blufi ./lib/adc_button ./lib/IS31FL3216 ./driver/i2c_bus
COMPONENT_PRIV_INCLUDEDIRS := ./lib/sdcard ./lib/button ./lib/touch ./lib/blufi ./lib/IS31FL3216 ./driver/i2c_bus

CFLAGS+=-D__FILENAME__=\"$(<F)\"
