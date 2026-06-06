#include "PCSCReader.h"

#ifdef WITH_NFC

#include <atomic>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <winscard.h>
#elif defined(__APPLE__)
#include <PCSC/winscard.h>
#include <PCSC/wintypes.h>
#else
#include <winscard.h>
#include <wintypes.h>
#endif

namespace PCSCReader {

static std::thread g_readerThread;
static std::atomic<bool> g_exitRequested(false);
static SCARDCONTEXT g_cardContext = 0;

static std::mutex g_pendingCardsMutex;
static std::queue<std::string> g_pendingCards;

static std::string bytesToHex(const unsigned char* bytes, DWORD len) {
  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  for (DWORD i = 0; i < len; ++i) {
    oss << std::setw(2) << static_cast<int>(bytes[i]);
  }
  return oss.str();
}

static std::string pcscErrorToString(LONG status) {
#ifdef _WIN32
  std::ostringstream oss;
  oss << "0x" << std::hex << std::setw(8) << std::setfill('0') << static_cast<unsigned long>(status);
  return oss.str();
#else
  return pcsc_stringify_error(status);
#endif
}

static void NfcThread() {
  LONG status =
      SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &g_cardContext);
  if (status != SCARD_S_SUCCESS) {
    g_cardContext = 0;
    return;
  }

  std::vector<char> readersBuffer;
  DWORD bufferSize = 0;

  SCardListReaders(g_cardContext, NULL, NULL, &bufferSize);
  if (bufferSize > 0) {
    readersBuffer.resize(bufferSize);
    SCardListReaders(g_cardContext, NULL, readersBuffer.data(), &bufferSize);
  }

  std::vector<SCARD_READERSTATE> readerStates;
  if (bufferSize > 0) {
    char* reader = readersBuffer.data();
    while (*reader != '\0') {
      SCARD_READERSTATE state = {};
      state.szReader = reader;
      state.dwCurrentState = SCARD_STATE_UNAWARE;
      readerStates.push_back(state);
      reader += strlen(reader) + 1;
    }
  }

  if (readerStates.empty()) {
    SCardReleaseContext(g_cardContext);
    g_cardContext = 0;
    return;
  }

  while (!g_exitRequested) {
    LONG status = SCardGetStatusChange(
        g_cardContext, INFINITE, readerStates.data(), readerStates.size());

    if (status != SCARD_S_SUCCESS) {
      break;
    }

    for (auto& state : readerStates) {
      if (!(state.dwEventState & SCARD_STATE_CHANGED)) {
        continue;
      }

      bool cardInserted = (state.dwEventState & SCARD_STATE_PRESENT) &&
                          !(state.dwCurrentState & SCARD_STATE_PRESENT);
      state.dwCurrentState = state.dwEventState & ~SCARD_STATE_CHANGED;
      if (!cardInserted) {
        continue;
      }

      SCARDHANDLE cardHandle = 0;
      DWORD activeProtocol = 0;
      LONG connectStatus = SCardConnect(
          g_cardContext, state.szReader, SCARD_SHARE_SHARED,
          SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &cardHandle,
          &activeProtocol);
      if (connectStatus != SCARD_S_SUCCESS) {
        continue;
      }

      unsigned char apdu[] = {0xFF, 0xCA, 0x00, 0x00, 0x00};
      unsigned char response[256];
      DWORD responseLen = sizeof(response);
      const SCARD_IO_REQUEST* sendPci =
          (activeProtocol == SCARD_PROTOCOL_T1) ? SCARD_PCI_T1 : SCARD_PCI_T0;

      LONG transmitStatus = SCardTransmit(
          cardHandle, sendPci, apdu, sizeof(apdu), NULL, response,
          &responseLen);

      if (transmitStatus != SCARD_S_SUCCESS) {
        std::cerr << "SCardTransmit failed: " << pcscErrorToString(transmitStatus) << std::endl;
      } else if (responseLen >= 2) {
        unsigned char sw1 = response[responseLen - 2];
        unsigned char sw2 = response[responseLen - 1];
        if (sw1 == 0x90 && sw2 == 0x00) {
          std::string cardId = bytesToHex(response, responseLen - 2);
          std::lock_guard<std::mutex> lock(g_pendingCardsMutex);
          g_pendingCards.push(cardId);
        } else {
          std::cerr << "Card returned status word: "
                    << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(sw1)
                    << std::setw(2) << static_cast<int>(sw2) << std::dec << std::endl;
        }
      }
      SCardDisconnect(cardHandle, SCARD_LEAVE_CARD);
    }
  }

  if (g_cardContext != 0) {
    SCardReleaseContext(g_cardContext);
    g_cardContext = 0;
  }
}

void Start() {
  g_exitRequested = false;
  g_readerThread = std::thread(NfcThread);
}

std::optional<std::string> PollCardId() {
  std::lock_guard<std::mutex> lock(g_pendingCardsMutex);
  if (g_pendingCards.empty()) {
    return std::nullopt;
  }
  std::string cardId = g_pendingCards.front();
  g_pendingCards.pop();
  return cardId;
}

void Stop() {
  g_exitRequested = true;
  if (g_cardContext != 0) {
    SCardCancel(g_cardContext);
  }
  if (g_readerThread.joinable()) {
    g_readerThread.join();
  }
}

}  // namespace PCSCReader

#else  // WITH_NFC

namespace PCSCReader {
void Start() {}
void Stop() {}
std::optional<std::string> PollCardId() { return std::nullopt; }
}  // namespace PCSCReader

#endif  // WITH_NFC
