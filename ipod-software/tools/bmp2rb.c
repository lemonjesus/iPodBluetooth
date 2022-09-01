/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2002 by Linus Nielsen Feltzing
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
/****************************************************************************
 *
 * Converts BMP files to Rockbox bitmap format
 *
 * 1999-05-03 Linus Nielsen Feltzing
 *
 * 2005-07-06 Jens Arnold
 *            added reading of 4, 16, 24 and 32 bit bmps
 *            added 2 new target formats (playergfx and iriver 4-grey)
 *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define debugf printf

#ifdef __GNUC__
#define STRUCT_PACKED __attribute__((packed))
#else
#define STRUCT_PACKED
#pragma pack (push, 2)
#endif

struct Fileheader
{
    unsigned short Type;          /* signature - 'BM' */
    unsigned int  Size;          /* file size in bytes */
    unsigned short Reserved1;     /* 0 */
    unsigned short Reserved2;     /* 0 */
    unsigned int  OffBits;       /* offset to bitmap */
    unsigned int  StructSize;    /* size of this struct (40) */
    unsigned int  Width;         /* bmap width in pixels */
    unsigned int  Height;        /* bmap height in pixels */
    unsigned short Planes;        /* num planes - always 1 */
    unsigned short BitCount;      /* bits per pixel */
    unsigned int  Compression;   /* compression flag */
    unsigned int  SizeImage;     /* image size in bytes */
    int           XPelsPerMeter; /* horz resolution */
    int           YPelsPerMeter; /* vert resolution */
    unsigned int  ClrUsed;       /* 0 -> color table size */
    unsigned int  ClrImportant;  /* important color count */
} STRUCT_PACKED;

struct RGBQUAD
{
    unsigned char rgbBlue;
    unsigned char rgbGreen;
    unsigned char rgbRed;
    unsigned char rgbReserved;
} STRUCT_PACKED;

union RAWDATA {
    void *d; /* unspecified */
    unsigned short *d16; /* depth <= 16 */
    struct { unsigned char b, g, r; } *d24; /* depth = 24 BGR */
};

short readshort(void* value)
{
    unsigned char* bytes = (unsigned char*) value;
    return bytes[0] | (bytes[1] << 8);
}

int readint(void* value)
{
    unsigned char* bytes = (unsigned char*) value;
    return bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24);
}

unsigned char brightness(struct RGBQUAD color)
{
    return (3 * (unsigned int)color.rgbRed + 6 * (unsigned int)color.rgbGreen
              + (unsigned int)color.rgbBlue) / 10;
}

#ifndef O_BINARY
#define O_BINARY 0 /* systems that don't have O_BINARY won't make a difference
                      on text and binary files */
#endif

/****************************************************************************
 * read_bmp_file()
 *
 * Reads an uncompressed BMP file and puts the data in a 4-byte-per-pixel
 * (RGBQUAD) array. Returns 0 on success.
 *
 ***************************************************************************/

int read_bmp_file(char* filename,
                  int *get_width,  /* in pixels */
                  int *get_height, /* in pixels */
                  struct RGBQUAD **bitmap)
{
    struct Fileheader fh;
    struct RGBQUAD palette[256];

    int fd = open(filename, O_RDONLY| O_BINARY);
    unsigned short data;
    unsigned char *bmp;
    int width, height;
    int padded_width;
    int size;
    int row, col, i;
    int numcolors, compression;
    int depth;

    if (fd == -1)
    {
        debugf("error - can't open '%s'\n", filename);
        return 1;
    }
    if (read(fd, &fh, sizeof(struct Fileheader)) !=
        sizeof(struct Fileheader))
    {
        debugf("error - can't Read Fileheader Stucture\n");
        close(fd);
        return 2;
    }

    compression = readint(&fh.Compression);

    if (compression != 0)
    {
        debugf("error - Unsupported compression %d\n", compression);
        close(fd);
        return 3;
    }

    depth = readshort(&fh.BitCount);

    if (depth <= 8)
    {
        numcolors = readint(&fh.ClrUsed);
        if (numcolors == 0)
            numcolors = 1 << depth;

        if (read(fd, &palette[0], numcolors * sizeof(struct RGBQUAD))
            != (int)(numcolors * sizeof(struct RGBQUAD)))
        {
            debugf("error - Can't read bitmap's color palette\n");
            close(fd);
            return 4;
        }
    }

    width = readint(&fh.Width);
    height = readint(&fh.Height);
    padded_width = ((width * depth + 31) / 8) & ~3; /* aligned 4-bytes boundaries */

    size = padded_width * height; /* read this many bytes */
    bmp = (unsigned char *)malloc(size);
    *bitmap = (struct RGBQUAD *)malloc(width * height * sizeof(struct RGBQUAD));

    if ((bmp == NULL) || (*bitmap == NULL))
    {
        debugf("error - Out of memory\n");
        close(fd);
        return 5;
    }

    if (lseek(fd, (off_t)readint(&fh.OffBits), SEEK_SET) < 0)
    {
        debugf("error - Can't seek to start of image data\n");
        close(fd);
        return 6;
    }
    if (read(fd, (unsigned char*)bmp, (int)size) != size)
    {
        debugf("error - Can't read image\n");
        close(fd);
        return 7;
    }

    close(fd);
    *get_width = width;
    *get_height = height;

    switch (depth)
    {
      case 1:
        for (row = 0; row < height; row++)
            for (col = 0; col < width; col++)
            {
                data = (bmp[(height - 1 - row) * padded_width + col / 8]
                        >> (~col & 7)) & 1;
                (*bitmap)[row * width + col] = palette[data];
            }
        break;

      case 4:
        for (row = 0; row < height; row++)
            for (col = 0; col < width; col++)
            {
                data = (bmp[(height - 1 - row) * padded_width + col / 2]
                        >> (4 * (~col & 1))) & 0x0F;
                (*bitmap)[row * width + col] = palette[data];
            }
        break;

      case 8:
        for (row = 0; row < height; row++)
            for (col = 0; col < width; col++)
            {
                data = bmp[(height - 1 - row) * padded_width + col];
                (*bitmap)[row * width + col] = palette[data];
            }
        break;

      case 16:
        for (row = 0; row < height; row++)
            for (col = 0; col < width; col++)
            {
                data = readshort(&bmp[(height - 1 - row) * padded_width + 2 * col]);
                (*bitmap)[row * width + col].rgbRed =
                    ((data >> 7) & 0xF8) | ((data >> 12) & 0x07);
                (*bitmap)[row * width + col].rgbGreen =
                    ((data >> 2) & 0xF8) | ((data >> 7) & 0x07);
                (*bitmap)[row * width + col].rgbBlue =
                    ((data << 3) & 0xF8) | ((data >> 2) & 0x07);
                (*bitmap)[row * width + col].rgbReserved = 0;
            }
        break;

      case 24:
        for (row = 0; row < height; row++)
            for (col = 0; col < width; col++)
            {
                i = (height - 1 - row) * padded_width + 3 * col;
                (*bitmap)[row * width + col].rgbRed = bmp[i+2];
                (*bitmap)[row * width + col].rgbGreen = bmp[i+1];
                (*bitmap)[row * width + col].rgbBlue = bmp[i];
                (*bitmap)[row * width + col].rgbReserved = 0;
            }
        break;

      case 32:
        for (row = 0; row < height; row++)
            for (col = 0; col < width; col++)
            {
                i = (height - 1 - row) * padded_width + 4 * col;
                (*bitmap)[row * width + col].rgbRed = bmp[i+2];
                (*bitmap)[row * width + col].rgbGreen = bmp[i+1];
                (*bitmap)[row * width + col].rgbBlue = bmp[i];
                (*bitmap)[row * width + col].rgbReserved = 0;
            }
        break;

      default: /* should never happen */
        debugf("error - Unsupported bitmap depth %d.\n", depth);
        return 8;
    }

    free(bmp);

    return 0; /* success */
}

/****************************************************************************
 * transform_bitmap()
 *
 * Transform a 4-byte-per-pixel bitmap (RGBQUAD) into one of the supported
 * destination formats
 ****************************************************************************/

int transform_bitmap(const struct RGBQUAD *src, int width, int height,
                     int format, union RAWDATA *dst, int *dst_width,
                     int *dst_height, int *dst_depth)
{
    int row, col;
    int dst_w, dst_h, dst_d;
    int alloc_size;
    union RAWDATA dest;

    switch (format)
    {
      case 0: /* Archos recorders, Ondio, Iriver H1x0 monochrome */
        dst_w = width;
        dst_h = (height + 7) / 8;
        dst_d = 8;
        break;

      case 1: /* Archos player graphics library */
        dst_w = (width + 7) / 8;
        dst_h = height;
        dst_d = 8;
        break;

      case 2: /* Iriver H1x0 4-grey */
        dst_w = width;
        dst_h = (height + 3) / 4;
        dst_d = 8;
        break;

      case 3: /* Canonical 8-bit grayscale */
        dst_w = width;
        dst_h = height;
        dst_d = 8;
        break;

      case 4: /* 16-bit packed RGB (5-6-5) */
      case 5: /* 16-bit packed and byte-swapped RGB (5-6-5) */
      case 8: /* 16-bit packed RGB (5-6-5) vertical stride*/
        dst_w = width;
        dst_h = height;
        dst_d = 16;
        break;

      case 6: /* greyscale iPods 4-grey */
        dst_w = (width + 3) / 4;
        dst_h = height;
        dst_d = 8;
        break;

      case 7: /* greyscale X5 remote 4-grey */
        dst_w = width;
        dst_h = (height + 7) / 8;
        dst_d = 16;
        break;

      case 9: /* 24-bit BGR */
        dst_w = width;
        dst_h = height;
        dst_d = 24;
        break;

      default: /* unknown */
        debugf("error - Undefined destination format\n");
        return 1;
    }

    if (dst_d <= 16)
        alloc_size = sizeof(dest.d16[0]);
    else /* 24 bit */
        alloc_size = sizeof(dest.d24[0]);
    dest.d = calloc(dst_w * dst_h, alloc_size);

    if (dest.d == NULL)
    {
        debugf("error - Out of memory.\n");
        return 2;
    }
    *dst_width = dst_w;
    *dst_height = dst_h;
    *dst_depth = dst_d;
    *dst = dest;

    switch (format)
    {
      case 0: /* Archos recorders, Ondio, Iriver H1x0 b&w */
        for (row = 0; row < height; row++)
            for (col = 0; col < width; col++)
            {
                dest.d16[(row/8) * dst_w + col] |=
                       (~brightness(src[row * width + col]) & 0x80) >> (~row & 7);
            }
        break;

      case 1: /* Archos player graphics library */
        for (row = 0; row < height; row++)
            for (col = 0; col < width; col++)
            {
                dest.d16[row * dst_w + (col/8)] |=
                       (~brightness(src[row * width + col]) & 0x80) >> (col & 7);
            }
        break;

      case 2: /* Iriver H1x0 4-grey */
        for (row = 0; row < height; row++)
            for (col = 0; col < width; col++)
            {
                dest.d16[(row/4) * dst_w + col] |=
                       (~brightness(src[row * width + col]) & 0xC0) >> (2 * (~row & 3));
            }
        break;

      case 3: /* Canonical 8-bit grayscale */
        for (row = 0; row < height; row++)
            for (col = 0; col < width; col++)
            {
                dest.d16[row * dst_w + col] = brightness(src[row * width + col]);
            }
        break;

      case 4: /* 16-bit packed RGB (5-6-5) */
      case 5: /* 16-bit packed and byte-swapped RGB (5-6-5) */
        for (row = 0; row < height; row++)
            for (col = 0; col < width; col++)
            {
                unsigned short rgb =
                    (((src[row * width + col].rgbRed >> 3) << 11) |
                     ((src[row * width + col].rgbGreen >> 2) << 5) |
                     ((src[row * width + col].rgbBlue >> 3)));

                if (format == 4)
                    dest.d16[row * dst_w + col] = rgb;
                else
                    dest.d16[row * dst_w + col] = ((rgb&0xff00)>>8)|((rgb&0x00ff)<<8);
            }
        break;

      case 6: /* greyscale iPods 4-grey */
        for (row = 0; row < height; row++)
            for (col = 0; col < width; col++)
            {
                dest.d16[row * dst_w + (col/4)] |=
                       (~brightness(src[row * width + col]) & 0xC0) >> (2 * (col & 3));
            }
        break;

      case 7: /* greyscale X5 remote 4-grey */
        for (row = 0; row < height; row++)
            for (col = 0; col < width; col++)
            {
                unsigned short data = (~brightness(src[row * width + col]) & 0xC0) >> 6;

                data = (data | (data << 7)) & 0x0101;
                dest.d16[(row/8) * dst_w + col] |= data << (row & 7);
            }
        break;
        
      case 8: /* 16-bit packed RGB (5-6-5) vertical stride*/
        for (row = 0; row < height; row++)
            for (col = 0; col < width; col++)
            {
                unsigned short rgb =
                    (((src[row * width + col].rgbRed >> 3) << 11) |
                     ((src[row * width + col].rgbGreen >> 2) << 5) |
                     ((src[row * width + col].rgbBlue >> 3)));
                     
                dest.d16[col * dst_h + row] = rgb;
            }
        break;

      case 9: /* 24-bit RGB */
        for (row = 0; row < height; row++)
            for (col = 0; col < width; col++)
            {
                dest.d24[row * width + col].r = src[row * width + col].rgbRed;
                dest.d24[row * width + col].g = src[row * width + col].rgbGreen;
                dest.d24[row * width + col].b = src[row * width + col].rgbBlue;
            }
        break;
    }

    return 0;
}

/****************************************************************************
 * generate_c_source()
 *
 * Outputs a C source code with the bitmap in an array, accompanied by
 * some #define's
 ****************************************************************************/

void generate_c_source(char *id, char* header_dir, int width, int height,
                       const union RAWDATA *t_bitmap, int t_width,
                       int t_height, int t_depth, bool t_mono, bool create_bm)
{
    FILE *f;
    FILE *fh;
    int i, a;
    char header_name[1024];
    bool have_header = header_dir && header_dir[0];
    create_bm = have_header && create_bm;

    if (!id || !id[0])
        id = "bitmap";

    f = stdout;

    if (have_header)
    {
        snprintf(header_name,sizeof(header_name),"%s/%s.h",header_dir,id);
        fh = fopen(header_name,"w+");

        if (fh == NULL)
        {
            debugf("error - can't open '%s'\n", header_name);
            return;
        }
        fprintf(fh,
                "#define BMPHEIGHT_%s %d\n"
                "#define BMPWIDTH_%s %d\n",
                id, height, id, width);
        if (t_depth <= 8)
            fprintf(fh, "extern const unsigned char %s[];\n", id);
        else if (t_depth <= 16)
            fprintf(fh, "extern const unsigned short %s[];\n", id);
        else
            fprintf(fh, "extern const fb_data %s[];\n", id);


        if (create_bm)
        {
            fprintf(f, "#include \"lcd.h\"\n");
            fprintf(fh, "extern const struct bitmap bm_%s;\n", id);
        }
        fclose(fh);
    } else {
        fprintf(f,
                "#define BMPHEIGHT_%s %d\n"
                "#define BMPWIDTH_%s %d\n",
                id, height, id, width);
    }

    if (create_bm) {
        fprintf(f, "#include \"%s\"\n", header_name);
    }

    if (t_depth <= 8)
        fprintf(f, "const unsigned char %s[] = {\n", id);
    else if (t_depth == 16)
        fprintf(f, "const unsigned short %s[] = {\n", id);
    else if (t_depth == 24)
        fprintf(f, "const fb_data %s[] = {\n", id);

    for (i = 0; i < t_height; i++)
    {
        for (a = 0; a < t_width; a++)
        {
            if (t_depth <= 8)
                fprintf(f, "0x%02x,%c", t_bitmap->d16[i * t_width + a],
                        (a + 1) % 13 ? ' ' : '\n');
            else if (t_depth == 16)
                fprintf(f, "0x%04x,%c", t_bitmap->d16[i * t_width + a],
                        (a + 1) % 10 ? ' ' : '\n');
            else if (t_depth == 24)
                fprintf(f, "{ .r = 0x%02x, .g = 0x%02x, .b = 0x%02x },%c",
                        t_bitmap->d24[i * t_width + a].r,
                        t_bitmap->d24[i * t_width + a].g,
                        t_bitmap->d24[i * t_width + a].b,
                        (a + 1) % 4 ? ' ' : '\n');
        }
        fprintf(f, "\n");
    }

    fprintf(f, "\n};\n\n");

    if (create_bm) {
        char format_line[] = "    .format = FORMAT_NATIVE, \n";
        fprintf(f, "const struct bitmap bm_%s = { \n"
                   "    .width = BMPWIDTH_%s, \n"
                   "    .height = BMPHEIGHT_%s, \n"
                   "%s"
                   "    .data = (unsigned char*)%s,\n"
                   "};\n",
                    id, id, id,
                    t_mono ? "" : format_line,
                    id);
    }
}

void generate_raw_file(const union RAWDATA *t_bitmap, 
                       int t_width, int t_height, int t_depth)
{
    FILE *f;
    int i, a;
    unsigned char lo,hi;

    f = stdout;

    for (i = 0; i < t_height; i++)
    {
        for (a = 0; a < t_width; a++)
        {
            if (t_depth <= 8)
            {
                lo = (t_bitmap->d16[i * t_width + a] & 0x00ff);
                fwrite(&lo, 1, 1, f);
            }
            else if (t_depth == 16)
            {
                lo = (t_bitmap->d16[i * t_width + a] & 0x00ff);
                hi = (t_bitmap->d16[i * t_width + a] & 0xff00) >> 8;
                fwrite(&lo, 1, 1, f);
                fwrite(&hi, 1, 1, f);
            }
            else /* 24 */
            {
                fwrite(&t_bitmap->d24[i * t_width + a], 3, 1, f);
            }
        }
    }
}

/****************************************************************************
 * generate_ascii()
 *
 * Outputs an ascii picture of the bitmap
 ****************************************************************************/

void generate_ascii(int width, int height, struct RGBQUAD *bitmap)
{
    FILE *f;
    int x, y;

    f = stdout;

    /* for screen output debugging */
    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            fprintf(f, (brightness(bitmap[y * width + x]) & 0x80) ? " " : "*");
        }
        fprintf(f, "\n");
    }
}

void print_usage(void)
{
    printf("Usage: %s [-i <id>] [-a] <bitmap file>\n"
           "\t-i <id>  Bitmap name (default is filename without extension)\n"
           "\t-h <dir> Create header file in <dir>/<id>.h\n"
           "\t-a       Show ascii picture of bitmap\n"
           "\t-b       Create bitmap struct along with pixel array\n"
           "\t-r       Generate RAW file (little-endian)\n"
           "\t-f <n>   Generate destination format n, default = 0\n"
           "\t         0  Archos recorder, Ondio, Iriver H1x0 mono\n"
           , APPLICATION_NAME);
    printf("\t         1  Archos player graphics library\n"
           "\t         2  Iriver H1x0 4-grey\n"
           "\t         3  Canonical 8-bit greyscale\n"
           "\t         4  16-bit packed 5-6-5 RGB (iriver H300)\n"
           "\t         5  16-bit packed and byte-swapped 5-6-5 RGB (iPod, Fuzev2)\n"
           "\t         6  Greyscale iPod 4-grey\n"
           "\t         7  Greyscale X5 remote 4-grey\n"
           "\t         8  16-bit packed 5-6-5 RGB with a vertical stride\n"
           "\t         9  24-bit BGR\n");
    printf("build date: " __DATE__ "\n\n");
}

int main(int argc, char **argv)
{
    char *bmp_filename = NULL;
    char *id = NULL;
    char* header_dir = NULL;
    int i;
    int ascii = false;
    int format = 0;
    struct RGBQUAD *bitmap = NULL;
    union RAWDATA t_bitmap = { NULL };
    int width, height;
    int t_width, t_height, t_depth;
    bool raw = false;
    bool create_bm = false;


    for (i = 1;i < argc;i++)
    {
        if (argv[i][0] == '-')
        {
            switch (argv[i][1])
            {
              case 'h':   /* .h filename */
                if (argv[i][2])
                {
                    header_dir = &argv[i][2];
                }
                else if (argc > i+1)
                {
                    header_dir = argv[i+1];
                    i++;
                }
                else
                {
                    print_usage();
                    exit(1);
                }
                break;

              case 'i':   /* ID */
                if (argv[i][2])
                {
                    id = &argv[i][2];
                }
                else if (argc > i+1)
                {
                    id = argv[i+1];
                    i++;
                }
                else
                {
                    print_usage();
                    exit(1);
                }
                break;

              case 'a':   /* Ascii art */
                ascii = true;
                break;

              case 'b':
                create_bm = true;
                break;

              case 'r':   /* Raw File */
                raw = true;
                break;

              case 'f':
                if (argv[i][2])
                {
                    format = atoi(&argv[i][2]);
                }
                else if (argc > i+1)
                {
                    format = atoi(argv[i+1]);
                    i++;
                }
                else
                {
                    print_usage();
                    exit(1);
                }
                break;

              default:
                print_usage();
                exit(1);
                break;
            }
        }
        else
        {
            if (!bmp_filename)
            {
                bmp_filename = argv[i];
            }
            else
            {
                print_usage();
                exit(1);
            }
        }
    }

    if (!bmp_filename)
    {
        print_usage();
        exit(1);
    }

    if (!id)
    {
        char *ptr=strrchr(bmp_filename, '/');
        if (ptr)
            ptr++;
        else
            ptr = bmp_filename;
        id = strdup(ptr);
        for (i = 0; id[i]; i++)
            if (id[i] == '.')
                id[i] = '\0';
    }

    if (read_bmp_file(bmp_filename, &width, &height, &bitmap))
        exit(1);


    if (ascii)
    {
        generate_ascii(width, height, bitmap);
    }
    else
    {
        if (transform_bitmap(bitmap, width, height, format, &t_bitmap, 
                             &t_width, &t_height, &t_depth))
            exit(1);
        if(raw)
            generate_raw_file(&t_bitmap, t_width, t_height, t_depth);
        else
            generate_c_source(id, header_dir, width, height, &t_bitmap, 
                              t_width, t_height, t_depth,
                              format <= 1, create_bm);
    }

    return 0;
}
