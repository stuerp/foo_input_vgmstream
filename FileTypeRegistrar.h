
/** $VER: FileTypeRegistrar.h (2022.12.28) P. Stuer **/

#pragma once

#include <foobar2000/SDK/foobar2000.h>

extern "C"
{
#include <src/vgmstream.h>
}

#pragma region("FileTypeRegistrar")
class FileTypeRegistrar : public input_file_type_v2
{
public:
    FileTypeRegistrar() noexcept
    {
        _Extensions = ::vgmstream_get_formats(&_ExtensionCount);
    }
    FileTypeRegistrar(const FileTypeRegistrar&) = delete;
    FileTypeRegistrar(const FileTypeRegistrar&&) = delete;
    FileTypeRegistrar& operator=(const FileTypeRegistrar&) = delete;
    FileTypeRegistrar& operator=(FileTypeRegistrar&&) = delete;
    virtual ~FileTypeRegistrar() { };

    #pragma region("Interface input_file_type")
    unsigned get_count() noexcept final
    {
        return (unsigned)_ExtensionCount;
    }

    bool is_associatable(unsigned) noexcept final
    {
        return true;
    }
    #pragma endregion

    #pragma region("Interface input_file_type_v2")
    void get_format_name(unsigned idx, pfc::string_base & out, bool isPlural) final
    {
        out.reset();

        pfc::stringToUpperAppend(out, _Extensions[idx], pfc::strlen_utf8(_Extensions[idx]));

        out += " Audio File";

        if (isPlural)
            out += "s";
    }

    void get_extensions(unsigned idx, pfc::string_base & out) final
    {
        out = _Extensions[idx];
    }
    #pragma endregion

private:
    const char ** _Extensions;
    size_t _ExtensionCount;
};

namespace
{
    static service_factory_single_t<FileTypeRegistrar> _FileTypeRegistrar;
}
#pragma endregion
