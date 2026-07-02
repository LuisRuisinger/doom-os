#ifndef DOOM_OS_KERNEL_CORE_TYPE_TRAITS_HPP_
#define DOOM_OS_KERNEL_CORE_TYPE_TRAITS_HPP_

namespace kernel::core {

// =================================================================================================
// integral_constant
// =================================================================================================

template <typename T, T Value>
struct integral_constant {
    static constexpr T value = Value;

    using value_type = T;
    using type = integral_constant<T, Value>;

    constexpr operator value_type() const { return value; }

    constexpr value_type operator()() const { return value; }
};

using true_type = integral_constant<bool, true>;
using false_type = integral_constant<bool, false>;

// =================================================================================================
// is_same
// =================================================================================================

template <typename A, typename B>
struct is_same : false_type {};

template <typename T>
struct is_same<T, T> : true_type {};

template <typename A, typename B>
inline constexpr bool is_same_v = is_same<A, B>::value;

// =================================================================================================
// enable_if_t
// =================================================================================================

template <bool Condition, typename T = void>
struct enable_if {};

template <typename T>
struct enable_if<true, T> {
    using type = T;
};

template <bool Pred, typename T = void>
using enable_if_t = typename enable_if<Pred, T>::type;

// =================================================================================================
// Void type
// =================================================================================================

template <typename...>
using void_T = void;

// =================================================================================================
// Is default constructible
// =================================================================================================

namespace detail {

template <typename T, typename = void>
struct is_default_constructible_impl : false_type {};

template <typename T>
struct is_default_constructible_impl<T, void_T<decltype(T())>> : true_type {};

}  // namespace detail

template <typename T>
struct is_default_constructible : detail::is_default_constructible_impl<T> {};

template <typename T>
inline constexpr bool is_default_constructible_v = is_default_constructible<T>::value;

// =================================================================================================
// Conditional
// =================================================================================================

template <bool Condition, typename TrueType, typename FalseType>
struct conditional {
    using type = TrueType;
};

template <typename TrueType, typename FalseType>
struct conditional<false, TrueType, FalseType> {
    using type = FalseType;
};

template <bool Condition, typename TrueType, typename FalseType>
using conditional_t = typename conditional<Condition, TrueType, FalseType>::type;

// =================================================================================================
// Index sequence
// =================================================================================================

template <usize... Is>
struct index_sequence {
    static constexpr usize size() { return sizeof...(Is); }
};

namespace detail {

template <typename Lhs, typename Rhs>
struct index_sequence_concat;

template <usize... Lhs, usize... Rhs>
struct index_sequence_concat<index_sequence<Lhs...>, index_sequence<Rhs...>> {
    using type = index_sequence<Lhs..., (sizeof...(Lhs) + Rhs)...>;
};

template <usize N>
struct make_index_sequence_impl {
   private:
    using left = typename make_index_sequence_impl<N / 2>::type;
    using right = typename make_index_sequence_impl<N - N / 2>::type;

   public:
    using type = typename index_sequence_concat<left, right>::type;
};

template <>
struct make_index_sequence_impl<0> {
    using type = index_sequence<>;
};

template <>
struct make_index_sequence_impl<1> {
    using type = index_sequence<0>;
};

}  // namespace detail

template <usize N>
using make_index_sequence = typename detail::make_index_sequence_impl<N>::type;

// =================================================================================================
// remove_reference
// =================================================================================================

template <typename T>
struct remove_reference {
    using type = T;
};

template <typename T>
struct remove_reference<T &> {
    using type = T;
};

template <typename T>
struct remove_reference<T &&> {
    using type = T;
};

template <typename T>
using remove_reference_t = typename remove_reference<T>::type;

// =================================================================================================
// remove_const / remove_volatile / remove_cv
// =================================================================================================

template <typename T>
struct remove_const {
    using type = T;
};

template <typename T>
struct remove_const<const T> {
    using type = T;
};

template <typename T>
using remove_const_t = typename remove_const<T>::type;

template <typename T>
struct remove_volatile {
    using type = T;
};

template <typename T>
struct remove_volatile<volatile T> {
    using type = T;
};

template <typename T>
using remove_volatile_t = typename remove_volatile<T>::type;

template <typename T>
struct remove_cv {
    using type = remove_volatile_t<remove_const_t<T>>;
};

template <typename T>
using remove_cv_t = typename remove_cv<T>::type;

template <typename T>
struct remove_cvref {
    using type = remove_cv_t<remove_reference_t<T>>;
};

template <typename T>
using remove_cvref_t = typename remove_cvref<T>::type;

// =================================================================================================
// cv/ref predicates
// =================================================================================================

template <typename T>
struct is_const : false_type {};

template <typename T>
struct is_const<const T> : true_type {};

template <typename T>
inline constexpr bool is_const_v = is_const<T>::value;

template <typename T>
struct is_volatile : false_type {};

template <typename T>
struct is_volatile<volatile T> : true_type {};

template <typename T>
inline constexpr bool is_volatile_v = is_volatile<T>::value;

template <typename T>
struct is_lvalue_reference : false_type {};

template <typename T>
struct is_lvalue_reference<T &> : true_type {};

template <typename T>
inline constexpr bool is_lvalue_reference_v = is_lvalue_reference<T>::value;

template <typename T>
struct is_rvalue_reference : false_type {};

template <typename T>
struct is_rvalue_reference<T &&> : true_type {};

template <typename T>
inline constexpr bool is_rvalue_reference_v = is_rvalue_reference<T>::value;

// =================================================================================================
// Integer classification
// =================================================================================================
//
// These are intentionally narrower than std::is_integral/std::is_signed.
// They classify numeric integer types suitable for integer formatting.
// bool and plain char are intentionally excluded.

template <typename T>
struct is_signed_integer_base : false_type {};

template <>
struct is_signed_integer_base<signed char> : true_type {};

template <>
struct is_signed_integer_base<short> : true_type {};

template <>
struct is_signed_integer_base<int> : true_type {};

template <>
struct is_signed_integer_base<long> : true_type {};

template <>
struct is_signed_integer_base<long long> : true_type {};

template <typename T>
struct is_signed_integer : is_signed_integer_base<remove_cvref_t<T>> {};

template <typename T>
inline constexpr bool is_signed_integer_v = is_signed_integer<T>::value;

template <typename T>
struct is_unsigned_integer_base : false_type {};

template <>
struct is_unsigned_integer_base<unsigned char> : true_type {};

template <>
struct is_unsigned_integer_base<unsigned short> : true_type {};

template <>
struct is_unsigned_integer_base<unsigned int> : true_type {};

template <>
struct is_unsigned_integer_base<unsigned long> : true_type {};

template <>
struct is_unsigned_integer_base<unsigned long long> : true_type {};

template <typename T>
struct is_unsigned_integer : is_unsigned_integer_base<remove_cvref_t<T>> {};

template <typename T>
inline constexpr bool is_unsigned_integer_v = is_unsigned_integer<T>::value;

template <typename T>
struct is_integer : integral_constant<bool, is_signed_integer_v<T> || is_unsigned_integer_v<T>> {};

template <typename T>
inline constexpr bool is_integer_v = is_integer<T>::value;

// =================================================================================================
// Pointers
// =================================================================================================

template <typename T>
struct is_pointer_base : false_type {};

template <typename T>
struct is_pointer_base<T *> : true_type {};

template <typename T>
struct is_pointer : is_pointer_base<remove_cv_t<T>> {};

template <typename T>
inline constexpr bool is_pointer_v = is_pointer<T>::value;

// =================================================================================================
// Tests
// =================================================================================================

static_assert(true_type::value);
static_assert(!false_type::value);

static_assert(integral_constant<int, 42>::value == 42);
static_assert(integral_constant<bool, true>{});
static_assert(!integral_constant<bool, false>{});

// =================================================================================================
// is_same
// =================================================================================================

static_assert(is_same_v<int, int>);
static_assert(!is_same_v<int, const int>);
static_assert(!is_same_v<int, unsigned int>);
static_assert(!is_same_v<int, long>);

// =================================================================================================
// remove_reference
// =================================================================================================

static_assert(is_same_v<remove_reference_t<int>, int>);
static_assert(is_same_v<remove_reference_t<int &>, int>);
static_assert(is_same_v<remove_reference_t<int &&>, int>);

static_assert(is_same_v<remove_reference_t<const int &>, const int>);
static_assert(is_same_v<remove_reference_t<volatile int &&>, volatile int>);
static_assert(is_same_v<remove_reference_t<const volatile int &>, const volatile int>);

using type_traits_test_array = int[4];

static_assert(is_same_v<remove_reference_t<type_traits_test_array &>, type_traits_test_array>);
static_assert(is_same_v<remove_reference_t<type_traits_test_array &&>, type_traits_test_array>);

// =================================================================================================
// remove_const
// =================================================================================================

static_assert(is_same_v<remove_const_t<int>, int>);
static_assert(is_same_v<remove_const_t<const int>, int>);
static_assert(is_same_v<remove_const_t<volatile int>, volatile int>);
static_assert(is_same_v<remove_const_t<const volatile int>, volatile int>);

static_assert(is_same_v<remove_const_t<const int *>, const int *>);
static_assert(is_same_v<remove_const_t<int *const>, int *>);
static_assert(is_same_v<remove_const_t<const int *const>, const int *>);

// =================================================================================================
// remove_volatile
// =================================================================================================

static_assert(is_same_v<remove_volatile_t<int>, int>);
static_assert(is_same_v<remove_volatile_t<volatile int>, int>);
static_assert(is_same_v<remove_volatile_t<const int>, const int>);
static_assert(is_same_v<remove_volatile_t<const volatile int>, const int>);

static_assert(is_same_v<remove_volatile_t<volatile int *>, volatile int *>);
static_assert(is_same_v<remove_volatile_t<int *volatile>, int *>);
static_assert(is_same_v<remove_volatile_t<volatile int *volatile>, volatile int *>);

// =================================================================================================
// remove_cv
// =================================================================================================

static_assert(is_same_v<remove_cv_t<int>, int>);
static_assert(is_same_v<remove_cv_t<const int>, int>);
static_assert(is_same_v<remove_cv_t<volatile int>, int>);
static_assert(is_same_v<remove_cv_t<const volatile int>, int>);

static_assert(is_same_v<remove_cv_t<const int *>, const int *>);
static_assert(is_same_v<remove_cv_t<volatile int *>, volatile int *>);
static_assert(is_same_v<remove_cv_t<const volatile int *>, const volatile int *>);

static_assert(is_same_v<remove_cv_t<int *const>, int *>);
static_assert(is_same_v<remove_cv_t<int *volatile>, int *>);
static_assert(is_same_v<remove_cv_t<int *const volatile>, int *>);
static_assert(is_same_v<remove_cv_t<const int *const volatile>, const int *>);

// =================================================================================================
// remove_cvref
// =================================================================================================

static_assert(is_same_v<remove_cvref_t<int>, int>);
static_assert(is_same_v<remove_cvref_t<const int>, int>);
static_assert(is_same_v<remove_cvref_t<volatile int>, int>);
static_assert(is_same_v<remove_cvref_t<const volatile int>, int>);

static_assert(is_same_v<remove_cvref_t<int &>, int>);
static_assert(is_same_v<remove_cvref_t<const int &>, int>);
static_assert(is_same_v<remove_cvref_t<volatile int &>, int>);
static_assert(is_same_v<remove_cvref_t<const volatile int &>, int>);

static_assert(is_same_v<remove_cvref_t<int &&>, int>);
static_assert(is_same_v<remove_cvref_t<const int &&>, int>);
static_assert(is_same_v<remove_cvref_t<volatile int &&>, int>);
static_assert(is_same_v<remove_cvref_t<const volatile int &&>, int>);

static_assert(is_same_v<remove_cvref_t<const int *>, const int *>);
static_assert(is_same_v<remove_cvref_t<const int *const &>, const int *>);
static_assert(
    is_same_v<remove_cvref_t<const volatile int *const volatile &>, const volatile int *>);

// =================================================================================================
// is_const / is_volatile
// =================================================================================================

static_assert(is_const_v<const int>);
static_assert(!is_const_v<int>);
static_assert(!is_const_v<volatile int>);
static_assert(is_const_v<const volatile int>);

static_assert(!is_const_v<const int &>);
static_assert(!is_const_v<const int &&>);

static_assert(!is_const_v<const int *>);
static_assert(is_const_v<int *const>);
static_assert(is_const_v<const int *const>);

static_assert(is_volatile_v<volatile int>);
static_assert(!is_volatile_v<int>);
static_assert(!is_volatile_v<const int>);
static_assert(is_volatile_v<const volatile int>);

static_assert(!is_volatile_v<volatile int &>);
static_assert(!is_volatile_v<volatile int &&>);

static_assert(!is_volatile_v<volatile int *>);
static_assert(is_volatile_v<int *volatile>);
static_assert(is_volatile_v<volatile int *volatile>);

// =================================================================================================
// references
// =================================================================================================

static_assert(is_lvalue_reference_v<int &>);
static_assert(is_lvalue_reference_v<const int &>);
static_assert(!is_lvalue_reference_v<int>);
static_assert(!is_lvalue_reference_v<int &&>);

static_assert(is_rvalue_reference_v<int &&>);
static_assert(is_rvalue_reference_v<const int &&>);
static_assert(!is_rvalue_reference_v<int>);
static_assert(!is_rvalue_reference_v<int &>);

// =================================================================================================
// signed integer classification
// =================================================================================================

static_assert(is_signed_integer_v<signed char>);
static_assert(is_signed_integer_v<short>);
static_assert(is_signed_integer_v<int>);
static_assert(is_signed_integer_v<long>);
static_assert(is_signed_integer_v<long long>);

static_assert(is_signed_integer_v<const int>);
static_assert(is_signed_integer_v<volatile int>);
static_assert(is_signed_integer_v<const volatile int>);
static_assert(is_signed_integer_v<int &>);
static_assert(is_signed_integer_v<const int &>);
static_assert(is_signed_integer_v<int &&>);

static_assert(!is_signed_integer_v<unsigned char>);
static_assert(!is_signed_integer_v<unsigned short>);
static_assert(!is_signed_integer_v<unsigned int>);
static_assert(!is_signed_integer_v<unsigned long>);
static_assert(!is_signed_integer_v<unsigned long long>);

static_assert(!is_signed_integer_v<bool>);
static_assert(!is_signed_integer_v<char>);
static_assert(!is_signed_integer_v<void>);
static_assert(!is_signed_integer_v<int *>);

// =================================================================================================
// unsigned integer classification
// =================================================================================================

static_assert(is_unsigned_integer_v<unsigned char>);
static_assert(is_unsigned_integer_v<unsigned short>);
static_assert(is_unsigned_integer_v<unsigned int>);
static_assert(is_unsigned_integer_v<unsigned long>);
static_assert(is_unsigned_integer_v<unsigned long long>);

static_assert(is_unsigned_integer_v<const unsigned int>);
static_assert(is_unsigned_integer_v<volatile unsigned int>);
static_assert(is_unsigned_integer_v<const volatile unsigned int>);
static_assert(is_unsigned_integer_v<unsigned int &>);
static_assert(is_unsigned_integer_v<const unsigned int &>);
static_assert(is_unsigned_integer_v<unsigned int &&>);

static_assert(!is_unsigned_integer_v<signed char>);
static_assert(!is_unsigned_integer_v<short>);
static_assert(!is_unsigned_integer_v<int>);
static_assert(!is_unsigned_integer_v<long>);
static_assert(!is_unsigned_integer_v<long long>);

static_assert(!is_unsigned_integer_v<bool>);
static_assert(!is_unsigned_integer_v<char>);
static_assert(!is_unsigned_integer_v<void>);
static_assert(!is_unsigned_integer_v<unsigned int *>);

// =================================================================================================
// integer classification
// =================================================================================================

static_assert(is_integer_v<signed char>);
static_assert(is_integer_v<unsigned char>);
static_assert(is_integer_v<short>);
static_assert(is_integer_v<unsigned short>);
static_assert(is_integer_v<int>);
static_assert(is_integer_v<unsigned int>);
static_assert(is_integer_v<long>);
static_assert(is_integer_v<unsigned long>);
static_assert(is_integer_v<long long>);
static_assert(is_integer_v<unsigned long long>);

static_assert(is_integer_v<const int &>);
static_assert(is_integer_v<const unsigned int &>);

static_assert(!is_integer_v<bool>);
static_assert(!is_integer_v<char>);
static_assert(!is_integer_v<void>);
static_assert(!is_integer_v<int *>);

// =================================================================================================
// pointers
// =================================================================================================

static_assert(is_pointer_v<int *>);
static_assert(is_pointer_v<const int *>);
static_assert(is_pointer_v<volatile int *>);
static_assert(is_pointer_v<const volatile int *>);

static_assert(is_pointer_v<int *const>);
static_assert(is_pointer_v<int *volatile>);
static_assert(is_pointer_v<int *const volatile>);

static_assert(is_pointer_v<const int *const>);
static_assert(is_pointer_v<volatile int *volatile>);
static_assert(is_pointer_v<const volatile int *const volatile>);

static_assert(!is_pointer_v<int>);
static_assert(!is_pointer_v<int &>);
static_assert(!is_pointer_v<int &&>);
static_assert(!is_pointer_v<void>);

using type_traits_test_function = void();

static_assert(is_pointer_v<type_traits_test_function *>);
static_assert(!is_pointer_v<type_traits_test_function>);
}  // namespace kernel::core

#endif  // DOOM_OS_KERNEL_CORE_TYPE_TRAITS_HPP_