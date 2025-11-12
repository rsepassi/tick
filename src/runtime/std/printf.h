/**
 * @author (c) Eyal Rozenberg <eyalroz1@gmx.com>
 *             2021-2024, Haifa, Palestine/Israel
 * @author (c) Marco Paland (info@paland.com)
 *             2014-2019, PALANDesign Hannover, Germany
 *
 * @note Others have made smaller contributions to this file: see the
 * contributors page at https://github.com/eyalroz/printf/graphs/contributors
 * or ask one of the authors.
 *
 * @brief Small stand-alone implementation of the printf family of functions
 * (`(v)printf`, `(v)s(n)printf` etc., geared towards use on embedded systems
 * with a very limited resources.
 *
 * @note the implementations are thread-safe; re-entrant; use no functions from
 * the standard library; and do not dynamically allocate any memory.
 *
 * @license The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef PRINTF_H_
#define PRINTF_H_

#include <stdarg.h>
#include <stddef.h>

/**
 * An implementation of the C standard's vsnprintf
 *
 * @param s An array in which to store the formatted string. It must be large
 *     enough to fit either the entire formatted output, or at least @p n
 *     characters. Alternatively, it can be NULL, in which case nothing will
 *     be printed, and only the number of characters which _could_ have been
 *     printed is tallied and returned.
 * @param n The maximum number of characters to write to the array, including
 *     a terminating null character
 * @param format A string specifying the format of the output, with %-marked
 *     specifiers of how to interpret additional arguments.
 * @param arg Additional arguments to the function, one for each specifier in
 *     @p format
 * @return The number of characters that COULD have been written into @p s, not
 *     counting the terminating null character. A value equal or larger than
 *     @p n indicates truncation. Only when the returned value is non-negative
 *     and less than @p n, the null-terminated string has been fully and
 *     successfully printed.
 */
/* @{ */
int vsnprintf_(char* s, size_t count, const char* format, va_list arg);
/* @} */

/**
 * vprintf with user-specified output function
 *
 * An alternative to @ref printf_, in which the output function is specified
 * dynamically (rather than @ref putchar_ being used)
 *
 * @param out An output function which takes one character and a type-erased
 *     additional parameters
 * @param extra_arg The type-erased argument to pass to the output function @p
 *     out with each call
 * @param format A string specifying the format of the output, with %-marked
 *     specifiers of how to interpret additional arguments.
 * @param arg Additional arguments to the function, one for each specifier in
 *     @p format
 * @return The number of characters for which the output function was invoked,
 *     not counting the terminating null character
 *
 */
int vfctprintf(void (*out)(char c, void* extra_arg), void* extra_arg, const char* format, va_list arg);

#endif  /* PRINTF_H_ */
