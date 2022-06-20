#include <SDL2/SDL.h>
#include <netcdf.h>
#include <sys/time.h>
#include <stdint.h>
#include "nctietue.h"

#define MIN(a,b) (a)<=(b)? (a): (b)
#define GET_SCALE(xlen,win_w,ylen,win_h) MIN((float)(ylen)/(win_h), (float)(xlen)/(win_w))
#define GET_XSCALE_1D(xlen, win_w) (float)(xlen)/(win_w)
#define ARRSIZE(a) sizeof(a)/sizeof(*(a))

static SDL_Renderer* rend;
static SDL_Window* window;
static SDL_Texture* base;
static int win_w, win_h, xid, yid;
static int invert_y, stop, echo_on;
static double scale;
static double minshift;
static double maxshift;
static unsigned char color1d_fg[3] = {0, 0, 0};
static unsigned char color1d_bg[3] = {255, 255, 255};
static void (*redraw)(nct_var*);

SDL_Event event;

typedef union {
  void* v;
  float f;
} Arg;

typedef struct {
  int key;
  int mod;
  void (*fun)(Arg);
  Arg arg;
} Binding;

#define ONE_TYPE(nctype, form, ctype)					\
  static void draw2d_##nctype(nct_var* var)				\
  {									\
    int value, xlen = var->dimlens[xid];				\
    double di=0, dj=0;							\
    ctype minmax[2];							\
    nct_minmax_##nctype(var, minmax);					\
    ctype range = minmax[1]-minmax[0];					\
    minmax[0] += range*minshift;					\
    minmax[1] += range*maxshift;					\
    if(echo_on)								\
      printf("min %" #form ", max %" #form "\n"				\
	     "minshift %.4lf, maxshift %.4lf\n",			\
	     minmax[0], minmax[1], minshift, maxshift);			\
    if(invert_y) {							\
      for(int j=win_h-1; j>=0; j--, dj+=scale) {			\
	for(int i=0; i<win_w; i++, di+=scale) {				\
	  ctype val = ((ctype*)var->data)[(int)dj*xlen + (int)di];	\
	  if(val<=minmax[0])						\
	    value = 0;							\
	  else if (val>=minmax[1])					\
	    value = 255;						\
	  else								\
	    value = (val - minmax[0]) * 255 / (minmax[1]-minmax[0]);	\
	  SDL_SetRenderDrawColor(rend, value, value, value, 255);	\
	  SDL_RenderDrawPoint(rend, i, j);				\
	}								\
	di = 0;								\
      }									\
    } else {								\
      for(int j=0; j<win_h; j++, dj+=scale) {				\
	for(int i=0; i<win_w; i++, di+=scale) {				\
	  int value = (((ctype*)var->data)[(int)dj*xlen + (int)di] - minmax[0]) * 255 / (minmax[1]-minmax[0]); \
	  SDL_SetRenderDrawColor(rend, value, value, value, 255);	\
	  SDL_RenderDrawPoint(rend, i, j);				\
	}								\
	di = 0;								\
      }									\
    }									\
  }
ALL_TYPES_EXCEPT_STRING
#undef ONE_TYPE

#define ONE_TYPE(nctype, b, ctype)					\
  static void draw1d_##nctype(nct_var* var)				\
  {									\
    SDL_SetRenderDrawColor(rend, color1d_bg[0], color1d_bg[1], color1d_bg[2], 255); \
    SDL_RenderClear(rend);						\
    double di=0;							\
    ctype minmax[2];							\
    nct_minmax_##nctype(var, minmax);					\
    SDL_SetRenderDrawColor(rend, color1d_fg[0], color1d_fg[1], color1d_fg[2], 255); \
    for(int i=0; i<win_w; i++, di+=scale) {				\
      int y = (((ctype*)var->data)[(int)di] - minmax[0]) * win_h / (minmax[1]-minmax[0]); \
      SDL_RenderDrawPoint(rend, i, y);					\
    }									\
  }
ALL_TYPES_EXCEPT_STRING
#undef ONE_TYPE

#define ONE_TYPE(nctype,b,ctype) [nctype]=draw2d_##nctype,
static void (*drawfunctions_2d[])(nct_var*) =
  {
  ALL_TYPES_EXCEPT_STRING
  };
#undef ONE_TYPE

#define ONE_TYPE(nctype,b,ctype) [nctype]=draw1d_##nctype,
static void (*drawfunctions_1d[])(nct_var*) =
  {
  ALL_TYPES_EXCEPT_STRING
  };
#undef ONE_TYPE

static void redraw_2d(nct_var* var) {
  SDL_SetRenderTarget(rend, base);
  drawfunctions_2d[var->xtype](var);
  SDL_SetRenderTarget(rend, NULL);
}

static void redraw_1d(nct_var* var) {
  SDL_SetRenderTarget(rend, base);
  drawfunctions_1d[var->xtype](var);
  SDL_SetRenderTarget(rend, NULL);
}

static uint_fast64_t time_now_ms() {
  struct timeval t;
  gettimeofday(&t, NULL);
  return t.tv_sec*1000 + t.tv_usec/1000;
}

static void quit(Arg unused) {
  stop = 1;
  SDL_DestroyTexture(base);
  SDL_DestroyRenderer(rend);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

static nct_var* var;

static void resized() {
  static uint_fast64_t lasttime;
  uint_fast64_t thistime = time_now_ms();
  if(thistime-lasttime < 20)
    return;
  lasttime = thistime;
  SDL_DestroyTexture(base);
  base = SDL_CreateTexture(rend, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, win_w, win_h);
  SDL_GetWindowSize(window, &win_w, &win_h);
  scale = GET_SCALE(var->dimlens[xid], win_w, var->dimlens[yid], win_h);
  redraw(var);
}

void invert_y_func(Arg unused) {
  invert_y = !invert_y;
  redraw(var);
}

void shift_min(Arg shift) {
  minshift += shift.f;
  redraw(var);
}

void shift_max(Arg shift) {
  maxshift += shift.f;
  redraw(var);
}

void toggle_echo(Arg _) {
  echo_on = !echo_on;
}

Binding keydown_bindings[] = {
  { SDLK_i, KMOD_SHIFT, invert_y_func, {0}        },
  { SDLK_q, 0,          quit,          {0}        },
  { SDLK_1, 0,          shift_min,     {.f=-0.02} },
  { SDLK_1, KMOD_SHIFT, shift_max,     {.f=-0.02} },
  { SDLK_2, 0,          shift_min,     {.f=0.02}  },
  { SDLK_2, KMOD_SHIFT, shift_max,     {.f=0.02}  },
  { SDLK_e, 0,          toggle_echo,   {0}        },
};

int get_modstate() {
  /*makes modstate side-insensitive*/
  int mod = 0;
  int mod0 = SDL_GetModState();
  if(mod0 & KMOD_CTRL)
    mod |= KMOD_CTRL;
  if(mod0 & KMOD_SHIFT)
    mod |= KMOD_SHIFT;
  if(mod0 & KMOD_ALT)
    mod |= KMOD_ALT;
  if(mod0 & KMOD_GUI)
    mod |= KMOD_GUI;
  return mod;
}

void keydown_func() {
  int len = ARRSIZE(keydown_bindings);
#define A keydown_bindings[i]
  for(int i=0; i<len; i++)
    if(event.key.keysym.sym == A.key)
      if(get_modstate() == A.mod)
	A.fun(A.arg);
#undef A
}

static void mainloop() {
  while(1) {
    while(SDL_PollEvent(&event)) {
      if(event.type == SDL_QUIT)
	quit((Arg){0});
      else if(event.type==SDL_WINDOWEVENT && event.window.event==SDL_WINDOWEVENT_RESIZED)
	resized();
      else if(event.type==SDL_KEYDOWN)
	keydown_func();
      if(stop) return;
    }
    if(stop) return;
    SDL_RenderCopy(rend, base, NULL, NULL);
    SDL_RenderPresent(rend);
    SDL_Delay(12);
  }
}

static void _plot_var_1d(nct_var* var) {
  /*come from nct_plot_var*/
  int xlen = var->dimlens[xid];
  window = SDL_CreateWindow("Figure", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, MIN(xlen,win_w), MIN(800,win_h), SDL_WINDOW_RESIZABLE);
  rend = SDL_CreateRenderer(window, -1, SDL_RENDERER_TARGETTEXTURE);
  SDL_GetWindowSize(window, &win_w, &win_h);
  base = SDL_CreateTexture(rend, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, win_w, win_h);
  scale = GET_XSCALE_1D(xlen, win_w);

  redraw_1d(var);

  mainloop();
}

void nct_plot_var(nct_vset* vset, int varid) {
  if(SDL_Init(SDL_INIT_VIDEO)) {
    fprintf(stderr, "%sError in SDL_INIT:%s %s\n", nct_error_color, SDL_GetError(), nct_default_color);
    return;
  }
  SDL_Event event;
  while(SDL_PollEvent(&event));
  var = vset->vars+varid;
  SDL_DisplayMode dm;
  if(SDL_GetCurrentDisplayMode(0, &dm)) {
    fprintf(stderr, "%sError in getting monitor size%s: %s\n", nct_error_color, SDL_GetError(), nct_default_color);
    win_w = win_h = 500;
  } else {
    win_w = dm.w;
    win_h = dm.h;
  }
  xid = var->ndims-1;
  yid = var->ndims-2;
  if(yid < 0) {
    redraw = redraw_1d;
    return _plot_var_1d(var);
  }
  redraw = redraw_2d;
  int xlen = var->dimlens[xid];
  int ylen = var->dimlens[yid];
  
  window = SDL_CreateWindow("Figure", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, MIN(xlen,win_w), MIN(ylen,win_h), SDL_WINDOW_RESIZABLE);
  rend = SDL_CreateRenderer(window, -1, SDL_RENDERER_TARGETTEXTURE);
  SDL_GetWindowSize(window, &win_w, &win_h);
  base = SDL_CreateTexture(rend, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, win_w, win_h);
  scale = GET_SCALE(xlen, win_w, ylen, win_h);

  redraw(var);

  mainloop();
}
