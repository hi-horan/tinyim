
#include "logic/logic_service.h"
#include "idgen/idgen.pb.h"

#include <glog/logging.h>
#include <brpc/closure_guard.h>
#include <brpc/controller.h>

namespace tinyim {

LogicServiceImpl::LogicServiceImpl(brpc::Channel *id_channel): id_channel_(id_channel){}

LogicServiceImpl::~LogicServiceImpl() {
  DLOG(INFO) << "Calling AccessServiceImpl dtor";
}

void LogicServiceImpl::SendMsg(google::protobuf::RpcController* controller,
                      const NewMsg* new_msg,
                      MsgReply* reply,
                      google::protobuf::Closure* done) {
  brpc::ClosureGuard done_guard(done);
  brpc::Controller* cntl = static_cast<brpc::Controller*>(controller);
  const tinyim::UserId user_id = new_msg->user_id();
  DLOG(INFO) << "Received request[log_id=" << cntl->log_id()
            << "] from " << cntl->remote_side()
            << " to " << cntl->local_side()
            << " user_id=" << user_id
            << " peer_id=" << new_msg->peer_id()
            << " timestamp=" << new_msg->client_timestamp()
            << " msg_type=" << new_msg->msg_type()
            << " message=" << new_msg->message()
            << " (attached=" << cntl->request_attachment() << ")";

  IdGenService_Stub stub(id_channel_);
  MsgIdRequest id_request;
  MsgIdReply id_reply;
  brpc::Controller id_cntl;
  id_cntl.set_log_id(id_cntl.log_id());

  auto user_and_id_num = request.add_user_ids();
  user_and_id_num->set_user_id(user_id);
  user_and_id_num->set_need_msgid_num(1);

  // 1. Get id for this msg
  stub.IdGenerate(&id_cntl, &id_request, &id_reply, nullptr);
  if (id_cntl.Failed()){
    DLOG(ERROR) << "Fail to call IdGenerate. " << cntl.ErrorText();
    cntl->SetFailed(id_cntl.ErrorCode(), id_cntl.ErrorText());
    return;
  }
  else{
    reply->set_user_id(user_id);
    reply->set_msg_id(id_reply.msg_ids(0).start_msg_id());

    // TODO last_msg_id
    reply->set_timestamp(std::time(nullptr));
  }

  // 2. Save this msg to db

  // ResetHeartBeatTimer(user_id);
}

}  // namespace tinyim