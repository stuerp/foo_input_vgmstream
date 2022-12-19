#pragma once

#include <foobar2000/SDK/foobar2000.h>

class input_file_type_v2_impl_vgmstream : public input_file_type_v2
{
public:
    input_file_type_v2_impl_vgmstream() noexcept
    {
        _Extensions = vgmstream_get_formats(&_ExtensionCount);
    }

    unsigned get_count() noexcept final
    {
        return _ExtensionCount;
    }

    bool is_associatable(unsigned) noexcept final
    {
        return true;
    }

    void get_format_name(unsigned idx, pfc::string_base & out, bool isPlural) final
    {
        out.reset();

        pfc::stringToUpperAppend(out, _Extensions[idx], pfc::strlen_utf8(_Extensions[idx]));

        out += " Audio File";

        if (isPlural)
            out += "s";
    }

    void get_extensions(unsigned idx, pfc::string_base & out) noexcept final
    {
        out = _Extensions[idx];
    }

private:
    const char ** _Extensions;
    size_t _ExtensionCount;
};

namespace
{
    static service_factory_single_t<input_file_type_v2_impl_vgmstream> _Filetypes;
}
