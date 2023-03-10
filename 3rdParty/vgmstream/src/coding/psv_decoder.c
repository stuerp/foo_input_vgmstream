#include <math.h>
#include "coding.h"
#include "../util.h"

/**
 * Sony's HEVAG (High Efficiency VAG) ADPCM, used in Vita/PS4 games (hardware decoded).
 * Evolution of the regular VAG (same flags and frames), uses 4 history samples and a bigger table.
 *
 * Reverse engineered from PC tools, base int code by daemon1 (using K=2^13 aka coefs * 8192, 0.9375 > 7680,
 * ).
 *
 * HEVAG is made to be compatible with original VAG, so table indexes <= 0x1c behave like old codec
 * setting hist3/4 coefs to 0 (as a minor optimization og code won't mult hist3/4 in lower table indexes).
 * 
 * Original code uses SIMD to do 4 codes at once (unrolled doing 7 groups of 4 codes = 28 samples).
 * Tables are rather complex to (presumable) handle hist dependencies, and since it's VAG compatible
 * should be the same (maybe rounding diffs?). Hist and output are floats (output converted to +-1.0
 * or +-32768.0 +-0.5 based on a flag). Code below just does it linearly 1 by 1 in pcm16.
 */

/* ADPCM table */
static const float hevag_coefs_f[128][4] = {
    {-0.0, -0.0, -0.0, -0.0 },
    { 0.9375, -0.0, -0.0, -0.0 },
    { 1.796875, -0.8125, -0.0, -0.0 },
    { 1.53125, -0.859375, -0.0, -0.0 },
    { 1.90625, -0.9375, -0.0, -0.0 },
    { 1.7982178, -0.86169434, -0.0, -0.0 },
    { 1.770874, -0.89916992, -0.0, -0.0 },
    { 1.6992188, -0.91821289, -0.0, -0.0 },
    { 1.6031494, -0.9375, -0.0, -0.0 },
    { 1.4682617, -0.9375, -0.0, -0.0 },
    { 1.3139648, -0.9375, -0.0, -0.0 },
    { 1.1424561, -0.9375, -0.0, -0.0 },
    { 0.95605469, -0.9375, -0.0, -0.0 },
    { 0.75695801, -0.9375, -0.0, -0.0 },
    { 0.54785156, -0.9375, -0.0, -0.0 },
    { 0.33166504, -0.9375, -0.0, -0.0 },
    { 0.11108398, -0.9375, -0.0, -0.0 },
    {-0.11108398, -0.9375, -0.0, -0.0 },
    {-0.33166504, -0.9375, -0.0, -0.0 },
    {-0.54785156, -0.9375, -0.0, -0.0 },
    {-0.75695801, -0.9375, -0.0, -0.0 },
    {-0.95605469, -0.9375, -0.0, -0.0 },
    {-1.1424561, -0.9375, -0.0, -0.0 },
    {-1.3139648, -0.9375, -0.0, -0.0 },
    {-1.4682617, -0.9375, -0.0, -0.0 },
    {-1.6031494, -0.9375, -0.0, -0.0 },
    {-1.6992188, -0.91821289, -0.0, -0.0 },
    {-1.770874, -0.89916992, -0.0, -0.0 },
    {-1.7982178, -0.86169434, -0.0, -0.0 },
    { 0.65625, -1.125, 0.40625, -0.375 },
    {-0.78125, -0.875, -0.40625, -0.28125 },
    {-1.28125, -0.90625, -0.4375, -0.125 },
    {-0.020385742, -0.33227539, -0.060302734, -0.066040039 },
    {-0.90698242, -0.27111816, -0.28051758, 0.051757812 },
    {-0.97668457, -0.38647461, -0.34350586, 0.03527832 },
    { 0.73461914, -0.57983398, 0.32336426, -0.15844727 },
    { 0.46362305, -0.84790039, 0.47302246, -0.1484375 },
    {-1.0054932, -0.31689453, -0.25280762, 0.027709961 },
    { 1.1229248, 0.24194336, -0.16870117, -0.28271484 },
    { 1.5894775, -0.37158203, -0.46289062, 0.15466309 },
    { 1.6005859, -0.54772949, -0.2746582, 0.20324707 },
    {-0.20361328, -0.45703125, -0.78808594, 0.10253906 },
    { 0.95446777, -0.52832031, 0.25769043, -0.061767578 },
    { 1.168335, -0.16308594, -0.092407227, 0.059448242 },
    { 1.2246094, -0.31274414, 0.036621094, 0.024291992 },
    {-0.57922363, -0.50317383, -0.66967773, -0.18225098 },
    {-0.71972656, 0.2902832, -0.58435059, -0.84802246 },
    {-0.14562988, -1.112915, -0.15100098, -0.38012695 },
    { 0.33972168, -0.86767578, -0.19226074, -0.17663574 },
    {-0.89526367, -0.25170898, -0.27001953, 0.054443359 },
    { 0.7479248, -0.3145752, -0.038452148, -0.0021972656 },
    { 1.1544189, -0.22680664, 0.012451172, 0.031494141 },
    { 0.96142578, -0.54724121, 0.25952148, -0.065673828 },
    {-0.87548828, -0.21911621, -0.25256348, 0.058837891 },
    {-0.89819336, -0.2565918, -0.27258301, 0.053710938 },
    {-1.1193848, -0.42834473, -0.32641602, -0.047729492 },
    {-0.32202148, -0.32312012, -0.23547363, -0.1998291 },
    { 0.2286377, 1.1209717, 0.22705078, -0.70141602 },
    { 1.1247559, 0.22692871, -0.13720703, -0.29626465 },
    { 1.6118164, -0.36767578, -0.50524902, 0.16723633 },
    { 1.5181885, -0.58496094, -0.03125, 0.075927734 },
    {-0.32385254, -0.13964844, -0.38842773, -0.83959961 },
    { 1.1390381, -0.12792969, -0.10107422, 0.061889648 },
    { 0.20043945, -0.075683594, -0.11547852, -0.51623535 },
    { 0.51831055, -0.92590332, -0.065063477, -0.27575684 },
    {-1.097168, -0.47497559, -0.34265137, 0.0053710938 },
    {-0.31274414, -0.3338623, -0.21118164, -0.23181152 },
    { 0.38842773, -0.058959961, -0.087158203, -0.17346191 },
    { 0.96887207, -0.46923828, 0.34436035, -0.12438965 },
    { 1.229126, -0.31848145, 0.038330078, 0.023803711 },
    { 1.0253906, -0.40246582, 0.18933105, -0.018920898 },
    {-1.0411377, -0.33874512, -0.296875, -0.041015625 },
    { 1.1568604, -0.22973633, 0.013183594, 0.03125 },
    { 0.0091552734, -0.27355957, -0.036376953, -0.84680176 },
    {-1.1160889, -0.5078125, -0.36169434, 0.00061035156 },
    {-0.88745117, -0.23901367, -0.26318359, 0.056152344 },
    {-0.33447266, 0.45715332, 0.72460938, -0.13293457 },
    { 1.0977783, 0.23779297, -0.083374023, -0.33007812 },
    { 1.5992432, -0.34606934, -0.47045898, 0.12878418 },
    { 1.164917, -0.23937988, 0.015869141, 0.030517578 },
    { 0.64355469, -0.52124023, 0.38134766, -0.38537598 },
    {-0.93945312, -0.41296387, -0.3548584, -0.055664062 },
    { 0.89221191, 0.3079834, 0.052978516, -0.30041504 },
    { 1.2542725, -0.34997559, 0.047729492, 0.020996094 },
    { 1.3354492, -0.45422363, 0.081176758, 0.01184082 },
    { 0.0029296875, -0.037841797, -0.15405273, 0.0390625 },
    {-0.99145508, -0.29431152, -0.28210449, -0.033081055 },
    {-1.0389404, -0.37438965, -0.28527832, 0.019897461 },
    { 0.039794922, -0.46948242, 0.051147461, -0.1138916 },
    { 1.0858154, 0.26782227, -0.066040039, -0.3515625 },
    { 1.4737549, -0.22900391, -0.24621582, -0.073364258 },
    { 1.0655518, -0.41784668, 0.2043457, -0.020629883 },
    { 1.5808105, -0.46960449, -0.36706543, 0.23754883 },
    { 1.2253418, -0.3137207, 0.036865234, 0.024169922 },
    { 1.1456299, -0.33654785, 0.12304688, 0.0050048828 },
    {-0.57617188, -0.61108398, -0.34814453, -0.14172363 },
    { 0.96057129, -0.52807617, 0.26062012, -0.061157227 },
    { 0.29907227, -1.0494385, 0.15856934, -0.33935547 },
    { 1.2441406, -0.33728027, 0.043945312, 0.022094727 },
    { 1.3809814, -0.51428223, 0.10168457, 0.0064697266 },
    { 1.239502, -0.33154297, 0.042114258, 0.022583008 },
    { 1.1765137, -0.17297363, -0.08996582, 0.058837891 },
    { 0.47045898, -0.5559082, 0.3470459, -0.41467285 },
    { 0.81774902, -0.6907959, 0.27453613, -0.13110352 },
    { 1.3527832, -0.47705078, 0.088867188, 0.009765625 },
    {-0.12524414, -1.1975098, -0.098266602, -0.42260742 },
    { 1.269043, -0.45727539, 0.16687012, -0.01171875 },
    { 1.2557373, 0.12060547, -0.23376465, -0.17541504 },
    { 0.9708252, 0.47338867, -0.093261719, -0.39831543 },
    { 1.5489502, -0.4119873, -0.40942383, 0.25378418 },
    { 0.81066895, 0.38647461, 0.028198242, -0.25500488 },
    {-0.28662109, -0.89770508, -0.23730469, -0.50317383 },
    { 1.1340332, -0.49304199, 0.23010254, -0.030029297 },
    { 0.56555176, -0.78161621, 0.21337891, -0.19763184 },
    { 1.3729248, -0.50354004, 0.097900391, 0.0074462891 },
    { 1.1971436, -0.27880859, 0.026733398, 0.027099609 },
    { 1.1884766, -0.1875, -0.086181641, 0.057739258 },
    { 1.0302734, -0.41943359, 0.19067383, -0.021484375 },
    { 1.1361084, -0.12463379, -0.10192871, 0.062133789 },
    { 0.20727539, -1.1016846, 0.083984375, -0.37072754 },
    { 1.2468262, -0.34069824, 0.044921875, 0.021850586 },
    { 1.0241699, 0.39648438, -0.092529297, -0.36486816 },
    { 0.87902832, 0.40478516, 0.0056152344, -0.3190918 },
    {-0.010742188, -0.95324707, -0.065673828, -0.5579834 },
    { 0.75598145, -0.63342285, 0.33691406, -0.15197754 },
    { 1.5045166, -0.1574707, -0.40087891, 0.030883789 },
    { 1.5947266, -0.49743652, -0.34472656, 0.22912598 },
    { 0.65100098, 0.36608887, 0.094604492, -0.13818359 },
};

#if 0
/* original scale used instead of "shift = 20 - i"
 * adjusted for +-1.0 floats, so if used for pcm16 needs to be scaled up ([i] = 1.0 / 128.0*i) */
static const float scale_table_f[16] = {
    0.0078125f, 0.00390625f, 0.001953125f, 0.0009765625f,
    0.00048828125f, 0.000244140625f, 0.0001220703125f, 0.00006103515625f,
    0.000030517578125f, 0.0000152587890625f, 0.00000762939453125f, 0.000003814697265625f,
    0.0000019073486328125f, 0.00000095367431640625f, 0.000000476837158203125f, 0.000000238418579101562f,
};
#endif


void decode_hevag(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    uint8_t frame[0x10] = {0};
    off_t frame_offset;
    int i, frames_in, sample_count = 0;
    size_t bytes_per_frame, samples_per_frame;
    int index, shift, flag;
    int32_t hist1 = stream->adpcm_history1_32;
    int32_t hist2 = stream->adpcm_history2_32;
    int32_t hist3 = stream->adpcm_history3_32;
    int32_t hist4 = stream->adpcm_history4_32;


    /* external interleave (fixed size), mono */
    bytes_per_frame = 0x10;
    samples_per_frame = (bytes_per_frame - 0x02) * 2; /* always 28 */
    frames_in = first_sample / samples_per_frame;
    first_sample = first_sample % samples_per_frame;

    /* parse frame header */
    frame_offset = stream->offset + bytes_per_frame * frames_in;
    read_streamfile(frame, frame_offset, bytes_per_frame, stream->streamfile); /* ignore EOF errors */
    index = (frame[0] >> 4) & 0xf;
    shift = (frame[0] >> 0) & 0xf;
    index = ((frame[1] >> 0) & 0xf0) | index;
    flag = (frame[1] >> 0) & 0xf; /* same flags */

    VGM_ASSERT_ONCE(index > 127 || shift > 12, "HEVAG: in+correct coefs/shift at %x\n", (uint32_t)frame_offset);
    if (index > 127) /* not validated so would read garbage, simulate with 0 */
        index = 0;
    //if (shift > 12) /* shouldn't happen but accepted in scale_table (just zeroes codes) */
    //    shift = 9;


    /* decode nibbles */
    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        int32_t code, sample = 0;

        if (flag < 0x07) { /* with flag 0x07 decoded sample must be 0 */
            uint8_t nibbles = frame[0x02 + i/2];

            code = (i&1 ? /* low nibble first */
                    get_high_nibble_signed(nibbles):
                    get_low_nibble_signed(nibbles));

            //code = (code << 0x4) * scale_table_f[shift]; /* OG s8 sign extension w/ scale table (+-1.0) */
            code = ((code << 12) >> shift);
            sample = hist1 * hevag_coefs_f[index][0] +
                     hist2 * hevag_coefs_f[index][1] +
                     hist3 * hevag_coefs_f[index][2] +
                     hist4 * hevag_coefs_f[index][3];
            sample = code + sample; /* no clamping here */
#if 0
            // base int code
            sample = code << (20 - shift_factor);
            sample = (hists... * coefs....) >> 5) + sample;
            sample >>= 8;
#endif
        }

        outbuf[sample_count] = clamp16(sample);
        sample_count += channelspacing;

        hist4 = hist3;
        hist3 = hist2;
        hist2 = hist1;
        hist1 = sample;
    }

    stream->adpcm_history1_32 = hist1;
    stream->adpcm_history2_32 = hist2;
    stream->adpcm_history3_32 = hist3;
    stream->adpcm_history4_32 = hist4;
}
