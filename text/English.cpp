#include "English.hpp"

auto English::isAbbreviation(Word *w) -> bool { return w->matchesAny(abbreviations, numAbbrev); }
