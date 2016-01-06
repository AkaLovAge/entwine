/// Arbiter amalgamated source (https://github.com/connormanning/arbiter).
/// It is intended to be used with #include "arbiter.hpp"

// //////////////////////////////////////////////////////////////////////
// Beginning of content of file: LICENSE
// //////////////////////////////////////////////////////////////////////

/*
The MIT License (MIT)

Copyright (c) 2015 Connor Manning

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.


*/

// //////////////////////////////////////////////////////////////////////
// End of content of file: LICENSE
// //////////////////////////////////////////////////////////////////////






#include "arbiter.hpp"

#ifndef ARBITER_IS_AMALGAMATION
#error "Compile with -I PATH_TO_ARBITER_DIRECTORY"
#endif


// //////////////////////////////////////////////////////////////////////
// Beginning of content of file: arbiter/arbiter.cpp
// //////////////////////////////////////////////////////////////////////

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/arbiter.hpp>

#include <arbiter/driver.hpp>
#endif

#include <algorithm>

namespace arbiter
{

namespace
{
    const std::string delimiter("://");

    const std::size_t concurrentHttpReqs(32);
    const std::size_t httpRetryCount(8);
}

Arbiter::Arbiter()
    : m_drivers()
    , m_pool(concurrentHttpReqs, httpRetryCount)
{
    init(Json::Value());
}

Arbiter::Arbiter(const Json::Value& json)
    : m_drivers()
    , m_pool(
            concurrentHttpReqs,
            httpRetryCount,
            json.isMember("arbiter") ?
                json["arbiter"]["verbose"].asBool() : false)
{
    init(json);
}

void Arbiter::init(const Json::Value& json)
{
    using namespace drivers;

    auto fs(Fs::create(m_pool, json["fs"]));
    if (fs) m_drivers["fs"] = std::move(fs);

    auto http(Http::create(m_pool, json["http"]));
    if (http) m_drivers["http"] = std::move(http);

    auto s3(S3::create(m_pool, json["s3"]));
    if (s3) m_drivers["s3"] = std::move(s3);

    auto dropbox(Dropbox::create(m_pool, json["dropbox"]));
    if (dropbox) m_drivers["dropbox"] = std::move(dropbox);
}

void Arbiter::addDriver(const std::string type, std::unique_ptr<Driver> driver)
{
    if (!driver) throw ArbiterError("Cannot add empty driver for " + type);
    m_drivers[type] = std::move(driver);
}

std::string Arbiter::get(const std::string path) const
{
    return getDriver(path).get(stripType(path));
}

std::vector<char> Arbiter::getBinary(const std::string path) const
{
    return getDriver(path).getBinary(stripType(path));
}

std::unique_ptr<std::string> Arbiter::tryGet(std::string path) const
{
    return getDriver(path).tryGet(stripType(path));
}

std::unique_ptr<std::vector<char>> Arbiter::tryGetBinary(std::string path) const
{
    return getDriver(path).tryGetBinary(stripType(path));
}

void Arbiter::put(const std::string path, const std::string& data) const
{
    return getDriver(path).put(stripType(path), data);
}

void Arbiter::put(const std::string path, const std::vector<char>& data) const
{
    return getDriver(path).put(stripType(path), data);
}

bool Arbiter::isRemote(const std::string path) const
{
    return getDriver(path).isRemote();
}

std::vector<std::string> Arbiter::resolve(
        const std::string path,
        const bool verbose) const
{
    return getDriver(path).resolve(stripType(path), verbose);
}

Endpoint Arbiter::getEndpoint(const std::string root) const
{
    return Endpoint(getDriver(root), stripType(root));
}

const Driver& Arbiter::getDriver(const std::string path) const
{
    const auto type(getType(path));

    if (!m_drivers.count(type))
    {
        throw ArbiterError("No driver for " + path);
    }

    return *m_drivers.at(type);
}

std::unique_ptr<fs::LocalHandle> Arbiter::getLocalHandle(
        const std::string path,
        const Endpoint& tempEndpoint) const
{
    std::unique_ptr<fs::LocalHandle> localHandle;

    if (isRemote(path))
    {
        if (tempEndpoint.isRemote())
        {
            throw ArbiterError("Temporary endpoint must be local.");
        }

        std::string name(path);
        std::replace(name.begin(), name.end(), '/', '-');
        std::replace(name.begin(), name.end(), '\\', '-');

        tempEndpoint.putSubpath(name, getBinary(path));

        localHandle.reset(
                new fs::LocalHandle(tempEndpoint.root() + name, true));
    }
    else
    {
        localHandle.reset(
                new fs::LocalHandle(fs::expandTilde(stripType(path)), false));
    }

    return localHandle;
}

std::string Arbiter::getType(const std::string path) const
{
    std::string type("fs");
    const std::size_t pos(path.find(delimiter));

    if (pos != std::string::npos)
    {
        type = path.substr(0, pos);
    }

    return type;
}

std::string Arbiter::stripType(const std::string raw)
{
    std::string result(raw);
    const std::size_t pos(raw.find(delimiter));

    if (pos != std::string::npos)
    {
        result = raw.substr(pos + delimiter.size());
    }

    return result;
}

} // namespace arbiter


// //////////////////////////////////////////////////////////////////////
// End of content of file: arbiter/arbiter.cpp
// //////////////////////////////////////////////////////////////////////






// //////////////////////////////////////////////////////////////////////
// Beginning of content of file: arbiter/driver.cpp
// //////////////////////////////////////////////////////////////////////

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/driver.hpp>

#include <arbiter/arbiter.hpp>
#endif

namespace arbiter
{

std::unique_ptr<std::vector<char>> Driver::tryGetBinary(std::string path) const
{
    std::unique_ptr<std::vector<char>> data(new std::vector<char>());
    if (!get(path, *data)) data.reset();
    return data;
}

std::vector<char> Driver::getBinary(std::string path) const
{
    std::vector<char> data;
    if (!get(path, data)) throw ArbiterError("Could not read file " + path);
    return data;
}

std::unique_ptr<std::string> Driver::tryGet(std::string path) const
{
    std::unique_ptr<std::string> result;
    std::unique_ptr<std::vector<char>> data(tryGetBinary(path));
    if (data) result.reset(new std::string(data->begin(), data->end()));
    return result;
}

std::string Driver::get(const std::string path) const
{
    const std::vector<char> data(getBinary(path));
    return std::string(data.begin(), data.end());
}

void Driver::put(std::string path, const std::string& data) const
{
    put(path, std::vector<char>(data.begin(), data.end()));
}

std::vector<std::string> Driver::resolve(
        std::string path,
        const bool verbose) const
{
    std::vector<std::string> results;

    if (path.size() > 1 && path.back() == '*')
    {
        if (verbose)
        {
            std::cout << "Resolving [" << type() << "]: " << path << " ..." <<
                std::flush;
        }

        results = glob(path, verbose);

        if (verbose)
        {
            std::cout << "\n\tResolved to " << results.size() <<
                " paths." << std::endl;
        }
    }
    else
    {
        if (type() != "fs") path = type() + "://" + path;

        results.push_back(path);
    }

    return results;
}

std::vector<std::string> Driver::glob(std::string path, bool verbose) const
{
    throw ArbiterError("Cannot glob driver for: " + path);
}

} // namespace arbiter


// //////////////////////////////////////////////////////////////////////
// End of content of file: arbiter/driver.cpp
// //////////////////////////////////////////////////////////////////////






// //////////////////////////////////////////////////////////////////////
// Beginning of content of file: arbiter/endpoint.cpp
// //////////////////////////////////////////////////////////////////////

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/endpoint.hpp>

#include <arbiter/arbiter.hpp>
#include <arbiter/driver.hpp>
#endif

namespace arbiter
{

namespace
{
    std::string postfixSlash(std::string path)
    {
        if (path.empty()) throw ArbiterError("Invalid root path");
        if (path.back() != '/') path.push_back('/');
        return path;
    }
}

Endpoint::Endpoint(const Driver& driver, const std::string root)
    : m_driver(driver)
    , m_root(fs::expandTilde(postfixSlash(root)))
{ }

std::string Endpoint::root() const
{
    return m_root;
}

std::string Endpoint::type() const
{
    return m_driver.type();
}

bool Endpoint::isRemote() const
{
    return m_driver.isRemote();
}

std::string Endpoint::getSubpath(const std::string subpath) const
{
    return m_driver.get(fullPath(subpath));
}

std::unique_ptr<std::string> Endpoint::tryGetSubpath(const std::string subpath)
    const
{
    return m_driver.tryGet(fullPath(subpath));
}

std::vector<char> Endpoint::getSubpathBinary(const std::string subpath) const
{
    return m_driver.getBinary(fullPath(subpath));
}

std::unique_ptr<std::vector<char>> Endpoint::tryGetSubpathBinary(
        const std::string subpath) const
{
    return m_driver.tryGetBinary(fullPath(subpath));
}

void Endpoint::putSubpath(
        const std::string subpath,
        const std::string& data) const
{
    m_driver.put(fullPath(subpath), data);
}

void Endpoint::putSubpath(
        const std::string subpath,
        const std::vector<char>& data) const
{
    m_driver.put(fullPath(subpath), data);
}

std::string Endpoint::fullPath(const std::string& subpath) const
{
    return m_root + subpath;
}

} // namespace arbiter


// //////////////////////////////////////////////////////////////////////
// End of content of file: arbiter/endpoint.cpp
// //////////////////////////////////////////////////////////////////////






// //////////////////////////////////////////////////////////////////////
// Beginning of content of file: arbiter/third/json/jsoncpp.cpp
// //////////////////////////////////////////////////////////////////////

/// Json-cpp amalgated source (http://jsoncpp.sourceforge.net/).
/// It is intended to be used with #include "json/json.h"

// //////////////////////////////////////////////////////////////////////
// Beginning of content of file: LICENSE
// //////////////////////////////////////////////////////////////////////

/*
The JsonCpp library's source code, including accompanying documentation,
tests and demonstration applications, are licensed under the following
conditions...

The author (Baptiste Lepilleur) explicitly disclaims copyright in all
jurisdictions which recognize such a disclaimer. In such jurisdictions,
this software is released into the Public Domain.

In jurisdictions which do not recognize Public Domain property (e.g. Germany as of
2010), this software is Copyright (c) 2007-2010 by Baptiste Lepilleur, and is
released under the terms of the MIT License (see below).

In jurisdictions which recognize Public Domain property, the user of this
software may choose to accept it either as 1) Public Domain, 2) under the
conditions of the MIT License (see below), or 3) under the terms of dual
Public Domain/MIT License conditions described here, as they choose.

The MIT License is about as close to Public Domain as a license can get, and is
described in clear, concise terms at:

   http://en.wikipedia.org/wiki/MIT_License

The full text of the MIT License follows:

========================================================================
Copyright (c) 2007-2010 Baptiste Lepilleur

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use, copy,
modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
========================================================================
(END LICENSE TEXT)

The MIT license is compatible with both the GPL and commercial
software, affording one all of the rights of Public Domain with the
minor nuisance of being required to keep the above copyright notice
and license text in the source code. Note also that by accepting the
Public Domain "license" you can re-license your copy using whatever
license you like.

*/

// //////////////////////////////////////////////////////////////////////
// End of content of file: LICENSE
// //////////////////////////////////////////////////////////////////////






#include "json/json.hpp"

#ifndef JSON_IS_AMALGAMATION
#error "Compile with -I PATH_TO_JSON_DIRECTORY"
#endif


// //////////////////////////////////////////////////////////////////////
// Beginning of content of file: src/lib_json/json_tool.h
// //////////////////////////////////////////////////////////////////////

// Copyright 2007-2010 Baptiste Lepilleur
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.
// See file LICENSE for detail or copy at http://jsoncpp.sourceforge.net/LICENSE

#ifndef LIB_JSONCPP_JSON_TOOL_H_INCLUDED
#define LIB_JSONCPP_JSON_TOOL_H_INCLUDED

/* This header provides common string manipulation support, such as UTF-8,
 * portable conversion from/to string...
 *
 * It is an internal header that must not be exposed.
 */

namespace Json {

/// Converts a unicode code-point to UTF-8.
static inline std::string codePointToUTF8(unsigned int cp) {
  std::string result;

  // based on description from http://en.wikipedia.org/wiki/UTF-8

  if (cp <= 0x7f) {
    result.resize(1);
    result[0] = static_cast<char>(cp);
  } else if (cp <= 0x7FF) {
    result.resize(2);
    result[1] = static_cast<char>(0x80 | (0x3f & cp));
    result[0] = static_cast<char>(0xC0 | (0x1f & (cp >> 6)));
  } else if (cp <= 0xFFFF) {
    result.resize(3);
    result[2] = static_cast<char>(0x80 | (0x3f & cp));
    result[1] = static_cast<char>(0x80 | (0x3f & (cp >> 6)));
    result[0] = static_cast<char>(0xE0 | (0xf & (cp >> 12)));
  } else if (cp <= 0x10FFFF) {
    result.resize(4);
    result[3] = static_cast<char>(0x80 | (0x3f & cp));
    result[2] = static_cast<char>(0x80 | (0x3f & (cp >> 6)));
    result[1] = static_cast<char>(0x80 | (0x3f & (cp >> 12)));
    result[0] = static_cast<char>(0xF0 | (0x7 & (cp >> 18)));
  }

  return result;
}

/// Returns true if ch is a control character (in range [1,31]).
static inline bool isControlCharacter(char ch) { return ch > 0 && ch <= 0x1F; }

enum {
  /// Constant that specify the size of the buffer that must be passed to
  /// uintToString.
  uintToStringBufferSize = 3 * sizeof(LargestUInt) + 1
};

// Defines a char buffer for use with uintToString().
typedef char UIntToStringBuffer[uintToStringBufferSize];

/** Converts an unsigned integer to string.
 * @param value Unsigned interger to convert to string
 * @param current Input/Output string buffer.
 *        Must have at least uintToStringBufferSize chars free.
 */
static inline void uintToString(LargestUInt value, char*& current) {
  *--current = 0;
  do {
    *--current = static_cast<signed char>(value % 10U + static_cast<unsigned>('0'));
    value /= 10;
  } while (value != 0);
}

/** Change ',' to '.' everywhere in buffer.
 *
 * We had a sophisticated way, but it did not work in WinCE.
 * @see https://github.com/open-source-parsers/jsoncpp/pull/9
 */
static inline void fixNumericLocale(char* begin, char* end) {
  while (begin < end) {
    if (*begin == ',') {
      *begin = '.';
    }
    ++begin;
  }
}

} // namespace Json {

#endif // LIB_JSONCPP_JSON_TOOL_H_INCLUDED

// //////////////////////////////////////////////////////////////////////
// End of content of file: src/lib_json/json_tool.h
// //////////////////////////////////////////////////////////////////////






// //////////////////////////////////////////////////////////////////////
// Beginning of content of file: src/lib_json/json_reader.cpp
// //////////////////////////////////////////////////////////////////////

// Copyright 2007-2011 Baptiste Lepilleur
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.
// See file LICENSE for detail or copy at http://jsoncpp.sourceforge.net/LICENSE

#if !defined(JSON_IS_AMALGAMATION)
#include <json/assertions.h>
#include <json/reader.h>
#include <json/value.h>
#include "json_tool.h"
#endif // if !defined(JSON_IS_AMALGAMATION)
#include <utility>
#include <cstdio>
#include <cassert>
#include <cstring>
#include <istream>
#include <sstream>
#include <memory>
#include <set>
#include <limits>

#if defined(_MSC_VER)
#if !defined(WINCE) && defined(__STDC_SECURE_LIB__) && _MSC_VER >= 1500 // VC++ 9.0 and above
#define snprintf sprintf_s
#elif _MSC_VER >= 1900 // VC++ 14.0 and above
#define snprintf std::snprintf
#else
#define snprintf _snprintf
#endif
#elif defined(__ANDROID__) || defined(__QNXNTO__)
#define snprintf snprintf
#elif __cplusplus >= 201103L
#define snprintf std::snprintf
#endif

#if defined(__QNXNTO__)
#define sscanf std::sscanf
#endif

#if defined(_MSC_VER) && _MSC_VER >= 1400 // VC++ 8.0
// Disable warning about strdup being deprecated.
#pragma warning(disable : 4996)
#endif

static int const stackLimit_g = 1000;
static int       stackDepth_g = 0;  // see readValue()

namespace Json {

#if __cplusplus >= 201103L || (defined(_CPPLIB_VER) && _CPPLIB_VER >= 520)
typedef std::unique_ptr<CharReader> CharReaderPtr;
#else
typedef std::auto_ptr<CharReader>   CharReaderPtr;
#endif

// Implementation of class Features
// ////////////////////////////////

Features::Features()
    : allowComments_(true), strictRoot_(false),
      allowDroppedNullPlaceholders_(false), allowNumericKeys_(false) {}

Features Features::all() { return Features(); }

Features Features::strictMode() {
  Features features;
  features.allowComments_ = false;
  features.strictRoot_ = true;
  features.allowDroppedNullPlaceholders_ = false;
  features.allowNumericKeys_ = false;
  return features;
}

// Implementation of class Reader
// ////////////////////////////////

static bool containsNewLine(Reader::Location begin, Reader::Location end) {
  for (; begin < end; ++begin)
    if (*begin == '\n' || *begin == '\r')
      return true;
  return false;
}

// Class Reader
// //////////////////////////////////////////////////////////////////

Reader::Reader()
    : errors_(), document_(), begin_(), end_(), current_(), lastValueEnd_(),
      lastValue_(), commentsBefore_(), features_(Features::all()),
      collectComments_() {}

Reader::Reader(const Features& features)
    : errors_(), document_(), begin_(), end_(), current_(), lastValueEnd_(),
      lastValue_(), commentsBefore_(), features_(features), collectComments_() {
}

bool
Reader::parse(const std::string& document, Value& root, bool collectComments) {
  document_ = document;
  const char* begin = document_.c_str();
  const char* end = begin + document_.length();
  return parse(begin, end, root, collectComments);
}

bool Reader::parse(std::istream& sin, Value& root, bool collectComments) {
  // std::istream_iterator<char> begin(sin);
  // std::istream_iterator<char> end;
  // Those would allow streamed input from a file, if parse() were a
  // template function.

  // Since std::string is reference-counted, this at least does not
  // create an extra copy.
  std::string doc;
  std::getline(sin, doc, (char)EOF);
  return parse(doc, root, collectComments);
}

bool Reader::parse(const char* beginDoc,
                   const char* endDoc,
                   Value& root,
                   bool collectComments) {
  if (!features_.allowComments_) {
    collectComments = false;
  }

  begin_ = beginDoc;
  end_ = endDoc;
  collectComments_ = collectComments;
  current_ = begin_;
  lastValueEnd_ = 0;
  lastValue_ = 0;
  commentsBefore_ = "";
  errors_.clear();
  while (!nodes_.empty())
    nodes_.pop();
  nodes_.push(&root);

  stackDepth_g = 0;  // Yes, this is bad coding, but options are limited.
  bool successful = readValue();
  Token token;
  skipCommentTokens(token);
  if (collectComments_ && !commentsBefore_.empty())
    root.setComment(commentsBefore_, commentAfter);
  if (features_.strictRoot_) {
    if (!root.isArray() && !root.isObject()) {
      // Set error location to start of doc, ideally should be first token found
      // in doc
      token.type_ = tokenError;
      token.start_ = beginDoc;
      token.end_ = endDoc;
      addError(
          "A valid JSON document must be either an array or an object value.",
          token);
      return false;
    }
  }
  return successful;
}

bool Reader::readValue() {
  // This is a non-reentrant way to support a stackLimit. Terrible!
  // But this deprecated class has a security problem: Bad input can
  // cause a seg-fault. This seems like a fair, binary-compatible way
  // to prevent the problem.
  if (stackDepth_g >= stackLimit_g) throwRuntimeError("Exceeded stackLimit in readValue().");
  ++stackDepth_g;

  Token token;
  skipCommentTokens(token);
  bool successful = true;

  if (collectComments_ && !commentsBefore_.empty()) {
    currentValue().setComment(commentsBefore_, commentBefore);
    commentsBefore_ = "";
  }

  switch (token.type_) {
  case tokenObjectBegin:
    successful = readObject(token);
    currentValue().setOffsetLimit(current_ - begin_);
    break;
  case tokenArrayBegin:
    successful = readArray(token);
    currentValue().setOffsetLimit(current_ - begin_);
    break;
  case tokenNumber:
    successful = decodeNumber(token);
    break;
  case tokenString:
    successful = decodeString(token);
    break;
  case tokenTrue:
    {
    Value v(true);
    currentValue().swapPayload(v);
    currentValue().setOffsetStart(token.start_ - begin_);
    currentValue().setOffsetLimit(token.end_ - begin_);
    }
    break;
  case tokenFalse:
    {
    Value v(false);
    currentValue().swapPayload(v);
    currentValue().setOffsetStart(token.start_ - begin_);
    currentValue().setOffsetLimit(token.end_ - begin_);
    }
    break;
  case tokenNull:
    {
    Value v;
    currentValue().swapPayload(v);
    currentValue().setOffsetStart(token.start_ - begin_);
    currentValue().setOffsetLimit(token.end_ - begin_);
    }
    break;
  case tokenArraySeparator:
  case tokenObjectEnd:
  case tokenArrayEnd:
    if (features_.allowDroppedNullPlaceholders_) {
      // "Un-read" the current token and mark the current value as a null
      // token.
      current_--;
      Value v;
      currentValue().swapPayload(v);
      currentValue().setOffsetStart(current_ - begin_ - 1);
      currentValue().setOffsetLimit(current_ - begin_);
      break;
    } // Else, fall through...
  default:
    currentValue().setOffsetStart(token.start_ - begin_);
    currentValue().setOffsetLimit(token.end_ - begin_);
    return addError("Syntax error: value, object or array expected.", token);
  }

  if (collectComments_) {
    lastValueEnd_ = current_;
    lastValue_ = &currentValue();
  }

  --stackDepth_g;
  return successful;
}

void Reader::skipCommentTokens(Token& token) {
  if (features_.allowComments_) {
    do {
      readToken(token);
    } while (token.type_ == tokenComment);
  } else {
    readToken(token);
  }
}

bool Reader::readToken(Token& token) {
  skipSpaces();
  token.start_ = current_;
  Char c = getNextChar();
  bool ok = true;
  switch (c) {
  case '{':
    token.type_ = tokenObjectBegin;
    break;
  case '}':
    token.type_ = tokenObjectEnd;
    break;
  case '[':
    token.type_ = tokenArrayBegin;
    break;
  case ']':
    token.type_ = tokenArrayEnd;
    break;
  case '"':
    token.type_ = tokenString;
    ok = readString();
    break;
  case '/':
    token.type_ = tokenComment;
    ok = readComment();
    break;
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
  case '-':
    token.type_ = tokenNumber;
    readNumber();
    break;
  case 't':
    token.type_ = tokenTrue;
    ok = match("rue", 3);
    break;
  case 'f':
    token.type_ = tokenFalse;
    ok = match("alse", 4);
    break;
  case 'n':
    token.type_ = tokenNull;
    ok = match("ull", 3);
    break;
  case ',':
    token.type_ = tokenArraySeparator;
    break;
  case ':':
    token.type_ = tokenMemberSeparator;
    break;
  case 0:
    token.type_ = tokenEndOfStream;
    break;
  default:
    ok = false;
    break;
  }
  if (!ok)
    token.type_ = tokenError;
  token.end_ = current_;
  return true;
}

void Reader::skipSpaces() {
  while (current_ != end_) {
    Char c = *current_;
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
      ++current_;
    else
      break;
  }
}

bool Reader::match(Location pattern, int patternLength) {
  if (end_ - current_ < patternLength)
    return false;
  int index = patternLength;
  while (index--)
    if (current_[index] != pattern[index])
      return false;
  current_ += patternLength;
  return true;
}

bool Reader::readComment() {
  Location commentBegin = current_ - 1;
  Char c = getNextChar();
  bool successful = false;
  if (c == '*')
    successful = readCStyleComment();
  else if (c == '/')
    successful = readCppStyleComment();
  if (!successful)
    return false;

  if (collectComments_) {
    CommentPlacement placement = commentBefore;
    if (lastValueEnd_ && !containsNewLine(lastValueEnd_, commentBegin)) {
      if (c != '*' || !containsNewLine(commentBegin, current_))
        placement = commentAfterOnSameLine;
    }

    addComment(commentBegin, current_, placement);
  }
  return true;
}

static std::string normalizeEOL(Reader::Location begin, Reader::Location end) {
  std::string normalized;
  normalized.reserve(end - begin);
  Reader::Location current = begin;
  while (current != end) {
    char c = *current++;
    if (c == '\r') {
      if (current != end && *current == '\n')
         // convert dos EOL
         ++current;
      // convert Mac EOL
      normalized += '\n';
    } else {
      normalized += c;
    }
  }
  return normalized;
}

void
Reader::addComment(Location begin, Location end, CommentPlacement placement) {
  assert(collectComments_);
  const std::string& normalized = normalizeEOL(begin, end);
  if (placement == commentAfterOnSameLine) {
    assert(lastValue_ != 0);
    lastValue_->setComment(normalized, placement);
  } else {
    commentsBefore_ += normalized;
  }
}

bool Reader::readCStyleComment() {
  while (current_ != end_) {
    Char c = getNextChar();
    if (c == '*' && *current_ == '/')
      break;
  }
  return getNextChar() == '/';
}

bool Reader::readCppStyleComment() {
  while (current_ != end_) {
    Char c = getNextChar();
    if (c == '\n')
      break;
    if (c == '\r') {
      // Consume DOS EOL. It will be normalized in addComment.
      if (current_ != end_ && *current_ == '\n')
        getNextChar();
      // Break on Moc OS 9 EOL.
      break;
    }
  }
  return true;
}

void Reader::readNumber() {
  const char *p = current_;
  char c = '0'; // stopgap for already consumed character
  // integral part
  while (c >= '0' && c <= '9')
    c = (current_ = p) < end_ ? *p++ : 0;
  // fractional part
  if (c == '.') {
    c = (current_ = p) < end_ ? *p++ : 0;
    while (c >= '0' && c <= '9')
      c = (current_ = p) < end_ ? *p++ : 0;
  }
  // exponential part
  if (c == 'e' || c == 'E') {
    c = (current_ = p) < end_ ? *p++ : 0;
    if (c == '+' || c == '-')
      c = (current_ = p) < end_ ? *p++ : 0;
    while (c >= '0' && c <= '9')
      c = (current_ = p) < end_ ? *p++ : 0;
  }
}

bool Reader::readString() {
  Char c = 0;
  while (current_ != end_) {
    c = getNextChar();
    if (c == '\\')
      getNextChar();
    else if (c == '"')
      break;
  }
  return c == '"';
}

bool Reader::readObject(Token& tokenStart) {
  Token tokenName;
  std::string name;
  Value init(objectValue);
  currentValue().swapPayload(init);
  currentValue().setOffsetStart(tokenStart.start_ - begin_);
  while (readToken(tokenName)) {
    bool initialTokenOk = true;
    while (tokenName.type_ == tokenComment && initialTokenOk)
      initialTokenOk = readToken(tokenName);
    if (!initialTokenOk)
      break;
    if (tokenName.type_ == tokenObjectEnd && name.empty()) // empty object
      return true;
    name = "";
    if (tokenName.type_ == tokenString) {
      if (!decodeString(tokenName, name))
        return recoverFromError(tokenObjectEnd);
    } else if (tokenName.type_ == tokenNumber && features_.allowNumericKeys_) {
      Value numberName;
      if (!decodeNumber(tokenName, numberName))
        return recoverFromError(tokenObjectEnd);
      name = numberName.asString();
    } else {
      break;
    }

    Token colon;
    if (!readToken(colon) || colon.type_ != tokenMemberSeparator) {
      return addErrorAndRecover(
          "Missing ':' after object member name", colon, tokenObjectEnd);
    }
    Value& value = currentValue()[name];
    nodes_.push(&value);
    bool ok = readValue();
    nodes_.pop();
    if (!ok) // error already set
      return recoverFromError(tokenObjectEnd);

    Token comma;
    if (!readToken(comma) ||
        (comma.type_ != tokenObjectEnd && comma.type_ != tokenArraySeparator &&
         comma.type_ != tokenComment)) {
      return addErrorAndRecover(
          "Missing ',' or '}' in object declaration", comma, tokenObjectEnd);
    }
    bool finalizeTokenOk = true;
    while (comma.type_ == tokenComment && finalizeTokenOk)
      finalizeTokenOk = readToken(comma);
    if (comma.type_ == tokenObjectEnd)
      return true;
  }
  return addErrorAndRecover(
      "Missing '}' or object member name", tokenName, tokenObjectEnd);
}

bool Reader::readArray(Token& tokenStart) {
  Value init(arrayValue);
  currentValue().swapPayload(init);
  currentValue().setOffsetStart(tokenStart.start_ - begin_);
  skipSpaces();
  if (*current_ == ']') // empty array
  {
    Token endArray;
    readToken(endArray);
    return true;
  }
  int index = 0;
  for (;;) {
    Value& value = currentValue()[index++];
    nodes_.push(&value);
    bool ok = readValue();
    nodes_.pop();
    if (!ok) // error already set
      return recoverFromError(tokenArrayEnd);

    Token token;
    // Accept Comment after last item in the array.
    ok = readToken(token);
    while (token.type_ == tokenComment && ok) {
      ok = readToken(token);
    }
    bool badTokenType =
        (token.type_ != tokenArraySeparator && token.type_ != tokenArrayEnd);
    if (!ok || badTokenType) {
      return addErrorAndRecover(
          "Missing ',' or ']' in array declaration", token, tokenArrayEnd);
    }
    if (token.type_ == tokenArrayEnd)
      break;
  }
  return true;
}

bool Reader::decodeNumber(Token& token) {
  Value decoded;
  if (!decodeNumber(token, decoded))
    return false;
  currentValue().swapPayload(decoded);
  currentValue().setOffsetStart(token.start_ - begin_);
  currentValue().setOffsetLimit(token.end_ - begin_);
  return true;
}

bool Reader::decodeNumber(Token& token, Value& decoded) {
  // Attempts to parse the number as an integer. If the number is
  // larger than the maximum supported value of an integer then
  // we decode the number as a double.
  Location current = token.start_;
  bool isNegative = *current == '-';
  if (isNegative)
    ++current;
  // TODO: Help the compiler do the div and mod at compile time or get rid of them.
  Value::LargestUInt maxIntegerValue =
      isNegative ? Value::LargestUInt(Value::maxLargestInt) + 1
                 : Value::maxLargestUInt;
  Value::LargestUInt threshold = maxIntegerValue / 10;
  Value::LargestUInt value = 0;
  while (current < token.end_) {
    Char c = *current++;
    if (c < '0' || c > '9')
      return decodeDouble(token, decoded);
    Value::UInt digit(c - '0');
    if (value >= threshold) {
      // We've hit or exceeded the max value divided by 10 (rounded down). If
      // a) we've only just touched the limit, b) this is the last digit, and
      // c) it's small enough to fit in that rounding delta, we're okay.
      // Otherwise treat this number as a double to avoid overflow.
      if (value > threshold || current != token.end_ ||
          digit > maxIntegerValue % 10) {
        return decodeDouble(token, decoded);
      }
    }
    value = value * 10 + digit;
  }
  if (isNegative && value == maxIntegerValue)
    decoded = Value::minLargestInt;
  else if (isNegative)
    decoded = -Value::LargestInt(value);
  else if (value <= Value::LargestUInt(Value::maxInt))
    decoded = Value::LargestInt(value);
  else
    decoded = value;
  return true;
}

bool Reader::decodeDouble(Token& token) {
  Value decoded;
  if (!decodeDouble(token, decoded))
    return false;
  currentValue().swapPayload(decoded);
  currentValue().setOffsetStart(token.start_ - begin_);
  currentValue().setOffsetLimit(token.end_ - begin_);
  return true;
}

bool Reader::decodeDouble(Token& token, Value& decoded) {
  double value = 0;
  std::string buffer(token.start_, token.end_);
  std::istringstream is(buffer);
  if (!(is >> value))
    return addError("'" + std::string(token.start_, token.end_) +
                        "' is not a number.",
                    token);
  decoded = value;
  return true;
}

bool Reader::decodeString(Token& token) {
  std::string decoded_string;
  if (!decodeString(token, decoded_string))
    return false;
  Value decoded(decoded_string);
  currentValue().swapPayload(decoded);
  currentValue().setOffsetStart(token.start_ - begin_);
  currentValue().setOffsetLimit(token.end_ - begin_);
  return true;
}

bool Reader::decodeString(Token& token, std::string& decoded) {
  decoded.reserve(token.end_ - token.start_ - 2);
  Location current = token.start_ + 1; // skip '"'
  Location end = token.end_ - 1;       // do not include '"'
  while (current != end) {
    Char c = *current++;
    if (c == '"')
      break;
    else if (c == '\\') {
      if (current == end)
        return addError("Empty escape sequence in string", token, current);
      Char escape = *current++;
      switch (escape) {
      case '"':
        decoded += '"';
        break;
      case '/':
        decoded += '/';
        break;
      case '\\':
        decoded += '\\';
        break;
      case 'b':
        decoded += '\b';
        break;
      case 'f':
        decoded += '\f';
        break;
      case 'n':
        decoded += '\n';
        break;
      case 'r':
        decoded += '\r';
        break;
      case 't':
        decoded += '\t';
        break;
      case 'u': {
        unsigned int unicode;
        if (!decodeUnicodeCodePoint(token, current, end, unicode))
          return false;
        decoded += codePointToUTF8(unicode);
      } break;
      default:
        return addError("Bad escape sequence in string", token, current);
      }
    } else {
      decoded += c;
    }
  }
  return true;
}

bool Reader::decodeUnicodeCodePoint(Token& token,
                                    Location& current,
                                    Location end,
                                    unsigned int& unicode) {

  if (!decodeUnicodeEscapeSequence(token, current, end, unicode))
    return false;
  if (unicode >= 0xD800 && unicode <= 0xDBFF) {
    // surrogate pairs
    if (end - current < 6)
      return addError(
          "additional six characters expected to parse unicode surrogate pair.",
          token,
          current);
    unsigned int surrogatePair;
    if (*(current++) == '\\' && *(current++) == 'u') {
      if (decodeUnicodeEscapeSequence(token, current, end, surrogatePair)) {
        unicode = 0x10000 + ((unicode & 0x3FF) << 10) + (surrogatePair & 0x3FF);
      } else
        return false;
    } else
      return addError("expecting another \\u token to begin the second half of "
                      "a unicode surrogate pair",
                      token,
                      current);
  }
  return true;
}

bool Reader::decodeUnicodeEscapeSequence(Token& token,
                                         Location& current,
                                         Location end,
                                         unsigned int& unicode) {
  if (end - current < 4)
    return addError(
        "Bad unicode escape sequence in string: four digits expected.",
        token,
        current);
  unicode = 0;
  for (int index = 0; index < 4; ++index) {
    Char c = *current++;
    unicode *= 16;
    if (c >= '0' && c <= '9')
      unicode += c - '0';
    else if (c >= 'a' && c <= 'f')
      unicode += c - 'a' + 10;
    else if (c >= 'A' && c <= 'F')
      unicode += c - 'A' + 10;
    else
      return addError(
          "Bad unicode escape sequence in string: hexadecimal digit expected.",
          token,
          current);
  }
  return true;
}

bool
Reader::addError(const std::string& message, Token& token, Location extra) {
  ErrorInfo info;
  info.token_ = token;
  info.message_ = message;
  info.extra_ = extra;
  errors_.push_back(info);
  return false;
}

bool Reader::recoverFromError(TokenType skipUntilToken) {
  int errorCount = int(errors_.size());
  Token skip;
  for (;;) {
    if (!readToken(skip))
      errors_.resize(errorCount); // discard errors caused by recovery
    if (skip.type_ == skipUntilToken || skip.type_ == tokenEndOfStream)
      break;
  }
  errors_.resize(errorCount);
  return false;
}

bool Reader::addErrorAndRecover(const std::string& message,
                                Token& token,
                                TokenType skipUntilToken) {
  addError(message, token);
  return recoverFromError(skipUntilToken);
}

Value& Reader::currentValue() { return *(nodes_.top()); }

Reader::Char Reader::getNextChar() {
  if (current_ == end_)
    return 0;
  return *current_++;
}

void Reader::getLocationLineAndColumn(Location location,
                                      int& line,
                                      int& column) const {
  Location current = begin_;
  Location lastLineStart = current;
  line = 0;
  while (current < location && current != end_) {
    Char c = *current++;
    if (c == '\r') {
      if (*current == '\n')
        ++current;
      lastLineStart = current;
      ++line;
    } else if (c == '\n') {
      lastLineStart = current;
      ++line;
    }
  }
  // column & line start at 1
  column = int(location - lastLineStart) + 1;
  ++line;
}

std::string Reader::getLocationLineAndColumn(Location location) const {
  int line, column;
  getLocationLineAndColumn(location, line, column);
  char buffer[18 + 16 + 16 + 1];
  snprintf(buffer, sizeof(buffer), "Line %d, Column %d", line, column);
  return buffer;
}

// Deprecated. Preserved for backward compatibility
std::string Reader::getFormatedErrorMessages() const {
  return getFormattedErrorMessages();
}

std::string Reader::getFormattedErrorMessages() const {
  std::string formattedMessage;
  for (Errors::const_iterator itError = errors_.begin();
       itError != errors_.end();
       ++itError) {
    const ErrorInfo& error = *itError;
    formattedMessage +=
        "* " + getLocationLineAndColumn(error.token_.start_) + "\n";
    formattedMessage += "  " + error.message_ + "\n";
    if (error.extra_)
      formattedMessage +=
          "See " + getLocationLineAndColumn(error.extra_) + " for detail.\n";
  }
  return formattedMessage;
}

std::vector<Reader::StructuredError> Reader::getStructuredErrors() const {
  std::vector<Reader::StructuredError> allErrors;
  for (Errors::const_iterator itError = errors_.begin();
       itError != errors_.end();
       ++itError) {
    const ErrorInfo& error = *itError;
    Reader::StructuredError structured;
    structured.offset_start = error.token_.start_ - begin_;
    structured.offset_limit = error.token_.end_ - begin_;
    structured.message = error.message_;
    allErrors.push_back(structured);
  }
  return allErrors;
}

bool Reader::pushError(const Value& value, const std::string& message) {
  size_t length = end_ - begin_;
  if(value.getOffsetStart() > length
    || value.getOffsetLimit() > length)
    return false;
  Token token;
  token.type_ = tokenError;
  token.start_ = begin_ + value.getOffsetStart();
  token.end_ = end_ + value.getOffsetLimit();
  ErrorInfo info;
  info.token_ = token;
  info.message_ = message;
  info.extra_ = 0;
  errors_.push_back(info);
  return true;
}

bool Reader::pushError(const Value& value, const std::string& message, const Value& extra) {
  size_t length = end_ - begin_;
  if(value.getOffsetStart() > length
    || value.getOffsetLimit() > length
    || extra.getOffsetLimit() > length)
    return false;
  Token token;
  token.type_ = tokenError;
  token.start_ = begin_ + value.getOffsetStart();
  token.end_ = begin_ + value.getOffsetLimit();
  ErrorInfo info;
  info.token_ = token;
  info.message_ = message;
  info.extra_ = begin_ + extra.getOffsetStart();
  errors_.push_back(info);
  return true;
}

bool Reader::good() const {
  return !errors_.size();
}

// exact copy of Features
class OurFeatures {
public:
  static OurFeatures all();
  bool allowComments_;
  bool strictRoot_;
  bool allowDroppedNullPlaceholders_;
  bool allowNumericKeys_;
  bool allowSingleQuotes_;
  bool failIfExtra_;
  bool rejectDupKeys_;
  bool allowSpecialFloats_;
  int stackLimit_;
};  // OurFeatures

// exact copy of Implementation of class Features
// ////////////////////////////////

OurFeatures OurFeatures::all() { return OurFeatures(); }

// Implementation of class Reader
// ////////////////////////////////

// exact copy of Reader, renamed to OurReader
class OurReader {
public:
  typedef char Char;
  typedef const Char* Location;
  struct StructuredError {
    size_t offset_start;
    size_t offset_limit;
    std::string message;
  };

  OurReader(OurFeatures const& features);
  bool parse(const char* beginDoc,
             const char* endDoc,
             Value& root,
             bool collectComments = true);
  std::string getFormattedErrorMessages() const;
  std::vector<StructuredError> getStructuredErrors() const;
  bool pushError(const Value& value, const std::string& message);
  bool pushError(const Value& value, const std::string& message, const Value& extra);
  bool good() const;

private:
  OurReader(OurReader const&);  // no impl
  void operator=(OurReader const&);  // no impl

  enum TokenType {
    tokenEndOfStream = 0,
    tokenObjectBegin,
    tokenObjectEnd,
    tokenArrayBegin,
    tokenArrayEnd,
    tokenString,
    tokenNumber,
    tokenTrue,
    tokenFalse,
    tokenNull,
    tokenNaN,
    tokenPosInf,
    tokenNegInf,
    tokenArraySeparator,
    tokenMemberSeparator,
    tokenComment,
    tokenError
  };

  class Token {
  public:
    TokenType type_;
    Location start_;
    Location end_;
  };

  class ErrorInfo {
  public:
    Token token_;
    std::string message_;
    Location extra_;
  };

  typedef std::deque<ErrorInfo> Errors;

  bool readToken(Token& token);
  void skipSpaces();
  bool match(Location pattern, int patternLength);
  bool readComment();
  bool readCStyleComment();
  bool readCppStyleComment();
  bool readString();
  bool readStringSingleQuote();
  bool readNumber(bool checkInf);
  bool readValue();
  bool readObject(Token& token);
  bool readArray(Token& token);
  bool decodeNumber(Token& token);
  bool decodeNumber(Token& token, Value& decoded);
  bool decodeString(Token& token);
  bool decodeString(Token& token, std::string& decoded);
  bool decodeDouble(Token& token);
  bool decodeDouble(Token& token, Value& decoded);
  bool decodeUnicodeCodePoint(Token& token,
                              Location& current,
                              Location end,
                              unsigned int& unicode);
  bool decodeUnicodeEscapeSequence(Token& token,
                                   Location& current,
                                   Location end,
                                   unsigned int& unicode);
  bool addError(const std::string& message, Token& token, Location extra = 0);
  bool recoverFromError(TokenType skipUntilToken);
  bool addErrorAndRecover(const std::string& message,
                          Token& token,
                          TokenType skipUntilToken);
  void skipUntilSpace();
  Value& currentValue();
  Char getNextChar();
  void
  getLocationLineAndColumn(Location location, int& line, int& column) const;
  std::string getLocationLineAndColumn(Location location) const;
  void addComment(Location begin, Location end, CommentPlacement placement);
  void skipCommentTokens(Token& token);

  typedef std::stack<Value*> Nodes;
  Nodes nodes_;
  Errors errors_;
  std::string document_;
  Location begin_;
  Location end_;
  Location current_;
  Location lastValueEnd_;
  Value* lastValue_;
  std::string commentsBefore_;
  int stackDepth_;

  OurFeatures const features_;
  bool collectComments_;
};  // OurReader

// complete copy of Read impl, for OurReader

OurReader::OurReader(OurFeatures const& features)
    : errors_(), document_(), begin_(), end_(), current_(), lastValueEnd_(),
      lastValue_(), commentsBefore_(),
      stackDepth_(0),
      features_(features), collectComments_() {
}

bool OurReader::parse(const char* beginDoc,
                   const char* endDoc,
                   Value& root,
                   bool collectComments) {
  if (!features_.allowComments_) {
    collectComments = false;
  }

  begin_ = beginDoc;
  end_ = endDoc;
  collectComments_ = collectComments;
  current_ = begin_;
  lastValueEnd_ = 0;
  lastValue_ = 0;
  commentsBefore_ = "";
  errors_.clear();
  while (!nodes_.empty())
    nodes_.pop();
  nodes_.push(&root);

  stackDepth_ = 0;
  bool successful = readValue();
  Token token;
  skipCommentTokens(token);
  if (features_.failIfExtra_) {
    if (token.type_ != tokenError && token.type_ != tokenEndOfStream) {
      addError("Extra non-whitespace after JSON value.", token);
      return false;
    }
  }
  if (collectComments_ && !commentsBefore_.empty())
    root.setComment(commentsBefore_, commentAfter);
  if (features_.strictRoot_) {
    if (!root.isArray() && !root.isObject()) {
      // Set error location to start of doc, ideally should be first token found
      // in doc
      token.type_ = tokenError;
      token.start_ = beginDoc;
      token.end_ = endDoc;
      addError(
          "A valid JSON document must be either an array or an object value.",
          token);
      return false;
    }
  }
  return successful;
}

bool OurReader::readValue() {
  if (stackDepth_ >= features_.stackLimit_) throwRuntimeError("Exceeded stackLimit in readValue().");
  ++stackDepth_;
  Token token;
  skipCommentTokens(token);
  bool successful = true;

  if (collectComments_ && !commentsBefore_.empty()) {
    currentValue().setComment(commentsBefore_, commentBefore);
    commentsBefore_ = "";
  }

  switch (token.type_) {
  case tokenObjectBegin:
    successful = readObject(token);
    currentValue().setOffsetLimit(current_ - begin_);
    break;
  case tokenArrayBegin:
    successful = readArray(token);
    currentValue().setOffsetLimit(current_ - begin_);
    break;
  case tokenNumber:
    successful = decodeNumber(token);
    break;
  case tokenString:
    successful = decodeString(token);
    break;
  case tokenTrue:
    {
    Value v(true);
    currentValue().swapPayload(v);
    currentValue().setOffsetStart(token.start_ - begin_);
    currentValue().setOffsetLimit(token.end_ - begin_);
    }
    break;
  case tokenFalse:
    {
    Value v(false);
    currentValue().swapPayload(v);
    currentValue().setOffsetStart(token.start_ - begin_);
    currentValue().setOffsetLimit(token.end_ - begin_);
    }
    break;
  case tokenNull:
    {
    Value v;
    currentValue().swapPayload(v);
    currentValue().setOffsetStart(token.start_ - begin_);
    currentValue().setOffsetLimit(token.end_ - begin_);
    }
    break;
  case tokenNaN:
    {
    Value v(std::numeric_limits<double>::quiet_NaN());
    currentValue().swapPayload(v);
    currentValue().setOffsetStart(token.start_ - begin_);
    currentValue().setOffsetLimit(token.end_ - begin_);
    }
    break;
  case tokenPosInf:
    {
    Value v(std::numeric_limits<double>::infinity());
    currentValue().swapPayload(v);
    currentValue().setOffsetStart(token.start_ - begin_);
    currentValue().setOffsetLimit(token.end_ - begin_);
    }
    break;
  case tokenNegInf:
    {
    Value v(-std::numeric_limits<double>::infinity());
    currentValue().swapPayload(v);
    currentValue().setOffsetStart(token.start_ - begin_);
    currentValue().setOffsetLimit(token.end_ - begin_);
    }
    break;
  case tokenArraySeparator:
  case tokenObjectEnd:
  case tokenArrayEnd:
    if (features_.allowDroppedNullPlaceholders_) {
      // "Un-read" the current token and mark the current value as a null
      // token.
      current_--;
      Value v;
      currentValue().swapPayload(v);
      currentValue().setOffsetStart(current_ - begin_ - 1);
      currentValue().setOffsetLimit(current_ - begin_);
      break;
    } // else, fall through ...
  default:
    currentValue().setOffsetStart(token.start_ - begin_);
    currentValue().setOffsetLimit(token.end_ - begin_);
    return addError("Syntax error: value, object or array expected.", token);
  }

  if (collectComments_) {
    lastValueEnd_ = current_;
    lastValue_ = &currentValue();
  }

  --stackDepth_;
  return successful;
}

void OurReader::skipCommentTokens(Token& token) {
  if (features_.allowComments_) {
    do {
      readToken(token);
    } while (token.type_ == tokenComment);
  } else {
    readToken(token);
  }
}

bool OurReader::readToken(Token& token) {
  skipSpaces();
  token.start_ = current_;
  Char c = getNextChar();
  bool ok = true;
  switch (c) {
  case '{':
    token.type_ = tokenObjectBegin;
    break;
  case '}':
    token.type_ = tokenObjectEnd;
    break;
  case '[':
    token.type_ = tokenArrayBegin;
    break;
  case ']':
    token.type_ = tokenArrayEnd;
    break;
  case '"':
    token.type_ = tokenString;
    ok = readString();
    break;
  case '\'':
    if (features_.allowSingleQuotes_) {
    token.type_ = tokenString;
    ok = readStringSingleQuote();
    break;
    } // else continue
  case '/':
    token.type_ = tokenComment;
    ok = readComment();
    break;
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
    token.type_ = tokenNumber;
    readNumber(false);
    break;
  case '-':
    if (readNumber(true)) {
      token.type_ = tokenNumber;
    } else {
      token.type_ = tokenNegInf;
      ok = features_.allowSpecialFloats_ && match("nfinity", 7);
    }
    break;
  case 't':
    token.type_ = tokenTrue;
    ok = match("rue", 3);
    break;
  case 'f':
    token.type_ = tokenFalse;
    ok = match("alse", 4);
    break;
  case 'n':
    token.type_ = tokenNull;
    ok = match("ull", 3);
    break;
  case 'N':
    if (features_.allowSpecialFloats_) {
      token.type_ = tokenNaN;
      ok = match("aN", 2);
    } else {
      ok = false;
    }
    break;
  case 'I':
    if (features_.allowSpecialFloats_) {
      token.type_ = tokenPosInf;
      ok = match("nfinity", 7);
    } else {
      ok = false;
    }
    break;
  case ',':
    token.type_ = tokenArraySeparator;
    break;
  case ':':
    token.type_ = tokenMemberSeparator;
    break;
  case 0:
    token.type_ = tokenEndOfStream;
    break;
  default:
    ok = false;
    break;
  }
  if (!ok)
    token.type_ = tokenError;
  token.end_ = current_;
  return true;
}

void OurReader::skipSpaces() {
  while (current_ != end_) {
    Char c = *current_;
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
      ++current_;
    else
      break;
  }
}

bool OurReader::match(Location pattern, int patternLength) {
  if (end_ - current_ < patternLength)
    return false;
  int index = patternLength;
  while (index--)
    if (current_[index] != pattern[index])
      return false;
  current_ += patternLength;
  return true;
}

bool OurReader::readComment() {
  Location commentBegin = current_ - 1;
  Char c = getNextChar();
  bool successful = false;
  if (c == '*')
    successful = readCStyleComment();
  else if (c == '/')
    successful = readCppStyleComment();
  if (!successful)
    return false;

  if (collectComments_) {
    CommentPlacement placement = commentBefore;
    if (lastValueEnd_ && !containsNewLine(lastValueEnd_, commentBegin)) {
      if (c != '*' || !containsNewLine(commentBegin, current_))
        placement = commentAfterOnSameLine;
    }

    addComment(commentBegin, current_, placement);
  }
  return true;
}

void
OurReader::addComment(Location begin, Location end, CommentPlacement placement) {
  assert(collectComments_);
  const std::string& normalized = normalizeEOL(begin, end);
  if (placement == commentAfterOnSameLine) {
    assert(lastValue_ != 0);
    lastValue_->setComment(normalized, placement);
  } else {
    commentsBefore_ += normalized;
  }
}

bool OurReader::readCStyleComment() {
  while (current_ != end_) {
    Char c = getNextChar();
    if (c == '*' && *current_ == '/')
      break;
  }
  return getNextChar() == '/';
}

bool OurReader::readCppStyleComment() {
  while (current_ != end_) {
    Char c = getNextChar();
    if (c == '\n')
      break;
    if (c == '\r') {
      // Consume DOS EOL. It will be normalized in addComment.
      if (current_ != end_ && *current_ == '\n')
        getNextChar();
      // Break on Moc OS 9 EOL.
      break;
    }
  }
  return true;
}

bool OurReader::readNumber(bool checkInf) {
  const char *p = current_;
  if (checkInf && p != end_ && *p == 'I') {
    current_ = ++p;
    return false;
  }
  char c = '0'; // stopgap for already consumed character
  // integral part
  while (c >= '0' && c <= '9')
    c = (current_ = p) < end_ ? *p++ : 0;
  // fractional part
  if (c == '.') {
    c = (current_ = p) < end_ ? *p++ : 0;
    while (c >= '0' && c <= '9')
      c = (current_ = p) < end_ ? *p++ : 0;
  }
  // exponential part
  if (c == 'e' || c == 'E') {
    c = (current_ = p) < end_ ? *p++ : 0;
    if (c == '+' || c == '-')
      c = (current_ = p) < end_ ? *p++ : 0;
    while (c >= '0' && c <= '9')
      c = (current_ = p) < end_ ? *p++ : 0;
  }
  return true;
}
bool OurReader::readString() {
  Char c = 0;
  while (current_ != end_) {
    c = getNextChar();
    if (c == '\\')
      getNextChar();
    else if (c == '"')
      break;
  }
  return c == '"';
}


bool OurReader::readStringSingleQuote() {
  Char c = 0;
  while (current_ != end_) {
    c = getNextChar();
    if (c == '\\')
      getNextChar();
    else if (c == '\'')
      break;
  }
  return c == '\'';
}

bool OurReader::readObject(Token& tokenStart) {
  Token tokenName;
  std::string name;
  Value init(objectValue);
  currentValue().swapPayload(init);
  currentValue().setOffsetStart(tokenStart.start_ - begin_);
  while (readToken(tokenName)) {
    bool initialTokenOk = true;
    while (tokenName.type_ == tokenComment && initialTokenOk)
      initialTokenOk = readToken(tokenName);
    if (!initialTokenOk)
      break;
    if (tokenName.type_ == tokenObjectEnd && name.empty()) // empty object
      return true;
    name = "";
    if (tokenName.type_ == tokenString) {
      if (!decodeString(tokenName, name))
        return recoverFromError(tokenObjectEnd);
    } else if (tokenName.type_ == tokenNumber && features_.allowNumericKeys_) {
      Value numberName;
      if (!decodeNumber(tokenName, numberName))
        return recoverFromError(tokenObjectEnd);
      name = numberName.asString();
    } else {
      break;
    }

    Token colon;
    if (!readToken(colon) || colon.type_ != tokenMemberSeparator) {
      return addErrorAndRecover(
          "Missing ':' after object member name", colon, tokenObjectEnd);
    }
    if (name.length() >= (1U<<30)) throwRuntimeError("keylength >= 2^30");
    if (features_.rejectDupKeys_ && currentValue().isMember(name)) {
      std::string msg = "Duplicate key: '" + name + "'";
      return addErrorAndRecover(
          msg, tokenName, tokenObjectEnd);
    }
    Value& value = currentValue()[name];
    nodes_.push(&value);
    bool ok = readValue();
    nodes_.pop();
    if (!ok) // error already set
      return recoverFromError(tokenObjectEnd);

    Token comma;
    if (!readToken(comma) ||
        (comma.type_ != tokenObjectEnd && comma.type_ != tokenArraySeparator &&
         comma.type_ != tokenComment)) {
      return addErrorAndRecover(
          "Missing ',' or '}' in object declaration", comma, tokenObjectEnd);
    }
    bool finalizeTokenOk = true;
    while (comma.type_ == tokenComment && finalizeTokenOk)
      finalizeTokenOk = readToken(comma);
    if (comma.type_ == tokenObjectEnd)
      return true;
  }
  return addErrorAndRecover(
      "Missing '}' or object member name", tokenName, tokenObjectEnd);
}

bool OurReader::readArray(Token& tokenStart) {
  Value init(arrayValue);
  currentValue().swapPayload(init);
  currentValue().setOffsetStart(tokenStart.start_ - begin_);
  skipSpaces();
  if (*current_ == ']') // empty array
  {
    Token endArray;
    readToken(endArray);
    return true;
  }
  int index = 0;
  for (;;) {
    Value& value = currentValue()[index++];
    nodes_.push(&value);
    bool ok = readValue();
    nodes_.pop();
    if (!ok) // error already set
      return recoverFromError(tokenArrayEnd);

    Token token;
    // Accept Comment after last item in the array.
    ok = readToken(token);
    while (token.type_ == tokenComment && ok) {
      ok = readToken(token);
    }
    bool badTokenType =
        (token.type_ != tokenArraySeparator && token.type_ != tokenArrayEnd);
    if (!ok || badTokenType) {
      return addErrorAndRecover(
          "Missing ',' or ']' in array declaration", token, tokenArrayEnd);
    }
    if (token.type_ == tokenArrayEnd)
      break;
  }
  return true;
}

bool OurReader::decodeNumber(Token& token) {
  Value decoded;
  if (!decodeNumber(token, decoded))
    return false;
  currentValue().swapPayload(decoded);
  currentValue().setOffsetStart(token.start_ - begin_);
  currentValue().setOffsetLimit(token.end_ - begin_);
  return true;
}

bool OurReader::decodeNumber(Token& token, Value& decoded) {
  // Attempts to parse the number as an integer. If the number is
  // larger than the maximum supported value of an integer then
  // we decode the number as a double.
  Location current = token.start_;
  bool isNegative = *current == '-';
  if (isNegative)
    ++current;
  // TODO: Help the compiler do the div and mod at compile time or get rid of them.
  Value::LargestUInt maxIntegerValue =
      isNegative ? Value::LargestUInt(-Value::minLargestInt)
                 : Value::maxLargestUInt;
  Value::LargestUInt threshold = maxIntegerValue / 10;
  Value::LargestUInt value = 0;
  while (current < token.end_) {
    Char c = *current++;
    if (c < '0' || c > '9')
      return decodeDouble(token, decoded);
    Value::UInt digit(c - '0');
    if (value >= threshold) {
      // We've hit or exceeded the max value divided by 10 (rounded down). If
      // a) we've only just touched the limit, b) this is the last digit, and
      // c) it's small enough to fit in that rounding delta, we're okay.
      // Otherwise treat this number as a double to avoid overflow.
      if (value > threshold || current != token.end_ ||
          digit > maxIntegerValue % 10) {
        return decodeDouble(token, decoded);
      }
    }
    value = value * 10 + digit;
  }
  if (isNegative)
    decoded = -Value::LargestInt(value);
  else if (value <= Value::LargestUInt(Value::maxInt))
    decoded = Value::LargestInt(value);
  else
    decoded = value;
  return true;
}

bool OurReader::decodeDouble(Token& token) {
  Value decoded;
  if (!decodeDouble(token, decoded))
    return false;
  currentValue().swapPayload(decoded);
  currentValue().setOffsetStart(token.start_ - begin_);
  currentValue().setOffsetLimit(token.end_ - begin_);
  return true;
}

bool OurReader::decodeDouble(Token& token, Value& decoded) {
  double value = 0;
  const int bufferSize = 32;
  int count;
  int length = int(token.end_ - token.start_);

  // Sanity check to avoid buffer overflow exploits.
  if (length < 0) {
    return addError("Unable to parse token length", token);
  }

  // Avoid using a string constant for the format control string given to
  // sscanf, as this can cause hard to debug crashes on OS X. See here for more
  // info:
  //
  //     http://developer.apple.com/library/mac/#DOCUMENTATION/DeveloperTools/gcc-4.0.1/gcc/Incompatibilities.html
  char format[] = "%lf";

  if (length <= bufferSize) {
    Char buffer[bufferSize + 1];
    memcpy(buffer, token.start_, length);
    buffer[length] = 0;
    count = sscanf(buffer, format, &value);
  } else {
    std::string buffer(token.start_, token.end_);
    count = sscanf(buffer.c_str(), format, &value);
  }

  if (count != 1)
    return addError("'" + std::string(token.start_, token.end_) +
                        "' is not a number.",
                    token);
  decoded = value;
  return true;
}

bool OurReader::decodeString(Token& token) {
  std::string decoded_string;
  if (!decodeString(token, decoded_string))
    return false;
  Value decoded(decoded_string);
  currentValue().swapPayload(decoded);
  currentValue().setOffsetStart(token.start_ - begin_);
  currentValue().setOffsetLimit(token.end_ - begin_);
  return true;
}

bool OurReader::decodeString(Token& token, std::string& decoded) {
  decoded.reserve(token.end_ - token.start_ - 2);
  Location current = token.start_ + 1; // skip '"'
  Location end = token.end_ - 1;       // do not include '"'
  while (current != end) {
    Char c = *current++;
    if (c == '"')
      break;
    else if (c == '\\') {
      if (current == end)
        return addError("Empty escape sequence in string", token, current);
      Char escape = *current++;
      switch (escape) {
      case '"':
        decoded += '"';
        break;
      case '/':
        decoded += '/';
        break;
      case '\\':
        decoded += '\\';
        break;
      case 'b':
        decoded += '\b';
        break;
      case 'f':
        decoded += '\f';
        break;
      case 'n':
        decoded += '\n';
        break;
      case 'r':
        decoded += '\r';
        break;
      case 't':
        decoded += '\t';
        break;
      case 'u': {
        unsigned int unicode;
        if (!decodeUnicodeCodePoint(token, current, end, unicode))
          return false;
        decoded += codePointToUTF8(unicode);
      } break;
      default:
        return addError("Bad escape sequence in string", token, current);
      }
    } else {
      decoded += c;
    }
  }
  return true;
}

bool OurReader::decodeUnicodeCodePoint(Token& token,
                                    Location& current,
                                    Location end,
                                    unsigned int& unicode) {

  if (!decodeUnicodeEscapeSequence(token, current, end, unicode))
    return false;
  if (unicode >= 0xD800 && unicode <= 0xDBFF) {
    // surrogate pairs
    if (end - current < 6)
      return addError(
          "additional six characters expected to parse unicode surrogate pair.",
          token,
          current);
    unsigned int surrogatePair;
    if (*(current++) == '\\' && *(current++) == 'u') {
      if (decodeUnicodeEscapeSequence(token, current, end, surrogatePair)) {
        unicode = 0x10000 + ((unicode & 0x3FF) << 10) + (surrogatePair & 0x3FF);
      } else
        return false;
    } else
      return addError("expecting another \\u token to begin the second half of "
                      "a unicode surrogate pair",
                      token,
                      current);
  }
  return true;
}

bool OurReader::decodeUnicodeEscapeSequence(Token& token,
                                         Location& current,
                                         Location end,
                                         unsigned int& unicode) {
  if (end - current < 4)
    return addError(
        "Bad unicode escape sequence in string: four digits expected.",
        token,
        current);
  unicode = 0;
  for (int index = 0; index < 4; ++index) {
    Char c = *current++;
    unicode *= 16;
    if (c >= '0' && c <= '9')
      unicode += c - '0';
    else if (c >= 'a' && c <= 'f')
      unicode += c - 'a' + 10;
    else if (c >= 'A' && c <= 'F')
      unicode += c - 'A' + 10;
    else
      return addError(
          "Bad unicode escape sequence in string: hexadecimal digit expected.",
          token,
          current);
  }
  return true;
}

bool
OurReader::addError(const std::string& message, Token& token, Location extra) {
  ErrorInfo info;
  info.token_ = token;
  info.message_ = message;
  info.extra_ = extra;
  errors_.push_back(info);
  return false;
}

bool OurReader::recoverFromError(TokenType skipUntilToken) {
  int errorCount = int(errors_.size());
  Token skip;
  for (;;) {
    if (!readToken(skip))
      errors_.resize(errorCount); // discard errors caused by recovery
    if (skip.type_ == skipUntilToken || skip.type_ == tokenEndOfStream)
      break;
  }
  errors_.resize(errorCount);
  return false;
}

bool OurReader::addErrorAndRecover(const std::string& message,
                                Token& token,
                                TokenType skipUntilToken) {
  addError(message, token);
  return recoverFromError(skipUntilToken);
}

Value& OurReader::currentValue() { return *(nodes_.top()); }

OurReader::Char OurReader::getNextChar() {
  if (current_ == end_)
    return 0;
  return *current_++;
}

void OurReader::getLocationLineAndColumn(Location location,
                                      int& line,
                                      int& column) const {
  Location current = begin_;
  Location lastLineStart = current;
  line = 0;
  while (current < location && current != end_) {
    Char c = *current++;
    if (c == '\r') {
      if (*current == '\n')
        ++current;
      lastLineStart = current;
      ++line;
    } else if (c == '\n') {
      lastLineStart = current;
      ++line;
    }
  }
  // column & line start at 1
  column = int(location - lastLineStart) + 1;
  ++line;
}

std::string OurReader::getLocationLineAndColumn(Location location) const {
  int line, column;
  getLocationLineAndColumn(location, line, column);
  char buffer[18 + 16 + 16 + 1];
  snprintf(buffer, sizeof(buffer), "Line %d, Column %d", line, column);
  return buffer;
}

std::string OurReader::getFormattedErrorMessages() const {
  std::string formattedMessage;
  for (Errors::const_iterator itError = errors_.begin();
       itError != errors_.end();
       ++itError) {
    const ErrorInfo& error = *itError;
    formattedMessage +=
        "* " + getLocationLineAndColumn(error.token_.start_) + "\n";
    formattedMessage += "  " + error.message_ + "\n";
    if (error.extra_)
      formattedMessage +=
          "See " + getLocationLineAndColumn(error.extra_) + " for detail.\n";
  }
  return formattedMessage;
}

std::vector<OurReader::StructuredError> OurReader::getStructuredErrors() const {
  std::vector<OurReader::StructuredError> allErrors;
  for (Errors::const_iterator itError = errors_.begin();
       itError != errors_.end();
       ++itError) {
    const ErrorInfo& error = *itError;
    OurReader::StructuredError structured;
    structured.offset_start = error.token_.start_ - begin_;
    structured.offset_limit = error.token_.end_ - begin_;
    structured.message = error.message_;
    allErrors.push_back(structured);
  }
  return allErrors;
}

bool OurReader::pushError(const Value& value, const std::string& message) {
  size_t length = end_ - begin_;
  if(value.getOffsetStart() > length
    || value.getOffsetLimit() > length)
    return false;
  Token token;
  token.type_ = tokenError;
  token.start_ = begin_ + value.getOffsetStart();
  token.end_ = end_ + value.getOffsetLimit();
  ErrorInfo info;
  info.token_ = token;
  info.message_ = message;
  info.extra_ = 0;
  errors_.push_back(info);
  return true;
}

bool OurReader::pushError(const Value& value, const std::string& message, const Value& extra) {
  size_t length = end_ - begin_;
  if(value.getOffsetStart() > length
    || value.getOffsetLimit() > length
    || extra.getOffsetLimit() > length)
    return false;
  Token token;
  token.type_ = tokenError;
  token.start_ = begin_ + value.getOffsetStart();
  token.end_ = begin_ + value.getOffsetLimit();
  ErrorInfo info;
  info.token_ = token;
  info.message_ = message;
  info.extra_ = begin_ + extra.getOffsetStart();
  errors_.push_back(info);
  return true;
}

bool OurReader::good() const {
  return !errors_.size();
}


class OurCharReader : public CharReader {
  bool const collectComments_;
  OurReader reader_;
public:
  OurCharReader(
    bool collectComments,
    OurFeatures const& features)
  : collectComments_(collectComments)
  , reader_(features)
  {}
  bool parse(
      char const* beginDoc, char const* endDoc,
      Value* root, std::string* errs) override {
    bool ok = reader_.parse(beginDoc, endDoc, *root, collectComments_);
    if (errs) {
      *errs = reader_.getFormattedErrorMessages();
    }
    return ok;
  }
};

CharReaderBuilder::CharReaderBuilder()
{
  setDefaults(&settings_);
}
CharReaderBuilder::~CharReaderBuilder()
{}
CharReader* CharReaderBuilder::newCharReader() const
{
  bool collectComments = settings_["collectComments"].asBool();
  OurFeatures features = OurFeatures::all();
  features.allowComments_ = settings_["allowComments"].asBool();
  features.strictRoot_ = settings_["strictRoot"].asBool();
  features.allowDroppedNullPlaceholders_ = settings_["allowDroppedNullPlaceholders"].asBool();
  features.allowNumericKeys_ = settings_["allowNumericKeys"].asBool();
  features.allowSingleQuotes_ = settings_["allowSingleQuotes"].asBool();
  features.stackLimit_ = settings_["stackLimit"].asInt();
  features.failIfExtra_ = settings_["failIfExtra"].asBool();
  features.rejectDupKeys_ = settings_["rejectDupKeys"].asBool();
  features.allowSpecialFloats_ = settings_["allowSpecialFloats"].asBool();
  return new OurCharReader(collectComments, features);
}
static void getValidReaderKeys(std::set<std::string>* valid_keys)
{
  valid_keys->clear();
  valid_keys->insert("collectComments");
  valid_keys->insert("allowComments");
  valid_keys->insert("strictRoot");
  valid_keys->insert("allowDroppedNullPlaceholders");
  valid_keys->insert("allowNumericKeys");
  valid_keys->insert("allowSingleQuotes");
  valid_keys->insert("stackLimit");
  valid_keys->insert("failIfExtra");
  valid_keys->insert("rejectDupKeys");
  valid_keys->insert("allowSpecialFloats");
}
bool CharReaderBuilder::validate(Json::Value* invalid) const
{
  Json::Value my_invalid;
  if (!invalid) invalid = &my_invalid;  // so we do not need to test for NULL
  Json::Value& inv = *invalid;
  std::set<std::string> valid_keys;
  getValidReaderKeys(&valid_keys);
  Value::Members keys = settings_.getMemberNames();
  size_t n = keys.size();
  for (size_t i = 0; i < n; ++i) {
    std::string const& key = keys[i];
    if (valid_keys.find(key) == valid_keys.end()) {
      inv[key] = settings_[key];
    }
  }
  return 0u == inv.size();
}
Value& CharReaderBuilder::operator[](std::string key)
{
  return settings_[key];
}
// static
void CharReaderBuilder::strictMode(Json::Value* settings)
{
//! [CharReaderBuilderStrictMode]
  (*settings)["allowComments"] = false;
  (*settings)["strictRoot"] = true;
  (*settings)["allowDroppedNullPlaceholders"] = false;
  (*settings)["allowNumericKeys"] = false;
  (*settings)["allowSingleQuotes"] = false;
  (*settings)["stackLimit"] = 1000;
  (*settings)["failIfExtra"] = true;
  (*settings)["rejectDupKeys"] = true;
  (*settings)["allowSpecialFloats"] = false;
//! [CharReaderBuilderStrictMode]
}
// static
void CharReaderBuilder::setDefaults(Json::Value* settings)
{
//! [CharReaderBuilderDefaults]
  (*settings)["collectComments"] = true;
  (*settings)["allowComments"] = true;
  (*settings)["strictRoot"] = false;
  (*settings)["allowDroppedNullPlaceholders"] = false;
  (*settings)["allowNumericKeys"] = false;
  (*settings)["allowSingleQuotes"] = false;
  (*settings)["stackLimit"] = 1000;
  (*settings)["failIfExtra"] = false;
  (*settings)["rejectDupKeys"] = false;
  (*settings)["allowSpecialFloats"] = false;
//! [CharReaderBuilderDefaults]
}

//////////////////////////////////
// global functions

bool parseFromStream(
    CharReader::Factory const& fact, std::istream& sin,
    Value* root, std::string* errs)
{
  std::ostringstream ssin;
  ssin << sin.rdbuf();
  std::string doc = ssin.str();
  char const* begin = doc.data();
  char const* end = begin + doc.size();
  // Note that we do not actually need a null-terminator.
  CharReaderPtr const reader(fact.newCharReader());
  return reader->parse(begin, end, root, errs);
}

std::istream& operator>>(std::istream& sin, Value& root) {
  CharReaderBuilder b;
  std::string errs;
  bool ok = parseFromStream(b, sin, &root, &errs);
  if (!ok) {
    fprintf(stderr,
            "Error from reader: %s",
            errs.c_str());

    throwRuntimeError(errs);
  }
  return sin;
}

} // namespace Json

// //////////////////////////////////////////////////////////////////////
// End of content of file: src/lib_json/json_reader.cpp
// //////////////////////////////////////////////////////////////////////






// //////////////////////////////////////////////////////////////////////
// Beginning of content of file: src/lib_json/json_valueiterator.inl
// //////////////////////////////////////////////////////////////////////

// Copyright 2007-2010 Baptiste Lepilleur
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.
// See file LICENSE for detail or copy at http://jsoncpp.sourceforge.net/LICENSE

// included by json_value.cpp

namespace Json {

// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// class ValueIteratorBase
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////

ValueIteratorBase::ValueIteratorBase()
    : current_(), isNull_(true) {
}

ValueIteratorBase::ValueIteratorBase(
    const Value::ObjectValues::iterator& current)
    : current_(current), isNull_(false) {}

Value& ValueIteratorBase::deref() const {
  return current_->second;
}

void ValueIteratorBase::increment() {
  ++current_;
}

void ValueIteratorBase::decrement() {
  --current_;
}

ValueIteratorBase::difference_type
ValueIteratorBase::computeDistance(const SelfType& other) const {
#ifdef JSON_USE_CPPTL_SMALLMAP
  return other.current_ - current_;
#else
  // Iterator for null value are initialized using the default
  // constructor, which initialize current_ to the default
  // std::map::iterator. As begin() and end() are two instance
  // of the default std::map::iterator, they can not be compared.
  // To allow this, we handle this comparison specifically.
  if (isNull_ && other.isNull_) {
    return 0;
  }

  // Usage of std::distance is not portable (does not compile with Sun Studio 12
  // RogueWave STL,
  // which is the one used by default).
  // Using a portable hand-made version for non random iterator instead:
  //   return difference_type( std::distance( current_, other.current_ ) );
  difference_type myDistance = 0;
  for (Value::ObjectValues::iterator it = current_; it != other.current_;
       ++it) {
    ++myDistance;
  }
  return myDistance;
#endif
}

bool ValueIteratorBase::isEqual(const SelfType& other) const {
  if (isNull_) {
    return other.isNull_;
  }
  return current_ == other.current_;
}

void ValueIteratorBase::copy(const SelfType& other) {
  current_ = other.current_;
  isNull_ = other.isNull_;
}

Value ValueIteratorBase::key() const {
  const Value::CZString czstring = (*current_).first;
  if (czstring.data()) {
    if (czstring.isStaticString())
      return Value(StaticString(czstring.data()));
    return Value(czstring.data(), czstring.data() + czstring.length());
  }
  return Value(czstring.index());
}

UInt ValueIteratorBase::index() const {
  const Value::CZString czstring = (*current_).first;
  if (!czstring.data())
    return czstring.index();
  return Value::UInt(-1);
}

std::string ValueIteratorBase::name() const {
  char const* keey;
  char const* end;
  keey = memberName(&end);
  if (!keey) return std::string();
  return std::string(keey, end);
}

char const* ValueIteratorBase::memberName() const {
  const char* cname = (*current_).first.data();
  return cname ? cname : "";
}

char const* ValueIteratorBase::memberName(char const** end) const {
  const char* cname = (*current_).first.data();
  if (!cname) {
    *end = NULL;
    return NULL;
  }
  *end = cname + (*current_).first.length();
  return cname;
}

// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// class ValueConstIterator
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////

ValueConstIterator::ValueConstIterator() {}

ValueConstIterator::ValueConstIterator(
    const Value::ObjectValues::iterator& current)
    : ValueIteratorBase(current) {}

ValueConstIterator::ValueConstIterator(ValueIterator const& other)
    : ValueIteratorBase(other) {}

ValueConstIterator& ValueConstIterator::
operator=(const ValueIteratorBase& other) {
  copy(other);
  return *this;
}

// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// class ValueIterator
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////

ValueIterator::ValueIterator() {}

ValueIterator::ValueIterator(const Value::ObjectValues::iterator& current)
    : ValueIteratorBase(current) {}

ValueIterator::ValueIterator(const ValueConstIterator& other)
    : ValueIteratorBase(other) {
  throwRuntimeError("ConstIterator to Iterator should never be allowed.");
}

ValueIterator::ValueIterator(const ValueIterator& other)
    : ValueIteratorBase(other) {}

ValueIterator& ValueIterator::operator=(const SelfType& other) {
  copy(other);
  return *this;
}

} // namespace Json

// //////////////////////////////////////////////////////////////////////
// End of content of file: src/lib_json/json_valueiterator.inl
// //////////////////////////////////////////////////////////////////////






// //////////////////////////////////////////////////////////////////////
// Beginning of content of file: src/lib_json/json_value.cpp
// //////////////////////////////////////////////////////////////////////

// Copyright 2011 Baptiste Lepilleur
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.
// See file LICENSE for detail or copy at http://jsoncpp.sourceforge.net/LICENSE

#if !defined(JSON_IS_AMALGAMATION)
#include <json/assertions.h>
#include <json/value.h>
#include <json/writer.h>
#endif // if !defined(JSON_IS_AMALGAMATION)
#include <math.h>
#include <sstream>
#include <utility>
#include <cstring>
#include <cassert>
#ifdef JSON_USE_CPPTL
#include <cpptl/conststring.h>
#endif
#include <cstddef> // size_t
#include <algorithm> // min()

#define JSON_ASSERT_UNREACHABLE assert(false)

namespace Json {

// This is a walkaround to avoid the static initialization of Value::null.
// kNull must be word-aligned to avoid crashing on ARM.  We use an alignment of
// 8 (instead of 4) as a bit of future-proofing.
#if defined(__ARMEL__)
#define ALIGNAS(byte_alignment) __attribute__((aligned(byte_alignment)))
#else
#define ALIGNAS(byte_alignment)
#endif
static const unsigned char ALIGNAS(8) kNull[sizeof(Value)] = { 0 };
const unsigned char& kNullRef = kNull[0];
const Value& Value::null = reinterpret_cast<const Value&>(kNullRef);
const Value& Value::nullRef = null;

const Int Value::minInt = Int(~(UInt(-1) / 2));
const Int Value::maxInt = Int(UInt(-1) / 2);
const UInt Value::maxUInt = UInt(-1);
#if defined(JSON_HAS_INT64)
const Int64 Value::minInt64 = Int64(~(UInt64(-1) / 2));
const Int64 Value::maxInt64 = Int64(UInt64(-1) / 2);
const UInt64 Value::maxUInt64 = UInt64(-1);
// The constant is hard-coded because some compiler have trouble
// converting Value::maxUInt64 to a double correctly (AIX/xlC).
// Assumes that UInt64 is a 64 bits integer.
static const double maxUInt64AsDouble = 18446744073709551615.0;
#endif // defined(JSON_HAS_INT64)
const LargestInt Value::minLargestInt = LargestInt(~(LargestUInt(-1) / 2));
const LargestInt Value::maxLargestInt = LargestInt(LargestUInt(-1) / 2);
const LargestUInt Value::maxLargestUInt = LargestUInt(-1);

#if !defined(JSON_USE_INT64_DOUBLE_CONVERSION)
template <typename T, typename U>
static inline bool InRange(double d, T min, U max) {
  return d >= min && d <= max;
}
#else  // if !defined(JSON_USE_INT64_DOUBLE_CONVERSION)
static inline double integerToDouble(Json::UInt64 value) {
  return static_cast<double>(Int64(value / 2)) * 2.0 + Int64(value & 1);
}

template <typename T> static inline double integerToDouble(T value) {
  return static_cast<double>(value);
}

template <typename T, typename U>
static inline bool InRange(double d, T min, U max) {
  return d >= integerToDouble(min) && d <= integerToDouble(max);
}
#endif // if !defined(JSON_USE_INT64_DOUBLE_CONVERSION)

/** Duplicates the specified string value.
 * @param value Pointer to the string to duplicate. Must be zero-terminated if
 *              length is "unknown".
 * @param length Length of the value. if equals to unknown, then it will be
 *               computed using strlen(value).
 * @return Pointer on the duplicate instance of string.
 */
static inline char* duplicateStringValue(const char* value,
                                         size_t length) {
  // Avoid an integer overflow in the call to malloc below by limiting length
  // to a sane value.
  if (length >= (size_t)Value::maxInt)
    length = Value::maxInt - 1;

  char* newString = static_cast<char*>(malloc(length + 1));
  if (newString == NULL) {
    throwRuntimeError(
        "in Json::Value::duplicateStringValue(): "
        "Failed to allocate string value buffer");
  }
  memcpy(newString, value, length);
  newString[length] = 0;
  return newString;
}

/* Record the length as a prefix.
 */
static inline char* duplicateAndPrefixStringValue(
    const char* value,
    unsigned int length)
{
  // Avoid an integer overflow in the call to malloc below by limiting length
  // to a sane value.
  JSON_ASSERT_MESSAGE(length <= (unsigned)Value::maxInt - sizeof(unsigned) - 1U,
                      "in Json::Value::duplicateAndPrefixStringValue(): "
                      "length too big for prefixing");
  unsigned actualLength = length + static_cast<unsigned>(sizeof(unsigned)) + 1U;
  char* newString = static_cast<char*>(malloc(actualLength));
  if (newString == 0) {
    throwRuntimeError(
        "in Json::Value::duplicateAndPrefixStringValue(): "
        "Failed to allocate string value buffer");
  }
  *reinterpret_cast<unsigned*>(newString) = length;
  memcpy(newString + sizeof(unsigned), value, length);
  newString[actualLength - 1U] = 0; // to avoid buffer over-run accidents by users later
  return newString;
}
inline static void decodePrefixedString(
    bool isPrefixed, char const* prefixed,
    unsigned* length, char const** value)
{
  if (!isPrefixed) {
    *length = static_cast<unsigned>(strlen(prefixed));
    *value = prefixed;
  } else {
    *length = *reinterpret_cast<unsigned const*>(prefixed);
    *value = prefixed + sizeof(unsigned);
  }
}
/** Free the string duplicated by duplicateStringValue()/duplicateAndPrefixStringValue().
 */
static inline void releaseStringValue(char* value) { free(value); }

} // namespace Json

// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// ValueInternals...
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
#if !defined(JSON_IS_AMALGAMATION)

#include "json_valueiterator.inl"
#endif // if !defined(JSON_IS_AMALGAMATION)

namespace Json {

Exception::Exception(std::string const& msg)
  : msg_(msg)
{}
Exception::~Exception() throw()
{}
char const* Exception::what() const throw()
{
  return msg_.c_str();
}
RuntimeError::RuntimeError(std::string const& msg)
  : Exception(msg)
{}
LogicError::LogicError(std::string const& msg)
  : Exception(msg)
{}
void throwRuntimeError(std::string const& msg)
{
  throw RuntimeError(msg);
}
void throwLogicError(std::string const& msg)
{
  throw LogicError(msg);
}

// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// class Value::CommentInfo
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////

Value::CommentInfo::CommentInfo() : comment_(0) {}

Value::CommentInfo::~CommentInfo() {
  if (comment_)
    releaseStringValue(comment_);
}

void Value::CommentInfo::setComment(const char* text, size_t len) {
  if (comment_) {
    releaseStringValue(comment_);
    comment_ = 0;
  }
  JSON_ASSERT(text != 0);
  JSON_ASSERT_MESSAGE(
      text[0] == '\0' || text[0] == '/',
      "in Json::Value::setComment(): Comments must start with /");
  // It seems that /**/ style comments are acceptable as well.
  comment_ = duplicateStringValue(text, len);
}

// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// class Value::CZString
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////

// Notes: policy_ indicates if the string was allocated when
// a string is stored.

Value::CZString::CZString(ArrayIndex aindex) : cstr_(0), index_(aindex) {}

Value::CZString::CZString(char const* str, unsigned ulength, DuplicationPolicy allocate)
    : cstr_(str) {
  // allocate != duplicate
  storage_.policy_ = allocate & 0x3;
  storage_.length_ = ulength & 0x3FFFFFFF;
}

Value::CZString::CZString(const CZString& other)
    : cstr_(other.storage_.policy_ != noDuplication && other.cstr_ != 0
                ? duplicateStringValue(other.cstr_, other.storage_.length_)
                : other.cstr_) {
  storage_.policy_ = (other.cstr_
                 ? (static_cast<DuplicationPolicy>(other.storage_.policy_) == noDuplication
                     ? noDuplication : duplicate)
                 : static_cast<DuplicationPolicy>(other.storage_.policy_));
  storage_.length_ = other.storage_.length_;
}

#if JSON_HAS_RVALUE_REFERENCES
Value::CZString::CZString(CZString&& other)
  : cstr_(other.cstr_), index_(other.index_) {
  other.cstr_ = nullptr;
}
#endif

Value::CZString::~CZString() {
  if (cstr_ && storage_.policy_ == duplicate)
    releaseStringValue(const_cast<char*>(cstr_));
}

void Value::CZString::swap(CZString& other) {
  std::swap(cstr_, other.cstr_);
  std::swap(index_, other.index_);
}

Value::CZString& Value::CZString::operator=(CZString other) {
  swap(other);
  return *this;
}

bool Value::CZString::operator<(const CZString& other) const {
  if (!cstr_) return index_ < other.index_;
  //return strcmp(cstr_, other.cstr_) < 0;
  // Assume both are strings.
  unsigned this_len = this->storage_.length_;
  unsigned other_len = other.storage_.length_;
  unsigned min_len = std::min(this_len, other_len);
  int comp = memcmp(this->cstr_, other.cstr_, min_len);
  if (comp < 0) return true;
  if (comp > 0) return false;
  return (this_len < other_len);
}

bool Value::CZString::operator==(const CZString& other) const {
  if (!cstr_) return index_ == other.index_;
  //return strcmp(cstr_, other.cstr_) == 0;
  // Assume both are strings.
  unsigned this_len = this->storage_.length_;
  unsigned other_len = other.storage_.length_;
  if (this_len != other_len) return false;
  int comp = memcmp(this->cstr_, other.cstr_, this_len);
  return comp == 0;
}

ArrayIndex Value::CZString::index() const { return index_; }

//const char* Value::CZString::c_str() const { return cstr_; }
const char* Value::CZString::data() const { return cstr_; }
unsigned Value::CZString::length() const { return storage_.length_; }
bool Value::CZString::isStaticString() const { return storage_.policy_ == noDuplication; }

// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// class Value::Value
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////

/*! \internal Default constructor initialization must be equivalent to:
 * memset( this, 0, sizeof(Value) )
 * This optimization is used in ValueInternalMap fast allocator.
 */
Value::Value(ValueType vtype) {
  initBasic(vtype);
  switch (vtype) {
  case nullValue:
    break;
  case intValue:
  case uintValue:
    value_.int_ = 0;
    break;
  case realValue:
    value_.real_ = 0.0;
    break;
  case stringValue:
    value_.string_ = 0;
    break;
  case arrayValue:
  case objectValue:
    value_.map_ = new ObjectValues();
    break;
  case booleanValue:
    value_.bool_ = false;
    break;
  default:
    JSON_ASSERT_UNREACHABLE;
  }
}

Value::Value(Int value) {
  initBasic(intValue);
  value_.int_ = value;
}

Value::Value(UInt value) {
  initBasic(uintValue);
  value_.uint_ = value;
}
#if defined(JSON_HAS_INT64)
Value::Value(Int64 value) {
  initBasic(intValue);
  value_.int_ = value;
}
Value::Value(UInt64 value) {
  initBasic(uintValue);
  value_.uint_ = value;
}
#endif // defined(JSON_HAS_INT64)

Value::Value(double value) {
  initBasic(realValue);
  value_.real_ = value;
}

Value::Value(const char* value) {
  initBasic(stringValue, true);
  value_.string_ = duplicateAndPrefixStringValue(value, static_cast<unsigned>(strlen(value)));
}

Value::Value(const char* beginValue, const char* endValue) {
  initBasic(stringValue, true);
  value_.string_ =
      duplicateAndPrefixStringValue(beginValue, static_cast<unsigned>(endValue - beginValue));
}

Value::Value(const std::string& value) {
  initBasic(stringValue, true);
  value_.string_ =
      duplicateAndPrefixStringValue(value.data(), static_cast<unsigned>(value.length()));
}

Value::Value(const StaticString& value) {
  initBasic(stringValue);
  value_.string_ = const_cast<char*>(value.c_str());
}

#ifdef JSON_USE_CPPTL
Value::Value(const CppTL::ConstString& value) {
  initBasic(stringValue, true);
  value_.string_ = duplicateAndPrefixStringValue(value, static_cast<unsigned>(value.length()));
}
#endif

Value::Value(bool value) {
  initBasic(booleanValue);
  value_.bool_ = value;
}

Value::Value(Value const& other)
    : type_(other.type_), allocated_(false)
      ,
      comments_(0), start_(other.start_), limit_(other.limit_)
{
  switch (type_) {
  case nullValue:
  case intValue:
  case uintValue:
  case realValue:
  case booleanValue:
    value_ = other.value_;
    break;
  case stringValue:
    if (other.value_.string_ && other.allocated_) {
      unsigned len;
      char const* str;
      decodePrefixedString(other.allocated_, other.value_.string_,
          &len, &str);
      value_.string_ = duplicateAndPrefixStringValue(str, len);
      allocated_ = true;
    } else {
      value_.string_ = other.value_.string_;
      allocated_ = false;
    }
    break;
  case arrayValue:
  case objectValue:
    value_.map_ = new ObjectValues(*other.value_.map_);
    break;
  default:
    JSON_ASSERT_UNREACHABLE;
  }
  if (other.comments_) {
    comments_ = new CommentInfo[numberOfCommentPlacement];
    for (int comment = 0; comment < numberOfCommentPlacement; ++comment) {
      const CommentInfo& otherComment = other.comments_[comment];
      if (otherComment.comment_)
        comments_[comment].setComment(
            otherComment.comment_, strlen(otherComment.comment_));
    }
  }
}

#if JSON_HAS_RVALUE_REFERENCES
// Move constructor
Value::Value(Value&& other) {
  initBasic(nullValue);
  swap(other);
}
#endif

Value::~Value() {
  switch (type_) {
  case nullValue:
  case intValue:
  case uintValue:
  case realValue:
  case booleanValue:
    break;
  case stringValue:
    if (allocated_)
      releaseStringValue(value_.string_);
    break;
  case arrayValue:
  case objectValue:
    delete value_.map_;
    break;
  default:
    JSON_ASSERT_UNREACHABLE;
  }

  if (comments_)
    delete[] comments_;
}

Value& Value::operator=(Value other) {
  swap(other);
  return *this;
}

void Value::swapPayload(Value& other) {
  ValueType temp = type_;
  type_ = other.type_;
  other.type_ = temp;
  std::swap(value_, other.value_);
  int temp2 = allocated_;
  allocated_ = other.allocated_;
  other.allocated_ = temp2 & 0x1;
}

void Value::swap(Value& other) {
  swapPayload(other);
  std::swap(comments_, other.comments_);
  std::swap(start_, other.start_);
  std::swap(limit_, other.limit_);
}

ValueType Value::type() const { return type_; }

int Value::compare(const Value& other) const {
  if (*this < other)
    return -1;
  if (*this > other)
    return 1;
  return 0;
}

bool Value::operator<(const Value& other) const {
  int typeDelta = type_ - other.type_;
  if (typeDelta)
    return typeDelta < 0 ? true : false;
  switch (type_) {
  case nullValue:
    return false;
  case intValue:
    return value_.int_ < other.value_.int_;
  case uintValue:
    return value_.uint_ < other.value_.uint_;
  case realValue:
    return value_.real_ < other.value_.real_;
  case booleanValue:
    return value_.bool_ < other.value_.bool_;
  case stringValue:
  {
    if ((value_.string_ == 0) || (other.value_.string_ == 0)) {
      if (other.value_.string_) return true;
      else return false;
    }
    unsigned this_len;
    unsigned other_len;
    char const* this_str;
    char const* other_str;
    decodePrefixedString(this->allocated_, this->value_.string_, &this_len, &this_str);
    decodePrefixedString(other.allocated_, other.value_.string_, &other_len, &other_str);
    unsigned min_len = std::min(this_len, other_len);
    int comp = memcmp(this_str, other_str, min_len);
    if (comp < 0) return true;
    if (comp > 0) return false;
    return (this_len < other_len);
  }
  case arrayValue:
  case objectValue: {
    int delta = int(value_.map_->size() - other.value_.map_->size());
    if (delta)
      return delta < 0;
    return (*value_.map_) < (*other.value_.map_);
  }
  default:
    JSON_ASSERT_UNREACHABLE;
  }
  return false; // unreachable
}

bool Value::operator<=(const Value& other) const { return !(other < *this); }

bool Value::operator>=(const Value& other) const { return !(*this < other); }

bool Value::operator>(const Value& other) const { return other < *this; }

bool Value::operator==(const Value& other) const {
  // if ( type_ != other.type_ )
  // GCC 2.95.3 says:
  // attempt to take address of bit-field structure member `Json::Value::type_'
  // Beats me, but a temp solves the problem.
  int temp = other.type_;
  if (type_ != temp)
    return false;
  switch (type_) {
  case nullValue:
    return true;
  case intValue:
    return value_.int_ == other.value_.int_;
  case uintValue:
    return value_.uint_ == other.value_.uint_;
  case realValue:
    return value_.real_ == other.value_.real_;
  case booleanValue:
    return value_.bool_ == other.value_.bool_;
  case stringValue:
  {
    if ((value_.string_ == 0) || (other.value_.string_ == 0)) {
      return (value_.string_ == other.value_.string_);
    }
    unsigned this_len;
    unsigned other_len;
    char const* this_str;
    char const* other_str;
    decodePrefixedString(this->allocated_, this->value_.string_, &this_len, &this_str);
    decodePrefixedString(other.allocated_, other.value_.string_, &other_len, &other_str);
    if (this_len != other_len) return false;
    int comp = memcmp(this_str, other_str, this_len);
    return comp == 0;
  }
  case arrayValue:
  case objectValue:
    return value_.map_->size() == other.value_.map_->size() &&
           (*value_.map_) == (*other.value_.map_);
  default:
    JSON_ASSERT_UNREACHABLE;
  }
  return false; // unreachable
}

bool Value::operator!=(const Value& other) const { return !(*this == other); }

const char* Value::asCString() const {
  JSON_ASSERT_MESSAGE(type_ == stringValue,
                      "in Json::Value::asCString(): requires stringValue");
  if (value_.string_ == 0) return 0;
  unsigned this_len;
  char const* this_str;
  decodePrefixedString(this->allocated_, this->value_.string_, &this_len, &this_str);
  return this_str;
}

bool Value::getString(char const** str, char const** cend) const {
  if (type_ != stringValue) return false;
  if (value_.string_ == 0) return false;
  unsigned length;
  decodePrefixedString(this->allocated_, this->value_.string_, &length, str);
  *cend = *str + length;
  return true;
}

std::string Value::asString() const {
  switch (type_) {
  case nullValue:
    return "";
  case stringValue:
  {
    if (value_.string_ == 0) return "";
    unsigned this_len;
    char const* this_str;
    decodePrefixedString(this->allocated_, this->value_.string_, &this_len, &this_str);
    return std::string(this_str, this_len);
  }
  case booleanValue:
    return value_.bool_ ? "true" : "false";
  case intValue:
    return valueToString(value_.int_);
  case uintValue:
    return valueToString(value_.uint_);
  case realValue:
    return valueToString(value_.real_);
  default:
    JSON_FAIL_MESSAGE("Type is not convertible to string");
  }
}

#ifdef JSON_USE_CPPTL
CppTL::ConstString Value::asConstString() const {
  unsigned len;
  char const* str;
  decodePrefixedString(allocated_, value_.string_,
      &len, &str);
  return CppTL::ConstString(str, len);
}
#endif

Value::Int Value::asInt() const {
  switch (type_) {
  case intValue:
    JSON_ASSERT_MESSAGE(isInt(), "LargestInt out of Int range");
    return Int(value_.int_);
  case uintValue:
    JSON_ASSERT_MESSAGE(isInt(), "LargestUInt out of Int range");
    return Int(value_.uint_);
  case realValue:
    JSON_ASSERT_MESSAGE(InRange(value_.real_, minInt, maxInt),
                        "double out of Int range");
    return Int(value_.real_);
  case nullValue:
    return 0;
  case booleanValue:
    return value_.bool_ ? 1 : 0;
  default:
    break;
  }
  JSON_FAIL_MESSAGE("Value is not convertible to Int.");
}

Value::UInt Value::asUInt() const {
  switch (type_) {
  case intValue:
    JSON_ASSERT_MESSAGE(isUInt(), "LargestInt out of UInt range");
    return UInt(value_.int_);
  case uintValue:
    JSON_ASSERT_MESSAGE(isUInt(), "LargestUInt out of UInt range");
    return UInt(value_.uint_);
  case realValue:
    JSON_ASSERT_MESSAGE(InRange(value_.real_, 0, maxUInt),
                        "double out of UInt range");
    return UInt(value_.real_);
  case nullValue:
    return 0;
  case booleanValue:
    return value_.bool_ ? 1 : 0;
  default:
    break;
  }
  JSON_FAIL_MESSAGE("Value is not convertible to UInt.");
}

#if defined(JSON_HAS_INT64)

Value::Int64 Value::asInt64() const {
  switch (type_) {
  case intValue:
    return Int64(value_.int_);
  case uintValue:
    JSON_ASSERT_MESSAGE(isInt64(), "LargestUInt out of Int64 range");
    return Int64(value_.uint_);
  case realValue:
    JSON_ASSERT_MESSAGE(InRange(value_.real_, minInt64, maxInt64),
                        "double out of Int64 range");
    return Int64(value_.real_);
  case nullValue:
    return 0;
  case booleanValue:
    return value_.bool_ ? 1 : 0;
  default:
    break;
  }
  JSON_FAIL_MESSAGE("Value is not convertible to Int64.");
}

Value::UInt64 Value::asUInt64() const {
  switch (type_) {
  case intValue:
    JSON_ASSERT_MESSAGE(isUInt64(), "LargestInt out of UInt64 range");
    return UInt64(value_.int_);
  case uintValue:
    return UInt64(value_.uint_);
  case realValue:
    JSON_ASSERT_MESSAGE(InRange(value_.real_, 0, maxUInt64),
                        "double out of UInt64 range");
    return UInt64(value_.real_);
  case nullValue:
    return 0;
  case booleanValue:
    return value_.bool_ ? 1 : 0;
  default:
    break;
  }
  JSON_FAIL_MESSAGE("Value is not convertible to UInt64.");
}
#endif // if defined(JSON_HAS_INT64)

LargestInt Value::asLargestInt() const {
#if defined(JSON_NO_INT64)
  return asInt();
#else
  return asInt64();
#endif
}

LargestUInt Value::asLargestUInt() const {
#if defined(JSON_NO_INT64)
  return asUInt();
#else
  return asUInt64();
#endif
}

double Value::asDouble() const {
  switch (type_) {
  case intValue:
    return static_cast<double>(value_.int_);
  case uintValue:
#if !defined(JSON_USE_INT64_DOUBLE_CONVERSION)
    return static_cast<double>(value_.uint_);
#else  // if !defined(JSON_USE_INT64_DOUBLE_CONVERSION)
    return integerToDouble(value_.uint_);
#endif // if !defined(JSON_USE_INT64_DOUBLE_CONVERSION)
  case realValue:
    return value_.real_;
  case nullValue:
    return 0.0;
  case booleanValue:
    return value_.bool_ ? 1.0 : 0.0;
  default:
    break;
  }
  JSON_FAIL_MESSAGE("Value is not convertible to double.");
}

float Value::asFloat() const {
  switch (type_) {
  case intValue:
    return static_cast<float>(value_.int_);
  case uintValue:
#if !defined(JSON_USE_INT64_DOUBLE_CONVERSION)
    return static_cast<float>(value_.uint_);
#else  // if !defined(JSON_USE_INT64_DOUBLE_CONVERSION)
    return integerToDouble(value_.uint_);
#endif // if !defined(JSON_USE_INT64_DOUBLE_CONVERSION)
  case realValue:
    return static_cast<float>(value_.real_);
  case nullValue:
    return 0.0;
  case booleanValue:
    return value_.bool_ ? 1.0f : 0.0f;
  default:
    break;
  }
  JSON_FAIL_MESSAGE("Value is not convertible to float.");
}

bool Value::asBool() const {
  switch (type_) {
  case booleanValue:
    return value_.bool_;
  case nullValue:
    return false;
  case intValue:
    return value_.int_ ? true : false;
  case uintValue:
    return value_.uint_ ? true : false;
  case realValue:
    // This is kind of strange. Not recommended.
    return (value_.real_ != 0.0) ? true : false;
  default:
    break;
  }
  JSON_FAIL_MESSAGE("Value is not convertible to bool.");
}

bool Value::isConvertibleTo(ValueType other) const {
  switch (other) {
  case nullValue:
    return (isNumeric() && asDouble() == 0.0) ||
           (type_ == booleanValue && value_.bool_ == false) ||
           (type_ == stringValue && asString() == "") ||
           (type_ == arrayValue && value_.map_->size() == 0) ||
           (type_ == objectValue && value_.map_->size() == 0) ||
           type_ == nullValue;
  case intValue:
    return isInt() ||
           (type_ == realValue && InRange(value_.real_, minInt, maxInt)) ||
           type_ == booleanValue || type_ == nullValue;
  case uintValue:
    return isUInt() ||
           (type_ == realValue && InRange(value_.real_, 0, maxUInt)) ||
           type_ == booleanValue || type_ == nullValue;
  case realValue:
    return isNumeric() || type_ == booleanValue || type_ == nullValue;
  case booleanValue:
    return isNumeric() || type_ == booleanValue || type_ == nullValue;
  case stringValue:
    return isNumeric() || type_ == booleanValue || type_ == stringValue ||
           type_ == nullValue;
  case arrayValue:
    return type_ == arrayValue || type_ == nullValue;
  case objectValue:
    return type_ == objectValue || type_ == nullValue;
  }
  JSON_ASSERT_UNREACHABLE;
  return false;
}

/// Number of values in array or object
ArrayIndex Value::size() const {
  switch (type_) {
  case nullValue:
  case intValue:
  case uintValue:
  case realValue:
  case booleanValue:
  case stringValue:
    return 0;
  case arrayValue: // size of the array is highest index + 1
    if (!value_.map_->empty()) {
      ObjectValues::const_iterator itLast = value_.map_->end();
      --itLast;
      return (*itLast).first.index() + 1;
    }
    return 0;
  case objectValue:
    return ArrayIndex(value_.map_->size());
  }
  JSON_ASSERT_UNREACHABLE;
  return 0; // unreachable;
}

bool Value::empty() const {
  if (isNull() || isArray() || isObject())
    return size() == 0u;
  else
    return false;
}

bool Value::operator!() const { return isNull(); }

void Value::clear() {
  JSON_ASSERT_MESSAGE(type_ == nullValue || type_ == arrayValue ||
                          type_ == objectValue,
                      "in Json::Value::clear(): requires complex value");
  start_ = 0;
  limit_ = 0;
  switch (type_) {
  case arrayValue:
  case objectValue:
    value_.map_->clear();
    break;
  default:
    break;
  }
}

void Value::resize(ArrayIndex newSize) {
  JSON_ASSERT_MESSAGE(type_ == nullValue || type_ == arrayValue,
                      "in Json::Value::resize(): requires arrayValue");
  if (type_ == nullValue)
    *this = Value(arrayValue);
  ArrayIndex oldSize = size();
  if (newSize == 0)
    clear();
  else if (newSize > oldSize)
    (*this)[newSize - 1];
  else {
    for (ArrayIndex index = newSize; index < oldSize; ++index) {
      value_.map_->erase(index);
    }
    assert(size() == newSize);
  }
}

Value& Value::operator[](ArrayIndex index) {
  JSON_ASSERT_MESSAGE(
      type_ == nullValue || type_ == arrayValue,
      "in Json::Value::operator[](ArrayIndex): requires arrayValue");
  if (type_ == nullValue)
    *this = Value(arrayValue);
  CZString key(index);
  ObjectValues::iterator it = value_.map_->lower_bound(key);
  if (it != value_.map_->end() && (*it).first == key)
    return (*it).second;

  ObjectValues::value_type defaultValue(key, nullRef);
  it = value_.map_->insert(it, defaultValue);
  return (*it).second;
}

Value& Value::operator[](int index) {
  JSON_ASSERT_MESSAGE(
      index >= 0,
      "in Json::Value::operator[](int index): index cannot be negative");
  return (*this)[ArrayIndex(index)];
}

const Value& Value::operator[](ArrayIndex index) const {
  JSON_ASSERT_MESSAGE(
      type_ == nullValue || type_ == arrayValue,
      "in Json::Value::operator[](ArrayIndex)const: requires arrayValue");
  if (type_ == nullValue)
    return nullRef;
  CZString key(index);
  ObjectValues::const_iterator it = value_.map_->find(key);
  if (it == value_.map_->end())
    return nullRef;
  return (*it).second;
}

const Value& Value::operator[](int index) const {
  JSON_ASSERT_MESSAGE(
      index >= 0,
      "in Json::Value::operator[](int index) const: index cannot be negative");
  return (*this)[ArrayIndex(index)];
}

void Value::initBasic(ValueType vtype, bool allocated) {
  type_ = vtype;
  allocated_ = allocated;
  comments_ = 0;
  start_ = 0;
  limit_ = 0;
}

// Access an object value by name, create a null member if it does not exist.
// @pre Type of '*this' is object or null.
// @param key is null-terminated.
Value& Value::resolveReference(const char* key) {
  JSON_ASSERT_MESSAGE(
      type_ == nullValue || type_ == objectValue,
      "in Json::Value::resolveReference(): requires objectValue");
  if (type_ == nullValue)
    *this = Value(objectValue);
  CZString actualKey(
      key, static_cast<unsigned>(strlen(key)), CZString::noDuplication); // NOTE!
  ObjectValues::iterator it = value_.map_->lower_bound(actualKey);
  if (it != value_.map_->end() && (*it).first == actualKey)
    return (*it).second;

  ObjectValues::value_type defaultValue(actualKey, nullRef);
  it = value_.map_->insert(it, defaultValue);
  Value& value = (*it).second;
  return value;
}

// @param key is not null-terminated.
Value& Value::resolveReference(char const* key, char const* cend)
{
  JSON_ASSERT_MESSAGE(
      type_ == nullValue || type_ == objectValue,
      "in Json::Value::resolveReference(key, end): requires objectValue");
  if (type_ == nullValue)
    *this = Value(objectValue);
  CZString actualKey(
      key, static_cast<unsigned>(cend-key), CZString::duplicateOnCopy);
  ObjectValues::iterator it = value_.map_->lower_bound(actualKey);
  if (it != value_.map_->end() && (*it).first == actualKey)
    return (*it).second;

  ObjectValues::value_type defaultValue(actualKey, nullRef);
  it = value_.map_->insert(it, defaultValue);
  Value& value = (*it).second;
  return value;
}

Value Value::get(ArrayIndex index, const Value& defaultValue) const {
  const Value* value = &((*this)[index]);
  return value == &nullRef ? defaultValue : *value;
}

bool Value::isValidIndex(ArrayIndex index) const { return index < size(); }

Value const* Value::find(char const* key, char const* cend) const
{
  JSON_ASSERT_MESSAGE(
      type_ == nullValue || type_ == objectValue,
      "in Json::Value::find(key, end, found): requires objectValue or nullValue");
  if (type_ == nullValue) return NULL;
  CZString actualKey(key, static_cast<unsigned>(cend-key), CZString::noDuplication);
  ObjectValues::const_iterator it = value_.map_->find(actualKey);
  if (it == value_.map_->end()) return NULL;
  return &(*it).second;
}
const Value& Value::operator[](const char* key) const
{
  Value const* found = find(key, key + strlen(key));
  if (!found) return nullRef;
  return *found;
}
Value const& Value::operator[](std::string const& key) const
{
  Value const* found = find(key.data(), key.data() + key.length());
  if (!found) return nullRef;
  return *found;
}

Value& Value::operator[](const char* key) {
  return resolveReference(key, key + strlen(key));
}

Value& Value::operator[](const std::string& key) {
  return resolveReference(key.data(), key.data() + key.length());
}

Value& Value::operator[](const StaticString& key) {
  return resolveReference(key.c_str());
}

#ifdef JSON_USE_CPPTL
Value& Value::operator[](const CppTL::ConstString& key) {
  return resolveReference(key.c_str(), key.end_c_str());
}
Value const& Value::operator[](CppTL::ConstString const& key) const
{
  Value const* found = find(key.c_str(), key.end_c_str());
  if (!found) return nullRef;
  return *found;
}
#endif

Value& Value::append(const Value& value) { return (*this)[size()] = value; }

Value Value::get(char const* key, char const* cend, Value const& defaultValue) const
{
  Value const* found = find(key, cend);
  return !found ? defaultValue : *found;
}
Value Value::get(char const* key, Value const& defaultValue) const
{
  return get(key, key + strlen(key), defaultValue);
}
Value Value::get(std::string const& key, Value const& defaultValue) const
{
  return get(key.data(), key.data() + key.length(), defaultValue);
}


bool Value::removeMember(const char* key, const char* cend, Value* removed)
{
  if (type_ != objectValue) {
    return false;
  }
  CZString actualKey(key, static_cast<unsigned>(cend-key), CZString::noDuplication);
  ObjectValues::iterator it = value_.map_->find(actualKey);
  if (it == value_.map_->end())
    return false;
  *removed = it->second;
  value_.map_->erase(it);
  return true;
}
bool Value::removeMember(const char* key, Value* removed)
{
  return removeMember(key, key + strlen(key), removed);
}
bool Value::removeMember(std::string const& key, Value* removed)
{
  return removeMember(key.data(), key.data() + key.length(), removed);
}
Value Value::removeMember(const char* key)
{
  JSON_ASSERT_MESSAGE(type_ == nullValue || type_ == objectValue,
                      "in Json::Value::removeMember(): requires objectValue");
  if (type_ == nullValue)
    return nullRef;

  Value removed;  // null
  removeMember(key, key + strlen(key), &removed);
  return removed; // still null if removeMember() did nothing
}
Value Value::removeMember(const std::string& key)
{
  return removeMember(key.c_str());
}

bool Value::removeIndex(ArrayIndex index, Value* removed) {
  if (type_ != arrayValue) {
    return false;
  }
  CZString key(index);
  ObjectValues::iterator it = value_.map_->find(key);
  if (it == value_.map_->end()) {
    return false;
  }
  *removed = it->second;
  ArrayIndex oldSize = size();
  // shift left all items left, into the place of the "removed"
  for (ArrayIndex i = index; i < (oldSize - 1); ++i){
    CZString keey(i);
    (*value_.map_)[keey] = (*this)[i + 1];
  }
  // erase the last one ("leftover")
  CZString keyLast(oldSize - 1);
  ObjectValues::iterator itLast = value_.map_->find(keyLast);
  value_.map_->erase(itLast);
  return true;
}

#ifdef JSON_USE_CPPTL
Value Value::get(const CppTL::ConstString& key,
                 const Value& defaultValue) const {
  return get(key.c_str(), key.end_c_str(), defaultValue);
}
#endif

bool Value::isMember(char const* key, char const* cend) const
{
  Value const* value = find(key, cend);
  return NULL != value;
}
bool Value::isMember(char const* key) const
{
  return isMember(key, key + strlen(key));
}
bool Value::isMember(std::string const& key) const
{
  return isMember(key.data(), key.data() + key.length());
}

#ifdef JSON_USE_CPPTL
bool Value::isMember(const CppTL::ConstString& key) const {
  return isMember(key.c_str(), key.end_c_str());
}
#endif

Value::Members Value::getMemberNames() const {
  JSON_ASSERT_MESSAGE(
      type_ == nullValue || type_ == objectValue,
      "in Json::Value::getMemberNames(), value must be objectValue");
  if (type_ == nullValue)
    return Value::Members();
  Members members;
  members.reserve(value_.map_->size());
  ObjectValues::const_iterator it = value_.map_->begin();
  ObjectValues::const_iterator itEnd = value_.map_->end();
  for (; it != itEnd; ++it) {
    members.push_back(std::string((*it).first.data(),
                                  (*it).first.length()));
  }
  return members;
}
//
//# ifdef JSON_USE_CPPTL
// EnumMemberNames
// Value::enumMemberNames() const
//{
//   if ( type_ == objectValue )
//   {
//      return CppTL::Enum::any(  CppTL::Enum::transform(
//         CppTL::Enum::keys( *(value_.map_), CppTL::Type<const CZString &>() ),
//         MemberNamesTransform() ) );
//   }
//   return EnumMemberNames();
//}
//
//
// EnumValues
// Value::enumValues() const
//{
//   if ( type_ == objectValue  ||  type_ == arrayValue )
//      return CppTL::Enum::anyValues( *(value_.map_),
//                                     CppTL::Type<const Value &>() );
//   return EnumValues();
//}
//
//# endif

static bool IsIntegral(double d) {
  double integral_part;
  return modf(d, &integral_part) == 0.0;
}

bool Value::isNull() const { return type_ == nullValue; }

bool Value::isBool() const { return type_ == booleanValue; }

bool Value::isInt() const {
  switch (type_) {
  case intValue:
    return value_.int_ >= minInt && value_.int_ <= maxInt;
  case uintValue:
    return value_.uint_ <= UInt(maxInt);
  case realValue:
    return value_.real_ >= minInt && value_.real_ <= maxInt &&
           IsIntegral(value_.real_);
  default:
    break;
  }
  return false;
}

bool Value::isUInt() const {
  switch (type_) {
  case intValue:
    return value_.int_ >= 0 && LargestUInt(value_.int_) <= LargestUInt(maxUInt);
  case uintValue:
    return value_.uint_ <= maxUInt;
  case realValue:
    return value_.real_ >= 0 && value_.real_ <= maxUInt &&
           IsIntegral(value_.real_);
  default:
    break;
  }
  return false;
}

bool Value::isInt64() const {
#if defined(JSON_HAS_INT64)
  switch (type_) {
  case intValue:
    return true;
  case uintValue:
    return value_.uint_ <= UInt64(maxInt64);
  case realValue:
    // Note that maxInt64 (= 2^63 - 1) is not exactly representable as a
    // double, so double(maxInt64) will be rounded up to 2^63. Therefore we
    // require the value to be strictly less than the limit.
    return value_.real_ >= double(minInt64) &&
           value_.real_ < double(maxInt64) && IsIntegral(value_.real_);
  default:
    break;
  }
#endif // JSON_HAS_INT64
  return false;
}

bool Value::isUInt64() const {
#if defined(JSON_HAS_INT64)
  switch (type_) {
  case intValue:
    return value_.int_ >= 0;
  case uintValue:
    return true;
  case realValue:
    // Note that maxUInt64 (= 2^64 - 1) is not exactly representable as a
    // double, so double(maxUInt64) will be rounded up to 2^64. Therefore we
    // require the value to be strictly less than the limit.
    return value_.real_ >= 0 && value_.real_ < maxUInt64AsDouble &&
           IsIntegral(value_.real_);
  default:
    break;
  }
#endif // JSON_HAS_INT64
  return false;
}

bool Value::isIntegral() const {
#if defined(JSON_HAS_INT64)
  return isInt64() || isUInt64();
#else
  return isInt() || isUInt();
#endif
}

bool Value::isDouble() const { return type_ == realValue || isIntegral(); }

bool Value::isNumeric() const { return isIntegral() || isDouble(); }

bool Value::isString() const { return type_ == stringValue; }

bool Value::isArray() const { return type_ == arrayValue; }

bool Value::isObject() const { return type_ == objectValue; }

void Value::setComment(const char* comment, size_t len, CommentPlacement placement) {
  if (!comments_)
    comments_ = new CommentInfo[numberOfCommentPlacement];
  if ((len > 0) && (comment[len-1] == '\n')) {
    // Always discard trailing newline, to aid indentation.
    len -= 1;
  }
  comments_[placement].setComment(comment, len);
}

void Value::setComment(const char* comment, CommentPlacement placement) {
  setComment(comment, strlen(comment), placement);
}

void Value::setComment(const std::string& comment, CommentPlacement placement) {
  setComment(comment.c_str(), comment.length(), placement);
}

bool Value::hasComment(CommentPlacement placement) const {
  return comments_ != 0 && comments_[placement].comment_ != 0;
}

std::string Value::getComment(CommentPlacement placement) const {
  if (hasComment(placement))
    return comments_[placement].comment_;
  return "";
}

void Value::setOffsetStart(size_t start) { start_ = start; }

void Value::setOffsetLimit(size_t limit) { limit_ = limit; }

size_t Value::getOffsetStart() const { return start_; }

size_t Value::getOffsetLimit() const { return limit_; }

std::string Value::toStyledString() const {
  StyledWriter writer;
  return writer.write(*this);
}

Value::const_iterator Value::begin() const {
  switch (type_) {
  case arrayValue:
  case objectValue:
    if (value_.map_)
      return const_iterator(value_.map_->begin());
    break;
  default:
    break;
  }
  return const_iterator();
}

Value::const_iterator Value::end() const {
  switch (type_) {
  case arrayValue:
  case objectValue:
    if (value_.map_)
      return const_iterator(value_.map_->end());
    break;
  default:
    break;
  }
  return const_iterator();
}

Value::iterator Value::begin() {
  switch (type_) {
  case arrayValue:
  case objectValue:
    if (value_.map_)
      return iterator(value_.map_->begin());
    break;
  default:
    break;
  }
  return iterator();
}

Value::iterator Value::end() {
  switch (type_) {
  case arrayValue:
  case objectValue:
    if (value_.map_)
      return iterator(value_.map_->end());
    break;
  default:
    break;
  }
  return iterator();
}

// class PathArgument
// //////////////////////////////////////////////////////////////////

PathArgument::PathArgument() : key_(), index_(), kind_(kindNone) {}

PathArgument::PathArgument(ArrayIndex index)
    : key_(), index_(index), kind_(kindIndex) {}

PathArgument::PathArgument(const char* key)
    : key_(key), index_(), kind_(kindKey) {}

PathArgument::PathArgument(const std::string& key)
    : key_(key.c_str()), index_(), kind_(kindKey) {}

// class Path
// //////////////////////////////////////////////////////////////////

Path::Path(const std::string& path,
           const PathArgument& a1,
           const PathArgument& a2,
           const PathArgument& a3,
           const PathArgument& a4,
           const PathArgument& a5) {
  InArgs in;
  in.push_back(&a1);
  in.push_back(&a2);
  in.push_back(&a3);
  in.push_back(&a4);
  in.push_back(&a5);
  makePath(path, in);
}

void Path::makePath(const std::string& path, const InArgs& in) {
  const char* current = path.c_str();
  const char* end = current + path.length();
  InArgs::const_iterator itInArg = in.begin();
  while (current != end) {
    if (*current == '[') {
      ++current;
      if (*current == '%')
        addPathInArg(path, in, itInArg, PathArgument::kindIndex);
      else {
        ArrayIndex index = 0;
        for (; current != end && *current >= '0' && *current <= '9'; ++current)
          index = index * 10 + ArrayIndex(*current - '0');
        args_.push_back(index);
      }
      if (current == end || *current++ != ']')
        invalidPath(path, int(current - path.c_str()));
    } else if (*current == '%') {
      addPathInArg(path, in, itInArg, PathArgument::kindKey);
      ++current;
    } else if (*current == '.') {
      ++current;
    } else {
      const char* beginName = current;
      while (current != end && !strchr("[.", *current))
        ++current;
      args_.push_back(std::string(beginName, current));
    }
  }
}

void Path::addPathInArg(const std::string& /*path*/,
                        const InArgs& in,
                        InArgs::const_iterator& itInArg,
                        PathArgument::Kind kind) {
  if (itInArg == in.end()) {
    // Error: missing argument %d
  } else if ((*itInArg)->kind_ != kind) {
    // Error: bad argument type
  } else {
    args_.push_back(**itInArg);
  }
}

void Path::invalidPath(const std::string& /*path*/, int /*location*/) {
  // Error: invalid path.
}

const Value& Path::resolve(const Value& root) const {
  const Value* node = &root;
  for (Args::const_iterator it = args_.begin(); it != args_.end(); ++it) {
    const PathArgument& arg = *it;
    if (arg.kind_ == PathArgument::kindIndex) {
      if (!node->isArray() || !node->isValidIndex(arg.index_)) {
        // Error: unable to resolve path (array value expected at position...
      }
      node = &((*node)[arg.index_]);
    } else if (arg.kind_ == PathArgument::kindKey) {
      if (!node->isObject()) {
        // Error: unable to resolve path (object value expected at position...)
      }
      node = &((*node)[arg.key_]);
      if (node == &Value::nullRef) {
        // Error: unable to resolve path (object has no member named '' at
        // position...)
      }
    }
  }
  return *node;
}

Value Path::resolve(const Value& root, const Value& defaultValue) const {
  const Value* node = &root;
  for (Args::const_iterator it = args_.begin(); it != args_.end(); ++it) {
    const PathArgument& arg = *it;
    if (arg.kind_ == PathArgument::kindIndex) {
      if (!node->isArray() || !node->isValidIndex(arg.index_))
        return defaultValue;
      node = &((*node)[arg.index_]);
    } else if (arg.kind_ == PathArgument::kindKey) {
      if (!node->isObject())
        return defaultValue;
      node = &((*node)[arg.key_]);
      if (node == &Value::nullRef)
        return defaultValue;
    }
  }
  return *node;
}

Value& Path::make(Value& root) const {
  Value* node = &root;
  for (Args::const_iterator it = args_.begin(); it != args_.end(); ++it) {
    const PathArgument& arg = *it;
    if (arg.kind_ == PathArgument::kindIndex) {
      if (!node->isArray()) {
        // Error: node is not an array at position ...
      }
      node = &((*node)[arg.index_]);
    } else if (arg.kind_ == PathArgument::kindKey) {
      if (!node->isObject()) {
        // Error: node is not an object at position...
      }
      node = &((*node)[arg.key_]);
    }
  }
  return *node;
}

} // namespace Json

// //////////////////////////////////////////////////////////////////////
// End of content of file: src/lib_json/json_value.cpp
// //////////////////////////////////////////////////////////////////////






// //////////////////////////////////////////////////////////////////////
// Beginning of content of file: src/lib_json/json_writer.cpp
// //////////////////////////////////////////////////////////////////////

// Copyright 2011 Baptiste Lepilleur
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.
// See file LICENSE for detail or copy at http://jsoncpp.sourceforge.net/LICENSE

#if !defined(JSON_IS_AMALGAMATION)
#include <json/writer.h>
#include "json_tool.h"
#endif // if !defined(JSON_IS_AMALGAMATION)
#include <iomanip>
#include <memory>
#include <sstream>
#include <utility>
#include <set>
#include <cassert>
#include <cstring>
#include <cstdio>

#if defined(_MSC_VER) && _MSC_VER >= 1200 && _MSC_VER < 1800 // Between VC++ 6.0 and VC++ 11.0
#include <float.h>
#define isfinite _finite
#elif defined(__sun) && defined(__SVR4) //Solaris
#if !defined(isfinite)
#include <ieeefp.h>
#define isfinite finite
#endif
#elif defined(_AIX)
#if !defined(isfinite)
#include <math.h>
#define isfinite finite
#endif
#elif defined(__hpux)
#if !defined(isfinite)
#if defined(__ia64) && !defined(finite)
#define isfinite(x) ((sizeof(x) == sizeof(float) ? \
                     _Isfinitef(x) : _IsFinite(x)))
#else
#include <math.h>
#define isfinite finite
#endif
#endif
#else
#include <cmath>
#if !(defined(__QNXNTO__)) // QNX already defines isfinite
#define isfinite std::isfinite
#endif
#endif

#if defined(_MSC_VER)
#if !defined(WINCE) && defined(__STDC_SECURE_LIB__) && _MSC_VER >= 1500 // VC++ 9.0 and above
#define snprintf sprintf_s
#elif _MSC_VER >= 1900 // VC++ 14.0 and above
#define snprintf std::snprintf
#else
#define snprintf _snprintf
#endif
#elif defined(__ANDROID__) || defined(__QNXNTO__)
#define snprintf snprintf
#elif __cplusplus >= 201103L
#define snprintf std::snprintf
#endif

#if defined(__BORLANDC__)
#include <float.h>
#define isfinite _finite
#define snprintf _snprintf
#endif

#if defined(_MSC_VER) && _MSC_VER >= 1400 // VC++ 8.0
// Disable warning about strdup being deprecated.
#pragma warning(disable : 4996)
#endif

namespace Json {

#if __cplusplus >= 201103L || (defined(_CPPLIB_VER) && _CPPLIB_VER >= 520)
typedef std::unique_ptr<StreamWriter> StreamWriterPtr;
#else
typedef std::auto_ptr<StreamWriter>   StreamWriterPtr;
#endif

static bool containsControlCharacter(const char* str) {
  while (*str) {
    if (isControlCharacter(*(str++)))
      return true;
  }
  return false;
}

static bool containsControlCharacter0(const char* str, unsigned len) {
  char const* end = str + len;
  while (end != str) {
    if (isControlCharacter(*str) || 0==*str)
      return true;
    ++str;
  }
  return false;
}

std::string valueToString(LargestInt value) {
  UIntToStringBuffer buffer;
  char* current = buffer + sizeof(buffer);
  if (value == Value::minLargestInt) {
    uintToString(LargestUInt(Value::maxLargestInt) + 1, current);
    *--current = '-';
  } else if (value < 0) {
    uintToString(LargestUInt(-value), current);
    *--current = '-';
  } else {
    uintToString(LargestUInt(value), current);
  }
  assert(current >= buffer);
  return current;
}

std::string valueToString(LargestUInt value) {
  UIntToStringBuffer buffer;
  char* current = buffer + sizeof(buffer);
  uintToString(value, current);
  assert(current >= buffer);
  return current;
}

#if defined(JSON_HAS_INT64)

std::string valueToString(Int value) {
  return valueToString(LargestInt(value));
}

std::string valueToString(UInt value) {
  return valueToString(LargestUInt(value));
}

#endif // # if defined(JSON_HAS_INT64)

std::string valueToString(double value, bool useSpecialFloats, unsigned int precision) {
  // Allocate a buffer that is more than large enough to store the 16 digits of
  // precision requested below.
  char buffer[32];
  int len = -1;

  char formatString[6];
  sprintf(formatString, "%%.%dg", precision);

  // Print into the buffer. We need not request the alternative representation
  // that always has a decimal point because JSON doesn't distingish the
  // concepts of reals and integers.
  if (isfinite(value)) {
    len = snprintf(buffer, sizeof(buffer), formatString, value);
  } else {
    // IEEE standard states that NaN values will not compare to themselves
    if (value != value) {
      len = snprintf(buffer, sizeof(buffer), useSpecialFloats ? "NaN" : "null");
    } else if (value < 0) {
      len = snprintf(buffer, sizeof(buffer), useSpecialFloats ? "-Infinity" : "-1e+9999");
    } else {
      len = snprintf(buffer, sizeof(buffer), useSpecialFloats ? "Infinity" : "1e+9999");
    }
    // For those, we do not need to call fixNumLoc, but it is fast.
  }
  assert(len >= 0);
  fixNumericLocale(buffer, buffer + len);
  return buffer;
}

std::string valueToString(double value) { return valueToString(value, false, 17); }

std::string valueToString(bool value) { return value ? "true" : "false"; }

std::string valueToQuotedString(const char* value) {
  if (value == NULL)
    return "";
  // Not sure how to handle unicode...
  if (strpbrk(value, "\"\\\b\f\n\r\t") == NULL &&
      !containsControlCharacter(value))
    return std::string("\"") + value + "\"";
  // We have to walk value and escape any special characters.
  // Appending to std::string is not efficient, but this should be rare.
  // (Note: forward slashes are *not* rare, but I am not escaping them.)
  std::string::size_type maxsize =
      strlen(value) * 2 + 3; // allescaped+quotes+NULL
  std::string result;
  result.reserve(maxsize); // to avoid lots of mallocs
  result += "\"";
  for (const char* c = value; *c != 0; ++c) {
    switch (*c) {
    case '\"':
      result += "\\\"";
      break;
    case '\\':
      result += "\\\\";
      break;
    case '\b':
      result += "\\b";
      break;
    case '\f':
      result += "\\f";
      break;
    case '\n':
      result += "\\n";
      break;
    case '\r':
      result += "\\r";
      break;
    case '\t':
      result += "\\t";
      break;
    // case '/':
    // Even though \/ is considered a legal escape in JSON, a bare
    // slash is also legal, so I see no reason to escape it.
    // (I hope I am not misunderstanding something.
    // blep notes: actually escaping \/ may be useful in javascript to avoid </
    // sequence.
    // Should add a flag to allow this compatibility mode and prevent this
    // sequence from occurring.
    default:
      if (isControlCharacter(*c)) {
        std::ostringstream oss;
        oss << "\\u" << std::hex << std::uppercase << std::setfill('0')
            << std::setw(4) << static_cast<int>(*c);
        result += oss.str();
      } else {
        result += *c;
      }
      break;
    }
  }
  result += "\"";
  return result;
}

// https://github.com/upcaste/upcaste/blob/master/src/upcore/src/cstring/strnpbrk.cpp
static char const* strnpbrk(char const* s, char const* accept, size_t n) {
  assert((s || !n) && accept);

  char const* const end = s + n;
  for (char const* cur = s; cur < end; ++cur) {
    int const c = *cur;
    for (char const* a = accept; *a; ++a) {
      if (*a == c) {
        return cur;
      }
    }
  }
  return NULL;
}
static std::string valueToQuotedStringN(const char* value, unsigned length) {
  if (value == NULL)
    return "";
  // Not sure how to handle unicode...
  if (strnpbrk(value, "\"\\\b\f\n\r\t", length) == NULL &&
      !containsControlCharacter0(value, length))
    return std::string("\"") + value + "\"";
  // We have to walk value and escape any special characters.
  // Appending to std::string is not efficient, but this should be rare.
  // (Note: forward slashes are *not* rare, but I am not escaping them.)
  std::string::size_type maxsize =
      length * 2 + 3; // allescaped+quotes+NULL
  std::string result;
  result.reserve(maxsize); // to avoid lots of mallocs
  result += "\"";
  char const* end = value + length;
  for (const char* c = value; c != end; ++c) {
    switch (*c) {
    case '\"':
      result += "\\\"";
      break;
    case '\\':
      result += "\\\\";
      break;
    case '\b':
      result += "\\b";
      break;
    case '\f':
      result += "\\f";
      break;
    case '\n':
      result += "\\n";
      break;
    case '\r':
      result += "\\r";
      break;
    case '\t':
      result += "\\t";
      break;
    // case '/':
    // Even though \/ is considered a legal escape in JSON, a bare
    // slash is also legal, so I see no reason to escape it.
    // (I hope I am not misunderstanding something.)
    // blep notes: actually escaping \/ may be useful in javascript to avoid </
    // sequence.
    // Should add a flag to allow this compatibility mode and prevent this
    // sequence from occurring.
    default:
      if ((isControlCharacter(*c)) || (*c == 0)) {
        std::ostringstream oss;
        oss << "\\u" << std::hex << std::uppercase << std::setfill('0')
            << std::setw(4) << static_cast<int>(*c);
        result += oss.str();
      } else {
        result += *c;
      }
      break;
    }
  }
  result += "\"";
  return result;
}

// Class Writer
// //////////////////////////////////////////////////////////////////
Writer::~Writer() {}

// Class FastWriter
// //////////////////////////////////////////////////////////////////

FastWriter::FastWriter()
    : yamlCompatiblityEnabled_(false), dropNullPlaceholders_(false),
      omitEndingLineFeed_(false) {}

void FastWriter::enableYAMLCompatibility() { yamlCompatiblityEnabled_ = true; }

void FastWriter::dropNullPlaceholders() { dropNullPlaceholders_ = true; }

void FastWriter::omitEndingLineFeed() { omitEndingLineFeed_ = true; }

std::string FastWriter::write(const Value& root) {
  document_ = "";
  writeValue(root);
  if (!omitEndingLineFeed_)
    document_ += "\n";
  return document_;
}

void FastWriter::writeValue(const Value& value) {
  switch (value.type()) {
  case nullValue:
    if (!dropNullPlaceholders_)
      document_ += "null";
    break;
  case intValue:
    document_ += valueToString(value.asLargestInt());
    break;
  case uintValue:
    document_ += valueToString(value.asLargestUInt());
    break;
  case realValue:
    document_ += valueToString(value.asDouble());
    break;
  case stringValue:
  {
    // Is NULL possible for value.string_?
    char const* str;
    char const* end;
    bool ok = value.getString(&str, &end);
    if (ok) document_ += valueToQuotedStringN(str, static_cast<unsigned>(end-str));
    break;
  }
  case booleanValue:
    document_ += valueToString(value.asBool());
    break;
  case arrayValue: {
    document_ += '[';
    int size = value.size();
    for (int index = 0; index < size; ++index) {
      if (index > 0)
        document_ += ',';
      writeValue(value[index]);
    }
    document_ += ']';
  } break;
  case objectValue: {
    Value::Members members(value.getMemberNames());
    document_ += '{';
    for (Value::Members::iterator it = members.begin(); it != members.end();
         ++it) {
      const std::string& name = *it;
      if (it != members.begin())
        document_ += ',';
      document_ += valueToQuotedStringN(name.data(), static_cast<unsigned>(name.length()));
      document_ += yamlCompatiblityEnabled_ ? ": " : ":";
      writeValue(value[name]);
    }
    document_ += '}';
  } break;
  }
}

// Class StyledWriter
// //////////////////////////////////////////////////////////////////

StyledWriter::StyledWriter()
    : rightMargin_(74), indentSize_(3), addChildValues_() {}

std::string StyledWriter::write(const Value& root) {
  document_ = "";
  addChildValues_ = false;
  indentString_ = "";
  writeCommentBeforeValue(root);
  writeValue(root);
  writeCommentAfterValueOnSameLine(root);
  document_ += "\n";
  return document_;
}

void StyledWriter::writeValue(const Value& value) {
  switch (value.type()) {
  case nullValue:
    pushValue("null");
    break;
  case intValue:
    pushValue(valueToString(value.asLargestInt()));
    break;
  case uintValue:
    pushValue(valueToString(value.asLargestUInt()));
    break;
  case realValue:
    pushValue(valueToString(value.asDouble()));
    break;
  case stringValue:
  {
    // Is NULL possible for value.string_?
    char const* str;
    char const* end;
    bool ok = value.getString(&str, &end);
    if (ok) pushValue(valueToQuotedStringN(str, static_cast<unsigned>(end-str)));
    else pushValue("");
    break;
  }
  case booleanValue:
    pushValue(valueToString(value.asBool()));
    break;
  case arrayValue:
    writeArrayValue(value);
    break;
  case objectValue: {
    Value::Members members(value.getMemberNames());
    if (members.empty())
      pushValue("{}");
    else {
      writeWithIndent("{");
      indent();
      Value::Members::iterator it = members.begin();
      for (;;) {
        const std::string& name = *it;
        const Value& childValue = value[name];
        writeCommentBeforeValue(childValue);
        writeWithIndent(valueToQuotedString(name.c_str()));
        document_ += " : ";
        writeValue(childValue);
        if (++it == members.end()) {
          writeCommentAfterValueOnSameLine(childValue);
          break;
        }
        document_ += ',';
        writeCommentAfterValueOnSameLine(childValue);
      }
      unindent();
      writeWithIndent("}");
    }
  } break;
  }
}

void StyledWriter::writeArrayValue(const Value& value) {
  unsigned size = value.size();
  if (size == 0)
    pushValue("[]");
  else {
    bool isArrayMultiLine = isMultineArray(value);
    if (isArrayMultiLine) {
      writeWithIndent("[");
      indent();
      bool hasChildValue = !childValues_.empty();
      unsigned index = 0;
      for (;;) {
        const Value& childValue = value[index];
        writeCommentBeforeValue(childValue);
        if (hasChildValue)
          writeWithIndent(childValues_[index]);
        else {
          writeIndent();
          writeValue(childValue);
        }
        if (++index == size) {
          writeCommentAfterValueOnSameLine(childValue);
          break;
        }
        document_ += ',';
        writeCommentAfterValueOnSameLine(childValue);
      }
      unindent();
      writeWithIndent("]");
    } else // output on a single line
    {
      assert(childValues_.size() == size);
      document_ += "[ ";
      for (unsigned index = 0; index < size; ++index) {
        if (index > 0)
          document_ += ", ";
        document_ += childValues_[index];
      }
      document_ += " ]";
    }
  }
}

bool StyledWriter::isMultineArray(const Value& value) {
  int size = value.size();
  bool isMultiLine = size * 3 >= rightMargin_;
  childValues_.clear();
  for (int index = 0; index < size && !isMultiLine; ++index) {
    const Value& childValue = value[index];
    isMultiLine = ((childValue.isArray() || childValue.isObject()) &&
                        childValue.size() > 0);
  }
  if (!isMultiLine) // check if line length > max line length
  {
    childValues_.reserve(size);
    addChildValues_ = true;
    int lineLength = 4 + (size - 1) * 2; // '[ ' + ', '*n + ' ]'
    for (int index = 0; index < size; ++index) {
      if (hasCommentForValue(value[index])) {
        isMultiLine = true;
      }
      writeValue(value[index]);
      lineLength += int(childValues_[index].length());
    }
    addChildValues_ = false;
    isMultiLine = isMultiLine || lineLength >= rightMargin_;
  }
  return isMultiLine;
}

void StyledWriter::pushValue(const std::string& value) {
  if (addChildValues_)
    childValues_.push_back(value);
  else
    document_ += value;
}

void StyledWriter::writeIndent() {
  if (!document_.empty()) {
    char last = document_[document_.length() - 1];
    if (last == ' ') // already indented
      return;
    if (last != '\n') // Comments may add new-line
      document_ += '\n';
  }
  document_ += indentString_;
}

void StyledWriter::writeWithIndent(const std::string& value) {
  writeIndent();
  document_ += value;
}

void StyledWriter::indent() { indentString_ += std::string(indentSize_, ' '); }

void StyledWriter::unindent() {
  assert(int(indentString_.size()) >= indentSize_);
  indentString_.resize(indentString_.size() - indentSize_);
}

void StyledWriter::writeCommentBeforeValue(const Value& root) {
  if (!root.hasComment(commentBefore))
    return;

  document_ += "\n";
  writeIndent();
  const std::string& comment = root.getComment(commentBefore);
  std::string::const_iterator iter = comment.begin();
  while (iter != comment.end()) {
    document_ += *iter;
    if (*iter == '\n' &&
       (iter != comment.end() && *(iter + 1) == '/'))
      writeIndent();
    ++iter;
  }

  // Comments are stripped of trailing newlines, so add one here
  document_ += "\n";
}

void StyledWriter::writeCommentAfterValueOnSameLine(const Value& root) {
  if (root.hasComment(commentAfterOnSameLine))
    document_ += " " + root.getComment(commentAfterOnSameLine);

  if (root.hasComment(commentAfter)) {
    document_ += "\n";
    document_ += root.getComment(commentAfter);
    document_ += "\n";
  }
}

bool StyledWriter::hasCommentForValue(const Value& value) {
  return value.hasComment(commentBefore) ||
         value.hasComment(commentAfterOnSameLine) ||
         value.hasComment(commentAfter);
}

// Class StyledStreamWriter
// //////////////////////////////////////////////////////////////////

StyledStreamWriter::StyledStreamWriter(std::string indentation)
    : document_(NULL), rightMargin_(74), indentation_(indentation),
      addChildValues_() {}

void StyledStreamWriter::write(std::ostream& out, const Value& root) {
  document_ = &out;
  addChildValues_ = false;
  indentString_ = "";
  indented_ = true;
  writeCommentBeforeValue(root);
  if (!indented_) writeIndent();
  indented_ = true;
  writeValue(root);
  writeCommentAfterValueOnSameLine(root);
  *document_ << "\n";
  document_ = NULL; // Forget the stream, for safety.
}

void StyledStreamWriter::writeValue(const Value& value) {
  switch (value.type()) {
  case nullValue:
    pushValue("null");
    break;
  case intValue:
    pushValue(valueToString(value.asLargestInt()));
    break;
  case uintValue:
    pushValue(valueToString(value.asLargestUInt()));
    break;
  case realValue:
    pushValue(valueToString(value.asDouble()));
    break;
  case stringValue:
  {
    // Is NULL possible for value.string_?
    char const* str;
    char const* end;
    bool ok = value.getString(&str, &end);
    if (ok) pushValue(valueToQuotedStringN(str, static_cast<unsigned>(end-str)));
    else pushValue("");
    break;
  }
  case booleanValue:
    pushValue(valueToString(value.asBool()));
    break;
  case arrayValue:
    writeArrayValue(value);
    break;
  case objectValue: {
    Value::Members members(value.getMemberNames());
    if (members.empty())
      pushValue("{}");
    else {
      writeWithIndent("{");
      indent();
      Value::Members::iterator it = members.begin();
      for (;;) {
        const std::string& name = *it;
        const Value& childValue = value[name];
        writeCommentBeforeValue(childValue);
        writeWithIndent(valueToQuotedString(name.c_str()));
        *document_ << " : ";
        writeValue(childValue);
        if (++it == members.end()) {
          writeCommentAfterValueOnSameLine(childValue);
          break;
        }
        *document_ << ",";
        writeCommentAfterValueOnSameLine(childValue);
      }
      unindent();
      writeWithIndent("}");
    }
  } break;
  }
}

void StyledStreamWriter::writeArrayValue(const Value& value) {
  unsigned size = value.size();
  if (size == 0)
    pushValue("[]");
  else {
    bool isArrayMultiLine = isMultineArray(value);
    if (isArrayMultiLine) {
      writeWithIndent("[");
      indent();
      bool hasChildValue = !childValues_.empty();
      unsigned index = 0;
      for (;;) {
        const Value& childValue = value[index];
        writeCommentBeforeValue(childValue);
        if (hasChildValue)
          writeWithIndent(childValues_[index]);
        else {
          if (!indented_) writeIndent();
          indented_ = true;
          writeValue(childValue);
          indented_ = false;
        }
        if (++index == size) {
          writeCommentAfterValueOnSameLine(childValue);
          break;
        }
        *document_ << ",";
        writeCommentAfterValueOnSameLine(childValue);
      }
      unindent();
      writeWithIndent("]");
    } else // output on a single line
    {
      assert(childValues_.size() == size);
      *document_ << "[ ";
      for (unsigned index = 0; index < size; ++index) {
        if (index > 0)
          *document_ << ", ";
        *document_ << childValues_[index];
      }
      *document_ << " ]";
    }
  }
}

bool StyledStreamWriter::isMultineArray(const Value& value) {
  int size = value.size();
  bool isMultiLine = size * 3 >= rightMargin_;
  childValues_.clear();
  for (int index = 0; index < size && !isMultiLine; ++index) {
    const Value& childValue = value[index];
    isMultiLine = ((childValue.isArray() || childValue.isObject()) &&
                        childValue.size() > 0);
  }
  if (!isMultiLine) // check if line length > max line length
  {
    childValues_.reserve(size);
    addChildValues_ = true;
    int lineLength = 4 + (size - 1) * 2; // '[ ' + ', '*n + ' ]'
    for (int index = 0; index < size; ++index) {
      if (hasCommentForValue(value[index])) {
        isMultiLine = true;
      }
      writeValue(value[index]);
      lineLength += int(childValues_[index].length());
    }
    addChildValues_ = false;
    isMultiLine = isMultiLine || lineLength >= rightMargin_;
  }
  return isMultiLine;
}

void StyledStreamWriter::pushValue(const std::string& value) {
  if (addChildValues_)
    childValues_.push_back(value);
  else
    *document_ << value;
}

void StyledStreamWriter::writeIndent() {
  // blep intended this to look at the so-far-written string
  // to determine whether we are already indented, but
  // with a stream we cannot do that. So we rely on some saved state.
  // The caller checks indented_.
  *document_ << '\n' << indentString_;
}

void StyledStreamWriter::writeWithIndent(const std::string& value) {
  if (!indented_) writeIndent();
  *document_ << value;
  indented_ = false;
}

void StyledStreamWriter::indent() { indentString_ += indentation_; }

void StyledStreamWriter::unindent() {
  assert(indentString_.size() >= indentation_.size());
  indentString_.resize(indentString_.size() - indentation_.size());
}

void StyledStreamWriter::writeCommentBeforeValue(const Value& root) {
  if (!root.hasComment(commentBefore))
    return;

  if (!indented_) writeIndent();
  const std::string& comment = root.getComment(commentBefore);
  std::string::const_iterator iter = comment.begin();
  while (iter != comment.end()) {
    *document_ << *iter;
    if (*iter == '\n' &&
       (iter != comment.end() && *(iter + 1) == '/'))
      // writeIndent();  // would include newline
      *document_ << indentString_;
    ++iter;
  }
  indented_ = false;
}

void StyledStreamWriter::writeCommentAfterValueOnSameLine(const Value& root) {
  if (root.hasComment(commentAfterOnSameLine))
    *document_ << ' ' << root.getComment(commentAfterOnSameLine);

  if (root.hasComment(commentAfter)) {
    writeIndent();
    *document_ << root.getComment(commentAfter);
  }
  indented_ = false;
}

bool StyledStreamWriter::hasCommentForValue(const Value& value) {
  return value.hasComment(commentBefore) ||
         value.hasComment(commentAfterOnSameLine) ||
         value.hasComment(commentAfter);
}

//////////////////////////
// BuiltStyledStreamWriter

/// Scoped enums are not available until C++11.
struct CommentStyle {
  /// Decide whether to write comments.
  enum Enum {
    None,  ///< Drop all comments.
    Most,  ///< Recover odd behavior of previous versions (not implemented yet).
    All  ///< Keep all comments.
  };
};

struct BuiltStyledStreamWriter : public StreamWriter
{
  BuiltStyledStreamWriter(
      std::string const& indentation,
      CommentStyle::Enum cs,
      std::string const& colonSymbol,
      std::string const& nullSymbol,
      std::string const& endingLineFeedSymbol,
      bool useSpecialFloats,
      unsigned int precision);
  int write(Value const& root, std::ostream* sout) override;
private:
  void writeValue(Value const& value);
  void writeArrayValue(Value const& value);
  bool isMultineArray(Value const& value);
  void pushValue(std::string const& value);
  void writeIndent();
  void writeWithIndent(std::string const& value);
  void indent();
  void unindent();
  void writeCommentBeforeValue(Value const& root);
  void writeCommentAfterValueOnSameLine(Value const& root);
  static bool hasCommentForValue(const Value& value);

  typedef std::vector<std::string> ChildValues;

  ChildValues childValues_;
  std::string indentString_;
  int rightMargin_;
  std::string indentation_;
  CommentStyle::Enum cs_;
  std::string colonSymbol_;
  std::string nullSymbol_;
  std::string endingLineFeedSymbol_;
  bool addChildValues_ : 1;
  bool indented_ : 1;
  bool useSpecialFloats_ : 1;
  unsigned int precision_;
};
BuiltStyledStreamWriter::BuiltStyledStreamWriter(
      std::string const& indentation,
      CommentStyle::Enum cs,
      std::string const& colonSymbol,
      std::string const& nullSymbol,
      std::string const& endingLineFeedSymbol,
      bool useSpecialFloats,
      unsigned int precision)
  : rightMargin_(74)
  , indentation_(indentation)
  , cs_(cs)
  , colonSymbol_(colonSymbol)
  , nullSymbol_(nullSymbol)
  , endingLineFeedSymbol_(endingLineFeedSymbol)
  , addChildValues_(false)
  , indented_(false)
  , useSpecialFloats_(useSpecialFloats)
  , precision_(precision)
{
}
int BuiltStyledStreamWriter::write(Value const& root, std::ostream* sout)
{
  sout_ = sout;
  addChildValues_ = false;
  indented_ = true;
  indentString_ = "";
  writeCommentBeforeValue(root);
  if (!indented_) writeIndent();
  indented_ = true;
  writeValue(root);
  writeCommentAfterValueOnSameLine(root);
  *sout_ << endingLineFeedSymbol_;
  sout_ = NULL;
  return 0;
}
void BuiltStyledStreamWriter::writeValue(Value const& value) {
  switch (value.type()) {
  case nullValue:
    pushValue(nullSymbol_);
    break;
  case intValue:
    pushValue(valueToString(value.asLargestInt()));
    break;
  case uintValue:
    pushValue(valueToString(value.asLargestUInt()));
    break;
  case realValue:
    pushValue(valueToString(value.asDouble(), useSpecialFloats_, precision_));
    break;
  case stringValue:
  {
    // Is NULL is possible for value.string_?
    char const* str;
    char const* end;
    bool ok = value.getString(&str, &end);
    if (ok) pushValue(valueToQuotedStringN(str, static_cast<unsigned>(end-str)));
    else pushValue("");
    break;
  }
  case booleanValue:
    pushValue(valueToString(value.asBool()));
    break;
  case arrayValue:
    writeArrayValue(value);
    break;
  case objectValue: {
    Value::Members members(value.getMemberNames());
    if (members.empty())
      pushValue("{}");
    else {
      writeWithIndent("{");
      indent();
      Value::Members::iterator it = members.begin();
      for (;;) {
        std::string const& name = *it;
        Value const& childValue = value[name];
        writeCommentBeforeValue(childValue);
        writeWithIndent(valueToQuotedStringN(name.data(), static_cast<unsigned>(name.length())));
        *sout_ << colonSymbol_;
        writeValue(childValue);
        if (++it == members.end()) {
          writeCommentAfterValueOnSameLine(childValue);
          break;
        }
        *sout_ << ",";
        writeCommentAfterValueOnSameLine(childValue);
      }
      unindent();
      writeWithIndent("}");
    }
  } break;
  }
}

void BuiltStyledStreamWriter::writeArrayValue(Value const& value) {
  unsigned size = value.size();
  if (size == 0)
    pushValue("[]");
  else {
    bool isMultiLine = (cs_ == CommentStyle::All) || isMultineArray(value);
    if (isMultiLine) {
      writeWithIndent("[");
      indent();
      bool hasChildValue = !childValues_.empty();
      unsigned index = 0;
      for (;;) {
        Value const& childValue = value[index];
        writeCommentBeforeValue(childValue);
        if (hasChildValue)
          writeWithIndent(childValues_[index]);
        else {
          if (!indented_) writeIndent();
          indented_ = true;
          writeValue(childValue);
          indented_ = false;
        }
        if (++index == size) {
          writeCommentAfterValueOnSameLine(childValue);
          break;
        }
        *sout_ << ",";
        writeCommentAfterValueOnSameLine(childValue);
      }
      unindent();
      writeWithIndent("]");
    } else // output on a single line
    {
      assert(childValues_.size() == size);
      *sout_ << "[";
      if (!indentation_.empty()) *sout_ << " ";
      for (unsigned index = 0; index < size; ++index) {
        if (index > 0)
          *sout_ << ", ";
        *sout_ << childValues_[index];
      }
      if (!indentation_.empty()) *sout_ << " ";
      *sout_ << "]";
    }
  }
}

bool BuiltStyledStreamWriter::isMultineArray(Value const& value) {
  int size = value.size();
  bool isMultiLine = size * 3 >= rightMargin_;
  childValues_.clear();
  for (int index = 0; index < size && !isMultiLine; ++index) {
    Value const& childValue = value[index];
    isMultiLine = ((childValue.isArray() || childValue.isObject()) &&
                        childValue.size() > 0);
  }
  if (!isMultiLine) // check if line length > max line length
  {
    childValues_.reserve(size);
    addChildValues_ = true;
    int lineLength = 4 + (size - 1) * 2; // '[ ' + ', '*n + ' ]'
    for (int index = 0; index < size; ++index) {
      if (hasCommentForValue(value[index])) {
        isMultiLine = true;
      }
      writeValue(value[index]);
      lineLength += int(childValues_[index].length());
    }
    addChildValues_ = false;
    isMultiLine = isMultiLine || lineLength >= rightMargin_;
  }
  return isMultiLine;
}

void BuiltStyledStreamWriter::pushValue(std::string const& value) {
  if (addChildValues_)
    childValues_.push_back(value);
  else
    *sout_ << value;
}

void BuiltStyledStreamWriter::writeIndent() {
  // blep intended this to look at the so-far-written string
  // to determine whether we are already indented, but
  // with a stream we cannot do that. So we rely on some saved state.
  // The caller checks indented_.

  if (!indentation_.empty()) {
    // In this case, drop newlines too.
    *sout_ << '\n' << indentString_;
  }
}

void BuiltStyledStreamWriter::writeWithIndent(std::string const& value) {
  if (!indented_) writeIndent();
  *sout_ << value;
  indented_ = false;
}

void BuiltStyledStreamWriter::indent() { indentString_ += indentation_; }

void BuiltStyledStreamWriter::unindent() {
  assert(indentString_.size() >= indentation_.size());
  indentString_.resize(indentString_.size() - indentation_.size());
}

void BuiltStyledStreamWriter::writeCommentBeforeValue(Value const& root) {
  if (cs_ == CommentStyle::None) return;
  if (!root.hasComment(commentBefore))
    return;

  if (!indented_) writeIndent();
  const std::string& comment = root.getComment(commentBefore);
  std::string::const_iterator iter = comment.begin();
  while (iter != comment.end()) {
    *sout_ << *iter;
    if (*iter == '\n' &&
       (iter != comment.end() && *(iter + 1) == '/'))
      // writeIndent();  // would write extra newline
      *sout_ << indentString_;
    ++iter;
  }
  indented_ = false;
}

void BuiltStyledStreamWriter::writeCommentAfterValueOnSameLine(Value const& root) {
  if (cs_ == CommentStyle::None) return;
  if (root.hasComment(commentAfterOnSameLine))
    *sout_ << " " + root.getComment(commentAfterOnSameLine);

  if (root.hasComment(commentAfter)) {
    writeIndent();
    *sout_ << root.getComment(commentAfter);
  }
}

// static
bool BuiltStyledStreamWriter::hasCommentForValue(const Value& value) {
  return value.hasComment(commentBefore) ||
         value.hasComment(commentAfterOnSameLine) ||
         value.hasComment(commentAfter);
}

///////////////
// StreamWriter

StreamWriter::StreamWriter()
    : sout_(NULL)
{
}
StreamWriter::~StreamWriter()
{
}
StreamWriter::Factory::~Factory()
{}
StreamWriterBuilder::StreamWriterBuilder()
{
  setDefaults(&settings_);
}
StreamWriterBuilder::~StreamWriterBuilder()
{}
StreamWriter* StreamWriterBuilder::newStreamWriter() const
{
  std::string indentation = settings_["indentation"].asString();
  std::string cs_str = settings_["commentStyle"].asString();
  bool eyc = settings_["enableYAMLCompatibility"].asBool();
  bool dnp = settings_["dropNullPlaceholders"].asBool();
  bool usf = settings_["useSpecialFloats"].asBool();
  unsigned int pre = settings_["precision"].asUInt();
  CommentStyle::Enum cs = CommentStyle::All;
  if (cs_str == "All") {
    cs = CommentStyle::All;
  } else if (cs_str == "None") {
    cs = CommentStyle::None;
  } else {
    throwRuntimeError("commentStyle must be 'All' or 'None'");
  }
  std::string colonSymbol = " : ";
  if (eyc) {
    colonSymbol = ": ";
  } else if (indentation.empty()) {
    colonSymbol = ":";
  }
  std::string nullSymbol = "null";
  if (dnp) {
    nullSymbol = "";
  }
  if (pre > 17) pre = 17;
  std::string endingLineFeedSymbol = "";
  return new BuiltStyledStreamWriter(
      indentation, cs,
      colonSymbol, nullSymbol, endingLineFeedSymbol, usf, pre);
}
static void getValidWriterKeys(std::set<std::string>* valid_keys)
{
  valid_keys->clear();
  valid_keys->insert("indentation");
  valid_keys->insert("commentStyle");
  valid_keys->insert("enableYAMLCompatibility");
  valid_keys->insert("dropNullPlaceholders");
  valid_keys->insert("useSpecialFloats");
  valid_keys->insert("precision");
}
bool StreamWriterBuilder::validate(Json::Value* invalid) const
{
  Json::Value my_invalid;
  if (!invalid) invalid = &my_invalid;  // so we do not need to test for NULL
  Json::Value& inv = *invalid;
  std::set<std::string> valid_keys;
  getValidWriterKeys(&valid_keys);
  Value::Members keys = settings_.getMemberNames();
  size_t n = keys.size();
  for (size_t i = 0; i < n; ++i) {
    std::string const& key = keys[i];
    if (valid_keys.find(key) == valid_keys.end()) {
      inv[key] = settings_[key];
    }
  }
  return 0u == inv.size();
}
Value& StreamWriterBuilder::operator[](std::string key)
{
  return settings_[key];
}
// static
void StreamWriterBuilder::setDefaults(Json::Value* settings)
{
  //! [StreamWriterBuilderDefaults]
  (*settings)["commentStyle"] = "All";
  (*settings)["indentation"] = "\t";
  (*settings)["enableYAMLCompatibility"] = false;
  (*settings)["dropNullPlaceholders"] = false;
  (*settings)["useSpecialFloats"] = false;
  (*settings)["precision"] = 17;
  //! [StreamWriterBuilderDefaults]
}

std::string writeString(StreamWriter::Factory const& builder, Value const& root) {
  std::ostringstream sout;
  StreamWriterPtr const writer(builder.newStreamWriter());
  writer->write(root, &sout);
  return sout.str();
}

std::ostream& operator<<(std::ostream& sout, Value const& root) {
  StreamWriterBuilder builder;
  StreamWriterPtr const writer(builder.newStreamWriter());
  writer->write(root, &sout);
  return sout;
}

} // namespace Json

// //////////////////////////////////////////////////////////////////////
// End of content of file: src/lib_json/json_writer.cpp
// //////////////////////////////////////////////////////////////////////






// //////////////////////////////////////////////////////////////////////
// End of content of file: arbiter/third/json/jsoncpp.cpp
// //////////////////////////////////////////////////////////////////////






// //////////////////////////////////////////////////////////////////////
// Beginning of content of file: arbiter/drivers/fs.cpp
// //////////////////////////////////////////////////////////////////////

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/arbiter.hpp>
#include <arbiter/drivers/fs.hpp>
#endif

#ifndef ARBITER_WINDOWS
#include <glob.h>
#include <sys/stat.h>
#else
#include <locale>
#include <codecvt>
#endif

#include <cstdlib>
#include <fstream>
#include <stdexcept>

namespace arbiter
{

namespace
{
    // Binary output, overwriting any existing file with a conflicting name.
    const std::ios_base::openmode binaryTruncMode(
            std::ofstream::binary |
            std::ofstream::out |
            std::ofstream::trunc);

    void noHome()
    {
        throw ArbiterError("No home directory found");
    }
}

namespace drivers
{

std::unique_ptr<Fs> Fs::create(HttpPool&, const Json::Value&)
{
    return std::unique_ptr<Fs>(new Fs());
}

bool Fs::get(std::string path, std::vector<char>& data) const
{
    bool good(false);

    path = fs::expandTilde(path);
    std::ifstream stream(path, std::ios::in | std::ios::binary);

    if (stream.good())
    {
        stream.seekg(0, std::ios::end);
        data.resize(static_cast<std::size_t>(stream.tellg()));
        stream.seekg(0, std::ios::beg);
        stream.read(data.data(), data.size());
        stream.close();
        good = true;
    }

    return good;
}

void Fs::put(std::string path, const std::vector<char>& data) const
{
    path = fs::expandTilde(path);
    std::ofstream stream(path, binaryTruncMode);

    if (!stream.good())
    {
        throw ArbiterError("Could not open " + path + " for writing");
    }

    stream.write(data.data(), data.size());

    if (!stream.good())
    {
        throw ArbiterError("Error occurred while writing " + path);
    }
}

std::vector<std::string> Fs::glob(std::string path, bool) const
{
    std::vector<std::string> results;

#ifndef ARBITER_WINDOWS
    path = fs::expandTilde(path);

    glob_t buffer;
    struct stat info;

    ::glob(path.c_str(), GLOB_NOSORT | GLOB_TILDE, 0, &buffer);

    for (std::size_t i(0); i < buffer.gl_pathc; ++i)
    {
        const std::string val(buffer.gl_pathv[i]);

        if (stat(val.c_str(), &info) == 0)
        {
            if (S_ISREG(info.st_mode))
            {
                results.push_back(val);
            }
        }
        else
        {
            throw ArbiterError("Error globbing - POSIX stat failed");
        }
    }

    globfree(&buffer);
#else
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    const std::wstring wide(converter.from_bytes(path));

    WIN32_FIND_DATA data;
    HANDLE hFind(FindFirstFile(wide.c_str(), &data));

    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
            {
                results.push_back(converter.to_bytes(data.cFileName));
            }
        }
        while (FindNextFile(hFind, &data));
    }
#endif

    return results;
}

} // namespace drivers

namespace fs
{

bool mkdirp(std::string dir)
{
    dir = expandTilde(dir);

#ifndef ARBITER_WINDOWS
    const bool err(::mkdir(dir.c_str(), S_IRWXU | S_IRGRP | S_IROTH));
    return (!err || errno == EEXIST);
#else
    throw ArbiterError("Windows mkdirp not done yet.");
#endif
}

bool remove(std::string filename)
{
    filename = expandTilde(filename);

#ifndef ARBITER_WINDOWS
    return ::remove(filename.c_str()) == 0;
#else
    throw ArbiterError("Windows remove not done yet.");
#endif
}

std::string expandTilde(std::string in)
{
    std::string out(in);

    if (!in.empty() && in.front() == '~')
    {
#ifndef ARBITER_WINDOWS
        if (!getenv("HOME"))
        {
            noHome();
        }

        static const std::string home(getenv("HOME"));
#else
        if (
                !getenv("USERPROFILE") &&
                !(getenv("HOMEDRIVE") && getenv("HOMEPATH")))
        {
            noHome();
        }

        static const std::string home(
                getenv("USERPROFILE") ? getenv("USERPROFILE") :
                    (getenv("HOMEDRIVE") + getenv("HOMEPATH"));
#endif

        out = home + in.substr(1);
    }

    return out;
}

LocalHandle::LocalHandle(const std::string localPath, const bool isRemote)
    : m_localPath(expandTilde(localPath))
    , m_isRemote(isRemote)
{ }

LocalHandle::~LocalHandle()
{
    if (m_isRemote) fs::remove(fs::expandTilde(m_localPath));
}

} // namespace fs
} // namespace arbiter


// //////////////////////////////////////////////////////////////////////
// End of content of file: arbiter/drivers/fs.cpp
// //////////////////////////////////////////////////////////////////////






// //////////////////////////////////////////////////////////////////////
// Beginning of content of file: arbiter/drivers/http.cpp
// //////////////////////////////////////////////////////////////////////

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/arbiter.hpp>
#include <arbiter/drivers/http.hpp>
#endif

#ifdef ARBITER_WINDOWS
#undef min
#undef max
#endif

#include <algorithm>
#include <cstring>
#include <iostream>

namespace
{
    struct PutData
    {
        PutData(const std::vector<char>& data)
            : data(data)
            , offset(0)
        { }

        const std::vector<char>& data;
        std::size_t offset;
    };

    std::size_t getCb(
            const char* in,
            std::size_t size,
            std::size_t num,
            std::vector<char>* out)
    {
        const std::size_t fullBytes(size * num);
        const std::size_t startSize(out->size());

        out->resize(out->size() + fullBytes);
        std::memcpy(out->data() + startSize, in, fullBytes);

        return fullBytes;
    }

    std::size_t putCb(
            char* out,
            std::size_t size,
            std::size_t num,
            PutData* in)
    {
        const std::size_t fullBytes(
                std::min(
                    size * num,
                    in->data.size() - in->offset));
        std::memcpy(out, in->data.data() + in->offset, fullBytes);

        in->offset += fullBytes;
        return fullBytes;
    }

    std::size_t headerCb(
            const char *buffer,
            std::size_t size,
            std::size_t num,
            arbiter::Headers* out)
    {
        const std::size_t fullBytes(size * num);

        std::string data(buffer, fullBytes);
        data.erase(std::remove(data.begin(), data.end(), '\n'), data.end());
        data.erase(std::remove(data.begin(), data.end(), '\r'), data.end());

        const std::size_t split(data.find_first_of(":"));

        // No colon means it isn't a header with data.
        if (split == std::string::npos) return fullBytes;

        const std::string key(data.substr(0, split));
        const std::string val(data.substr(split + 1, data.size()));

        (*out)[key] = val;

        return fullBytes;
    }

    size_t eatLogging(void *out, size_t size, size_t num, void *in)
    {
        return size * num;
    }

    const bool followRedirect(true);

    const auto baseSleepTime(std::chrono::milliseconds(1));
    const auto maxSleepTime (std::chrono::milliseconds(4096));

    const std::map<char, std::string> sanitizers
    {
        { ' ', "%20" },
        { '!', "%21" },
        { '"', "%22" },
        { '#', "%23" },
        { '$', "%24" },
        { '\'', "%27" },
        { '(', "%28" },
        { ')', "%29" },
        { '*', "%2A" },
        { '+', "%2B" },
        { ',', "%2C" },
        { ';', "%3B" },
        { '<', "%3C" },
        { '>', "%3E" },
        { '@', "%40" },
        { '[', "%5B" },
        { '\\', "%5C" },
        { ']', "%5D" },
        { '^', "%5E" },
        { '`', "%60" },
        { '{', "%7B" },
        { '|', "%7C" },
        { '}', "%7D" },
        { '~', "%7E" }
    };
}

namespace arbiter
{
namespace drivers
{

Http::Http(HttpPool& pool) : m_pool(pool) { }

std::unique_ptr<Http> Http::create(HttpPool& pool, const Json::Value&)
{
    return std::unique_ptr<Http>(new Http(pool));
}

bool Http::get(std::string path, std::vector<char>& data) const
{
    bool good(false);

    auto http(m_pool.acquire());
    HttpResponse res(http.get(path));

    if (res.ok())
    {
        data = res.data();
        good = true;
    }

    return good;
}

void Http::put(std::string path, const std::vector<char>& data) const
{
    auto http(m_pool.acquire());

    if (!http.put(path, data).ok())
    {
        throw ArbiterError("Couldn't HTTP PUT to " + path);
    }
}

std::string Http::sanitize(std::string path)
{
    std::string result;

    for (const auto c : path)
    {
        auto it(sanitizers.find(c));

        if (it == sanitizers.end())
        {
            result += c;
        }
        else
        {
            result += it->second;
        }
    }

    return result;
}

} // namespace drivers

Curl::Curl(bool verbose)
    : m_curl(0)
    , m_headers(0)
    , m_verbose(verbose)
    , m_data()
{
    m_curl = curl_easy_init();
}

Curl::~Curl()
{
    curl_easy_cleanup(m_curl);
    curl_slist_free_all(m_headers);
    m_headers = 0;
}

void Curl::init(std::string path, const Headers& headers)
{
    // Reset our curl instance and header list.
    curl_slist_free_all(m_headers);
    m_headers = 0;

    // Set path.
    curl_easy_setopt(m_curl, CURLOPT_URL, path.c_str());

    // Needed for multithreaded Curl usage.
    curl_easy_setopt(m_curl, CURLOPT_NOSIGNAL, 1L);

    // Substantially faster DNS lookups without IPv6.
    curl_easy_setopt(m_curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

    // Don't wait forever.
    curl_easy_setopt(m_curl, CURLOPT_TIMEOUT, 120);

    // Configuration options.
    if (followRedirect) curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, 1L);

    // Insert supplied headers.
    for (const auto& h : headers)
    {
        m_headers = curl_slist_append(
                m_headers,
                (h.first + ": " + h.second).c_str());
    }
}

HttpResponse Curl::get(std::string path, Headers headers)
{
    int httpCode(0);
    std::vector<char> data;

    if (m_verbose) curl_easy_setopt(m_curl, CURLOPT_VERBOSE, 1L);

    path = drivers::Http::sanitize(path);
    init(path, headers);

    // Register callback function and date pointer to consume the result.
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, getCb);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &data);

    // Insert all headers into the request.
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, m_headers);

    // Run the command.
    curl_easy_perform(m_curl);
    curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_easy_reset(m_curl);
    return HttpResponse(httpCode, data);
}

HttpResponse Curl::put(
        std::string path,
        const std::vector<char>& data,
        Headers headers)
{
    path = drivers::Http::sanitize(path);
    init(path, headers);

    if (m_verbose) curl_easy_setopt(m_curl, CURLOPT_VERBOSE, 1L);

    int httpCode(0);

    std::unique_ptr<PutData> putData(new PutData(data));

    // Register callback function and data pointer to create the request.
    curl_easy_setopt(m_curl, CURLOPT_READFUNCTION, putCb);
    curl_easy_setopt(m_curl, CURLOPT_READDATA, putData.get());

    // Insert all headers into the request.
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, m_headers);

    // Specify that this is a PUT request.
    curl_easy_setopt(m_curl, CURLOPT_PUT, 1L);

    // Must use this for binary data, otherwise curl will use strlen(), which
    // will likely be incorrect.
    curl_easy_setopt(
            m_curl,
            CURLOPT_INFILESIZE_LARGE,
            static_cast<curl_off_t>(data.size()));

    // Hide Curl's habit of printing things to console even with verbose set
    // to false.
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, eatLogging);

    // Run the command.
    curl_easy_perform(m_curl);
    curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_easy_reset(m_curl);
    return HttpResponse(httpCode);
}

HttpResponse Curl::post(
        std::string path,
        const std::vector<char>& data,
        Headers headers)
{
    path = drivers::Http::sanitize(path);
    init(path, headers);
    if (m_verbose) curl_easy_setopt(m_curl, CURLOPT_VERBOSE, 1L);

    int httpCode(0);

    std::unique_ptr<PutData> putData(new PutData(data));
    std::vector<char> writeData;

    // Register callback function and data pointer to create the request.
    curl_easy_setopt(m_curl, CURLOPT_READFUNCTION, putCb);
    curl_easy_setopt(m_curl, CURLOPT_READDATA, putData.get());

    // Register callback function and data pointer to consume the result.
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, getCb);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &writeData);

    // Insert all headers into the request.
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, m_headers);

    // Set up callback and data pointer for received headers.
    Headers receivedHeaders;
    curl_easy_setopt(m_curl, CURLOPT_HEADERFUNCTION, headerCb);
    curl_easy_setopt(m_curl, CURLOPT_HEADERDATA, &receivedHeaders);

    // Specify that this is a POST request.
    curl_easy_setopt(m_curl, CURLOPT_POST, 1L);

    // Must use this for binary data, otherwise curl will use strlen(), which
    // will likely be incorrect.
    curl_easy_setopt(
            m_curl,
            CURLOPT_INFILESIZE_LARGE,
            static_cast<curl_off_t>(data.size()));

    // Run the command.
    curl_easy_perform(m_curl);
    curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_easy_reset(m_curl);
    HttpResponse response(httpCode, writeData, receivedHeaders);
    return response;
}


///////////////////////////////////////////////////////////////////////////////

HttpResource::HttpResource(
        HttpPool& pool,
        Curl& curl,
        const std::size_t id,
        const std::size_t retry)
    : m_pool(pool)
    , m_curl(curl)
    , m_id(id)
    , m_retry(retry)
{ }

HttpResource::~HttpResource()
{
    m_pool.release(m_id);
}

HttpResponse HttpResource::get(
        const std::string path,
        const Headers headers)
{
    auto f([this, path, headers]()->HttpResponse
    {
        return m_curl.get(path, headers);
    });

    return exec(f);
}

HttpResponse HttpResource::put(
        std::string path,
        const std::vector<char>& data,
        Headers headers)
{
    auto f([this, path, &data, headers]()->HttpResponse
    {
        return m_curl.put(path, data, headers);
    });

    return exec(f);
}

HttpResponse HttpResource::post(
        std::string path,
        const std::vector<char>& data,
        Headers headers)
{
    auto f([this, path, &data, headers]()->HttpResponse
    {
        return m_curl.post(path, data, headers);
    });

    return exec(f);
}

HttpResponse HttpResource::exec(std::function<HttpResponse()> f)
{
    HttpResponse res;
    std::size_t tries(0);

    do
    {
        res = f();
    }
    while (res.serverError() && tries++ < m_retry);

    return res;
}

///////////////////////////////////////////////////////////////////////////////

HttpPool::HttpPool(std::size_t concurrent, std::size_t retry, bool verbose)
    : m_curls(concurrent)
    , m_available(concurrent)
    , m_retry(retry)
    , m_mutex()
    , m_cv()
{
    for (std::size_t i(0); i < concurrent; ++i)
    {
        m_available[i] = i;
        m_curls[i].reset(new Curl(verbose));
    }
}

HttpResource HttpPool::acquire()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this]()->bool { return !m_available.empty(); });

    const std::size_t id(m_available.back());
    Curl& curl(*m_curls[id]);

    m_available.pop_back();

    return HttpResource(*this, curl, id, m_retry);
}

void HttpPool::release(const std::size_t id)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_available.push_back(id);
    lock.unlock();

    m_cv.notify_one();
}

} // namespace arbiter


// //////////////////////////////////////////////////////////////////////
// End of content of file: arbiter/drivers/http.cpp
// //////////////////////////////////////////////////////////////////////






// //////////////////////////////////////////////////////////////////////
// Beginning of content of file: arbiter/drivers/s3.cpp
// //////////////////////////////////////////////////////////////////////

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/arbiter.hpp>
#include <arbiter/drivers/s3.hpp>
#endif

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <thread>

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/arbiter.hpp>
#include <arbiter/drivers/fs.hpp>
#include <arbiter/third/xml/xml.hpp>
#include <arbiter/util/crypto.hpp>
#endif

namespace arbiter
{

namespace
{
    const std::string baseUrl(".s3.amazonaws.com/");

    std::string getQueryString(const Query& query)
    {
        std::string result;

        bool first(true);
        for (const auto& q : query)
        {
            result += (first ? "?" : "&") + q.first + "=" + q.second;
            first = false;
        }

        return result;
    }

    struct Resource
    {
        Resource(std::string fullPath)
        {
            const std::size_t split(fullPath.find("/"));

            bucket = fullPath.substr(0, split);

            if (split != std::string::npos)
            {
                object = fullPath.substr(split + 1);
            }
        }

        std::string buildPath(Query query = Query()) const
        {
            const std::string queryString(getQueryString(query));
            return "http://" + bucket + baseUrl + object + queryString;
        }

        std::string bucket;
        std::string object;
    };

    typedef Xml::xml_node<> XmlNode;

    const std::string badResponse("Unexpected contents in AWS response");
}

namespace drivers
{

AwsAuth::AwsAuth(const std::string access, const std::string hidden)
    : m_access(access)
    , m_hidden(hidden)
{ }

std::unique_ptr<AwsAuth> AwsAuth::find(std::string user)
{
    std::unique_ptr<AwsAuth> auth;

    if (user.empty())
    {
        user = getenv("AWS_PROFILE") ? getenv("AWS_PROFILE") : "default";
    }

    drivers::Fs fs;
    std::unique_ptr<std::string> file(fs.tryGet("~/.aws/credentials"));

    // First, try reading credentials file.
    if (file)
    {
        std::size_t index(0);
        std::size_t pos(0);
        std::vector<std::string> lines;

        do
        {
            index = file->find('\n', pos);
            std::string line(file->substr(pos, index - pos));

            line.erase(
                    std::remove_if(line.begin(), line.end(), ::isspace),
                    line.end());

            lines.push_back(line);

            pos = index + 1;
        }
        while (index != std::string::npos);

        if (lines.size() >= 3)
        {
            std::size_t i(0);

            const std::string userFind("[" + user + "]");
            const std::string accessFind("aws_access_key_id=");
            const std::string hiddenFind("aws_secret_access_key=");

            while (i < lines.size() - 2 && !auth)
            {
                if (lines[i].find(userFind) != std::string::npos)
                {
                    const std::string& accessLine(lines[i + 1]);
                    const std::string& hiddenLine(lines[i + 2]);

                    std::size_t accessPos(accessLine.find(accessFind));
                    std::size_t hiddenPos(hiddenLine.find(hiddenFind));

                    if (
                            accessPos != std::string::npos &&
                            hiddenPos != std::string::npos)
                    {
                        const std::string access(
                                accessLine.substr(
                                    accessPos + accessFind.size(),
                                    accessLine.find(';')));

                        const std::string hidden(
                                hiddenLine.substr(
                                    hiddenPos + hiddenFind.size(),
                                    hiddenLine.find(';')));

                        auth.reset(new AwsAuth(access, hidden));
                    }
                }

                ++i;
            }
        }
    }

    // Fall back to environment settings.
    if (!auth)
    {
        if (getenv("AWS_ACCESS_KEY_ID") && getenv("AWS_SECRET_ACCESS_KEY"))
        {
            auth.reset(
                    new AwsAuth(
                        getenv("AWS_ACCESS_KEY_ID"),
                        getenv("AWS_SECRET_ACCESS_KEY")));
        }
        else if (
                getenv("AMAZON_ACCESS_KEY_ID") &&
                getenv("AMAZON_SECRET_ACCESS_KEY"))
        {
            auth.reset(
                    new AwsAuth(
                        getenv("AMAZON_ACCESS_KEY_ID"),
                        getenv("AMAZON_SECRET_ACCESS_KEY")));
        }
    }

    return auth;
}

std::string AwsAuth::access() const
{
    return m_access;
}

std::string AwsAuth::hidden() const
{
    return m_hidden;
}

S3::S3(HttpPool& pool, const AwsAuth auth)
    : m_pool(pool)
    , m_auth(auth)
{ }

std::unique_ptr<S3> S3::create(HttpPool& pool, const Json::Value& json)
{
    std::unique_ptr<S3> s3;

    if (!json.isNull() && json.isMember("access") & json.isMember("hidden"))
    {
        AwsAuth auth(json["access"].asString(), json["hidden"].asString());
        s3.reset(new S3(pool, auth));
    }
    else
    {
        auto auth(AwsAuth::find(json.isNull() ? "" : json["user"].asString()));
        if (auth) s3.reset(new S3(pool, *auth));
    }

    return s3;
}

bool S3::get(std::string rawPath, std::vector<char>& data) const
{
    return buildRequestAndGet(rawPath, Query(), data);
}

bool S3::buildRequestAndGet(
        std::string rawPath,
        const Query& query,
        std::vector<char>& data,
        const Headers userHeaders) const
{
    rawPath = Http::sanitize(rawPath);
    const Resource resource(rawPath);

    const std::string path(resource.buildPath(query));

    Headers headers(httpGetHeaders(rawPath));
    for (const auto& h : userHeaders) headers[h.first] = h.second;

    auto http(m_pool.acquire());

    HttpResponse res(http.get(path, headers));

    if (res.ok())
    {
        data = res.data();
        return true;
    }
    else
    {
        return false;
    }
}

void S3::put(std::string rawPath, const std::vector<char>& data) const
{
    const Resource resource(rawPath);

    const std::string path(resource.buildPath());
    const Headers headers(httpPutHeaders(rawPath));

    auto http(m_pool.acquire());

    if (!http.put(path, data, headers).ok())
    {
        throw ArbiterError("Couldn't S3 PUT to " + rawPath);
    }
}

std::vector<std::string> S3::glob(std::string path, bool verbose) const
{
    std::vector<std::string> results;
    path.pop_back();

    // https://docs.aws.amazon.com/AmazonS3/latest/API/RESTBucketGET.html
    const Resource resource(path);
    const std::string& bucket(resource.bucket);
    const std::string& object(resource.object);
    const std::string prefix(resource.object.empty() ? "" : resource.object);

    Query query;

    if (prefix.size()) query["prefix"] = prefix;

    bool more(false);
    std::vector<char> data;

    do
    {
        if (verbose) std::cout << "." << std::flush;

        if (!buildRequestAndGet(resource.bucket + "/", query, data))
        {
            throw ArbiterError("Couldn't S3 GET " + resource.bucket);
        }

        data.push_back('\0');

        Xml::xml_document<> xml;

        try
        {
            xml.parse<0>(data.data());
        }
        catch (Xml::parse_error)
        {
            throw ArbiterError("Could not parse S3 response.");
        }

        if (XmlNode* topNode = xml.first_node("ListBucketResult"))
        {
            if (XmlNode* truncNode = topNode->first_node("IsTruncated"))
            {
                std::string t(truncNode->value());
                std::transform(t.begin(), t.end(), t.begin(), tolower);

                more = (t == "true");
            }

            if (XmlNode* conNode = topNode->first_node("Contents"))
            {
                for ( ; conNode; conNode = conNode->next_sibling())
                {
                    if (XmlNode* keyNode = conNode->first_node("Key"))
                    {
                        std::string key(keyNode->value());

                        // The prefix may contain slashes (i.e. is a sub-dir)
                        // but we only include the top level after that.
                        if (key.find('/', prefix.size()) == std::string::npos)
                        {
                            results.push_back("s3://" + bucket + "/" + key);

                            if (more)
                            {
                                query["marker"] =
                                    object + key.substr(prefix.size());
                            }
                        }
                    }
                    else
                    {
                        throw ArbiterError(badResponse);
                    }
                }
            }
            else
            {
                throw ArbiterError(badResponse);
            }
        }
        else
        {
            throw ArbiterError(badResponse);
        }

        xml.clear();
    }
    while (more);

    return results;
}

Headers S3::httpGetHeaders(std::string filePath) const
{
    const std::string httpDate(getHttpDate());
    const std::string signedEncoded(
            getSignedEncodedString(
                "GET",
                filePath,
                httpDate));

    Headers headers;

    headers["Date"] = httpDate;
    headers["Authorization"] = "AWS " + m_auth.access() + ":" + signedEncoded;

    return headers;
}

Headers S3::httpPutHeaders(std::string filePath) const
{
    const std::string httpDate(getHttpDate());
    const std::string signedEncoded(
            getSignedEncodedString(
                "PUT",
                filePath,
                httpDate,
                "application/octet-stream"));

    Headers headers;

    headers["Content-Type"] = "application/octet-stream";
    headers["Date"] = httpDate;
    headers["Authorization"] = "AWS " + m_auth.access() + ":" + signedEncoded;
    headers["Transfer-Encoding"] = "";
    headers["Expect"] = "";

    return headers;
}

std::string S3::getHttpDate() const
{
    time_t rawTime;
    char charBuf[80];

    time(&rawTime);

#ifndef ARBITER_WINDOWS
    tm* timeInfoPtr = localtime(&rawTime);
#else
    tm timeInfo;
    localtime_s(&timeInfo, &rawTime);
    tm* timeInfoPtr(&timeInfo);
#endif

    strftime(charBuf, 80, "%a, %d %b %Y %H:%M:%S %z", timeInfoPtr);
    std::string stringBuf(charBuf);

    return stringBuf;
}

std::string S3::getSignedEncodedString(
        std::string command,
        std::string file,
        std::string httpDate,
        std::string contentType) const
{
    const std::string toSign(getStringToSign(
                command,
                file,
                httpDate,
                contentType));

    const std::vector<char> signedData(signString(toSign));
    return encodeBase64(signedData);
}

std::string S3::getStringToSign(
        std::string command,
        std::string file,
        std::string httpDate,
        std::string contentType) const
{
    return
        command + "\n" +
        "\n" +
        contentType + "\n" +
        httpDate + "\n" +
        "/" + file;
}

std::vector<char> S3::signString(std::string input) const
{
    return crypto::hmacSha1(m_auth.hidden(), input);
}

std::string S3::encodeBase64(std::vector<char> data) const
{
    std::vector<uint8_t> input;
    for (std::size_t i(0); i < data.size(); ++i)
    {
        char c(data[i]);
        input.push_back(*reinterpret_cast<uint8_t*>(&c));
    }

    const std::string vals(
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");

    std::size_t fullSteps(input.size() / 3);
    while (input.size() % 3) input.push_back(0);
    uint8_t* pos(input.data());
    uint8_t* end(input.data() + fullSteps * 3);

    std::string output(fullSteps * 4, '_');
    std::size_t outIndex(0);

    const uint32_t mask(0x3F);

    while (pos != end)
    {
        uint32_t chunk((*pos) << 16 | *(pos + 1) << 8 | *(pos + 2));

        output[outIndex++] = vals[(chunk >> 18) & mask];
        output[outIndex++] = vals[(chunk >> 12) & mask];
        output[outIndex++] = vals[(chunk >>  6) & mask];
        output[outIndex++] = vals[chunk & mask];

        pos += 3;
    }

    if (end != input.data() + input.size())
    {
        const std::size_t num(pos - end == 1 ? 2 : 3);
        uint32_t chunk(*(pos) << 16 | *(pos + 1) << 8 | *(pos + 2));

        output.push_back(vals[(chunk >> 18) & mask]);
        output.push_back(vals[(chunk >> 12) & mask]);
        if (num == 3) output.push_back(vals[(chunk >> 6) & mask]);
    }

    while (output.size() % 4) output.push_back('=');

    return output;
}



// These functions allow a caller to directly pass additional headers into
// their GET request.  This is only applicable when using the S3 driver
// directly, as these are not available through the Arbiter.

std::vector<char> S3::getBinary(std::string rawPath, Headers headers) const
{
    std::vector<char> data;
    const std::string stripped(Arbiter::stripType(rawPath));

    if (!buildRequestAndGet(stripped, Query(), data, headers))
    {
        throw ArbiterError("Couldn't S3 GET " + rawPath);
    }

    return data;
}

std::string S3::get(std::string rawPath, Headers headers) const
{
    std::vector<char> data(getBinary(rawPath, headers));
    return std::string(data.begin(), data.end());
}

} // namespace drivers
} // namespace arbiter


// //////////////////////////////////////////////////////////////////////
// End of content of file: arbiter/drivers/s3.cpp
// //////////////////////////////////////////////////////////////////////






// //////////////////////////////////////////////////////////////////////
// Beginning of content of file: arbiter/drivers/dropbox.cpp
// //////////////////////////////////////////////////////////////////////

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/arbiter.hpp>
#endif

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <functional>

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/arbiter.hpp>
#include <arbiter/drivers/fs.hpp>
#include <arbiter/drivers/dropbox.hpp>
#include <arbiter/third/xml/xml.hpp>
#include <arbiter/util/crypto.hpp>
#include <arbiter/third/json/json.hpp>
#endif

namespace arbiter
{

namespace
{
    const std::string getUrl("https://content.dropboxapi.com/2/files/download");
    const std::string listUrl("https://api.dropboxapi.com/2/files/list_folder");
    const std::string continueListUrl(listUrl + "/continue");

    const auto ins([](unsigned char lhs, unsigned char rhs)
    {
        return std::tolower(lhs) == std::tolower(rhs);
    });

    std::string toSanitizedString(const Json::Value& v)
    {
        Json::FastWriter writer;
        std::string f(writer.write(v));
        f.erase(std::remove(f.begin(), f.end(), '\n'), f.end());
        return f;
    }

    const std::string dirTag("folder");
    const std::string fileTag("file");
}

namespace drivers
{

Dropbox::Dropbox(HttpPool& pool, const DropboxAuth auth)
    : m_pool(pool)
    , m_auth(auth)
{ }

std::unique_ptr<Dropbox> Dropbox::create(
        HttpPool& pool,
        const Json::Value& json)
{
    std::unique_ptr<Dropbox> dropbox;

    if (!json.isNull() && json.isMember("token"))
    {
        dropbox.reset(new Dropbox(pool, DropboxAuth(json["token"].asString())));
    }

    return dropbox;
}

Headers Dropbox::httpGetHeaders(const std::string contentType) const
{
    Headers headers;

    headers["Authorization"] = "Bearer " + m_auth.token();
    headers["Transfer-Encoding"] = "chunked";
    headers["Expect"] = "100-continue";
    headers["Content-Type"] = contentType;

    return headers;
}

bool Dropbox::get(const std::string rawPath, std::vector<char>& data) const
{
    const std::string path(Http::sanitize(rawPath));
    Headers headers(httpGetHeaders());
    auto http(m_pool.acquire());

    Json::Value json;
    json["path"] = std::string("/" + path);
    headers["Dropbox-API-Arg"] = toSanitizedString(json);

    std::vector<char> postData;
    HttpResponse res(http.post(getUrl, postData, headers));

    if (res.ok())
    {
        if (!res.headers().count("original-content-length")) return false;

        data = res.data();

        const std::size_t size(
                std::stol(res.headers().at("original-content-length")));

        if (size == res.data().size()) return true;
        else throw ArbiterError("Data size check failed");
    }
    else
    {
        std::string message(res.data().data(), res.data().size());
        throw ArbiterError("Server responded with '" + message + "'");
    }

    return false;
}

void Dropbox::put(std::string rawPath, const std::vector<char>& data) const
{
    throw ArbiterError("PUT not yet supported for " + type());
}

std::string Dropbox::continueFileInfo(std::string cursor) const
{
    Headers headers(httpGetHeaders("application/json"));

    auto http(m_pool.acquire());

    Json::Value json;
    json["cursor"] = cursor;
    std::string f = toSanitizedString(json);

    std::vector<char> postData(f.begin(), f.end());
    HttpResponse res(http.post(continueListUrl, postData, headers));

    if (res.ok())
    {
        return std::string(res.data().data(), res.data().size());
    }
    else
    {
        std::string message(res.data().data(), res.data().size());
        throw ArbiterError("Server responded with '" + message + "'");
    }

    return std::string("");
}

std::vector<std::string> Dropbox::glob(std::string rawPath, bool verbose) const
{
    std::vector<std::string> results;

    const std::string path(
            Http::sanitize(rawPath.substr(0, rawPath.size() - 2)));

    auto listPath = [this](std::string path)->std::string
    {
        auto http(m_pool.acquire());
        Headers headers(httpGetHeaders("application/json"));

        Json::Value request;
        request["path"] = std::string("/" + path);
        request["recursive"] = false;
        request["include_media_info"] = false;
        request["include_deleted"] = false;

        std::string f = toSanitizedString(request);

        std::vector<char> postData(f.begin(), f.end());
        HttpResponse res(http.post(listUrl, postData, headers));

        if (res.ok())
        {
            return std::string(res.data().data(), res.data().size());
        }
        else if (res.code() == 409)
        {
            return "";
        }
        else
        {
            std::string message(res.data().data(), res.data().size());
            throw ArbiterError("Server responded with '" + message + "'");
        }
    };

    bool more(false);
    std::string cursor("");

    auto processPath = [verbose, &results, &more, &cursor](std::string data)
    {
        if (data.empty()) return;

        if (verbose) std::cout << '.';

        Json::Value json;
        Json::Reader reader;
        reader.parse(data, json, false);

        const Json::Value& entries(json["entries"]);

        if (entries.isNull())
        {
            throw ArbiterError("Returned JSON from Dropbox was NULL");
        }
        if (!entries.isArray())
        {
            throw ArbiterError("Returned JSON from Dropbox was not an array");
        }

        more = json["has_more"].asBool();
        cursor = json["cursor"].asString();

        for (std::size_t i(0); i < entries.size(); ++i)
        {
            const Json::Value& v(entries[static_cast<Json::ArrayIndex>(i)]);
            const std::string tag(v[".tag"].asString());

            // Only insert files.
            if (std::equal(tag.begin(), tag.end(), fileTag.begin(), ins))
            {
                // Results already begin with a slash.
                results.push_back("dropbox:/" + v["path_lower"].asString());
            }
        }
    };

    processPath(listPath(path));

    if (more)
    {
        do
        {
            processPath(continueFileInfo(cursor));
        }
        while (more);
    }

    return results;
}

} // namespace drivers
} // namespace arbiter


// //////////////////////////////////////////////////////////////////////
// End of content of file: arbiter/drivers/dropbox.cpp
// //////////////////////////////////////////////////////////////////////






// //////////////////////////////////////////////////////////////////////
// Beginning of content of file: arbiter/util/crypto.cpp
// //////////////////////////////////////////////////////////////////////

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/util/crypto.hpp>
#endif

#include <cstdint>

#define ROTLEFT(a, b) ((a << b) | (a >> (32 - b)))

namespace arbiter
{
namespace crypto
{
namespace
{
    const std::size_t block(64);

    std::vector<char> append(
            const std::vector<char>& a,
            const std::vector<char>& b)
    {
        std::vector<char> out(a);
        out.insert(out.end(), b.begin(), b.end());
        return out;
    }

    std::vector<char> append(
            const std::vector<char>& a,
            const std::string& b)
    {
        return append(a, std::vector<char>(b.begin(), b.end()));
    }

    // SHA1 implementation:
    //      https://github.com/B-Con/crypto-algorithms
    //
    // HMAC:
    //      https://en.wikipedia.org/wiki/Hash-based_message_authentication_code
    //
    typedef struct
    {
        uint8_t data[64];
        uint32_t datalen;
        unsigned long long bitlen;
        uint32_t state[5];
        uint32_t k[4];
    } SHA1_CTX;

    void sha1_transform(SHA1_CTX *ctx, const uint8_t* data)
    {
        uint32_t a, b, c, d, e, i, j, t, m[80];

        for (i = 0, j = 0; i < 16; ++i, j += 4)
        {
            m[i] =
                (data[j] << 24) + (data[j + 1] << 16) +
                (data[j + 2] << 8) + (data[j + 3]);
        }

        for ( ; i < 80; ++i)
        {
            m[i] = (m[i - 3] ^ m[i - 8] ^ m[i - 14] ^ m[i - 16]);
            m[i] = (m[i] << 1) | (m[i] >> 31);
        }

        a = ctx->state[0];
        b = ctx->state[1];
        c = ctx->state[2];
        d = ctx->state[3];
        e = ctx->state[4];

        for (i = 0; i < 20; ++i) {
            t = ROTLEFT(a, 5) + ((b & c) ^ (~b & d)) + e + ctx->k[0] + m[i];
            e = d;
            d = c;
            c = ROTLEFT(b, 30);
            b = a;
            a = t;
        }
        for ( ; i < 40; ++i) {
            t = ROTLEFT(a, 5) + (b ^ c ^ d) + e + ctx->k[1] + m[i];
            e = d;
            d = c;
            c = ROTLEFT(b, 30);
            b = a;
            a = t;
        }
        for ( ; i < 60; ++i) {
            t = ROTLEFT(a, 5) + ((b & c) ^ (b & d) ^ (c & d)) + e +
                ctx->k[2] + m[i];
            e = d;
            d = c;
            c = ROTLEFT(b, 30);
            b = a;
            a = t;
        }
        for ( ; i < 80; ++i) {
            t = ROTLEFT(a, 5) + (b ^ c ^ d) + e + ctx->k[3] + m[i];
            e = d;
            d = c;
            c = ROTLEFT(b, 30);
            b = a;
            a = t;
        }

        ctx->state[0] += a;
        ctx->state[1] += b;
        ctx->state[2] += c;
        ctx->state[3] += d;
        ctx->state[4] += e;
    }

    void sha1_init(SHA1_CTX *ctx)
    {
        ctx->datalen = 0;
        ctx->bitlen = 0;
        ctx->state[0] = 0x67452301;
        ctx->state[1] = 0xEFCDAB89;
        ctx->state[2] = 0x98BADCFE;
        ctx->state[3] = 0x10325476;
        ctx->state[4] = 0xc3d2e1f0;
        ctx->k[0] = 0x5a827999;
        ctx->k[1] = 0x6ed9eba1;
        ctx->k[2] = 0x8f1bbcdc;
        ctx->k[3] = 0xca62c1d6;
    }

    void sha1_update(SHA1_CTX *ctx, const uint8_t* data, size_t len)
    {
        for (std::size_t i(0); i < len; ++i)
        {
            ctx->data[ctx->datalen] = data[i];
            ++ctx->datalen;
            if (ctx->datalen == 64)
            {
                sha1_transform(ctx, ctx->data);
                ctx->bitlen += 512;
                ctx->datalen = 0;
            }
        }
    }

    void sha1_final(SHA1_CTX *ctx, uint8_t* hash)
    {
        uint32_t i;

        i = ctx->datalen;

        // Pad whatever data is left in the buffer.
        if (ctx->datalen < 56)
        {
            ctx->data[i++] = 0x80;
            while (i < 56)
                ctx->data[i++] = 0x00;
        }
        else
        {
            ctx->data[i++] = 0x80;
            while (i < 64)
                ctx->data[i++] = 0x00;
            sha1_transform(ctx, ctx->data);
            std::memset(ctx->data, 0, 56);
        }

        // Append to the padding the total message's length in bits and
        // transform.
        ctx->bitlen += ctx->datalen * 8;
        ctx->data[63] = static_cast<uint8_t>(ctx->bitlen);
        ctx->data[62] = static_cast<uint8_t>(ctx->bitlen >> 8);
        ctx->data[61] = static_cast<uint8_t>(ctx->bitlen >> 16);
        ctx->data[60] = static_cast<uint8_t>(ctx->bitlen >> 24);
        ctx->data[59] = static_cast<uint8_t>(ctx->bitlen >> 32);
        ctx->data[58] = static_cast<uint8_t>(ctx->bitlen >> 40);
        ctx->data[57] = static_cast<uint8_t>(ctx->bitlen >> 48);
        ctx->data[56] = static_cast<uint8_t>(ctx->bitlen >> 56);
        sha1_transform(ctx, ctx->data);

        // Since this implementation uses little endian byte ordering and MD
        // uses big endian, reverse all the bytes when copying the final state
        // to the output hash.
        for (i = 0; i < 4; ++i)
        {
            hash[i]      = (ctx->state[0] >> (24 - i * 8)) & 0x000000ff;
            hash[i + 4]  = (ctx->state[1] >> (24 - i * 8)) & 0x000000ff;
            hash[i + 8]  = (ctx->state[2] >> (24 - i * 8)) & 0x000000ff;
            hash[i + 12] = (ctx->state[3] >> (24 - i * 8)) & 0x000000ff;
            hash[i + 16] = (ctx->state[4] >> (24 - i * 8)) & 0x000000ff;
        }
    }

    std::vector<char> sha1(const std::vector<char>& data)
    {
        SHA1_CTX ctx;
        std::vector<char> out(20);

        sha1_init(&ctx);
        sha1_update(
                &ctx,
                reinterpret_cast<const uint8_t*>(data.data()),
                data.size());
        sha1_final(&ctx, reinterpret_cast<uint8_t*>(out.data()));

        return out;
    }

    std::string sha1(const std::string& data)
    {
        auto hashed(sha1(std::vector<char>(data.begin(), data.end())));
        return std::string(hashed.begin(), hashed.end());
    }

} // unnamed namespace

std::vector<char> hmacSha1(std::string key, const std::string message)
{
    if (key.size() > block) key = sha1(key);
    if (key.size() < block) key.insert(key.end(), block - key.size(), 0);

    std::vector<char> okeypad(block, 0x5c);
    std::vector<char> ikeypad(block, 0x36);

    for (std::size_t i(0); i < block; ++i)
    {
        okeypad[i] ^= key[i];
        ikeypad[i] ^= key[i];
    }

    return sha1(append(okeypad, sha1(append(ikeypad, message))));
}

} // namespace crypto
} // namespace arbiter

// //////////////////////////////////////////////////////////////////////
// End of content of file: arbiter/util/crypto.cpp
// //////////////////////////////////////////////////////////////////////





