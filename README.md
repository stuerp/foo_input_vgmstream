
# foo_input_vgmstream

[foo_input_vgmstream](https://github.com/stuerp/foo_input_vgmstream/releases) is a [foobar2000](https://www.foobar2000.org/) component that adds playback of streamed (prerecorded) video game audio to foobar2000.

It is based on [foo_input_vgmstream](https://gitlab.com/kode54/foo_input_vgmstream) by [kode54](https://gitlab.com/kode54).

## Features

* Supports dark mode.

* Compatible with foobar2000 1.6.13 and foobar2000 2.0 or later (32 and 64-bit version).

## Requirements

* Tested on Microsoft Windows 10 or later.
* [foobar2000](https://www.foobar2000.org/download) v1.6.13 or later. ![foobar2000](https://www.foobar2000.org/button-small.png)

## Getting started

* Double-click `foo_input_vgmstream.fbk2-component`.

or

* Import `foo_input_vgmstream.fbk2-component` into foobar2000 using "File / Preferences / Components / Install...".

## Developing

The code builds out-of-the box with Visual Studio.

### Requirements

To build the code:

* [Microsoft Visual Studio 2022 Community Edition](https://visualstudio.microsoft.com/downloads/)
* [foobar2000 SDK](https://www.foobar2000.org/SDK)
* [Windows Template Library (WTL)](https://github.com/Win32-WTL/WTL) 10.0.10320

To create the deployment package:

* [PowerShell 7.2](https://github.com/PowerShell/PowerShell)

### Setup

Create the following directory structure:

    3rdParty
        WTL10_10320
    bin
        x86
    foo_input_vgmstream
        3rdParty
            vgmstream
    out
    sdk

* `3rdParty/WTL10_10320` contains WTL 10.0.10320.
* `bin` contains a portable version of foobar2000 64-bit for debugging purposes.
* `bin/x86` contains a portable version of foobar2000 32-bit for debugging purposes.
* `foo_input_vgmstream` contains the [Git](https://github.com/stuerp/foo_input_vgmstream) repository.
* `foo_input_vgmstream/3rdParty` contains the [vgmstream](https://github.com/vgmstream/vgmstream.git) repository.
* `out` receives a deployable version of the component.
* `sdk` contains the foobar2000 SDK.

### Building

Open `foo_input_vgmstream.sln` with Visual Studio and build the solution.

### Packaging

To create the component first build the x86 configuration and next the x64 configuration.

## Contributing

If you'd like to contribute, please fork the repository and use a feature
branch. Pull requests are warmly welcome.

## Change Log

v0.1.0.0, 2022-12-19, *"Scratchin' the itch"*

* Initial release of x64 version for [foobar2000](https://www.foobar2000.org/) v2.0.

## Acknowledgements / Credits

* Peter Pawlowski, for the [foobar2000](https://www.foobar2000.org/) audio player. ![foobar2000](https://www.foobar2000.org/button-small.png)
* [vgmstream](https://github.com/vgmstream), for the [vgmstream](https://github.com/vgmstream/vgmstream) library.
* [kode54](https://gitlab.com/kode54), for the original [foo_input_vgmstream](https://gitlab.com/kode54/vgmstream) component.

## Reference Material

* [vgmstream documenation](https://vgmstream.org/)

## Links

* Home page: https://github.com/stuerp/foo_input_vgmstream
* Repository: https://github.com/stuerp/foo_input_vgmstream.git
* Issue tracker: https://github.com/stuerp/foo_input_vgmstream/issues

## License

![License: MIT](https://img.shields.io/badge/license-MIT-yellow.svg)
