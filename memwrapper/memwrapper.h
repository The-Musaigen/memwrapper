#ifndef MEMWRAPPER_H_
#define MEMWRAPPER_H_

// version
#define MW_VERSION 102
#define MW_VERSION_STR "1.0.2"

// preprocessor
#if defined(_WIN32) && !defined(_WIN64)
#define MW_WIN_X86
#endif   // defined(_WIN32) && !defined(_WIN64)

#if !defined(MW_WIN_X86)
#error "only win86 supported."
#endif

#if defined(_MSVC_LANG)
#define MW_CPP _MSVC_LANG
#else
#define MW_CPP __cplusplus
#endif   // defined(_MSVC_LANG)

#if !(MW_CPP >= 201703L)
#error "only c++17 and newer."
#endif   // !(MW_CPP >= 201703L)

#if defined(MW_WIN_X86)
#include <Windows.h>
#endif   // defined(MW_WIN_X86)

#include <cstdint>
#include <string>
#include <vector>
#include <type_traits>

#if defined(MW_WIN_X86)
#include "hde/hde32.h"

#include "x86/memwrapper_basic.hpp"
#include "x86/memwrapper_llmo.hpp"
#include "x86/memwrapper_detail.hpp"
#include "x86/memwrapper_allocator.hpp"
#include "x86/memwrapper_hook.hpp"
#endif   // defined(MW_WIN_X86)

#endif   // !MEMWRAPPER_H_
