#include "cxxopts.hpp"

#include <utility>

namespace cxxopts {
    using std::match_results;
    using std::sub_match;
    using std::pair;
    using std::min;

    cxxopts::OptionException::OptionException(string message) : mMessage(move(message)) {
    }

    const char *cxxopts::OptionException::what() const noexcept {
      return mMessage.c_str();
    }

    cxxopts::OptionSpecException::OptionSpecException(const string &message) : OptionException(message) {
    }

    cxxopts::OptionParseException::OptionParseException(const string &message) : OptionException(message) {
    }

    cxxopts::OptionExistsError::OptionExistsError(const string &option) : OptionSpecException(
            "Option " + leftQuote + option + rightQuote + " already exists") {
    }

    cxxopts::InvalidOptionFormatError::InvalidOptionFormatError(const string &format) : OptionSpecException(
            "Invalid option format " + leftQuote + format + rightQuote) {
    }

    cxxopts::OptionSyntaxException::OptionSyntaxException(const string &text) : OptionParseException(
            "Argument " + leftQuote + text + rightQuote + " starts with a - but has incorrect syntax") {
    }

    cxxopts::OptionNotExistsException::OptionNotExistsException(const string &option) : OptionParseException(
            "Option " + leftQuote + option + rightQuote + " does not exist") {
    }

    cxxopts::MissingArgumentException::MissingArgumentException(const string &option) : OptionParseException(
            "Option " + leftQuote + option + rightQuote + " is missing an argument") {
    }

    cxxopts::OptionRequiresArgumentException::OptionRequiresArgumentException(const string &option) : OptionParseException(
            "Option " + leftQuote + option + rightQuote + " requires an argument") {
    }

    cxxopts::OptionNotPresentException::OptionNotPresentException(const string &option) : OptionParseException(
            "Option " + leftQuote + option + rightQuote + " not present") {
    }

    cxxopts::ArgumentIncorrectType::ArgumentIncorrectType(const string &arg) : OptionParseException(
            "Argument " + leftQuote + arg + rightQuote + " failed to parse") {
    }

    cxxopts::OptionDetails::OptionDetails(string s, string l, cxxopts::string desc, shared_ptr<const Value> val) : mShort(move(s)),
            mLong(move(l)), mDesc(move(desc)), mValue(move(val)), mCount(0) {
    }

    cxxopts::OptionDetails::OptionDetails(const cxxopts::OptionDetails &rhs) : mDesc(rhs.mDesc), mCount(rhs.mCount) {
      mValue = rhs.mValue->clone();
    }

    const cxxopts::string &cxxopts::OptionDetails::description() const {
      return mDesc;
    }

    const cxxopts::Value &cxxopts::OptionDetails::value() const {
      return *mValue;
    }

    shared_ptr<cxxopts::Value> cxxopts::OptionDetails::makeStorage() const {
      return mValue->clone();
    }

    const string &cxxopts::OptionDetails::shortName() const {
      return mShort;
    }

    const string &cxxopts::OptionDetails::longName() const {
      return mLong;
    }

    void cxxopts::OptionValue::parse(const shared_ptr<const OptionDetails> &details, const string &text) {
      ensureValue(details);
      ++mCount;
      mValue->parse(text);
    }

    void cxxopts::OptionValue::parseDefault(const shared_ptr<const OptionDetails> &details) {
      ensureValue(details);
      mDefault = true;
      mValue->parse();
    }

    size_t cxxopts::OptionValue::count() const noexcept {
      return mCount;
    }

    bool cxxopts::OptionValue::hasDefault() const noexcept {
      return mDefault;
    }

    void cxxopts::OptionValue::ensureValue(const shared_ptr<const OptionDetails> &details) {
      if( mValue == nullptr ) {
        mValue = details->makeStorage();
      }
    }

    cxxopts::KeyValue::KeyValue(string key, string value) : mKey(move(key)), mValue(move(value)) {
    }

    const string &cxxopts::KeyValue::key() const {
      return mKey;
    }

    const string &cxxopts::KeyValue::value() const {
      return mValue;
    }

    cxxopts::Option::Option(string opts, string desc, shared_ptr<const Value> value, string argHelp) : opts(move(opts)), desc(move(desc)),
            value_(move(value)), argHelp(move(argHelp)) {
    }

    cxxopts::ParseResult::ParseResult(shared_ptr<unordered_map<string, shared_ptr<OptionDetails>>> options, vector<string> positional,
                                      bool allowUnrecognised, int &argc, char **&argv) : mOptions(move(options)),
            mPositional(move(positional)), mNextPositional(mPositional.begin()), mAllowUnrecognised(allowUnrecognised) {
      parse(argc, argv);
    }

    void cxxopts::Options::addOptions(const string &group, initializer_list<Option> options) {
      OptionAdder optionAdder(*this, group);
      for( const auto &option: options ) {
        optionAdder(option.opts, option.desc, option.value_, option.argHelp);
      }
    }

    cxxopts::OptionAdder cxxopts::Options::addOptions(string group) {
      return OptionAdder(*this, move(group));
    }

    cxxopts::OptionAdder &
    cxxopts::OptionAdder::operator()(const string &opts, const string &desc, const shared_ptr<const Value> &value, string argHelp) {
      match_results<const char *> result;
      regex_match(opts.c_str(), result, optionSpecifier);

      if( result.empty()) {
        throwOrMimic<InvalidOptionFormatError>(opts);
      }

      const auto &shortMatch = result[2];
      const auto &longMatch = result[3];

      if( !shortMatch.length() && !longMatch.length()) {
        throwOrMimic<InvalidOptionFormatError>(opts);
      } else if( longMatch.length() == 1 && shortMatch.length()) {
        throwOrMimic<InvalidOptionFormatError>(opts);
      }

      auto optionNames = [](const sub_match<const char *> &short_, const sub_match<const char *> &long_) {
          if( long_.length() == 1 ) {
            return make_tuple(long_.str(), short_.str());
          } else {
            return make_tuple(short_.str(), long_.str());
          }
      }(shortMatch, longMatch);

      mOptions.addOption(mGroup, get<0>(optionNames), get<1>(optionNames), desc, value, move(argHelp));

      return *this;
    }

    void cxxopts::ParseResult::parseDefault(const shared_ptr<OptionDetails> &details) {
      mResults[details].parseDefault(details);
    }

    void cxxopts::ParseResult::parseOption(const shared_ptr<OptionDetails> &value, const string & /*name*/, const string &arg) {
      auto &result = mResults[value];
      result.parse(value, arg);

      mSequential.emplace_back(value->longName(), arg);
    }

    void
    cxxopts::ParseResult::checkedParseArg(int argc, char **argv, int &current, const shared_ptr<OptionDetails> &value, const string &name) {
      if( current + 1 >= argc ) {
        if( value->value().hasImplicit()) {
          parseOption(value, name, value->value().getImplicitValue());
        } else {
          throwOrMimic<MissingArgumentException>(name);
        }
      } else {
        if( value->value().hasImplicit()) {
          parseOption(value, name, value->value().getImplicitValue());
        } else {
          parseOption(value, name, argv[current + 1]);
          ++current;
        }
      }
    }

    void cxxopts::ParseResult::addToOption(const string &option, const string &arg) {
      auto iter = mOptions->find(option);

      if( iter == mOptions->end()) {
        throwOrMimic<OptionNotExistsException>(option);
      }

      parseOption(iter->second, option, arg);
    }

    bool cxxopts::ParseResult::consumePositional(const string &a) {
      while( mNextPositional != mPositional.end()) {
        auto iter = mOptions->find(*mNextPositional);
        if( iter != mOptions->end()) {
          auto &result = mResults[iter->second];
          if( !iter->second->value().isContainer()) {
            if( result.count() == 0 ) {
              addToOption(*mNextPositional, a);
              ++mNextPositional;
              return true;
            } else {
              ++mNextPositional;
              continue;
            }
          } else {
            addToOption(*mNextPositional, a);
            return true;
          }
        } else {
          throwOrMimic<OptionNotExistsException>(*mNextPositional);
        }
      }

      return false;
    }

    void cxxopts::Options::parsePositional(string option) {
      parsePositional(vector<string> {move(option)});
    }

    void cxxopts::Options::parsePositional(vector<string> options) {
      mPositional = move(options);
      mNextPositional = mPositional.begin();

      mPositionalSet.insert(mPositional.begin(), mPositional.end());
    }

    void cxxopts::Options::parsePositional(initializer_list<string> options) {
      parsePositional(vector<string>(options));
    }

    cxxopts::ParseResult cxxopts::Options::parse(int &argc, char **&argv) {
      ParseResult result(mOptions, mPositional, mAllowUnrecognised, argc, argv);
      return result;
    }

    void cxxopts::ParseResult::parse(int &argc, char **&argv) {
      int current = 1;

      int nextKeep = 1;

      bool consumeRemaining = false;

      while( current != argc ) {
        if( strcmp(argv[current], "--") == 0 ) {
          consumeRemaining = true;
          ++current;
          break;
        }

        match_results<const char *> result;
        regex_match(argv[current], result, optionMatcher);

        if( result.empty()) {
          //not a flag

          // but if it starts with a `-`, then it's an error
          if( argv[current][0] == '-' && argv[current][1] != '\0' ) {
            if( !mAllowUnrecognised ) {
              throwOrMimic<OptionSyntaxException>(argv[current]);
            }
          }

          //if true is returned here then it was consumed, otherwise it is
          //ignored
          if( consumePositional(argv[current])) {
          } else {
            argv[nextKeep] = argv[current];
            ++nextKeep;
          }
          //if we return from here then it was parsed successfully, so continue
        } else {
          //short or long option?
          if( result[4].length() != 0 ) {
            const string &s = result[4];

            for( size_t i = 0; i != s.size(); ++i ) {
              string name(1, s[i]);
              auto iter = mOptions->find(name);

              if( iter == mOptions->end()) {
                if( mAllowUnrecognised ) {
                  continue;
                } else {
                  //error
                  throwOrMimic<OptionNotExistsException>(name);
                }
              }

              auto value = iter->second;

              if( i + 1 == s.size()) {
                //it must be the last argument
                checkedParseArg(argc, argv, current, value, name);
              } else if( value->value().hasImplicit()) {
                parseOption(value, name, value->value().getImplicitValue());
              } else {
                //error
                throwOrMimic<OptionRequiresArgumentException>(name);
              }
            }
          } else if( result[1].length() != 0 ) {
            const string &name = result[1];

            auto iter = mOptions->find(name);

            if( iter == mOptions->end()) {
              if( mAllowUnrecognised ) {
                // keep unrecognised options in argument list, skip to next argument
                argv[nextKeep] = argv[current];
                ++nextKeep;
                ++current;
                continue;
              } else {
                //error
                throwOrMimic<OptionNotExistsException>(name);
              }
            }

            auto opt = iter->second;

            //equals provided for long option?
            if( result[2].length() != 0 ) {
              //parse the option given

              parseOption(opt, name, result[3]);
            } else {
              //parse the next argument
              checkedParseArg(argc, argv, current, opt, name);
            }
          }

        }

        ++current;
      }

      for( auto &opt : *mOptions ) {
        auto &detail = opt.second;
        auto &value = detail->value();

        auto &store = mResults[detail];

        if( value.hasDefault() && !store.count() && !store.hasDefault()) {
          parseDefault(detail);
        }
      }

      if( consumeRemaining ) {
        while( current < argc ) {
          if( !consumePositional(argv[current])) {
            break;
          }
          ++current;
        }

        //adjust argv for any that couldn't be swallowed
        while( current != argc ) {
          argv[nextKeep] = argv[current];
          ++nextKeep;
          ++current;
        }
      }

      argc = nextKeep;

    }

    void cxxopts::Options::addOption(const string &group, const Option &option) {
      addOptions(group, {option});
    }

    void
    cxxopts::Options::addOption(const string &group, const string &s, const string &l, string desc, const shared_ptr<const Value> &value,
                                string argHelp) {
      auto stringDesc = toLocalString(move(desc));
      auto option = make_shared<OptionDetails>(s, l, stringDesc, value);

      if( !s.empty()) {
        addOneOption(s, option);
      }

      if( !l.empty()) {
        addOneOption(l, option);
      }

      //add the help details
      auto &options = mHelp[group];

      options.options.emplace_back(HelpOptionDetails {s, l, stringDesc, value->hasDefault(), value->getDefaultValue(), value->hasImplicit(),
                                                      value->getImplicitValue(), move(argHelp), value->isContainer(), value->isBoolean()});
    }

    void cxxopts::Options::addOneOption(const string &option, const shared_ptr<OptionDetails> &details) {
      auto in = mOptions->emplace(option, details);

      if( !in.second ) {
        throwOrMimic<OptionExistsError>(option);
      }
    }

    cxxopts::string cxxopts::Options::helpOneGroup(const string &g) const {
      typedef vector<pair < string, string>>
      optionHelp;

      auto group = mHelp.find(g);
      if( group == mHelp.end()) {
        return "";
      }

      optionHelp format;

      size_t longest = 0;

      string result;

      if( !g.empty()) {
        result += toLocalString(" " + g + " options:\n");
      }

      for( const auto &o : group->second.options ) {
        if( mPositionalSet.find(o.l) != mPositionalSet.end() && !mShowPositional ) {
          continue;
        }

        auto s = formatOption(o);
        longest = (max)(longest, stringLength(s));
        format.push_back(make_pair(s, string()));
      }

      longest = (min)(longest, static_cast<size_t>(optionLongest));

      //widest allowed description
      auto allowed = size_t {76} - longest - optionDescGap;

      auto fIter = format.begin();
      for( const auto &o : group->second.options ) {
        if( mPositionalSet.find(o.l) != mPositionalSet.end() && !mShowPositional ) {
          continue;
        }

        auto d = formatDescription(o, longest + optionDescGap, allowed);

        result += fIter->first;
        if( stringLength(fIter->first) > longest ) {
          result += '\n';
          result += toLocalString(string(longest + optionDescGap, ' '));
        } else {
          result += toLocalString(string(longest + optionDescGap - stringLength(fIter->first), ' '));
        }
        result += d;
        result += '\n';

        ++fIter;
      }

      return result;
    }

    void cxxopts::Options::generateGroupHelp(string &result, const vector<string> &printGroups) const {
      for( size_t i = 0; i != printGroups.size(); ++i ) {
        const string &groupHelpText = helpOneGroup(printGroups[i]);
        if( empty(groupHelpText)) {
          continue;
        }
        result += groupHelpText;
        if( i < printGroups.size() - 1 ) {
          result += '\n';
        }
      }
    }

    void cxxopts::Options::generateAllGroupsHelp(string &result) const {
      vector<string> allGroups;
      allGroups.reserve(mHelp.size());

      for( auto &group : mHelp ) {
        allGroups.push_back(group.first);
      }

      generateGroupHelp(result, allGroups);
    }

    string cxxopts::Options::help(const vector<string> &helpGroups) const {
      string result = mHelpString + "\nUsage:\n  " + toLocalString(mProgram) + " " + toLocalString(mCustomHelp);

      if( !mPositional.empty() && !mPositionalHelp.empty()) {
        result += " " + toLocalString(mPositionalHelp);
      }

      result += "\n\n";

      if( helpGroups.empty()) {
        generateAllGroupsHelp(result);
      } else {
        generateGroupHelp(result, helpGroups);
      }

      return toUtf8String(result);
    }

    vector<string> cxxopts::Options::groups() const {
      vector<string> g;

      transform(mHelp.begin(), mHelp.end(), back_inserter(g), [](const map<string, HelpGroupDetails>::value_type &pair) {
          return pair.first;
      });

      return g;
    }

    const cxxopts::HelpGroupDetails &cxxopts::Options::groupHelp(const string &group) const {
      return mHelp.at(group);
    }
}