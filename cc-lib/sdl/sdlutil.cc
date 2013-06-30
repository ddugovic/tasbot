
#include "SDL.h"
#include "SDL_image.h"
#include "sdlutil.h"
#include <string>
#include "util.h"

/* by default, use display format. but
   when in console mode (for instance)
   there is no display!! */
#ifndef USE_DISPLAY_FORMAT
# define USE_DISPLAY_FORMAT 1
#endif

/* XXX It seems that byte order doesn't affect the
   ordering of the colors within a Uint32. */
#if 0 /* SDL_BYTEORDER == SDL_BIG_ENDIAN */
  const Uint32 sdlutil::rmask = 0xff000000;
  const Uint32 sdlutil::gmask = 0x00ff0000;
  const Uint32 sdlutil::bmask = 0x0000ff00;
  const Uint32 sdlutil::amask = 0x000000ff;
  const Uint32 rshift = 24;
  const Uint32 gshift = 16;
  const Uint32 bshift = 8;
  const Uint32 ashift = 0;
#else
  const Uint32 sdlutil::rmask = 0x000000ff;
  const Uint32 sdlutil::gmask = 0x0000ff00;
  const Uint32 sdlutil::bmask = 0x00ff0000;
  const Uint32 sdlutil::amask = 0xff000000;
  const Uint32 ashift = 24;
  const Uint32 bshift = 16;
  const Uint32 gshift = 8;
  const Uint32 rshift = 0;
#endif

SDL_Surface * sdlutil::resize_canvas(SDL_Surface * s, int w, int h, Uint32 color) {
  SDL_Surface * m = makesurface(w, h);
  if (!m) return 0;

  for(int y = 0; y < h; y ++)
    for(int x = 0; x < w; x ++) {
      Uint32 c = color;
      if (y < s->h && x < s->w)
	c = getpixel(s, x, y);
      setpixel(m, x, y, c);
    }

  return m;
}

SDL_Surface * sdlutil::duplicate(SDL_Surface * surf) {
  return SDL_ConvertSurface(surf, surf->format, surf->flags);
}

void sdlutil::eatevents(int ticks, Uint32 mask) {

  int now = SDL_GetTicks ();
  int destiny = now + ticks;

  SDL_Event e;
  while (now < destiny) {

    SDL_PumpEvents ();
    while (1 == SDL_PeepEvents(&e, 1, SDL_GETEVENT, mask)) { }

    now = SDL_GetTicks ();
  }

}

/* recursive; nmips can't reasonably be very large */
bool sdlutil::make_mipmaps(SDL_Surface ** s, int nmips) {
  if (nmips <= 1) return true;

  s[1] = shrink50(s[0]);
  if (!s[1]) return false;
  return make_mipmaps(&s[1], nmips - 1);
}

void sdlutil::clearsurface(SDL_Surface * s, Uint32 color) {
  SDL_FillRect(s, 0, color);
}

Uint32 sdlutil::mix2(Uint32 ia, Uint32 ib) {

  Uint32 ar = (ia >> rshift) & 0xFF;
  Uint32 br = (ib >> rshift) & 0xFF;

  Uint32 ag = (ia >> gshift) & 0xFF;
  Uint32 bg = (ib >> gshift) & 0xFF;

  Uint32 ab = (ia >> bshift) & 0xFF;
  Uint32 bb = (ib >> bshift) & 0xFF;

  Uint32 aa = (ia >> ashift) & 0xFF;
  Uint32 ba = (ib >> ashift) & 0xFF;

  /* if these are all fractions 0..1,
     color output is
       color1 * alpha1 + color2 * alpha2
       ---------------------------------
              alpha1 + alpha2               */

  Uint32 r = 0;
  Uint32 g = 0;
  Uint32 b = 0;

  if ((aa + ba) > 0) {
    /* really want 
        (ar / 255) * (aa / 255) +
        (br / 255) * (ba / 255)
	-------------------------
	(aa / 255) + (ba / 255)

	to get r as a fraction. we then
	multiply by 255 to get the output
	as a byte.
	
	but this is:

        ar * (aa / 255) +
        br * (ba / 255)
	-----------------------
	(aa / 255) + (ba / 255)

	which can be simplified to
	
	ar * aa + br * ba
	-----------------
	     aa + ba

	.. and doing the division last keeps
	us from quantization errors.
    */
    r = (ar * aa + br * ba) / (aa + ba);
    g = (ag * aa + bg * ba) / (aa + ba);
    b = (ab * aa + bb * ba) / (aa + ba);
  }

  /* alpha output is just the average */
  Uint32 a = (aa + ba) >> 1;

  return (r << rshift) | (g << gshift) | (b << bshift) | (a << ashift);
}

Uint32 sdlutil::mix4(Uint32 a, Uint32 b, Uint32 c, Uint32 d) {
  /* XXX PERF 

     would probably get better results (less quant error) and
     better performance to write this directly, using the calculation
     in mix2. */
  return mix2(mix2(a, b), mix2(c, d));
}

Uint32 sdlutil::mixfrac(Uint32 a, Uint32 b, float f) {

  Uint32 factor  = (Uint32) (f * 0x100);
  Uint32 ofactor = 0x100 - factor;

  Uint32 o24 = (((a >> 24) & 0xFF) * factor +
		((b >> 24) & 0xFF) * ofactor) >> (8);

  Uint32 o16 = (((a >> 16) & 0xFF) * factor +
		((b >> 16) & 0xFF) * ofactor) >> (8);

  Uint32 o8 = (((a >> 8) & 0xFF) * factor +
		((b >> 8) & 0xFF) * ofactor) >> (8);


  Uint32 o = ((a & 0xFF) * factor +
	      (b & 0xFF) * ofactor) >> (8);

  return (o24 << 24) | (o16 << 16) | (o8 << 8) | o;

}


Uint32 sdlutil::hsv(SDL_Surface * sur, float h, float s, float v, float a) {
  /* XXX equality for floating point? this can't be right. */
  int r, g, b;
  if (s == 0.0f) {
    r = (int)(v * 255);
    g = (int)(v * 255);
    b = (int)(v * 255);
  } else {
    float hue = h * 6.0f;
    int fh = (int)hue;
    float var_1 = v * (1 - s);
    float var_2 = v * (1 - s * (hue - fh));
    float var_3 = v * (1 - s * (1 - (hue - fh)));

    float red, green, blue;

    switch((int)hue) {
    case 0:  red = v     ; green = var_3 ; blue = var_1; break;
    case 1:  red = var_2 ; green = v     ; blue = var_1; break;
    case 2:  red = var_1 ; green = v     ; blue = var_3; break;
    case 3:  red = var_1 ; green = var_2 ; blue = v    ; break;
    case 4:  red = var_3 ; green = var_1 ; blue = v    ; break;
    default: red = v     ; green = var_1 ; blue = var_2; break;
    }

    r = (int)(red * 255);
    g = (int)(green * 255);
    b = (int)(blue * 255);
  }

  return SDL_MapRGBA(sur->format, r, g, b, (int)(a * 255));

}

SDL_Surface * sdlutil::alphadim(SDL_Surface * src) {
  /* must be 32 bpp */
  if (src->format->BytesPerPixel != 4) return 0;

  int ww = src->w, hh = src->h;
  
  SDL_Surface * ret = makesurface(ww, hh);

  if (!ret) return 0;

  slock(ret);
  slock(src);

  for(int y = 0; y < hh; y ++)
    for(int x = 0; x < ww; x ++) {
      Uint32 color = *((Uint32 *)src->pixels + y*src->pitch/4 + x);
      
      /* divide alpha channel by 2 */
      /* XXX this seems to be wrong on powerpc (dims some other channel
	 instead). but if the masks are right, how could this go wrong?*/
      color =
	(color & (sdlutil::rmask | sdlutil::gmask | sdlutil::bmask)) |
	(((color & sdlutil::amask) >> 1) & sdlutil::amask);
  
      *((Uint32 *)ret->pixels + y*ret->pitch/4 + x) = color;

    }

  sulock(src);
  sulock(ret);

  return ret;

}

SDL_Surface * sdlutil::shrink50(SDL_Surface * src) {
  
  /* must be 32 bpp */
  if (src->format->BytesPerPixel != 4) return 0;

  int ww = src->w, hh = src->h;
  
  /* we need there to be at least two pixels for each destination
     pixel, so if the src dimensions are not even, then we ignore
     that last pixel. */
  if (ww & 1) ww -= 1;
  if (hh & 1) hh -= 1;

  if (ww <= 0) return 0;
  if (hh <= 0) return 0;

  SDL_Surface * ret = makesurface(ww/2, hh/2);

  if (!ret) return 0;

  slock(ret);
  slock(src);

  for(int y = 0; y < ret->h; y ++)
    for(int x = 0; x < ret->w; x ++) {
      /* get and mix four src pixels for each dst pixel */
      Uint32 c1 = *((Uint32 *)src->pixels + (y*2)*src->pitch/4 + (x*2));
      Uint32 c2 = *((Uint32 *)src->pixels + (1+y*2)*src->pitch/4 + (1+x*2));
      Uint32 c3 = *((Uint32 *)src->pixels + (1+y*2)*src->pitch/4 + (x*2));
      Uint32 c4 = *((Uint32 *)src->pixels + (y*2)*src->pitch/4 + (1+x*2));

      Uint32 color = mix4(c1, c2, c3, c4);
  
      *((Uint32 *)ret->pixels + y*ret->pitch/4 + x) = color;

    }

  sulock(src);
  sulock(ret);

  return ret;
}

/* try to make a hardware surface, and, failing that,
   make a software surface */
SDL_Surface * sdlutil::makesurface(int w, int h, bool alpha) {

  /* PERF need to investigate relative performance 
     of sw/hw surfaces */
  SDL_Surface * ss = 0;
#if 0
    SDL_CreateRGBSurface(SDL_HWSURFACE |
			 (alpha?SDL_SRCALPHA:0),
			 w, h, 32, 
			 rmask, gmask, bmask,
			 amask);
#endif

  if (!ss) ss = SDL_CreateRGBSurface(SDL_SWSURFACE |
				     (alpha?SDL_SRCALPHA:0),
				     w, h, 32,
				     rmask, gmask, bmask,
				     amask);

  if (ss && !alpha) SDL_SetAlpha(ss, 0, 255);

  /* then convert to the display format. */
# if USE_DISPLAY_FORMAT
  if (ss) {
    SDL_Surface * rr;
    if (alpha) rr = SDL_DisplayFormatAlpha(ss);
    else rr = SDL_DisplayFormat(ss);
    SDL_FreeSurface(ss);
    return rr;
  } else return 0;
# else
  return ss;
# endif
}

SDL_Surface * sdlutil::makealpharect(int w, int h, int r, int g, int b, int a) {

  SDL_Surface * ret = makesurface(w, h);

  if (!ret) return 0;

  Uint32 color = SDL_MapRGBA(ret->format, r, g, b, a);

  SDL_FillRect(ret, 0, color);
  
  return ret;
}

SDL_Surface * sdlutil::makealpharectgrad(int w, int h,
					 int r1, int b1, int g1, int a1,
					 int r2, int b2, int g2, int a2,
					 float bias) {

  SDL_Surface * ret = makesurface(w, h);

  if (!ret) return 0;

  /* draws each line as a separate rectangle. */
  for(int i = 0; i < h; i ++) {
    /* no bias yet */
    float frac = 1.0f - ((float)i / (float)h);
    int r = (int) ((r1 * frac) + (r2 * (1.0 - frac)));
    int g = (int) ((g1 * frac) + (g2 * (1.0 - frac)));
    int b = (int) ((b1 * frac) + (b2 * (1.0 - frac)));
    int a = (int) ((a1 * frac) + (a2 * (1.0 - frac)));
    Uint32 color = SDL_MapRGBA(ret->format, r, g, b, a);
    SDL_Rect rect;
    rect.x = 0;
    rect.y = i;
    rect.w = w;
    rect.h = 1;
    SDL_FillRect(ret, &rect, color);
  }    

  return ret;
}

void sdlutil::fillrect(SDL_Surface * s, Uint32 color, int x, int y, int w, int h) {
  SDL_Rect dst;
  dst.x = x;
  dst.y = y;
  dst.w = w;
  dst.h = h;
  SDL_FillRect(s, &dst, color);
}

void sdlutil::blitall(SDL_Surface * src, SDL_Surface * dst, int x, int y) {
  SDL_Rect rec;
  rec.x = x;
  rec.y = y;
  SDL_BlitSurface(src, 0, dst, &rec);
}

void sdlutil::outline(SDL_Surface * s, int n, int r, int g, int b, int a) {

  Uint32 color = SDL_MapRGBA(s->format, r, g, b, a);

  SDL_Rect dst;
  dst.x = 0;
  dst.y = 0;
  dst.h = n;
  dst.w = s->w;
  SDL_FillRect(s, &dst, color);
  dst.w = n;
  dst.h = s->h;
  SDL_FillRect(s, &dst, color);
  dst.x = s->w - n;
  SDL_FillRect(s, &dst, color);
  dst.y = s->h - n;
  dst.x = 0;
  dst.w = s->w;
  dst.h = n;
  SDL_FillRect(s, &dst, color);

}

SDL_Surface * sdlutil::makescreen(int w, int h) {

  /* Can't use HWSURFACE here, because not handling this SDL_BlitSurface
     case mentioned in the documentation:

     "If either of the surfaces were in video memory, and the blit returns -2, 
     the video memory was lost, so it should be reloaded with artwork 
     and re-blitted." 

     Plus, on Windows, the only time you get HWSURFACE is with FULLSCREEN.

     -- Adam
  */

  /* SDL_ANYFORMAT	
     "Normally, if a video surface of the requested bits-per-pixel (bpp) 
     is not available, SDL will emulate one with a shadow surface. 
     Passing SDL_ANYFORMAT prevents this and causes SDL to use the 
     video surface, regardless of its pixel depth."

     Probably should not pass this.

     -- Adam
  */


  /* SDL_DOUBLEBUF only valid with SDL_HWSURFACE! */
  SDL_Surface * ret = SDL_SetVideoMode(w, h, 32,
				       SDL_SWSURFACE |
				       SDL_RESIZABLE);
  return ret;
}

/* XXX apparently you are not supposed
   to lock the surface for blits, only
   direct pixel access. should check
   this out if mysterious crashes on some
   systems ... */

void sdlutil::slock(SDL_Surface *screen) {
  if(SDL_MUSTLOCK(screen))
    SDL_LockSurface(screen);
}

void sdlutil::sulock(SDL_Surface *screen) {
  if (SDL_MUSTLOCK(screen))
    SDL_UnlockSurface(screen);
}


SDL_Surface * sdlutil::imgload(const char *file, bool alpha) {
  SDL_Surface * img = IMG_Load(file);
  if (!img) {
    return NULL;
  }

# if USE_DISPLAY_FORMAT
  SDL_Surface * dispimg;
  if (alpha) dispimg = SDL_DisplayFormatAlpha(img);
  else dispimg = SDL_DisplayFormat(img);
  SDL_FreeSurface(img);
  return dispimg;
# else
  return img;
# endif
}


/* lock before calling */
/* getpixel function only:
   Copyright (c) 2004 Bill Kendrick, Tom Murphy
   from the GPL "Tux Paint" */
Uint32 sdlutil::getpixel(SDL_Surface * surface, int x, int y) {
  int bpp;
  Uint8 * p;
  Uint32 pixel;

  pixel = 0;

  /* Check that x/y values are within the bounds of this surface... */

  if (x >= 0 && y >= 0 && x < surface -> w && y < surface -> h) {

    /* Determine bytes-per-pixel for the surface in question: */

    bpp = surface->format->BytesPerPixel;

    /* Set a pointer to the exact location in memory of the pixel
       in question: */

    p = (Uint8 *) (((Uint8 *)surface->pixels) +  /* Start at top of RAM */
		   (y * surface->pitch) +  /* Go down Y lines */
		   (x * bpp));             /* Go in X pixels */

    /* Return the correctly-sized piece of data containing the
       pixel's value (an 8-bit palette value, or a 16-, 24- or 32-bit
       RGB value) */

    if (bpp == 1)         /* 8-bit display */
      pixel = *p;
    else if (bpp == 2)    /* 16-bit display */
      pixel = *(Uint16 *)p;
    else if (bpp == 3) {    /* 24-bit display */
      /* Depending on the byte-order, it could be stored RGB or BGR! */

      if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
	pixel = p[0] << 16 | p[1] << 8 | p[2];
      else
	pixel = p[0] | p[1] << 8 | p[2] << 16;
    } else if (bpp == 4) {    /* 32-bit display */
      pixel = *(Uint32 *)p;
    }
  }
  return pixel;
}

/* based on getpixel above */
void sdlutil::setpixel(SDL_Surface * surface, int x, int y,
		       Uint32 px) {
  int bpp;
  Uint8 * p;

  /* Check that x/y values are within the bounds of this surface... */

  if (x >= 0 && y >= 0 && x < surface -> w && y < surface -> h) {

    /* Determine bytes-per-pixel for the surface in question: */

    bpp = surface->format->BytesPerPixel;

    /* Set a pointer to the exact location in memory of the pixel
       in question: */

    p = (Uint8 *) (((Uint8 *)surface->pixels) +  /* Start at top of RAM */
		   (y * surface->pitch) +  /* Go down Y lines */
		   (x * bpp));             /* Go in X pixels */

    /* Return the correctly-sized piece of data containing the
       pixel's value (an 8-bit palette value, or a 16-, 24- or 32-bit
       RGB value) */

    if (bpp == 1)         /* 8-bit display */
      *p = px;
    else if (bpp == 2)    /* 16-bit display */
      *(Uint16 *)p = px;
    else if (bpp == 3) {    /* 24-bit display */
      /* Depending on the byte-order, it could be stored RGB or BGR! */

      /* XX never tested */
      if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
	p += 2;
	for (int i = 0; i < 3; i ++) {
	  *p = px & 255;
	  px >>= 8;
	  p--;
	}
      }
      else {
	for (int i = 0; i < 3; i ++) {
	  *p = px & 255;
	  px >>= 8;
	  p++;
	}
      }
    } else if (bpp == 4) {    /* 32-bit display */
      *(Uint32 *)p = px;
    }
  }
}

void sdlutil::drawline(SDL_Surface * screen, int x0, int y0,
		       int x1, int y1, 
		       Uint8 R, Uint8 G, Uint8 B) {
  /* PERF could maprgb once */

  line * l = line::create(x0, y0, x1, y1);
  if (!l) return;

  /* direct pixel access */
  slock(screen);
  int x, y;
  drawpixel(screen, x0, y0, R, G, B);
  while (l->next(x, y)) {
    drawpixel(screen, x, y, R, G, B);
  }
  sulock(screen);
  l->destroy();
}


/* XXX change to use function pointer? */
/* lock before calling */
void sdlutil::drawpixel(SDL_Surface *screen, int x, int y,
			Uint8 R, Uint8 G, Uint8 B) {
  Uint32 color = SDL_MapRGB(screen->format, R, G, B);
  switch (screen->format->BytesPerPixel) {
    case 1: // Assuming 8-bpp
      {
        Uint8 *bufp;
        bufp = (Uint8 *)screen->pixels + y*screen->pitch + x;
        *bufp = color;
      }
      break;
    case 2: // Probably 15-bpp or 16-bpp
      {
        Uint16 *bufp;
        bufp = (Uint16 *)screen->pixels + y*screen->pitch/2 + x;
        *bufp = color;
      }
      break;
    case 3: // Slow 24-bpp mode, usually not used
      {
        Uint8 *bufp;
        bufp = (Uint8 *)screen->pixels + y*screen->pitch + x * 3;
        if(SDL_BYTEORDER == SDL_LIL_ENDIAN) {
	    bufp[0] = color;
	    bufp[1] = color >> 8;
	    bufp[2] = color >> 16;
	  } else {
	    bufp[2] = color;
	    bufp[1] = color >> 8;
	    bufp[0] = color >> 16;
	  }
      }
      break;
    case 4: // Probably 32-bpp
      {
        Uint32 *bufp;
        bufp = (Uint32 *)screen->pixels + y*screen->pitch/4 + x;
        *bufp = color;
      }
      break;
    }
}


void sdlutil::printsurfaceinfo(SDL_Surface * surf) {
  int f = surf->flags;

#define INFO(flag, str) \
  printf(" %s " str "\n", (f & (flag))?"X":" ");

  printf("==== info for surface at %p ====\n", surf);
  INFO(SDL_HWSURFACE,   "In Video Memory");
  INFO(SDL_ASYNCBLIT,   "Asynch blits if possible");
  INFO(SDL_ANYFORMAT,   "Any pixel format");
  INFO(SDL_HWPALETTE,   "Exclusive palette");
  INFO(SDL_DOUBLEBUF,   "Double buffered");
  INFO(SDL_FULLSCREEN,  "Full screen");
  INFO(SDL_OPENGL,      "OpenGL context");
  INFO(SDL_OPENGLBLIT,  "Supports OpenGL blitting");
  INFO(SDL_RESIZABLE,   "Can resize");
  INFO(SDL_HWACCEL,     "Blit is hardware accelerated");
  INFO(SDL_SRCCOLORKEY, "Colorkey blitting");
  INFO(SDL_RLEACCEL,    "RLE accelerated blitting");
  INFO(SDL_SRCALPHA,    "Blit uses source alpha blending");
  INFO(SDL_PREALLOC,    "Uses preallocated memory");

#undef INFO
}

SDL_Surface * sdlutil::fliphoriz(SDL_Surface * src) {
  SDL_Surface * dst = makesurface(src->w, src->h);
  if (!dst) return 0;

  slock(src);
  slock(dst);
  
  for(int y = 0; y < src->h; y ++) {
    for(int x = 0; x < src->w; x ++) {
      Uint32 px = getpixel(src, x, y);
      setpixel(dst, (src->w - x) - 1, y, px);
    }
  }

  slock(dst);
  sulock(src);
  return dst;
}
