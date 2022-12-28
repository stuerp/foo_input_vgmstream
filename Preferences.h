
/** $VER: Preferences.h (2022.12.28) P. Stuer **/

#pragma once

#include <foobar2000/helpers/foobar2000+atl.h>
#include <foobar2000/helpers/atl-misc.h>

#include <coreDarkMode.h>

#include "Resources.h"

#define DEFAULT_LOOP_COUNT "2.00"
#define DEFAULT_FADE_SECONDS "10.00"
#define DEFAULT_FADE_DELAY_SECONDS "0.00"

#define DEFAULT_LOOP_FOREVER false
#define DEFAULT_IGNORE_LOOP false

#define DEFAULT_DISABLE_SUBSONGS false
#define DEFAULT_DOWNMIX_CHANNELS "8"

#define DEFAULT_TAGFILE_DISABLE false
#define DEFAULT_OVERRIDE_TITLE false
#define DEFAULT_ACCEPT_UNKNOWN_EXTENSIONS false
#define DEFAULT_ACCEPT_COMMON_EXTENSIONS false

class PreferencesDialog : public CDialogImpl<PreferencesDialog>, public preferences_page_instance
{
public:
    PreferencesDialog(preferences_page_callback::ptr callback) : _PageCallback(callback) { }
    PreferencesDialog(const PreferencesDialog&) = delete;
    PreferencesDialog(const PreferencesDialog&&) = delete;
    PreferencesDialog& operator=(const PreferencesDialog&) = delete;
    PreferencesDialog& operator=(PreferencesDialog&&) = delete;
    virtual ~PreferencesDialog() { };

    enum
    {
        IDD = IDD_CONFIG
    };

    t_uint32 get_state() final;
    void apply() final;
    void reset() noexcept final;

#pragma warning(push)
#pragma warning(disable: 26433 26496)
    BEGIN_MSG_MAP(UsfPreferences)
        MSG_WM_INITDIALOG(OnInitDialog)
        COMMAND_HANDLER_EX(IDC_IGNORE_LOOP, BN_CLICKED, OnEditChange)
        COMMAND_HANDLER_EX(IDC_LOOP_FOREVER, BN_CLICKED, OnEditChange)
        COMMAND_HANDLER_EX(IDC_LOOP_NORMALLY, BN_CLICKED, OnEditChange)
        COMMAND_HANDLER_EX(IDC_FADE_SECONDS, EN_CHANGE, OnEditChange)
        COMMAND_HANDLER_EX(IDC_FADE_DELAY_SECONDS, EN_CHANGE, OnEditChange)
        COMMAND_HANDLER_EX(IDC_LOOP_COUNT, EN_CHANGE, OnEditChange)
        COMMAND_HANDLER_EX(IDC_DISABLE_SUBSONGS, BN_CLICKED, OnEditChange)
        COMMAND_HANDLER_EX(IDC_DOWNMIX_CHANNELS, EN_CHANGE, OnEditChange)
        COMMAND_HANDLER_EX(IDC_TAGFILE_DISABLE, BN_CLICKED, OnEditChange)
        COMMAND_HANDLER_EX(IDC_OVERRIDE_TITLE, BN_CLICKED, OnEditChange)
        COMMAND_HANDLER_EX(IDC_EXTS_UNKNOWN_ON, BN_CLICKED, OnEditChange)
        COMMAND_HANDLER_EX(IDC_EXTS_COMMON_ON, BN_CLICKED, OnEditChange)
    END_MSG_MAP()
#pragma warning(pop)

private:
    BOOL OnInitDialog(CWindow, LPARAM);
    void OnEditChange(UINT, int, CWindow);
    bool HasChanged();

    const preferences_page_callback::ptr _PageCallback;
    fb2k::CCoreDarkModeHooks _DarkModeHooks;
};

class PreferencesPage : public preferences_page_impl<PreferencesDialog>
{
public:
    PreferencesPage() noexcept { };
    PreferencesPage(const PreferencesPage&) = delete;
    PreferencesPage(const PreferencesPage&&) = delete;
    PreferencesPage& operator=(const PreferencesPage&) = delete;
    PreferencesPage& operator=(PreferencesPage&&) = delete;
    virtual ~PreferencesPage() { }

public:
    const char * get_name() noexcept final;
    GUID get_guid() noexcept final;
    GUID get_parent_guid() noexcept final;
};
