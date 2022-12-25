#include <stdio.h>
#include <io.h>

extern "C"
{
#include <src/vgmstream.h>
#include <src/util.h>
}

#include "foo_input_vgmstream.h"
#include "Preferences.h"

static cfg_string   cfg_LoopCount({ 0xfc8dfd72,0xfae8,0x44cc,{0xbe,0x99,0x1c,0x7b,0x27,0x7a,0xb6,0xb9} }, DEFAULT_LOOP_COUNT);
static cfg_bool     cfg_LoopForever({ 0xa19e36eb,0x72a0,0x4077,{0x91,0x43,0x38,0xb4,0x05,0xfc,0x91,0xc5} }, DEFAULT_LOOP_FOREVER);
static cfg_bool     cfg_IgnoreLoop({ 0xddda7ab6,0x7bb6,0x4abe,{0xb9,0x66,0x2d,0xb7,0x8f,0xe4,0xcc,0xab} }, DEFAULT_IGNORE_LOOP);

static cfg_string   cfg_FadeLength({ 0x61da7ef1,0x56a5,0x4368,{0xae,0x06,0xec,0x6f,0xd7,0xe6,0x15,0x5d} }, DEFAULT_FADE_SECONDS);
static cfg_string   cfg_FadeDelay({ 0x73907787,0xaf49,0x4659,{0x96,0x8e,0x9f,0x70,0xa1,0x62,0x49,0xc4} }, DEFAULT_FADE_DELAY_SECONDS);

static cfg_bool     cfg_DisableSubsongs({ 0xa8cdd664,0xb32b,0x4a36,{0x83,0x07,0xa0,0x4c,0xcd,0x52,0xa3,0x7c} }, DEFAULT_DISABLE_SUBSONGS);
static cfg_string   cfg_DownmixChannels({ 0x5a0e65dd,0xeb37,0x4c67,{0x9a,0xb1,0x3f,0xb0,0xc9,0x7e,0xb0,0xe0} }, DEFAULT_DOWNMIX_CHANNELS);
static cfg_bool     cfg_TagfileDisable({ 0xc1971eb7,0xa930,0x4bae,{0x9e,0x7f,0xa9,0x50,0x36,0x32,0x41,0xb3} }, DEFAULT_TAGFILE_DISABLE);
static cfg_bool     cfg_OverrideTitle({ 0xe794831f,0xd067,0x4337,{0x97,0x85,0x10,0x57,0x39,0x4b,0x1b,0x97} }, DEFAULT_OVERRIDE_TITLE);

static cfg_bool     cfg_AcceptUnknownExtensions({ 0xd92dc6a2,0x9683,0x422d,{0x8e,0xd1,0x59,0x46,0xd5,0xbf,0x01,0x6f} }, DEFAULT_ACCEPT_UNKNOWN_EXTENSIONS);
static cfg_bool     cfg_AcceptCommonExtensions({ 0x405af423,0x5037,0x4eae,{0xa6,0xe3,0x72,0xd0,0x12,0x7d,0x84,0x6c} }, DEFAULT_ACCEPT_COMMON_EXTENSIONS);

#pragma region("InputHandler")
/// <summary>
/// Loads the settings.
/// </summary>
void InputHandler::LoadSettings()
{
    std::ignore = sscanf_s(cfg_LoopCount.get_ptr(), "%lf", &_LoopCount);

    // Exact 0 was allowed before (AKA "intro only") but confuses people and may result in unplayable files.
    if (_LoopCount <= 0)
        _LoopCount = 1;

    std::ignore = sscanf_s(cfg_FadeLength.get_ptr(), "%lf", &_FadeLength);
    std::ignore = sscanf_s(cfg_FadeDelay.get_ptr(), "%lf", &_FadeDelay);

    _LoopForever = cfg_LoopForever;
    _IgnoreLoop = cfg_IgnoreLoop;

    _DisableSubsongs = cfg_DisableSubsongs;

    std::ignore = sscanf_s(cfg_DownmixChannels.get_ptr(), "%d", &_DownmixChannelCount);

    _TagfileDisable = cfg_TagfileDisable;
    _OverrideTitle = cfg_OverrideTitle;
}

void InputHandler::LoadConfig(int * acceptUnknownExtensions, int * acceptCommonExtensions)
{
    if ((acceptUnknownExtensions == nullptr) || (acceptCommonExtensions == nullptr))
        return;

    *acceptUnknownExtensions = cfg_AcceptUnknownExtensions ? 1 : 0;
    *acceptCommonExtensions = cfg_AcceptCommonExtensions ? 1 : 0;
}
#pragma endregion

#pragma region("Preferences")
t_uint32 PreferencesDialog::get_state()
{
    t_uint32 State = preferences_state::resettable | preferences_state::dark_mode_supported;

    if (HasChanged())
        State |= preferences_state::changed;

    return State;
}

void PreferencesDialog::reset() noexcept
{
    ::uSetDlgItemText(m_hWnd, IDC_LOOP_COUNT, DEFAULT_LOOP_COUNT);
    ::uSetDlgItemText(m_hWnd, IDC_FADE_SECONDS, DEFAULT_FADE_SECONDS);
    ::uSetDlgItemText(m_hWnd, IDC_FADE_DELAY_SECONDS, DEFAULT_FADE_DELAY_SECONDS);

    CheckDlgButton(IDC_LOOP_NORMALLY, (!DEFAULT_IGNORE_LOOP && !DEFAULT_LOOP_FOREVER) ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(IDC_LOOP_FOREVER, DEFAULT_LOOP_FOREVER ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(IDC_IGNORE_LOOP, DEFAULT_IGNORE_LOOP ? BST_CHECKED : BST_UNCHECKED);

    CheckDlgButton(IDC_DISABLE_SUBSONGS, DEFAULT_DISABLE_SUBSONGS ? BST_CHECKED : BST_UNCHECKED);

    ::uSetDlgItemText(m_hWnd, IDC_DOWNMIX_CHANNELS, DEFAULT_DOWNMIX_CHANNELS);

    CheckDlgButton(IDC_TAGFILE_DISABLE, DEFAULT_TAGFILE_DISABLE ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(IDC_OVERRIDE_TITLE, DEFAULT_OVERRIDE_TITLE ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(IDC_EXTS_UNKNOWN_ON, DEFAULT_ACCEPT_UNKNOWN_EXTENSIONS ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(IDC_EXTS_COMMON_ON, DEFAULT_ACCEPT_COMMON_EXTENSIONS ? BST_CHECKED : BST_UNCHECKED);
}

void PreferencesDialog::apply()
{
    int CharsConsumed;

    pfc::string Text;

    {
        Text = ::uGetDlgItemText(m_hWnd, IDC_FADE_SECONDS);

        double FadeLengthInSeconds;

        if ((::sscanf_s(Text.get_ptr(), "%lf%n", &FadeLengthInSeconds, &CharsConsumed) < 1) || (CharsConsumed != (int)::strlen(Text.get_ptr())) || (FadeLengthInSeconds < 0.0))
        {
            ::uMessageBox(m_hWnd,
                "Invalid value for Fade Length\n"
                "Must be a number greater than or equal to zero",
                "Error", MB_OK | MB_ICONERROR);
            return;
        }
        else
            cfg_FadeLength = Text.get_ptr();
    }

    {
        Text = ::uGetDlgItemText(m_hWnd, IDC_FADE_DELAY_SECONDS);

        double FadeDelayInSeconds;

        if (::sscanf_s(Text.get_ptr(), "%lf%n", &FadeDelayInSeconds, &CharsConsumed) < 1 || CharsConsumed != (int)::strlen(Text.get_ptr()) || FadeDelayInSeconds < 0.0)
        {
            ::uMessageBox(m_hWnd,
                "Invalid value for Fade Delay\n"
                "Must be a number",
                "Error", MB_OK | MB_ICONERROR);
            return;
        }
        else
            cfg_FadeDelay = Text.get_ptr();
    }

    {
        Text = ::uGetDlgItemText(m_hWnd, IDC_LOOP_COUNT);

        double LoopCount;

        if (::sscanf_s(Text.get_ptr(), "%lf%n", &LoopCount, &CharsConsumed) < 1 || CharsConsumed != (int)::strlen(Text.get_ptr()) || LoopCount < 0.0)
        {
            ::uMessageBox(m_hWnd,
                "Invalid value for Loop Count\n"
                "Must be a number greater than or equal to zero",
                "Error", MB_OK | MB_ICONERROR);
            return;
        }
        else
            cfg_LoopCount = Text.get_ptr();
    }

    {
        Text = ::uGetDlgItemText(m_hWnd, IDC_DOWNMIX_CHANNELS);

        int DownmixChannelCount;

        if (::sscanf_s(Text.get_ptr(), "%d%n", &DownmixChannelCount, &CharsConsumed) < 1 || CharsConsumed != (int)::strlen(Text.get_ptr()) || DownmixChannelCount < 0)
        {
            ::uMessageBox(m_hWnd,
                "Invalid value for Downmix Channels\n"
                "Must be a number greater than or equal to zero",
                "Error", MB_OK | MB_ICONERROR);
            return;
        }
        else
            cfg_DownmixChannels = Text.get_ptr();
    }

    cfg_IgnoreLoop = IsDlgButtonChecked(IDC_IGNORE_LOOP) ? true : false;
    cfg_LoopForever = IsDlgButtonChecked(IDC_LOOP_FOREVER) ? true : false;

    cfg_DisableSubsongs = IsDlgButtonChecked(IDC_DISABLE_SUBSONGS) ? true : false;

    cfg_TagfileDisable = IsDlgButtonChecked(IDC_TAGFILE_DISABLE) ? true : false;
    cfg_OverrideTitle = IsDlgButtonChecked(IDC_OVERRIDE_TITLE) ? true : false;
    cfg_AcceptUnknownExtensions = IsDlgButtonChecked(IDC_EXTS_UNKNOWN_ON) ? true : false;
    cfg_AcceptCommonExtensions = IsDlgButtonChecked(IDC_EXTS_COMMON_ON) ? true : false;
}

BOOL PreferencesDialog::OnInitDialog(CWindow, LPARAM)
{
    ::uSetDlgItemText(m_hWnd, IDC_LOOP_COUNT, cfg_LoopCount);
    ::uSetDlgItemText(m_hWnd, IDC_FADE_SECONDS, cfg_FadeLength);
    ::uSetDlgItemText(m_hWnd, IDC_FADE_DELAY_SECONDS, cfg_FadeDelay);

    CheckDlgButton(IDC_LOOP_NORMALLY, (UINT)((!cfg_IgnoreLoop && !cfg_LoopForever) ? BST_CHECKED : BST_UNCHECKED));
    CheckDlgButton(IDC_LOOP_FOREVER, (UINT)(cfg_LoopForever ? BST_CHECKED : BST_UNCHECKED));
    CheckDlgButton(IDC_IGNORE_LOOP, (UINT)(cfg_IgnoreLoop ? BST_CHECKED : BST_UNCHECKED));

    CheckDlgButton(IDC_DISABLE_SUBSONGS, (UINT)(cfg_DisableSubsongs ? BST_CHECKED : BST_UNCHECKED));

    ::uSetDlgItemText(m_hWnd, IDC_DOWNMIX_CHANNELS, cfg_DownmixChannels);

    CheckDlgButton(IDC_TAGFILE_DISABLE, (UINT)(cfg_TagfileDisable ? BST_CHECKED : BST_UNCHECKED));
    CheckDlgButton(IDC_OVERRIDE_TITLE, (UINT)(cfg_OverrideTitle ? BST_CHECKED : BST_UNCHECKED));
    CheckDlgButton(IDC_EXTS_UNKNOWN_ON, (UINT)(cfg_AcceptUnknownExtensions ? BST_CHECKED : BST_UNCHECKED));
    CheckDlgButton(IDC_EXTS_COMMON_ON, (UINT)(cfg_AcceptCommonExtensions ? BST_CHECKED : BST_UNCHECKED));

    _DarkModeHooks.AddDialogWithControls(*this);

    return TRUE;
}

void PreferencesDialog::OnEditChange(UINT, int, CWindow)
{
    _PageCallback->on_state_changed();
}

bool PreferencesDialog::HasChanged()
{
    {
        pfc::string LoopCount(cfg_LoopCount);

        if (LoopCount != ::uGetDlgItemText(m_hWnd, IDC_LOOP_COUNT)) return true;
    }

    {
        pfc::string FadeLength(cfg_FadeLength);

        if (FadeLength != ::uGetDlgItemText(m_hWnd, IDC_FADE_SECONDS)) return true;
    }

    {
        pfc::string FadeDelay(cfg_FadeDelay);

        if (FadeDelay != ::uGetDlgItemText(m_hWnd, IDC_FADE_DELAY_SECONDS)) return true;
    }

    {
        if (IsDlgButtonChecked(IDC_LOOP_NORMALLY) && (!cfg_IgnoreLoop || !cfg_LoopForever))
            return true;

        if (IsDlgButtonChecked(IDC_LOOP_FOREVER) && !cfg_LoopForever)
            return true;

        if (IsDlgButtonChecked(IDC_IGNORE_LOOP) && !cfg_IgnoreLoop)
            return true;
    }

    {
        bool IsChecked = (IsDlgButtonChecked(IDC_DISABLE_SUBSONGS) != 0);

        if (cfg_DisableSubsongs != IsChecked)
            return true;
    }

    {
        pfc::string DownmixChannels(cfg_DownmixChannels);

        if (DownmixChannels != ::uGetDlgItemText(m_hWnd, IDC_DOWNMIX_CHANNELS))
            return true;
    }

    {
        bool IsChecked = (IsDlgButtonChecked(IDC_TAGFILE_DISABLE) != 0);

        if (cfg_TagfileDisable != IsChecked)
            return true;
    }

    {
        bool IsChecked = (IsDlgButtonChecked(IDC_OVERRIDE_TITLE) != 0);

        if (cfg_OverrideTitle != IsChecked)
            return true;
    }

    {
        bool IsChecked = (IsDlgButtonChecked(IDC_EXTS_UNKNOWN_ON) != 0);

        if (cfg_AcceptUnknownExtensions != IsChecked)
            return true;
    }

    {
        bool IsChecked = (IsDlgButtonChecked(IDC_EXTS_COMMON_ON) != 0);

        if (cfg_AcceptCommonExtensions != IsChecked)
            return true;
    }

    return FALSE;
}
#pragma endregion

#pragma region("Preferences Page")
const char * PreferencesPage::get_name() noexcept
{
    return InputHandler::g_get_name();
}

GUID PreferencesPage::get_guid() noexcept
{
    return InputHandler::g_get_preferences_guid();
}

GUID PreferencesPage::get_parent_guid() noexcept
{
    return guid_input;
}

static preferences_page_factory_t<PreferencesPage> _PreferencesPage;
#pragma endregion
