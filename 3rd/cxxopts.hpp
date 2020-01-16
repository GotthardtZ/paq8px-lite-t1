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

namespace cxxopts {
    typedef std::string String;

    template<typename T>
    T toLocalString(T &&t) {
      return std::forward<T>(t);
    }

    inline size_t stringLength(const String &s) {
      return s.length();
    }

    inline String &stringAppend(String &s, const String &a) {
      return s.append(a);
    }

    inline String &stringAppend(String &s, size_t n, char c) {
      return s.append(n, c);
    }

    template<typename Iterator>
    String &stringAppend(String &s, Iterator begin, Iterator end) {
      return s.append(begin, end);
    }

    template<typename T>
    std::string toUtf8String(T &&t) {
      return std::forward<T>(t);
    }

    inline bool empty(const std::string &s) {
      return s.empty();
    }
}

namespace cxxopts {
    namespace {
#ifdef _WIN32
        const std::string leftQuote("\'");
        const std::string rightQuote("\'");
#else
        const std::string leftQuote("‘");
        const std::string rightQuote("’");
#endif
    }

    class Value : public std::enable_shared_from_this<Value> {
    public:

        virtual ~Value() = default;

        virtual std::shared_ptr<Value> clone() const = 0;

        virtual void parse(const std::string &text) const = 0;

        virtual void parse() const = 0;

        virtual bool hasDefault() const = 0;

        virtual bool isContainer() const = 0;

        virtual bool hasImplicit() const = 0;

        virtual std::string getDefaultValue() const = 0;

        virtual std::string getImplicitValue() const = 0;

        virtual std::shared_ptr<Value> defaultValue(const std::string &value) = 0;

        virtual std::shared_ptr<Value> implicitValue(const std::string &value) = 0;

        virtual std::shared_ptr<Value> noImplicitValue() = 0;

        virtual bool isBoolean() const = 0;
    };

    class OptionException : public std::exception {
    public:
        explicit OptionException(std::string message);

        [[nodiscard]] const char *what() const noexcept override;

    private:
        std::string mMessage;
    };

    class OptionSpecException : public OptionException {
    public:
        explicit OptionSpecException(const std::string &message);
    };

    class OptionParseException : public OptionException {
    public:
        explicit OptionParseException(const std::string &message);
    };

    class OptionExistsError : public OptionSpecException {
    public:
        explicit OptionExistsError(const std::string &option);
    };

    class InvalidOptionFormatError : public OptionSpecException {
    public:
        explicit InvalidOptionFormatError(const std::string &format);
    };

    class OptionSyntaxException : public OptionParseException {
    public:
        explicit OptionSyntaxException(const std::string &text);
    };

    class OptionNotExistsException : public OptionParseException {
    public:
        explicit OptionNotExistsException(const std::string &option);
    };

    class MissingArgumentException : public OptionParseException {
    public:
        explicit MissingArgumentException(const std::string &option);
    };

    class OptionRequiresArgumentException : public OptionParseException {
    public:
        explicit OptionRequiresArgumentException(const std::string &option);
    };

    class OptionNotPresentException : public OptionParseException {
    public:
        explicit OptionNotPresentException(const std::string &option);
    };

    class ArgumentIncorrectType : public OptionParseException {
    public:
        explicit ArgumentIncorrectType(const std::string &arg);
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
            std::basic_regex<char> integerPattern("(-)?(0x)?([0-9a-zA-Z]+)|((0x)?0)");
            std::basic_regex<char> truthyPattern("(t|T)(rue)?|1");
            std::basic_regex<char> falsyPattern("(f|F)(alse)?|0");
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
            void checkSignedRange(bool negative, U value, const std::string &text) {
              SignedCheck<T, std::numeric_limits<T>::is_signed>()(negative, value, text);
            }
        }

        template<typename R, typename T>
        R checkedNegate(T &&t, const std::string &, std::true_type) {
          // if we got to here, then `t` is a positive number that fits into
          // `R`. So to avoid MSVC C4146, we first cast it to `R`.
          // See https://github.com/jarro2783/cxxopts/issues/62 for more details.
          return static_cast<R>(-static_cast<R>(t - 1) - 1);
        }

        template<typename R, typename T>
        T checkedNegate(T &&t, const std::string &text, std::false_type) {
          throwOrMimic<ArgumentIncorrectType>(text);
          return t;
        }

        template<typename T>
        void integerParser(const std::string &text, T &value) {
          std::smatch match;
          std::regex_match(text, match, integerPattern);

          if( match.length() == 0 ) {
            throwOrMimic<ArgumentIncorrectType>(text);
          }

          if( match.length(4) > 0 ) {
            value = 0;
            return;
          }

          using US = typename std::make_unsigned<T>::type;

          constexpr bool isSigned = std::numeric_limits<T>::is_signed;
          const bool negative = match.length(1) > 0;
          const uint8_t base = match.length(2) > 0 ? 16 : 10;

          auto valueMatch = match[3];

          US result = 0;

          for( auto iter = valueMatch.first; iter != valueMatch.second; ++iter ) {
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

          detail::checkSignedRange<T>(negative, result, text);

          if( negative ) {
            value = checkedNegate<T>(result, text, std::integral_constant<bool, isSigned>());
          } else {
            value = static_cast<T>(result);
          }
        }

        template<typename T>
        void stringstreamParser(const std::string &text, T &value) {
          std::stringstream in(text);
          in >> value;
          if( !in ) {
            throwOrMimic<ArgumentIncorrectType>(text);
          }
        }

        inline void parseValue(const std::string &text, uint8_t &value) {
          integerParser(text, value);
        }

        inline void parseValue(const std::string &text, int8_t &value) {
          integerParser(text, value);
        }

        inline void parseValue(const std::string &text, uint16_t &value) {
          integerParser(text, value);
        }

        inline void parseValue(const std::string &text, int16_t &value) {
          integerParser(text, value);
        }

        inline void parseValue(const std::string &text, uint32_t &value) {
          integerParser(text, value);
        }

        inline void parseValue(const std::string &text, int32_t &value) {
          integerParser(text, value);
        }

        inline void parseValue(const std::string &text, uint64_t &value) {
          integerParser(text, value);
        }

        inline void parseValue(const std::string &text, int64_t &value) {
          integerParser(text, value);
        }

        inline void parseValue(const std::string &text, SIMD &value) {
          if( text == "NONE" ) {
            value = SIMD::SIMD_NONE;
          } else if( text == "SSE2" ) {
            value = SIMD::SIMD_SSE2;
          } else if( text == "AVX2" ) {
            value = SIMD::SIMD_AVX2;
          } else {
            throw ArgumentIncorrectType(text);
          }
        }

        inline void parseValue(const std::string &text, bool &value) {
          std::smatch result;
          std::regex_match(text, result, truthyPattern);

          if( !result.empty()) {
            value = true;
            return;
          }

          std::regex_match(text, result, falsyPattern);
          if( !result.empty()) {
            value = false;
            return;
          }

          throwOrMimic<ArgumentIncorrectType>(text);
        }

        inline void parseValue(const std::string &text, std::string &value) {
          value = text;
        }

        // The fallback parser. It uses the stringstream parser to parse all types
        // that have not been overloaded explicitly.  It has to be placed in the
        // source code before all other more specialized templates.
        template<typename T>
        void parseValue(const std::string &text, T &value) {
          stringstreamParser(text, value);
        }

        template<typename T>
        void parseValue(const std::string &text, std::vector<T> &value) {
          std::stringstream in(text);
          std::string token;
          while( !in.eof() && std::getline(in, token, CXXOPTS_VECTOR_DELIMITER)) {
            T v;
            parseValue(token, v);
            value.emplace_back(std::move(v));
          }
        }

#ifdef CXXOPTS_HAS_OPTIONAL

        template<typename T>
        void parseValue(const std::string &text, std::optional<T> &value) {
          T result;
          parseValue(text, result);
          value = std::move(result);
        }

#endif

        inline void parseValue(const std::string &text, char &c) {
          if( text.length() != 1 ) {
            throwOrMimic<ArgumentIncorrectType>(text);
          }

          c = text[0];
        }

        template<typename T>
        struct TypeIsContainer {
            static constexpr bool value = false;
        };

        template<typename T>
        struct TypeIsContainer<std::vector<T>> {
            static constexpr bool value = true;
        };

        template<typename T>
        class AbstractValue : public Value {
            using self = AbstractValue<T>;

        public:
            AbstractValue() : mResult(std::make_shared<T>()), mStore(mResult.get()) {
            }

            explicit AbstractValue(T *t) : mStore(t) {}

            ~AbstractValue() override = default;

            AbstractValue(const AbstractValue &rhs) {
              if( rhs.mResult ) {
                mResult = std::make_shared<T>();
                mStore = mResult.get();
              } else {
                mStore = rhs.mStore;
              }

              mDefault = rhs.mDefault;
              mImplicit = rhs.mImplicit;
              mDefaultValue = rhs.mDefaultValue;
              mImplicitValue = rhs.mImplicitValue;
            }

            void parse(const std::string &text) const override {
              parseValue(text, *mStore);
            }

            bool isContainer() const override {
              return TypeIsContainer<T>::value;
            }

            void parse() const override {
              parseValue(mDefaultValue, *mStore);
            }

            bool hasDefault() const override {
              return mDefault;
            }

            bool hasImplicit() const override {
              return mImplicit;
            }

            std::shared_ptr<Value> defaultValue(const std::string &value) override {
              mDefault = true;
              mDefaultValue = value;
              return shared_from_this();
            }

            std::shared_ptr<Value> implicitValue(const std::string &value) override {
              mImplicit = true;
              mImplicitValue = value;
              return shared_from_this();
            }

            std::shared_ptr<Value> noImplicitValue() override {
              mImplicit = false;
              return shared_from_this();
            }

            std::string getDefaultValue() const override {
              return mDefaultValue;
            }

            std::string getImplicitValue() const override {
              return mImplicitValue;
            }

            bool isBoolean() const override {
              return std::is_same<T, bool>::value;
            }

            const T &get() const {
              if( mStore == nullptr ) {
                return *mResult;
              } else {
                return *mStore;
              }
            }

        protected:
            std::shared_ptr<T> mResult;
            T *mStore;

            bool mDefault = false;
            bool mImplicit = false;

            std::string mDefaultValue;
            std::string mImplicitValue;
        };

        template<typename T>
        class StandardValue : public AbstractValue<T> {
        public:
            using AbstractValue<T>::AbstractValue;

            [[nodiscard]] std::shared_ptr<Value> clone() const {
              return std::make_shared<StandardValue<T>>(*this);
            }
        };

        template<>
        class StandardValue<bool> : public AbstractValue<bool> {
        public:
            ~StandardValue() override = default;

            StandardValue() {
              setDefaultAndImplicit();
            }

            explicit StandardValue(bool *b) : AbstractValue(b) {
              setDefaultAndImplicit();
            }

            std::shared_ptr<Value> clone() const override {
              return std::make_shared<StandardValue<bool>>(*this);
            }

        private:

            void setDefaultAndImplicit() {
              mDefault = true;
              mDefaultValue = "false";
              mImplicit = true;
              mImplicitValue = "true";
            }
        };
    }

    template<typename T>
    std::shared_ptr<Value> value() {
      return std::make_shared<values::StandardValue<T>>();
    }

    template<typename T>
    std::shared_ptr<Value> value(T &t) {
      return std::make_shared<values::StandardValue<T>>(&t);
    }

    class OptionAdder;

    class OptionDetails {
    public:
        OptionDetails(std::string s, std::string l, String desc, std::shared_ptr<const Value> val);
        OptionDetails(const OptionDetails &rhs);
        OptionDetails(OptionDetails &&rhs) = default;
        [[nodiscard]] const String &description() const;
        [[nodiscard]] const Value &value() const;
        [[nodiscard]] std::shared_ptr<Value> makeStorage() const;
        [[nodiscard]] const std::string &shortName() const;
        [[nodiscard]] const std::string &longName() const;

    private:
        std::string mShort;
        std::string mLong;
        String mDesc;
        std::shared_ptr<const Value> mValue;
        int mCount;
    };

    struct HelpOptionDetails {
        std::string s;
        std::string l;
        String desc;
        bool hasDefault;
        std::string defaultValue;
        bool hasImplicit;
        std::string implicitValue;
        std::string argHelp;
        bool isContainer;
        bool isBoolean;
    };

    struct HelpGroupDetails {
        std::string name;
        std::string description;
        std::vector<HelpOptionDetails> options;
    };

    class OptionValue {
    public:
        void parse(const std::shared_ptr<const OptionDetails> &details, const std::string &text);

        void parseDefault(const std::shared_ptr<const OptionDetails> &details);

        [[nodiscard]] size_t count() const noexcept;

        // TODO: maybe default options should count towards the number of arguments
        [[nodiscard]] bool hasDefault() const noexcept;

        template<typename T>
        const T &as() const {
          if( mValue == nullptr ) {
            throwOrMimic<std::domain_error>("No value");
          }

#ifdef CXXOPTS_NO_RTTI
          return static_cast<const values::StandardValue<T>&>(*mValue).get();
#else
          return dynamic_cast<const values::StandardValue<T> &>(*mValue).get();
#endif
        }

    private:
        void ensureValue(const std::shared_ptr<const OptionDetails> &details);

        std::shared_ptr<Value> mValue;
        size_t mCount = 0;
        bool mDefault = false;
    };

    class KeyValue {
    public:
        KeyValue(std::string key, std::string value);

        [[nodiscard]] const std::string &key() const;

        [[nodiscard]] const std::string &value() const;

        template<typename T>
        T as() const {
          T result;
          values::parseValue(mValue, result);
          return result;
        }

    private:
        std::string mKey;
        std::string mValue;
    };

    class ParseResult {
    public:

        ParseResult(std::shared_ptr<std::unordered_map<std::string, std::shared_ptr<OptionDetails>>>, std::vector<std::string>,
                    bool allowUnrecognised, int &, char **&);

        [[nodiscard]] size_t count(const std::string &o) const {
          auto iter = mOptions->find(o);
          if( iter == mOptions->end()) {
            return 0;
          }

          auto rIter = mResults.find(iter->second);

          return rIter->second.count();
        }

        const OptionValue &operator[](const std::string &option) const {
          auto iter = mOptions->find(option);

          if( iter == mOptions->end()) {
            throwOrMimic<OptionNotPresentException>(option);
          }

          auto rIter = mResults.find(iter->second);

          return rIter->second;
        }

        [[nodiscard]] const std::vector<KeyValue> &arguments() const {
          return mSequential;
        }

    private:

        void parse(int &argc, char **&argv);

        void addToOption(const std::string &option, const std::string &arg);

        bool consumePositional(const std::string &a);

        void parseOption(const std::shared_ptr<OptionDetails> &value, const std::string &, const std::string &arg = "");

        void parseDefault(const std::shared_ptr<OptionDetails> &details);

        void checkedParseArg(int argc, char **argv, int &current, const std::shared_ptr<OptionDetails> &value, const std::string &name);

        const std::shared_ptr<std::unordered_map<std::string, std::shared_ptr<OptionDetails>>> mOptions;
        std::vector<std::string> mPositional;
        std::vector<std::string>::iterator mNextPositional;
        std::unordered_set<std::string> mPositionalSet;
        std::unordered_map<std::shared_ptr<OptionDetails>, OptionValue> mResults;

        bool mAllowUnrecognised;

        std::vector<KeyValue> mSequential;
    };

    struct Option {
        Option(std::string opts, std::string desc, std::shared_ptr<const Value> value = ::cxxopts::value<bool>(), std::string argHelp = "");

        std::string opts;
        std::string desc;
        std::shared_ptr<const Value> value_;
        std::string argHelp;
    };

    class Options {
        typedef std::unordered_map<std::string, std::shared_ptr<OptionDetails>> optionMap;
    public:

        explicit Options(std::string program, std::string helpString = "") : mProgram(std::move(program)),
                mHelpString(toLocalString(std::move(helpString))), mCustomHelp("[OPTION...]"), mPositionalHelp("positional parameters"),
                mShowPositional(false), mAllowUnrecognised(false), mOptions(std::make_shared<optionMap>()),
                mNextPositional(mPositional.end()) {
        }

        Options &positionalHelp(std::string helpText) {
          mPositionalHelp = std::move(helpText);
          return *this;
        }

        Options &customHelp(std::string helpText) {
          mCustomHelp = std::move(helpText);
          return *this;
        }

        Options &showPositionalHelp() {
          mShowPositional = true;
          return *this;
        }

        Options &allowUnrecognisedOptions() {
          mAllowUnrecognised = true;
          return *this;
        }

        ParseResult parse(int &argc, char **&argv);

        OptionAdder addOptions(std::string group = "");

        void addOptions(const std::string &group, std::initializer_list<Option> options);

        void addOption(const std::string &group, const Option &option);

        void addOption(const std::string &group, const std::string &s, const std::string &l, std::string desc,
                       const std::shared_ptr<const Value> &value, std::string argHelp);

        //parse positional arguments into the given option
        void parsePositional(std::string option);

        void parsePositional(std::vector<std::string> options);

        void parsePositional(std::initializer_list<std::string> options);

        template<typename Iterator>
        void parsePositional(Iterator begin, Iterator end) {
          parsePositional(std::vector<std::string> {begin, end});
        }

        [[nodiscard]] std::string help(const std::vector<std::string> &helpGroups = {}) const;

        [[nodiscard]] std::vector<std::string> groups() const;

        [[nodiscard]] const HelpGroupDetails &groupHelp(const std::string &group) const;

    private:

        void addOneOption(const std::string &option, const std::shared_ptr<OptionDetails> &details);

        [[nodiscard]] String helpOneGroup(const std::string &g) const;

        void generateGroupHelp(String &result, const std::vector<std::string> &printGroups) const;

        void generateAllGroupsHelp(String &result) const;

        std::string mProgram;
        String mHelpString;
        std::string mCustomHelp;
        std::string mPositionalHelp;
        bool mShowPositional;
        bool mAllowUnrecognised;

        std::shared_ptr<optionMap> mOptions;
        std::vector<std::string> mPositional;
        std::vector<std::string>::iterator mNextPositional;
        std::unordered_set<std::string> mPositionalSet;

        //mapping from groups to help options
        std::map<std::string, HelpGroupDetails> mHelp;
    };

    class OptionAdder {
    public:

        OptionAdder(Options &options, std::string group) : mOptions(options), mGroup(std::move(group)) {
        }

        OptionAdder &
        operator()(const std::string &opts, const std::string &desc, const std::shared_ptr<const Value> &value = ::cxxopts::value<bool>(),
                   std::string argHelp = "");

    private:
        Options &mOptions;
        std::string mGroup;
    };

    namespace {
        constexpr int optionLongest = 30;
        constexpr int optionDescGap = 2;

        std::basic_regex<char> optionMatcher("--([[:alnum:]][-_[:alnum:]]+)(=(.*))?|-([[:alnum:]]+)");

        std::basic_regex<char> optionSpecifier("(([[:alnum:]]),)?[ ]*([[:alnum:]][-_[:alnum:]]*)?");

        String formatOption(const HelpOptionDetails &o) {
          auto &s = o.s;
          auto &l = o.l;

          String result = "  ";

          if( !s.empty()) {
            result += "-" + toLocalString(s) + ",";
          } else {
            result += "   ";
          }

          if( !l.empty()) {
            result += " --" + toLocalString(l);
          }

          auto arg = !o.argHelp.empty() ? toLocalString(o.argHelp) : "arg";

          if( !o.isBoolean ) {
            if( o.hasImplicit ) {
              result += " [=" + arg + "(=" + toLocalString(o.implicitValue) + ")]";
            } else {
              result += " " + arg;
            }
          }

          return result;
        }

        String formatDescription(const HelpOptionDetails &o, size_t start, size_t width) {
          auto desc = o.desc;

          if( o.hasDefault && (!o.isBoolean || o.defaultValue != "false")) {
            if( !o.defaultValue.empty()) {
              desc += toLocalString(" (default: " + o.defaultValue + ")");
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
