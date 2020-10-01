
#ifndef HORSE64_NONLOCALE_H_
#define HORSE64_NONLOCALE_H_

#include <locale.h>

extern locale_t h64locale;

extern float strtof_l(const char * __restrict, char **__restrict, locale_t);
extern long long int strtoll_l(const char * __restrict, char **__restrict, int, locale_t);
extern double strtod_l(const char * __restrict, char **__restrict, locale_t);

static inline long long int h64strtoll(const char *s, char **endptr, int base) {
    return strtoll_l(s, endptr, base, h64locale);
}

static inline double h64atof(const char *s) {
    return strtod_l(s, NULL, h64locale);
}

static inline long long int h64atoll(const char *s) {
    return strtoll_l(s, NULL, 10, h64locale);
}

#endif  // HORSE64_NONLOCALE_H_
