#include "capturestate.hpp"

#include "../defines.hpp"
#include "../onthepitch/match.hpp"

#include <boost/uuid/detail/sha1.hpp>
#include <sstream>
#include <iomanip>

std::string CaptureMatchState(Match *match) {
  EnvState state("", false);
  match->ProcessState(&state);
  std::string raw = state.GetState();

  boost::uuids::detail::sha1 sha;
  sha.process_bytes(raw.data(), raw.size());
  boost::uuids::detail::sha1::digest_type digest;
  sha.get_digest(digest);

  std::ostringstream oss;
  for (int i = 0; i < 20; i++) {
    oss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
  }
  return oss.str();
}
