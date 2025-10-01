// BID (Binary Integer Decimal) stub implementation for ARM64 macOS
// This is a workaround for development - TWS API uses Intel's libbid which is x86 only
// For production, use proper decimal library or compile libbid for ARM

#include <string>
#include <sstream>
#include <iomanip>

extern "C" {

// BID64 is stored as uint64_t
typedef unsigned long long UINT64;

// Convert double to BID64 (stub: just reinterpret as uint64)
UINT64 __binary64_to_bid64(double x) {
    union { double d; UINT64 i; } u;
    u.d = x;
    return u.i;
}

// Convert BID64 to double (stub: just reinterpret as double)
double __bid64_to_binary64(UINT64 x) {
    union { UINT64 i; double d; } u;
    u.i = x;
    return u.d;
}

// Convert string to BID64
void __bid64_from_string(char* ps, UINT64* px) {
    if (ps && px) {
        double val = std::stod(ps);
        *px = __binary64_to_bid64(val);
    }
}

// Convert BID64 to string
void __bid64_to_string(char* ps, UINT64 x) {
    if (ps) {
        double val = __bid64_to_binary64(x);
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6) << val;
        std::strcpy(ps, oss.str().c_str());
    }
}

// BID64 arithmetic operations
UINT64 __bid64_add(UINT64 x, UINT64 y) {
    return __binary64_to_bid64(__bid64_to_binary64(x) + __bid64_to_binary64(y));
}

UINT64 __bid64_sub(UINT64 x, UINT64 y) {
    return __binary64_to_bid64(__bid64_to_binary64(x) - __bid64_to_binary64(y));
}

UINT64 __bid64_mul(UINT64 x, UINT64 y) {
    return __binary64_to_bid64(__bid64_to_binary64(x) * __bid64_to_binary64(y));
}

UINT64 __bid64_div(UINT64 x, UINT64 y) {
    return __binary64_to_bid64(__bid64_to_binary64(x) / __bid64_to_binary64(y));
}

} // extern "C"