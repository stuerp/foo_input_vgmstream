
/** $VER: Resources.h (2022.12.28) P. Stuer **/

#pragma once

#define TOSTRING_IMPL(x) #x
#define TOSTRING(x) TOSTRING_IMPL(x)

/** Component specific **/

#define STR_COMPONENT_NAME      "VGMStream Player"
#define STR_COMPONENT_VERSION   TOSTRING(NUM_PRODUCT_MAJOR) "." TOSTRING(NUM_PRODUCT_MINOR) "." TOSTRING(NUM_PRODUCT_PATCH) "." TOSTRING(NUM_PRODUCT_PRERELEASE)
#define STR_COMPONENT_FILENAME  "foo_input_vgmstream.dll"

/** Generic **/

#define STR_COMPANY_NAME        TEXT("")
#define STR_INTERNAL_NAME       TEXT(STR_COMPONENT_NAME)
#define STR_COMMENTS            TEXT("Written by Christopher Snowhill, Peter Stuer")
#define STR_COPYRIGHT           TEXT("Copyright (c) 2012-2022. All rights reserved.")

#define NUM_FILE_MAJOR          1
#define NUM_FILE_MINOR          0
#define NUM_FILE_PATCH          0
#define NUM_FILE_PRERELEASE     1

#define STR_FILE_NAME           TEXT(STR_COMPONENT_FILENAME)
#define STR_FILE_VERSION        TOSTRING(NUM_FILE_MAJOR) TEXT(".") TOSTRING(NUM_FILE_MINOR) TEXT(".") TOSTRING(NUM_FILE_PATCH) TEXT(".") TOSTRING(NUM_FILE_PRERELEASE)
#define STR_FILE_DESCRIPTION    TEXT("A foobar2000 component that implements playback of streamed (prerecorded) video game audio")

#define NUM_PRODUCT_MAJOR       1
#define NUM_PRODUCT_MINOR       0
#define NUM_PRODUCT_PATCH       0
#define NUM_PRODUCT_PRERELEASE  0

#define STR_PRODUCT_NAME        STR_COMPANY_NAME TEXT(" ") STR_INTERNAL_NAME
#define STR_PRODUCT_VERSION     TOSTRING(NUM_PRODUCT_MAJOR) TEXT(".") TOSTRING(NUM_PRODUCT_MINOR) TEXT(".") TOSTRING(NUM_PRODUCT_PATCH) TEXT(".") TOSTRING(NUM_PRODUCT_PRERELEASE)

#define STR_ABOUT_NAME          STR_INTERNAL_NAME
#define STR_ABOUT_WEB           TEXT("https://github.com/stuerp/foo_input_vgmstream")
#define STR_ABOUT_EMAIL         TEXT("mailto:peter.stuer@outlook.com")

#define IDD_CONFIG                      101

#define IDC_LOOP_COUNT                  1000
#define IDC_FADE_SECONDS                1001
#define IDC_FADE_DELAY_SECONDS          1002
#define IDC_LOOP_NORMALLY               1003
#define IDC_LOOP_FOREVER                1004
#define IDC_IGNORE_LOOP                 1005
#define IDC_THREAD_PRIORITY_SLIDER      1006
#define IDC_THREAD_PRIORITY_TEXT        1007
#define IDC_DEFAULT_BUTTON              1008
#define IDC_DISABLE_SUBSONGS            1009
#define IDC_DOWNMIX_CHANNELS            1010
#define IDC_TAGFILE_DISABLE             1011
#define IDC_OVERRIDE_TITLE              1012
#define IDC_EXTS_UNKNOWN_ON             1015
#define IDC_EXTS_COMMON_ON              1016
