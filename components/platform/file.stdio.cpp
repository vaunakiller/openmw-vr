#include "file.hpp"

#include <errno.h>
#include <string.h>
#include <string>
#include <stdexcept>
#include <cassert>

namespace Platform::File {

    static auto getNativeHandle(Handle handle)
    {
        assert(handle != Handle::Invalid);

        return reinterpret_cast<FILE*>(static_cast<intptr_t>(handle));
    }

    static int getNativeSeekType(SeekType seek)
    {
        if (seek == SeekType::Begin)
            return SEEK_SET;
        if (seek == SeekType::Current)
            return SEEK_CUR;
        if (seek == SeekType::End)
            return SEEK_END;
        return -1;
    }

    Handle open(const char* filename)
    {
        FILE* handle = fopen(filename, "rb");
        if (handle == nullptr)
        {
            throw std::runtime_error(std::string("Failed to open '") + filename + "' for reading: " + strerror(errno));
        }
        return static_cast<Handle>(reinterpret_cast<intptr_t>(handle));
    }

    void close(Handle handle)
    {
        auto nativeHandle = getNativeHandle(handle);
        fclose(nativeHandle);
    }

    void seek(Handle handle, size_t position, SeekType type /*= SeekType::Begin*/)
    {
        const auto nativeHandle = getNativeHandle(handle);
        const auto nativeSeekType = getNativeSeekType(type);
        if (fseek(nativeHandle, position, nativeSeekType) != 0)
        {
            throw std::runtime_error(std::string("An fseek() call failed: ") + strerror(errno));
        }
    }

    size_t size(Handle handle)
    {
        auto nativeHandle = getNativeHandle(handle);
        
        const auto oldPos = tell(handle);
        seek(handle, 0, SeekType::End);
        const auto fileSize = tell(handle);
        seek(handle, oldPos, SeekType::Begin);

        return static_cast<size_t>(fileSize);
    }

    size_t tell(Handle handle)
    {
        auto nativeHandle = getNativeHandle(handle);

        long position = ftell(nativeHandle);
        if (position == -1)
        {
            throw std::runtime_error(std::string("An ftell() call failed: ") + strerror(errno));
        }
        return static_cast<size_t>(position);
    }

    size_t read(Handle handle, void* data, size_t size)
    {
        auto nativeHandle = getNativeHandle(handle);

        int amount = fread(data, 1, size, nativeHandle);
        if (amount == 0 && ferror(nativeHandle))
        {
            throw std::runtime_error(std::string("An attempt to read ") + std::to_string(size) + " bytes failed: " + strerror(errno));
        }
        return static_cast<size_t>(amount);
    }

}
