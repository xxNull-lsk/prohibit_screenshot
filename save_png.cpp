#include <stdio.h>
#include <stdlib.h>
#include <png.h>
#include "save_png.h"
void save_png(const char *filename, int width, int height, unsigned char *data) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        fclose(fp);
        fprintf(stderr, "Could not allocate write struct\n");
        exit(EXIT_FAILURE);
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
        fclose(fp);
        fprintf(stderr, "Could not allocate info struct\n");
        exit(EXIT_FAILURE);
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        fprintf(stderr, "Error during init_io\n");
        exit(EXIT_FAILURE);
    }

    png_init_io(png_ptr, fp);

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        fprintf(stderr, "Error during writing header\n");
        exit(EXIT_FAILURE);
    }

    png_set_IHDR(png_ptr, info_ptr, width, height,
                 8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png_ptr, info_ptr);

    for (int y = 0; y < height; ++y) {
        png_bytep row_pointer = data + y * width * 4;
        if (setjmp(png_jmpbuf(png_ptr))) {
            png_destroy_write_struct(&png_ptr, &info_ptr);
            fclose(fp);
            fprintf(stderr, "Error during writing rows\n");
            exit(EXIT_FAILURE);
        }
        png_write_row(png_ptr, row_pointer);
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        fprintf(stderr, "Error during end of write\n");
        exit(EXIT_FAILURE);
    }

    png_write_end(png_ptr, NULL);

    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
}