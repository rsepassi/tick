#include "tai.h"
#include "tai_leapsecs_dat.h"

/* ============================================================================
 * TAI Functions - Basic time operations with 1-second precision
 * ============================================================================ */

void tai_add(struct tai *t, struct tai *a, struct tai *b)
{
  t->x = a->x + b->x;
}

void tai_sub(struct tai *t, struct tai *a, struct tai *b)
{
  t->x = a->x - b->x;
}

void tai_pack(char *s, struct tai *t)
{
  uint64 x;

  x = t->x;
  s[7] = x & 255; x >>= 8;
  s[6] = x & 255; x >>= 8;
  s[5] = x & 255; x >>= 8;
  s[4] = x & 255; x >>= 8;
  s[3] = x & 255; x >>= 8;
  s[2] = x & 255; x >>= 8;
  s[1] = x & 255; x >>= 8;
  s[0] = x;
}

void tai_unpack(char *s, struct tai *t)
{
  uint64 x;

  x = (unsigned char) s[0];
  x <<= 8; x += (unsigned char) s[1];
  x <<= 8; x += (unsigned char) s[2];
  x <<= 8; x += (unsigned char) s[3];
  x <<= 8; x += (unsigned char) s[4];
  x <<= 8; x += (unsigned char) s[5];
  x <<= 8; x += (unsigned char) s[6];
  x <<= 8; x += (unsigned char) s[7];
  t->x = x;
}

void tai_now(struct tai *t, uint64 seconds_since_epoch)
{
  t->x = 4611686018427387914ULL + seconds_since_epoch;
}


/* ============================================================================
 * TAIA Functions - Time operations with 1-attosecond precision
 * ============================================================================ */

void taia_tai(struct taia *ta, struct tai *t)
{
  *t = ta->sec;
}

void taia_now(struct taia *t, uint64 seconds_since_epoch, unsigned long nanoseconds)
{
  t->sec.x = 4611686018427387914ULL + seconds_since_epoch;
  t->nano = nanoseconds;
  t->atto = 0;
}

double taia_frac(struct taia *t)
{
  return (t->atto * 0.000000001 + t->nano) * 0.000000001;
}

double taia_approx(struct taia *t)
{
  return tai_approx(&t->sec) + taia_frac(t);
}

void taia_add(struct taia *t, struct taia *a, struct taia *b)
{
  t->sec.x = a->sec.x + b->sec.x;
  t->nano = a->nano + b->nano;
  t->atto = a->atto + b->atto;
  if (t->atto > 999999999UL) {
    t->atto -= 1000000000UL;
    ++t->nano;
  }
  if (t->nano > 999999999UL) {
    t->nano -= 1000000000UL;
    ++t->sec.x;
  }
}

void taia_sub(struct taia *t, struct taia *a, struct taia *b)
{
  unsigned long unano = a->nano;
  unsigned long uatto = a->atto;

  t->sec.x = a->sec.x - b->sec.x;
  t->nano = unano - b->nano;
  t->atto = uatto - b->atto;
  if (t->atto > uatto) {
    t->atto += 1000000000UL;
    --t->nano;
  }
  if (t->nano > unano) {
    t->nano += 1000000000UL;
    --t->sec.x;
  }
}

void taia_half(struct taia *t, struct taia *a)
{
  t->atto = a->atto >> 1;
  if (a->nano & 1) t->atto += 500000000UL;
  t->nano = a->nano >> 1;
  if (a->sec.x & 1) t->nano += 500000000UL;
  t->sec.x = a->sec.x >> 1;
}

int taia_less(struct taia *a, struct taia *b)
{
  if (a->sec.x < b->sec.x) return 1;
  if (a->sec.x > b->sec.x) return 0;
  if (a->nano < b->nano) return 1;
  if (a->nano > b->nano) return 0;
  return a->atto < b->atto;
}

void taia_pack(char *s, struct taia *t)
{
  unsigned long x;

  tai_pack(s, &t->sec);
  s += 8;

  x = t->atto;
  s[7] = x & 255; x >>= 8;
  s[6] = x & 255; x >>= 8;
  s[5] = x & 255; x >>= 8;
  s[4] = x;
  x = t->nano;
  s[3] = x & 255; x >>= 8;
  s[2] = x & 255; x >>= 8;
  s[1] = x & 255; x >>= 8;
  s[0] = x;
}

void taia_unpack(char *s, struct taia *t)
{
  unsigned long x;

  tai_unpack(s, &t->sec);
  s += 8;

  x = (unsigned char) s[4];
  x <<= 8; x += (unsigned char) s[5];
  x <<= 8; x += (unsigned char) s[6];
  x <<= 8; x += (unsigned char) s[7];
  t->atto = x;
  x = (unsigned char) s[0];
  x <<= 8; x += (unsigned char) s[1];
  x <<= 8; x += (unsigned char) s[2];
  x <<= 8; x += (unsigned char) s[3];
  t->nano = x;
}

unsigned int taia_fmtfrac(char *s, struct taia *t)
{
  unsigned long x;

  if (s) {
    x = t->atto;
    s[17] = '0' + (x % 10); x /= 10;
    s[16] = '0' + (x % 10); x /= 10;
    s[15] = '0' + (x % 10); x /= 10;
    s[14] = '0' + (x % 10); x /= 10;
    s[13] = '0' + (x % 10); x /= 10;
    s[12] = '0' + (x % 10); x /= 10;
    s[11] = '0' + (x % 10); x /= 10;
    s[10] = '0' + (x % 10); x /= 10;
    s[9] = '0' + (x % 10);
    x = t->nano;
    s[8] = '0' + (x % 10); x /= 10;
    s[7] = '0' + (x % 10); x /= 10;
    s[6] = '0' + (x % 10); x /= 10;
    s[5] = '0' + (x % 10); x /= 10;
    s[4] = '0' + (x % 10); x /= 10;
    s[3] = '0' + (x % 10); x /= 10;
    s[2] = '0' + (x % 10); x /= 10;
    s[1] = '0' + (x % 10); x /= 10;
    s[0] = '0' + (x % 10);
  }

  return 18;
}


/* ============================================================================
 * CALDATE Functions - Calendar date operations
 * ============================================================================ */

unsigned int caldate_fmt(char *s, struct caldate *cd)
{
  long x;
  int i = 0;

  x = cd->year; if (x < 0) x = -x; do { ++i; x /= 10; } while(x);

  if (s) {
    x = cd->year;
    if (x < 0) { x = -x; *s++ = '-'; }
    s += i; do { *--s = '0' + (x % 10); x /= 10; } while(x); s += i;

    x = cd->month;
    s[0] = '-'; s[2] = '0' + (x % 10); x /= 10; s[1] = '0' + (x % 10);

    x = cd->day;
    s[3] = '-'; s[5] = '0' + (x % 10); x /= 10; s[4] = '0' + (x % 10);
  }

  return (cd->year < 0) + i + 6;
}

unsigned int caldate_scan(char *s, struct caldate *cd)
{
  int sign = 1;
  char *t = s;
  unsigned long z;
  unsigned long c;

  if (*t == '-') { ++t; sign = -1; }
  z = 0; while ((c = (unsigned char) (*t - '0')) <= 9) { z = z * 10 + c; ++t; }
  cd->year = z * sign;

  if (*t++ != '-') return 0;
  z = 0; while ((c = (unsigned char) (*t - '0')) <= 9) { z = z * 10 + c; ++t; }
  cd->month = z;

  if (*t++ != '-') return 0;
  z = 0; while ((c = (unsigned char) (*t - '0')) <= 9) { z = z * 10 + c; ++t; }
  cd->day = z;

  return t - s;
}

void caldate_frommjd(struct caldate *cd, long day, int *pwday, int *pyday)
{
  long year;
  long month;
  int yday;

  year = day / 146097L;
  day %= 146097L;
  day += 678881L;
  while (day >= 146097L) { day -= 146097L; ++year; }

  /* year * 146097 + day - 678881 is MJD; 0 <= day < 146097 */
  /* 2000-03-01, MJD 51604, is year 5, day 0 */

  if (pwday) *pwday = (day + 3) % 7;

  year *= 4;
  if (day == 146096L) { year += 3; day = 36524L; }
  else { year += day / 36524L; day %= 36524L; }
  year *= 25;
  year += day / 1461;
  day %= 1461;
  year *= 4;

  yday = (day < 306);
  if (day == 1460) { year += 3; day = 365; }
  else { year += day / 365; day %= 365; }
  yday += day;

  day *= 10;
  month = (day + 5) / 306;
  day = (day + 5) % 306;
  day /= 10;
  if (month >= 10) { yday -= 306; ++year; month -= 10; }
  else { yday += 59; month += 2; }

  cd->year = year;
  cd->month = month + 1;
  cd->day = day + 1;

  if (pyday) *pyday = yday;
}

static unsigned long times365[4] = { 0, 365, 730, 1095 };
static unsigned long times36524[4] = { 0, 36524UL, 73048UL, 109572UL };
static unsigned long montab[12] =
  { 0, 31, 61, 92, 122, 153, 184, 214, 245, 275, 306, 337 };
/* month length after february is (306 * m + 5) / 10 */

long caldate_mjd(struct caldate *cd)
{
  long y;
  long m;
  long d;

  d = cd->day - 678882L;
  m = cd->month - 1;
  y = cd->year;

  d += 146097L * (y / 400);
  y %= 400;

  if (m >= 2) m -= 2; else { m += 10; --y; }

  y += (m / 12);
  m %= 12;
  if (m < 0) { m += 12; --y; }

  d += montab[m];

  d += 146097L * (y / 400);
  y %= 400;
  if (y < 0) { y += 400; d -= 146097L; }

  d += times365[y & 3];
  y >>= 2;

  d += 1461L * (y % 25);
  y /= 25;

  d += times36524[y & 3];

  return d;
}

void caldate_normalize(struct caldate *cd)
{
  caldate_frommjd(cd, caldate_mjd(cd), (int *) 0, (int *) 0);
}

void caldate_easter(struct caldate *cd)
{
  long c;
  long t;
  long j;
  long n;
  long y;

  y = cd->year;

  c = (y / 100) + 1;
  t = 210 - (((c * 3) / 4) % 210);
  j = y % 19;
  n = 57 - ((14 + j * 11 + (c * 8 + 5) / 25 + t) % 30);
  if ((n == 56) && (j > 10)) --n;
  if (n == 57) --n;
  n -= ((((y % 28) * 5) / 4 + t + n + 2) % 7);

  if (n < 32) { cd->month = 3; cd->day = n; }
  else { cd->month = 4; cd->day = n - 31; }
}


/* ============================================================================
 * CALTIME Functions - Calendar date and time operations
 * ============================================================================ */

unsigned int caltime_fmt(char *s, struct caltime *ct)
{
  unsigned int result;
  long x;

  result = caldate_fmt(s, &ct->date);

  if (s) {
    s += result;

    x = ct->hour;
    s[0] = ' ';
    s[2] = '0' + (x % 10); x /= 10;
    s[1] = '0' + (x % 10);
    s += 3;

    x = ct->minute;
    s[0] = ':';
    s[2] = '0' + (x % 10); x /= 10;
    s[1] = '0' + (x % 10);
    s += 3;

    x = ct->second;
    s[0] = ':';
    s[2] = '0' + (x % 10); x /= 10;
    s[1] = '0' + (x % 10);
    s += 3;

    s[0] = ' ';
    x = ct->offset;
    if (x < 0) { s[1] = '-'; x = -x; } else s[1] = '+';

    s[5] = '0' + (x % 10); x /= 10;
    s[4] = '0' + (x % 6); x /= 6;
    s[3] = '0' + (x % 10); x /= 10;
    s[2] = '0' + (x % 10);
  }

  return result + 15;
}

unsigned int caltime_scan(char *s, struct caltime *ct)
{
  char *t = s;
  unsigned long z;
  unsigned long c;
  int sign;

  t += caldate_scan(t, &ct->date);

  while ((*t == ' ') || (*t == '\t') || (*t == 'T')) ++t;
  z = 0; while ((c = (unsigned char) (*t - '0')) <= 9) { z = z * 10 + c; ++t; }
  ct->hour = z;

  if (*t++ != ':') return 0;
  z = 0; while ((c = (unsigned char) (*t - '0')) <= 9) { z = z * 10 + c; ++t; }
  ct->minute = z;

  if (*t != ':')
    ct->second = 0;
  else {
    ++t;
    z = 0; while ((c = (unsigned char) (*t - '0')) <= 9) { z = z * 10 + c; ++t; }
    ct->second = z;
  }

  while ((*t == ' ') || (*t == '\t')) ++t;
  if (*t == '+') sign = 1; else if (*t == '-') sign = -1; else return 0;
  ++t;
  c = (unsigned char) (*t++ - '0'); if (c > 9) return 0; z = c;
  c = (unsigned char) (*t++ - '0'); if (c > 9) return 0; z = z * 10 + c;
  c = (unsigned char) (*t++ - '0'); if (c > 9) return 0; z = z * 6 + c;
  c = (unsigned char) (*t++ - '0'); if (c > 9) return 0; z = z * 10 + c;
  ct->offset = z * sign;

  return t - s;
}

void caltime_tai(struct caltime *ct, struct tai *t)
{
  long day;
  long s;

  /* XXX: check for overflow? */

  day = caldate_mjd(&ct->date);

  s = ct->hour * 60 + ct->minute;
  s = (s - ct->offset) * 60 + ct->second;

  t->x = day * 86400ULL + 4611686014920671114ULL + (long long) s;

  leapsecs_add(t, ct->second == 60);
}

void caltime_utc(struct caltime *ct, struct tai *t, int *pwday, int *pyday)
{
  struct tai t2 = *t;
  uint64 u;
  int leap;
  long s;

  /* XXX: check for overflow? */

  leap = leapsecs_sub(&t2);
  u = t2.x;

  u += 58486;
  s = u % 86400ULL;

  ct->second = (s % 60) + leap; s /= 60;
  ct->minute = s % 60; s /= 60;
  ct->hour = s;

  u /= 86400ULL;
  caldate_frommjd(&ct->date, /*XXX*/(long) (u - 53375995543064ULL), pwday, pyday);

  ct->offset = 0;
}


/* ============================================================================
 * LEAPSECS Functions - Leap second handling
 * ============================================================================ */

/*
 * Leap second data is statically compiled in from tai_leapsecs_dat.h
 * which defines:
 *   - leapsecs_table[LEAPSECS_MAX] - array of leap second timestamps (256 entries)
 *   - LEAPSECS_INIT_COUNT - number of leap seconds compiled in
 *   - LEAPSECS_MAX - maximum capacity (256)
 *
 * The table starts with compiled-in data but can be updated dynamically.
 */

static int leapsecs_num = LEAPSECS_INIT_COUNT;

int leapsecs_set(const struct tai *table, int count)
{
  int i;

  if (count < 0 || count > LEAPSECS_MAX) return -1;

  /* Handle NULL/0 to reset to initial data */
  if (table == (struct tai *) 0 || count == 0) {
    leapsecs_num = LEAPSECS_INIT_COUNT;
    return 0;
  }

  /* Copy the table */
  for (i = 0; i < count; ++i) {
    leapsecs_table[i] = table[i];
  }

  leapsecs_num = count;
  return 0;
}

int leapsecs_read(const char *data, int datalen)
{
  int count;
  int i;
  struct tai temp[LEAPSECS_MAX];

  /* Validate input */
  if (data == (char *) 0 || datalen < 0) return -1;
  if (datalen % TAI_PACK != 0) return -1;

  count = datalen / TAI_PACK;
  if (count > LEAPSECS_MAX) return -1;

  /* Unpack all entries into temporary buffer */
  for (i = 0; i < count; ++i) {
    tai_unpack((char *) (data + i * TAI_PACK), &temp[i]);
  }

  /* Now set the table */
  return leapsecs_set(temp, count);
}

void leapsecs_add(struct tai *t, int hit)
{
  int i;
  uint64 u;

  u = t->x;

  for (i = 0; i < leapsecs_num; ++i) {
    if (u < leapsecs_table[i].x) break;
    if (!hit || (u > leapsecs_table[i].x)) ++u;
  }

  t->x = u;
}

int leapsecs_sub(struct tai *t)
{
  int i;
  uint64 u;
  int s;

  u = t->x;
  s = 0;

  for (i = 0; i < leapsecs_num; ++i) {
    if (u < leapsecs_table[i].x) break;
    ++s;
    if (u == leapsecs_table[i].x) { t->x = u - s; return 1; }
  }

  t->x = u - s;
  return 0;
}
