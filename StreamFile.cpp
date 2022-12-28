
/** $VER: StreamFile.cpp (2022.12.28) P. Stuer **/

#include <stdio.h>
#include <io.h>

#include <foobar2000/SDK/foobar2000.h>

extern "C"
{
#include <src/streamfile.h>
#include <src/util.h>
}

#include "foo_input_vgmstream.h"
#include "Resources.h"

#pragma warning(disable: 26408 26438 26440 26446 26461 26482)

// Implements a wrapper for a VGMStream STREAMFILE that operates via foobar2000's file service using a buffer.
struct StreamFile
{
    STREAMFILE vt;              /* callbacks */

    bool m_file_opened;         /* if foobar IO service opened the file */
    service_ptr_t<file> m_file; /* foobar IO service */
    abort_callback * p_abort;   /* foobar error stuff */
    /*const*/ char * name;      /* IO filename */
    int name_len;               /* cache */

    char * archname;            /* for foobar's foo_unpack archives */
    int archname_len;           /* cache */
    int archpath_end;           /* where the last \ ends before archive name */
    int archfile_end;           /* where the last | ends before file name */

    offv_t offset;              /* last read offset (info) */
    offv_t buf_offset;          /* current buffer data start */
    uint8_t * buf;              /* data buffer */
    size_t buf_size;            /* max buffer size */
    size_t valid_size;          /* current buffer size */
    t_filesize file_size;       /* buffered file size */
};

static STREAMFILE * open_foo_streamfile_buffer(const char * const filePath, size_t bufferSize, abort_callback * abortHandler, t_filestats * stats);
static STREAMFILE * open_foo_streamfile_buffer_by_file(service_ptr_t<file> file, bool isFileOpened, const char * const filePath, size_t buf_size, abort_callback * abortHandler);

STREAMFILE * open_foo_streamfile(const char * const filePath, abort_callback * abortHandler, t_filestats * stats)
{
    return ::open_foo_streamfile_buffer(filePath, STREAMFILE_DEFAULT_BUFFER_SIZE, abortHandler, stats);
}

static size_t foo_read(StreamFile * sf, uint8_t * dst, offv_t offset, size_t length)
{
    if (!sf || !sf->m_file_opened || !dst || length <= 0 || offset < 0)
        return 0;

    size_t read_total = 0;

    /* is the part of the requested length in the buffer? */
    if (offset >= sf->buf_offset && offset < (offv_t)(sf->buf_offset + sf->valid_size))
    {
        const int buf_into = (int) (offset - sf->buf_offset);

        size_t buf_limit = sf->valid_size - buf_into;

        if (buf_limit > length)
            buf_limit = length;

        ::memcpy(dst, sf->buf + buf_into, buf_limit);

        read_total += buf_limit;
        length -= buf_limit;
        offset += buf_limit;
        dst += buf_limit;
    }

    /* read the rest of the requested length */
    while (length > 0)
    {
        /* ignore requests at EOF */
        if (offset >= (offv_t)sf->file_size)
        {
            //offset = sf->file_size; /* seems fseek doesn't clamp offset */
            //VGM_ASSERT_ONCE(offset > sf->file_size, "STDIO: reading over file_size 0x%x @ 0x%lx + 0x%x\n", sf->file_size, offset, length);
            break;
        }

        /* position to new offset */
        try
        {
            sf->m_file->seek((t_filesize)offset, *sf->p_abort);
        }
        catch (...)
        {
            break; /* this shouldn't happen in our code */
        }

        /* fill the buffer (offset now is beyond buf_offset) */
        try
        {
            sf->buf_offset = offset;
            sf->valid_size = sf->m_file->read(sf->buf, sf->buf_size, *sf->p_abort);
        }
        catch (...)
        {
            break; /* improbable? */
        }

        /* decide how much must be read this time */
        size_t buf_limit = (length > sf->buf_size) ? sf->buf_size : length;

        /* give up on partial reads (EOF) */
        if (sf->valid_size < buf_limit)
        {
            memcpy(dst, sf->buf, sf->valid_size);
            offset += sf->valid_size;
            read_total += sf->valid_size;
            break;
        }

        /* use the new buffer */
        ::memcpy(dst, sf->buf, buf_limit);

        offset += buf_limit;
        read_total += buf_limit;
        length -= buf_limit;
        dst += buf_limit;
    }

    sf->offset = offset; /* last fread offset */

    return read_total;
}

static size_t foo_get_size(StreamFile * sf)
{
    return (size_t)sf->file_size;
}

static offv_t GetOffset(StreamFile * sf) noexcept
{
    return sf->offset;
}

static void foo_get_name(StreamFile * sf, char * name, size_t name_size)
{
    if (name == nullptr)
        return;

    size_t copy_size = (size_t)(sf->name_len) + 1;

    if (copy_size > name_size)
        copy_size = name_size;

    ::memcpy(name, sf->name, copy_size);
    name[copy_size - 1] = '\0';

    /*
    //TODO: check again (looks like a truncate-from-the-end copy, probably a useless remnant of olden times)
    if (sf->name_len > name_size) {
        strcpy(name, sf->name + sf->name_len - name_size + 1);
    } else {
        strcpy(name, sf->name);
    }
    */
}

static void CloseFile(StreamFile * sf)
{
    sf->m_file.release();

    free(sf->name);
    free(sf->archname);
    free(sf->buf);
    free(sf);
}

static STREAMFILE * foo_open(StreamFile * sf, const char * const filePath, size_t buf_size)
{
    if (filePath == nullptr)
        return nullptr;

    service_ptr_t<file> File;

    // vgmstream may need to open "files based on another" (like a changing extension) and "files in the same subdir" (like .txth)
    // or read "base filename" to do comparison. When dealing with archives (foo_unpack plugin) the later two cases would fail, since
    // vgmstream doesn't separate the  "|" special notation foo_unpack adds.
    // To fix this, when this SF is part of an archive we give vgmstream the name without | and restore the archive on open
    // - get name:      "unpack://zip|23|file://C:\file.zip|subfile.adpcm"
    // > returns:       "unpack://zip|23|file://C:\subfile.adpcm" (otherwise base name would be "file.zip|subfile.adpcm")
    // - try opening    "unpack://zip|23|file://C:\.txth
    // > opens:         "unpack://zip|23|file://C:\file.zip|.txth
    // (assumes archives won't need to open files outside archives, and goes before filedup trick)
    if (sf->archname)
    {
        char FilePath[PATH_LIMIT] = { 0 };
        const char * dirsep = NULL;

        // newly open files should be "(current-path)\newfile" or "(current-path)\folder\newfile", so we need to make
        // (archive-path = current-path)\(rest = newfile plus new folders)
        const size_t filename_len = ::strlen(filePath);

        if (filename_len > (size_t)sf->archpath_end)
        {
            dirsep = &filePath[sf->archpath_end];
        }
        else
        {
            dirsep = strrchr(filePath, '\\'); // vgmstream shouldn't remove paths though
            if (!dirsep)
                dirsep = filePath;
            else
                dirsep += 1;
        }

        //TODO improve strops
        ::memcpy(FilePath, sf->archname, (size_t)sf->archfile_end); //copy current path+archive

        FilePath[sf->archfile_end] = '\0';

        ::concatn(sizeof(FilePath), FilePath, dirsep); //paste possible extra dirs and filename

        // subfolders inside archives use "/" (path\archive.ext|subfolder/file.ext)
        for (int i = sf->archfile_end; i < sizeof(FilePath); i++)
        {
            if (FilePath[i] == '\0')
                break;

            if (FilePath[i] == '\\')
                FilePath[i] = '/';
        }

        FB2K_console_formatter() << STR_COMPONENT_FILENAME << ": " << FilePath;

        return ::open_foo_streamfile_buffer(FilePath, buf_size, sf->p_abort, NULL);
    }

    // if same name, duplicate the file pointer we already have open
    if (sf->m_file_opened && !strcmp(sf->name, filePath))
    {
        File = sf->m_file; //copy?
        {
            STREAMFILE * new_sf = ::open_foo_streamfile_buffer_by_file(File, sf->m_file_opened, filePath, buf_size, sf->p_abort);
            if (new_sf)
            {
                return new_sf;
            }
            // failure, close it and try the default path (which will probably fail a second time)
        }
    }

    // a normal open, open a new file
    return ::open_foo_streamfile_buffer(filePath, buf_size, sf->p_abort, NULL);
}

static STREAMFILE * open_foo_streamfile_buffer_by_file(service_ptr_t<file> file, bool m_file_opened, const char * const filePath, size_t bufferSize, abort_callback * abortHandler)
{
    uint8_t * Buffer = (uint8_t *) ::calloc(bufferSize, sizeof(uint8_t));

    StreamFile * This = (StreamFile *) ::calloc(1, sizeof(StreamFile));

    if (Buffer == nullptr || This == nullptr)
        goto fail;

    This->vt.read = (size_t(__cdecl *)(_STREAMFILE *, uint8_t *, offv_t, size_t)) foo_read;
    This->vt.get_size = (size_t(__cdecl *)(_STREAMFILE *)) foo_get_size;
    This->vt.get_offset = (offv_t(__cdecl *)(_STREAMFILE *)) GetOffset;
    This->vt.get_name = (void(__cdecl *)(_STREAMFILE *, char *, size_t)) foo_get_name;
    This->vt.open = (_STREAMFILE * (__cdecl *)(_STREAMFILE *, const char * const, size_t)) foo_open;
    This->vt.close = (void(__cdecl *)(_STREAMFILE *)) CloseFile;

    This->m_file_opened = m_file_opened;
    This->m_file = file;
    This->p_abort = abortHandler;
    This->buf_size = bufferSize;
    This->buf = Buffer;

    //TODO: foobar filenames look like "file://C:\path\to\file.adx"
    // maybe should hide the internal protocol and restore on open?
    This->name = ::_strdup(filePath);

    if (!This->name)
        goto fail;

    This->name_len = (int)::strlen(This->name);

    // foobar supports .zip/7z/etc archives directly, in this format: "unpack://zip|(number))|file://C:\path\to\file.zip|subfile.adx"
    // Detect if current is inside archive, so when trying to get filename or open companion files it's handled correctly    
    // Subfolders have inside the archive use / instead or / (path\archive.zip|subfolder/file)
    if (::strncmp(filePath, "unpack", 6) == 0)
    {
        const char * archfile_ptr = ::strrchr(This->name, '|');

        if (archfile_ptr)
            This->archfile_end = (int)((intptr_t) archfile_ptr + 1 - (intptr_t) This->name); // after "|""

        const char * archpath_ptr = ::strrchr(This->name, '\\');

        if (archpath_ptr)
            This->archpath_end = (int)((intptr_t) archpath_ptr + 1 - (intptr_t) This->name); // after "\\"

        if (This->archpath_end <= 0 || This->archfile_end <= 0 || This->archpath_end > This->archfile_end || This->archfile_end > This->name_len || This->archfile_end >= PATH_LIMIT)
        {
            // ???
            This->archpath_end = 0;
            This->archfile_end = 0;
        }
        else
        {
            This->archname = ::_strdup(filePath);

            if (!This->archname)
                goto fail;

            This->archname_len = This->name_len;

            // change from "(path)\\(archive)|(filename)" to "(path)\\filename)"
            This->name[This->archpath_end] = '\0';
            ::concatn(This->name_len, This->name, &This->archname[This->archfile_end]);
        }
    }

    /* cache file_size */
    This->file_size = This->m_file_opened ? This->m_file->get_size(*This->p_abort) : 0;

    /* STDIO has an optimization to close unneeded FDs if file size is less than buffer,
     * but seems foobar doesn't need this (reuses FDs?) */

    return &This->vt;

fail:
    if (Buffer)
        ::free(Buffer);

    if (This)
        ::free(This);

    return nullptr;
}

static STREAMFILE * open_foo_streamfile_buffer(const char * const filePath, size_t bufferSize, abort_callback * abortHandler, t_filestats * stats)
{
    if (abortHandler == nullptr)
        return nullptr;

    STREAMFILE * sf = nullptr;

    try
    {
        const bool FileExists = filesystem::g_exists(filePath, *abortHandler);

        if (!FileExists)
        {
            /* allow non-existing files in some cases */
            if (!::vgmstream_is_virtual_filename(filePath))
                return nullptr;
        }

        service_ptr_t<file> infile;

        if (FileExists)
        {
            filesystem::g_open_read(infile, filePath, *abortHandler);

            if (stats)
                *stats = infile->get_stats(*abortHandler);
        }

        sf = ::open_foo_streamfile_buffer_by_file(infile, FileExists, filePath, bufferSize, abortHandler);

        if (sf == nullptr)
        {
            //m_file.release(); //refcounted and cleaned after it goes out of scope
        }

    }
    catch (...)
    {
        /* somehow foobar2000 throws an exception on g_exists when filename has a double \
         * (traditionally Windows treats that like a single slash and fopen handles it fine) */
        return nullptr;
    }

    return sf;
}
