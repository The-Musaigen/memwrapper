#ifndef MEMWRAPPER_ALLOCATOR_HPP_
#define MEMWRAPPER_ALLOCATOR_HPP_

namespace memwrapper {
enum class Registers { Eax, Ecx, Edx, Ebx, Esp, Ebp, Esi, Edi };

/**
 * \brief Default page size.
 */
constexpr auto kPageSize4Kb = 4096u;

/**
 * \brief Allocator for more easier byte interaction.
 */
class basic_allocator {
  protected:
    /**
     * \brief Pointer to the start array of bytes.
     */
    uint8_t* m_code;
    /**
     * \brief Size of array.
     */
    uint32_t m_size;
    /**
     * \brief Offset for write.
     */
    uint32_t m_offset;

  public:
    basic_allocator(const uint32_t size = kPageSize4Kb)
        : m_code(nullptr)
        , m_size(0)
        , m_offset(0) {
        SYSTEM_INFO sysinfo{ 0 };
        GetSystemInfo(&sysinfo);

        m_size = detail::align_value(size, sysinfo.dwPageSize);
        m_code = reinterpret_cast<uint8_t*>(VirtualAlloc(
            NULL, m_size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    }

    /**
     * Writes new byte in the array and shifts offset.
     *
     * \param opcode New byte.
     */
    basic_allocator& db(const uint8_t opcode) {
        if ((m_offset + sizeof(uint8_t)) > m_size)
            return *this;

        m_code[m_offset++] = opcode;
        return *this;
    }

    /**
     * Writes array of elements with specific size and shifts offset.
     *
     * \param object Array of elements.
     * \param size Size of array.
     */
    template<typename T>
    basic_allocator& db(T* object, const uint32_t size) {
        uint8_t* oplist{ reinterpret_cast<uint8_t*>(object) };
        for (uint32_t i = 0; i < size; i++)
            db(oplist[i]);

        return *this;
    }

    /**
     * Writes new value with size > 1 bytes and shifts offset.
     *
     * \param value New value.
     */
    template<typename T>
    basic_allocator& dbvalue(const T value) {
        if constexpr (sizeof(T) > 1) {
            detail::byteof<T> byteof{ value };

            for (size_t i = 0; i < sizeof(T); i++)
                db(byteof.bytes[i]);
        } else
            db(static_cast<uint8_t>(value));

        return *this;
    }

    /**
     * Writes a sequence of any values in the array.
     *
     * \param value Head of sequence.
     * \param ...args Tail of sequence.
     */
    template<typename Head, typename... Tail>
    basic_allocator& dbvalues(const Head value, const Tail&&... args) {
        dbvalue(value);

        if constexpr (sizeof...(Tail) > 1)
            dbvalues(std::forward<Tail>(args)...);
        else
            dbvalue(std::forward<Tail>(args)...);

        return *this;
    }

    /**
     * \return Pointer to the start array of bytes.
     */
    memory_pointer begin() const { return { m_code }; }
    /**
     * \return Current position in the array.
     */
    memory_pointer now() const { return { &m_code[m_offset] }; }
    /**
     * \return Pointer to the end array of bytes.
     */
    memory_pointer end() const { return { &m_code[m_size - 1u] }; }

    /**
     * \param offset Offset from start of array.
     * \return Pointer to the element of array.
     */
    memory_pointer get(const uint32_t offset = 0) const {
        return (offset >= m_size) ? end() : memory_pointer(&m_code[offset]);
    }

    /**
     * \param offset Offset from start of array.
     * \return Pointer to the element of array.
     */
    template<typename T = uint8_t*>
    T get(const uint32_t offset = 0) const {
        return get(offset).cast<T>();
    }

    /**
     * Releases the pointer to array.
     */
    void free() { VirtualFree(m_code, 0, MEM_RELEASE); }

    /**
     * Marks that our code is ready to use.
     */
    void ready() const { flush_memory(m_code, m_size); }

    /**
     * \return Current position in the array.
     */
    uint32_t get_offset() const { return m_offset; }
    /**
     * Sets new position in the array.
     *
     * \param offset New position.
     */
    void set_offset(const uint32_t offset) {
        if ((offset < 0) || (offset >= m_size))
            return;

        m_offset = offset;
    }
};   // !class basic_allocator

class asm_allocator : public basic_allocator {
  public:
    asm_allocator(const uint32_t size = kPageSize4Kb)
        : basic_allocator(size) {}

    asm_allocator& jmp(const memory_pointer& to) {
        auto rel32 = detail::get_relative_address(to, now());

        db(0xE9);
        dbvalue(rel32);
        return *this;
    }

    asm_allocator& push(const Registers reg86) {
        db(0x50 + static_cast<uint8_t>(reg86));
        return *this;
    }

    asm_allocator& pop(const Registers reg86) {
        db(0x58 + static_cast<uint8_t>(reg86));
        return *this;
    }

    asm_allocator& mov(const Registers in, const Registers out, const uint8_t offset) {
        db(0x8B);

        if (!offset)
            db(0x08 * static_cast<uint8_t>(in) + static_cast<uint8_t>(out));
        else
            db(0x08 * static_cast<uint8_t>(in) + static_cast<uint8_t>(out) +
               0x40);

        if (out == Registers::Esp)
            db(0x24);

        if (offset)
            db(offset);

        return *this;
    }

    asm_allocator& mov(const uint32_t* in, const Registers out) {
        if (out == Registers::Eax)
            db(0xA3);
        else {
            db(0x89);
            db(0x0D + static_cast<uint8_t>(out) * 0x08 - 0x08);
        }

        dbvalue(in);
        return *this;
    }
};   // !class asm_allocator : public basic_allocator
}   // namespace memwrapper

#endif   // !MEMWRAPPER_ALLOCATOR_HPP_
