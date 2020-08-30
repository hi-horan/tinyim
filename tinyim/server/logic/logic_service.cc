
#include "access/access.pb.h"
#include "common/messages.pb.h"
#include "dbproxy/dbproxy.pb.h"
#include "logic/logic_service.h"
#include "idgen/idgen.pb.h"

#include <vector>

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <brpc/channel.h>
#include <brpc/closure_guard.h>
#include <brpc/controller.h>
#include <brpc/options.pb.h>

DEFINE_int32(max_retry, 3, "Max retries(not including the first RPC)");
DEFINE_string(connection_type, "single", "Connection type. Available values: single, pooled, short");
DEFINE_int32(timeout_ms, 100, "RPC timeout in milliseconds");

namespace tinyim {

class SendtoAccessClosure: public ::google::protobuf::Closure {
 public:
  SendtoAccessClosure(brpc::Controller* cntl, Pong* pong): cntl_(cntl), pong_(pong){}

  virtual Run() override {
    if (cntl_.Failed()){
      DLOG(ERROR) << "Fail to call SendtoAccess. " << cntl.ErrorText();
    }
    delete this;
  }

  ~SendtoAccessClosure(){
    delete cntl_;
    delete pong_;
  }

 private:
  brpc::Controller* cntl_;
  Pong* pong_;
};

LogicServiceImpl::LogicServiceImpl(brpc::Channel *id_channel,
                                   brpc::Channel *db_channel): id_channel_(id_channel),
                                                               db_channel_(db_channel) {}

LogicServiceImpl::~LogicServiceImpl() {
  DLOG(INFO) << "Calling AccessServiceImpl dtor";
}

void LogicServiceImpl::SendMsg(google::protobuf::RpcController* controller,
                      const NewMsg* new_msg,
                      MsgReply* reply,
                      google::protobuf::Closure* done) {
  brpc::ClosureGuard done_guard(done);
  brpc::Controller* cntl = static_cast<brpc::Controller*>(controller);
  const tinyim::user_id_t user_id = new_msg->user_id();
  DLOG(INFO) << "Received request[log_id=" << cntl->log_id()
            << "] from " << cntl->remote_side()
            << " to " << cntl->local_side()
            << " user_id=" << user_id
            << " peer_id=" << new_msg->peer_id()
            << " timestamp=" << new_msg->client_timestamp()
            << " msg_type=" << new_msg->msg_type()
            << " message=" << new_msg->message()
            << " (attached=" << cntl->request_attachment() << ")";


  // 1. check duplicate
  DbproxyService_Stub db_stub(db_channel_);
  brpc::Controller db_cntl;
  UserId cur_user_id;
  cur_user_id.set_user_id(user_id);
  UserLastSendData last_send_data;
  db_stub.GetUserLastSendData(&db_cntl, &cur_user_id, &last_send_data, nullptr);
  if (db_cntl.Failed()){
    DLOG(ERROR) << "Fail to call GetGroupMember. " << db_cntl.ErrorText();
    cntl->SetFailed(db_cntl.ErrorCode(), db_cntl.ErrorText().c_str());
    return;
  }
  else {
    // TODO when last_send_data.client_timestamp() = 0
    if (last_send_data.client_timestamp() == new_msg->client_timestamp()) {
      reply->set_msg_id(last_send_data.msg_id());
      reply->set_timestamp(last_send_data.client_timestamp());
      return;
    }
  }

  // 2. Get id for this msg
  IdGenService_Stub id_stub(id_channel_);
  MsgIdRequest id_request;
  MsgIdReply id_reply;
  brpc::Controller id_cntl;
  id_cntl.set_log_id(cntl.log_id());
  {
    auto user_and_id_num = id_request.add_user_ids();
    user_and_id_num->set_user_id(user_id);
    user_and_id_num->set_need_msgid_num(1);
  }
  if (new_msg->msg_type() == MsgType::PRIVATE){
    auto peer_and_id_num = id_request.add_user_ids();
    peer_and_id_num->set_user_id(new_msg->peer_id());
    peer_and_id_num->set_need_msgid_num(1);
  }
  else {
    DbproxyService_Stub db_stub(db_channel_);
    GroupId group_id;
    group_id.set_group_id(new_msg->peer_id());
    brpc::Controller db_cntl;
    UserIds user_ids;
    db_cntl.set_log_id(cntl->log_id());
    db_stub.GetGroupMember(&db_cntl, &group_id, &user_ids, nullptr);
    if (db_cntl.Failed()){
      DLOG(ERROR) << "Fail to call GetGroupMember. " << db_cntl.ErrorText();
      cntl->SetFailed(db_cntl.ErrorCode(), db_cntl.ErrorText().c_str());
      return;
    }
    else {
      for (int i = 0; i < user_ids.user_id_size(); ++i){
        const user_id_t cur_user_id = user_ids.user_id(i);
        if (cur_user_id == user_id){
          continue;
        }
        auto user_and_id_num = id_request.add_user_ids();
        user_and_id_num->set_user_id(cur_user_id);
        user_and_id_num->set_need_msgid_num(1);
      }
    }
  }

  id_stub.IdGenerate(&id_cntl, &id_request, &id_reply, nullptr);
  if (id_cntl.Failed()){
      DLOG(ERROR) << "Fail to call IdGenerate. " << id_cntl.ErrorText();
      cntl->SetFailed(id_cntl.ErrorCode(), id_cntl.ErrorText().c_str());
      return;
  }

  // 3. save user last send data
  int32_t timestamp = std::time(nullptr);
  brpc::Controller db_set_cntl;
  Reply set_reply;
  last_send_data.set_msg_id(id_reply.msg_ids(0).start_msg_id());
  db_stub.SetUserLastSendData(&db_set_cntl, &last_send_data, &set_reply, nullptr);
  if (db_set_cntl.Failed()){
      DLOG(ERROR) << "Fail to call SetUserLastSendData. " << db_set_cntl.ErrorText();
      cntl->SetFailed(db_set_cntl.ErrorCode(), db_set_cntl.ErrorText().c_str());
      return;
  }

  // 4. Save this msg to db
  if (new_msg->msg_type() == MsgType::PRIVATE) {
    const msg_id_t msg_id = id_reply.msg_ids(0).start_msg_id();
    NewPrivateMsg new_private_msg;
    new_private_msg.set_user_id(user_id);
    new_private_msg.set_msg_type(new_msg->msg_type());
    new_private_msg.set_peer_id(new_msg->peer_id());
    new_private_msg.set_client_timestamp(new_msg->client_timestamp());
    new_private_msg.set_timestamp(timestamp); // TODO should earlier
    DbproxyService_Stub db_stub2(db_channel_);
    brpc::Controller db_cntl;
    Reply db_reply;
    db_stub2.SavePrivateMsg(&db_cntl, &new_private_msg, &db_reply, nullptr);
    if (db_cntl.Failed()){
      DLOG(ERROR) << "Fail to call SavePrivateMsg. " << db_cntl.ErrorText();
      cntl->SetFailed(db_cntl.ErrorCode(), db_cntl.ErrorText().c_str());
      return;
    }
    else{
      reply->set_msg_id(msg_id);
      // reply->set_timestamp(timestamp);
      return;
    }
  }
  else{
    NewGroupMsg new_group_msg;
    msg_id_t msg_id = 0;
    for (int i = 0, size = id_reply.msg_ids_size(); i < size; ++i){
      auto puser_and_msgid = new_group_msg.add_user_and_msgids();
      if (user_id == id_reply.msg_ids(i).user_id()){
        msg_id = id_reply.msg_ids(i).start_msg_id();
      }
      puser_and_msgid->set_user_id(id_reply.msg_ids(i).user_id());
      puser_and_msgid->set_msg_id(id_reply.msg_ids(i).start_msg_id());
    }
    DbproxyService_Stub db_stub2(db_channel_);
    brpc::Controller db_cntl;
    Reply db_reply;
    db_stub2.SaveGroupMsg(&db_cntl, &new_group_msg, &db_reply, nullptr);
    if (db_cntl.Failed()){
      DLOG(ERROR) << "Fail to call SaveGroupMsg. " << id_cntl.ErrorText();
      cntl->SetFailed(id_cntl.ErrorCode(), id_cntl.ErrorText().c_str());
      return;
    }
    else {
      CHECK_NE(msg_id, 0) << "Id is wrong. user_id=" << user_id;
      reply->set_msg_id(msg_id);
      // reply->set_timestamp(timestamp);
    }
  }
  // 5. Send msg to peer or all user in group
  DbproxyService_Stub session_stub(db_channel_);
  brpc::Controller session_cntl;
  Sessions sessions;
  UserIds user_ids;

  for (int i = 1, size = id_request.user_ids_size(); i < size; ++i){
    // i = 0 is user_id
    user_ids.add_user_id(id_request.user_ids(i).user_id());
  }

  session_stub.GetSessions(&session_cntl, &user_ids, &sessions, nullptr);
  if (session_cntl.Failed()){
    DLOG(ERROR) << "Fail to call GetSessions. " << session_cntl.ErrorText();
    cntl->SetFailed(session_cntl.ErrorCode(), session_cntl.ErrorText().c_str());
    return;
  }
  CHECK_NE(msg_id, 0) << "Id is wrong. user_id=" << user_id;
  reply->set_msg_id(msg_id);

  std::vector<brpc::Channel*> channel_vec;
  channel_vec.reserve(sessions.session_size());
  {
    std::unique_lock lck(access_map_mtx_);
    for (int i = 0, size = sessions.session_size(); i < size; ++i){
      if (!sessions.session(i).has_session()){
        continue;
      }
      if (access_map_.count(sessions.session(i).addr()) == 0) {
        brpc::ChannelOptions options;
        options.protocol = brpc::PROTOCOL_BAIDU_STD;
        options.connection_type = FLAGS_connection_type;
        options.timeout_ms = FLAGS_timeout_ms/*milliseconds*/;
        options.max_retry = FLAGS_max_retry;
        access_map_[sessions.session(i).addr()].Init(sessions.session(i).addr().c_str(), &options);
      }
      channel_vec.push_back(&(access_map_[sessions.session(i).addr()]));
    }
  }
  for (int i = 0, size = sessions.session_size(); i < size; ++i){
    if (!sessions.session(i).has_session()){
      continue;
    }
    auto cntl = new brpc::Controller;
    Msg msg;
    // XXX const char*
    msg.set_user_id(user_id);
    msg.set_peer_id(user_ids.user_id(i));
    msg.set_message(new_msg->message().c_str());
    msg.set_msg_id(id_reply.msg_ids(i + 1).start_msg_id());

    auto pong = new Pong;
    auto send_to_access_closure = new SendtoAccessClosure(cntl, pong);

    AccessService_Stub stub;
    stub.SendtoAccess(cntl, &msg, pong, send_to_access_closure);
  }
}

void LogicServiceImpl::PullData(google::protobuf::RpcController* controller,
                                const Ping* ping,
                                PullReply* pull_reply,
                                google::protobuf::Closure* done){

  // ResetHeartBeatTimer(user_id);
}

}  // namespace tinyim