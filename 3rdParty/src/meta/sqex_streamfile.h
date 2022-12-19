#ifndef _SQEX_SEAD_STREAMFILE_H_
#define _SQEX_SEAD_STREAMFILE_H_
#include "../streamfile.h"


typedef struct {
    size_t start;
    size_t key_start;
} sqex_io_data;


static size_t sqex_io_read(STREAMFILE *sf, uint8_t *dest, off_t offset, size_t length, sqex_io_data* data) {
    /* Found in FFXII_TZA.exe (same key in SCD Ogg V3), also in AKB's sdlib as "EncKey" */
    static const uint8_t key[0x100] = {
        0x3A,0x32,0x32,0x32,0x03,0x7E,0x12,0xF7,0xB2,0xE2,0xA2,0x67,0x32,0x32,0x22,0x32, // 00-0F
        0x32,0x52,0x16,0x1B,0x3C,0xA1,0x54,0x7B,0x1B,0x97,0xA6,0x93,0x1A,0x4B,0xAA,0xA6, // 10-1F
        0x7A,0x7B,0x1B,0x97,0xA6,0xF7,0x02,0xBB,0xAA,0xA6,0xBB,0xF7,0x2A,0x51,0xBE,0x03, // 20-2F
        0xF4,0x2A,0x51,0xBE,0x03,0xF4,0x2A,0x51,0xBE,0x12,0x06,0x56,0x27,0x32,0x32,0x36, // 30-3F
        0x32,0xB2,0x1A,0x3B,0xBC,0x91,0xD4,0x7B,0x58,0xFC,0x0B,0x55,0x2A,0x15,0xBC,0x40, // 40-4F
        0x92,0x0B,0x5B,0x7C,0x0A,0x95,0x12,0x35,0xB8,0x63,0xD2,0x0B,0x3B,0xF0,0xC7,0x14, // 50-5F
        0x51,0x5C,0x94,0x86,0x94,0x59,0x5C,0xFC,0x1B,0x17,0x3A,0x3F,0x6B,0x37,0x32,0x32, // 60-6F
        0x30,0x32,0x72,0x7A,0x13,0xB7,0x26,0x60,0x7A,0x13,0xB7,0x26,0x50,0xBA,0x13,0xB4, // 70-7F
        0x2A,0x50,0xBA,0x13,0xB5,0x2E,0x40,0xFA,0x13,0x95,0xAE,0x40,0x38,0x18,0x9A,0x92, // 80-8F
        0xB0,0x38,0x00,0xFA,0x12,0xB1,0x7E,0x00,0xDB,0x96,0xA1,0x7C,0x08,0xDB,0x9A,0x91, // 90-9F
        0xBC,0x08,0xD8,0x1A,0x86,0xE2,0x70,0x39,0x1F,0x86,0xE0,0x78,0x7E,0x03,0xE7,0x64, // A0-AF
        0x51,0x9C,0x8F,0x34,0x6F,0x4E,0x41,0xFC,0x0B,0xD5,0xAE,0x41,0xFC,0x0B,0xD5,0xAE, // B0-BF
        0x41,0xFC,0x3B,0x70,0x71,0x64,0x33,0x32,0x12,0x32,0x32,0x36,0x70,0x34,0x2B,0x56, // C0-CF
        0x22,0x70,0x3A,0x13,0xB7,0x26,0x60,0xBA,0x1B,0x94,0xAA,0x40,0x38,0x00,0xFA,0xB2, // D0-DF
        0xE2,0xA2,0x67,0x32,0x32,0x12,0x32,0xB2,0x32,0x32,0x32,0x32,0x75,0xA3,0x26,0x7B, // E0-EF
        0x83,0x26,0xF9,0x83,0x2E,0xFF,0xE3,0x16,0x7D,0xC0,0x1E,0x63,0x21,0x07,0xE3,0x01, // F0-FF
    };
    int i;
    size_t bytes = read_streamfile(dest, offset, length, sf);

    /* decrypt data (xor) */
    //if (offset >= data->start) {
        for (i = 0; i < bytes; i++) {
            if (offset + i >= data->start) { //todo recheck
                dest[i] ^= key[(data->key_start + (offset - data->start) + i) % sizeof(key)];
            }
        }
    //}

    return bytes;
}

/* decrypts subfile if needed */
static STREAMFILE* setup_sqex_streamfile(STREAMFILE* sf, off_t subfile_offset, size_t subfile_size, int encryption, size_t header_size, size_t key_start, const char* ext) {
    STREAMFILE* new_sf = NULL;

    /* setup sf */
    new_sf = open_wrap_streamfile(sf);
    new_sf = open_clamp_streamfile_f(new_sf, subfile_offset, subfile_size);
    if (encryption) {
        sqex_io_data io_data = {0};

        io_data.start = header_size;
        io_data.key_start = key_start;

        new_sf = open_io_streamfile_f(new_sf, &io_data, sizeof(sqex_io_data), sqex_io_read, NULL);
    }

    new_sf = open_fakename_streamfile_f(new_sf, NULL, ext);
    return new_sf;
}

#endif /* _SQEX_SEAD_STREAMFILE_H_ */
