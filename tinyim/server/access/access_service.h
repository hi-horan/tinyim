#ifndef TINYIM_ACCESS_ACCESS_SERVICE_H_
#define TINYIM_ACCESS_ACCESS_SERVICE_H_

#include "access.pb.h"

#include <mutex>
#include <unordered_map>

// #include <brpc/server.h>
// #include <butil/status.h>
#include <bthread/unstable.h>

#include "type.h"

namespace brpc {
class Channel;
class Controller;
}  // namespace brpc

namespace butil{
class Status;
}  // namespace butil

namespace tinyim {

struct HeartBeatTimeoutArg;

class AccessServiceImpl : public AccessService {
 public:
  AccessServiceImpl(brpc::Channel *logic_channel, brpc::Channel* db_channel);
  virtual ~AccessServiceImpl();

  void SignIn(google::protobuf::RpcController* controller,
              const SigninData*,
              Pong* reply,
              google::protobuf::Closure* done) override;

  void SignOut(google::protobuf::RpcController* controller,
               const UserId*,
               Pong* reply,
               google::protobuf::Closure* done) override;

  void SendMsg(google::protobuf::RpcController* controller,
               const NewMsg* new_msg,
               MsgReply* reply,
               google::protobuf::Closure* done) override;

  void PullData(google::protobuf::RpcController* controller,
                const Ping* ping,
                Msgs* msgs,
                google::protobuf::Closure* done) override;

  void HeartBeat(google::protobuf::RpcController* controller,
                 const Ping* ping,
                 Pong* pong,
                 google::protobuf::Closure* done) override;

  void SendtoAccess(google::protobuf::RpcController* controller,
                    const Msg* msg,
                    Pong* pong,
                    google::protobuf::Closure* done) override;

  butil::Status ResetHeartBeatTimer(user_id_t user_id);

  butil::Status ClearUserData(user_id_t user_id);

  butil::Status PushClosureAndReply(user_id_t user_id,
                                    google::protobuf::Closure* done,
                                    Msgs* msgs,
                                    brpc::Controller* cntl);

  butil::Status PopClosureAndReply(user_id_t user_id,
                                   google::protobuf::Closure** done,
                                   Msgs** msgs,
                                   brpc::Controller** cntl);
  void ClearClosureAndReply();
  void Clear();
 private:

  struct Data{
    google::protobuf::Closure* done;
    Msgs* msgs;
    brpc::Controller* cntl;

    bthread_timer_t heartbeat_timeout_id;
    HeartBeatTimeoutArg* hbarg;
  };
  enum { kBucketNum = 16 };
  std::mutex mutex_[kBucketNum];
  std::unordered_map<user_id_t, Data> id_map_[kBucketNum];

  brpc::Channel *logic_channel_;
  brpc::Channel *db_channel_;
};

}  // namespace tinyim

#endif  // TINYIM_ACCESS_ACCESS_SERVICE_H_