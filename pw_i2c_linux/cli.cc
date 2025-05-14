// Copyright 2025 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "pw_assert/check.h"
#include "pw_i2c_linux/initiator.h"
#include "pw_log/log.h"
#include "pw_preprocessor/util.h"
#include "pw_result/result.h"
#include "pw_status/status.h"

namespace pw::i2c {
namespace {

const struct option kLongOptions[] = {
    {"addr10", required_argument, nullptr, 'A'},
    {"address", required_argument, nullptr, 'a'},
    {"device", required_argument, nullptr, 'D'},
    {"human", no_argument, nullptr, 'h'},
    {"input", required_argument, nullptr, 'i'},
    {"lsb", no_argument, nullptr, 'l'},
    {"output", required_argument, nullptr, 'o'},
    {"rx-count", required_argument, nullptr, 'r'},
    {},  // terminator
};

// Starts with ':' to skip printing errors and return ':' for a missing option
// argument.
// const char* kShortOptions = ":b:D:F:hi:lm:o:r:";
const char* kShortOptions = ":a:A:D:hi:lm:o:r:";

void Usage() {
  std::cerr << "Usage: pw_i2c_linux_cli -D DEVICE -A|-a ADDR [flags]"
            << std::endl;
  std::cerr << "Required flags:" << std::endl;
  std::cerr << "  -A/--addr10   Target address, 0x prefix allowed (10-bit i2c "
               "extension)"
            << std::endl;
  std::cerr << "  -a/--address  Target address, 0x prefix allowed (7-bit "
               "standard i2c)"
            << std::endl;
  std::cerr << "  -D/--device   I2C device path (e.g. /dev/i2c-0" << std::endl;
  std::cerr << std::endl;
  std::cerr << "Optional flags:" << std::endl;
  std::cerr << "  -h/--human    Human-readable output (default: binary, "
               "unless output to stdout tty)"
            << std::endl;
  std::cerr << "  -i/--input    Input file, or - for stdin" << std::endl;
  std::cerr << "                If not given, no data is sent." << std::endl;
  std::cerr << "  -l/--lsb      LSB first (default: MSB first)" << std::endl;
  std::cerr << "  -o/--output   Output file (default: stdout)" << std::endl;
  std::cerr << "  -r/--rx-count Number of bytes to receive (defaults to size "
               "of input)"
            << std::endl;
}

struct Args {
  std::string device;
  std::optional<std::string> input_path;
  std::string output_path = "-";
  bool human_readable = false;
  std::optional<unsigned int> rx_count;
  std::optional<Address> address;

  bool lsb_first = false;
};

template <class T>
std::optional<T> ParseNumber(std::string_view str) {
  std::string _str(str);
  errno = 0;
  long value = std::strtol(_str.c_str(), nullptr, 0);
  if (errno != 0) {
    PW_LOG_ERROR("Unable to parse param: [%s] error %d", _str.c_str(), errno);
    return std::nullopt;
  }

  if (value >= std::numeric_limits<T>::min() &&
      value <= std::numeric_limits<T>::max()) {
    return value;
  }
  PW_LOG_ERROR("Value is out of range: %ld", value);
  return std::nullopt;
}

Result<Args> ParseArgs(int argc, char* argv[]) {
  Args args;
  bool human_readable_given;

  while (true) {
    int current_optind = optind;
    int c = getopt_long(argc, argv, kShortOptions, kLongOptions, nullptr);
    if (c == -1) {
      break;
    }

    switch (c) {
      case 'A': {
        if (args.address.has_value()) {
          PW_LOG_ERROR("Must specify exactly one of -A|-a");
          return Status::InvalidArgument();
        }
        std::optional<uint16_t> addr10 = ParseNumber<uint16_t>(optarg);
        if (!addr10) {
          PW_LOG_ERROR("Invalid 10-bit i2c address: %s", optarg);
          return Status::InvalidArgument();
        }
        args.address = Address::TenBit(addr10.value());
        break;
      }
      case 'a': {
        if (args.address.has_value()) {
          PW_LOG_ERROR("Must specify exactly one of -A|-a");
          return Status::InvalidArgument();
        }
        std::optional<uint8_t> addr = ParseNumber<uint8_t>(optarg);
        if (!addr) {
          PW_LOG_ERROR("Invalid 7-bit i2c address: %s", optarg);
          return Status::InvalidArgument();
        }
        args.address = Address::SevenBit(addr.value());
        break;
      }
      case 'D':
        args.device = optarg;
        break;
      case 'h':
        human_readable_given = true;
        break;
      case 'i':
        args.input_path = optarg;
        break;
      case 'l':
        args.lsb_first = true;
        break;
      case 'o':
        args.output_path = optarg;
        break;
      case 'r': {
        auto count = ParseNumber<unsigned int>(optarg);
        if (!count) {
          PW_LOG_ERROR("Invalid count: %s", optarg);
          return Status::InvalidArgument();
        }
        args.rx_count = count;
        break;
      }
      case '?':
        if (optopt) {
          PW_LOG_ERROR("Invalid flag: -%c", optopt);
        } else {
          PW_LOG_ERROR("Invalid flag: %s", argv[current_optind]);
        }
        Usage();
        return Status::InvalidArgument();
      case ':':
        PW_LOG_ERROR("Missing argument to %s", argv[current_optind]);
        return Status::InvalidArgument();
    }
  }

  args.human_readable = human_readable_given ||
                        (args.output_path == "-" && isatty(STDOUT_FILENO));

  // Check for required flags
  if (args.device.empty()) {
    PW_LOG_ERROR("Missing required flag: -D/--device");
    Usage();
    return Status::InvalidArgument();
  }

  // Check for required flags
  if (!args.address.has_value()) {
    PW_LOG_ERROR("Missing required flag: -A|-a ADDR");
    Usage();
    return Status::InvalidArgument();
  }

  // Either input file or rx count must be provided
  if (!args.input_path && !args.rx_count) {
    PW_LOG_ERROR("Either -i/--input or -r/--rx must be provided.");
    return Status::InvalidArgument();
  }

  return args;
}

std::vector<std::byte> ReadInput(const std::string& path, size_t limit) {
  std::ifstream input_file;
  if (path != "-") {
    input_file.open(path, std::ifstream::in);
    if (!input_file.is_open()) {
      PW_LOG_ERROR("Failed to open %s", path.c_str());
      exit(2);
    }
  }
  std::istream& instream = input_file.is_open() ? input_file : std::cin;

  std::vector<std::byte> result;
  for (size_t i = 0; i < limit; i++) {
    int b = instream.get();
    if (b == EOF) {
      break;
    }
    result.push_back(static_cast<std::byte>(b));
  }

  return result;
}

void WriteOutput(const std::string& path,
                 std::vector<std::byte> data,
                 bool human_readable) {
  std::ofstream output_file;
  if (path != "-") {
    output_file.open(path, std::ifstream::out);
    if (!output_file.is_open()) {
      PW_LOG_ERROR("Failed to open %s", path.c_str());
      exit(2);
    }
  }
  std::ostream& out = output_file.is_open() ? output_file : std::cout;

  if (human_readable) {
    out << '"';
  }

  for (std::byte b : data) {
    char c = static_cast<char>(b);
    if (!human_readable || std::isprint(c)) {
      out.put(c);
    } else if (c == '\0') {
      out << "\\0";
    } else if (c == '\n') {
      out << "\\n";
    } else {
      out << "\\x" << std::hex << std::setfill('0') << std::setw(2)
          << static_cast<unsigned int>(c);
    }
  }

  if (human_readable) {
    out << '"' << std::endl;
  }
}

int MainInNamespace(int argc, char* argv[]) {
  auto maybe_args = ParseArgs(argc, argv);
  if (!maybe_args.ok()) {
    return 1;
  }
  auto args = std::move(maybe_args.value());

  pw::Result<int> fd = LinuxInitiator::OpenI2cBus(args.device.c_str());
  if (!fd.ok()) {
    PW_LOG_ERROR("Failed to open %s: %s", args.device.c_str(), strerror(errno));
    return 1;
  }
  PW_LOG_DEBUG("Opened %s", args.device.c_str());

  LinuxInitiator initiator(fd.value());

  // Read input data for transmit.
  std::vector<std::byte> tx_data;
  if (args.input_path) {
    tx_data = ReadInput(args.input_path.value(), 1024);
  }

  // Set up receive buffer.
  std::vector<std::byte> rx_data(args.rx_count ? args.rx_count.value() : 0);

  // Perform a transfer!
  PW_LOG_DEBUG(
      "Ready to send %zu, receive %zu bytes", tx_data.size(), rx_data.size());

  PW_CHECK(args.address.has_value());
  std::vector<Message> messages;
  if (!tx_data.empty()) {
    messages.push_back(Message::WriteMessage(args.address.value(), tx_data));
  }

  if (!rx_data.empty()) {
    messages.push_back(Message::ReadMessage(args.address.value(), rx_data));
  }

  constexpr auto kTimeout =
      pw::chrono::SystemClock::for_at_least(std::chrono::milliseconds(500));

  if (auto status = initiator.TransferFor(messages, kTimeout); !status.ok()) {
    PW_LOG_ERROR("Failed to transfer data: %s", status.str());
    return 2;
  }
  PW_LOG_DEBUG("Transfer successful! tx_bytes=%zu rx_bytes=%zu",
               tx_data.size(),
               rx_data.size());

  WriteOutput(args.output_path, rx_data, args.human_readable);

  close(fd.value());

  return 0;
}

}  // namespace
}  // namespace pw::i2c

int main(int argc, char* argv[]) {
  return pw::i2c::MainInNamespace(argc, argv);
}
