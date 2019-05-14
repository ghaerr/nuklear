/*
 * Copyright (c) 2019 Greg Haerr <greg@censoft.com>
 *
 * Nuklear demo for Nano-X
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
*/
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_IMPLEMENTATION
#define NK_XLIB_IMPLEMENTATION
/*#define NK_XLIB_INCLUDE_STB_IMAGE*/
#include "../../nuklear.h"
#include "nano-X.h"
#include "nuklear_nxlib.h"

#define WINDOW_WIDTH    800
#define WINDOW_HEIGHT   600

typedef struct NXWindow NXWindow;
struct NXWindow {
    GR_WINDOW_ID win;
    NXFont *font;
};

static void
die(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputs("\n", stderr);
    exit(EXIT_FAILURE);
}

/* ===============================================================
 *
 *                          EXAMPLE
 *
 * ===============================================================*/
/* This are some code examples to provide a small overview of what can be
 * done with this library. To try out an example uncomment the defines */
#define INCLUDE_ALL
/*#define INCLUDE_STYLE */
/*#define INCLUDE_CALCULATOR */
/*#define INCLUDE_OVERVIEW */
/*#define INCLUDE_NODE_EDITOR */

#ifdef INCLUDE_ALL
  #define INCLUDE_STYLE
  #define INCLUDE_CALCULATOR
  #define INCLUDE_OVERVIEW
  #define INCLUDE_NODE_EDITOR
#endif

#ifdef INCLUDE_STYLE
  #include "../style.c"
#endif
#ifdef INCLUDE_CALCULATOR
  #include "../calculator.c"
#endif
#ifdef INCLUDE_OVERVIEW
  #include "../overview.c"
#endif
#ifdef INCLUDE_NODE_EDITOR
  #include "../node_editor.c"
#endif

/* ===============================================================
 *
 *                          DEMO
 *
 * ===============================================================*/
int
main(void)
{
    int running = 1;
    NXWindow nxw;
    struct nk_context *ctx;
	GR_WM_PROPS props;

    /* Nano-X*/
    memset(&nxw, 0, sizeof nxw);
	if (GrOpen() < 0)
    	die("Unable to open graphics");

	/*props = GR_WM_PROPS_CAPTION | GR_WM_PROPS_BORDER;*/
	props = GR_WM_PROPS_APPWINDOW;
	nxw.win = GrNewBufferedWindow(props, "Nuklear on Microwindows", GR_ROOT_WINDOW_ID,
		0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, GR_RGB(30,30,30));
	GrSelectEvents(nxw.win, GR_EVENT_MASK_CLOSE_REQ | GR_EVENT_MASK_UPDATE |
		GR_EVENT_MASK_KEY_DOWN | GR_EVENT_MASK_KEY_UP |
		GR_EVENT_MASK_BUTTON_DOWN | GR_EVENT_MASK_BUTTON_UP | GR_EVENT_MASK_MOUSE_POSITION);
	GrMapWindow(nxw.win);

    /* GUI */
    nxw.font = nk_xfont_create(GR_FONT_SYSTEM_VAR);
    ctx = nk_xlib_init(nxw.font, nxw.win, WINDOW_WIDTH, WINDOW_HEIGHT);

    #ifdef INCLUDE_STYLE
    /*set_style(ctx, THEME_WHITE);*/
    /*set_style(ctx, THEME_RED);*/
    /*set_style(ctx, THEME_BLUE);*/
    /*set_style(ctx, THEME_DARK);*/
    #endif

    while (running)
    {
        /* Input */
        GR_EVENT evt;
        nk_input_begin(ctx);
		do {
			GrGetNextEvent(&evt);
            if (evt.type == GR_EVENT_TYPE_CLOSE_REQ)
				running = 0;
            nk_xlib_handle_event(&evt);
        }
        while (running && GrPeekEvent(&evt));
        nk_input_end(ctx);

		if (!running)
			break;

        /* GUI */
        if (nk_begin(ctx, "Demo", nk_rect(50, 50, 200, 200),
            NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
            NK_WINDOW_CLOSABLE|NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE))
        {
            enum {EASY, HARD};
            static int op = EASY;
            static int property = 20;

            nk_layout_row_static(ctx, 30, 80, 1);
            if (nk_button_label(ctx, "button"))
                fprintf(stdout, "button pressed\n");
            nk_layout_row_dynamic(ctx, 30, 2);
            if (nk_option_label(ctx, "easy", op == EASY)) op = EASY;
            if (nk_option_label(ctx, "hard", op == HARD)) op = HARD;
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_property_int(ctx, "Compression:", 0, &property, 100, 10, 1);
        }
        nk_end(ctx);
        if (nk_window_is_hidden(ctx, "Demo")) break;

        /* -------------- EXAMPLES ---------------- */
        #ifdef INCLUDE_CALCULATOR
          calculator(ctx);
        #endif
        #ifdef INCLUDE_OVERVIEW
          overview(ctx);
        #endif
        #ifdef INCLUDE_NODE_EDITOR
          node_editor(ctx);
        #endif
        /* ----------------------------------------- */

        /* Draw */
        nk_xlib_render(nxw.win, nk_rgb(30,30,30));
		GrFlushWindow(nxw.win);
        GrFlush();
    }

    nk_xfont_del(nxw.font);
    nk_xlib_shutdown();
	GrDestroyWindow(nxw.win);
	GrClose();
    return 0;
}
