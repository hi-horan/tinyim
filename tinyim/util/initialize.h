#ifndef TINYIM_UTIL_INITIALIZE_H_
#define TINYIM_UTIL_INITIALIZE_H_

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <cstdlib>
#include <ctime>

namespace tinyim {

class Initialize final {
 public:
  Initialize(int& argc, char** argv[]) {
    google::InitGoogleLogging((*argv)[0]);
    google::SetStderrLogging(google::GLOG_INFO);
    FLAGS_colorlogtostderr = true;

    gflags::ParseCommandLineFlags(&argc, argv, true);

    LOG(INFO) << "Initialize";

    std::srand(std::time(nullptr));

#ifndef NDEBUG
    // ::event_enable_debug_logging(EVENT_DBG_ALL);
#endif
  }

  ~Initialize() {
    LOG(INFO) << "~Initialize";

    google::ShutdownGoogleLogging();
  }
};

}  // namespace tinyim

#endif // TINYIM_UTIL_INITIALIZE_H_
