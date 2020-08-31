#include "dbproxy/dbproxy_service.h"

#include <gflags/gflags.h>
#include <glog/logging.h>

DEFINE_string(db_connect_info, "dbname=test user=root", "Messages database connect information");
DEFINE_string(db_name, "mysql", "Database name");

DEFINE_string(db_group_member_connect_info, "dbname=test user=root", "Group members database connect information");
DEFINE_string(db_group_member_name, "mysql", "Database name");
// TODO db reconnect when timeout

namespace tinyim {

constexpr int kConnectNumEachDb = 10;

DbproxyServiceImpl::DbproxyServiceImpl():db_message_connect_pool_(kConnectNumEachDb),
                                                               db_group_members_connect_pool_(kConnectNumEachDb) {
  for (int i = 0; i < kConnectNumEachDb; ++i){
    db_message_connect_pool_.at(i).open(FLAGS_db_name, FLAGS_db_connect_info);
  }
  for (int i = 0; i < kConnectNumEachDb; ++i){
    db_group_members_connect_pool_.at(i).open(FLAGS_db_group_member_name, FLAGS_db_group_member_connect_info);
  }
}

DbproxyServiceImpl::~DbproxyServiceImpl() {}

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
  try {
    auto pool = ChooseDatabase(new_msg->user_id());
    auto peer_pool = ChooseDatabase(new_msg->peer_id());
    if (pool == peer_pool){
      soci::session sql(*pool);
      sql << "INSERT INTO msssages(user_id, peer_id, msg_id, group_id, message, send, time) "
             "VALUES (:user_id, :peer_id, :msg_id, :group_id, :message, :send, :time), "
             "(:user_id2, :peer_id2, :msg_id2, :group_id2, :message2, :send2, :time2);",
             soci::use(new_msg->user_id()),
             soci::use(new_msg->peer_id()),
             soci::use(new_msg->msg_id()),

             soci::use(0),
             soci::use(new_msg->message()),
             soci::use(1),
             soci::use(new_msg->timestamp());


             soci::use(new_msg->peer_id()),
             soci::use(new_msg->user_id()),
             soci::use(new_msg->msg_id()),

             soci::use(0),
             soci::use(new_msg->message()),
             soci::use(0),
             soci::use(new_msg->timestamp());
    }
    else{
      {
        soci::session sql(*pool);
        sql << "INSERT INTO msssages(user_id, peer_id, msg_id, group_id, message, send, time) "
               "VALUES (:user_id, :peer_id, :msg_id, :group_id, :message, :send, :time);",
               soci::use(new_msg->user_id()),
               soci::use(new_msg->peer_id()),
               soci::use(new_msg->msg_id()),

               soci::use(0),
               soci::use(new_msg->message()),
               soci::use(1),
               soci::use(new_msg->timestamp());
      }
      {
        soci::session sql(*peer_pool);
        sql << "INSERT INTO msssages(user_id, peer_id, msg_id, group_id, message, send, time) "
               "VALUES (:user_id, :peer_id, :msg_id, :group_id, :message, :send, :time);",
               soci::use(new_msg->peer_id()),
               soci::use(new_msg->user_id()),
               soci::use(new_msg->msg_id()),

               soci::use(0),
               soci::use(new_msg->message()),
               soci::use(0),
               soci::use(new_msg->timestamp());
      }
    }
  }
  catch (const soci::soci_error& err) {
    LOG(ERROR) << "Fail to insert into messages"
                << " user_id=" << new_msg->user_id()
                << " peer_id=" << new_msg->peer_id()
                << " msg_id=" << new_msg->msg_id()
                << ". " << err.what();
  }
}

void DbproxyServiceImpl::SaveGroupMsg(google::protobuf::RpcController* controller,
                                      const NewGroupMsg* new_group_msg,
                                      Reply* reply,
                                      google::protobuf::Closure* done){
  try {
    const user_id_t user_id_from = new_group_msg->user_id_from();
    const group_id_t group_id = new_group_msg->group_id();
    const int time = new_group_msg->timestamp();

    auto pool = ChooseDatabase(user_id_from);
    soci::session sql(*pool);
    sql << "INSERT INTO msssages(user_id, peer_id, msg_id, group_id, message, send, time) "
            "VALUES (:user_id, :peer_id, :msg_id, :group_id, :message, :send, :time) ",
            soci::use(user_id_from),
            soci::use(user_id_from),
            soci::use(new_group_msg->msg_id_from()),

            soci::use(group_id),
            soci::use(new_group_msg->message()),
            soci::use(1),
            soci::use(time);

    for (int i = 0, size = new_group_msg->user_and_msgids_size(); i < size; ++i){
      // TODO each db inserts all once
      const auto& user_and_msgid = new_group_msg->user_and_msgids(i);
      auto pool = ChooseDatabase(user_and_msgid.user_id());
      soci::session sql(*pool);
      sql << "INSERT INTO msssages(user_id, peer_id, msg_id, group_id, message, send, time) "
              "VALUES (:user_id, :peer_id, :msg_id, :group_id, :message, :send, :time) ",
              soci::use(user_and_msgid.user_id()),
              soci::use(user_id_from),
              soci::use(user_and_msgid.msg_id()),

              soci::use(group_id),
              soci::use(new_group_msg->message()),
              soci::use(0),
              soci::use(time);
    }
  }
  catch (const soci::soci_error& err) {
    LOG(ERROR) << "Fail to insert into messages. " << err.what();
  }
}

void DbproxyServiceImpl::GetGroupMember(google::protobuf::RpcController* controller,
                                        const GroupId* new_msg,
                                        UserIds* reply,
                                        google::protobuf::Closure* done) {
  try {
    soci::session sql(db_group_members_connect_pool_);
    soci::rowset<soci::row> rs =
        (sql.prepare << "SELECT user_id, user_name FROM group_members WHERE group_id = :group_id;",
            soci::use(new_msg->group_id()));

    for (soci::rowset<soci::row>::iterator it = rs.begin(); it != rs.end(); ++it) {
      const auto& row = *it;
      reply->add_user_id(row.get<user_id_t>(0));
    }
  }
  catch (const soci::soci_error& err) {
    LOG(ERROR) << "Fail to select from group_members"
               << " group_id=" << new_msg->group_id()
               << ". " << err.what();
  }
}

}  // namespace tinyim

