#include "utils/lv_snapshot.h"

#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <iostream>

#include <zlib.h>

/* ---------------------------------------------------------------------------
 * Minimal PNG encoder (truecolor RGB, 8-bit, zlib-compressed IDAT).
 * No libpng dependency — only zlib's compress2() + crc32() are used.
 * ------------------------------------------------------------------------- */

/* Big-endian helper: append a 4-byte value. */
static void be32(uint8_t out[4], uint32_t v)
{
    out[0] = (uint8_t)(v >> 24);
    out[1] = (uint8_t)(v >> 16);
    out[2] = (uint8_t)(v >> 8);
    out[3] = (uint8_t)(v);
}

/* Write one PNG chunk: length + type + data + CRC32(type+data). */
static int png_write_chunk(std::ofstream & f, const char type[4],
                           const uint8_t * data, uint32_t len)
{
    uint8_t hdr[4];
    be32(hdr, len);
    f.write((const char *)hdr, 4);
    f.write(type, 4);
    if (len) f.write((const char *)data, len);
    if (!f.good()) return -1;

    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (const Bytef *)type, 4);
    if (len) crc = crc32(crc, data, len);
    uint8_t crcb[4];
    be32(crcb, (uint32_t)crc);
    f.write((const char *)crcb, 4);
    return f.good() ? 0 : -1;
}

int lv_snapshot_save_png(lv_obj_t * obj, const char * path)
{
    if(!obj || !path) return -1;

    /* 1. Snapshot the object tree into a draw buffer at RGB888 (24-bit). */
    lv_draw_buf_t * buf = lv_snapshot_take(obj, LV_COLOR_FORMAT_RGB888);
    if(!buf) {
        std::cerr << "[snapshot] lv_snapshot_take failed (LV_USE_SNAPSHOT enabled?)\n";
        return -1;
    }

    const uint32_t w = buf->header.w;
    const uint32_t h = buf->header.h;
    const uint32_t stride = buf->header.stride;
    const uint8_t * px = buf->data;
    if(w == 0 || h == 0 || !px) {
        std::cerr << "[snapshot] bad draw buf (" << w << "x" << h << ")\n";
        lv_draw_buf_destroy(buf);
        return -1;
    }

    /* 2. Build raw scanlines (each prefixed with filter byte 0 = None).
     *    LVGL stores 24-bit pixels as B,G,R in memory; PNG wants R,G,B,
     *    so swap channels while copying. */
    const uint32_t row_len = w * 3;               /* payload bytes per row */
    const uint32_t raw_row = row_len + 1;         /* + filter byte */
    const size_t   raw_size = (size_t)raw_row * h;
    uint8_t * raw = (uint8_t *)malloc(raw_size);
    if(!raw) {
        lv_draw_buf_destroy(buf);
        return -1;
    }
    for(uint32_t y = 0; y < h; y++) {
        uint8_t * dst = raw + (size_t)y * raw_row;
        const uint8_t * src = px + (size_t)y * stride;
        dst[0] = 0;                               /* filter: None */
        for(uint32_t x = 0; x < w; x++) {
            dst[3 * x + 1] = src[3 * x + 2];      /* R */
            dst[3 * x + 2] = src[3 * x + 1];      /* G */
            dst[3 * x + 3] = src[3 * x + 0];      /* B */
        }
    }

    /* 3. Compress the scanlines with zlib -> IDAT payload. */
    uLongf comp_len = compressBound((uLong)raw_size);
    uint8_t * comp = (uint8_t *)malloc(comp_len);
    if(!comp) {
        free(raw);
        lv_draw_buf_destroy(buf);
        return -1;
    }
    if(compress2(comp, &comp_len, raw, (uLong)raw_size, 6) != Z_OK) {
        std::cerr << "[snapshot] zlib compress2 failed\n";
        free(comp);
        free(raw);
        lv_draw_buf_destroy(buf);
        return -1;
    }

    /* 4. Assemble the PNG file. */
    std::ofstream f(path, std::ios::binary);
    if(!f) {
        std::cerr << "[snapshot] cannot open " << path << "\n";
        free(comp);
        free(raw);
        lv_draw_buf_destroy(buf);
        return -1;
    }

    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    f.write((const char *)sig, 8);

    uint8_t ihdr[13];
    be32(ihdr + 0, w);
    be32(ihdr + 4, h);
    ihdr[8]  = 8;   /* bit depth */
    ihdr[9]  = 2;   /* color type: truecolor (RGB) */
    ihdr[10] = 0;   /* compression */
    ihdr[11] = 0;   /* filter method */
    ihdr[12] = 0;   /* interlace */
    png_write_chunk(f, "IHDR", ihdr, sizeof(ihdr));
    png_write_chunk(f, "IDAT", comp, (uint32_t)comp_len);
    png_write_chunk(f, "IEND", NULL, 0);

    f.close();
    free(comp);
    free(raw);
    lv_draw_buf_destroy(buf);

    std::cerr << "[snapshot] saved " << w << "x" << h << " -> " << path << "\n";
    return 0;
}
