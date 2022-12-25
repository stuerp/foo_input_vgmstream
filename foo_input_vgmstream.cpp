#include <stdio.h>
#include <io.h>

#include <foobar2000/SDK/foobar2000.h>

extern "C"
{
#include <src/vgmstream.h>
#include <src/plugins.h>
}

#include <version.h>

#include "foo_input_vgmstream.h"
#include "FileTypeRegistrar.h"
#include "StreamFile.h"

#include "Resource.h"

#pragma warning(push)
#pragma warning(disable: 26447) // The function is declared 'noexcept' but calls function which may throw exceptions.
static void LogMessage(int, const char * message) noexcept
{
    FB2K_console_formatter() << STR_COMPONENT_FILENAME << ": " << message;
}
#pragma warning(pop)

#pragma region("Interface input_impl")
/// <summary>
/// Creates a new instance.
/// </summary>
#pragma warning(disable: 26455)
InputHandler::InputHandler()
{
    _VGMStream = nullptr;
    _SubsongIndex = 0; // 0 = not set, will be properly changed on first setup_vgmstream
    _DirectSubsong = false;
    _OutputChannelCount = 0;

    _IsDecoding = false;
    _LengthInSamples = 0;

    _DecodePositionInSamples = 0;
    _DecodePositionInMs = 0;

    _FadeLength = 10.0;
    _FadeDelay = 0.0;

    _LoopCount = 2.0;
    _LoopForever = false;
    _IgnoreLoop = false;

    _DisableSubsongs = false;

    _DownmixChannelCount = 0;
    _TagfileDisable = false;
    _OverrideTitle = false;

    _TagFileName = "!tags.m3u";

    LoadSettings();

    ::vgmstream_set_log_callback(VGM_LOG_LEVEL_ALL, &LogMessage);
}

// called first when a new file is accepted, before playing it
void InputHandler::open(service_ptr_t<file> file, const char * filePath, t_input_open_reason reason, abort_callback & abortHandler)
{
    if (filePath == nullptr)
        throw exception_io_data();

    _FilePath = filePath;

    // Allow non-existing files in some cases.
    const bool IsFileVirtual = (::vgmstream_is_virtual_filename(_FilePath) == 1) && !filesystem::g_exists(filePath, abortHandler);

    // Don't try to open virtual files as it'll fail.
    // (doesn't seem to have any adverse effect, except maybe no stats)
    // setup_vgmstream also makes further checks before file is finally opened.
    if (!IsFileVirtual)
    {
        // keep file stats around (timestamp, filesize)
        if (file.is_empty())
            input_open_file_helper(file, _FilePath, reason, abortHandler);

        _Stats  = file->get_stats(abortHandler);
        _Stats2 = file->get_stats2_(static_cast<uint32_t>(stats2_all), abortHandler);
    }

    switch (reason)
    {
        case input_open_decode: // prepare to retrieve info and decode
        case input_open_info_read: // prepare to retrieve info
            InitializeVGMStream(abortHandler); // must init vgmstream to get subsongs
            break;

        case input_open_info_write: // prepare to retrieve info and tag
        default:
            throw exception_io_data();
            break;
    }
}

// called after opening file (possibly per subsong too)
unsigned InputHandler::get_subsong_count() noexcept
{
    // if the plugin uses input_factory_t template and returns > 1 here when adding a song to the playlist,
    // foobar will automagically "unpack" it by calling decode_initialize/get_info with all subsong indexes.
    // There is no need to add any playlist code, only properly handle the subsong index.
    if (_DisableSubsongs)
        return 1;

    // vgmstream ready as method is valid after open() with any reason
    unsigned int SubsongCount = (unsigned int)_VGMStream->num_streams;

    if (SubsongCount == 0)
        SubsongCount = 1; // most formats don't have subsongs

    // pretend we don't have subsongs as we don't want foobar to unpack the rest
    if (_DirectSubsong)
        SubsongCount = 1;

    return SubsongCount;
}

// called after get_subsong_count to play subsong N (even when count is 1)
t_uint32 InputHandler::get_subsong(unsigned subsongIndex) noexcept
{
    return subsongIndex + 1; // translates index (0..N < subsong_count) for vgmstream: 1=first
}

// called before playing to get info
void InputHandler::get_info(t_uint32 subsongIndex, file_info & fileInfo, abort_callback & abortHandler)
{
    int length_in_ms = 0, channels = 0, samplerate = 0;
    int total_samples = -1;
    int bitrate = 0;
    int loop_flag = -1, loop_start = -1, loop_end = -1;
    pfc::string8 description;
    pfc::string8_fast temp;

    GetVGMSubsongInfo(subsongIndex, temp, &length_in_ms, &total_samples, &loop_flag, &loop_start, &loop_end, &samplerate, &channels, &bitrate, description, abortHandler);

    /* set tag info (metadata tab in file properties) */

    /* Shows a custom subsong title by default with subsong name, to simplify for average users.
     * This can be overriden and extended using the exported STREAM_x below and foobar's formatting.
     * foobar defaults to filename minus extension if there is no meta "title" value. */
    if (!_OverrideTitle)
        fileInfo.meta_set("TITLE", temp);

    if (GetDescriptionTag(temp, description, "stream count: "))
        fileInfo.meta_set("stream_count", temp);

    if (GetDescriptionTag(temp, description, "stream index: "))
        fileInfo.meta_set("stream_index", temp);

    if (GetDescriptionTag(temp, description, "stream name: "))
        fileInfo.meta_set("stream_name", temp);

    /* get external file tags */
    //todo optimize and don't parse tags again for this session (not sure how), seems foobar
    // calls get_info on every play even if the file hasn't changes, and won't refresh "meta"
    // unless forced or closing playlist+exe
    if (!_TagfileDisable)
    {
        //todo use foobar's fancy-but-arcane string functions
        char TagFilePath[PATH_LIMIT];

        ::strcpy_s(TagFilePath, sizeof(TagFilePath), _FilePath);

        char * EndOfPath = ::strrchr(TagFilePath, '\\');

        if (EndOfPath != nullptr)
        {
            EndOfPath[1] = '\0';  /* includes "\", remove after that from tagfile_path */

            ::strcat_s(TagFilePath, sizeof(TagFilePath), _TagFileName);
        }
        else
            ::strcpy_s(TagFilePath, sizeof(TagFilePath), _TagFileName);

        {
            STREAMFILE * TagsStream = ::open_foo_streamfile(TagFilePath, &abortHandler, NULL);

            if (TagsStream != nullptr)
            {
                const char * TagKey, * TagValue;

                VGMSTREAM_TAGS * Tags = ::vgmstream_tags_init(&TagKey, &TagValue);

                ::vgmstream_tags_reset(Tags, _FilePath);

                while (::vgmstream_tags_next_tag(Tags, TagsStream))
                {
                    if (replaygain_info::g_is_meta_replaygain(TagKey))
                    {
                        fileInfo.info_set_replaygain(TagKey, TagValue);
                        /* there is info_set_replaygain_auto too but no doc */
                    }
                    else
                    if (::stricmp_utf8("ALBUMARTIST", TagKey) == 0)
                        fileInfo.meta_set("ALBUM ARTIST", TagValue); // Normalize as foobar won't handle (though it's accepted in .ogg)
                    else
                        fileInfo.meta_set(TagKey, TagValue);
                }

                ::vgmstream_tags_close(Tags);

                ::close_streamfile(TagsStream);
            }
        }
    }

    /* set technical info (details tab in file properties) */
    fileInfo.info_set("vgmstream_version", STR_COMPONENT_VERSION);
    fileInfo.info_set_int("samplerate", samplerate);
    fileInfo.info_set_int("channels", channels);
    fileInfo.info_set_int("bitspersample", 16);

    /* not quite accurate but some people are confused by "lossless"
     * (could set lossless if PCM, but then again PCMFloat or PCM8 are converted/"lossy" in vgmstream) */
    fileInfo.info_set("encoding", "lossy/lossless");
    fileInfo.info_set_bitrate(bitrate / 1000);

    if (total_samples > 0)
        fileInfo.info_set_int("stream_total_samples", total_samples);

    if (loop_start >= 0 && loop_end > loop_start)
    {
        if (!loop_flag) fileInfo.info_set("looping", "disabled");
        fileInfo.info_set_int("loop_start", loop_start);
        fileInfo.info_set_int("loop_end", loop_end);
    }

    fileInfo.set_length(((double) length_in_ms) / 1000);

    if (GetDescriptionTag(temp, description, "encoding: ")) fileInfo.info_set("codec", temp);
    if (GetDescriptionTag(temp, description, "layout: ")) fileInfo.info_set("layout", temp);
    if (GetDescriptionTag(temp, description, "interleave: ", ' ')) fileInfo.info_set("interleave", temp);
    if (GetDescriptionTag(temp, description, "interleave last block:", ' ')) fileInfo.info_set("interleave_last_block", temp);

    if (GetDescriptionTag(temp, description, "block size: ")) fileInfo.info_set("block_size", temp);
    if (GetDescriptionTag(temp, description, "metadata from: ")) fileInfo.info_set("metadata_source", temp);
    if (GetDescriptionTag(temp, description, "stream count: ")) fileInfo.info_set("stream_count", temp);
    if (GetDescriptionTag(temp, description, "stream index: ")) fileInfo.info_set("stream_index", temp);
    if (GetDescriptionTag(temp, description, "stream name: ")) fileInfo.info_set("stream_name", temp);

    if (GetDescriptionTag(temp, description, "channel mask: ")) fileInfo.info_set("channel_mask", temp);
    if (GetDescriptionTag(temp, description, "output channels: ")) fileInfo.info_set("output_channels", temp);
    if (GetDescriptionTag(temp, description, "input channels: ")) fileInfo.info_set("input_channels", temp);
}

t_filestats2 InputHandler::get_stats2(uint32_t, abort_callback &) noexcept
{
    return _Stats2;
}

// called right before actually playing (decoding) a song/subsong
void InputHandler::decode_initialize(t_uint32 p_subsong, unsigned p_flags, abort_callback & p_abort)
{
    // if subsong changes recreate vgmstream
    if (_SubsongIndex != p_subsong && !_DirectSubsong)
    {
        _SubsongIndex = p_subsong;
        InitializeVGMStream(p_abort);
    }

    // "don't loop forever" flag (set when converting to file, scanning for replaygain, etc)
    // flag is set *after* loading vgmstream + applying config so manually disable
    const bool ForceIgnoreLoop = !!(p_flags & input_flag_no_looping);

    if (ForceIgnoreLoop) // could always set but vgmstream is re-created on play start
        ::vgmstream_set_play_forever(_VGMStream, 0);

    decode_seek(0, p_abort);
};

// called when audio buffer needs to be filled
bool InputHandler::decode_run(audio_chunk & audioChunk, abort_callback &)
{
    if (!_IsDecoding || (_VGMStream == nullptr))
        return false;

    size_t SamplesToDo = SAMPLE_BUFFER_SIZE;

    {
        const bool ShouldPlayForever = (::vgmstream_get_play_forever(_VGMStream) != 0);

        if ((_DecodePositionInSamples + SAMPLE_BUFFER_SIZE) > _LengthInSamples && !ShouldPlayForever)
            SamplesToDo = _LengthInSamples - (size_t)_DecodePositionInSamples;
        else
            SamplesToDo = SAMPLE_BUFFER_SIZE;

        if (SamplesToDo == 0)
        {
            _IsDecoding = false;

            return false;
        }

        ::render_vgmstream(_SampleBuffer, (int32_t)SamplesToDo, _VGMStream);
    }

    {
        unsigned int ChannelConfig = _VGMStream->channel_layout;

        if (ChannelConfig == 0)
            ChannelConfig = audio_chunk::g_guess_channel_config(_OutputChannelCount);

        const t_size ByteCount = (SamplesToDo * _OutputChannelCount * sizeof(_SampleBuffer[0]));

        audioChunk.set_data_fixedpoint((char *) _SampleBuffer, ByteCount, (unsigned int)_VGMStream->sample_rate, _OutputChannelCount, 16, ChannelConfig);

        _DecodePositionInSamples += SamplesToDo;
        _DecodePositionInMs = _DecodePositionInSamples * (size_t)1000 / (size_t)_VGMStream->sample_rate;
    }

    return true;
}

// called when seeking
void InputHandler::decode_seek(double timeInSeconds, abort_callback &)
{
    uint64_t SeekPositionInSamples = audio_math::time_to_samples(timeInSeconds, (uint32_t)_VGMStream->sample_rate);

    const bool ShouldPlayForever = (::vgmstream_get_play_forever(_VGMStream) != 0);

    // possible when disabling looping without refreshing foobar's cached song length (p_seconds can't go over seek bar with infinite looping on, though)
    if (SeekPositionInSamples > _LengthInSamples)
        SeekPositionInSamples = _LengthInSamples;

    ::seek_vgmstream(_VGMStream, (int32_t)SeekPositionInSamples);

    _DecodePositionInSamples = SeekPositionInSamples;
    _DecodePositionInMs = _DecodePositionInSamples * 1000LL / _VGMStream->sample_rate;

    _IsDecoding = ShouldPlayForever || _DecodePositionInSamples < _LengthInSamples;
}

bool InputHandler::decode_can_seek() noexcept
{
    return true;
}

#pragma region("")
void InputHandler::retag_set_info(t_uint32, const file_info &, abort_callback &) noexcept
{
}

void InputHandler::retag_commit(abort_callback &) noexcept
{
}
#pragma endregion

#pragma region("Interface input_entry")
/// <summary>
/// Determines whether specified content type can be handled by this input.
/// </summary>
bool InputHandler::g_is_our_content_type(const char *) noexcept
{
    return false;
}

/// <summary>
/// Determines whether specified file type can be handled by this input. This must not use any kind of file access; the result should be only based on file path / extension.
/// </summary>
bool InputHandler::g_is_our_path(const char *, const char * extension)
{
    vgmstream_ctx_valid_cfg cfg = { 0 };

    cfg.is_extension = 1;

    LoadConfig(&cfg.accept_unknown, &cfg.accept_common);

    return ::vgmstream_ctx_is_valid(extension, &cfg) > 0 ? true : false;
}
#pragma endregion

#pragma region("Interface input_entry_v2")
/// <summary>
/// Returns a GUID used to identify us among other decoders in the decoder priority table.
/// </summary>
GUID InputHandler::g_get_guid() noexcept
{
    static const GUID guid = { 0x9e7263c7, 0x4cdd, 0x482c,{ 0x9a, 0xec, 0x5e, 0x71, 0x28, 0xcb, 0xc3, 0x4 } };

    return guid;
}

/// <summary>
/// Returns Name to present to the user in the decoder priority table.
/// </summary>
const char * InputHandler::g_get_name() noexcept
{
    return STR_COMPONENT_NAME;
}

/// <summary>
/// Returns a GUID of this decoder's preferences page (optional), null guid if there's no page to present.
/// </summary>
GUID InputHandler::g_get_preferences_guid() noexcept
{
    static const GUID guid = { 0x2b5d0302, 0x165b, 0x409c,{ 0x94, 0x74, 0x2c, 0x8c, 0x2c, 0xd7, 0x6a, 0x25 } };

    return guid;
}

/// <summary>
/// Returns true if the decoder should be put at the end of the list when it's first sighted, false otherwise (will be put at the beginning of the list).
/// </summary>
bool InputHandler::g_is_low_merit() noexcept
{
    return true;
}
#pragma endregion

#pragma endregion

#pragma region("Interface input_impl_interface_wrapper_t")
void InputHandler::remove_tags(abort_callback &) noexcept
{
}
#pragma endregion

#pragma region("Interface InputHandler")
/// <summary>
/// Initializes the VGMStream.
/// </summary>
/// <param name="abortHandler"></param>
void InputHandler::InitializeVGMStream(abort_callback & abortHandler)
{
    // close first in case of changing subsongs
    if (_VGMStream)
        ::close_vgmstream(_VGMStream);

    _VGMStream = CreateVGMStream(_SubsongIndex, _FilePath, abortHandler);

    if (_VGMStream == nullptr)
        throw exception_io_data();

    // default subsong is 0, meaning first init (vgmstream should open first stream, but not set stream_index).
    // if the stream_index is already set, then the subsong was opened directly by some means (txtp, playlist, etc).
    // add flag as in that case we only want to play the subsong but not unpack subsongs (which foobar does by default).
    if (_SubsongIndex == 0 && _VGMStream->stream_index > 0)
    {
        _SubsongIndex = (unsigned)_VGMStream->stream_index;
        _DirectSubsong = true;
    }

    if (_SubsongIndex == 0)
        _SubsongIndex = 1;

    ApplyVGMConfig(_VGMStream);

    /* enable after all config but before outbuf (though ATM outbuf is not dynamic so no need to read input_channels) */
    ::vgmstream_mixing_autodownmix(_VGMStream, _DownmixChannelCount);
    ::vgmstream_mixing_enable(_VGMStream, SAMPLE_BUFFER_SIZE, nullptr, (int *)&_OutputChannelCount);

    _LengthInSamples = (size_t)::vgmstream_get_samples(_VGMStream);

    _DecodePositionInSamples = 0;
    _DecodePositionInMs = 0;
}

/// <summary>
/// Creates a VGMStream.
/// </summary>
/// <param name="subsongIndex"></param>
/// <param name="filename"></param>
/// <param name="abortHandler"></param>
/// <returns></returns>
VGMSTREAM * InputHandler::CreateVGMStream(t_uint32 subsongIndex, const char * const filePath, abort_callback & abortHandler)
{
    VGMSTREAM * VGMStream = nullptr;

    STREAMFILE * sf = ::open_foo_streamfile(filePath, &abortHandler, &_Stats);

    if (sf)
    {
        sf->stream_index = (int)subsongIndex;

        VGMStream = ::init_vgmstream_from_STREAMFILE(sf);

        ::close_streamfile(sf);
    }

    return VGMStream;
}

void InputHandler::ApplyVGMConfig(VGMSTREAM * vgmstream) noexcept
{
    vgmstream_cfg_t vcfg = { 0 };

    vcfg.allow_play_forever = 1;
    vcfg.play_forever = _LoopForever;
    vcfg.loop_count = _LoopCount;
    vcfg.fade_time = _FadeLength;
    vcfg.fade_delay = _FadeDelay;
    vcfg.ignore_loop = (int)_IgnoreLoop;

    ::vgmstream_apply_config(vgmstream, &vcfg);
}

void InputHandler::GetVGMSubsongInfo(t_uint32 subsongIndex, pfc::string_base & title, int * lengthInMs, int * lengthInSamples, int * isStreamLooped, int * firstLoopSample, int * lastLoopSample, int * sampleRate, int * channelCount, int * bitRate, pfc::string_base & description, abort_callback & p_abort)
{
    VGMSTREAM * InfoStream = nullptr;
    bool IsInfoStream = false;
    unsigned int InfoChannelCount = 0;

    // reuse current vgmstream if not querying a new subsong
    // if it's a direct subsong then subsong may be N while p_subsong 1
    // there is no need to recreate the infostream, there is only one subsong used
    if (_SubsongIndex != subsongIndex && !_DirectSubsong)
    {
        InfoStream = CreateVGMStream(subsongIndex, _FilePath, p_abort);

        if (InfoStream == nullptr)
            throw exception_io_data();

        IsInfoStream = true;

        ApplyVGMConfig(InfoStream);

        ::vgmstream_mixing_autodownmix(InfoStream, _DownmixChannelCount);
        ::vgmstream_mixing_enable(InfoStream, 0, nullptr, (int *)&InfoChannelCount);
    }
    else
    {
        // vgmstream ready as get_info is valid after open() with any reason
        InfoStream = _VGMStream;
        InfoChannelCount = _OutputChannelCount;
    }

    if (lengthInMs)
    {
        *lengthInMs = -1000;

        if (InfoStream)
        {
            if (lengthInSamples) *lengthInSamples = InfoStream->num_samples;

            if (isStreamLooped) *isStreamLooped = InfoStream->loop_flag;
            if (firstLoopSample) *firstLoopSample = InfoStream->loop_start_sample;
            if (lastLoopSample) *lastLoopSample = InfoStream->loop_end_sample;

            if (sampleRate) *sampleRate = InfoStream->sample_rate;
            if (channelCount) *channelCount = (int)InfoChannelCount;
            if (bitRate) *bitRate = ::get_vgmstream_average_bitrate(InfoStream);

            int SampleCount = ::vgmstream_get_samples(InfoStream);

            *lengthInMs = SampleCount * 1000LL / InfoStream->sample_rate;

            char Description[1024];

            ::describe_vgmstream(InfoStream, Description, sizeof(Description));

            description = Description;
        }
    }

    /* infostream gets added with index 0 (other) or 1 (current) */
    if (InfoStream && title)
    {
        char Title[1024] = { 0 };

        const char * FilePath = _FilePath;

        vgmstream_title_t TitleConfig = { 0 };
        
        TitleConfig.remove_extension = 1;
        TitleConfig.remove_archive = 1;

        ::vgmstream_get_title(Title, sizeof(Title), FilePath, InfoStream, &TitleConfig);

        title = Title;
    }

    // and only close if was querying a new subsong
    if (IsInfoStream)
    {
        close_vgmstream(InfoStream);
        InfoStream = nullptr;
    }
}

bool InputHandler::GetDescriptionTag(pfc::string_base & temp, pfc::string_base const & description, const char * tag, char delimiter)
{
    t_size pos = description.find_first(tag);

    if (pos == pfc::infinite_size)
        return false;

    pos += ::strlen(tag);

    t_size eos = description.find_first(delimiter, pos);

    if (eos == pfc::infinite_size)
        eos = description.length();

    temp.set_string(description + pos, eos - pos);

    return true;
}

static input_factory_t<InputHandler> _InputHandler;
#pragma endregion

#pragma warning(push)
#pragma warning(disable: 4265 4625 4626 5026 5027 26433 26436 26455)
DECLARE_COMPONENT_VERSION(STR_COMPONENT_NAME, STR_COMPONENT_VERSION,
    STR_COMPONENT_FILENAME " " STR_COMPONENT_VERSION "\n"
    "\n"
    "Adds a Preview playback mode.\n"
    "\n"
    "Build with foobar2000 SDK " TOSTRING(FOOBAR2000_SDK_VERSION) ".\n"
    "Build: " __TIME__ ", " __DATE__
    "\n"
    "Using vgmstream " VGMSTREAM_VERSION
    "by hcs, FastElbja, manakoAT, bxaimc, snakemeat, soneek, kode54, bnnm, Nicknine, Thealexbarney, CyberBotX, and many others\n"
    "\n"
    "Original foobar2000 plugin by Josh W, kode54, others\n"
    "\n"
    "https://github.com/vgmstream/vgmstream/\n"
    "https://sourceforge.net/projects/vgmstream/ (original)"
);

VALIDATE_COMPONENT_FILENAME(STR_COMPONENT_FILENAME);
#pragma warning(pop)
