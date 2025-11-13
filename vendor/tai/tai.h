#ifndef TAI_H
#define TAI_H

#include <stdint.h>

/* Use standard C99 uint64_t instead of custom typedef */
typedef uint64_t uint64;

/*
 * tai - manipulate times with 1-second precision
 *
 * A struct tai stores an integer between 0 inclusive and 2^64 exclusive.
 * The format of struct tai is designed to speed up common operations;
 * applications should not look inside struct tai.
 *
 * A struct tai variable is commonly used to store a TAI64 label. Each
 * TAI64 label refers to one second of real time. TAI64 labels span a period
 * of hundreds of billions of years. See http://pobox.com/~djb/proto/tai64.txt
 * for more information.
 *
 * A struct tai variable may also be used to store the numerical difference
 * between two TAI64 labels.
 */
struct tai {
  uint64 x;
};

/*
 * tai_approx - returns a double-precision approximation to t. The result
 * of tai_approx is always nonnegative.
 */
#define tai_approx(t) ((double) ((t)->x))

/*
 * tai_less - returns 1 if a is smaller than b, 0 otherwise.
 */
#define tai_less(t,u) ((t)->x < (u)->x)

/*
 * tai_add - adds a and b modulo 2^64 and puts the result into t. The
 * inputs and outputs may overlap.
 */
void tai_add(struct tai *t, struct tai *a, struct tai *b);

/*
 * tai_sub - subtracts b from a modulo 2^64 and puts the result into t.
 * The inputs and outputs may overlap.
 */
void tai_sub(struct tai *t, struct tai *a, struct tai *b);

/*
 * tai_now - get current time, with 1-second precision
 *
 * tai_now puts the current time into t.
 *
 * More precisely: tai_now puts into t its best guess as to the TAI64 label
 * for the 1-second interval that contains the current time.
 *
 * This implementation takes the current time as an argument (seconds_since_epoch)
 * rather than calling time(). The caller should provide the number of TAI seconds
 * since 1970-01-01 00:00:10 TAI. For Unix time, this is typically:
 *   unix_time + 10 (for dates before first leap second)
 * or the appropriate offset based on leap seconds at that time.
 */
void tai_now(struct tai *t, uint64 seconds_since_epoch);

/*
 * tai_pack - convert TAI64 labels to external format
 *
 * tai_pack converts a TAI64 label from internal format in t to TAI64 format
 * in buf. TAI_PACK is 8.
 *
 * See http://pobox.com/~djb/proto/tai64.txt for more information about
 * TAI64 format.
 */
#define TAI_PACK 8
void tai_pack(char *buf, struct tai *t);

/*
 * tai_unpack - convert TAI64 labels from external format
 *
 * tai_unpack converts a TAI64 label from TAI64 format in buf to internal
 * format in t.
 */
void tai_unpack(char *buf, struct tai *t);


/*
 * taia - manipulate times with 1-attosecond precision
 *
 * A struct taia stores an integer between 0 inclusive and 2^64x10^18 exclusive.
 * The format of struct taia is designed to speed up common operations;
 * applications should not look inside struct taia.
 *
 * A struct taia variable is commonly used to store a TAI64NA label. Each
 * TAI64NA label refers to one attosecond of real time; see
 * http://pobox.com/~djb/proto/tai64.txt for more information. The integer
 * in the struct taia is 10^18 times the second count, plus 10^9 times the
 * nanosecond count, plus the attosecond count.
 *
 * A struct taia variable may also be used to store the numerical difference
 * between two TAI64NA labels.
 */
struct taia {
  struct tai sec;
  unsigned long nano; /* 0...999999999 */
  unsigned long atto; /* 0...999999999 */
};

/*
 * taia_tai - places into sec the integer part of t/10^18.
 */
void taia_tai(struct taia *ta, struct tai *t);

/*
 * taia_now - get current time, with 1-attosecond precision
 *
 * taia_now puts the current time into t.
 *
 * More precisely: taia_now puts into t its best guess as to the TAI64NA
 * label for the 1-attosecond interval that contains the current time.
 *
 * This implementation takes the current time as arguments (seconds_since_epoch
 * and nanoseconds) rather than calling gettimeofday(). The caller should provide:
 *   - seconds_since_epoch: number of TAI seconds since 1970-01-01 00:00:10 TAI
 *   - nanoseconds: nanoseconds within the current second (0...999999999)
 * The function will add 500 attoseconds for rounding.
 */
void taia_now(struct taia *t, uint64 seconds_since_epoch, unsigned long nanoseconds);

/*
 * taia_approx - returns a double-precision approximation to t/10^18. The
 * result of taia_approx is always nonnegative.
 */
double taia_approx(struct taia *t);

/*
 * taia_frac - returns a double-precision approximation to the fraction part
 * of t/10^18. The result of taia_frac is always nonnegative.
 */
double taia_frac(struct taia *t);

/*
 * taia_less - returns 1 if a is smaller than b, 0 otherwise.
 */
int taia_less(struct taia *a, struct taia *b);

/*
 * taia_add - adds a and b modulo 2^64x10^18 and puts the result into t.
 * The inputs and outputs may overlap.
 */
void taia_add(struct taia *t, struct taia *a, struct taia *b);

/*
 * taia_sub - subtracts b from a modulo 2^64x10^18 and puts the result into
 * t. The inputs and outputs may overlap.
 */
void taia_sub(struct taia *t, struct taia *a, struct taia *b);

/*
 * taia_half - divides a by 2, rounding down, and puts the result into t.
 * The input and output may overlap.
 */
void taia_half(struct taia *t, struct taia *a);

/*
 * taia_pack - convert TAI64NA labels to external format
 *
 * taia_pack converts a TAI64NA label from internal format in t to TAI64NA
 * format in buf. TAIA_PACK is 16.
 *
 * See http://pobox.com/~djb/proto/tai64.txt for more information about
 * TAI64NA format.
 */
#define TAIA_PACK 16
void taia_pack(char *buf, struct taia *t);

/*
 * taia_unpack - convert TAI64NA labels from external format
 *
 * taia_unpack converts a TAI64NA label from TAI64NA format in buf to internal
 * format in t.
 */
void taia_unpack(char *buf, struct taia *t);

/*
 * taia_fmtfrac - prints the remainder of t/10^18, padded to exactly 18 digits,
 * into the character buffer s, without a terminating NUL. It returns 18, the
 * number of characters written. s may be zero; then taia_fmtfrac returns 18
 * without printing anything.
 *
 * The macro TAIA_FMTFRAC is defined as 19; this is enough space for the
 * output of taia_fmtfrac and a terminating NUL.
 */
#define TAIA_FMTFRAC 19
unsigned int taia_fmtfrac(char *s, struct taia *t);


/*
 * caldate - calendar dates
 *
 * A struct caldate value is a calendar date. It has three components:
 * year, month (1...12), and day (1...31).
 */
struct caldate {
  long year;
  int month;
  int day;
};

/*
 * caldate_fmt - prints cd in ISO style (yyyy-mm-dd) into the character
 * buffer s, without a terminating NUL. It returns the number of characters
 * printed. s may be zero; then caldate_fmt returns the number of characters
 * that would have been printed.
 */
unsigned int caldate_fmt(char *s, struct caldate *cd);

/*
 * caldate_scan - reads a calendar date in ISO style from the beginning of
 * the character buffer s and puts it into cd. It returns the number of
 * characters read. If s does not start with an ISO-style date, caldate_scan
 * returns 0.
 */
unsigned int caldate_scan(char *s, struct caldate *cd);

/*
 * caldate_mjd - manipulate calendar dates
 *
 * Every calendar date has a modified Julian day number. The day number
 * increases by 1 every day. Day number 0 is 17 November 1858. Day number
 * 51604 is 1 March 2000.
 *
 * caldate_frommjd puts into cd the date corresponding to the modified Julian
 * day number day. It also computes the day of the week (0 through 6) and the
 * day of the year (0 through 365). It stores the day of the week in *weekday
 * if weekday is nonzero. It stores the day of the year in *yearday if yearday
 * is nonzero.
 *
 * caldate_mjd returns the modified Julian day number corresponding to the
 * date in cd. It accepts days outside the range 1 to 31, referring to days
 * before the beginning or after the end of the month. It also accepts months
 * outside the range 1 to 12, referring to months before the beginning or after
 * the end of the year.
 *
 * caldate_normalize calls caldate_frommjd with the result of caldate_mjd.
 * This means that it accepts out-of-range months and out-of-range days in cd,
 * and puts a valid calendar date back into cd.
 *
 * LIMITATIONS:
 * The caldate routines currently support the Gregorian calendar, which was
 * defined in 1582 and adopted at different times in different countries.
 * For earlier dates the caldate routines work with "virtual Gregorian,"
 * defined mathematically by the 400-year Gregorian cycle for years before
 * 1582. The Julian calendar is not supported.
 *
 * The Gregorian calendar will be replaced by a new calendar within a few
 * thousand years. The caldate_frommjd and caldate_mjd routines will be
 * upgraded accordingly. The current caldate_frommjd and caldate_mjd routines
 * are not useful for dates far in the future.
 *
 * Day numbers will overflow a 32-bit long sometime after the year 5000000;
 * all systems should upgrade to 64-bit longs before then. caldate_mjd does
 * not check for overflow.
 */
void caldate_frommjd(struct caldate *cd, long day, int *weekday, int *yearday);
long caldate_mjd(struct caldate *cd);
void caldate_normalize(struct caldate *cd);

/*
 * caldate_easter - reads the year from cd and then changes cd to the date
 * of Easter in the same year.
 */
void caldate_easter(struct caldate *cd);


/*
 * caltime - calendar dates and times
 *
 * A struct caltime value is a calendar date and time with an offset in
 * minutes from UTC. It has five components: date (a struct caldate),
 * hour (0...23), minute (0...59), second (0...60), and offset (-5999...5999).
 *
 * For example, a leap second occurred on 30 June 1997 at 23:59:60 UTC.
 * The local time in New York was 30 June 1997 19:59:60 -0400. This local
 * time is represented inside a struct caltime with date containing 1997,
 * 6, 30; hour 19; minute 59; second 60; and offset -240 (4 hours).
 */
struct caltime {
  struct caldate date;
  int hour;
  int minute;
  int second;
  long offset;
};

/*
 * caltime_fmt - prints ct in ISO style (yyyy-mm-dd hh:mm:ss +oooo) into the
 * character buffer s, without a terminating NUL. It returns the number of
 * characters printed. s may be zero; then caltime_fmt returns the number of
 * characters that would have been printed.
 */
unsigned int caltime_fmt(char *s, struct caltime *ct);

/*
 * caltime_scan - reads a calendar date, time, and offset in ISO style from
 * the beginning of the character buffer s and puts them into ct. It returns
 * the number of characters read. If s does not start with an ISO-style date
 * and time (including offset), caltime_scan returns 0.
 */
unsigned int caltime_scan(char *s, struct caltime *ct);

/*
 * caltime_tai - convert calendar dates and times
 *
 * caltime_tai reads a date, time, and UTC offset from ct. It puts the
 * corresponding TAI64 label into t.
 *
 * caltime_utc reads a TAI64 label from t. It puts the corresponding date
 * and time into ct, with UTC offset 0.
 *
 * caltime_utc fills in weekday and yearday the same way as caldate_frommjd.
 *
 * LIMITATIONS:
 * The sequence of TAI64 labels has been determined for the next few hundred
 * billion years. The same is not true, however, for calendar dates and times.
 * New leap seconds are added every year or two; and the Gregorian calendar
 * will change in a few thousand years. This means that caltime_tai and
 * caltime_utc are not useful for dates far in the future.
 *
 * NOTE: These functions require leap second data which is provided by the
 * leapsecs functions (not included in freestanding version).
 */
void caltime_tai(struct caltime *ct, struct tai *t);
void caltime_utc(struct caltime *ct, struct tai *t, int *weekday, int *yearday);


/*
 * leapsecs - handle UTC leap seconds
 *
 * leapsecs_sub changes a seconds-since-epoch count into a non-leap-seconds-
 * since-epoch count. It interprets t as a TAI64 label, subtracts from t the
 * number of leap seconds that have occurred before or at t, and places the
 * result back into t. Returns 1 if t was a leap second, 0 otherwise.
 *
 * leapsecs_add reverses the effect of leapsecs_sub. hit must be 1 for a
 * leap second, 0 otherwise.
 *
 * leapsecs_set sets the leap-second table from a buffer containing TAI64-encoded
 * leap second times. The buffer should contain count entries, each TAI_PACK bytes
 * long. The table is copied internally. Returns 0 on success, -1 on error.
 * Pass NULL and 0 to clear the table.
 *
 * leapsecs_read reads leap second data from a TAI64-encoded buffer. This is
 * a convenience wrapper around leapsecs_set that handles the unpacking.
 * The data buffer should contain TAI64-encoded leap second times (8 bytes each).
 * Returns 0 on success, -1 on error.
 *
 * WARNING: The caller is responsible for providing leap second data. New leap
 * seconds are added every year or two. For reliability, all programs should
 * ensure they have current leap second data.
 */
int leapsecs_read(const char *data, int datalen);
int leapsecs_set(const struct tai *table, int count);
void leapsecs_add(struct tai *t, int hit);
int leapsecs_sub(struct tai *t);

#endif
