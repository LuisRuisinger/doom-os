/* =================================================================================================
 * C++ x SIMD
 *
 * A 4x4 transform kernel written in SSE intrinsics from a C++ application. The kernel itself is
 * compiled -mno-sse -mno-80387 and never issues XMM instructions, so this example proves that
 * cpu::fpu_component made SSE legal before main() runs.
 *
 * This intentionally stays in the conservative C++ subset DoomOS can support today: no exceptions,
 * no RTTI, no iostreams, and no dynamic C++ allocation. The example still uses the C++ standard
 * library through freestanding headers such as <cstddef>, <cstdint>, and <type_traits>, while
 * newlib supplies printf, malloc, free, and snprintf.
 *
 * Results are printed a digit at a time rather than with %f. newlib here is configured
 * --disable-newlib-io-float, so the double formatting paths are not in the library at all.
 * ============================================================================================== */

#include <cstddef>
#include <cstdint>
#include <type_traits>

/* newlib's own headers rather than <cstdio>/<cstdlib>, which this toolchain does not have:
 * libstdc++ is built --disable-hosted-libstdcxx, so only the freestanding subset is installed.
 * The C headers are newlib's and are extern "C" guarded, so they are correct from C++ - and
 * getting the declarations from the library that defines them beats restating them here, where
 * nothing would check the restatement against the real prototypes. */
#include <stdio.h>
#include <stdlib.h>

#include <xmmintrin.h>

namespace {

constexpr std::size_t VECTOR_COUNT = 6;
constexpr float       TOLERANCE_EXACT = 0.0f;
constexpr float       TOLERANCE_RSQRT = 1.0e-3f;

struct vector4 {
    float lanes[4];
};

static_assert(std::is_trivially_copyable_v<vector4>);
static_assert(sizeof(vector4) == 4 * sizeof(float));
static_assert(alignof(vector4) == alignof(float));
static_assert(sizeof(std::uint32_t) == 4);

/* Column major, so a column is contiguous and a transform is four broadcasts and four multiplies
 * rather than four dot products. */
alignas(16) constexpr float TRANSFORM[16] = {
    2.0f, 0.0f, 0.0f, 0.0f, /* column 0 */
    0.0f, 4.0f, 0.0f, 0.0f, /* column 1 */
    0.0f, 0.0f, 8.0f, 0.0f, /* column 2 */
    3.0f, 5.0f, 7.0f, 1.0f, /* column 3, translation */
};

/* =================================================================================================
 * SSE
 * ============================================================================================== */

void transform_simd(const vector4 &v, vector4 &out)
{
    const __m128 vec = _mm_loadu_ps(v.lanes);

    const __m128 x = _mm_shuffle_ps(vec, vec, _MM_SHUFFLE(0, 0, 0, 0));
    const __m128 y = _mm_shuffle_ps(vec, vec, _MM_SHUFFLE(1, 1, 1, 1));
    const __m128 z = _mm_shuffle_ps(vec, vec, _MM_SHUFFLE(2, 2, 2, 2));
    const __m128 w = _mm_shuffle_ps(vec, vec, _MM_SHUFFLE(3, 3, 3, 3));

    __m128 result = _mm_mul_ps(x, _mm_load_ps(&TRANSFORM[0]));

    result = _mm_add_ps(result, _mm_mul_ps(y, _mm_load_ps(&TRANSFORM[4])));
    result = _mm_add_ps(result, _mm_mul_ps(z, _mm_load_ps(&TRANSFORM[8])));
    result = _mm_add_ps(result, _mm_mul_ps(w, _mm_load_ps(&TRANSFORM[12])));

    _mm_storeu_ps(out.lanes, result);
}

__m128 horizontal_sum(__m128 lanes)
{
    lanes = _mm_add_ps(lanes, _mm_shuffle_ps(lanes, lanes, _MM_SHUFFLE(1, 0, 3, 2)));

    return _mm_add_ps(lanes, _mm_shuffle_ps(lanes, lanes, _MM_SHUFFLE(2, 3, 0, 1)));
}

float dot_simd(const vector4 &a, const vector4 &b)
{
    return _mm_cvtss_f32(horizontal_sum(_mm_mul_ps(_mm_loadu_ps(a.lanes), _mm_loadu_ps(b.lanes))));
}

void normalize_simd(const vector4 &v, vector4 &out)
{
    const __m128 vec = _mm_loadu_ps(v.lanes);
    const __m128 length_squared = horizontal_sum(_mm_mul_ps(vec, vec));

    _mm_storeu_ps(out.lanes, _mm_mul_ps(vec, _mm_rsqrt_ps(length_squared)));
}

/* =================================================================================================
 * Scalar references
 * ============================================================================================== */

void transform_scalar(const vector4 &v, vector4 &out)
{
    for (std::size_t row = 0; row < 4; ++row) {
        out.lanes[row] = TRANSFORM[row] * v.lanes[0] + TRANSFORM[4 + row] * v.lanes[1] +
                         TRANSFORM[8 + row] * v.lanes[2] + TRANSFORM[12 + row] * v.lanes[3];
    }
}

float dot_scalar(const vector4 &a, const vector4 &b)
{
    return a.lanes[0] * b.lanes[0] + a.lanes[1] * b.lanes[1] + a.lanes[2] * b.lanes[2] +
           a.lanes[3] * b.lanes[3];
}

bool close_enough(float lhs, float rhs, float tolerance)
{
    const float difference = lhs - rhs;

    return (difference < 0.0f ? -difference : difference) <= tolerance;
}

/* =================================================================================================
 * Printing
 * ============================================================================================== */

const char *fixed(char *buffer, std::size_t size, float value)
{
    const int scaled = static_cast<int>(value * 10.0f + (value < 0.0f ? -0.5f : 0.5f));
    const int magnitude = scaled < 0 ? -scaled : scaled;

    snprintf(buffer, size, "%s%d.%d", scaled < 0 ? "-" : "", magnitude / 10, magnitude % 10);

    return buffer;
}

void print_vector(const vector4 &v)
{
    char buffer[16];

    printf("(");

    for (std::size_t lane = 0; lane < 4; ++lane) {
        printf("%s%6s", lane == 0 ? "" : " ", fixed(buffer, sizeof buffer, v.lanes[lane]));
    }

    printf(")");
}

int failures = 0;
int checks = 0;

void record(bool ok)
{
    ++checks;

    if (!ok) {
        ++failures;
    }
}

void check(bool ok, const char *what)
{
    record(ok);

    printf("  %-28s %s\n", what, ok ? "ok" : "FAILED");
}

}  // namespace

int main()
{
    /* On the heap, so the SIMD code reads and writes memory that came from newlib malloc, through
     * the DoomOS _sbrk shim, rather than from the image. */
    auto *vectors = static_cast<vector4 *>(malloc(VECTOR_COUNT * sizeof(vector4)));

    if (vectors == nullptr) {
        printf("c++ simd: malloc failed, no room for %zu vectors\n", VECTOR_COUNT);
        return 1;
    }

    for (std::size_t i = 0; i < VECTOR_COUNT; ++i) {
        vectors[i].lanes[0] = static_cast<float>(i + 1);
        vectors[i].lanes[1] = static_cast<float>(static_cast<int>(i * 2) - 3);
        vectors[i].lanes[2] = 0.5f * static_cast<float>(i + 2);
        vectors[i].lanes[3] = 1.0f;
    }

    printf("c++ simd: 4x4 transform, SSE intrinsics against scalar C++\n");

    for (std::size_t i = 0; i < VECTOR_COUNT; ++i) {
        vector4 packed{};
        vector4 scalar{};

        transform_simd(vectors[i], packed);
        transform_scalar(vectors[i], scalar);

        bool lanes_match = true;

        for (std::size_t lane = 0; lane < 4; ++lane) {
            if (!close_enough(packed.lanes[lane], scalar.lanes[lane], TOLERANCE_EXACT)) {
                lanes_match = false;
            }
        }

        printf("  ");
        print_vector(vectors[i]);
        printf(" -> ");
        print_vector(packed);
        printf("  %s\n", lanes_match ? "ok" : "FAILED");

        record(lanes_match);
    }

    printf("c++ simd: reductions\n");

    check(close_enough(dot_simd(vectors[0], vectors[1]), dot_scalar(vectors[0], vectors[1]),
                       TOLERANCE_EXACT),
          "dot product");

    bool units_ok = true;

    for (std::size_t i = 0; i < VECTOR_COUNT; ++i) {
        vector4 unit{};

        normalize_simd(vectors[i], unit);

        if (!close_enough(dot_scalar(unit, unit), 1.0f, TOLERANCE_RSQRT)) {
            units_ok = false;
        }
    }

    check(units_ok, "rsqrtps normalise");

    free(vectors);

    printf("c++ simd: %d checks, %d failed\n", checks, failures);

    return failures == 0 ? 0 : 1;
}
