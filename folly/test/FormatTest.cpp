/*
 * Copyright 2014 Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "folly/Format.h"

#include <glog/logging.h>
#include <gflags/gflags.h>
#include <gtest/gtest.h>

#include "folly/FBVector.h"
#include "folly/FileUtil.h"
#include "folly/dynamic.h"
#include "folly/json.h"

using namespace folly;

template <class... Args>
std::string fstr(StringPiece fmt, Args&&... args) {
  return format(fmt, std::forward<Args>(args)...).str();
}

template <class C>
std::string vstr(StringPiece fmt, const C& c) {
  return vformat(fmt, c).str();
}

template <class... Args>
std::string fstrChecked(StringPiece fmt, Args&&... args) {
  return formatChecked(fmt, std::forward<Args>(args)...).str();
}

template <class C>
std::string vstrChecked(StringPiece fmt, const C& c) {
  return vformatChecked(fmt, c).str();
}

template <class Uint>
void compareOctal(Uint u) {
  char buf1[detail::kMaxOctalLength + 1];
  buf1[detail::kMaxOctalLength] = '\0';
  char* p = buf1 + detail::uintToOctal(buf1, detail::kMaxOctalLength, u);

  char buf2[detail::kMaxOctalLength + 1];
  sprintf(buf2, "%jo", static_cast<uintmax_t>(u));

  EXPECT_EQ(std::string(buf2), std::string(p));
}

template <class Uint>
void compareHex(Uint u) {
  char buf1[detail::kMaxHexLength + 1];
  buf1[detail::kMaxHexLength] = '\0';
  char* p = buf1 + detail::uintToHexLower(buf1, detail::kMaxHexLength, u);

  char buf2[detail::kMaxHexLength + 1];
  sprintf(buf2, "%jx", static_cast<uintmax_t>(u));

  EXPECT_EQ(std::string(buf2), std::string(p));
}

template <class Uint>
void compareBinary(Uint u) {
  char buf[detail::kMaxBinaryLength + 1];
  buf[detail::kMaxBinaryLength] = '\0';
  char* p = buf + detail::uintToBinary(buf, detail::kMaxBinaryLength, u);

  std::string repr;
  if (u == 0) {
    repr = '0';
  } else {
    std::string tmp;
    for (; u; u >>= 1) {
      tmp.push_back(u & 1 ? '1' : '0');
    }
    repr.assign(tmp.rbegin(), tmp.rend());
  }

  EXPECT_EQ(repr, std::string(p));
}

TEST(Format, uintToOctal) {
  for (unsigned i = 0; i < (1u << 16) + 2; i++) {
    compareOctal(i);
  }
}

TEST(Format, uintToHex) {
  for (unsigned i = 0; i < (1u << 16) + 2; i++) {
    compareHex(i);
  }
}

TEST(Format, uintToBinary) {
  for (unsigned i = 0; i < (1u << 16) + 2; i++) {
    compareBinary(i);
  }
}

TEST(Format, Simple) {
  EXPECT_EQ("hello", fstr("hello"));
  EXPECT_EQ("42", fstr("{}", 42));
  EXPECT_EQ("42 42", fstr("{0} {0}", 42));
  EXPECT_EQ("00042  23   42", fstr("{0:05} {1:3} {0:4}", 42, 23));
  EXPECT_EQ("hello world hello 42",
            fstr("{0} {1} {0} {2}", "hello", "world", 42));
  EXPECT_EQ("XXhelloXX", fstr("{:X^9}", "hello"));
  EXPECT_EQ("XXX42XXXX", fstr("{:X^9}", 42));
  EXPECT_EQ("-0xYYYY2a", fstr("{:Y=#9x}", -42));
  EXPECT_EQ("*", fstr("{}", '*'));
  EXPECT_EQ("42", fstr("{}", 42));
  EXPECT_EQ("0042", fstr("{:04}", 42));

  EXPECT_EQ("hello  ", fstr("{:7}", "hello"));
  EXPECT_EQ("hello  ", fstr("{:<7}", "hello"));
  EXPECT_EQ("  hello", fstr("{:>7}", "hello"));

  std::vector<int> v1 {10, 20, 30};
  EXPECT_EQ("0020", fstr("{0[1]:04}", v1));
  EXPECT_EQ("0020", vstr("{1:04}", v1));
  EXPECT_EQ("10 20", vstr("{} {}", v1));

  const std::vector<int> v2 = v1;
  EXPECT_EQ("0020", fstr("{0[1]:04}", v2));
  EXPECT_EQ("0020", vstr("{1:04}", v2));
  EXPECT_THROW(fstr("{0[3]:04}", v2), std::out_of_range);
  EXPECT_THROW(vstr("{3:04}", v2), std::out_of_range);
  EXPECT_EQ("0020", fstr("{0[1]:04}", defaulted(v2, 42)));
  EXPECT_EQ("0020", vstr("{1:04}", defaulted(v2, 42)));
  EXPECT_EQ("0042", fstr("{0[3]:04}", defaulted(v2, 42)));
  EXPECT_EQ("0042", vstr("{3:04}", defaulted(v2, 42)));

  const int p[] = {10, 20, 30};
  const int* q = p;
  EXPECT_EQ("0020", fstr("{0[1]:04}", p));
  EXPECT_EQ("0020", vstr("{1:04}", p));
  EXPECT_EQ("0020", fstr("{0[1]:04}", q));
  EXPECT_EQ("0020", vstr("{1:04}", q));
  EXPECT_NE("", fstr("{}", q));

  EXPECT_EQ("0x", fstr("{}", p).substr(0, 2));
  EXPECT_EQ("10", vstr("{}", p));
  EXPECT_EQ("0x", fstr("{}", q).substr(0, 2));
  EXPECT_EQ("10", vstr("{}", q));
  q = nullptr;
  EXPECT_EQ("(null)", fstr("{}", q));

  std::map<int, std::string> m { {10, "hello"}, {20, "world"} };
  EXPECT_EQ("worldXX", fstr("{[20]:X<7}", m));
  EXPECT_EQ("worldXX", vstr("{20:X<7}", m));
  EXPECT_THROW(fstr("{[42]:X<7}", m), std::out_of_range);
  EXPECT_THROW(vstr("{42:X<7}", m), std::out_of_range);
  EXPECT_EQ("worldXX", fstr("{[20]:X<7}", defaulted(m, "meow")));
  EXPECT_EQ("worldXX", vstr("{20:X<7}", defaulted(m, "meow")));
  EXPECT_EQ("meowXXX", fstr("{[42]:X<7}", defaulted(m, "meow")));
  EXPECT_EQ("meowXXX", vstr("{42:X<7}", defaulted(m, "meow")));

  std::map<std::string, std::string> m2 { {"hello", "world"} };
  EXPECT_EQ("worldXX", fstr("{[hello]:X<7}", m2));
  EXPECT_EQ("worldXX", vstr("{hello:X<7}", m2));
  EXPECT_THROW(fstr("{[none]:X<7}", m2), std::out_of_range);
  EXPECT_THROW(vstr("{none:X<7}", m2), std::out_of_range);
  EXPECT_EQ("worldXX", fstr("{[hello]:X<7}", defaulted(m2, "meow")));
  EXPECT_EQ("worldXX", vstr("{hello:X<7}", defaulted(m2, "meow")));
  EXPECT_EQ("meowXXX", fstr("{[none]:X<7}", defaulted(m2, "meow")));
  EXPECT_EQ("meowXXX", vstr("{none:X<7}", defaulted(m2, "meow")));

  // Test indexing in strings
  EXPECT_EQ("61 62", fstr("{0[0]:x} {0[1]:x}", "abcde"));
  EXPECT_EQ("61 62", vstr("{0:x} {1:x}", "abcde"));
  EXPECT_EQ("61 62", fstr("{0[0]:x} {0[1]:x}", std::string("abcde")));
  EXPECT_EQ("61 62", vstr("{0:x} {1:x}", std::string("abcde")));

  // Test booleans
  EXPECT_EQ("true", fstr("{}", true));
  EXPECT_EQ("1", fstr("{:d}", true));
  EXPECT_EQ("false", fstr("{}", false));
  EXPECT_EQ("0", fstr("{:d}", false));

  // Test pairs
  {
    std::pair<int, std::string> p {42, "hello"};
    EXPECT_EQ("    42 hello ", fstr("{0[0]:6} {0[1]:6}", p));
    EXPECT_EQ("    42 hello ", vstr("{:6} {:6}", p));
  }

  // Test tuples
  {
    std::tuple<int, std::string, int> t { 42, "hello", 23 };
    EXPECT_EQ("    42 hello      23", fstr("{0[0]:6} {0[1]:6} {0[2]:6}", t));
    EXPECT_EQ("    42 hello      23", vstr("{:6} {:6} {:6}", t));
  }

  // Test writing to stream
  std::ostringstream os;
  os << format("{} {}", 42, 23);
  EXPECT_EQ("42 23", os.str());

  // Test appending to string
  std::string s;
  format(&s, "{} {}", 42, 23);
  format(&s, " hello {:X<7}", "world");
  EXPECT_EQ("42 23 hello worldXX", s);

  // Test writing to FILE. I'd use open_memstream but that's not available
  // outside of Linux (even though it's in POSIX.1-2008).
  {
    int fds[2];
    CHECK_ERR(pipe(fds));
    SCOPE_EXIT { closeNoInt(fds[1]); };
    {
      FILE* fp = fdopen(fds[1], "wb");
      PCHECK(fp);
      SCOPE_EXIT { fclose(fp); };
      writeTo(fp, format("{} {}", 42, 23));  // <= 512 bytes (PIPE_BUF)
    }

    char buf[512];
    ssize_t n = readFull(fds[0], buf, sizeof(buf));
    CHECK_GE(n, 0);

    EXPECT_EQ("42 23", std::string(buf, n));
  }
}

TEST(Format, Float) {
  double d = 1;
  EXPECT_EQ("1", fstr("{}", 1.0));
  EXPECT_EQ("0.1", fstr("{}", 0.1));
  EXPECT_EQ("0.01", fstr("{}", 0.01));
  EXPECT_EQ("0.001", fstr("{}", 0.001));
  EXPECT_EQ("0.0001", fstr("{}", 0.0001));
  EXPECT_EQ("1e-5", fstr("{}", 0.00001));
  EXPECT_EQ("1e-6", fstr("{}", 0.000001));

  EXPECT_EQ("10", fstr("{}", 10.0));
  EXPECT_EQ("100", fstr("{}", 100.0));
  EXPECT_EQ("1000", fstr("{}", 1000.0));
  EXPECT_EQ("10000", fstr("{}", 10000.0));
  EXPECT_EQ("100000", fstr("{}", 100000.0));
  EXPECT_EQ("1e+6", fstr("{}", 1000000.0));
  EXPECT_EQ("1e+7", fstr("{}", 10000000.0));

  EXPECT_EQ("1.00", fstr("{:.2f}", 1.0));
  EXPECT_EQ("0.10", fstr("{:.2f}", 0.1));
  EXPECT_EQ("0.01", fstr("{:.2f}", 0.01));
  EXPECT_EQ("0.00", fstr("{:.2f}", 0.001));
}

TEST(Format, MultiLevel) {
  std::vector<std::map<std::string, std::string>> v = {
    {
      {"hello", "world"},
    },
  };

  EXPECT_EQ("world", fstr("{[0.hello]}", v));
}

TEST(Format, dynamic) {
  auto dyn = parseJson(
      "{\n"
      "  \"hello\": \"world\",\n"
      "  \"x\": [20, 30],\n"
      "  \"y\": {\"a\" : 42}\n"
      "}");

  EXPECT_EQ("world", fstr("{0[hello]}", dyn));
  EXPECT_THROW(fstr("{0[none]}", dyn), std::out_of_range);
  EXPECT_EQ("world", fstr("{0[hello]}", defaulted(dyn, "meow")));
  EXPECT_EQ("meow", fstr("{0[none]}", defaulted(dyn, "meow")));

  EXPECT_EQ("20", fstr("{0[x.0]}", dyn));
  EXPECT_THROW(fstr("{0[x.2]}", dyn), std::out_of_range);

  // No support for "deep" defaulting (dyn["x"] is not defaulted)
  auto v = dyn.at("x");
  EXPECT_EQ("20", fstr("{0[0]}", v));
  EXPECT_THROW(fstr("{0[2]}", v), std::out_of_range);
  EXPECT_EQ("20", fstr("{0[0]}", defaulted(v, 42)));
  EXPECT_EQ("42", fstr("{0[2]}", defaulted(v, 42)));

  EXPECT_EQ("42", fstr("{0[y.a]}", dyn));

  EXPECT_EQ("(null)", fstr("{}", dynamic(nullptr)));
}

namespace {

struct KeyValue {
  std::string key;
  int value;
};

}  // namespace

namespace folly {

template <> class FormatValue<KeyValue> {
 public:
  explicit FormatValue(const KeyValue& kv) : kv_(kv) { }

  template <class FormatCallback>
  void format(FormatArg& arg, FormatCallback& cb) const {
    format_value::formatFormatter(
        folly::format("<key={}, value={}>", kv_.key, kv_.value),
        arg, cb);
  }

 private:
  const KeyValue& kv_;
};

}  // namespace

TEST(Format, Custom) {
  KeyValue kv { "hello", 42 };

  EXPECT_EQ("<key=hello, value=42>", fstr("{}", kv));
  EXPECT_EQ("<key=hello, value=42>", fstr("{:10}", kv));
  EXPECT_EQ("<key=hello", fstr("{:.10}", kv));
  EXPECT_EQ("<key=hello, value=42>XX", fstr("{:X<23}", kv));
  EXPECT_EQ("XX<key=hello, value=42>", fstr("{:X>23}", kv));
  EXPECT_EQ("<key=hello, value=42>", fstr("{0[0]}", &kv));
  EXPECT_NE("", fstr("{}", &kv));
}

namespace {

struct Opaque {
  int k;
};

} // namespace

TEST(Format, Unformatted) {
  Opaque o;
  EXPECT_NE("", fstr("{}", &o));
  EXPECT_DEATH(fstr("{0[0]}", &o), "No formatter available for this type");
  EXPECT_THROW(fstrChecked("{0[0]}", &o), std::invalid_argument);
}

TEST(Format, Nested) {
  EXPECT_EQ("1 2 3 4", fstr("{} {} {}", 1, 2, format("{} {}", 3, 4)));
  //
  // not copyable, must hold temporary in scope instead.
  auto&& saved = format("{} {}", 3, 4);
  EXPECT_EQ("1 2 3 4", fstr("{} {} {}", 1, 2, saved));
}

TEST(Format, OutOfBounds) {
  std::vector<int> ints{1, 2, 3, 4, 5};
  EXPECT_EQ("1 3 5", fstr("{0[0]} {0[2]} {0[4]}", ints));
  EXPECT_THROW(fstr("{[5]}", ints), std::out_of_range);
  EXPECT_THROW(fstrChecked("{[5]}", ints), std::out_of_range);

  std::map<std::string, int> map{{"hello", 0}, {"world", 1}};
  EXPECT_EQ("hello = 0", fstr("hello = {[hello]}", map));
  EXPECT_THROW(fstr("{[nope]}", map), std::out_of_range);
  EXPECT_THROW(vstr("{nope}", map), std::out_of_range);
  EXPECT_THROW(vstrChecked("{nope}", map), std::out_of_range);
}

TEST(Format, BogusFormatString) {
  // format() will crash the program if the format string is invalid.
  EXPECT_DEATH(fstr("}"), "single '}' in format string");
  EXPECT_DEATH(fstr("foo}bar"), "single '}' in format string");
  EXPECT_DEATH(fstr("foo{bar"), "missing ending '}'");
  EXPECT_DEATH(fstr("{[test]"), "missing ending '}'");
  EXPECT_DEATH(fstr("{-1.3}"), "argument index must be non-negative");
  EXPECT_DEATH(fstr("{1.3}", 0, 1, 2), "index not allowed");
  EXPECT_DEATH(fstr("{0} {} {1}", 0, 1, 2),
               "may not have both default and explicit arg indexes");

  // formatChecked() should throw exceptions rather than crashing the program
  EXPECT_THROW(fstrChecked("}"), std::invalid_argument);
  EXPECT_THROW(fstrChecked("foo}bar"), std::invalid_argument);
  EXPECT_THROW(fstrChecked("foo{bar"), std::invalid_argument);
  EXPECT_THROW(fstrChecked("{[test]"), std::invalid_argument);
  EXPECT_THROW(fstrChecked("{-1.3}"), std::invalid_argument);
  EXPECT_THROW(fstrChecked("{1.3}", 0, 1, 2), std::invalid_argument);
  EXPECT_THROW(fstrChecked("{0} {} {1}", 0, 1, 2), std::invalid_argument);

  // This one fails in detail::enforceWhitespace(), which throws
  // std::range_error
  EXPECT_DEATH(fstr("{0[test}"), "Non-whitespace: \\[");
  EXPECT_THROW(fstrChecked("{0[test}"), std::exception);
}

int main(int argc, char *argv[]) {
  testing::InitGoogleTest(&argc, argv);
  google::ParseCommandLineFlags(&argc, &argv, true);
  return RUN_ALL_TESTS();
}

