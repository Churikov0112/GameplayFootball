#ifndef _HPP_CAPTURESTATE
#define _HPP_CAPTURESTATE

#include <string>

class Match;

// Runs Match::ProcessState and returns a SHA-1 hash of the serialized state.
std::string CaptureMatchState(Match *match);

#endif
