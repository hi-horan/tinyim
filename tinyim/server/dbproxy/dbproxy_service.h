#ifndef TINYIM_DBPROXY_DBPROXY_SERVICE_H_
#define TINYIM_DBPROXY_DBPROXY_SERVICE_H_

#include "dbproxy.pb.h"

namespace brpc {
class Channel;
class Controller;
}  // namespace brpc

namespace tinyim {
class DbproxyServiceImpl : public DbproxyService {
 public:
  explicit DbproxyServiceImpl(brpc::Channel* channel);
  ~DbproxyServiceImpl();


  virtual void AuthAndSaveSession(google::protobuf::RpcController* controller,
                                  const SigninData* new_msg,
                                  Pong* pong,
                                  google::protobuf::Closure* done) override;

  virtual void ClearSession(google::protobuf::RpcController* controller,
                            const UserId*,
                            Pong*,
                            google::protobuf::Closure* done) override;

  virtual void SavePrivateMsg(google::protobuf::RpcController* controller,
                              const NewPrivateMsg* new_msg,
                              Reply* reply,
                              google::protobuf::Closure* done) override;

  virtual void SaveGroupMsg(google::protobuf::RpcController* controller,
                            const NewGroupMsg*,
                            Reply* reply,
                            google::protobuf::Closure* done) override;

  virtual void GetGroupMember(google::protobuf::RpcController* controller,
                              const GroupId* new_msg,
                              UserIds* reply,
                              google::protobuf::Closure* done) override;
};



}  // namespace tinyim




#endif  // TINYIM_DBPROXY_DBPROXY_SERVICE_H_