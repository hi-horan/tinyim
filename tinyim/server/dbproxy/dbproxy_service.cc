#include "dbproxy/dbproxy_service.h"

namespace tinyim {

void DbproxyServiceImpl::AuthAndSaveSession(google::protobuf::RpcController* controller,
                                            const SigninData* new_msg,
                                            Pong* pong,
                                            google::protobuf::Closure* done){

}

void DbproxyServiceImpl::ClearSession(google::protobuf::RpcController* controller,
                                      const UserId*,
                                      Pong*,
                                      google::protobuf::Closure* done){

}

void DbproxyServiceImpl::SavePrivateMsg(google::protobuf::RpcController* controller,
                                        const NewPrivateMsg* new_msg,
                                        Reply* reply,
                                        google::protobuf::Closure* done){

}

void DbproxyServiceImpl::SaveGroupMsg(google::protobuf::RpcController* controller,
                                      const NewGroupMsg*,
                                      Reply* reply,
                                      google::protobuf::Closure* done){

}

void DbproxyServiceImpl::GetGroupMember(google::protobuf::RpcController* controller,
                                        const GroupId* new_msg,
                                        UserIds* reply,
                                        google::protobuf::Closure* done){

}

}  // namespace tinyim

