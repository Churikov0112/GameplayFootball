// written by bastiaan konings schuiling 2008 - 2014
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#include "surface.hpp"

#include "base/log.hpp"

namespace blunted {

  Surface::Surface() : surface(0) {
    //printf("CREATING SURFACE\n");
  }

  Surface::~Surface() {
    //printf("ANNIHILATING SURFACE.. ");
    if (surface) {
      SDL_DestroySurface(surface);
      surface = 0;
    }
  }

  Surface::Surface(const Surface &src) {
    this->surface = SDL_ConvertSurfaceAndColorspace(src.surface, src.surface->format, NULL, SDL_COLORSPACE_SRGB, 0);
    assert(this->surface);
  }

  SDL_Surface *Surface::GetData() {
    return surface;
  }

  void Surface::SetData(SDL_Surface *surface) {
    if (this->surface) SDL_DestroySurface(this->surface);
    this->surface = surface;
  }

  void Surface::Resize(int x, int y) {

    assert(this->surface);
    int xcur = this->surface->w;
    int ycur = this->surface->h;
    double xfac, yfac;
    xfac = x / (xcur * 1.0);
    yfac = y / (ycur * 1.0);
    if (yfac == 0) yfac = xfac;
    if (xfac == 0) xfac = yfac;
    if (xfac == 0 || yfac == 0) return;
    SDL_Surface *newSurf = SDL_ScaleSurface(this->surface, int(xcur * xfac), int(ycur * yfac), SDL_SCALEMODE_LINEAR);
    SDL_DestroySurface(this->surface);
    this->surface = newSurf;
  }

  void Surface::GetSize(int &x, int &y) {
    x = this->surface->w;
    y = this->surface->h;
  }

  void Surface::SetAlpha(float alpha) {
    sdl_setsurfacealpha(this->surface, int(round(alpha * 255)));
  }

}
