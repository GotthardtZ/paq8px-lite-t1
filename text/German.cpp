#include "German.hpp"

auto German::isAbbreviation(Word *w) -> bool { return w->matchesAny(abbreviations, numAbbrev); }
