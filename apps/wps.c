/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2002 Jerome Kuptz
 *
 * All files in this archive are subject to the GNU General Public License.
 * See the file COPYING in the source tree root for full license agreement.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "file.h"
#include "lcd.h"
#include "button.h"
#include "kernel.h"
#include "tree.h"
#include "debug.h"
#include "sprintf.h"
#include "settings.h"
#include "wps.h"
#include "mpeg.h"
#include "usb.h"
#include "powermgmt.h"
#include "status.h"
#include "main_menu.h"
#ifdef HAVE_LCD_BITMAP
#include "icons.h"
#include "widgets.h"
#endif

#ifdef LOADABLE_FONTS
#include "ajf.h"
#endif

#ifdef HAVE_LCD_BITMAP
#define LINE_Y      (global_settings.statusbar&&statusbar_enabled?1:0) /* Y position the entry-list starts at */
#else /* HAVE_LCD_BITMAP */
#define LINE_Y      0 /* Y position the entry-list starts at */
#endif /* HAVE_LCD_BITMAP */

#define PLAY_DISPLAY_DEFAULT         0 
#define PLAY_DISPLAY_FILENAME_SCROLL 1 
#define PLAY_DISPLAY_TRACK_TITLE     2 

#ifdef HAVE_RECORDER_KEYPAD
#define RELEASE_MASK (BUTTON_F1 | BUTTON_DOWN)
#else
#define RELEASE_MASK (BUTTON_MENU | BUTTON_STOP)
#endif

bool keys_locked = false;

static void draw_screen(struct mp3entry* id3)
{
    int font_height;
#ifdef LOADABLE_FONTS
    unsigned char *font = lcd_getcurrentldfont();
    font_height  = ajf_get_fontheight(font);
#else
    font_height = 8;
#endif


    lcd_clear_display();
    if(!id3)
    {
#ifdef HAVE_LCD_CHARCELLS
        lcd_puts(0, 0, "End of list");
        lcd_puts(0, 1, "<Press ON>");
#else
        lcd_puts(0, 2, "<End of song list>");
        lcd_puts(5, 4, "Press ON");
#endif
    }
    else
    {
        switch ( global_settings.wps_display ) {
            case PLAY_DISPLAY_TRACK_TITLE:
            {
                char ch = '/';
                char* end;
                char* szTok;
                char* szDelimit;
                char* szPeriod;
                char szArtist[26];
                char szBuff[257];
                szBuff[sizeof(szBuff)-1] = 0;

                strncpy(szBuff, id3->path, sizeof(szBuff));

                szTok = strtok_r(szBuff, "/", &end);
                szTok = strtok_r(NULL, "/", &end);

                // Assume path format of: Genre/Artist/Album/Mp3_file
                strncpy(szArtist,szTok,sizeof(szArtist));
                szArtist[sizeof(szArtist)-1] = 0;
                szDelimit = strrchr(id3->path, ch);
                lcd_puts(0,LINE_Y, szArtist?szArtist:"<nothing>");

                // removes the .mp3 from the end of the display buffer
                szPeriod = strrchr(szDelimit, '.');
                if (szPeriod != NULL)
                    *szPeriod = 0;

                lcd_puts_scroll(0,LINE_Y+1,(++szDelimit));
                break;
            }
            case PLAY_DISPLAY_FILENAME_SCROLL:
            {
                char ch = '/';
                char* szLast = strrchr(id3->path, ch);

                if (szLast)
                    lcd_puts_scroll(0,LINE_Y, (++szLast));
                else
                    lcd_puts_scroll(0,LINE_Y, id3->path);
                break;
            }
            case PLAY_DISPLAY_DEFAULT:
            {
                int l = LINE_Y;
#ifdef HAVE_LCD_BITMAP
                char buffer[64];

                lcd_puts_scroll(0, l++, id3->path);
                lcd_puts(0, l++, id3->title?id3->title:"");
                lcd_puts(0, l++, id3->album?id3->album:"");
                lcd_puts(0, l++, id3->artist?id3->artist:"");

                if(LINE_Y==0&&font_height<=8) {
                    if(id3->vbr)
                        snprintf(buffer, sizeof(buffer), "%d kbit (avg)",
                                 id3->bitrate);
                    else
                        snprintf(buffer, sizeof(buffer), "%d kbit", id3->bitrate);

                    lcd_puts(0, l++, buffer);
                    snprintf(buffer,sizeof(buffer), "%d Hz", id3->frequency);
                    lcd_puts(0, l++, buffer);
                }
                else {
                    if(id3->vbr)
                        snprintf(buffer, sizeof(buffer), "%dkbit(a) %dHz",
                                 id3->bitrate, id3->frequency);
                    else
                        snprintf(buffer, sizeof(buffer), "%dkbit    %dHz",
                                 id3->bitrate, id3->frequency);

                    lcd_puts(0, l++, buffer);
                }
#else
                lcd_puts(0, l++, id3->artist?id3->artist:"<no artist>");
                lcd_puts_scroll(0, l++, id3->title?id3->title:"<no title>");
#endif
                break;
            }
        }
    }
    status_draw();
    lcd_update();
}

void display_keylock_text(bool locked)
{
    lcd_scroll_pause();
    lcd_clear_display();

#ifdef HAVE_LCD_CHARCELLS
    if(locked)
        lcd_puts(0, 0, "Keylock ON");
    else
        lcd_puts(0, 0, "Keylock OFF");
#else
    if(locked)
    {
        lcd_puts(2, 3, "Key lock is ON");
    }
    else
    {
        lcd_puts(2, 3, "Key lock is OFF");
    }
    lcd_update();
#endif
    
    sleep(HZ);
    lcd_scroll_resume();
}

void display_mute_text(bool muted)
{
    lcd_scroll_pause();
    lcd_clear_display();

#ifdef HAVE_LCD_CHARCELLS
    if(muted)
        lcd_puts(0, 0, "Mute ON");
    else
        lcd_puts(0, 0, "Mute OFF");
#else
    if(muted)
    {
        lcd_puts(2, 3, "Mute is ON");
    }
    else
    {
        lcd_puts(2, 3, "Mute is OFF");
    }
    lcd_update();
#endif
    
    sleep(HZ);
    lcd_scroll_resume();
}

/* demonstrates showing different formats from playtune */
int wps_show(void)
{
    struct mp3entry* id3 = NULL;
    bool dont_go_to_menu = false;
    bool menu_button_is_down = false;
    bool pending_keylock = true; /* Keylock will go ON next time */
    int old_release_mask;
    int button;
    char buffer[32];

    old_release_mask = button_set_release(RELEASE_MASK);

    if(mpeg_is_playing())
    {
        id3 = mpeg_current_track();
        draw_screen(id3);
    }
    
    while ( 1 )
    {
        button = button_get_w_tmo(HZ/5);
        
        if(mpeg_has_changed_track())
        {
            lcd_stop_scroll();
            id3 = mpeg_current_track();
            draw_screen(id3);
        }
        
        switch(button)
        {
            case BUTTON_ON:
                if (keys_locked)
                {
                    display_keylock_text(keys_locked);
                    draw_screen(id3);
                    break;
                }

#ifdef HAVE_LCD_CHARCELLS
                lcd_icon(ICON_RECORD, false);
#endif
                button_set_release(old_release_mask);
                return 0;
                
#ifdef HAVE_RECORDER_KEYPAD
            case BUTTON_PLAY:
#else
            case BUTTON_UP:
#endif
                if (keys_locked)
                {
                    display_keylock_text(keys_locked);
                    draw_screen(id3);
                    break;
                }

                if ( mpeg_is_playing() )
                {
                    mpeg_pause();
                    status_set_playmode(STATUS_PAUSE);
                }
                else
                {
                    mpeg_resume();
                    status_set_playmode(STATUS_PLAY);
                }
                break;

#ifdef HAVE_RECORDER_KEYPAD
            case BUTTON_UP:
            case BUTTON_UP | BUTTON_REPEAT:
                if (keys_locked)
                {
                    display_keylock_text(keys_locked);
                    draw_screen(id3);
                    break;
                }

                global_settings.volume++;
                if(global_settings.volume > mpeg_sound_max(SOUND_VOLUME))
                    global_settings.volume = mpeg_sound_max(SOUND_VOLUME);
                mpeg_sound_set(SOUND_VOLUME, global_settings.volume);
                break;

            case BUTTON_DOWN:
            case BUTTON_DOWN | BUTTON_REPEAT:
                if (keys_locked)
                {
                    display_keylock_text(keys_locked);
                    draw_screen(id3);
                    break;
                }

                global_settings.volume--;
                if(global_settings.volume < mpeg_sound_min(SOUND_VOLUME))
                    global_settings.volume = mpeg_sound_min(SOUND_VOLUME);
                mpeg_sound_set(SOUND_VOLUME, global_settings.volume);
                break;
#endif

            case BUTTON_LEFT:
                if (keys_locked)
                {
                    display_keylock_text(keys_locked);
                    draw_screen(id3);
                    break;
                }
                mpeg_prev();
                status_set_playmode(STATUS_PLAY);
                break;

            case BUTTON_RIGHT:
                if (keys_locked)
                {
                    display_keylock_text(keys_locked);
                    draw_screen(id3);
                    break;
                }
                mpeg_next();
                status_set_playmode(STATUS_PLAY);
                break;

#ifdef HAVE_PLAYER_KEYPAD
            case BUTTON_MENU | BUTTON_LEFT:
            case BUTTON_MENU | BUTTON_LEFT | BUTTON_REPEAT:
                dont_go_to_menu = true;
                global_settings.volume--;
                if(global_settings.volume < mpeg_sound_min(SOUND_VOLUME))
                    global_settings.volume = mpeg_sound_min(SOUND_VOLUME);
                mpeg_sound_set(SOUND_VOLUME, global_settings.volume);
                break;

            case BUTTON_MENU | BUTTON_RIGHT:
            case BUTTON_MENU | BUTTON_RIGHT | BUTTON_REPEAT:
                dont_go_to_menu = true;
                global_settings.volume++;
                if(global_settings.volume > mpeg_sound_max(SOUND_VOLUME))
                    global_settings.volume = mpeg_sound_max(SOUND_VOLUME);
                mpeg_sound_set(SOUND_VOLUME, global_settings.volume);
                break;

#ifdef HAVE_RECORDER_KEYPAD
            case BUTTON_F1 | BUTTON_UP:
#else
            case BUTTON_MENU | BUTTON_UP:
#endif
                if (keys_locked)
                {
                    display_keylock_text(keys_locked);
                    draw_screen(id3);
                    break;
                }
                dont_go_to_menu = true;

                if(global_settings.muted == false)
                {
                    global_settings.muted = true;
                    mpeg_sound_set(SOUND_VOLUME, 0);
                    display_mute_text(global_settings.muted);
                    draw_screen(id3);
                }
                else
                {
                    global_settings.muted = false;
                    mpeg_sound_set(SOUND_VOLUME, global_settings.volume);
                    display_mute_text(global_settings.muted);
                    draw_screen(id3);
                }
                break;
                    
            case BUTTON_MENU:
                lcd_icon(ICON_PARAM, true);
                menu_button_is_down = true;
                break;

            case BUTTON_STOP | BUTTON_REL:
                /* The STOP key has been release while the MENU key
                   was held */
                if(menu_button_is_down)
                    pending_keylock = !pending_keylock;
                break;
#else
            case BUTTON_F1:
                menu_button_is_down = true;
                break;
                
            case BUTTON_DOWN | BUTTON_REL:
                /* The DOWN key has been release while the F1 key
                   was held */
                if(menu_button_is_down)
                {
                    pending_keylock = !pending_keylock;
                    debugf("pending: %d\n", pending_keylock);
                }
                break;
#endif
                    
#ifdef HAVE_RECORDER_KEYPAD
            case BUTTON_F1 | BUTTON_DOWN:
#else
            case BUTTON_MENU | BUTTON_STOP:
#endif
                if(keys_locked != pending_keylock)
                {
                    keys_locked = pending_keylock;

#ifdef HAVE_LCD_CHARCELLS
                    if(keys_locked)
                        lcd_icon(ICON_RECORD, true);
                    else
                        lcd_icon(ICON_RECORD, false);
#endif
                    display_keylock_text(keys_locked);
                    draw_screen(id3);
                }

                dont_go_to_menu = true;
                break;
                    
#ifdef HAVE_RECORDER_KEYPAD
            case BUTTON_F1 | BUTTON_REL:
#else
            case BUTTON_MENU | BUTTON_REL:
#endif
#ifdef HAVE_LCD_CHARCELLS
                lcd_icon(ICON_PARAM, false);
#endif
                if(!keys_locked && !dont_go_to_menu && menu_button_is_down)
                {
#ifdef HAVE_LCD_BITMAP
                    bool laststate=statusbar(false);
#endif
                    lcd_stop_scroll();
                    button_set_release(old_release_mask);
                    main_menu();
#ifdef HAVE_LCD_BITMAP
                    statusbar(laststate);
#endif
                    old_release_mask = button_set_release(RELEASE_MASK);
                    id3 = mpeg_current_track();
                    draw_screen(id3);
                }
                else
                {
                    dont_go_to_menu = false;
                }
                menu_button_is_down = false;
                break;

#ifdef HAVE_RECORDER_KEYPAD
            case BUTTON_F3:
#ifdef HAVE_LCD_BITMAP
                if(global_settings.statusbar) {
                    statusbar_toggle();
                    draw_screen(id3);
                }
#endif
                break;
#endif

#ifdef HAVE_RECORDER_KEYPAD
            case BUTTON_OFF:
#else
            case BUTTON_STOP:
#endif
                if (keys_locked)
                {
                    display_keylock_text(keys_locked);
                    draw_screen(id3);
                    break;
                }

                mpeg_stop();
                status_set_playmode(STATUS_STOP);
                button_set_release(old_release_mask);
                return 0;
                    
#ifndef SIMULATOR
            case SYS_USB_CONNECTED: {
#ifdef HAVE_LCD_BITMAP
                bool laststate=statusbar(false);
#endif
                /* Tell the USB thread that we are safe */
                DEBUGF("wps got SYS_USB_CONNECTED\n");
                usb_acknowledge(SYS_USB_CONNECTED_ACK);
                    
                /* Wait until the USB cable is extracted again */
                usb_wait_for_disconnect(&button_queue);

#ifdef HAVE_LCD_BITMAP
                statusbar(laststate);
#endif
                /* Signal to our caller that we have been in USB mode */
                return SYS_USB_CONNECTED;
                break;
            }
#endif
            case BUTTON_NONE: /* Timeout */
                if (mpeg_is_playing() && id3)
                {
#ifdef HAVE_LCD_BITMAP
                    snprintf(buffer,sizeof(buffer),
                             "Time:%3d:%02d/%d:%02d",
                             id3->elapsed / 60000,
                             id3->elapsed % 60000 / 1000,
                             id3->length / 60000,
                             id3->length % 60000 / 1000 );
                        
                    lcd_puts(0, 6, buffer);
                        
                    slidebar(0, LCD_HEIGHT-6, LCD_WIDTH, 6,
                             id3->elapsed*100/id3->length,
                             Grow_Right);
                        
                    lcd_update();
#else
                    /* Display time with the filename scroll only because
                       the screen has room. */
                    if (global_settings.wps_display ==
                        PLAY_DISPLAY_FILENAME_SCROLL)
                    { 
                        snprintf(buffer,sizeof(buffer), "%d:%02d/%d:%02d  ",
                                 id3->elapsed / 60000,
                                 id3->elapsed % 60000 / 1000,
                                 id3->length / 60000,
                                 id3->length % 60000 / 1000 );
                            
                        lcd_puts(0, 1, buffer);
                        lcd_update();
                    }
#endif
                }
                
                status_draw();
                break;
        }
    }
}
