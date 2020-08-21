
#include "access.pb.h"

#include <errno.h>
#include <mutex>
#include <unordered_map>

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <brpc/server.h>
#include <brpc/stream.h>
#include <bthread/bthread.h>
#include <butil/status.h>

#include "util/initialize.h"
#include "type.h"

DEFINE_int32(port, 8001, "TCP Port of this server");
DEFINE_int32(idle_timeout_s, -1, "Connection will be closed if there is no "
             "read/write operations during the last `idle_timeout_s'");
DEFINE_int32(logoff_ms, 2000, "Maximum duration of server's LOGOFF state "
             "(waiting for client to close connection before server stops)");

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

    PushClosureAndReply(user_id, done, pull_reply);

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


  }

  butil::Status PushClosureAndReply(tinyim::UserId user_id,
                                    google::protobuf::Closure* done,
                                    tinyim::PullReply* reply) {
    const int bucket = user_id % kBucketNum;
    auto& id_map = id_map_[bucket];

    std::unique_lock<std::mutex> lck(mutex_[bucket]);
    if (id_map.count(user_id) == 0){
      id_map[user_id] = {done, reply};
      lck.unlock();
    }
    else {
      auto origin_data = id_map[user_id];
      id_map[user_id] = {done, reply};
      lck.unlock();

      origin_data.reply->set_data_type(tinyim::DataType::NONE);
      origin_data.done->Run();
    }
    return butil::Status::OK();
  }

  butil::Status PopClosureAndReply(tinyim::UserId user_id,
                                   google::protobuf::Closure** done,
                                   tinyim::PullReply** reply){
    const int bucket = user_id % kBucketNum;
    auto& id_map = id_map_[bucket];

    std::unique_lock<std::mutex> lck(mutex_[bucket]);
    if (id_map.count(user_id) > 0){
      if (done != nullptr){
        *done = id_map[user_id].done;
        *reply = id_map[user_id].reply;
        id_map.erase(user_id);
        lck.unlock();
        return butil::Status::OK();
      }
      else{
        auto origin_data = id_map[user_id];
        id_map.erase(user_id);
        lck.unlock();

        origin_data.reply->set_data_type(tinyim::DataType::NONE);
        origin_data.done->Run();
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
        iter->second.reply->set_data_type(tinyim::DataType::NONE);
        iter->second.done->Run();
      }
    }
  }
  void Clear(){
    ClearClosureAndReply();
  }
 private:
  enum { kBucketNum = 16 };
  std::mutex mutex_[kBucketNum];

  struct Data{
    google::protobuf::Closure* done;
    tinyim::PullReply* reply;
  };
  std::unordered_map<tinyim::UserId, Data> id_map_[kBucketNum];
};

}  // namespace tinyim


int main(int argc, char* argv[]) {
  tinyim::Initialize init(argc, &argv);

  brpc::Server server;

  tinyim::AccessServiceImpl access_service_impl;

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
