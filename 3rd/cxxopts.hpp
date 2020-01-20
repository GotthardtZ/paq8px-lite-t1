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
    using std::string;
    using std::stringstream;
    using std::begin;
    using std::end;
    using std::forward;
    using std::vector;
    using std::unordered_map;
    using std::shared_ptr;
    using std::enable_shared_from_this;
    using std::numeric_limits;
    using std::true_type;
    using std::basic_regex;
    using std::unordered_set;
    using std::exception;
    using std::false_type;
    using std::smatch;
    using std::integral_constant;
    using std::is_same;
    using std::make_shared;
    using std::optional;
    using std::domain_error;
    using std::make_unsigned;
    using std::is_base_of;
    using std::map;
    using std::initializer_list;

    template<typename T>
    T toLocalString(T &&t) {
      return forward<T>(t);
    }

    inline size_t stringLength(const string &s) {
      return s.length();
    }

    inline string &stringAppend(string &s, const string &a) {
      return s.append(a);
    }

    inline string &stringAppend(string &s, size_t n, char c) {
      return s.append(n, c);
    }

    template<typename Iterator>
    string &stringAppend(string &s, Iterator begin, Iterator end) {
      return s.append(begin, end);
    }

    template<typename T>
    string toUtf8String(T &&t) {
      return forward<T>(t);
    }

    inline bool empty(const string &s) {
      return s.empty();
    }
}

namespace cxxopts {
    namespace {
#ifdef _WIN32
        const string leftQuote("\'");
        const string rightQuote("\'");
#else
        const string leftQuote("‘");
        const string rightQuote("’");
#endif
    }

    class Value : public enable_shared_from_this<Value> {
    public:

        virtual ~Value() = default;

        virtual shared_ptr<Value> clone() const = 0;

        virtual void parse(const string &text) const = 0;

        virtual void parse() const = 0;

        virtual bool hasDefault() const = 0;

        virtual bool isContainer() const = 0;

        virtual bool hasImplicit() const = 0;

        virtual string getDefaultValue() const = 0;

        virtual string getImplicitValue() const = 0;

        virtual shared_ptr<Value> defaultValue(const string &value) = 0;

        virtual shared_ptr<Value> implicitValue(const string &value) = 0;

        virtual shared_ptr<Value> noImplicitValue() = 0;

        virtual bool isBoolean() const = 0;
    };

    class OptionException : public exception {
    public:
        explicit OptionException(string message);

        [[nodiscard]] const char *what() const noexcept override;

    private:
        string mMessage;
    };

    class OptionSpecException : public OptionException {
    public:
        explicit OptionSpecException(const string &message);
    };

    class OptionParseException : public OptionException {
    public:
        explicit OptionParseException(const string &message);
    };

    class OptionExistsError : public OptionSpecException {
    public:
        explicit OptionExistsError(const string &option);
    };

    class InvalidOptionFormatError : public OptionSpecException {
    public:
        explicit InvalidOptionFormatError(const string &format);
    };

    class OptionSyntaxException : public OptionParseException {
    public:
        explicit OptionSyntaxException(const string &text);
    };

    class OptionNotExistsException : public OptionParseException {
    public:
        explicit OptionNotExistsException(const string &option);
    };

    class MissingArgumentException : public OptionParseException {
    public:
        explicit MissingArgumentException(const string &option);
    };

    class OptionRequiresArgumentException : public OptionParseException {
    public:
        explicit OptionRequiresArgumentException(const string &option);
    };

    class OptionNotPresentException : public OptionParseException {
    public:
        explicit OptionNotPresentException(const string &option);
    };

    class ArgumentIncorrectType : public OptionParseException {
    public:
        explicit ArgumentIncorrectType(const string &arg);
    };

    template<typename T>
    void throwOrMimic(const string &text) {
      static_assert(is_base_of<exception, T>::value, "throwOrMimic only works on exception and "
                                                               "deriving classes");

#ifndef CXXOPTS_NO_EXCEPTIONS
      // If CXXOPTS_NO_EXCEPTIONS is not defined, just throw
      throw T {text};
#else
      // Otherwise manually instantiate the exception, print what() to stderr,
      // and abort
      T exception{text};
      cerr << exception.what() << endl;
      cerr << "Aborting (exceptions disabled)..." << endl;
      abort();
#endif
    }

    namespace values {
        namespace {
            basic_regex<char> integerPattern("(-)?(0x)?([0-9a-zA-Z]+)|((0x)?0)");
            basic_regex<char> truthyPattern("(t|T)(rue)?|1");
            basic_regex<char> falsyPattern("(f|F)(alse)?|0");
        }

        namespace detail {
            template<typename T, bool B>
            struct SignedCheck;

            template<typename T>
            struct SignedCheck<T, true> {
                template<typename U>
                void operator()(bool negative, U u, const string &text) {
                  if( negative ) {
                    if( u > static_cast<U>((numeric_limits<T>::min)())) {
                      throwOrMimic<ArgumentIncorrectType>(text);
                    }
                  } else {
                    if( u > static_cast<U>((numeric_limits<T>::max)())) {
                      throwOrMimic<ArgumentIncorrectType>(text);
                    }
                  }
                }
            };

            template<typename T>
            struct SignedCheck<T, false> {
                template<typename U>
                void operator()(bool, U, const string &) {}
            };

            template<typename T, typename U>
            void checkSignedRange(bool negative, U value, const string &text) {
              SignedCheck<T, numeric_limits<T>::is_signed>()(negative, value, text);
            }
        }

        template<typename R, typename T>
        R checkedNegate(T &&t, const string &, true_type) {
          // if we got to here, then `t` is a positive number that fits into
          // `R`. So to avoid MSVC C4146, we first cast it to `R`.
          // See https://github.com/jarro2783/cxxopts/issues/62 for more details.
          return static_cast<R>(-static_cast<R>(t - 1) - 1);
        }

        template<typename R, typename T>
        T checkedNegate(T &&t, const string &text, false_type) {
          throwOrMimic<ArgumentIncorrectType>(text);
          return t;
        }

        template<typename T>
        void integerParser(const string &text, T &value) {
          smatch match;
          regex_match(text, match, integerPattern);

          if( match.length() == 0 ) {
            throwOrMimic<ArgumentIncorrectType>(text);
          }

          if( match.length(4) > 0 ) {
            value = 0;
            return;
          }

          using US = typename make_unsigned<T>::type;

          constexpr bool isSigned = numeric_limits<T>::is_signed;
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
            value = checkedNegate<T>(result, text, integral_constant<bool, isSigned>());
          } else {
            value = static_cast<T>(result);
          }
        }

        template<typename T>
        void stringstreamParser(const string &text, T &value) {
          stringstream in(text);
          in >> value;
          if( !in ) {
            throwOrMimic<ArgumentIncorrectType>(text);
          }
        }

        inline void parseValue(const string &text, uint8_t &value) {
          integerParser(text, value);
        }

        inline void parseValue(const string &text, int8_t &value) {
          integerParser(text, value);
        }

        inline void parseValue(const string &text, uint16_t &value) {
          integerParser(text, value);
        }

        inline void parseValue(const string &text, int16_t &value) {
          integerParser(text, value);
        }

        inline void parseValue(const string &text, uint32_t &value) {
          integerParser(text, value);
        }

        inline void parseValue(const string &text, int32_t &value) {
          integerParser(text, value);
        }

        inline void parseValue(const string &text, uint64_t &value) {
          integerParser(text, value);
        }

        inline void parseValue(const string &text, int64_t &value) {
          integerParser(text, value);
        }

        inline void parseValue(const string &text, SIMD &value) {
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

        inline void parseValue(const string &text, bool &value) {
          smatch result;
          regex_match(text, result, truthyPattern);

          if( !result.empty()) {
            value = true;
            return;
          }

          regex_match(text, result, falsyPattern);
          if( !result.empty()) {
            value = false;
            return;
          }

          throwOrMimic<ArgumentIncorrectType>(text);
        }

        inline void parseValue(const string &text, string &value) {
          value = text;
        }

        // The fallback parser. It uses the stringstream parser to parse all types
        // that have not been overloaded explicitly.  It has to be placed in the
        // source code before all other more specialized templates.
        template<typename T>
        void parseValue(const string &text, T &value) {
          stringstreamParser(text, value);
        }

        template<typename T>
        void parseValue(const string &text, vector<T> &value) {
          stringstream in(text);
          string token;
          while( !in.eof() && getline(in, token, CXXOPTS_VECTOR_DELIMITER)) {
            T v;
            parseValue(token, v);
            value.emplace_back(move(v));
          }
        }

#ifdef CXXOPTS_HAS_OPTIONAL

        template<typename T>
        void parseValue(const string &text, optional<T> &value) {
          T result;
          parseValue(text, result);
          value = move(result);
        }

#endif

        inline void parseValue(const string &text, char &c) {
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
        struct TypeIsContainer<vector<T>> {
            static constexpr bool value = true;
        };

        template<typename T>
        class AbstractValue : public Value {
            using self = AbstractValue<T>;

        public:
            AbstractValue() : mResult(make_shared<T>()), mStore(mResult.get()) {
            }

            explicit AbstractValue(T *t) : mStore(t) {}

            ~AbstractValue() override = default;

            AbstractValue(const AbstractValue &rhs) {
              if( rhs.mResult ) {
                mResult = make_shared<T>();
                mStore = mResult.get();
              } else {
                mStore = rhs.mStore;
              }

              mDefault = rhs.mDefault;
              mImplicit = rhs.mImplicit;
              mDefaultValue = rhs.mDefaultValue;
              mImplicitValue = rhs.mImplicitValue;
            }

            void parse(const string &text) const override {
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

            shared_ptr<Value> defaultValue(const string &value) override {
              mDefault = true;
              mDefaultValue = value;
              return shared_from_this();
            }

            shared_ptr<Value> implicitValue(const string &value) override {
              mImplicit = true;
              mImplicitValue = value;
              return shared_from_this();
            }

            shared_ptr<Value> noImplicitValue() override {
              mImplicit = false;
              return shared_from_this();
            }

            string getDefaultValue() const override {
              return mDefaultValue;
            }

            string getImplicitValue() const override {
              return mImplicitValue;
            }

            bool isBoolean() const override {
              return is_same<T, bool>::value;
            }

            const T &get() const {
              if( mStore == nullptr ) {
                return *mResult;
              } else {
                return *mStore;
              }
            }

        protected:
            shared_ptr<T> mResult;
            T *mStore;

            bool mDefault = false;
            bool mImplicit = false;

            string mDefaultValue;
            string mImplicitValue;
        };

        template<typename T>
        class StandardValue : public AbstractValue<T> {
        public:
            using AbstractValue<T>::AbstractValue;

            [[nodiscard]] shared_ptr<Value> clone() const {
              return make_shared<StandardValue<T>>(*this);
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

            shared_ptr<Value> clone() const override {
              return make_shared<StandardValue<bool>>(*this);
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
    shared_ptr<Value> value() {
      return make_shared<values::StandardValue<T>>();
    }

    template<typename T>
    shared_ptr<Value> value(T &t) {
      return make_shared<values::StandardValue<T>>(&t);
    }

    class OptionAdder;

    class OptionDetails {
    public:
        OptionDetails(string s, string l, string desc, shared_ptr<const Value> val);
        OptionDetails(const OptionDetails &rhs);
        OptionDetails(OptionDetails &&rhs) = default;
        [[nodiscard]] const string &description() const;
        [[nodiscard]] const Value &value() const;
        [[nodiscard]] shared_ptr<Value> makeStorage() const;
        [[nodiscard]] const string &shortName() const;
        [[nodiscard]] const string &longName() const;

    private:
        string mShort;
        string mLong;
        string mDesc;
        shared_ptr<const Value> mValue;
        int mCount;
    };

    struct HelpOptionDetails {
        string s;
        string l;
        string desc;
        bool hasDefault;
        string defaultValue;
        bool hasImplicit;
        string implicitValue;
        string argHelp;
        bool isContainer;
        bool isBoolean;
    };

    struct HelpGroupDetails {
        string name;
        string description;
        vector<HelpOptionDetails> options;
    };

    class OptionValue {
    public:
        void parse(const shared_ptr<const OptionDetails> &details, const string &text);

        void parseDefault(const shared_ptr<const OptionDetails> &details);

        [[nodiscard]] size_t count() const noexcept;

        // TODO: maybe default options should count towards the number of arguments
        [[nodiscard]] bool hasDefault() const noexcept;

        template<typename T>
        const T &as() const {
          if( mValue == nullptr ) {
            throwOrMimic<domain_error>("No value");
          }

#ifdef CXXOPTS_NO_RTTI
          return static_cast<const values::StandardValue<T>&>(*mValue).get();
#else
          return dynamic_cast<const values::StandardValue<T> &>(*mValue).get();
#endif
        }

    private:
        void ensureValue(const shared_ptr<const OptionDetails> &details);

        shared_ptr<Value> mValue;
        size_t mCount = 0;
        bool mDefault = false;
    };

    class KeyValue {
    public:
        KeyValue(string key, string value);

        [[nodiscard]] const string &key() const;

        [[nodiscard]] const string &value() const;

        template<typename T>
        T as() const {
          T result;
          values::parseValue(mValue, result);
          return result;
        }

    private:
        string mKey;
        string mValue;
    };

    class ParseResult {
    public:

        ParseResult(shared_ptr<unordered_map<string, shared_ptr<OptionDetails>>>, vector<string>,
                    bool allowUnrecognised, int &, char **&);

        [[nodiscard]] size_t count(const string &o) const {
          auto iter = mOptions->find(o);
          if( iter == mOptions->end()) {
            return 0;
          }

          auto rIter = mResults.find(iter->second);

          return rIter->second.count();
        }

        const OptionValue &operator[](const string &option) const {
          auto iter = mOptions->find(option);

          if( iter == mOptions->end()) {
            throwOrMimic<OptionNotPresentException>(option);
          }

          auto rIter = mResults.find(iter->second);

          return rIter->second;
        }

        [[nodiscard]] const vector<KeyValue> &arguments() const {
          return mSequential;
        }

    private:

        void parse(int &argc, char **&argv);

        void addToOption(const string &option, const string &arg);

        bool consumePositional(const string &a);

        void parseOption(const shared_ptr<OptionDetails> &value, const string &, const string &arg = "");

        void parseDefault(const shared_ptr<OptionDetails> &details);

        void checkedParseArg(int argc, char **argv, int &current, const shared_ptr<OptionDetails> &value, const string &name);

        const shared_ptr<unordered_map<string, shared_ptr<OptionDetails>>> mOptions;
        vector<string> mPositional;
        vector<string>::iterator mNextPositional;
        unordered_set<string> mPositionalSet;
        unordered_map<shared_ptr<OptionDetails>, OptionValue> mResults;

        bool mAllowUnrecognised;

        vector<KeyValue> mSequential;
    };

    struct Option {
        Option(string opts, string desc, shared_ptr<const Value> value = ::cxxopts::value<bool>(), string argHelp = "");

        string opts;
        string desc;
        shared_ptr<const Value> value_;
        string argHelp;
    };

    class Options {
        typedef unordered_map<string, shared_ptr<OptionDetails>> optionMap;
    public:

        explicit Options(string program, string helpString = "") : mProgram(move(program)),
                mHelpString(toLocalString(move(helpString))), mCustomHelp("[OPTION...]"), mPositionalHelp("positional parameters"),
                mShowPositional(false), mAllowUnrecognised(false), mOptions(make_shared<optionMap>()),
                mNextPositional(mPositional.end()) {
        }

        Options &positionalHelp(string helpText) {
          mPositionalHelp = move(helpText);
          return *this;
        }

        Options &customHelp(string helpText) {
          mCustomHelp = move(helpText);
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

        OptionAdder addOptions(string group = "");

        void addOptions(const string &group, initializer_list<Option> options);

        void addOption(const string &group, const Option &option);

        void addOption(const string &group, const string &s, const string &l, string desc,
                       const shared_ptr<const Value> &value, string argHelp);

        //parse positional arguments into the given option
        void parsePositional(string option);

        void parsePositional(vector<string> options);

        void parsePositional(initializer_list<string> options);

        template<typename Iterator>
        void parsePositional(Iterator begin, Iterator end) {
          parsePositional(vector<string> {begin, end});
        }

        [[nodiscard]] string help(const vector<string> &helpGroups = {}) const;

        [[nodiscard]] vector<string> groups() const;

        [[nodiscard]] const HelpGroupDetails &groupHelp(const string &group) const;

    private:

        void addOneOption(const string &option, const shared_ptr<OptionDetails> &details);

        [[nodiscard]] string helpOneGroup(const string &g) const;

        void generateGroupHelp(string &result, const vector<string> &printGroups) const;

        void generateAllGroupsHelp(string &result) const;

        string mProgram;
        string mHelpString;
        string mCustomHelp;
        string mPositionalHelp;
        bool mShowPositional;
        bool mAllowUnrecognised;

        shared_ptr<optionMap> mOptions;
        vector<string> mPositional;
        vector<string>::iterator mNextPositional;
        unordered_set<string> mPositionalSet;

        //mapping from groups to help options
        map<string, HelpGroupDetails> mHelp;
    };

    class OptionAdder {
    public:

        OptionAdder(Options &options, string group) : mOptions(options), mGroup(move(group)) {
        }

        OptionAdder &
        operator()(const string &opts, const string &desc, const shared_ptr<const Value> &value = ::cxxopts::value<bool>(),
                   string argHelp = "");

    private:
        Options &mOptions;
        string mGroup;
    };

    namespace {
        constexpr int optionLongest = 30;
        constexpr int optionDescGap = 2;

        basic_regex<char> optionMatcher("--([[:alnum:]][-_[:alnum:]]+)(=(.*))?|-([[:alnum:]]+)");

        basic_regex<char> optionSpecifier("(([[:alnum:]]),)?[ ]*([[:alnum:]][-_[:alnum:]]*)?");

        string formatOption(const HelpOptionDetails &o) {
          auto &s = o.s;
          auto &l = o.l;

          string result = "  ";

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

        string formatDescription(const HelpOptionDetails &o, size_t start, size_t width) {
          auto desc = o.desc;

          if( o.hasDefault && (!o.isBoolean || o.defaultValue != "false")) {
            if( !o.defaultValue.empty()) {
              desc += toLocalString(" (default: " + o.defaultValue + ")");
            } else {
              desc += toLocalString(" (default: \"\")");
            }
          }

          string result;

          auto current = begin(desc);
          auto startLine = current;
          auto lastSpace = current;

          auto size = size_t {};

          while( current != end(desc)) {
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
