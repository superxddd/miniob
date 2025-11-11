/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "common/type/date_type.h"

#include "common/lang/exception.h"
#include "common/log/log.h"
#include "common/value.h"

namespace {

inline bool is_leap(int y) { return (y % 400 == 0) || (y % 4 == 0 && y % 100 != 0); }

inline int days_in_month(int y, int m)
{
  static const int mdays[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
  if (m == 2) return mdays[m] + (is_leap(y) ? 1 : 0);
  return mdays[m];
}

// 1970-01-01 as day 0
inline bool ymd_to_days(int y, int m, int d, int &days)
{
  if (m < 1 || m > 12) return false;
  int dim = days_in_month(y, m);
  if (d < 1 || d > dim) return false;
  // count days from 1970-01-01 to (y,m,d)
  // handle years
  long long total = 0;
  if (y >= 1970) {
    for (int yy = 1970; yy < y; ++yy) total += is_leap(yy) ? 366 : 365;
  } else {
    for (int yy = y; yy < 1970; ++yy) total -= is_leap(yy) ? 366 : 365;
  }
  // months
  for (int mm = 1; mm < m; ++mm) total += days_in_month(y, mm);
  total += (d - 1);
  days = static_cast<int>(total);
  return true;
}

inline void days_to_ymd(int days, int &y, int &m, int &d)
{
  y = 1970;
  long long dd = days;
  if (dd >= 0) {
    while (true) {
      int ydays = is_leap(y) ? 366 : 365;
      if (dd >= ydays) { dd -= ydays; ++y; } else break;
    }
  } else {
    while (true) {
      int py = y - 1;
      int ydays = is_leap(py) ? 366 : 365;
      if (-dd > ydays) { dd += ydays; --y; } else break;
    }
  }
  m = 1;
  while (true) {
    int md = days_in_month(y, m);
    if (dd >= md) { dd -= md; ++m; } else break;
  }
  d = static_cast<int>(dd) + 1;
}

inline bool parse_date_text(const string &s, int &days)
{
  // accept formats: YYYY-M-D / YYYY-MM-D / YYYY-M-DD / YYYY-MM-DD
  // find two '-' separators
  size_t p1 = s.find('-');
  if (p1 == string::npos) return false;
  size_t p2 = s.find('-', p1 + 1);
  if (p2 == string::npos) return false;
  string ys = s.substr(0, p1);
  string ms = s.substr(p1 + 1, p2 - p1 - 1);
  string ds = s.substr(p2 + 1);
  if (ys.size() != 4 || ms.empty() || ms.size() > 2 || ds.empty() || ds.size() > 2) return false;
  // all digits
  auto is_digits = [](const string &t) {
    for (char c : t) if (c < '0' || c > '9') return false;
    return true;
  };
  if (!is_digits(ys) || !is_digits(ms) || !is_digits(ds)) return false;
  int y = atoi(ys.c_str());
  int m = atoi(ms.c_str());
  int d = atoi(ds.c_str());
  return ymd_to_days(y, m, d, days);
}

inline string format_date_text(int days)
{
  int y,m,d;
  days_to_ymd(days, y, m, d);
  char buf[16];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d", y, m, d);
  return string(buf);
}

} // namespace

int DateType::compare(const Value &left, const Value &right) const
{
  int li = left.get_int();
  int ri = right.get_int();
  if (li < ri) return -1;
  if (li > ri) return 1;
  return 0;
}

RC DateType::to_string(const Value &val, string &result) const
{
  result = format_date_text(val.get_int());
  return RC::SUCCESS;
}

RC DateType::cast_to(const Value &val, AttrType to_type, Value &result) const
{
  if (to_type == AttrType::DATES) {
    result = val;
    return RC::SUCCESS;
  }
  if (to_type == AttrType::CHARS) {
    string s;
    RC rc = to_string(val, s);
    if (OB_FAIL(rc)) return rc;
    result.set_string(s.c_str());
    return RC::SUCCESS;
  }
  return RC::UNSUPPORTED;
}

RC DateType::set_value_from_str(Value &val, const string &data) const
{
  int days = 0;
  if (!parse_date_text(data, days)) {
    return RC::INVALID_ARGUMENT;
  }
  val.set_int(days);
  val.set_type(AttrType::DATES);
  return RC::SUCCESS;
}


