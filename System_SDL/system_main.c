/* $NiH: system_main.c,v 1.22 2003/10/15 12:16:15 wiz Exp $ */
/*
  system_main.c -- main program
  Copyright (C) 2002-2003 Thomas Klausner

  This file is part of NeoPop-SDL, a NeoGeo Pocket emulator
  The author can be contacted at <tk@giga.or.at>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <sys/time.h>
#include <math.h>
#include <unistd.h>
#include "config.h"
#include "NeoPop-SDL.h"

#define NGP_FPS 59.95

char *prg;
struct timeval throttle_last;
_u8 system_frameskip_key;
_u32 throttle_rate;
int do_exit = 0;

static void
printversion(void)
{
    printf(PROGRAM_NAME " (SDL) " NEOPOP_VERSION " (SDL-Version "
	   VERSION ")\n");
}

static void
usage(int exitcode)
{
    printversion();
    printf("NeoGeo Pocket emulator\n\n"
	   "Usage: %s [-cefghjMmSsv] [game]\n"
	   "\t-c\t\tstart in colour mode (default: automatic)\n"
	   "\t-e\t\temulate English language NeoGeo Pocket (default)\n"
	   "\t-f count\tframeskip: show one in `count' frames (default: 1)\n"
	   "\t-g\t\tstart in greyscale mode (default: automatic)\n"
	   "\t-h\t\tshow this short help\n"
	   "\t-j\t\temulate Japanese language NeoGeo Pocket\n"
	   "\t-l state\tload start state from file `state'\n"
	   "\t-M\t\tdo not use smoothed magnification modes\n"
	   "\t-m\t\tuse smoothed magnification modes (default)\n"
	   "\t-S\t\tsilent mode\n"
	   "\t-s\t\twith sound (default)\n"
	   "\t-v\t\tshow version number\n", prg);

    exit(exitcode);
}

void
system_message(char *vaMessage, ...)
{
    va_list vl;

    va_start(vl, vaMessage);
    vprintf(vaMessage, vl);
    va_end(vl);
    printf("\n");
}

void
system_VBL(void)
{
    static int frame_counter = 0;
    struct timeval current_time;
    _u32 throttle_diff;

    system_graphics_update();

    system_input_update();

    if (mute == FALSE)
	system_sound_update();

    /* throttling */
    do {
	gettimeofday(&current_time, NULL);
	if (current_time.tv_sec == throttle_last.tv_sec)
	    throttle_diff = current_time.tv_usec - throttle_last.tv_usec;
	else
	    throttle_diff = 1000000 + current_time.tv_usec
		- throttle_last.tv_usec;
    } while (throttle_diff < throttle_rate);
    throttle_last = current_time;

    if (frame_counter++ > NGP_FPS) {
	char title[128];
	int fps;

	frame_counter = 0;
	fps = (int)(1000000/throttle_diff+.5);

	(void)snprintf(title, sizeof(title), PROGRAM_NAME " - %s - %dfps/FS%d",
		       rom.name, fps, system_frameskip_key);

	/* set window caption */
	SDL_WM_SetCaption(title, NULL);
    }

    return;
}

int
main(int argc, char *argv[])
{
    char *start_state;
    int ch;
    int i;

    prg = argv[0];
    start_state = NULL;

    /* some defaults, to be changed by getopt args */
    /* auto-select colour mode */
    system_colour = COLOURMODE_AUTO;
    /* default to English as language for now */
    language_english = TRUE;
    /* default to smooth graphics magnification */
    graphics_mag_smooth = 1;
    /* default to sound on */
    mute = FALSE;
    /* show every frame */
    system_frameskip_key = 1;

    while ((ch=getopt(argc, argv, "cef:ghjl:MmSsv")) != -1) {
	switch (ch) {
	case 'c':
	    system_colour = COLOURMODE_COLOUR;
	    break;
	case 'e':
	    language_english = TRUE;
	    break;
	case 'f':
	    i = atoi(optarg);
	    if (i >=1 && i <= 7)
		system_frameskip_key = i;
	    break;
	case 'g':
	    system_colour = COLOURMODE_GREYSCALE;
	    break;
	case 'h':
	    usage(1);
	    break;
	case 'j':
	    language_english = FALSE;
	    break;
	case 'l':
	    start_state = optarg;
	    break;
	case 'M':
	    graphics_mag_smooth = 0;
	    break;
	case 'm':
	    graphics_mag_smooth = 1;
	    break;
	case 'S':
	    mute = TRUE;
	    break;
	case 's':
	    mute = FALSE;
	    break;
	case 'v':
	    printversion();
	    exit(0);
	    break;
	default:
	    usage(1);
	}
    }

    argc -= optind;
    argv += optind;

    /* Fill BIOS buffer */
    if (bios_install() == FALSE) {
	fprintf(stderr, "cannot install BIOS\n");
	exit(1);
    }

    /* initialize SDL and create standard-sized surface */
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
       fprintf(stderr, "cannot initialize SDL: %s\n", SDL_GetError());
       exit(1);
    }
    atexit(SDL_Quit);

    if (system_graphics_init() == FALSE) {
	fprintf(stderr, "cannot create window: %s\n", SDL_GetError());
	exit(1);
    }

    if (mute == FALSE && system_sound_init() == FALSE) {
	fprintf(stderr, "cannot turn on sound: %s\n", SDL_GetError());
	mute = TRUE;
    }

    /*
     * Throttle rate is number_of_ticks_per_second divided by number
     * of complete frames that should be shown per second.
     * In this case, both are constants;
     */
    throttle_rate = 1000000/NGP_FPS;

    if (argc > 0) {
	if (system_rom_load(argv[0]) == FALSE)
	    exit(1);
    }
    else {
	fprintf(stderr, "no ROM loaded\n");
    }

    reset();
    SDL_PauseAudio(0);
    if (start_state != NULL)
	state_restore(start_state);

    gettimeofday(&throttle_last, NULL);
    i = 200;
    do {
	emulate();
	if (i-- == 0 && !mute) {
	    /* for the sound thread */
	    SDL_Delay(1);
	    i = 200;
	}
    } while (do_exit == 0);

    system_rom_unload();
    system_sound_shutdown();

    return 0;
}
