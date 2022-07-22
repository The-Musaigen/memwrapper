#ifndef MEMWRAPPER_DETAIL_HPP_
#define MEMWRAPPER_DETAIL_HPP_

namespace memwrapper {
namespace detail {
template<typename T>
union byteof {
    T       value;
    uint8_t bytes[sizeof(T)];

    byteof(T value)
        : value(value) {}
};

enum class CallingConvention { Cdecl, Stdcall, Thiscall, Fastcall };

template<typename Function>
struct function_traits;

template<typename Ret, typename... Args>
struct function_traits<Ret(__cdecl*)(Args...)> {
    using return_type = Ret;

    static constexpr auto args_count      = sizeof...(Args);
    static constexpr auto call_convention = CallingConvention::Cdecl;
};   // !struct function_traits<Ret(__cdecl*)(Args...)>

template<typename Ret, typename... Args>
struct function_traits<Ret(__stdcall*)(Args...)> {
    using return_type = Ret;

    static constexpr auto args_count      = sizeof...(Args);
    static constexpr auto call_convention = CallingConvention::Stdcall;
};   // !struct function_traits<Ret(__stdcall*)(Args...)>

template<typename Ret, typename... Args>
struct function_traits<Ret(__thiscall*)(Args...)> {
    using return_type = Ret;

    static constexpr auto args_count      = sizeof...(Args) - 1;   // Decrease ecx register. 
    static constexpr auto call_convention = CallingConvention::Thiscall;
};   // !struct function_traits<Ret(__thiscall*)(Args...)>

template<typename Ret, typename... Args>
struct function_traits<Ret(__fastcall*)(Args...)> {
    using return_type = Ret;

    static constexpr auto args_count =
        sizeof...(Args) - 2;   // Decrease ecx and edx registers.
    static constexpr auto call_convention = CallingConvention::Fastcall;
};   // !struct function_traits<Ret(__fastcall*)(Args...)>

template<typename Ret, typename Class, typename... Args>
struct function_traits<Ret (Class::*)(Args...)> {
    using return_type = Ret;

    static constexpr auto args_count      = sizeof...(Args);
    static constexpr auto call_convention = CallingConvention::Thiscall;
};   // !struct function_traits<Ret(Class::*)(Args...)>

template<typename T>
using return_type_t = typename function_traits<T>::return_type;

template<typename T>
constexpr auto call_convention_v = function_traits<T>::call_convention;

template<CallingConvention>
struct function_caller;

template<>
struct function_caller<CallingConvention::Cdecl> {
    template<typename Ret, typename... Args>
    static inline Ret call(const memory_pointer& fn, Args... args) {
        return fn.cast<Ret(__cdecl*)(Args...)>()(std::forward<Args>(args)...);
    }
};   // !struct function_caller<CallingConvention::Cdecl>

template<>
struct function_caller<CallingConvention::Stdcall> {
    template<typename Ret, typename... Args>
    static inline Ret call(const memory_pointer& fn, Args... args) {
        return fn.cast<Ret(__stdcall*)(Args...)>()(std::forward<Args>(args)...);
    }
};   // !struct function_caller<CallingConvention::Stdcall>

template<>
struct function_caller<CallingConvention::Thiscall> {
    template<typename Ret, typename... Args>
    static inline Ret call(const memory_pointer& fn, Args... args) {
        return fn.cast<Ret(__thiscall*)(Args...)>()(std::forward<Args>(args)...);
    }
};   // !struct function_caller<CallingConvention::Thiscall>

template<>
struct function_caller<CallingConvention::Fastcall> {
    template<typename Ret, typename... Args>
    static inline Ret call(const memory_pointer& fn, Args... args) {
        return fn.cast<Ret(__fastcall*)(Args...)>()(std::forward<Args>(args)...);
    }
};   // !struct function_caller<CallingConvention::Fastcall>

inline uint32_t get_relative_address(const memory_pointer& to,
                                     const memory_pointer& from,
                                     const size_t          oplen = 5) {
    return (to.addressof() - from.addressof() - oplen);
}

inline uint32_t restore_absolute_address(const memory_pointer& imm,
                                         const memory_pointer& from,
                                         const size_t          oplen = 5) {
    return (imm.addressof() + from.addressof() + oplen);
}

inline uint32_t align_value(const uint32_t value, const uint32_t alignment) {
    const uint32_t remainder = (value % alignment);
    return (remainder == 0) ? (value) : (value - remainder + alignment);
}

#pragma pack(push, 1)
struct jmp_relative {
    uint8_t  opcode;
    uint32_t operand;
};
struct call_relative : public jmp_relative {};
struct jcc_relative {
    uint8_t  opcode;
    uint8_t  opcode2;
    uint32_t operand;
};
#pragma pack(pop)
}   // namespace detail

template<typename Ret, detail::CallingConvention Convention, typename... Args>
inline Ret call_function(const memory_pointer& fn, Args&&... args) {
    return detail::function_caller<Convention>::template call<Ret, Args...>(
        fn, std::forward<Args>(args)...);
}

template<typename Ret, typename... Args>
inline Ret call_cdecl(const memory_pointer& fn, Args&&... args) {
    return call_function<Ret, detail::CallingConvention::Cdecl>(
        fn, std::forward<Args>(args)...);
}

template<typename Ret, typename... Args>
inline Ret call_winapi(const memory_pointer& fn, Args&&... args) {
    return call_function<Ret, detail::CallingConvention::Stdcall>(
        fn, std::forward<Args>(args)...);
}

template<typename Ret, typename... Args>
inline Ret call_method(const memory_pointer& fn, Args&&... args) {
    return call_function<Ret, detail::CallingConvention::Thiscall>(
        fn, std::forward<Args>(args)...);
}

template<typename Ret, typename... Args>
inline Ret call_fast(const memory_pointer& fn, Args&&... args) {
    return call_function<Ret, detail::CallingConvention::Fastcall>(
        fn, std::forward<Args>(args)...);
}
}   // namespace memwrapper

#endif   // !MEMWRAPPER_DETAIL_HPP_
