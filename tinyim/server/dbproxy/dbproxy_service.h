#ifndef TINYIM_DBPROXY_DBPROXY_SERVICE_H_
#define TINYIM_DBPROXY_DBPROXY_SERVICE_H_

#include "dbproxy.pb.h"
#include "type.h"

#include <soci/soci.h>

namespace brpc {
class Channel;
class Controller;
}  // namespace brpc

namespace tinyim {
class DbproxyServiceImpl : public DbproxyService {
 public:
  explicit DbproxyServiceImpl();
  ~DbproxyServiceImpl();

  void AuthAndSaveSession(google::protobuf::RpcController* controller,
                          const SigninData* new_msg,
                          Pong* pong,
                          google::protobuf::Closure* done) override;

  void ClearSession(google::protobuf::RpcController* controller,
                    const UserId*,
                    Pong*,
                    google::protobuf::Closure* done) override;

  void SavePrivateMsg(google::protobuf::RpcController* controller,
                      const NewPrivateMsg* new_msg,
                      Reply* reply,
                      google::protobuf::Closure* done) override;

  void SaveGroupMsg(google::protobuf::RpcController* controller,
                    const NewGroupMsg*,
                    Reply* reply,
                    google::protobuf::Closure* done) override;

  void GetGroupMember(google::protobuf::RpcController* controller,
                      const GroupId* new_msg,
                      UserIds* reply,
                      google::protobuf::Closure* done) override;
 private:

  soci::connection_pool* ChooseDatabase(user_id_t user_id){
    // TODO consistent hash
    return &db_message_connect_pool_;
  }
   // TODO 分库分表
  soci::connection_pool db_message_connect_pool_;

  soci::connection_pool db_group_members_connect_pool_;
};

}  // namespace tinyim

#endif  // TINYIM_DBPROXY_DBPROXY_SERVICE_H_