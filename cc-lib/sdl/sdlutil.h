
#ifndef __SDLUTIL_H
#define __SDLUTIL_H

#include "SDL.h"

struct sdlutil {
  static SDL_Surface * makescreen(int w, int h);
  static void slock(SDL_Surface *surf);
  static void sulock(SDL_Surface *surf);

  /* eat and ignore all the events in the queue for a few ticks */
  static void eatevents(int ticks, Uint32 mask = 0xFFFFFFFF);

  /* for direct pixel access: lock first, unlock after */
  static void drawpixel(SDL_Surface *surf, int x, int y,
                        Uint8 R, Uint8 G, Uint8 B);
  static Uint32 getpixel(SDL_Surface *, int x, int y);
  static void   setpixel(SDL_Surface *, int x, int y,
                         Uint32 color);

  static SDL_Surface * duplicate(SDL_Surface * surf);

  static void drawline(SDL_Surface *, int x1, int y1, int x2, int y2,
                       Uint8 R, Uint8 G, Uint8 B);

  static void clearsurface(SDL_Surface *, Uint32);

  /* make a n pixel border around a surface. */
  static void outline(SDL_Surface *, int n, int r, int g, int b, int a);

  /* Blit the entire source surface to the destination at the given
     position. */
  static void blitall(SDL_Surface * src, SDL_Surface * dst, int x, int y);

  /* Just like SDL_Fillrect, but no need to crate SDL_Rects */
  static void fillrect(SDL_Surface *, Uint32 color,
                       int x, int y, int w, int h);

  /* create a rectangle of the specified size, filled with a color
     at a certain alpha value. This can then be blit to the screen. */
  static SDL_Surface * makealpharect(int w, int h, int r,
                                     int g, int b, int a);

  /* make an alpha rectangle with a vertical gradient between the two
     specified colors.  bias should be between 0 and 1; a higher bias
     means the gradient will have more of the bottom color.
  */
  static SDL_Surface * makealpharectgrad(int w, int h,
                                         int r1, int b1, int g1, int a1,
                                         int r2, int b2, int g2, int a2,
                                         float bias = 0.5);

  static SDL_Surface * makesurface(int w, int h, bool alpha = true);

  /* make a new surface with the same contents as the old, or 'color' where
     undefined */
  static SDL_Surface * resize_canvas(SDL_Surface * s, int w, int h, Uint32 color);

  /* print out flags and maybe other things */
  static void printsurfaceinfo(SDL_Surface * surf);

  /* shrink a 32bpp surface to 50% of its original size,
     returning the new surface. */
  static SDL_Surface * shrink50(SDL_Surface * src);

  /* create a mipmap array (of nmips successively half-sized images)
     in the array surfs. The first entry of surfs should be filled
     with a surface, so for nmips==1 this does nothing. */
  static bool make_mipmaps(SDL_Surface ** surfs, int nmips);

  /* divide alpha channel by 2 */
  static SDL_Surface * alphadim(SDL_Surface * src);

  /* flip a surface horizontally */
  static SDL_Surface * fliphoriz(SDL_Surface * src);

  /* load an image into a fast surface */
  static SDL_Surface * imgload(const char * file, bool alpha = true);

  /* mix two 32bit colors, doing the right thing for alpha */
  static Uint32 mix2(Uint32, Uint32);

  /* mix four 32bit colors, byte-order agnostic */
  static Uint32 mix4(Uint32, Uint32, Uint32, Uint32);

  /* frac should be between 0 and 1, and we get frac of the first
     color and 1-frac of the second */
  static Uint32 mixfrac(Uint32, Uint32, float frac);

  static const Uint32 rmask, gmask, bmask, amask;

  /* convert from hue-saturation-value-alpha space to RGBA (compatible
     with the supplied surface) */
  static Uint32 hsv(SDL_Surface * surf, float h, float s, float v, float a);

};

#endif
