/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2002 by wavey@wavey.org
 *nn
 * All files in this archive are subject to the GNU General Public License.
 * See the file COPYING in the source tree root for full license agreement.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

#include <stdio.h>
#include <stdarg.h>
#include "panic.h"
#include "lcd.h"
#include "debug.h"

static char panic_buf[128];

/*
 * "Dude. This is pretty fucked-up, right here." 
 */
void panicf( char *fmt, ...)
{
    va_list ap;

#ifndef SIMULATOR
    /* Disable interrupts */
    asm volatile ("ldc\t%0,sr" : : "r"(15<<4));
#endif
    
    va_start( ap, fmt );
    vsnprintf( panic_buf, sizeof(panic_buf), fmt, ap );
    va_end( ap );

#ifdef HAVE_LCD_CHARCELLS
#ifdef HAVE_NEW_CHARCELL_LCD
    lcd_double_height(false);
#endif
    lcd_puts(0,0,panic_buf);
#elif defined(HAVE_LCD_BITMAP)
    lcd_clear_display();
    lcd_putsxy(0,0,panic_buf,0);
    lcd_update();

#else
    /* no LCD */
#endif
    DEBUGF(panic_buf);
    while(1);
}
