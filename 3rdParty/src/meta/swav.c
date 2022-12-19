#include "meta.h"
#include "../util.h"


/* SWAV - wave files generated by the DS SDK */
VGMSTREAM* init_vgmstream_swav(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t  start_offset, data_size, file_size;
    int channels, loop_flag, codec, bits_per_sample, sample_rate;
    int32_t loop_start, loop_end;
    coding_t coding_type;
    


    /* checks */
    if (!is_id32be(0x00,sf, "SWAV"))
        goto fail;

    /* .swav: standard
     * .adpcm: Merlin - A Servant of Two Masters (DS) */
    if (!check_extensions(sf, "swav,adpcm"))
        goto fail;

    /* 0x04: BOM mark */
    /* 0x06: version? (1.00) */
    file_size = read_u32le(0x08,sf);
    /* 0x0c: always 16? */
    /* 0x0e: always 1? */

    if (!is_id32be(0x10,sf, "DATA"))
        goto fail;
    data_size = read_u32le(0x14,sf);
    codec = read_u8(0x18,sf);
    loop_flag = read_u8(0x19,sf);
    sample_rate = read_u16le(0x1A,sf);
    /* 0x1c: related to size? */
    loop_start = read_u16le(0x1E,sf);
    loop_end = read_s32le(0x20,sf);

    start_offset = 0x24;

    /* strange values found in Face Training (DSi) samples, may be pitch/etc reference info? (samples sounds ok like this) */
    if (sample_rate < 0x2000)
        sample_rate = 44100;

    channels = 1;
    if (get_streamfile_size(sf) != file_size) {
        if (get_streamfile_size(sf) != (file_size - 0x24) * 2 + 0x24)
            goto fail;
        channels = 2;
    }

    switch (codec) {
        case 0:
            coding_type = coding_PCM8;
            bits_per_sample = 8;
            break;
        case 1:
            coding_type = coding_PCM16LE;
            bits_per_sample = 16;
            break;
        case 2:
            coding_type = coding_IMA_int;
            bits_per_sample = 4;
            break;
        default:
            goto fail;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->num_samples = (data_size - 0x14) * 8 / bits_per_sample;
    vgmstream->sample_rate = sample_rate;
    if (loop_flag) {
        vgmstream->loop_start_sample = loop_start * 32 / bits_per_sample;
        vgmstream->loop_end_sample = loop_end * 32 / bits_per_sample + vgmstream->loop_start_sample;
    }

    if (coding_type == coding_IMA_int) {
        /* handle IMA frame header */
        vgmstream->loop_start_sample -= 32 / bits_per_sample;
        vgmstream->loop_end_sample -= 32 / bits_per_sample;
        vgmstream->num_samples -= 32 / bits_per_sample;

        {
            int i;
            for (i = 0; i < channels; i++) {
                vgmstream->ch[i].adpcm_history1_32 = read_s16le(start_offset + 0 + 4*i, sf);
                vgmstream->ch[i].adpcm_step_index = read_s16le(start_offset + 2 + 4*i, sf);
            }
        }

        start_offset += 4 * channels;
    }

    vgmstream->coding_type = coding_type;
    vgmstream->meta_type = meta_SWAV;
    if (channels == 2) {
        vgmstream->layout_type = layout_interleave;
        vgmstream->interleave_block_size = 1;
    } else {
        vgmstream->layout_type = layout_none;
    }

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
