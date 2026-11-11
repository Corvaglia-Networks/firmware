#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "supervisor.h"

int main(int argc, char** argv) {
  absl::InitializeLog();
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);

  if (argc < 2) {
    LOG(ERROR) << "Usage: hardwared <tile.elf>";
    return 1;
  }

  Supervisor supervisor(argv[1]);
  return supervisor.Run();
}