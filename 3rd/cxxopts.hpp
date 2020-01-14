/*

Copyright (c) 2014, 2015, 2016, 2017 Jarryd Beck

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*/

#ifndef CXXOPTS_HPP_INCLUDED
#define CXXOPTS_HPP_INCLUDED

#include <cstring>
#include <cctype>
#include <exception>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "../utils.hpp"

#ifdef __cpp_lib_optional
#include <optional>
#define CXXOPTS_HAS_OPTIONAL
#endif

#ifndef CXXOPTS_VECTOR_DELIMITER
#define CXXOPTS_VECTOR_DELIMITER ','
#endif

#define CXXOPTS__VERSION_MAJOR 2
#define CXXOPTS__VERSION_MINOR 2
#define CXXOPTS__VERSION_PATCH 0

namespace cxxopts {
}

//when we ask cxxopts to use Unicode, help strings are processed using ICU,
//which results in the correct lengths being computed for strings when they
//are formatted for the help output
//it is necessary to make sure that <unicode/unistr.h> can be found by the
//compiler, and that icu-uc is linked in to the binary.

#ifdef CXXOPTS_USE_UNICODE
#include <unicode/unistr.h>

namespace cxxopts
{
  typedef icu::UnicodeString String;

  inline
  String
  toLocalString(std::string s)
  {
    return icu::UnicodeString::fromUTF8(std::move(s));
  }

  class UnicodeStringIterator : public
    std::iterator<std::forward_iterator_tag, int32_t>
  {
    public:

    UnicodeStringIterator(const icu::UnicodeString* string, int32_t pos)
    : s(string)
    , i(pos)
    {
    }

    value_type
    operator*() const
    {
      return s->char32At(i);
    }

    bool
    operator==(const UnicodeStringIterator& rhs) const
    {
      return s == rhs.s && i == rhs.i;
    }

    bool
    operator!=(const UnicodeStringIterator& rhs) const
    {
      return !(*this == rhs);
    }

    UnicodeStringIterator&
    operator++()
    {
      ++i;
      return *this;
    }

    UnicodeStringIterator
    operator+(int32_t v)
    {
      return UnicodeStringIterator(s, i + v);
    }

    private:
    const icu::UnicodeString* s;
    int32_t i;
  };

  inline
  String&
  stringAppend(String&s, String a)
  {
    return s.append(std::move(a));
  }

  inline
  String&
  stringAppend(String& s, int n, UChar32 c)
  {
    for (int i = 0; i != n; ++i)
    {
      s.append(c);
    }

    return s;
  }

  template <typename Iterator>
  String&
  stringAppend(String& s, Iterator begin, Iterator end)
  {
    while (begin != end)
    {
      s.append(*begin);
      ++begin;
    }

    return s;
  }

  inline
  size_t
  stringLength(const String& s)
  {
    return s.length();
  }

  inline
  std::string
  toUTF8String(const String& s)
  {
    std::string result;
    s.toUTF8String(result);

    return result;
  }

  inline
  bool
  empty(const String& s)
  {
    return s.isEmpty();
  }
}

namespace std
{
  inline
  cxxopts::UnicodeStringIterator
  begin(const icu::UnicodeString& s)
  {
    return cxxopts::UnicodeStringIterator(&s, 0);
  }

  inline
  cxxopts::UnicodeStringIterator
  end(const icu::UnicodeString& s)
  {
    return cxxopts::UnicodeStringIterator(&s, s.length());
  }
}

//ifdef CXXOPTS_USE_UNICODE
#else

namespace cxxopts {
    typedef std::string String;

    template<typename T>
    T toLocalString(T &&t) {
      return std::forward<T>(t);
    }

    inline size_t stringLength(const String &s) {
      return s.length();
    }

    inline String &stringAppend(String &s, String a) {
      return s.append(std::move(a));
    }

    inline String &stringAppend(String &s, size_t n, char c) {
      return s.append(n, c);
    }

    template<typename Iterator>
    String &stringAppend(String &s, Iterator begin, Iterator end) {
      return s.append(begin, end);
    }

    template<typename T>
    std::string toUTF8String(T &&t) {
      return std::forward<T>(t);
    }

    inline bool empty(const std::string &s) {
      return s.empty();
    }
}

//ifdef CXXOPTS_USE_UNICODE
#endif

namespace cxxopts {
    namespace {
#ifdef _WIN32
        const std::string LQUOTE("\'");
        const std::string RQUOTE("\'");
#else
        const std::string LQUOTE("‘");
        const std::string RQUOTE("’");
#endif
    }

    class Value : public std::enable_shared_from_this<Value> {
    public:

        virtual ~Value() = default;

        virtual std::shared_ptr<Value> clone() const = 0;

        virtual void parse(const std::string &text) const = 0;

        virtual void parse() const = 0;

        virtual bool has_default() const = 0;

        virtual bool is_container() const = 0;

        virtual bool has_implicit() const = 0;

        virtual std::string get_default_value() const = 0;

        virtual std::string get_implicit_value() const = 0;

        virtual std::shared_ptr<Value> default_value(const std::string &value) = 0;

        virtual std::shared_ptr<Value> implicit_value(const std::string &value) = 0;

        virtual std::shared_ptr<Value> no_implicit_value() = 0;

        virtual bool is_boolean() const = 0;
    };

    class OptionException : public std::exception {
    public:
        OptionException(std::string message);

        virtual const char *what() const noexcept;

    private:
        std::string m_message;
    };

    class OptionSpecException : public OptionException {
    public:
        OptionSpecException(const std::string &message);
    };

    class OptionParseException : public OptionException {
    public:
        OptionParseException(const std::string &message);
    };

    class OptionExistsError : public OptionSpecException {
    public:
        OptionExistsError(const std::string &option);
    };

    class InvalidOptionFormatError : public OptionSpecException {
    public:
        InvalidOptionFormatError(const std::string &format);
    };

    class OptionSyntaxException : public OptionParseException {
    public:
        OptionSyntaxException(const std::string &text);
    };

    class OptionNotExistsException : public OptionParseException {
    public:
        OptionNotExistsException(const std::string &option);
    };

    class MissingArgumentException : public OptionParseException {
    public:
        MissingArgumentException(const std::string &option);
    };

    class OptionRequiresArgumentException : public OptionParseException {
    public:
        OptionRequiresArgumentException(const std::string &option);
    };

    class OptionNotPresentException : public OptionParseException {
    public:
        OptionNotPresentException(const std::string &option);
    };

    class ArgumentIncorrectType : public OptionParseException {
    public:
        ArgumentIncorrectType(const std::string &arg);
    };

    template<typename T>
    void throwOrMimic(const std::string &text) {
      static_assert(std::is_base_of<std::exception, T>::value, "throwOrMimic only works on std::exception and "
                                                               "deriving classes");

#ifndef CXXOPTS_NO_EXCEPTIONS
      // If CXXOPTS_NO_EXCEPTIONS is not defined, just throw
      throw T {text};
#else
      // Otherwise manually instantiate the exception, print what() to stderr,
      // and abort
      T exception{text};
      std::cerr << exception.what() << std::endl;
      std::cerr << "Aborting (exceptions disabled)..." << std::endl;
      std::abort();
#endif
    }

    namespace values {
        namespace {
            std::basic_regex<char> integer_pattern("(-)?(0x)?([0-9a-zA-Z]+)|((0x)?0)");
            std::basic_regex<char> truthy_pattern("(t|T)(rue)?|1");
            std::basic_regex<char> falsy_pattern("(f|F)(alse)?|0");
        }

        namespace detail {
            template<typename T, bool B>
            struct SignedCheck;

            template<typename T>
            struct SignedCheck<T, true> {
                template<typename U>
                void operator()(bool negative, U u, const std::string &text) {
                  if( negative ) {
                    if( u > static_cast<U>((std::numeric_limits<T>::min)())) {
                      throwOrMimic<ArgumentIncorrectType>(text);
                    }
                  } else {
                    if( u > static_cast<U>((std::numeric_limits<T>::max)())) {
                      throwOrMimic<ArgumentIncorrectType>(text);
                    }
                  }
                }
            };

            template<typename T>
            struct SignedCheck<T, false> {
                template<typename U>
                void operator()(bool, U, const std::string &) {}
            };

            template<typename T, typename U>
            void check_signed_range(bool negative, U value, const std::string &text) {
              SignedCheck<T, std::numeric_limits<T>::is_signed>()(negative, value, text);
            }
        }

        template<typename R, typename T>
        R checked_negate(T &&t, const std::string &, std::true_type) {
          // if we got to here, then `t` is a positive number that fits into
          // `R`. So to avoid MSVC C4146, we first cast it to `R`.
          // See https://github.com/jarro2783/cxxopts/issues/62 for more details.
          return static_cast<R>(-static_cast<R>(t - 1) - 1);
        }

        template<typename R, typename T>
        T checked_negate(T &&t, const std::string &text, std::false_type) {
          throwOrMimic<ArgumentIncorrectType>(text);
          return t;
        }

        template<typename T>
        void integer_parser(const std::string &text, T &value) {
          std::smatch match;
          std::regex_match(text, match, integer_pattern);

          if( match.length() == 0 ) {
            throwOrMimic<ArgumentIncorrectType>(text);
          }

          if( match.length(4) > 0 ) {
            value = 0;
            return;
          }

          using US = typename std::make_unsigned<T>::type;

          constexpr bool is_signed = std::numeric_limits<T>::is_signed;
          const bool negative = match.length(1) > 0;
          const uint8_t base = match.length(2) > 0 ? 16 : 10;

          auto value_match = match[3];

          US result = 0;

          for( auto iter = value_match.first; iter != value_match.second; ++iter ) {
            US digit = 0;

            if( *iter >= '0' && *iter <= '9' ) {
              digit = static_cast<US>(*iter - '0');
            } else if( base == 16 && *iter >= 'a' && *iter <= 'f' ) {
              digit = static_cast<US>(*iter - 'a' + 10);
            } else if( base == 16 && *iter >= 'A' && *iter <= 'F' ) {
              digit = static_cast<US>(*iter - 'A' + 10);
            } else {
              throwOrMimic<ArgumentIncorrectType>(text);
            }

            const US next = static_cast<US>(result * base + digit);
            if( result > next ) {
              throwOrMimic<ArgumentIncorrectType>(text);
            }

            result = next;
          }

          detail::check_signed_range<T>(negative, result, text);

          if( negative ) {
            value = checked_negate<T>(result, text, std::integral_constant<bool, is_signed>());
          } else {
            value = static_cast<T>(result);
          }
        }

        template<typename T>
        void stringstream_parser(const std::string &text, T &value) {
          std::stringstream in(text);
          in >> value;
          if( !in ) {
            throwOrMimic<ArgumentIncorrectType>(text);
          }
        }

        inline void parse_value(const std::string &text, uint8_t &value) {
          integer_parser(text, value);
        }

        inline void parse_value(const std::string &text, int8_t &value) {
          integer_parser(text, value);
        }

        inline void parse_value(const std::string &text, uint16_t &value) {
          integer_parser(text, value);
        }

        inline void parse_value(const std::string &text, int16_t &value) {
          integer_parser(text, value);
        }

        inline void parse_value(const std::string &text, uint32_t &value) {
          integer_parser(text, value);
        }

        inline void parse_value(const std::string &text, int32_t &value) {
          integer_parser(text, value);
        }

        inline void parse_value(const std::string &text, uint64_t &value) {
          integer_parser(text, value);
        }

        inline void parse_value(const std::string &text, int64_t &value) {
          integer_parser(text, value);
        }

        inline void parse_value(const std::string &text, SIMD &value){
          if(text == "NONE"){
            value = SIMD::SIMD_NONE;
          } else if( text == "SSE2") {
            value = SIMD::SIMD_SSE2;
          } else if(text == "AVX2"){
            value = SIMD::SIMD_AVX2;
          } else {
            throw ArgumentIncorrectType(text);
          }
        }

        inline void parse_value(const std::string &text, bool &value) {
          std::smatch result;
          std::regex_match(text, result, truthy_pattern);

          if( !result.empty()) {
            value = true;
            return;
          }

          std::regex_match(text, result, falsy_pattern);
          if( !result.empty()) {
            value = false;
            return;
          }

          throwOrMimic<ArgumentIncorrectType>(text);
        }

        inline void parse_value(const std::string &text, std::string &value) {
          value = text;
        }

        // The fallback parser. It uses the stringstream parser to parse all types
        // that have not been overloaded explicitly.  It has to be placed in the
        // source code before all other more specialized templates.
        template<typename T>
        void parse_value(const std::string &text, T &value) {
          stringstream_parser(text, value);
        }

        template<typename T>
        void parse_value(const std::string &text, std::vector<T> &value) {
          std::stringstream in(text);
          std::string token;
          while( !in.eof() && std::getline(in, token, CXXOPTS_VECTOR_DELIMITER)) {
            T v;
            parse_value(token, v);
            value.emplace_back(std::move(v));
          }
        }

#ifdef CXXOPTS_HAS_OPTIONAL
        template <typename T>
        void
        parse_value(const std::string& text, std::optional<T>& value)
        {
          T result;
          parse_value(text, result);
          value = std::move(result);
        }
#endif

        inline void parse_value(const std::string &text, char &c) {
          if( text.length() != 1 ) {
            throwOrMimic<ArgumentIncorrectType>(text);
          }

          c = text[0];
        }

        template<typename T>
        struct type_is_container {
            static constexpr bool value = false;
        };

        template<typename T>
        struct type_is_container<std::vector<T>> {
            static constexpr bool value = true;
        };

        template<typename T>
        class abstract_value : public Value {
            using Self = abstract_value<T>;

        public:
            abstract_value() : m_result(std::make_shared<T>()), m_store(m_result.get()) {
            }

            abstract_value(T *t) : m_store(t) {
            }

            virtual ~abstract_value() = default;

            abstract_value(const abstract_value &rhs) {
              if( rhs.m_result ) {
                m_result = std::make_shared<T>();
                m_store = m_result.get();
              } else {
                m_store = rhs.m_store;
              }

              m_default = rhs.m_default;
              m_implicit = rhs.m_implicit;
              m_default_value = rhs.m_default_value;
              m_implicit_value = rhs.m_implicit_value;
            }

            void parse(const std::string &text) const {
              parse_value(text, *m_store);
            }

            bool is_container() const {
              return type_is_container<T>::value;
            }

            void parse() const {
              parse_value(m_default_value, *m_store);
            }

            bool has_default() const {
              return m_default;
            }

            bool has_implicit() const {
              return m_implicit;
            }

            std::shared_ptr<Value> default_value(const std::string &value) {
              m_default = true;
              m_default_value = value;
              return shared_from_this();
            }

            std::shared_ptr<Value> implicit_value(const std::string &value) {
              m_implicit = true;
              m_implicit_value = value;
              return shared_from_this();
            }

            std::shared_ptr<Value> no_implicit_value() {
              m_implicit = false;
              return shared_from_this();
            }

            std::string get_default_value() const {
              return m_default_value;
            }

            std::string get_implicit_value() const {
              return m_implicit_value;
            }

            bool is_boolean() const {
              return std::is_same<T, bool>::value;
            }

            const T &get() const {
              if( m_store == nullptr ) {
                return *m_result;
              } else {
                return *m_store;
              }
            }

        protected:
            std::shared_ptr<T> m_result;
            T *m_store;

            bool m_default = false;
            bool m_implicit = false;

            std::string m_default_value;
            std::string m_implicit_value;
        };

        template<typename T>
        class standard_value : public abstract_value<T> {
        public:
            using abstract_value<T>::abstract_value;

            std::shared_ptr<Value> clone() const {
              return std::make_shared<standard_value<T>>(*this);
            }
        };

        template<>
        class standard_value<bool> : public abstract_value<bool> {
        public:
            ~standard_value() = default;

            standard_value() {
              set_default_and_implicit();
            }

            standard_value(bool *b) : abstract_value(b) {
              set_default_and_implicit();
            }

            std::shared_ptr<Value> clone() const {
              return std::make_shared<standard_value<bool>>(*this);
            }

        private:

            void set_default_and_implicit() {
              m_default = true;
              m_default_value = "false";
              m_implicit = true;
              m_implicit_value = "true";
            }
        };
    }

    template<typename T>
    std::shared_ptr<Value> value() {
      return std::make_shared<values::standard_value<T>>();
    }

    template<typename T>
    std::shared_ptr<Value> value(T &t) {
      return std::make_shared<values::standard_value<T>>(&t);
    }

    class OptionAdder;

    class OptionDetails {
    public:
        OptionDetails(std::string short_, std::string long_, String desc, std::shared_ptr<const Value> val);
        OptionDetails(const OptionDetails &rhs);
        OptionDetails(OptionDetails &&rhs) = default;
        const String &description() const;
        const Value &value() const;
        std::shared_ptr<Value> makeStorage() const;
        const std::string &shortName() const;
        const std::string &longName() const;

    private:
        std::string m_short;
        std::string m_long;
        String m_desc;
        std::shared_ptr<const Value> m_value;
        int m_count;
    };

    struct HelpOptionDetails {
        std::string s;
        std::string l;
        String desc;
        bool has_default;
        std::string default_value;
        bool has_implicit;
        std::string implicit_value;
        std::string arg_help;
        bool is_container;
        bool is_boolean;
    };

    struct HelpGroupDetails {
        std::string name;
        std::string description;
        std::vector<HelpOptionDetails> options;
    };

    class OptionValue {
    public:
        void parse(const std::shared_ptr<const OptionDetails>& details, const std::string &text);

        void parseDefault(const std::shared_ptr<const OptionDetails>& details);

        size_t count() const noexcept;

        // TODO: maybe default options should count towards the number of arguments
        bool hasDefault() const noexcept;

        template<typename T>
        const T &as() const {
          if( m_value == nullptr ) {
            throwOrMimic<std::domain_error>("No value");
          }

#ifdef CXXOPTS_NO_RTTI
          return static_cast<const values::standard_value<T>&>(*m_value).get();
#else
          return dynamic_cast<const values::standard_value<T> &>(*m_value).get();
#endif
        }

    private:
        void ensureValue(const std::shared_ptr<const OptionDetails>& details);

        std::shared_ptr<Value> m_value;
        size_t m_count = 0;
        bool m_default = false;
    };

    class KeyValue {
    public:
        KeyValue(std::string key, std::string value);

        const std::string &key() const;

        const std::string &value() const;

        template<typename T>
        T as() const {
          T result;
          values::parse_value(m_value, result);
          return result;
        }

    private:
        std::string m_key;
        std::string m_value;
    };

    class ParseResult {
    public:

        ParseResult(std::shared_ptr<std::unordered_map<std::string, std::shared_ptr<OptionDetails>>> , std::vector<std::string>,
                    bool allowUnrecognised, int &, char **&);

        size_t count(const std::string &o) const {
          auto iter = m_options->find(o);
          if( iter == m_options->end()) {
            return 0;
          }

          auto riter = m_results.find(iter->second);

          return riter->second.count();
        }

        const OptionValue &operator[](const std::string &option) const {
          auto iter = m_options->find(option);

          if( iter == m_options->end()) {
            throwOrMimic<OptionNotPresentException>(option);
          }

          auto riter = m_results.find(iter->second);

          return riter->second;
        }

        const std::vector<KeyValue> &arguments() const {
          return m_sequential;
        }

    private:

        void parse(int &argc, char **&argv);

        void addToOption(const std::string &option, const std::string &arg);

        bool consumePositional(const std::string& a);

        void parseOption(const std::shared_ptr<OptionDetails>& value, const std::string &, const std::string &arg = "");

        void parseDefault(const std::shared_ptr<OptionDetails>& details);

        void checkedParseArg(int argc, char **argv, int &current, const std::shared_ptr<OptionDetails>& value, const std::string &name);

        const std::shared_ptr<std::unordered_map<std::string, std::shared_ptr<OptionDetails>>> m_options;
        std::vector<std::string> m_positional;
        std::vector<std::string>::iterator m_next_positional;
        std::unordered_set<std::string> m_positional_set;
        std::unordered_map<std::shared_ptr<OptionDetails>, OptionValue> m_results;

        bool m_allow_unrecognised;

        std::vector<KeyValue> m_sequential;
    };

    struct Option {
        Option(std::string opts, std::string desc, std::shared_ptr<const Value> value = ::cxxopts::value<bool>(),
               std::string argHelp = "");

        std::string opts_;
        std::string desc_;
        std::shared_ptr<const Value> value_;
        std::string arg_help_;
    };

    class Options {
        typedef std::unordered_map<std::string, std::shared_ptr<OptionDetails>> OptionMap;
    public:

        Options(std::string program, std::string help_string = "") : m_program(std::move(program)),
                m_help_string(toLocalString(std::move(help_string))), m_custom_help("[OPTION...]"),
                m_positional_help("positional parameters"), m_show_positional(false), m_allow_unrecognised(false),
                m_options(std::make_shared<OptionMap>()), m_next_positional(m_positional.end()) {
        }

        Options &positional_help(std::string help_text) {
          m_positional_help = std::move(help_text);
          return *this;
        }

        Options &custom_help(std::string help_text) {
          m_custom_help = std::move(help_text);
          return *this;
        }

        Options &show_positional_help() {
          m_show_positional = true;
          return *this;
        }

        Options &allow_unrecognised_options() {
          m_allow_unrecognised = true;
          return *this;
        }

        ParseResult parse(int &argc, char **&argv);

        OptionAdder addOptions(std::string group = "");

        void addOptions(const std::string &group, std::initializer_list<Option> options);

        void addOption(const std::string &group, const Option &option);

        void addOption(const std::string &group, const std::string &s, const std::string &l, std::string desc,
                       const std::shared_ptr<const Value>& value, std::string argHelp);

        //parse positional arguments into the given option
        void parsePositional(std::string option);

        void parsePositional(std::vector<std::string> options);

        void parsePositional(std::initializer_list<std::string> options);

        template<typename Iterator>
        void parse_positional(Iterator begin, Iterator end) {
          parsePositional(std::vector<std::string> {begin, end});
        }

        std::string help(const std::vector<std::string> &helpGroups = {}) const;

        std::vector<std::string> groups() const;

        const HelpGroupDetails &groupHelp(const std::string &group) const;

    private:

        void addOneOption(const std::string &option, const std::shared_ptr<OptionDetails>& details);

        String helpOneGroup(const std::string &g) const;

        void generateGroupHelp(String &result, const std::vector<std::string> &printGroups) const;

        void generateAllGroupsHelp(String &result) const;

        std::string m_program;
        String m_help_string;
        std::string m_custom_help;
        std::string m_positional_help;
        bool m_show_positional;
        bool m_allow_unrecognised;

        std::shared_ptr<OptionMap> m_options;
        std::vector<std::string> m_positional;
        std::vector<std::string>::iterator m_next_positional;
        std::unordered_set<std::string> m_positional_set;

        //mapping from groups to help options
        std::map<std::string, HelpGroupDetails> m_help;
    };

    class OptionAdder {
    public:

        OptionAdder(Options &options, std::string group) : m_options(options), m_group(std::move(group)) {
        }

        OptionAdder &
        operator()(const std::string &opts, const std::string &desc, const std::shared_ptr<const Value>& value = ::cxxopts::value<bool>(),
                   std::string argHelp = "");

    private:
        Options &m_options;
        std::string m_group;
    };

    namespace {
        constexpr int OPTION_LONGEST = 30;
        constexpr int OPTION_DESC_GAP = 2;

        std::basic_regex<char> option_matcher("--([[:alnum:]][-_[:alnum:]]+)(=(.*))?|-([[:alnum:]]+)");

        std::basic_regex<char> option_specifier("(([[:alnum:]]),)?[ ]*([[:alnum:]][-_[:alnum:]]*)?");

        String format_option(const HelpOptionDetails &o) {
          auto &s = o.s;
          auto &l = o.l;

          String result = "  ";

          if( s.size() > 0 ) {
            result += "-" + toLocalString(s) + ",";
          } else {
            result += "   ";
          }

          if( l.size() > 0 ) {
            result += " --" + toLocalString(l);
          }

          auto arg = o.arg_help.size() > 0 ? toLocalString(o.arg_help) : "arg";

          if( !o.is_boolean ) {
            if( o.has_implicit ) {
              result += " [=" + arg + "(=" + toLocalString(o.implicit_value) + ")]";
            } else {
              result += " " + arg;
            }
          }

          return result;
        }

        String format_description(const HelpOptionDetails &o, size_t start, size_t width) {
          auto desc = o.desc;

          if( o.has_default && (!o.is_boolean || o.default_value != "false")) {
            if( o.default_value != "" ) {
              desc += toLocalString(" (default: " + o.default_value + ")");
            } else {
              desc += toLocalString(" (default: \"\")");
            }
          }

          String result;

          auto current = std::begin(desc);
          auto startLine = current;
          auto lastSpace = current;

          auto size = size_t {};

          while( current != std::end(desc)) {
            if( *current == ' ' ) {
              lastSpace = current;
            }

            if( *current == '\n' ) {
              startLine = current + 1;
              lastSpace = startLine;
            } else if( size > width ) {
              if( lastSpace == startLine ) {
                stringAppend(result, startLine, current + 1);
                stringAppend(result, "\n");
                stringAppend(result, start, ' ');
                startLine = current + 1;
                lastSpace = startLine;
              } else {
                stringAppend(result, startLine, lastSpace);
                stringAppend(result, "\n");
                stringAppend(result, start, ' ');
                startLine = lastSpace + 1;
                lastSpace = startLine;
              }
              size = 0;
            } else {
              ++size;
            }

            ++current;
          }

          //append whatever is left
          stringAppend(result, startLine, current);

          return result;
        }
    }
}

#endif //CXXOPTS_HPP_INCLUDED
