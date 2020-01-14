#include "cxxopts.hpp"

#include <utility>

cxxopts::OptionException::OptionException(std::string message) : m_message(std::move(message)) {
}

const char *cxxopts::OptionException::what() const noexcept {
  return m_message.c_str();
}

cxxopts::OptionSpecException::OptionSpecException(const std::string &message) : OptionException(message) {
}

cxxopts::OptionParseException::OptionParseException(const std::string &message) : OptionException(message) {
}

cxxopts::OptionExistsError::OptionExistsError(const std::string &option) : OptionSpecException(
        "Option " + LQUOTE + option + RQUOTE + " already exists") {
}

cxxopts::InvalidOptionFormatError::InvalidOptionFormatError(const std::string &format) : OptionSpecException(
        "Invalid option format " + LQUOTE + format + RQUOTE) {
}

cxxopts::OptionSyntaxException::OptionSyntaxException(const std::string &text) : OptionParseException(
        "Argument " + LQUOTE + text + RQUOTE + " starts with a - but has incorrect syntax") {
}

cxxopts::OptionNotExistsException::OptionNotExistsException(const std::string &option) : OptionParseException(
        "Option " + LQUOTE + option + RQUOTE + " does not exist") {
}

cxxopts::MissingArgumentException::MissingArgumentException(const std::string &option) : OptionParseException(
        "Option " + LQUOTE + option + RQUOTE + " is missing an argument") {
}

cxxopts::OptionRequiresArgumentException::OptionRequiresArgumentException(const std::string &option) : OptionParseException(
        "Option " + LQUOTE + option + RQUOTE + " requires an argument") {
}

cxxopts::OptionNotPresentException::OptionNotPresentException(const std::string &option) : OptionParseException(
        "Option " + LQUOTE + option + RQUOTE + " not present") {
}

cxxopts::ArgumentIncorrectType::ArgumentIncorrectType(const std::string &arg) : OptionParseException(
        "Argument " + LQUOTE + arg + RQUOTE + " failed to parse") {
}

cxxopts::OptionDetails::OptionDetails(std::string short_, std::string long_, cxxopts::String desc, std::shared_ptr<const Value> val)
        : m_short(std::move(short_)), m_long(std::move(long_)), m_desc(std::move(desc)), m_value(std::move(val)), m_count(0) {
}

cxxopts::OptionDetails::OptionDetails(const cxxopts::OptionDetails &rhs) : m_desc(rhs.m_desc), m_count(rhs.m_count) {
  m_value = rhs.m_value->clone();
}

const cxxopts::String &cxxopts::OptionDetails::description() const {
  return m_desc;
}

const cxxopts::Value &cxxopts::OptionDetails::value() const {
  return *m_value;
}

std::shared_ptr<cxxopts::Value> cxxopts::OptionDetails::makeStorage() const {
  return m_value->clone();
}

const std::string &cxxopts::OptionDetails::shortName() const {
  return m_short;
}

const std::string &cxxopts::OptionDetails::longName() const {
  return m_long;
}

void cxxopts::OptionValue::parse(const std::shared_ptr<const OptionDetails> &details, const std::string &text) {
  ensureValue(details);
  ++m_count;
  m_value->parse(text);
}

void cxxopts::OptionValue::parseDefault(const std::shared_ptr<const OptionDetails> &details) {
  ensureValue(details);
  m_default = true;
  m_value->parse();
}

size_t cxxopts::OptionValue::count() const noexcept {
  return m_count;
}

bool cxxopts::OptionValue::hasDefault() const noexcept {
  return m_default;
}

void cxxopts::OptionValue::ensureValue(const std::shared_ptr<const OptionDetails> &details) {
  if( m_value == nullptr ) {
    m_value = details->makeStorage();
  }
}

cxxopts::KeyValue::KeyValue(std::string key, std::string value) : m_key(std::move(key)), m_value(std::move(value)) {
}

const std::string &cxxopts::KeyValue::key() const {
  return m_key;
}

const std::string &cxxopts::KeyValue::value() const {
  return m_value;
}

cxxopts::Option::Option(std::string opts, std::string desc, std::shared_ptr<const Value> value, std::string argHelp) : opts_(
        std::move(opts)), desc_(std::move(desc)), value_(std::move(value)), arg_help_(std::move(argHelp)) {
}

cxxopts::ParseResult::ParseResult(std::shared_ptr<std::unordered_map<std::string, std::shared_ptr<OptionDetails>>> options,
                                  std::vector<std::string> positional, bool allowUnrecognised, int &argc, char **&argv) : m_options(
        std::move(options)), m_positional(std::move(positional)), m_next_positional(m_positional.begin()),
        m_allow_unrecognised(allowUnrecognised) {
  parse(argc, argv);
}

void cxxopts::Options::addOptions(const std::string &group, std::initializer_list<Option> options) {
  OptionAdder optionAdder(*this, group);
  for( const auto &option: options ) {
    optionAdder(option.opts_, option.desc_, option.value_, option.arg_help_);
  }
}

cxxopts::OptionAdder cxxopts::Options::addOptions(std::string group) {
  return OptionAdder(*this, std::move(group));
}

cxxopts::OptionAdder &cxxopts::OptionAdder::operator()(const std::string &opts, const std::string &desc, const std::shared_ptr<const Value>& value,
                                                       std::string argHelp) {
  std::match_results<const char *> result;
  std::regex_match(opts.c_str(), result, option_specifier);

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

  m_options.addOption(m_group, std::get<0>(optionNames), std::get<1>(optionNames), desc, value, std::move(argHelp));

  return *this;
}

void cxxopts::ParseResult::parseDefault(const std::shared_ptr<OptionDetails> &details) {
  m_results[details].parseDefault(details);
}

void cxxopts::ParseResult::parseOption(const std::shared_ptr<OptionDetails> &value, const std::string & /*name*/, const std::string &arg) {
  auto &result = m_results[value];
  result.parse(value, arg);

  m_sequential.emplace_back(value->longName(), arg);
}

void cxxopts::ParseResult::checkedParseArg(int argc, char **argv, int &current, const std::shared_ptr<OptionDetails> &value,
                                           const std::string &name) {
  if( current + 1 >= argc ) {
    if( value->value().has_implicit()) {
      parseOption(value, name, value->value().get_implicit_value());
    } else {
      throwOrMimic<MissingArgumentException>(name);
    }
  } else {
    if( value->value().has_implicit()) {
      parseOption(value, name, value->value().get_implicit_value());
    } else {
      parseOption(value, name, argv[current + 1]);
      ++current;
    }
  }
}

void cxxopts::ParseResult::addToOption(const std::string &option, const std::string &arg) {
  auto iter = m_options->find(option);

  if( iter == m_options->end()) {
    throwOrMimic<OptionNotExistsException>(option);
  }

  parseOption(iter->second, option, arg);
}

bool cxxopts::ParseResult::consumePositional(const std::string& a) {
  while( m_next_positional != m_positional.end()) {
    auto iter = m_options->find(*m_next_positional);
    if( iter != m_options->end()) {
      auto &result = m_results[iter->second];
      if( !iter->second->value().is_container()) {
        if( result.count() == 0 ) {
          addToOption(*m_next_positional, a);
          ++m_next_positional;
          return true;
        } else {
          ++m_next_positional;
          continue;
        }
      } else {
        addToOption(*m_next_positional, a);
        return true;
      }
    } else {
      throwOrMimic<OptionNotExistsException>(*m_next_positional);
    }
  }

  return false;
}

void cxxopts::Options::parsePositional(std::string option) {
  parsePositional(std::vector<std::string> {std::move(option)});
}

void cxxopts::Options::parsePositional(std::vector<std::string> options) {
  m_positional = std::move(options);
  m_next_positional = m_positional.begin();

  m_positional_set.insert(m_positional.begin(), m_positional.end());
}

void cxxopts::Options::parsePositional(std::initializer_list<std::string> options) {
  parsePositional(std::vector<std::string>(options));
}

cxxopts::ParseResult cxxopts::Options::parse(int &argc, char **&argv) {
  ParseResult result(m_options, m_positional, m_allow_unrecognised, argc, argv);
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
    std::regex_match(argv[current], result, option_matcher);

    if( result.empty()) {
      //not a flag

      // but if it starts with a `-`, then it's an error
      if( argv[current][0] == '-' && argv[current][1] != '\0' ) {
        if( !m_allow_unrecognised ) {
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
          auto iter = m_options->find(name);

          if( iter == m_options->end()) {
            if( m_allow_unrecognised ) {
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
          } else if( value->value().has_implicit()) {
            parseOption(value, name, value->value().get_implicit_value());
          } else {
            //error
            throwOrMimic<OptionRequiresArgumentException>(name);
          }
        }
      } else if( result[1].length() != 0 ) {
        const std::string &name = result[1];

        auto iter = m_options->find(name);

        if( iter == m_options->end()) {
          if( m_allow_unrecognised ) {
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

  for( auto &opt : *m_options ) {
    auto &detail = opt.second;
    auto &value = detail->value();

    auto &store = m_results[detail];

    if( value.has_default() && !store.count() && !store.hasDefault()) {
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

  if( !s.empty() ) {
    addOneOption(s, option);
  }

  if( !l.empty() ) {
    addOneOption(l, option);
  }

  //add the help details
  auto &options = m_help[group];

  options.options.emplace_back(HelpOptionDetails {s, l, stringDesc, value->has_default(), value->get_default_value(), value->has_implicit(),
                                                  value->get_implicit_value(), std::move(argHelp), value->is_container(),
                                                  value->is_boolean()});
}

void cxxopts::Options::addOneOption(const std::string &option, const std::shared_ptr<OptionDetails> &details) {
  auto in = m_options->emplace(option, details);

  if( !in.second ) {
    throwOrMimic<OptionExistsError>(option);
  }
}

cxxopts::String cxxopts::Options::helpOneGroup(const std::string &g) const {
  typedef std::vector<std::pair<String, String>> optionHelp;

  auto group = m_help.find(g);
  if( group == m_help.end()) {
    return "";
  }

  optionHelp format;

  size_t longest = 0;

  String result;

  if( !g.empty()) {
    result += toLocalString(" " + g + " options:\n");
  }

  for( const auto &o : group->second.options ) {
    if( m_positional_set.find(o.l) != m_positional_set.end() && !m_show_positional ) {
      continue;
    }

    auto s = format_option(o);
    longest = (std::max)(longest, stringLength(s));
    format.push_back(std::make_pair(s, String()));
  }

  longest = (std::min)(longest, static_cast<size_t>(OPTION_LONGEST));

  //widest allowed description
  auto allowed = size_t {76} - longest - OPTION_DESC_GAP;

  auto fIter = format.begin();
  for( const auto &o : group->second.options ) {
    if( m_positional_set.find(o.l) != m_positional_set.end() && !m_show_positional ) {
      continue;
    }

    auto d = format_description(o, longest + OPTION_DESC_GAP, allowed);

    result += fIter->first;
    if( stringLength(fIter->first) > longest ) {
      result += '\n';
      result += toLocalString(std::string(longest + OPTION_DESC_GAP, ' '));
    } else {
      result += toLocalString(std::string(longest + OPTION_DESC_GAP - stringLength(fIter->first), ' '));
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
  allGroups.reserve(m_help.size());

  for( auto &group : m_help ) {
    allGroups.push_back(group.first);
  }

  generateGroupHelp(result, allGroups);
}

std::string cxxopts::Options::help(const std::vector<std::string> &helpGroups) const {
  String result = m_help_string + "\nUsage:\n  " + toLocalString(m_program) + " " + toLocalString(m_custom_help);

  if( !m_positional.empty() && !m_positional_help.empty()) {
    result += " " + toLocalString(m_positional_help);
  }

  result += "\n\n";

  if( helpGroups.empty()) {
    generateAllGroupsHelp(result);
  } else {
    generateGroupHelp(result, helpGroups);
  }

  return toUTF8String(result);
}

std::vector<std::string> cxxopts::Options::groups() const {
  std::vector<std::string> g;

  std::transform(m_help.begin(), m_help.end(), std::back_inserter(g), [](const std::map<std::string, HelpGroupDetails>::value_type &pair) {
      return pair.first;
  });

  return g;
}

const cxxopts::HelpGroupDetails &cxxopts::Options::groupHelp(const std::string &group) const {
  return m_help.at(group);
}