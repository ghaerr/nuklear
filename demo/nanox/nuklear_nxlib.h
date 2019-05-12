/*
 * Copyright (c) 2019 Greg Haerr <greg@censoft.com>
 *
 * Nuklear port to Nano-X
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
/*
 * ==============================================================
 *
 *                              API
 *
 * ===============================================================
 */
#ifndef NK_XLIB_H_
#define NK_XLIB_H_

#include "nano-X.h"

typedef struct NXFont NXFont;
NK_API struct nk_context*   nk_xlib_init(NXFont *nxfont, GR_WINDOW_ID wid, unsigned w, unsigned h);
NK_API int                  nk_xlib_handle_event(GR_EVENT *evt);
NK_API void                 nk_xlib_render(GR_WINDOW_ID win, struct nk_color clear);
NK_API void                 nk_xlib_shutdown(void);
NK_API void                 nk_xlib_set_font(NXFont*);
NK_API void                 nk_xlib_push_font(NXFont*);

/* Image */
#ifdef NK_XLIB_INCLUDE_STB_IMAGE
NK_API struct nk_image nk_xsurf_load_image_from_file(char const *filename);
NK_API struct nk_image nk_xsurf_load_image_from_memory(const void *membuf, nk_uint membufSize);
#endif

/* Font */
NK_API NXFont *             nk_xfont_create(const char *name);
NK_API void                 nk_xfont_del(NXFont *font);

#endif
/*
 * ==============================================================
 *
 *                          IMPLEMENTATION
 *
 * ===============================================================
 */
#ifdef NK_XLIB_IMPLEMENTATION
#include "nano-X.h"

#include <sys/time.h>
#include <unistd.h>
#include <time.h>


#ifdef NK_XLIB_IMPLEMENT_STB_IMAGE
#define STB_IMAGE_IMPLEMENTATION
#endif

#ifdef NK_XLIB_INCLUDE_STB_IMAGE
#include "../../example/stb_image.h"
#endif


#ifndef NK_X11_DOUBLE_CLICK_LO
#define NK_X11_DOUBLE_CLICK_LO 20
#endif
#ifndef NK_X11_DOUBLE_CLICK_HI
#define NK_X11_DOUBLE_CLICK_HI 200
#endif

struct NXFont {
	GR_FONT_ID fontid;
    GR_GC_ID gc;
    int ascent;
    int descent;
    int height;
    struct nk_user_font handle;
};

typedef struct NXSurface NXSurface;
struct NXSurface {
	GR_WINDOW_ID wid;
    GR_GC_ID gc;
	GR_REGION_ID clip;
    unsigned int w, h;
};

static struct {
    struct nk_context ctx;
	GR_WINDOW_ID wid;
    struct NXSurface *surf;
    long last_button_click;
} nxlib;

#if 0000
typedef struct XImageWithAlpha XImageWithAlpha;
struct XImageWithAlpha {
    XImage* ximage;
    GC clipMaskGC;
    Pixmap clipMask;
};
#endif

NK_INTERN long
nk_timestamp(void)
{
    struct timeval tv;
    if (gettimeofday(&tv, NULL) < 0) return 0;
    return (long)((long)tv.tv_sec * 1000 + (long)tv.tv_usec/1000);
}

NK_INTERN unsigned long
nk_color_from_byte(const nk_byte *c)
{
    return GR_RGB(c[0], c[1], c[2]);
}

NK_INTERN NXSurface*
nk_xsurf_create(GR_WINDOW_ID wid, unsigned int w, unsigned int h)
{
    NXSurface *surface = (NXSurface*)calloc(1, sizeof(NXSurface));
    surface->wid = wid;
    surface->gc = GrNewGC();
	surface->clip = 0;
    surface->w = w;
    surface->h = h;
	GrSetGCUseBackground(surface->gc, GR_FALSE);
    //XSetLineAttributes(xlib.dpy, surface->gc, 1, LineSolid, CapButt, JoinMiter);
    return surface;
}

#if 0000
NK_INTERN void
nk_xsurf_resize(NXSurface *surf, unsigned int w, unsigned int h)
{
    if(!surf) return;
    if (surf->w == w && surf->h == h) return;
    surf->w = w; surf->h = h;
    if(surf->drawable) XFreePixmap(surf->dpy, surf->drawable);
    surf->drawable = XCreatePixmap(surf->dpy, surf->root, w, h,
        (unsigned int)DefaultDepth(surf->dpy, surf->screen));
}
#endif

NK_INTERN void
nk_xsurf_scissor(NXSurface *surf, float x, float y, float w, float h)
{
    GR_RECT clip_rect;
    clip_rect.x = (short)(x-1);		//FIXME
    clip_rect.y = (short)(y-1);
    clip_rect.width = (unsigned short)(w+2);
    clip_rect.height = (unsigned short)(h+2);
	if (surf->clip)
		GrDestroyRegion(surf->clip);
	surf->clip = GrNewRegion();
	GrUnionRectWithRegion(surf->clip, &clip_rect);
	GrSetGCRegion(surf->gc, surf->clip);
}

NK_INTERN void
nk_xsurf_stroke_line(NXSurface *surf, short x0, short y0, short x1,
    short y1, unsigned int line_thickness, struct nk_color col)
{
    unsigned long c = nk_color_from_byte(&col.r);
    GrSetGCForeground(surf->gc, c);
    //XSetLineAttributes(surf->dpy, surf->gc, line_thickness, LineSolid, CapButt, JoinMiter);
    GrLine(surf->wid, surf->gc, (int)x0, (int)y0, (int)x1, (int)y1);
    //XSetLineAttributes(surf->dpy, surf->gc, 1, LineSolid, CapButt, JoinMiter);
}

/*
 * X11 compatible DrawArc and FillArc translation routine
 * X11 angle1=start, angle2=distance (negative=clockwise)
 */
NK_INTERN void
GrDrawArcPie(GR_DRAW_ID d, GR_GC_ID gc, GR_COORD x, GR_COORD y, GR_SIZE width, GR_SIZE height,
	GR_COORD angle1, GR_COORD angle2, int mode)
{
	GR_SIZE rx, ry;
	GR_COORD startAngle, endAngle;
	const GR_COORD FULLCIRCLE = 360 * 64;

	/* don't draw anything if no arc requested*/
	if (angle2 == 0)
		return;

#if 0
	/*
	 * Convert X11 width/height to Nano-X radius required
	 * for arc drawing.
	 * This causes problems when width is even, so we
	 * punt for the time being.  This causes smaller
	 * arcs to be drawn than required, but always within
	 * the width/height bounding box.
	 */
	if (!(width & 1))
		--width;
	if (!(height & 1))
		--height;
#endif
	rx = width / 2;
	ry = height / 2;
	/*
	 * Convert X11 start/distance angles to Nano-X start/end angles.
	 */
	if (angle1 == 0 && angle2 >= FULLCIRCLE) {
		startAngle = 0;
		endAngle = 0;
	} else {
		if (angle2 > FULLCIRCLE)
			angle2 = FULLCIRCLE;
		else if (angle2 < -FULLCIRCLE)
			angle2 = -FULLCIRCLE;
		if (angle2 < 0) {
			startAngle = angle1 + angle2;
			endAngle = angle1;
		} else {
			startAngle = angle1;
			endAngle = angle1 + angle2;
		}
		if (startAngle < 0)
			startAngle = FULLCIRCLE - (-startAngle) % FULLCIRCLE;
		if (startAngle >= FULLCIRCLE)
			startAngle = startAngle % FULLCIRCLE;
		if (endAngle < 0)
			endAngle = FULLCIRCLE - (-endAngle) % FULLCIRCLE;
		if (endAngle >= FULLCIRCLE)
			endAngle = endAngle % FULLCIRCLE;
	}
	GrArcAngle(d, gc, x+rx, y+ry, rx, ry, startAngle, endAngle, mode);
}

/* X11 compatible DrawArc*/
NK_INTERN void
GrDrawArc(GR_DRAW_ID d, GR_GC_ID gc, GR_COORD x, GR_COORD y,
	GR_SIZE width, GR_SIZE height, GR_COORD angle1, GR_COORD angle2)
{
	/* X11 width/height is one less than Nano-X width/height*/
	GrDrawArcPie(d, gc, x, y, width+1, height+1, angle1, angle2, GR_ARC);
}

/* X11 compatible FillArc*/
NK_INTERN void
GrFillArc(GR_DRAW_ID d, GR_GC_ID gc, GR_COORD x, GR_COORD y,
	GR_SIZE width, GR_SIZE height, GR_COORD angle1, GR_COORD angle2)
{
	/* X11 width/height is one less than Nano-X width/height*/
	GrDrawArcPie(d, gc, x, y, width+1, height+1, angle1, angle2, GR_PIE);
}

NK_INTERN void
nk_xsurf_stroke_rect(NXSurface* surf, short x, short y, unsigned short w,
    unsigned short h, unsigned short r, unsigned short line_thickness, struct nk_color col)
{
    unsigned long c = nk_color_from_byte(&col.r);

    GrSetGCForeground(surf->gc, c);
    //XSetLineAttributes(surf->dpy, surf->gc, line_thickness, LineSolid, CapButt, JoinMiter);
    if (r == 0) {
		GrRect(surf->wid, surf->gc, x, y, w+1, h+1);	//FIXME note +1 for width/height
		return;
	}

    {short xc = x + r;
    short yc = y + r;
    short wc = (short)(w - 2 * r);
    short hc = (short)(h - 2 * r);

    GrLine(surf->wid, surf->gc, xc, y, xc+wc, y);
    GrLine(surf->wid, surf->gc, x+w, yc, x+w, yc+hc);
    GrLine(surf->wid, surf->gc, xc, y+h, xc+wc, y+h);
    GrLine(surf->wid, surf->gc, x, yc, x, yc+hc);

    GrDrawArc(surf->wid, surf->gc, xc + wc - r, y,
        (unsigned)r*2, (unsigned)r*2, 0 * 64, 90 * 64);
    GrDrawArc(surf->wid, surf->gc, x, y,
        (unsigned)r*2, (unsigned)r*2, 90 * 64, 90 * 64);
    GrDrawArc(surf->wid, surf->gc, x, yc + hc - r,
        (unsigned)r*2, (unsigned)2*r, 180 * 64, 90 * 64);
    GrDrawArc(surf->wid, surf->gc, xc + wc - r, yc + hc - r,
        (unsigned)r*2, (unsigned)2*r, -90 * 64, 90 * 64);}
    //XSetLineAttributes(surf->dpy, surf->gc, 1, LineSolid, CapButt, JoinMiter);
}

NK_INTERN void
nk_xsurf_fill_rect(NXSurface* surf, short x, short y, unsigned short w,
    unsigned short h, unsigned short r, struct nk_color col)
{
    unsigned long c = nk_color_from_byte(&col.r);
    GrSetGCForeground(surf->gc, c);
    if (r == 0) {
		GrFillRect(surf->wid, surf->gc, x, y, w, h);
		return;
	}

    {short xc = x + r;
    short yc = y + r;
    short wc = (short)(w - 2 * r);
    short hc = (short)(h - 2 * r);

    GR_POINT pnts[12];
    pnts[0].x = x;
    pnts[0].y = yc;
    pnts[1].x = xc;
    pnts[1].y = yc;
    pnts[2].x = xc;
    pnts[2].y = y;

    pnts[3].x = xc + wc;
    pnts[3].y = y;
    pnts[4].x = xc + wc;
    pnts[4].y = yc;
    pnts[5].x = x + w;
    pnts[5].y = yc;

    pnts[6].x = x + w;
    pnts[6].y = yc + hc;
    pnts[7].x = xc + wc;
    pnts[7].y = yc + hc;
    pnts[8].x = xc + wc;
    pnts[8].y = y + h;

    pnts[9].x = xc;
    pnts[9].y = y + h;
    pnts[10].x = xc;
    pnts[10].y = yc + hc;
    pnts[11].x = x;
    pnts[11].y = yc + hc;

    GrFillPoly(surf->wid, surf->gc, 12, pnts);
    GrFillArc(surf->wid, surf->gc, xc + wc - r, y,
        (unsigned)r*2, (unsigned)r*2, 0 * 64, 90 * 64);
    GrFillArc(surf->wid, surf->gc, x, y,
        (unsigned)r*2, (unsigned)r*2, 90 * 64, 90 * 64);
    GrFillArc(surf->wid, surf->gc, x, yc + hc - r,
        (unsigned)r*2, (unsigned)2*r, 180 * 64, 90 * 64);
    GrFillArc(surf->wid, surf->gc, xc + wc - r, yc + hc - r,
        (unsigned)r*2, (unsigned)2*r, -90 * 64, 90 * 64);}
}

NK_INTERN void
nk_xsurf_fill_triangle(NXSurface *surf, short x0, short y0, short x1,
    short y1, short x2, short y2, struct nk_color col)
{
    GR_POINT pnts[3];
    unsigned long c = nk_color_from_byte(&col.r);

    pnts[0].x = (short)x0;
    pnts[0].y = (short)y0;
    pnts[1].x = (short)x1;
    pnts[1].y = (short)y1;
    pnts[2].x = (short)x2;
    pnts[2].y = (short)y2;
    GrSetGCForeground(surf->gc, c);
    GrFillPoly(surf->wid, surf->gc, 3, pnts);
}

NK_INTERN void
nk_xsurf_stroke_triangle(NXSurface *surf, short x0, short y0, short x1,
    short y1, short x2, short y2, unsigned short line_thickness, struct nk_color col)
{
    unsigned long c = nk_color_from_byte(&col.r);

    GrSetGCForeground(surf->gc, c);
    //XSetLineAttributes(surf->dpy, surf->gc, line_thickness, LineSolid, CapButt, JoinMiter);
    GrLine(surf->wid, surf->gc, x0, y0, x1, y1);
    GrLine(surf->wid, surf->gc, x1, y1, x2, y2);
    GrLine(surf->wid, surf->gc, x2, y2, x0, y0);
    //XSetLineAttributes(surf->dpy, surf->gc, 1, LineSolid, CapButt, JoinMiter);
}

NK_INTERN void
nk_xsurf_fill_polygon(NXSurface *surf,  const struct nk_vec2i *pnts, int count,
    struct nk_color col)
{
    int i = 0;
    #define MAX_POINTS 128
    GR_POINT xpnts[MAX_POINTS];
    unsigned long c = nk_color_from_byte(&col.r);

    GrSetGCForeground(surf->gc, c);
    for (i = 0; i < count && i < MAX_POINTS; ++i) {
        xpnts[i].x = pnts[i].x;
        xpnts[i].y = pnts[i].y;
    }
    GrFillPoly(surf->wid, surf->gc, count, xpnts);
    #undef MAX_POINTS
}

NK_INTERN void
nk_xsurf_stroke_polygon(NXSurface *surf, const struct nk_vec2i *pnts, int count,
    unsigned short line_thickness, struct nk_color col)
{
    int i = 0;
    unsigned long c = nk_color_from_byte(&col.r);

    GrSetGCForeground(surf->gc, c);
    //XSetLineAttributes(surf->dpy, surf->gc, line_thickness, LineSolid, CapButt, JoinMiter);
    for (i = 1; i < count; ++i)
        GrLine(surf->wid, surf->gc, pnts[i-1].x, pnts[i-1].y, pnts[i].x, pnts[i].y);
    GrLine(surf->wid, surf->gc, pnts[count-1].x, pnts[count-1].y, pnts[0].x, pnts[0].y);
    //XSetLineAttributes(surf->dpy, surf->gc, 1, LineSolid, CapButt, JoinMiter);
}

NK_INTERN void
nk_xsurf_stroke_polyline(NXSurface *surf, const struct nk_vec2i *pnts,
    int count, unsigned short line_thickness, struct nk_color col)
{
    int i = 0;
    unsigned long c = nk_color_from_byte(&col.r);

    //XSetLineAttributes(surf->dpy, surf->gc, line_thickness, LineSolid, CapButt, JoinMiter);
    GrSetGCForeground(surf->gc, c);
    for (i = 0; i < count-1; ++i)
        GrLine(surf->wid, surf->gc, pnts[i].x, pnts[i].y, pnts[i+1].x, pnts[i+1].y);
    //XSetLineAttributes(surf->dpy, surf->gc, 1, LineSolid, CapButt, JoinMiter);
}

NK_INTERN void
nk_xsurf_fill_circle(NXSurface *surf, short x, short y, unsigned short w,
    unsigned short h, struct nk_color col)
{
    unsigned long c = nk_color_from_byte(&col.r);
	int rx = w/2 - 1;
	int ry = h/2 - 1;

    GrSetGCForeground(surf->gc, c);
	GrFillEllipse(surf->wid, surf->gc, x+rx, y+ry, rx, ry);
}

NK_INTERN void
nk_xsurf_stroke_circle(NXSurface *surf, short x, short y, unsigned short w,
    unsigned short h, unsigned short line_thickness, struct nk_color col)
{
    unsigned long c = nk_color_from_byte(&col.r);
	int rx = w/2 - 1;
	int ry = h/2 - 1;

    GrSetGCForeground(surf->gc, c);
    //XSetLineAttributes(surf->dpy, surf->gc, line_thickness, LineSolid, CapButt, JoinMiter);
	GrEllipse(surf->wid, surf->gc, x+rx, y+ry, rx, ry);
    //XSetLineAttributes(surf->dpy, surf->gc, 1, LineSolid, CapButt, JoinMiter);
}

NK_INTERN void
nk_xsurf_stroke_curve(NXSurface *surf, struct nk_vec2i p1,
    struct nk_vec2i p2, struct nk_vec2i p3, struct nk_vec2i p4,
    unsigned int num_segments, unsigned short line_thickness, struct nk_color col)
{
    unsigned int i_step;
    float t_step;
    struct nk_vec2i last = p1;

    //XSetLineAttributes(surf->dpy, surf->gc, line_thickness, LineSolid, CapButt, JoinMiter);
    num_segments = NK_MAX(num_segments, 1);
    t_step = 1.0f/(float)num_segments;
    for (i_step = 1; i_step <= num_segments; ++i_step) {
        float t = t_step * (float)i_step;
        float u = 1.0f - t;
        float w1 = u*u*u;
        float w2 = 3*u*u*t;
        float w3 = 3*u*t*t;
        float w4 = t * t *t;
        float x = w1 * p1.x + w2 * p2.x + w3 * p3.x + w4 * p4.x;
        float y = w1 * p1.y + w2 * p2.y + w3 * p3.y + w4 * p4.y;
        nk_xsurf_stroke_line(surf, last.x, last.y, (short)x, (short)y, line_thickness,col);
        last.x = (short)x; last.y = (short)y;
    }
    //XSetLineAttributes(surf->dpy, surf->gc, 1, LineSolid, CapButt, JoinMiter);
}

NK_INTERN void
nk_xsurf_draw_text(NXSurface *surf, short x, short y, unsigned short w, unsigned short h,
    const char *text, int len, NXFont *font, struct nk_color cbg, struct nk_color cfg)
{
    int tx, ty;
    unsigned long bg = nk_color_from_byte(&cbg.r);
    unsigned long fg = nk_color_from_byte(&cfg.r);

    GrSetGCForeground(surf->gc, bg);
    GrFillRect(surf->wid, surf->gc, x, y, w, h);
    if(!text || !font || !len) return;

    tx = x;
    ty = y + font->ascent;
    GrSetGCForeground(surf->gc, fg);
    GrText(surf->wid, surf->gc, tx, ty, (void*)text, len, GR_TFBASELINE);
}


#if 0000
#ifdef NK_XLIB_INCLUDE_STB_IMAGE
NK_INTERN struct nk_image
nk_stbi_image_to_xsurf(unsigned char *data, int width, int height, int channels) {
    NXSurface *surf = xlib.surf;
    struct nk_image img;
    int bpl = channels;
    long i, isize = width*height*channels;
    XImageWithAlpha *aimage = (XImageWithAlpha*)calloc( 1, sizeof(XImageWithAlpha) );
    int depth = DefaultDepth(surf->dpy, surf->screen); 
    if (data == NULL) return nk_image_id(0);
    if (aimage == NULL) return nk_image_id(0);
    
    switch (depth){
        case 24:
            bpl = 4;
        break;
        case 16:
        case 15:
            bpl = 2;
        break;
        default:
            bpl = 1;
        break;
    }
    
    /* rgba to bgra */
    if (channels >= 3){
        for (i=0; i < isize; i += channels) {
            unsigned char red  = data[i+2];
            unsigned char blue = data[i];
            data[i]   = red;
            data[i+2] = blue;
        }
    }

    if (channels == 4){
        const unsigned alpha_treshold = 127;        
        aimage->clipMask = XCreatePixmap(surf->dpy, surf->drawable, width, height, 1);
        
        if( aimage->clipMask ){
            aimage->clipMaskGC = XCreateGC(surf->dpy, aimage->clipMask, 0, 0);
            XSetForeground(surf->dpy, aimage->clipMaskGC, BlackPixel(surf->dpy, surf->screen));
            XFillRectangle(surf->dpy, aimage->clipMask, aimage->clipMaskGC, 0, 0, width, height);

            XSetForeground(surf->dpy, aimage->clipMaskGC, WhitePixel(surf->dpy, surf->screen));
            for (i=0; i < isize; i += channels){
                unsigned char alpha = data[i+3];
                int div = i / channels;
                int x = div % width;
                int y = div / width;
                if( alpha > alpha_treshold )
                    XDrawPoint(surf->dpy, aimage->clipMask, aimage->clipMaskGC, x, y);
            }
        }
    }
    
    aimage->ximage = XCreateImage(surf->dpy, 
           CopyFromParent, depth, 
           ZPixmap, 0, 
           (char*)data, 
           width, height, 
           bpl*8, bpl * width); 
    img = nk_image_ptr( (void*)aimage);
    img.h = height;
    img.w = width;
    return img;
}

NK_API struct nk_image
nk_xsurf_load_image_from_memory(const void *membuf, nk_uint membufSize)
{
    int x,y,n;
    unsigned char *data;
    data = stbi_load_from_memory(membuf, membufSize, &x, &y, &n, 0);
    return nk_stbi_image_to_xsurf(data, x, y, n);
}

NK_API struct nk_image
nk_xsurf_load_image_from_file(char const *filename)
{
    int x,y,n;
    unsigned char *data;
    data = stbi_load(filename, &x, &y, &n, 0);
    return nk_stbi_image_to_xsurf(data, x, y, n);
}
#endif /* NK_XLIB_INCLUDE_STB_IMAGE */

NK_INTERN void
nk_xsurf_draw_image(NXSurface *surf, short x, short y, unsigned short w, unsigned short h,
    struct nk_image img, struct nk_color col)
{
    XImageWithAlpha *aimage = img.handle.ptr;
    if (aimage){
        if (aimage->clipMask){
            XSetClipMask(surf->dpy, surf->gc, aimage->clipMask);
            XSetClipOrigin(surf->dpy, surf->gc, x, y); 
        }
        XPutImage(surf->dpy, surf->drawable, surf->gc, aimage->ximage, 0, 0, x, y, w, h);
        XSetClipMask(surf->dpy, surf->gc, None);
    }
}

void
nk_xsurf_image_free(struct nk_image* image)
{
    NXSurface *surf = xlib.surf;
    XImageWithAlpha *aimage = image->handle.ptr;
    if (!aimage) return;
    XDestroyImage(aimage->ximage);
    XFreePixmap(surf->dpy, aimage->clipMask);
    XFreeGC(surf->dpy, aimage->clipMaskGC);
    free(aimage);
}
#endif

NK_INTERN void
nk_xsurf_clear(NXSurface *surf, unsigned long color)
{
	GrSetGCForeground(surf->gc, color);
	GrFillRect(surf->wid, surf->gc, 0, 0, surf->w, surf->h);
}

NK_INTERN void
nk_xsurf_del(NXSurface *surf)
{
    GrDestroyGC(surf->gc);
	if (surf->clip)
		GrDestroyRegion(surf->clip);
    free(surf);
}

NK_API NXFont*
nk_xfont_create(const char *name)
{
    NXFont *font;
	GR_FONT_INFO finfo;
	GR_FONT_ID fontid = GrCreateFontEx(name, 0, 0, NULL);
	if (!fontid)
		fontid = GrCreateFontEx(GR_FONT_SYSTEM_FIXED, 0, 0, NULL);
	if (!fontid)
		return 0;

    font = (NXFont*)calloc(1, sizeof(NXFont));
	font->fontid = fontid;
	font->gc = GrNewGC();
	GrSetGCUseBackground(font->gc, GR_FALSE);
	GrSetGCFont(font->gc, fontid);

	GrGetFontInfo(fontid, &finfo);
	font->ascent = finfo.baseline;
	font->descent = finfo.descent;
	font->height = finfo.height;

    return font;
}

NK_INTERN float
nk_xfont_get_text_width(nk_handle handle, float height, const char *text, int len)
{
    NXFont *font = (NXFont*)handle.ptr;
	GR_SIZE w = 0, h, b;
    if(!font || !text)
        return 0;

	GrGetGCTextSize(font->gc, (void *)text, len, 0, &w, &h, &b);
	return (float)w;
}

NK_API void
nk_xfont_del(NXFont *font)
{
    if(!font) return;

	GrDestroyFont(font->fontid);
	GrDestroyGC(font->gc);
    free(font);
}

NK_API struct nk_context*
nk_xlib_init(NXFont *nxfont, GR_WINDOW_ID wid, unsigned int w, unsigned int h)
{
    struct nk_user_font *font = &nxfont->handle;
    font->userdata = nk_handle_ptr(nxfont);
    font->height = (float)nxfont->height;
    font->width = nk_xfont_get_text_width;

    /* create invisible cursor */
    //{static XColor dummy; char data[1] = {0};
    //Pixmap blank = XCreateBitmapFromData(dpy, root, data, 1, 1);
    //if (blank == None) return 0;
    //xlib.cursor = XCreatePixmapCursor(dpy, blank, blank, &dummy, &dummy, 0, 0);
    //XFreePixmap(dpy, blank);}

    nxlib.surf = nk_xsurf_create(wid, w, h);
	nxlib.wid = wid;
    nk_init_default(&nxlib.ctx, font);
    return &nxlib.ctx;
}

NK_API void
nk_xlib_set_font(NXFont *xfont)
{
    struct nk_user_font *font = &xfont->handle;
    font->userdata = nk_handle_ptr(xfont);
    font->height = (float)xfont->height;
    font->width = nk_xfont_get_text_width;
    nk_style_set_font(&nxlib.ctx, font);
}

NK_API void
nk_xlib_push_font(NXFont *xfont)
{
    struct nk_user_font *font = &xfont->handle;
    font->userdata = nk_handle_ptr(xfont);
    font->height = (float)xfont->height;
    font->width = nk_xfont_get_text_width;
    nk_style_push_font(&nxlib.ctx, font);
}

NK_API int
nk_xlib_handle_event(GR_EVENT *evt)
{
    struct nk_context *ctx = &nxlib.ctx;

	if (evt->type == GR_EVENT_TYPE_NONE) return 0;
//printf("got %d %d,%d %d\n", evt->type, evt->update.wid, evt->update.subwid, evt->update.utype);
//if (evt->mouse.wid != nxlib.wid) { printf("ERROR type %d bad wid %d,%d\n", evt->type, evt->mouse.wid, nxlib.wid); return 0; }
    /* optional grabbing behavior */
    if (ctx->input.mouse.grab) {
        //XDefineCursor(xlib.dpy, xlib.root, xlib.cursor);
        ctx->input.mouse.grab = 0;
    } else if (ctx->input.mouse.ungrab) {
        //XWarpPointer(xlib.dpy, None, xlib.root, 0, 0, 0, 0,
            //(int)ctx->input.mouse.prev.x, (int)ctx->input.mouse.prev.y);
        //XUndefineCursor(xlib.dpy, xlib.root);
        ctx->input.mouse.ungrab = 0;
    }

    if (evt->type == GR_EVENT_TYPE_KEY_DOWN || evt->type == GR_EVENT_TYPE_KEY_UP)
    {
        /* Key handler */
        int ret, down = (evt->type == GR_EVENT_TYPE_KEY_DOWN);
		MWKEY code = evt->keystroke.ch;
        if (code == MWKEY_LSHIFT || code == MWKEY_RSHIFT) nk_input_key(ctx, NK_KEY_SHIFT, down);
        else if (code == MWKEY_DELETE)    nk_input_key(ctx, NK_KEY_DEL, down);
        else if (code == MWKEY_ENTER)    nk_input_key(ctx, NK_KEY_ENTER, down);
        else if (code == MWKEY_TAB)       nk_input_key(ctx, NK_KEY_TAB, down);
        else if (code == MWKEY_LEFT)      nk_input_key(ctx, NK_KEY_LEFT, down);
        else if (code == MWKEY_RIGHT)     nk_input_key(ctx, NK_KEY_RIGHT, down);
        else if (code == MWKEY_UP)        nk_input_key(ctx, NK_KEY_UP, down);
        else if (code == MWKEY_DOWN)      nk_input_key(ctx, NK_KEY_DOWN, down);
        else if (code == MWKEY_BACKSPACE) nk_input_key(ctx, NK_KEY_BACKSPACE, down);
        else if (code == MWKEY_ESCAPE)    nk_input_key(ctx, NK_KEY_TEXT_RESET_MODE, down);
        else if (code == MWKEY_PAGEUP)   nk_input_key(ctx, NK_KEY_SCROLL_UP, down);
        else if (code == MWKEY_PAGEDOWN) nk_input_key(ctx, NK_KEY_SCROLL_DOWN, down);
        else if (code == MWKEY_HOME) {
            nk_input_key(ctx, NK_KEY_TEXT_START, down);
            nk_input_key(ctx, NK_KEY_SCROLL_START, down);
        } else if (code == MWKEY_END) {
            nk_input_key(ctx, NK_KEY_TEXT_END, down);
            nk_input_key(ctx, NK_KEY_SCROLL_END, down);
        } else {
            if (code == 'c' && (evt->keystroke.modifiers & MWKMOD_CTRL))
                nk_input_key(ctx, NK_KEY_COPY, down);
            else if (code == 'v' && (evt->keystroke.modifiers & MWKMOD_CTRL))
                nk_input_key(ctx, NK_KEY_PASTE, down);
            else if (code == 'x' && (evt->keystroke.modifiers & MWKMOD_CTRL))
                nk_input_key(ctx, NK_KEY_CUT, down);
            else if (code == 'z' && (evt->keystroke.modifiers & MWKMOD_CTRL))
                nk_input_key(ctx, NK_KEY_TEXT_UNDO, down);
            else if (code == 'r' && (evt->keystroke.modifiers & MWKMOD_CTRL))
                nk_input_key(ctx, NK_KEY_TEXT_REDO, down);
            else if (code == MWKEY_LEFT && (evt->keystroke.modifiers & MWKMOD_CTRL))
                nk_input_key(ctx, NK_KEY_TEXT_WORD_LEFT, down);
            else if (code == MWKEY_RIGHT && (evt->keystroke.modifiers & MWKMOD_CTRL))
                nk_input_key(ctx, NK_KEY_TEXT_WORD_RIGHT, down);
            else if (code == 'b' && (evt->keystroke.modifiers & MWKMOD_CTRL))
                nk_input_key(ctx, NK_KEY_TEXT_LINE_START, down);
            else if (code == 'e' && (evt->keystroke.modifiers & MWKMOD_CTRL))
                nk_input_key(ctx, NK_KEY_TEXT_LINE_END, down);
            else {
                if (code == 'i')
                    nk_input_key(ctx, NK_KEY_TEXT_INSERT_MODE, down);
                else if (code == 'r')
                    nk_input_key(ctx, NK_KEY_TEXT_REPLACE_MODE, down);
                if (down) {
					if (code < 256) {
						char buf[32];
						buf[0] = (char)code;
						buf[1] = 0;
                    	nk_input_glyph(ctx, buf);
					}
				}
            }
        }
        return 1;
    } else if (evt->type == GR_EVENT_TYPE_BUTTON_DOWN || evt->type == GR_EVENT_TYPE_BUTTON_UP) {
        /* Button handler */
        int down = (evt->type == GR_EVENT_TYPE_BUTTON_DOWN);
        const int x = evt->button.x, y = evt->button.y;
        if ((evt->button.buttons & GR_BUTTON_SCROLLUP) && down)
            nk_input_scroll(ctx, nk_vec2(0, 1.0f));
        else if ((evt->button.buttons & GR_BUTTON_SCROLLDN) && down)
            nk_input_scroll(ctx, nk_vec2(0, -1.0f));
        else if ((evt->button.buttons & GR_BUTTON_L)
		      || (evt->button.changebuttons & GR_BUTTON_L)) {
            if (down) { /* Double-Click Button handler */
                long dt = nk_timestamp() - nxlib.last_button_click;
                if (dt > NK_X11_DOUBLE_CLICK_LO && dt < NK_X11_DOUBLE_CLICK_HI)
                    nk_input_button(ctx, NK_BUTTON_DOUBLE, x, y, nk_true);
                nxlib.last_button_click = nk_timestamp();
            } else nk_input_button(ctx, NK_BUTTON_DOUBLE, x, y, nk_false);
            nk_input_button(ctx, NK_BUTTON_LEFT, x, y, down);
        } else if ((evt->button.buttons & GR_BUTTON_M)
		        || (evt->button.changebuttons & GR_BUTTON_M))
            nk_input_button(ctx, NK_BUTTON_MIDDLE, x, y, down);
        else if ((evt->button.buttons & GR_BUTTON_R)
		      || (evt->button.changebuttons & GR_BUTTON_R))
            nk_input_button(ctx, NK_BUTTON_RIGHT, x, y, down);
        else return 0;
        return 1;
    } else if (evt->type == GR_EVENT_TYPE_MOUSE_POSITION) {
        /* Mouse motion handler */
        const int x = evt->mouse.x, y = evt->mouse.y;
        nk_input_motion(ctx, x, y);
        if (ctx->input.mouse.grabbed) {
            ctx->input.mouse.pos.x = ctx->input.mouse.prev.x;
            ctx->input.mouse.pos.y = ctx->input.mouse.prev.y;
            //XWarpPointer(xlib.dpy, None, xlib.surf->root, 0, 0, 0, 0, (int)ctx->input.mouse.pos.x, (int)ctx->input.mouse.pos.y);
        }
        return 1;
    }
#if 0
	else if (evt->type == ConfigureNotify) {
        /* Window resize handler */
        unsigned int width, height;
        XWindowAttributes attr;
        XGetWindowAttributes(dpy, win, &attr);
        width = (unsigned int)attr.width;
        height = (unsigned int)attr.height;
        nk_xsurf_resize(xlib.surf, width, height);
        return 1;
    }
#endif
    return 0;
}

NK_API void
nk_xlib_shutdown(void)
{
    nk_xsurf_del(nxlib.surf);
    nk_free(&nxlib.ctx);
    //XFreeCursor(xlib.dpy, xlib.cursor);
    nk_memset(&nxlib.ctx, 0, sizeof(nxlib.ctx));
}

NK_API void
nk_xlib_render(GR_WINDOW_ID win, struct nk_color clear)
{
    const struct nk_command *cmd;
    struct nk_context *ctx = &nxlib.ctx;
    NXSurface *surf = nxlib.surf;

    nk_xsurf_clear(nxlib.surf, nk_color_from_byte(&clear.r));
    nk_foreach(cmd, &nxlib.ctx)
    {
        switch (cmd->type) {
        case NK_COMMAND_NOP: break;
        case NK_COMMAND_SCISSOR: {
            const struct nk_command_scissor *s =(const struct nk_command_scissor*)cmd;
            nk_xsurf_scissor(surf, s->x, s->y, s->w, s->h);
        } break;
        case NK_COMMAND_LINE: {
            const struct nk_command_line *l = (const struct nk_command_line *)cmd;
            nk_xsurf_stroke_line(surf, l->begin.x, l->begin.y, l->end.x,
                l->end.y, l->line_thickness, l->color);
        } break;
        case NK_COMMAND_RECT: {
            const struct nk_command_rect *r = (const struct nk_command_rect *)cmd;
            nk_xsurf_stroke_rect(surf, r->x, r->y, NK_MAX(r->w -r->line_thickness, 0),
                NK_MAX(r->h - r->line_thickness, 0), (unsigned short)r->rounding,
                r->line_thickness, r->color);
        } break;
        case NK_COMMAND_RECT_FILLED: {
            const struct nk_command_rect_filled *r = (const struct nk_command_rect_filled *)cmd;
            nk_xsurf_fill_rect(surf, r->x, r->y, r->w, r->h,
                (unsigned short)r->rounding, r->color);
        } break;
        case NK_COMMAND_CIRCLE: {
            const struct nk_command_circle *c = (const struct nk_command_circle *)cmd;
            nk_xsurf_stroke_circle(surf, c->x, c->y, c->w, c->h, c->line_thickness, c->color);
        } break;
        case NK_COMMAND_CIRCLE_FILLED: {
            const struct nk_command_circle_filled *c = (const struct nk_command_circle_filled *)cmd;
            nk_xsurf_fill_circle(surf, c->x, c->y, c->w, c->h, c->color);
        } break;
        case NK_COMMAND_TRIANGLE: {
            const struct nk_command_triangle*t = (const struct nk_command_triangle*)cmd;
            nk_xsurf_stroke_triangle(surf, t->a.x, t->a.y, t->b.x, t->b.y,
                t->c.x, t->c.y, t->line_thickness, t->color);
        } break;
        case NK_COMMAND_TRIANGLE_FILLED: {
            const struct nk_command_triangle_filled *t = (const struct nk_command_triangle_filled *)cmd;
            nk_xsurf_fill_triangle(surf, t->a.x, t->a.y, t->b.x, t->b.y,
                t->c.x, t->c.y, t->color);
        } break;
        case NK_COMMAND_POLYGON: {
            const struct nk_command_polygon *p =(const struct nk_command_polygon*)cmd;
            nk_xsurf_stroke_polygon(surf, p->points, p->point_count, p->line_thickness,p->color);
        } break;
        case NK_COMMAND_POLYGON_FILLED: {
            const struct nk_command_polygon_filled *p = (const struct nk_command_polygon_filled *)cmd;
            nk_xsurf_fill_polygon(surf, p->points, p->point_count, p->color);
        } break;
        case NK_COMMAND_POLYLINE: {
            const struct nk_command_polyline *p = (const struct nk_command_polyline *)cmd;
            nk_xsurf_stroke_polyline(surf, p->points, p->point_count, p->line_thickness, p->color);
        } break;
        case NK_COMMAND_TEXT: {
            const struct nk_command_text *t = (const struct nk_command_text*)cmd;
            nk_xsurf_draw_text(surf, t->x, t->y, t->w, t->h,
                (const char*)t->string, t->length,
                (NXFont*)t->font->userdata.ptr,
                t->background, t->foreground);
        } break;
        case NK_COMMAND_CURVE: {
            const struct nk_command_curve *q = (const struct nk_command_curve *)cmd;
            nk_xsurf_stroke_curve(surf, q->begin, q->ctrl[0], q->ctrl[1],
                q->end, 22, q->line_thickness, q->color);
        } break;
#if 0
        case NK_COMMAND_IMAGE: {
            const struct nk_command_image *i = (const struct nk_command_image *)cmd;
            nk_xsurf_draw_image(surf, i->x, i->y, i->w, i->h, i->img, i->col);
        } break;
#endif
        case NK_COMMAND_RECT_MULTI_COLOR:
        case NK_COMMAND_ARC:
        case NK_COMMAND_ARC_FILLED:
        case NK_COMMAND_CUSTOM:
        default: break;
        }
    }
    nk_clear(ctx);
    //nk_xsurf_blit(screen, surf, surf->w, surf->h);
}
#endif
