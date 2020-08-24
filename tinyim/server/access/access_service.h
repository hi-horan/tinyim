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
  explicit AccessServiceImpl(brpc::Channel *channel);
  virtual ~AccessServiceImpl();

  virtual void SendMsg(google::protobuf::RpcController* controller,
                       const NewMsg* new_msg,
                       MsgReply* reply,
                       google::protobuf::Closure* done) override;

  virtual void PullData(google::protobuf::RpcController* controller,
                        const Ping* ping,
                        PullReply* pull_reply,
                        google::protobuf::Closure* done) override;

  virtual void HeartBeat(google::protobuf::RpcController* controller,
                         const Ping* ping,
                         Pong* pong,
                         google::protobuf::Closure* done) override;

  butil::Status ResetHeartBeatTimer(UserId user_id);

  butil::Status ClearUserData(UserId user_id);

  butil::Status PushClosureAndReply(UserId user_id,
                                    google::protobuf::Closure* done,
                                    PullReply* reply,
                                    brpc::Controller* cntl);

  butil::Status PopClosureAndReply(UserId user_id,
                                   google::protobuf::Closure** done,
                                   PullReply** reply,
                                   brpc::Controller** cntl);
  void ClearClosureAndReply();
  void Clear();
 private:

  struct Data{
    google::protobuf::Closure* done;
    PullReply* reply;
    brpc::Controller* cntl;

    bthread_timer_t heartbeat_timeout_id;
    HeartBeatTimeoutArg* hbarg;
  };
  enum { kBucketNum = 16 };
  std::mutex mutex_[kBucketNum];
  std::unordered_map<UserId, Data> id_map_[kBucketNum];

  brpc::Channel *logic_channel_;
};

}  // namespace tinyim

#endif  // TINYIM_ACCESS_ACCESS_SERVICE_H_