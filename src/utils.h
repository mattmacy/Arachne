#pragma once
namespace Arachne {
// A macro to disallow the copy constructor and operator= functions
#ifndef DISALLOW_COPY_AND_ASSIGN
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&) = delete;    \
    TypeName& operator=(const TypeName&) = delete;
#endif

#define CACHE_LINE_SIZE 64
#define PAGE_SIZE 4096

// The following macros issue hints to the compiler that a particular branch is
// more likely than another.
#ifndef likely
#ifdef __GNUC__
// Note that the double negation here is used for coercion to the boolean type.
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif
#endif

}
