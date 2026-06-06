#ifndef PCSC_READER_H
#define PCSC_READER_H

#include <optional>
#include <string>

namespace PCSCReader {
void Start();
void Stop();
std::optional<std::string> PollCardId();
}  // namespace PCSCReader

#endif  // PCSC_READER_H
