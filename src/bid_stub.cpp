// BID (Binary Integer Decimal) stub implementation for ARM64 macOS
//
// SIMPLIFIED APPROACH:
// Intel's libbid is x86-only, so we skip BID64 format entirely.
// Instead, we store double directly in uint64_t using union.
//
// This works because:
// 1. TWS sends position as STRING over socket (e.g. "130")
// 2. __bid64_from_string: "130" -> double 130.0 -> store in uint64
// 3. __bid64_to_binary64: extract double from uint64 -> 130.0
// 4. __bid64_to_string: 130.0 -> "130.000000"
//
// We lose some precision for very large numbers, but for stock quantities
// (typically < 1 million shares), double precision (15-17 digits) is sufficient.

#include <string>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <cstring>

extern "C" {

// BID64 is typedef'd as uint64_t in TWS API
typedef unsigned long long UINT64;

// Signature for __bid64_from_string changed - it returns UINT64 value
// But we have 2 variants in different TWS API versions, so support both

// Convert double to "BID64" (simplified: just store double in uint64)
UINT64 __binary64_to_bid64(double x, unsigned int rounding_mode, unsigned int* flags) {
    union { double d; UINT64 i; } u;
    u.d = x;
    if (flags) *flags = 0;
    return u.i;
}

// Convert "BID64" to double (simplified: extract double from uint64)
double __bid64_to_binary64(UINT64 x, unsigned int rounding_mode, unsigned int* flags) {
    union { UINT64 i; double d; } u;
    u.i = x;
    if (flags) *flags = 0;
    return u.d;
}

// Convert string to "BID64" - this is the KEY function called by TWS API
UINT64 __bid64_from_string(char* ps, unsigned int rounding_mode, unsigned int* flags) {
    if (!ps) {
        if (flags) *flags = 0x01; // Invalid flag
        return 0;
    }

    try {
        double val = std::stod(ps);
        if (flags) *flags = 0;
        return __binary64_to_bid64(val, 0, nullptr);
    } catch (...) {
        if (flags) *flags = 0x01; // Invalid flag
        return 0;
    }
}

// Note: Legacy void __bid64_from_string(char*, UINT64*) removed
// because C extern "C" doesn't support overloading.
// TWS API uses the UINT64 return version.

// Convert BID64 to string
void __bid64_to_string(char* ps, UINT64 x, unsigned int* flags) {
    if (ps) {
        double val = __bid64_to_binary64(x, 0, nullptr);
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6) << val;
        std::strcpy(ps, oss.str().c_str());
        if (flags) *flags = 0;
    }
}

// BID64 arithmetic operations with proper signatures
UINT64 __bid64_add(UINT64 x, UINT64 y, unsigned int rounding_mode, unsigned int* flags) {
    double a = __bid64_to_binary64(x, 0, nullptr);
    double b = __bid64_to_binary64(y, 0, nullptr);
    return __binary64_to_bid64(a + b, 0, nullptr);
}

UINT64 __bid64_sub(UINT64 x, UINT64 y, unsigned int rounding_mode, unsigned int* flags) {
    double a = __bid64_to_binary64(x, 0, nullptr);
    double b = __bid64_to_binary64(y, 0, nullptr);
    return __binary64_to_bid64(a - b, 0, nullptr);
}

UINT64 __bid64_mul(UINT64 x, UINT64 y, unsigned int rounding_mode, unsigned int* flags) {
    double a = __bid64_to_binary64(x, 0, nullptr);
    double b = __bid64_to_binary64(y, 0, nullptr);
    return __binary64_to_bid64(a * b, 0, nullptr);
}

UINT64 __bid64_div(UINT64 x, UINT64 y, unsigned int rounding_mode, unsigned int* flags) {
    double a = __bid64_to_binary64(x, 0, nullptr);
    double b = __bid64_to_binary64(y, 0, nullptr);
    if (b == 0.0) {
        if (flags) *flags = 0x02; // Division by zero
        return 0;
    }
    return __binary64_to_bid64(a / b, 0, nullptr);
}

} // extern "C"