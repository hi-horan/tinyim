
#include "access/access_service.h"

#include <errno.h>
#include <memory>

#include <brpc/channel.h>
#include <bthread/bthread.h>
#include <butil/time.h>
#include <gflags/gflags.h>
#include <glog/logging.h>

DEFINE_int32(recv_heartbeat_timeout_s, 30, "Receive heartbeat timeout");

namespace tinyim {
struct HeartBeatTimeoutArg{
  UserId user_id;
  AccessServiceImpl *access_service_impl;
};

}  // namespace tinyim

namespace {
static void UserHeartBeatTimeoutHeadler(void* arg){
  std::unique_ptr<tinyim::HeartBeatTimeoutArg>
    parg(static_cast<tinyim::HeartBeatTimeoutArg*>(arg));

  tinyim::UserId user_id = parg->user_id;
  auto access_service_impl = parg->access_service_impl;
  access_service_impl->ClearUserData(user_id);
}
}

namespace tinyim {

AccessServiceImpl::AccessServiceImpl(brpc::Channel* channel): logic_channel_(channel) {}

AccessServiceImpl::~AccessServiceImpl() {
  DLOG(INFO) << "Calling AccessServiceImpl dtor";
}

void AccessServiceImpl::SendMsg(google::protobuf::RpcController* controller,
                      const NewMsg* new_msg,
                      MsgReply* reply,
                      google::protobuf::Closure* done) {
  brpc::ClosureGuard done_guard(done);
  brpc::Controller* cntl = static_cast<brpc::Controller*>(controller);
  const UserId user_id = new_msg->user_id();
  DLOG(INFO) << "Received request[log_id=" << cntl->log_id()
            << "] from " << cntl->remote_side()
            << " to " << cntl->local_side()
            << " user_id=" << user_id
            << " peer_id=" << new_msg->peer_id()
            << " timestamp=" << new_msg->client_timestamp()
            << " msg_type=" << new_msg->msg_type()
            << " message=" << new_msg->message()
            << " (attached=" << cntl->request_attachment() << ")";

  ResetHeartBeatTimer(user_id);

  reply->set_user_id(user_id);
  reply->set_msg_id(1);
  reply->set_last_msg_id(2);
  reply->set_timestamp(std::time(nullptr));
}

void AccessServiceImpl::PullData(google::protobuf::RpcController* controller,
                      const Ping* ping,
                      PullReply* pull_reply,
                      google::protobuf::Closure* done) {
  brpc::ClosureGuard done_guard(done);
  brpc::Controller* cntl = static_cast<brpc::Controller*>(controller);
  const UserId user_id = ping->user_id();
  DLOG(INFO) << "Received request[log_id=" << cntl->log_id()
            << "] from " << cntl->remote_side()
            << " to " << cntl->local_side()
            << " user_id=" << user_id;

  PushClosureAndReply(user_id, done, pull_reply, cntl);

  done_guard.release();
}

void AccessServiceImpl::HeartBeat(google::protobuf::RpcController* controller,
                        const Ping* ping,
                        Pong* pong,
                        google::protobuf::Closure* done) {
  brpc::ClosureGuard done_guard(done);
  brpc::Controller* cntl = static_cast<brpc::Controller*>(controller);
  const UserId user_id = ping->user_id();
  DLOG(INFO) << "Received request[log_id=" << cntl->log_id()
            << "] from " << cntl->remote_side()
            << " to " << cntl->local_side()
            << " user_id=" << user_id;

  ResetHeartBeatTimer(user_id);
}

butil::Status AccessServiceImpl::ResetHeartBeatTimer(UserId user_id){
  static const int64_t heartbeat_timeout_us = FLAGS_recv_heartbeat_timeout_s * 1000000;
  const int bucket = user_id % kBucketNum;
  auto& id_map = id_map_[bucket];
  // allocate it before lock
  auto hbarg = new HeartBeatTimeoutArg{user_id, this};
  std::unique_ptr<HeartBeatTimeoutArg> lazy_delete;

  const int64_t cur_time_us = butil::gettimeofday_us();
  std::unique_lock<std::mutex> lck(mutex_[bucket]);
  if (id_map.count(user_id) != 0){
    auto& data = id_map[user_id];
    if (data.heartbeat_timeout_id != 0 /*TimerThread::INVALID_TASK_ID */ ){
      if (bthread_timer_del(data.heartbeat_timeout_id) == 0){
        lazy_delete.reset(data.hbarg);
      }
    }
    bthread_timer_add(&data.heartbeat_timeout_id,
                      butil::microseconds_to_timespec(cur_time_us + heartbeat_timeout_us),
                      UserHeartBeatTimeoutHeadler,
                      hbarg);
    data.hbarg = hbarg;
  }
  else{
    bthread_timer_t heartbeat_timeout_id;
    bthread_timer_add(&heartbeat_timeout_id,
                      butil::microseconds_to_timespec(cur_time_us + heartbeat_timeout_us),
                      UserHeartBeatTimeoutHeadler,
                      hbarg);
    id_map[user_id] = { nullptr, nullptr, nullptr,
                        heartbeat_timeout_id, hbarg };
  }
  lck.unlock();
  return butil::Status::OK();
}

butil::Status AccessServiceImpl::ClearUserData(UserId user_id){
  const int bucket = user_id % kBucketNum;
  auto& id_map = id_map_[bucket];
  std::unique_ptr<HeartBeatTimeoutArg> lazy_delete;

  std::unique_lock<std::mutex> lck(mutex_[bucket]);
  if (id_map.count(user_id) > 0){
      auto origin_data = id_map[user_id];
      auto& data = id_map[user_id];
      if (data.heartbeat_timeout_id != 0){
        if (bthread_timer_del(data.heartbeat_timeout_id) == 0){
          lazy_delete.reset(data.hbarg);
        }
      }
      id_map.erase(user_id);
      lck.unlock();

      if (origin_data.reply != nullptr){
        origin_data.reply->set_data_type(DataType::NONE);
        origin_data.cntl->CloseConnection("Server close");
        origin_data.done->Run();
      }
      return butil::Status::OK();
  }
  else{
    lck.unlock();
    return butil::Status(EINVAL, "Have no this Closure");
  }
}

butil::Status AccessServiceImpl::PushClosureAndReply(UserId user_id,
                                  google::protobuf::Closure* done,
                                  PullReply* reply,
                                  brpc::Controller* cntl){
  const int bucket = user_id % kBucketNum;
  auto& id_map = id_map_[bucket];
  std::unique_ptr<HeartBeatTimeoutArg> lazy_delete;
  std::unique_ptr<HeartBeatTimeoutArg> ptr(new HeartBeatTimeoutArg{user_id, this});
  brpc::ClosureGuard lazy_run;

  std::unique_lock<std::mutex> lck(mutex_[bucket]);
  if (id_map.count(user_id) == 0){
    id_map[user_id] = { done, reply, cntl, 0, nullptr };
  }
  else {
    auto& data = id_map[user_id];
    if (data.done){
      data.reply->set_data_type(DataType::NONE);
      lazy_run.reset(data.done);
    }
    data.done = done;
    data.reply = reply;
    data.cntl = cntl;
    if (data.heartbeat_timeout_id != 0){
      if (bthread_timer_del(data.heartbeat_timeout_id) == 0){
        // hbarg no change
        data.heartbeat_timeout_id = 0;
        lazy_delete.reset(data.hbarg);
        data.hbarg = nullptr;
      }
    }
  }
  const int64_t cur_time_us = butil::gettimeofday_us();
  static const int64_t heartbeat_timeout_us = FLAGS_recv_heartbeat_timeout_s * 1000000;
  bthread_timer_t heartbeat_timeout_id;
  bthread_timer_add(&heartbeat_timeout_id,
                    butil::microseconds_to_timespec(cur_time_us + heartbeat_timeout_us),
                    UserHeartBeatTimeoutHeadler,
                    ptr.release());
  id_map[user_id].heartbeat_timeout_id = heartbeat_timeout_id;
  lck.unlock();

  return butil::Status::OK();
}

butil::Status AccessServiceImpl::PopClosureAndReply(UserId user_id,
                                  google::protobuf::Closure** done,
                                  PullReply** reply,
                                  brpc::Controller** cntl) {
  brpc::ClosureGuard lazy_run;
  const int bucket = user_id % kBucketNum;
  auto& id_map = id_map_[bucket];

  std::unique_lock<std::mutex> lck(mutex_[bucket]);
  if (id_map.count(user_id) > 0){
    if (done != nullptr){
      auto& data = id_map[user_id];

      // *done = id_map[user_id].done;
      *done = data.done;
      data.done = nullptr;
      // *reply = id_map[user_id].reply;
      *reply = data.reply;
      data.reply = nullptr;
      // *cntl = id_map[user_id].cntl;
      *cntl = data.cntl;
      data.cntl = nullptr;
      // id_map.erase(user_id);
      lck.unlock();
      return butil::Status::OK();
    }
    else{
      auto& data = id_map[user_id];

      if (data.done){
        lazy_run.reset(data.done);
        data.done = nullptr;
        data.reply->set_data_type(DataType::NONE);
        data.reply = nullptr;
        data.cntl = nullptr;
      }
      // if (bthread_timer_del(data.heartbeat_timeout_id) == 0){
        // data.heartbeat_timeout_id = 0;
        // delete data.hbarg;
        // data.hbarg = nullptr;
      // }

      lck.unlock();
      return butil::Status::OK();
    }
  }
  else{
    lck.unlock();
    return butil::Status(EINVAL, "Have no this Closure");
  }
}

void AccessServiceImpl::ClearClosureAndReply() {
  // FIXME Should lock  when process is stopping?
  for (int bucket = 0; bucket < kBucketNum; ++bucket){
    for (auto iter = id_map_[bucket].begin(); iter != id_map_[bucket].end(); ++iter){
      auto& data = iter->second;
      if (data.reply != nullptr){
        data.reply->set_data_type(DataType::NONE);
        data.cntl->CloseConnection("Server close");
        data.done->Run();
      }
      if (data.heartbeat_timeout_id != 0 /*TimerThread::INVALID_TASK_ID*/){
        if (bthread_timer_del(data.heartbeat_timeout_id) == 0){
          delete data.hbarg;
        }
      }
    }
  }
}
void AccessServiceImpl::Clear(){
  ClearClosureAndReply();
}

}  // namespace tinyim