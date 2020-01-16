#include "cxxopts.hpp"

#include <utility>

cxxopts::OptionException::OptionException(std::string message) : mMessage(std::move(message)) {
}

const char *cxxopts::OptionException::what() const noexcept {
  return mMessage.c_str();
}

cxxopts::OptionSpecException::OptionSpecException(const std::string &message) : OptionException(message) {
}

cxxopts::OptionParseException::OptionParseException(const std::string &message) : OptionException(message) {
}

cxxopts::OptionExistsError::OptionExistsError(const std::string &option) : OptionSpecException(
        "Option " + leftQuote + option + rightQuote + " already exists") {
}

cxxopts::InvalidOptionFormatError::InvalidOptionFormatError(const std::string &format) : OptionSpecException(
        "Invalid option format " + leftQuote + format + rightQuote) {
}

cxxopts::OptionSyntaxException::OptionSyntaxException(const std::string &text) : OptionParseException(
        "Argument " + leftQuote + text + rightQuote + " starts with a - but has incorrect syntax") {
}

cxxopts::OptionNotExistsException::OptionNotExistsException(const std::string &option) : OptionParseException(
        "Option " + leftQuote + option + rightQuote + " does not exist") {
}

cxxopts::MissingArgumentException::MissingArgumentException(const std::string &option) : OptionParseException(
        "Option " + leftQuote + option + rightQuote + " is missing an argument") {
}

cxxopts::OptionRequiresArgumentException::OptionRequiresArgumentException(const std::string &option) : OptionParseException(
        "Option " + leftQuote + option + rightQuote + " requires an argument") {
}

cxxopts::OptionNotPresentException::OptionNotPresentException(const std::string &option) : OptionParseException(
        "Option " + leftQuote + option + rightQuote + " not present") {
}

cxxopts::ArgumentIncorrectType::ArgumentIncorrectType(const std::string &arg) : OptionParseException(
        "Argument " + leftQuote + arg + rightQuote + " failed to parse") {
}

cxxopts::OptionDetails::OptionDetails(std::string s, std::string l, cxxopts::String desc, std::shared_ptr<const Value> val) : mShort(
        std::move(s)), mLong(std::move(l)), mDesc(std::move(desc)), mValue(std::move(val)), mCount(0) {
}

cxxopts::OptionDetails::OptionDetails(const cxxopts::OptionDetails &rhs) : mDesc(rhs.mDesc), mCount(rhs.mCount) {
  mValue = rhs.mValue->clone();
}

const cxxopts::String &cxxopts::OptionDetails::description() const {
  return mDesc;
}

const cxxopts::Value &cxxopts::OptionDetails::value() const {
  return *mValue;
}

std::shared_ptr<cxxopts::Value> cxxopts::OptionDetails::makeStorage() const {
  return mValue->clone();
}

const std::string &cxxopts::OptionDetails::shortName() const {
  return mShort;
}

const std::string &cxxopts::OptionDetails::longName() const {
  return mLong;
}

void cxxopts::OptionValue::parse(const std::shared_ptr<const OptionDetails> &details, const std::string &text) {
  ensureValue(details);
  ++mCount;
  mValue->parse(text);
}

void cxxopts::OptionValue::parseDefault(const std::shared_ptr<const OptionDetails> &details) {
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

void cxxopts::OptionValue::ensureValue(const std::shared_ptr<const OptionDetails> &details) {
  if( mValue == nullptr ) {
    mValue = details->makeStorage();
  }
}

cxxopts::KeyValue::KeyValue(std::string key, std::string value) : mKey(std::move(key)), mValue(std::move(value)) {
}

const std::string &cxxopts::KeyValue::key() const {
  return mKey;
}

const std::string &cxxopts::KeyValue::value() const {
  return mValue;
}

cxxopts::Option::Option(std::string opts, std::string desc, std::shared_ptr<const Value> value, std::string argHelp) : opts(
        std::move(opts)), desc(std::move(desc)), value_(std::move(value)), argHelp(std::move(argHelp)) {
}

cxxopts::ParseResult::ParseResult(std::shared_ptr<std::unordered_map<std::string, std::shared_ptr<OptionDetails>>> options,
                                  std::vector<std::string> positional, bool allowUnrecognised, int &argc, char **&argv) : mOptions(
        std::move(options)), mPositional(std::move(positional)), mNextPositional(mPositional.begin()),
        mAllowUnrecognised(allowUnrecognised) {
  parse(argc, argv);
}

void cxxopts::Options::addOptions(const std::string &group, std::initializer_list<Option> options) {
  OptionAdder optionAdder(*this, group);
  for( const auto &option: options ) {
    optionAdder(option.opts, option.desc, option.value_, option.argHelp);
  }
}

cxxopts::OptionAdder cxxopts::Options::addOptions(std::string group) {
  return OptionAdder(*this, std::move(group));
}

cxxopts::OptionAdder &
cxxopts::OptionAdder::operator()(const std::string &opts, const std::string &desc, const std::shared_ptr<const Value> &value,
                                 std::string argHelp) {
  std::match_results<const char *> result;
  std::regex_match(opts.c_str(), result, optionSpecifier);

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

  auto optionNames = [](const std::sub_match<const char *> &short_, const std::sub_match<const char *> &long_) {
      if( long_.length() == 1 ) {
        return std::make_tuple(long_.str(), short_.str());
      } else {
        return std::make_tuple(short_.str(), long_.str());
      }
  }(shortMatch, longMatch);

  mOptions.addOption(mGroup, std::get<0>(optionNames), std::get<1>(optionNames), desc, value, std::move(argHelp));

  return *this;
}

void cxxopts::ParseResult::parseDefault(const std::shared_ptr<OptionDetails> &details) {
  mResults[details].parseDefault(details);
}

void cxxopts::ParseResult::parseOption(const std::shared_ptr<OptionDetails> &value, const std::string & /*name*/, const std::string &arg) {
  auto &result = mResults[value];
  result.parse(value, arg);

  mSequential.emplace_back(value->longName(), arg);
}

void cxxopts::ParseResult::checkedParseArg(int argc, char **argv, int &current, const std::shared_ptr<OptionDetails> &value,
                                           const std::string &name) {
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

void cxxopts::ParseResult::addToOption(const std::string &option, const std::string &arg) {
  auto iter = mOptions->find(option);

  if( iter == mOptions->end()) {
    throwOrMimic<OptionNotExistsException>(option);
  }

  parseOption(iter->second, option, arg);
}

bool cxxopts::ParseResult::consumePositional(const std::string &a) {
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

void cxxopts::Options::parsePositional(std::string option) {
  parsePositional(std::vector<std::string> {std::move(option)});
}

void cxxopts::Options::parsePositional(std::vector<std::string> options) {
  mPositional = std::move(options);
  mNextPositional = mPositional.begin();

  mPositionalSet.insert(mPositional.begin(), mPositional.end());
}

void cxxopts::Options::parsePositional(std::initializer_list<std::string> options) {
  parsePositional(std::vector<std::string>(options));
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

    std::match_results<const char *> result;
    std::regex_match(argv[current], result, optionMatcher);

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
        const std::string &s = result[4];

        for( std::size_t i = 0; i != s.size(); ++i ) {
          std::string name(1, s[i]);
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
        const std::string &name = result[1];

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

void cxxopts::Options::addOption(const std::string &group, const Option &option) {
  addOptions(group, {option});
}

void cxxopts::Options::addOption(const std::string &group, const std::string &s, const std::string &l, std::string desc,
                                 const std::shared_ptr<const Value> &value, std::string argHelp) {
  auto stringDesc = toLocalString(std::move(desc));
  auto option = std::make_shared<OptionDetails>(s, l, stringDesc, value);

  if( !s.empty()) {
    addOneOption(s, option);
  }

  if( !l.empty()) {
    addOneOption(l, option);
  }

  //add the help details
  auto &options = mHelp[group];

  options.options.emplace_back(HelpOptionDetails {s, l, stringDesc, value->hasDefault(), value->getDefaultValue(), value->hasImplicit(),
                                                  value->getImplicitValue(), std::move(argHelp), value->isContainer(), value->isBoolean()});
}

void cxxopts::Options::addOneOption(const std::string &option, const std::shared_ptr<OptionDetails> &details) {
  auto in = mOptions->emplace(option, details);

  if( !in.second ) {
    throwOrMimic<OptionExistsError>(option);
  }
}

cxxopts::String cxxopts::Options::helpOneGroup(const std::string &g) const {
  typedef std::vector<std::pair<String, String>> optionHelp;

  auto group = mHelp.find(g);
  if( group == mHelp.end()) {
    return "";
  }

  optionHelp format;

  size_t longest = 0;

  String result;

  if( !g.empty()) {
    result += toLocalString(" " + g + " options:\n");
  }

  for( const auto &o : group->second.options ) {
    if( mPositionalSet.find(o.l) != mPositionalSet.end() && !mShowPositional ) {
      continue;
    }

    auto s = formatOption(o);
    longest = (std::max)(longest, stringLength(s));
    format.push_back(std::make_pair(s, String()));
  }

  longest = (std::min)(longest, static_cast<size_t>(optionLongest));

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
      result += toLocalString(std::string(longest + optionDescGap, ' '));
    } else {
      result += toLocalString(std::string(longest + optionDescGap - stringLength(fIter->first), ' '));
    }
    result += d;
    result += '\n';

    ++fIter;
  }

  return result;
}

void cxxopts::Options::generateGroupHelp(String &result, const std::vector<std::string> &printGroups) const {
  for( size_t i = 0; i != printGroups.size(); ++i ) {
    const String &groupHelpText = helpOneGroup(printGroups[i]);
    if( empty(groupHelpText)) {
      continue;
    }
    result += groupHelpText;
    if( i < printGroups.size() - 1 ) {
      result += '\n';
    }
  }
}

void cxxopts::Options::generateAllGroupsHelp(String &result) const {
  std::vector<std::string> allGroups;
  allGroups.reserve(mHelp.size());

  for( auto &group : mHelp ) {
    allGroups.push_back(group.first);
  }

  generateGroupHelp(result, allGroups);
}

std::string cxxopts::Options::help(const std::vector<std::string> &helpGroups) const {
  String result = mHelpString + "\nUsage:\n  " + toLocalString(mProgram) + " " + toLocalString(mCustomHelp);

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

std::vector<std::string> cxxopts::Options::groups() const {
  std::vector<std::string> g;

  std::transform(mHelp.begin(), mHelp.end(), std::back_inserter(g), [](const std::map<std::string, HelpGroupDetails>::value_type &pair) {
      return pair.first;
  });

  return g;
}

const cxxopts::HelpGroupDetails &cxxopts::Options::groupHelp(const std::string &group) const {
  return mHelp.at(group);
}