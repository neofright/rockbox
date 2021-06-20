/***************************************************************************
*             __________               __   ___.
*   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
*   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
*   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
*   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
*                     \/            \/     \/    \/            \/
* $Id: $
*
* Copyright (C) 2011-2021 by Tomasz Moń
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
* KIND, either express or implied.
*
****************************************************************************/

#include <stdio.h>
#include "config.h"
#include "system.h"
#include "power.h"
#include "kernel.h"
#include "logf.h"
#include "avr-sansaconnect.h"
#include "uart-target.h"
#include "usb.h"
#include "button.h"
#include "backlight.h"
#include "powermgmt.h"
#include "aic3x.h"

//#define BUTTON_DEBUG

#ifdef BUTTON_DEBUG
#include "lcd-target.h"
#include "lcd.h"
#include "font.h"
#include "common.h"
#endif

#ifdef BUTTON_DEBUG
#define dbgprintf DEBUGF
#else
#define dbgprintf(...)
#endif

#define CMD_SYNC        0xAA
#define CMD_CLOSE       0xCC
#define CMD_LCM_POWER   0xC9
#define LCM_POWER_OFF   0x00
#define LCM_POWER_ON    0x01
#define LCM_POWER_SLEEP 0x02
#define LCM_POWER_WAKE  0x03
#define LCM_REPOWER_ON  0x04

#define CMD_STATE       0xBB
#define CMD_VER         0xBC
#define CMD_WHEEL_EN    0xD0
#define CMD_SET_INTCHRG 0xD1
#define CMD_CODEC_RESET 0xD7
#define CMD_AMP_ENABLE  0xCA
#define CMD_FILL        0xFF

#define CMD_SYS_CTRL 0xDA
#define SYS_CTRL_POWEROFF 0x00

/* protects spi avr commands from concurrent access */
static struct mutex avr_mtx;

/* buttons thread */
#define BTN_INTERRUPT 1
static int btn = 0;
static bool hold_switch;
#ifndef BOOTLOADER
static long avr_stack[DEFAULT_STACK_SIZE/sizeof(long)];
static const char avr_thread_name[] = "avr";
static struct semaphore avr_thread_trigger;
#endif

/* OF bootloader will refuse to start software if low power is set
 * Bits 3, 4, 5, 6 and 7 are unknown.
 */
#define BATTERY_STATUS_LOW_POWER         (1 << 2)
#define BATTERY_STATUS_CHARGER_CONNECTED (1 << 1)
#define BATTERY_STATUS_CHARGING          (1 << 0)
static uint8_t avr_battery_status;

#define BATTERY_LEVEL_NOT_DETECTED       (1 << 7)
#define BATTERY_LEVEL_PERCENTAGE_MASK     0x7F
static uint8_t avr_battery_level = 100;

static inline unsigned short be2short(unsigned char* buf)
{
   return (unsigned short)((buf[0] << 8) | buf[1]);
}

#define BUTTON_DIRECT_MASK (BUTTON_LEFT | BUTTON_UP | BUTTON_RIGHT | BUTTON_DOWN | BUTTON_SELECT | BUTTON_VOL_UP | BUTTON_VOL_DOWN | BUTTON_NEXT | BUTTON_PREV)

#ifndef BOOTLOADER
static void handle_wheel(unsigned char wheel)
{
    static int key = 0;
    static unsigned char velocity = 0;
    static unsigned long wheel_delta = 1ul << 24;
    static unsigned char wheel_prev = 0;
    static long nextbacklight_hw_on = 0;
    static int prev_key = -1;
    static int prev_key_post = 0;

    if (TIME_AFTER(current_tick, nextbacklight_hw_on))
    {
        backlight_on();
        reset_poweroff_timer();
        nextbacklight_hw_on = current_tick + HZ/4;
    }

    if (wheel_prev < wheel)
    {
        key = BUTTON_SCROLL_FWD;
        velocity = wheel - wheel_prev;
    }
    else if (wheel_prev > wheel)
    {
        key = BUTTON_SCROLL_BACK;
        velocity = wheel_prev - wheel;
    }

    if (prev_key != key && velocity < 2 /* filter "rewinds" */)
    {
        /* direction reversal */
        prev_key = key;
        wheel_delta = 1ul << 24;
        return;
    }

    /* TODO: take velocity into account */
    if (queue_empty(&button_queue))
    {
        if (prev_key_post == key)
        {
            key |= BUTTON_REPEAT;
        }

        /* Post directly, don't update btn as avr doesn't give
           interrupt on scroll stop */
        queue_post(&button_queue, key, wheel_delta);

        wheel_delta = 1ul << 24;

        prev_key_post = key;
    }
    else
    {
        /* skipped post - increment delta and limit to 7 bits */
        wheel_delta += 1ul << 24;

        if (wheel_delta > (0x7ful << 24))
            wheel_delta = 0x7ful << 24;
    }

    wheel_prev = wheel;

    prev_key = key;
}
#endif

/* buf must be 11-byte array of byte (reply from avr_hid_get_state() */
static void parse_button_state(unsigned char *buf)
{
    unsigned short main_btns_state = be2short(&buf[4]);
#ifdef BUTTON_DEBUG
    unsigned short main_btns_changed = be2short(&buf[6]);
#endif

    /* make sure other bits doesn't conflict with our "free bits" buttons */
    main_btns_state &= BUTTON_DIRECT_MASK;

    if (buf[3] & 0x01) /* is power button pressed? */
    {
        main_btns_state |= BUTTON_POWER;
    }

    btn = main_btns_state;

#ifndef BOOTLOADER
    /* check if stored hold_switch state changed (prevents lost changes) */
    if ((buf[3] & 0x20) /* hold change notification */ ||
        (hold_switch != ((buf[3] & 0x02) >> 1)))
    {
#endif
        hold_switch = (buf[3] & 0x02) >> 1;
#ifdef BUTTON_DEBUG
        dbgprintf("HOLD changed (%d)", hold_switch);
#endif
#ifndef BOOTLOADER
        backlight_hold_changed(hold_switch);
    }
#endif
#ifndef BOOTLOADER
    if ((hold_switch == false) && (buf[3] & 0x80)) /* scrollwheel change */
    {
        handle_wheel(buf[2]);
    }
#endif

#ifdef BUTTON_DEBUG
    if (buf[3] & 0x10) /* power button change */
    {
        /* power button state has changed */
        main_btns_changed |= BUTTON_POWER;
    }

    if (btn & BUTTON_LEFT)        dbgprintf("LEFT");
    if (btn & BUTTON_UP)          dbgprintf("UP");
    if (btn & BUTTON_RIGHT)       dbgprintf("RIGHT");
    if (btn & BUTTON_DOWN)        dbgprintf("DOWN");
    if (btn & BUTTON_SELECT)      dbgprintf("SELECT");
    if (btn & BUTTON_VOL_UP)      dbgprintf("VOL UP");
    if (btn & BUTTON_VOL_DOWN)    dbgprintf("VOL DOWN");
    if (btn & BUTTON_NEXT)        dbgprintf("NEXT");
    if (btn & BUTTON_PREV)        dbgprintf("PREV");
    if (btn & BUTTON_POWER)       dbgprintf("POWER");
    if (btn & BUTTON_HOLD)        dbgprintf("HOLD");
    if (btn & BUTTON_SCROLL_FWD)  dbgprintf("SCROLL FWD");
    if (btn & BUTTON_SCROLL_BACK) dbgprintf("SCROLL BACK");
#endif
}

static void spi_txrx(unsigned char *buf_tx, unsigned char *buf_rx, int n)
{
    int i;
    unsigned short rxdata;

    mutex_lock(&avr_mtx);

    bitset16(&IO_CLK_MOD2, CLK_MOD2_SIF1);
    IO_SERIAL1_TX_ENABLE = 0x0001;

    for (i = 0; i<n; i++)
    {
        IO_SERIAL1_TX_DATA = buf_tx[i];

        /* 100 us wait for AVR */
        udelay(100);

        do
        {
            rxdata = IO_SERIAL1_RX_DATA;
        } while (rxdata & (1<<8));

        if (buf_rx != NULL)
            buf_rx[i] = rxdata & 0xFF;

        /* 100 us wait to give AVR time to process data */
        udelay(100);
    }

    IO_SERIAL1_TX_ENABLE = 0;
    bitclr16(&IO_CLK_MOD2, CLK_MOD2_SIF1);

    mutex_unlock(&avr_mtx);
}

void avr_hid_sync(void)
{
    int i;
    unsigned char prg[4] = {CMD_SYNC, CMD_VER, CMD_FILL, CMD_CLOSE};

    /* Send SYNC three times */
    for (i = 0; i<3; i++)
    {
        spi_txrx(prg, NULL, sizeof(prg));
    }
}

void avr_hid_init(void)
{
    /*
       setup alternate GIO functions:
       GIO29 - SIF1 Enable (Directly connected to AVR's SS)
       GIO30 - SIF1 Clock
       GIO31 - SIF1 Data In
       GIO32 - SIF1 Data Out
    */
    IO_GIO_FSEL2 = (IO_GIO_FSEL2 & 0x00FF) | 0xAA00;
    /* GIO29, GIO30 - outputs, GIO31 - input */
    IO_GIO_DIR1 = (IO_GIO_DIR1 & ~((1 << 13) | (1 << 14))) | (1 << 15);
    /* GIO32 - output */
    bitclr16(&IO_GIO_DIR2, (1 << 0));

    /* RATE = 219 (0xDB) -> 200 kHz */
    IO_SERIAL1_MODE = 0x6DB;

    mutex_init(&avr_mtx);
}

int _battery_level(void)
{
    /* OF still plays music when level is at 0 */
    if (avr_battery_level & BATTERY_LEVEL_NOT_DETECTED)
    {
        return 0;
    }
    return avr_battery_level & BATTERY_LEVEL_PERCENTAGE_MASK;
}

unsigned int power_input_status(void)
{
    if (avr_battery_status & BATTERY_STATUS_CHARGER_CONNECTED)
    {
        return POWER_INPUT_USB_CHARGER;
    }
    return POWER_INPUT_NONE;
}

bool charging_state(void)
{
    return (avr_battery_status & BATTERY_STATUS_CHARGING) != 0;
}

static void avr_hid_get_state(void)
{
    static unsigned char cmd[11] = {CMD_SYNC, CMD_STATE,
      CMD_FILL, CMD_FILL, CMD_FILL, CMD_FILL, CMD_FILL, CMD_FILL, CMD_FILL, CMD_FILL,
      CMD_CLOSE};

    static unsigned char buf[11];

    /* In very unlikely case the command has to be repeated */
    do
    {
        spi_txrx(cmd, buf, sizeof(cmd));
    }
    while ((buf[1] != CMD_SYNC) || (buf[10] != CMD_CLOSE));

    avr_battery_status = buf[8];
    avr_battery_level = buf[9];
    parse_button_state(buf);
}

static void avr_hid_enable_wheel(void)
{
    unsigned char wheel_en[4] = {CMD_SYNC, CMD_WHEEL_EN, 0x01, CMD_CLOSE};

    spi_txrx(wheel_en, NULL, sizeof(wheel_en));
}

/* command that is sent by "hidtool -J 1" issued on every OF boot */
void avr_hid_enable_charger(void)
{
    unsigned char charger_en[4] = {CMD_SYNC, CMD_SET_INTCHRG, 0x01, CMD_CLOSE};

    spi_txrx(charger_en, NULL, sizeof(charger_en));
}

void avr_hid_lcm_sleep(void)
{
    unsigned char lcm_sleep[4] = {CMD_SYNC, CMD_LCM_POWER, LCM_POWER_SLEEP, CMD_CLOSE};

    spi_txrx(lcm_sleep, NULL, sizeof(lcm_sleep));
}


void avr_hid_lcm_wake(void)
{
    unsigned char lcm_wake[4] = {CMD_SYNC, CMD_LCM_POWER, LCM_POWER_WAKE, CMD_CLOSE};

    spi_txrx(lcm_wake, NULL, sizeof(lcm_wake));
}

void avr_hid_lcm_power_on(void)
{
    unsigned char lcm_power_on[4] = {CMD_SYNC, CMD_LCM_POWER, LCM_POWER_ON, CMD_CLOSE};

    spi_txrx(lcm_power_on, NULL, sizeof(lcm_power_on));
}

void avr_hid_lcm_power_off(void)
{
    unsigned char lcm_power_off[4] = {CMD_SYNC, CMD_LCM_POWER, LCM_POWER_OFF, CMD_CLOSE};

    spi_txrx(lcm_power_off, NULL, sizeof(lcm_power_off));
}

void avr_hid_reset_codec(void)
{
    unsigned char codec_reset[4] = {CMD_SYNC, CMD_CODEC_RESET, CMD_CLOSE, CMD_FILL};

    spi_txrx(codec_reset, NULL, sizeof(codec_reset));
}

void avr_hid_set_amp_enable(unsigned char enable)
{
    unsigned char amp_enable[4] = {CMD_SYNC, CMD_AMP_ENABLE, enable, CMD_CLOSE};

    spi_txrx(amp_enable, NULL, sizeof(amp_enable));
}

void avr_hid_power_off(void)
{
    unsigned char prg[4] = {CMD_SYNC, CMD_SYS_CTRL, SYS_CTRL_POWEROFF, CMD_CLOSE};

    spi_txrx(prg, NULL, sizeof(prg));
}

#ifndef BOOTLOADER
static bool avr_state_changed(void)
{
    return (IO_GIO_BITSET0 & 0x1) ? false : true;
}

static bool headphones_inserted(void)
{
    return (IO_GIO_BITSET0 & 0x04) ? false : true;
}

static void set_audio_output(bool headphones)
{
    if (headphones)
    {
        /* Stereo output on headphones */
        avr_hid_set_amp_enable(0);
        aic3x_switch_output(true);
    }
    else
    {
        /* Mono output on built-in speaker */
        aic3x_switch_output(false);
        avr_hid_set_amp_enable(1);
    }
}

void avr_thread(void)
{
    bool headphones_active_state = headphones_inserted();
    bool headphones_state;

    set_audio_output(headphones_active_state);

    while (1)
    {
        semaphore_wait(&avr_thread_trigger, TIMEOUT_BLOCK);

        if (avr_state_changed())
        {
            /* Read buttons state */
            avr_hid_get_state();
        }

        headphones_state = headphones_inserted();
        if (headphones_state != headphones_active_state)
        {
            set_audio_output(headphones_state);
            headphones_active_state = headphones_state;
        }
    }
}

void GIO0(void) __attribute__ ((section(".icode")));
void GIO0(void)
{
    /* Clear interrupt */
    IO_INTC_IRQ1 = (1 << 5);

    semaphore_release(&avr_thread_trigger);
}

void GIO2(void) __attribute__ ((section(".icode")));
void GIO2(void)
{
    /* Clear interrupt */
    IO_INTC_IRQ1 = (1 << 7);

    semaphore_release(&avr_thread_trigger);
}
#endif

void button_init_device(void)
{
    btn = 0;
    hold_switch = false;
#ifndef BOOTLOADER
    semaphore_init(&avr_thread_trigger, 1, 1);
    create_thread(avr_thread, avr_stack, sizeof(avr_stack), 0,
                  avr_thread_name IF_PRIO(, PRIORITY_USER_INTERFACE)
                  IF_COP(, CPU));
#endif
    IO_GIO_DIR0 |= 0x01; /* Set GIO0 as input */

    /* Enable wheel */
    avr_hid_enable_wheel();
    /* Read button status and tell avr we want interrupt on next change */
    avr_hid_get_state();

#ifndef BOOTLOADER
    IO_GIO_IRQPORT |= 0x05; /* Enable GIO0/GIO2 external interrupt */
    IO_GIO_INV0 &= ~0x05; /* Clear INV for GIO0/GIO2 */
    /* falling edge detection on GIO0, any edge on GIO2 */
    IO_GIO_IRQEDGE = (IO_GIO_IRQEDGE & ~0x01) | 0x04;

    /* Enable GIO0 and GIO2 interrupts */
    IO_INTC_EINT1 |= INTR_EINT1_EXT0 | INTR_EINT1_EXT2;
#endif
}

int button_read_device(void)
{
    if(hold_switch)
        return 0;
    else
        return btn;
}

bool button_hold(void)
{
    return hold_switch;
}

void lcd_enable(bool on)
{
    (void)on;
}

