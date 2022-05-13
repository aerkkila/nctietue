#include <png.h>
#include <errno.h>
#include "nctietue.h"

nct_vset* nct_open_png_gd(nct_vset* dest, char* name) {
  FILE* filein = fopen(name, "r");
  if(!filein) {
    perror("nct_open_png_gd");
    return NULL;
  }
  png_structp png_p = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if(!png_p) {
    fclose(filein);
    return NULL;
  }
  png_infop info_p = png_create_info_struct(png_p);
  if(!info_p) {
    fclose(filein);
    png_destroy_read_struct(&png_p, (png_infopp)NULL, (png_infopp)NULL);
    return NULL;
  }

  png_init_io(png_ptr, filein);
  png_read_info(png_p, info_p);

  int width, height, bit_depth, color_type, interlace_type;
  png_get_IHDR(png_p, info_p, &width, &height, &bit_depth, &color_type, &interlace_type, NULL, NULL);
  if(interlace_type != PNG_INTERLACE_NONE) {
    fprintf(stderr, "png_interlace_type must be PNG_INTERLACE_NONE\n");
    exit_this();
  }
  if(color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_RGB_ALPHA)
    png_set_rgb_to_gray_fixed(png_p, 1, -1, -1);
  png_read_update_info(png_p, info_p);

  int pixelsize = 1 + (bit_depth==16);
  int rowlen = pixelsize*width;
  png_bytep data = malloc(pixelsize*width*height);
  for(int i=0; i<height; i++)
    png_read_row(png_p, data+i*rowlen, NULL);
  
  fclose(filein);
  png_destroy_read_struct(&png_p, &info_p, NULL);

  /*Png things end. Nct things begin.*/

  if(width < 1<<16)
    nct_add_coord(dest, to_nct_coord(nct_range_NC_USHORT(0,width,1)), width, NC_USHORT, "x");
  else if(width < 1<<32)
    nct_add_coord(dest, to_nct_coord(nct_range_NC_UINT(0,width,1)), width, NC_UINT, "x");
  else
    nct_add_coord(dest, to_nct_coord(nct_range_NC_UINT64(0,width,1)), width, NC_UINT64, "x");

  if(height < 1<<16)
    nct_add_coord(dest, to_nct_coord(nct_range_NC_USHORT(0,height,1)), height, NC_USHORT, "y");
  else if(height < 1<<32)
    nct_add_coord(dest, to_nct_coord(nct_range_NC_UINT(0,height,1)), height, NC_UINT, "y");
  else
    nct_add_coord(dest, to_nct_coord(nct_range_NC_UINT64(0,height,1)), height, NC_UINT64, "y");

  print(dest);
  return dest;
}