#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <png.h>

typedef struct {
    int width;
    int height;
    unsigned char* data;
} Image;

Image* load_png(const char* filename) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        perror("无法打开PNG文件");
        return NULL;
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        fclose(fp);
        return NULL;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        fclose(fp);
        return NULL;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return NULL;
    }

    png_init_io(png_ptr, fp);
    png_read_info(png_ptr, info_ptr);

    int bit_depth, color_type;
    bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    color_type = png_get_color_type(png_ptr, info_ptr);

    if (bit_depth == 16)
        png_set_strip_16(png_ptr);

    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);

    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png_ptr);

    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png_ptr);

    if (color_type == PNG_COLOR_TYPE_RGB ||
        color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);

    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png_ptr);

    png_read_update_info(png_ptr, info_ptr);

    Image* image = malloc(sizeof(Image));
    image->width = png_get_image_width(png_ptr, info_ptr);
    image->height = png_get_image_height(png_ptr, info_ptr);
    image->data = malloc(image->width * image->height * 4); // ARGB format

    png_bytepp row_pointers = png_malloc(png_ptr, sizeof(png_bytep) * image->height);
    for (int y = 0; y < image->height; y++)
        row_pointers[y] = image->data + y * image->width * 4;

    png_read_image(png_ptr, row_pointers);

    png_free(png_ptr, row_pointers);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    fclose(fp);

    return image;
}

Image* load_image(const char* filename) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        perror("无法打开IMAGE文件");
        return NULL;
    }
    Image* image = malloc(sizeof(Image));
    fread(&image->width, 4, 1, fp);
    fread(&image->height, 4, 1, fp);
    image->data = malloc(image->width * image->height * 3);
    fread(image->data, 1, image->width * image->height * 3, fp);
    fclose(fp);
    return image;
}

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

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("用法: %s <IMAGE文件> <PNG文件>\n", argv[0]);
        return 1;
    }
    Image* image = load_image(argv[1]);
    if (!image) {
        return 1;
    }
    unsigned char* data = (unsigned char*)malloc(image->width * image->height * 4);
    for (int i = 0; i < image->width * image->height; i++) {
        data[i * 4 + 0] = image->data[i * 3 + 0];
        data[i * 4 + 1] = image->data[i * 3 + 1];
        data[i * 4 + 2] = image->data[i * 3 + 2];
        data[i * 4 + 3] = 0xFF;
    }  
    save_png(argv[2], image->width, image->height, data);
    free(data);

    return 0;
}