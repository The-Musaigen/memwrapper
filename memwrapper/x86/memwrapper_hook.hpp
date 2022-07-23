#ifndef MEMWRAPPER_HOOK_HPP_
#define MEMWRAPPER_HOOK_HPP_

namespace memwrapper {
namespace detail {

enum MemhookFlags : uint32_t {
    kNone            = (0),
    kInstalled       = (1 << 0),
    kListingBroken   = (1 << 1),
    kExecutable      = (1 << 2),
    kCallInstruction = (1 << 3)
};

struct memhook_context {
    uintptr_t return_address;
};   // struct memhook_context;
}   // namespace detail

/**
 * Constants.
 */
constexpr uint8_t  kCallOpcode = 0xE8;
constexpr uint8_t  kJumpOpcode = 0xE9;
constexpr uint8_t  kNopOpcode  = 0x90;
constexpr uint32_t kJumpSize   = 0x05u;

template<typename Function>
class memhook {
  protected:
    using memhook_original_code_t = std::unique_ptr<uint8_t[]>;
    using memhook_trampoline_t    = std::unique_ptr<asm_allocator>;

    using memhook_flags_t    = detail::MemhookFlags;
    using memhook_call_abs_t = uintptr_t;

    using Ret = detail::return_type_t<Function>;

    /**
     * The function in memory where the hook will be installed.
     */
    memory_pointer m_hookee;
    /**
     * The function in memory that will be the hook.
     */
    memory_pointer m_hooker;
    /**
     * Hook size.
     */
    size_t m_size;
    /**
     * Original instructions for recovery..
     */
    memhook_original_code_t m_original_code;
    /**
     * ASM code for the trampoline.
     */
    memhook_trampoline_t m_trampoline_code;
    /**
     * Hook flags.
     */
    uint32_t m_flags;
    /**
     * Stores the absolute address of a function for a call instruction.
     */
    memhook_call_abs_t m_call_abs;
    /**
     * Hook runtime context.
     */
    detail::memhook_context m_context;

  public:
    /**
     * We forbid constructing from other hooks.
     */
    memhook(const memhook&) = delete;
    memhook(memhook&&)   = delete;
    memhook& operator=(const memhook&) = delete;
    memhook& operator=(memhook&&) = delete;

    /**
     * Constructor.
     * 
     * \param hookee The function in memory where the hook will be installed.
     * \param hooker The function in memory that will be the hook.
     */
    memhook(const memory_pointer& hookee, const memory_pointer& hooker)
        : m_hookee(hookee)
        , m_hooker(hooker)
        , m_size(0u)
        , m_call_abs(0u)
        , m_flags(memhook_flags_t::kNone) {
        uint8_t* cursor = m_hookee;

        while (m_size < kJumpSize) {
            hde32s hs;
            hde32_disasm(cursor, &hs);

            if (hs.flags & F_ERROR) {
                m_flags |= memhook_flags_t::kListingBroken;
                break;
            }

            m_size += hs.len;
            cursor += hs.len;
        }

        if (is_executable(m_hookee))
            m_flags |= memhook_flags_t::kExecutable;
    }

    /**
     * Destructor.
     */
    ~memhook() { remove(); }

    /**
     * Installs the hook.
     */
    void install() {
        using std::make_unique, detail::get_relative_address;

        // Checking is we available to place hook.
        if ((m_flags & memhook_flags_t::kInstalled) ||
            (m_flags & memhook_flags_t::kListingBroken) ||
            !(m_flags & memhook_flags_t::kExecutable))
            return;

        if (m_original_code) {
            // Installing jump to our hooker-function again.
            m_trampoline_code->set_offset(0xBu);
            m_trampoline_code->jmp(m_hooker).ready();

            // Marking as installed.
            m_flags |= memhook_flags_t::kInstalled;
            return;
        } else {
            hde32s hs;
            hde32_disasm(m_hookee, &hs);

            // If call instruction.
            if (hs.opcode == kCallOpcode) {
                m_call_abs =
                    detail::restore_absolute_address(hs.imm.imm32, m_hookee);
                m_flags |= memhook_flags_t::kCallInstruction;
            }
        }

        // Creating trampoline and original code instances.
        m_original_code   = make_unique<uint8_t[]>(m_size);
        m_trampoline_code = make_unique<asm_allocator>();

        // Copying original code.
        copy_memory(m_original_code.get(), m_hookee, m_size);

        // Copying return address into `m_context` structure.
        m_trampoline_code->push(Registers::Eax)
            .mov(Registers::Eax, Registers::Esp, sizeof(uint32_t))
            .mov(&m_context.return_address, Registers::Eax)
            .pop(Registers::Eax)
            // Jumping to our hooker-function.
            .jmp(m_hooker);

        // Rewriting original instructions.
        if ((m_flags & memhook_flags_t::kCallInstruction) == 0)
            generate_trampoline_instructions();

        // Marking as ready to execute.
        m_trampoline_code->ready();

        // Patching `hookee`.
        if ((m_flags & memhook_flags_t::kCallInstruction) == 0)
            write_memory(m_hookee, kJumpOpcode);

        write_memory(
            m_hookee.front(1u),
            get_relative_address(m_trampoline_code->begin(), m_hookee));

        if (m_size > kJumpSize)
            fill_memory(m_hookee.front(kJumpSize), kNopOpcode,
                        m_size - kJumpSize);

        // Marking as installed.
        m_flags |= memhook_flags_t::kInstalled;
    }

    /**
     * Removes the hook.
     */
    void remove() {
        // Checking is we can remove hook.
        if ((m_flags & memhook_flags_t::kInstalled) == 0)
            return;

        // Creating lambda-functions.
        auto unload_hook = [this]() {
            // Copying original instructions back.
            copy_memory(m_hookee, m_original_code.get(), m_size);

            // Releasing the trampoline.
            m_trampoline_code->free();

            // Resetting the smart pointers.
            m_trampoline_code.reset();
            m_original_code.reset();

            // Removing flags.
            m_flags = memhook_flags_t::kNone;
        };

        auto patch_hook = [this]() {
            if (m_flags & memhook_flags_t::kCallInstruction) {
                // Redirecting jump to stored function absolute address;
                m_trampoline_code->set_offset(0xBu);
                m_trampoline_code->jmp(m_call_abs);
            } else {
                // Nop jump to avoid crash or calling hooker-function.
                fill_memory(m_trampoline_code->get(0xBu), kNopOpcode,
                            kJumpSize);
            }

            // Flushing trampoline.
            m_trampoline_code->ready();

            // Marking as uninstalled.
            m_flags &= ~memhook_flags_t::kInstalled;
        };

        //  Implementing hook remove.
        hde32s hs;
        hde32_disasm(m_hookee, &hs);

        // Listing is broken, not a call/jmp instruction (relative + imm32).
        if ((hs.flags & F_ERROR) || !(hs.flags & F_RELATIVE) ||
            !(hs.flags & F_IMM32))
            return unload_hook();

        uintptr_t destination =
            detail::restore_absolute_address(hs.imm.imm32, m_hookee, hs.len);
        uintptr_t trampoline = m_trampoline_code->get<uintptr_t>();

        if ((destination == trampoline) || (destination == m_call_abs))
            unload_hook();
        else
            patch_hook();
    }

    /**
     * Calls the original function we hooked.
     */
    template<typename... Args>
    Ret call(Args... args) const {
        // Shortcut.
        using std::forward, detail::call_convention_v;

        uintptr_t call_address;
        if ((m_flags & memhook_flags_t::kCallInstruction))
            call_address = m_call_abs;
        else
            call_address = m_trampoline_code->get(0x10u);

        // Calling our function.
        return call_function<Ret, call_convention_v<Function>>(
            call_address, forward<Args>(args)...);
    }

    detail::memhook_context get_context() const { return m_context; }

  private:
    void generate_trampoline_instructions() {
        using detail::get_relative_address, detail::restore_absolute_address;

        // Creating structures of instructions with rel32.
        detail::call_relative call = { 0xE8, 0x00000000u };
        detail::jmp_relative  jmp  = { 0xE9, 0x00000000u };
        detail::jcc_relative  jcc  = { 0x0F, 0x80, 0x00000000u };

        uint8_t* now  = m_hookee;
        uint32_t step = 0u;

        bool finished = false;
        while (!finished) {
            if (step >= m_size) {
                m_trampoline_code->jmp(now);
                break;
            }

            void*    opcode;
            uint32_t oplen;

            hde32s   hs;
            uint32_t len = hde32_disasm(now, &hs);

            if (hs.flags & F_ERROR)
                break;

            if (hs.opcode == 0xE8) {
                uintptr_t destination =
                    restore_absolute_address(hs.imm.imm32, now);

                call.operand =
                    get_relative_address(destination, m_trampoline_code->now());

                opcode = &call;
                oplen  = sizeof(call);
            } else if ((hs.opcode & 0xFD) == 0xE9) {
                uintptr_t destination =
                    reinterpret_cast<uintptr_t>(now + hs.len);
                if (hs.opcode == 0xEB)
                    destination += hs.imm.imm8;
                else
                    destination += hs.imm.imm32;

                jmp.operand =
                    get_relative_address(destination, m_trampoline_code->now());

                opcode = &jmp;
                oplen  = sizeof(jmp);
            } else if ((hs.opcode & 0xF0) == 0x70 ||
                       (hs.opcode2 & 0xF0) == 0x80) {
                uintptr_t destination =
                    reinterpret_cast<uint32_t>(now + hs.len);
                if ((hs.opcode & 0xF0) == 0x70)
                    destination += hs.imm.imm8;
                else
                    destination += hs.imm.imm32;

                uint8_t cond =
                    ((hs.opcode != 0x0F ? hs.opcode : hs.opcode2) & 0x0F);

                jcc.opcode2 = (0x80 | cond);
                jcc.operand = get_relative_address(
                    destination, m_trampoline_code->now(), sizeof(jcc));

                opcode = &jcc;
                oplen  = sizeof(jcc);
            } else {
                opcode = now;
                oplen  = len;
            }

            // Adding instruction to code.
            m_trampoline_code->db(opcode, oplen);

            // Shifting cursor.
            step += oplen;
            now += len;
        }
    }
};
}   // namespace memwrapper

#endif   // !MEMWRAPPER_HOOK_HPP_
