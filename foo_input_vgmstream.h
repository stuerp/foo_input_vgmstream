#pragma once

#include <foobar2000/SDK/foobar2000.h>

extern "C"
{
#include <src/vgmstream.h>
}

#pragma region("InputHandler")
constexpr size_t SAMPLE_BUFFER_SIZE = 1024;

class InputHandler : public input_stubs
{
public:
    InputHandler();
    InputHandler(const InputHandler&) = delete;
    InputHandler(const InputHandler&&) = delete;
    InputHandler& operator=(const InputHandler&) = delete;
    InputHandler& operator=(InputHandler&&) = delete;
    virtual ~InputHandler()
    {
        ::close_vgmstream(_VGMStream);
        _VGMStream = nullptr;
    }

    #pragma region("Interface input_impl")
    void open(service_ptr_t<file> file, const char * filePath, t_input_open_reason reason, abort_callback & abortHandler);

    unsigned get_subsong_count() noexcept;
    t_uint32 get_subsong(unsigned subsongIndex) noexcept;
    void get_info(t_uint32 p_subsong,file_info & p_info,abort_callback & p_abort);
    t_filestats2 get_stats2(uint32_t, abort_callback & p_abort) noexcept;

    void decode_initialize(t_uint32 p_subsong,unsigned p_flags,abort_callback & p_abort);
    bool decode_run(audio_chunk & p_chunk,abort_callback & p_abort);
    void decode_seek(double p_seconds,abort_callback & p_abort);
    bool decode_can_seek() noexcept;

    void retag_set_info(t_uint32 p_subsong,const file_info & p_info,abort_callback & abortHandler) noexcept;
    void retag_commit(abort_callback & p_abort) noexcept;
	
    static bool g_is_our_content_type(const char * p_content_type) noexcept;
    static bool g_is_our_path(const char * p_path,const char * p_extension);

    static GUID g_get_guid() noexcept;
    static const char * g_get_name() noexcept;
    static GUID g_get_preferences_guid() noexcept;
    static bool g_is_low_merit() noexcept;
    #pragma endregion

    #pragma region("Interface input_impl_interface_wrapper_t")
    void remove_tags(abort_callback & abortHandler) noexcept;
    #pragma endregion

private:
    void LoadSettings();
    static void LoadConfig(int * accept_unknown, int * accept_common);

    void InitializeVGMStream(abort_callback & abortHandler);
    VGMSTREAM * CreateVGMStream(t_uint32 subsongIndex, const char * const filePath, abort_callback & abortHandler);
    void GetVGMSubsongInfo(t_uint32 p_subsong, pfc::string_base & title, int * length_in_ms, int * total_samples, int * loop_flag, int * loop_start, int * loop_end, int * sample_rate, int * channels, int * bitrate, pfc::string_base & description, abort_callback & p_abort);
    bool GetDescriptionTag(pfc::string_base & temp, pfc::string_base const & description, const char * tag, char delimiter = '\n');
    void ApplyVGMConfig(VGMSTREAM * vgmstream) noexcept;

private:
    pfc::string8 _FilePath;
    t_filestats _Stats;
    t_filestats2 _Stats2;

    pfc::string8 _TagFileName;

    VGMSTREAM * _VGMStream;
    t_uint32 _SubsongIndex;
    bool _DirectSubsong;
    unsigned int _OutputChannelCount;

    int16_t _SampleBuffer[SAMPLE_BUFFER_SIZE * VGMSTREAM_MAX_CHANNELS];
    bool _IsDecoding;
    size_t _LengthInSamples;
    size_t _DecodePositionInSamples;
    size_t _DecodePositionInMs;

    // Settings
    double _LoopCount;
    double _FadeLength; // in seconds
    double _FadeDelay; // in seconds
    bool _LoopForever;
    bool _IgnoreLoop;
    bool _DisableSubsongs;
    int _DownmixChannelCount;
    bool _TagfileDisable;
    bool _OverrideTitle;
};
#pragma endregion
