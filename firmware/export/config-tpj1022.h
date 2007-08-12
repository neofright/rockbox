/*
 * This config file is for the Tatung Elio TPJ-1022
 */

#define TARGET_TREE /* this target is using the target tree system */

/* For Rolo and boot loader */
#define MODEL_NUMBER 15

/* define this if you have recording possibility */
/*#define HAVE_RECORDING*/ /* TODO: add support for this */

/* define this if you have a bitmap LCD display */
#define HAVE_LCD_BITMAP

/* define this if you have a colour LCD */
#define HAVE_LCD_COLOR

/* define this if you have access to the quickscreen */
#define HAVE_QUICKSCREEN

/* define this if you have access to the pitchscreen */
#define HAVE_PITCHSCREEN

/* LCD dimensions */
#define LCD_WIDTH  220
#define LCD_HEIGHT 176
#define LCD_DEPTH  16   /* 65536 colours */
#define LCD_PIXELFORMAT RGB565 

/* #define IRAM_LCDFRAMEBUFFER IDATA_ATTR *//* put the lcd frame buffer in IRAM */

#define CONFIG_KEYPAD ELIO_TPJ1022_PAD

/* Define this if you do software codec */
#define CONFIG_CODEC SWCODEC

/* define this if you have a real-time clock */
#ifndef BOOTLOADER
//#define CONFIG_RTC RTC_E8564
#endif

/* Define this if you have a software controlled poweroff */
#define HAVE_SW_POWEROFF

/* The number of bytes reserved for loadable codecs */
#define CODEC_SIZE 0x80000

/* The number of bytes reserved for loadable plugins */
#define PLUGIN_BUFFER_SIZE 0x80000

/* Define this if you have the WM8731 audio codec */
#define HAVE_WM8731

#define AB_REPEAT_ENABLE 1

/* Define this for LCD backlight available */
#define HAVE_BACKLIGHT

/* Type of mobile power */
#define CONFIG_BATTERY BATT_LPCS355385
#define BATTERY_CAPACITY_DEFAULT 1550 /* default battery capacity
                                        TODO: check this, probably different
                                        for different models too */
#define BATTERY_CAPACITY_MIN 1500  /* min. capacity selectable */
#define BATTERY_CAPACITY_MAX 1600 /* max. capacity selectable */
#define BATTERY_CAPACITY_INC 10   /* capacity increment */
#define BATTERY_TYPES_COUNT  1    /* only one type */
#define BATTERY_SCALE_FACTOR 5865

/* Hardware controlled charging? FIXME */
//#define CONFIG_CHARGING CHARGING_SIMPLE

/* define this if the unit can be powered or charged via USB */
/*#define HAVE_USB_POWER*/

#ifndef SIMULATOR

/* Define this if you have a PortalPlayer PP5020 */
#define CONFIG_CPU PP5020

/* Define this if you want to use the PP5020 i2c interface */
#define CONFIG_I2C I2C_PP5020

/* define this if the hardware can be powered off while charging */
/* TODO: should this be set for the H10? */
//#define HAVE_POWEROFF_WHILE_CHARGING

/* The start address index for ROM builds */
#define ROM_START 0x00000000

/* Define this to the CPU frequency */
/* TODO: this is probably wrong */
#define CPU_FREQ      11289600

/* Type of LCD */
#define CONFIG_LCD LCD_TPJ1022

#define DEFAULT_CONTRAST_SETTING    19

/* Offset ( in the firmware file's header ) to the file length */
#define FIRMWARE_OFFSET_FILE_LENGTH 0

/* Offset ( in the firmware file's header ) to the file CRC */
#define FIRMWARE_OFFSET_FILE_CRC 0

/* Offset ( in the firmware file's header ) to the real data */
#define FIRMWARE_OFFSET_FILE_DATA 8

/* #define USB_IPODSTYLE */

/* USB On-the-go */
#define CONFIG_USBOTG USBOTG_ARC

/* Virtual LED (icon) */
#define CONFIG_LED LED_VIRTUAL

/* Define this if you have adjustable CPU frequency */
#define HAVE_ADJUSTABLE_CPU_FREQ

#define BOOTFILE_EXT "elio"
#define BOOTFILE "rockbox." BOOTFILE_EXT
#define BOOTDIR "/.rockbox"

#endif
