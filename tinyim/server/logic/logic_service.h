#ifndef TINYIM_LOGIC_LOGIC_SERVICE_H_
#define TINYIM_LOGIC_LOGIC_SERVICE_H_

#include "logic.pb.h"
#include "type.h"

#include <mutex>
#include <unordered_map>

#include <bthread/unstable.h>

namespace brpc {
class Channel;
class Controller;
}  // namespace brpc

namespace butil{
class Status;
}  // namespace butil

namespace tinyim {

class LogicServiceImpl : public tinyim::LogicService {
 public:
  explicit LogicServiceImpl(brpc::Channel *id_channel, brpc::Channel* db_channel);
  virtual ~LogicServiceImpl();

  virtual void SendMsg(google::protobuf::RpcController* controller,
                       const NewMsg* new_msg,
                       MsgReply* reply,
                       google::protobuf::Closure* done) override;

  virtual void PullData(google::protobuf::RpcController* controller,
                        const Ping* ping,
                        PullReply* pull_reply,
                        google::protobuf::Closure* done) override;

  // butil::Status ClearUserData(user_id_t user_id);

  // butil::Status PushClosureAndReply(user_id_t user_id,
                                    // google::protobuf::Closure* done,
                                    // PullReply* reply,
                                    // brpc::Controller* cntl);

  // butil::Status PopClosureAndReply(user_id_t user_id,
                                   // google::protobuf::Closure** done,
                                   // PullReply** reply,
                                   // brpc::Controller** cntl);
  // void ClearClosureAndReply();
  // void Clear();
 private:

  // struct Data{
    // google::protobuf::Closure* done;
    // PullReply* reply;
    // brpc::Controller* cntl;

    // // bthread_timer_t heartbeat_timeout_id;
    // // HeartBeatTimeoutArg* hbarg;

    // // TODO pull timeout
  // };
  // enum { kBucketNum = 16 };
  // std::mutex mutex_[kBucketNum];
  // std::unordered_map<user_id_t, Data> id_map_[kBucketNum];

  brpc::Channel *id_channel_;
  brpc::Channel *db_channel_;
};

}  // namespace tinyim


#endif  // TINYIM_LOGIC_LOGIC_SERVICE_H_