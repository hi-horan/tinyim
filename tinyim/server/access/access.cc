
#include "access.pb.h"

#include <errno.h>
#include <memory>
#include <mutex>
#include <unordered_map>

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <brpc/server.h>
#include <brpc/stream.h>
#include <bthread/bthread.h>
#include <bthread/unstable.h>
#include <butil/status.h>
#include <butil/time.h>

#include "util/consistent_hash.h"
#include "util/initialize.h"
#include "type.h"

DEFINE_int32(port, 8001, "TCP Port of this server");
DEFINE_int32(idle_timeout_s, -1, "Connection will be closed if there is no "
             "read/write operations during the last `idle_timeout_s'");
DEFINE_int32(logoff_ms, 2000, "Maximum duration of server's LOGOFF state "
             "(waiting for client to close connection before server stops)");
DEFINE_int32(recv_heartbeat_timeout_s, 30, "Receive heartbeat timeout");
DEFINE_int32(vnode_count, 300, "Virtual node count");
DEFINE_string(id_server_addr, "192.168.1.100:8200", "Id server address");

namespace tinyim {

// void* testPushMsg(void *arg){
  // auto stream_id = reinterpret_cast<brpc::StreamId>(arg);
  // LOG(INFO) << "Pushing msg use stream_id=" << stream_id;
  // int index = 1;
  // while(!brpc::IsAskedToQuit()) {
    // butil::IOBuf msg;
    // msg.append("hello access client. No.");
    // msg.append(std::to_string(index++));
    // LOG(INFO) << "Calling StreamWrite. " << stream_id;
    // auto ret = brpc::StreamWrite(stream_id, msg);
    // if (ret != 0){
      // LOG(INFO) << "ret=" << ret << "errno= " << errno;
      // LOG(INFO) << "bthread over" << stream_id;
      // return nullptr; // is over
    // }
    // // bthread_usleep(100 * 1000);
  // }
  // // brpc::StreamClose(stream_id);
  // LOG(INFO) << "bthread over" << stream_id;
  // return nullptr;
// }

// class TmpHandler: public brpc::StreamInputHandler {

 // public:
  // virtual int on_received_messages(brpc::StreamId id,
                                    // butil::IOBuf *const messages[],
                                    // size_t size) {
    // std::ostringstream os;
    // for (size_t i = 0; i < size; ++i) {
        // os << "msg[" << i << "]=" << *messages[i];
    // }
    // LOG(INFO) << "Received from Stream=" << id << ": " << os.str();
    // return 0;
  // }
  // virtual void on_idle_timeout(brpc::StreamId id) {
    // LOG(INFO) << "Stream=" << id << " has no data transmission for a while";
  // }

  // virtual void on_closed(brpc::StreamId id) {
    // LOG(INFO) << "Stream=" << id << " is closed";
    // // TODO remove stream id from id_cache
    // // std::unique_lock lock(mutex_[bucket]);
    // // if (id_cache.count(user_id) > 0){
      // // brpc::StreamClose(id_cache[user_id]);
    // // }
  // }

// };

class AccessServiceImpl : public tinyim::AccessService {
 public:
  AccessServiceImpl() {}
  virtual ~AccessServiceImpl() {
    DLOG(INFO) << "Calling AccessServiceImpl dtor";
  }

  virtual void SendMsg(google::protobuf::RpcController* controller,
                       const tinyim::NewMsg* new_msg,
                       tinyim::MsgReply* reply,
                       google::protobuf::Closure* done) override {
    brpc::ClosureGuard done_guard(done);
    brpc::Controller* cntl = static_cast<brpc::Controller*>(controller);
    const tinyim::UserId user_id = new_msg->user_id();
    LOG(INFO) << "Received request[log_id=" << cntl->log_id()
              << "] from " << cntl->remote_side()
              << " to " << cntl->local_side()
              << " user_id=" << user_id
              << " peer_id=" << new_msg->peer_id()
              << " timestamp=" << new_msg->timestamp()
              << " msg_type=" << new_msg->msg_type()
              << " message=" << new_msg->message()
              << " (attached=" << cntl->request_attachment() << ")";

    ResetHeartBeatTimer(user_id);

    reply->set_user_id(user_id);
    reply->set_msg_id(1);
    reply->set_last_msg_id(2);
    reply->set_timestamp(std::time(nullptr));


    // static brpc::StreamId last_stream_id = -1;
    // brpc::StreamId stream_id;
    // GetStreamId(user_id, stream_id);
    // if (stream_id != last_stream_id){
      // last_stream_id = stream_id;
      // bthread_t bt;
      // LOG(INFO) << "Creating new bthread for PushMsg";
      // bthread_start_background(&bt, nullptr, testPushMsg, reinterpret_cast<void*>(stream_id));
    // }
  }

  virtual void PullData(google::protobuf::RpcController* controller,
                        const tinyim::Ping* ping,
                        tinyim::PullReply* pull_reply,
                        google::protobuf::Closure* done) override {
    brpc::ClosureGuard done_guard(done);
    brpc::Controller* cntl = static_cast<brpc::Controller*>(controller);
    const tinyim::UserId user_id = ping->user_id();
    LOG(INFO) << "Received request[log_id=" << cntl->log_id()
              << "] from " << cntl->remote_side()
              << " to " << cntl->local_side()
              << " user_id=" << user_id;

    PushClosureAndReply(user_id, done, pull_reply, cntl);

    done_guard.release();

    // static brpc::StreamId last_stream_id = -1;
    // brpc::StreamId stream_id;
    // GetStreamId(user_id, stream_id);
    // if (stream_id != last_stream_id){
      // last_stream_id = stream_id;
      // bthread_t bt;
      // LOG(INFO) << "Creating new bthread for PushMsg";
      // bthread_start_background(&bt, nullptr, testPushMsg, reinterpret_cast<void*>(stream_id));
    // }
  }

  virtual void HeartBeat(google::protobuf::RpcController* controller,
                         const tinyim::Ping* ping,
                         tinyim::Pong* pong,
                         google::protobuf::Closure* done) override {
    brpc::ClosureGuard done_guard(done);
    brpc::Controller* cntl = static_cast<brpc::Controller*>(controller);
    const tinyim::UserId user_id = ping->user_id();
    LOG(INFO) << "Received request[log_id=" << cntl->log_id()
              << "] from " << cntl->remote_side()
              << " to " << cntl->local_side()
              << " user_id=" << user_id;

    ResetHeartBeatTimer(user_id);

  }


  butil::Status ResetHeartBeatTimer(UserId user_id){
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

  static void UserHeartBeatTimeoutHeadler(void* arg){
    std::unique_ptr<HeartBeatTimeoutArg> parg(static_cast<HeartBeatTimeoutArg*>(arg));
    // std::unique_ptr<HeartBeatTimeOutArg> parg(static_cast<HeartBeatTimeOutArg*>(arg));
    UserId user_id = parg->user_id;
    auto access_service_impl = parg->access_service_impl;
    access_service_impl->ClearUserData(user_id);
  }

  butil::Status ClearUserData(UserId user_id){
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
          origin_data.reply->set_data_type(tinyim::DataType::NONE);
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

  butil::Status PushClosureAndReply(tinyim::UserId user_id,
                                    google::protobuf::Closure* done,
                                    tinyim::PullReply* reply,
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
        data.reply->set_data_type(tinyim::DataType::NONE);
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

  butil::Status PopClosureAndReply(tinyim::UserId user_id,
                                   google::protobuf::Closure** done,
                                   tinyim::PullReply** reply,
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

  void ClearClosureAndReply() {
    // FIXME Should lock  when process is stopping?
    for (int bucket = 0; bucket < kBucketNum; ++bucket){
      for (auto iter = id_map_[bucket].begin(); iter != id_map_[bucket].end(); ++iter){
        auto& data = iter->second;
        if (data.reply != nullptr){
          data.reply->set_data_type(tinyim::DataType::NONE);
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
  void Clear(){
    ClearClosureAndReply();
  }

 private:
  struct HeartBeatTimeoutArg{
    UserId user_id;
    AccessServiceImpl *access_service_impl;
  };

  struct Data{
    google::protobuf::Closure* done;
    tinyim::PullReply* reply;
    brpc::Controller* cntl;

    bthread_timer_t heartbeat_timeout_id;
    HeartBeatTimeoutArg* hbarg;
  };
  enum { kBucketNum = 16 };
  std::mutex mutex_[kBucketNum];
  std::unordered_map<tinyim::UserId, Data> id_map_[kBucketNum];
};

}  // namespace tinyim


int main(int argc, char* argv[]) {
  tinyim::Initialize init(argc, &argv);

  brpc::Server server;

  tinyim::AccessServiceImpl access_service_impl;

  const char* nodes[] = {
    "192.168.1.100:8100",
    "192.168.1.100:8101",
    "192.168.1.100:8102",
    "192.168.1.100:8103",
    "192.168.1.100:8104",
  };

  tinyim::ConsistentHash consistent_hash;
  for (int node = 0; node < static_cast<int>(sizeof(nodes)/sizeof(nodes[0])); ++node) {
    for (int vnode = 0; vnode < FLAGS_vnode_count; ++vnode){
      if (!consistent_hash.insert({node, vnode}).second){
        LOG(WARNING) << "conflict node=" << node << " vnode=" << vnode;
      }
    }
  }
  LOG(INFO) << consistent_hash;

  if (server.AddService(&access_service_impl,
                        brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
    LOG(ERROR) << "Fail to add service";
    return -1;
  }

  brpc::ServerOptions options;
  options.idle_timeout_sec = FLAGS_idle_timeout_s;
  if (server.Start(FLAGS_port, &options) != 0) {
    LOG(ERROR) << "Fail to start access";
    return -1;
  }

  while (!brpc::IsAskedToQuit()) {
    bthread_usleep(1000000L);
  }
  access_service_impl.Clear();

  server.RunUntilAskedToQuit();

  return 0;
}
