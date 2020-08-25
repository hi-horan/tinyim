
#include "access/access.pb.h"
#include "dbproxy/dbproxy.pb.h"
#include "logic/logic_service.h"
#include "idgen/idgen.pb.h"

#include <glog/logging.h>
#include <brpc/channel.h>
#include <brpc/closure_guard.h>
#include <brpc/controller.h>

namespace tinyim {

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

  IdGenService_Stub id_stub(id_channel_);
  MsgIdRequest id_request;
  MsgIdReply id_reply;
  brpc::Controller id_cntl;
  id_cntl.set_log_id(id_cntl.log_id());


  // 1. Get id for this msg
  if (new_msg->msg_type() == MsgType::PRIVATE){
    auto user_and_id_num = id_request.add_user_ids();
    user_and_id_num->set_user_id(user_id);
    user_and_id_num->set_need_msgid_num(1);

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
    db_cntl.set_log_id(id_cntl.log_id());
    db_stub.GetGroupMember(&db_cntl, &group_id, &user_ids, nullptr);
    if (db_cntl.Failed()){
      DLOG(ERROR) << "Fail to call GetGroupMember. " << id_cntl.ErrorText();
      cntl->SetFailed(id_cntl.ErrorCode(), id_cntl.ErrorText().c_str());
      return;
    }
    else {
      for (int i = 0; i < user_ids.user_id_size(); ++i){
        auto user_and_id_num = id_request.add_user_ids();
        user_and_id_num->set_user_id(user_ids.user_id(i));
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

  // 2. Save this msg to db
  int32_t timestamp = std::time(nullptr);
  if (new_msg->msg_type() == MsgType::PRIVATE){
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
      DLOG(ERROR) << "Fail to call SavePrivateMsg. " << id_cntl.ErrorText();
      cntl->SetFailed(id_cntl.ErrorCode(), id_cntl.ErrorText().c_str());
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
}

void LogicServiceImpl::PullData(google::protobuf::RpcController* controller,
                                const Ping* ping,
                                PullReply* pull_reply,
                                google::protobuf::Closure* done){

  // ResetHeartBeatTimer(user_id);
}

}  // namespace tinyim