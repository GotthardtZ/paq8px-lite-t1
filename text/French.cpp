#include "French.hpp"

auto French::isAbbreviation(Word *w) -> bool { return w->matchesAny(abbreviations, numAbbrev); }
