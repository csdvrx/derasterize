/* Just in case uchar.h is missing, as in some msys2 */

#if !(defined(__cplusplus) && defined(__GXX_EXPERIMENTAL_CXX0X__)) ||   \
    !defined(__GNUC__) ||                       \
    (__GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 4))
typedef uint_least16_t char16_t;
typedef uint_least32_t char32_t;
#endif
