#ifndef TINYIM_UTIL_INITIALIZE_H_
#define TINYIM_UTIL_INITIALIZE_H_

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <cstdlib>
#include <ctime>

namespace tinyim {

// extern const int16_t sso_port;
// extern const char* sso_addr;

// extern const int16_t sso_redis_port;
// extern const char* sso_redis_addr;

// extern const int16_t sso_mysql_port;
// extern const char* sso_mysql_addr;

// extern const char* access_addr;
// extern const int16_t access_port;

// extern const char* idgen_addr;
// extern const int16_t idgen_port;

class Initialize final {
 public:
  Initialize(int& argc, char** argv[]) {
    google::InitGoogleLogging((*argv)[0]);
    google::SetStderrLogging(google::GLOG_INFO);
    FLAGS_colorlogtostderr = true;

    gflags::ParseCommandLineFlags(&argc, argv, true);

    LOG(INFO) << "Initialize";

    std::srand(std::time(nullptr));

    // if (::signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
      // LOG(WARNING) << "set sigpipe to sig_ign error";
    // }
    // else {
      // LOG(INFO) << "ignore sigpipe";
    // }

#ifndef NDEBUG
    // ::event_enable_debug_logging(EVENT_DBG_ALL);
#endif
  }

  ~Initialize() {
    LOG(INFO) << "~Initialize";

    google::ShutdownGoogleLogging();
  }
};

// void SigintCallback(evutil_socket_t fd, short event, void *arg);

}  // namespace tinyim

#endif // TINYIM_UTIL_INITIALIZE_H_
