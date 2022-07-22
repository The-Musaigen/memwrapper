#ifndef MEMWRAPPER_LLMO_HPP_
#define MEMWRAPPER_LLMO_HPP_

namespace memwrapper {
enum class MemoryProt {
    None             = (0),
    NoAccess         = (1 << 1),
    ReadOnly         = (1 << 2),
    ReadWrite        = (1 << 3),
    WriteCopy        = (1 << 4),
    Execute          = (1 << 5),
    ExecuteRead      = (1 << 6),
    ExecuteReadWrite = (1 << 7),
    ExecuteWriteCopy = (1 << 8)
};

namespace detail {
/**
 * Converts a memory protection constant to memory flag.
 *
 * \param prot Memory protection constant.
 * \return Memory flag.
 */
inline uint32_t convert_memory_protection_constant(MemoryProt prot) {
    switch (prot) {
    case MemoryProt::NoAccess: return PAGE_NOACCESS;
    case MemoryProt::ReadOnly: return PAGE_READONLY;
    case MemoryProt::ReadWrite: return PAGE_READWRITE;
    case MemoryProt::WriteCopy: return PAGE_WRITECOPY;
    case MemoryProt::Execute: return PAGE_EXECUTE;
    case MemoryProt::ExecuteRead: return PAGE_EXECUTE_READ;
    case MemoryProt::ExecuteReadWrite: return PAGE_EXECUTE_READWRITE;
    case MemoryProt::ExecuteWriteCopy: return PAGE_EXECUTE_WRITECOPY;

    default: return 0;
    }
}
}   // namespace detail

/**
 * @brief Class for RAII protect changing.
 */
class scoped_unprotect {
  protected:
    /**
     * Stores information about pointer on the unprotected memory.
     */
    void* m_pointer;
    /**
     * Stores a size of the unprotected memory.
     */
    size_t m_size;
    /**
     * Stores a protection that was here.
     */
    DWORD m_protection;
    /**
     * Result of unprotection.
     */
    bool m_result;

  public:
    /**
     * Constructor. Changes the memory protection.
     *
     * \param pointer Pointer on the memory will be unprotected.
     * \param size Size of a region that will be unprotected.
     * \param prot New protection of the region.
     */
    scoped_unprotect(const memory_pointer& pointer, const size_t size,
                     const MemoryProt prot = MemoryProt::ExecuteReadWrite)
        : m_pointer(pointer)
        , m_size(size) {
        auto new_protect = detail::convert_memory_protection_constant(prot);

        m_result = (VirtualProtect(m_pointer, m_size, new_protect,
                                   &m_protection) != 0);
    }

    /**
     * Destructor. Restores original protection.
     */
    ~scoped_unprotect() {
        if (good())
            VirtualProtect(m_pointer, m_size, m_protection, &m_protection);
    }

    /**
     * \return Resulf of unprotection (true == good; false == bad)
     */
    bool good() { return m_result; }
};   // !class scoped_unprotect

// LLM operations

/**
 * Flushes the memory region with specific size.
 *
 * \param at Memory region start address.
 * \param size Size of the memory, that will be flushed.
 * \return Was memory flushed or not.
 */
inline bool flush_memory(const memory_pointer& at, const size_t size) {
    return (FlushInstructionCache(GetCurrentProcess(), at, size) != 0);
}

/**
 * Reads a memory.
 *
 * \param at Memory region.
 * \return Value stored in this region.
 */
template<typename T>
inline T read_memory(const memory_pointer& at) {
    // Unprotecting.
    scoped_unprotect unprotect(at, sizeof(T));
    // Returning stored value.
    return *at.cast<T*>();
}

/**
 * Writes a value into a memory.
 *
 * \param at The memory region.
 * \param value New region value.
 */
template<typename T>
inline void write_memory(const memory_pointer& at, const T value) {
    // Unprotecting.
    scoped_unprotect unprotect(at, sizeof(T));
    // Writing new value.
    *at.cast<T*>() = value;
    // Flushing information about this region in CPU.
    flush_memory(at, sizeof(T));
}

/**
 * Filles a specific value into a memory with specific size.
 *
 * \param at A memory region.
 * \param value New value that will be stored in this region.
 * \param size Size of this region.
 */
inline void fill_memory(const memory_pointer& at, const int value,
                        const size_t size) {
    // Unprotecting.
    scoped_unprotect unprotect(at, size);
    // Do memset.
    std::memset(at, value, size);
    // Flushing information about this region in CPU.
    flush_memory(at, size);
}

/**
 * Copying an information from one memory region to other.
 *
 * \param dst Destination where will be stored new value.
 * \param src Source from that will be copyied value.
 * \param size Size of the memory region.
 */
inline void copy_memory(const memory_pointer& dst, const memory_pointer& src,
                        const size_t size) {
    // Unprotecting the memory region.
    scoped_unprotect unprotect(dst, size);
    // Copying new value to the memory region.
    std::memcpy(dst, src, size);
    // Flushing information about this region in CPU.
    flush_memory(dst, size);
}

/**
 * Compares an information from one memory region with other.
 *
 * \param buff1 The memory region of first buffer.
 * \param buff2 The memory region of second buffer.
 * \param size Number of bytes to compare.
 * \return Zero if they all match or a value different from zero which is
 * greater if they do not.
 */
inline int compare_memory(const memory_pointer& buff1,
                          const memory_pointer& buff2, const size_t size) {
    // Unprotecting first memory region.
    scoped_unprotect unprotect0(buff1, size);
    // Unprotecting second memory region.
    scoped_unprotect unprotect1(buff2, size);
    // Comparing.
    return std::memcmp(buff1, buff2, size);
}

/**
 * Checking is memory region executable.
 *
 * \param at Memory region to check.
 * \return Status of memory region, true if executable, false if not.
 */
inline bool is_executable(const memory_pointer& at) {
    MEMORY_BASIC_INFORMATION mbi{ 0 };
    VirtualQuery(at, &mbi, sizeof(mbi));
    return (mbi.State == MEM_COMMIT) && (mbi.Protect != PAGE_NOACCESS);
}

/**
 * Searches a memory region with specific module, pattern and mask.
 *
 * \param mod The module in which the search will be performed. (example.dll,
 * process.exe) \param pattern The pattern to search for. \param mask The mask
 * of the pattern. \return Address of memory region. Zero if isn't found.
 */
static uintptr_t search_memory_pattern(std::string_view mod,
                                       std::string_view pattern,
                                       std::string_view mask) {
    // Trying to get module base address.
    memory_pointer handle = GetModuleHandle(mod.data());

    // Module isn't loaded into programm.
    if (!handle)
        return 0;

    // Retrieving information about module page.
    MEMORY_BASIC_INFORMATION mbi{ 0 };
    if (!VirtualQuery(handle, &mbi, sizeof(mbi)))
        return 0;

    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(mbi.AllocationBase);
    auto pe  = reinterpret_cast<IMAGE_NT_HEADERS*>(
        reinterpret_cast<uint32_t>(dos) + dos->e_lfanew);

    // Checking for signature type.
    if (pe->Signature != IMAGE_NT_SIGNATURE)
        return 0;

    auto now = reinterpret_cast<uint8_t*>(mbi.AllocationBase);
    auto end = now + pe->OptionalHeader.SizeOfImage;

    size_t i{};
    while (now < end) {
        for (i = 0; i < mask.size(); i++) {
            if (&now[i] >= end)
                break;

            auto character = mask[i];
            auto byte      = static_cast<uint8_t>(pattern[i]);

            // If byte is optional then skip.
            if (character == '?')
                continue;

            // Otherwise, checking for same
            if (now[i] != byte)
                break;
        }

        // If in the end of the mask.
        if (!mask[i])
            return reinterpret_cast<uintptr_t>(now);

        ++now;
    }

    return 0;
}

/**
 * @brief Class for RAII writing.
 */
template<typename T>
class scoped_write {
  protected:
    /**
     * Stores information about pointer, there will be wrote data.
     */
    void* m_pointer;
    /**
     * Is backup is initialized.
     */
    bool m_initialized;
    /**
     * Backup.
     */
    T m_data;

  public:
    scoped_write()                    = default;
    scoped_write(const scoped_write&) = delete;
    scoped_write(scoped_write&&)      = delete;

    /**
     * Installs patch in {at} with value {value}.
     *
     * \param at Memory object, where will be placed patch.
     * \param value New value of this object.
     */
    scoped_write(const memory_pointer& at, const T value)
        : m_pointer(at) {
        // Read previous data for backup.
        m_data = read_memory<T>(at);
        // Marking as initialized.
        m_initialized = true;

        // Installing new value.
        write_memory<T>(at, value);
    }

    ~scoped_write() { restore(); }

    /**
     * Delayed patch in {at} with value {value}.
     *
     * \param at Memory object, where will be placed patch.
     * \param value New value of this object.
     */
    void install(const memory_pointer& at, const T value) {
        // Updating members.
        m_pointer = at;

        // Read previous data for backup.
        m_data = read_memory<T>(at, value);
        // Marking as initialized.
        m_initialized = true;

        // Installing new value.
        write_memory<T>(at, value);
    }

    /**
     * Restores previous data.
     */
    void restore() {
        // If backup was initialized.
        if (m_initialized)
            write_memory<T>(m_pointer, m_data);

        // Marking as uninitialized.
        m_initialized = false;
    }
};   // !class scoped_write<T>

/**
 * @brief Class for RAII copying.
 */
template<size_t bufsize>
class scoped_copy {
  protected:
    /**
     * Stores information about pointer, there will be patch installed.
     */
    void* m_pointer;
    /**
     * Is value initialized.
     */
    bool m_initialized;
    /**
     * Backup.
     */
    uint8_t m_buf[bufsize];

  public:
    scoped_copy()                   = default;
    scoped_copy(const scoped_copy&) = delete;
    scoped_copy(scoped_copy&&)      = delete;

    /**
     * Installs patch in {at} with value {value}.
     *
     * \param at Memory object, where will be placed patch.
     * \param data New data of this object.
     */
    scoped_copy(const memory_pointer& at, const memory_pointer& data)
        : m_pointer(at) {
        // Copying previous data for backup.
        copy_memory(m_buf, at, bufsize);
        // Installing new data.
        copy_memory(at, data, bufsize);

        // Marking as initialized.
        m_initialized = true;
    }

    ~scoped_copy() { restore(); }

    /**
     * Delayed patch in {at} with value {value}.
     *
     * \param at Memory object, where will be placed patch.
     * \param data New data of this object.
     */
    void install(const memory_pointer& at, const memory_pointer& data) {
        // Updating members.
        m_pointer = at;

        // Copying previous data for backup.
        copy_memory(m_buf, at, bufsize);
        // Installing new data.
        copy_memory(at, data, bufsize);

        // Marking as initialized.
        m_initialized = true;
    }

    /**
     * Restores previous data.
     */
    void restore() {
        // If backup was initialized.
        if (m_initialized)
            copy_memory(m_pointer, m_buf,
                        bufsize);   // Copying backup to the pointer.

        // Marking as uninitialized.
        m_initialized = false;
    }
};   // !class scoped_copy<bufsize>

/**
 * @brief Class for RAII filling.
 */
template<size_t bufsize>
class scoped_fill {
  protected:
    /**
     * Stores information about pointer, there will be patch installed.
     */
    void* m_pointer;
    /**
     * Is backup was initialized.
     */
    bool m_initialized;
    /**
     * Backup.
     */
    uint8_t m_buf[bufsize];

  public:
    scoped_fill()                   = default;
    scoped_fill(const scoped_fill&) = delete;
    scoped_fill(scoped_fill&&)      = delete;

    /**
     * Installs patch in {at} with value {value}.
     *
     * \param at Memory object, where will be placed patch.
     * \param data New data of this object.
     */
    scoped_fill(const memory_pointer& at, const int value)
        : m_pointer(at) {
        // Copying previous data for backup.
        copy_memory(m_buf, at, bufsize);
        // Fills new data.
        fill_memory(at, value, bufsize);
        // Marking as installed.
        m_initialized = true;
    }

    ~scoped_fill() { restore(); }

    /**
     * Delayed patch in {at} with value {value}.
     *
     * \param at Memory object, where will be placed patch.
     * \param data New data of this object.
     */
    void install(const memory_pointer& at, const int value) {
        // Updating members.
        m_pointer = at;

        // Copying previous data for backup.
        copy_memory(m_buf, at, bufsize);
        // Filling new data.
        fill_memory(at, value, bufsize);

        // Marking as installed.
        m_initialized = true;
    }

    void restore() {
        // If backup was initialized.
        if (m_initialized)
            copy_memory(m_pointer, m_buf,
                        bufsize);   // Restoring previous data.

        // Marking as uninitialized.
        m_initialized = false;
    }
};   // !class scoped_fill<bufsize>

/**
 * @brief Unit-class that stores all information for specific patch-unit.
 */
class scoped_patch_unit {
    using byte_vector    = std::vector<uint8_t>;
    using string_vector  = std::vector<std::string_view>;
    using address_vector = std::vector<memory_pointer>;

    /**
     * Stores information about pointer there will be patch installed.
     */
    memory_pointer m_address;
    /**
     * Stores information about data that will replace.
     */
    byte_vector m_replacement;
    /**
     * Backup.
     */
    byte_vector m_original;

  public:
    scoped_patch_unit()                         = delete;
    scoped_patch_unit(const scoped_patch_unit&) = default;
    scoped_patch_unit(scoped_patch_unit&&)      = default;

    /**
     * \param mod Module there will be patch installed.
     * \param offset Offset of the module there will be patch installed.
     * \param replacement Data that will replace.
     * \param original Original data for backup.
     */
    scoped_patch_unit(std::string_view mod, const memory_pointer& offset,
                      const byte_vector& replacement,
                      const byte_vector& original)
        : m_replacement(replacement)
        , m_original(original)
        , m_address(GetModuleHandle(mod.data()) + offset.addressof()) {}

    /**
     * \param mod Module there will be patch installed.
     * \param offset Offset of the module there will be patch installed.
     * \param replacement Data that will replace.
     * \param original Original data for backup.
     */
    scoped_patch_unit(std::string_view mod, const memory_pointer& offset,
                      const byte_vector& replacement)
        : m_replacement(replacement) {
        auto handle = reinterpret_cast<uint32_t>(GetModuleHandle(mod.data()));

        m_address = handle + offset.addressof();
        m_original.resize(replacement.size());

        copy_memory(m_original.data(), m_address, m_original.size());
    }

    /**
     * \param address Address of the core module there will be patch installed.
     * \param replacement Data that will replace.
     * \param original Original data for backup.
     */
    scoped_patch_unit(const memory_pointer& address,
                      const byte_vector&    replacement,
                      const byte_vector&    original)
        : m_address(address)
        , m_replacement(replacement)
        , m_original(original) {}

    /**
     * \param address Address of the core module there will be patch installed.
     * \param replacement Data that will replace.
     */
    scoped_patch_unit(const memory_pointer& address,
                      const byte_vector&    replacement)
        : m_address(address)
        , m_replacement(replacement) {
        m_original.resize(replacement.size());

        copy_memory(m_original.data(), m_address, m_original.size());
    }

    /**
     * Installes the patch.
     */
    void install() {
        copy_memory(m_address, m_replacement.data(), m_replacement.size());
    }

    /**
     * Backups the original data.
     */
    void restore() {
        copy_memory(m_address, m_original.data(), m_original.size());
    }
};   // !class scoped_patch_unit

/**
 * @brief RAII class for patching created like wrapper for \c scoped_patch_unit
 * \c class.
 */
class scoped_patch {
    /**
     * Stores all patch units.
     */
    std::vector<scoped_patch_unit> m_units;

  public:
    scoped_patch()                    = default;
    scoped_patch(const scoped_patch&) = delete;
    scoped_patch(scoped_patch&&)      = delete;

    /**
     * \param units Units vector to copy.
     */
    scoped_patch(const std::vector<scoped_patch_unit>& units)
        : m_units(units) {}

    /**
     * Destructor. Removes the patch.
     */
    ~scoped_patch() { remove(); }

    /**
     * Installes the patch.
     */
    void install() {
        if (m_units.empty())
            return;

        for (auto& unit : m_units)
            unit.install();
    }

    /**
     * Removes the patch.
     */
    void remove() {
        if (m_units.empty())
            return;

        for (auto& unit : m_units)
            unit.restore();
    }

    /**
     * Toggles the patch depends on bool-flag.
     *
     * \param status Bool-flag.
     */
    void toggle(const bool status) {
        if (status)
            install();
        else
            remove();
    }

    /**
     * Adds an unit.
     */
    void add(const scoped_patch_unit& unit) { m_units.push_back(unit); }
};   // !class scoped_patch

}   // namespace memwrapper

#endif   // !MEMWRAPPER_LLMO_HPP_
