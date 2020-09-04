#include "dbproxy/dbproxy_service.h"

#include <cstdio>
#include <sstream>

#include <gflags/gflags.h>
#include <glog/logging.h>

DEFINE_string(db_connect_info, "dbname=test user=root", "Messages database connect information");
DEFINE_string(db_name, "mysql", "Database name");

DEFINE_string(db_group_member_connect_info, "dbname=test user=root", "Group members database connect information");
DEFINE_string(db_group_member_name, "mysql", "Database name");

DEFINE_string(redis_connection_type, "", "Connection type. Available values: single, pooled, short");
DEFINE_string(redis_server, "127.0.0.1:6379", "IP Address of server");
DEFINE_int32(redis_timeout_ms, 1000, "RPC timeout in milliseconds");
DEFINE_int32(redis_max_retry, 3, "Max retries(not including the first RPC)");

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

  brpc::ChannelOptions options;
  options.protocol = brpc::PROTOCOL_REDIS;
  options.connection_type = FLAGS_redis_connection_type;
  options.timeout_ms = FLAGS_redis_timeout_ms/*milliseconds*/;
  options.max_retry = FLAGS_redis_max_retry;
  if (redis_channel_.Init(FLAGS_redis_server.c_str(), &options) != 0) {
    LOG(ERROR) << "Fail to initialize channel";
    exit(-1);
  }
}

DbproxyServiceImpl::~DbproxyServiceImpl() {}

void DbproxyServiceImpl::AuthAndSaveSession(google::protobuf::RpcController* controller,
                                            const SigninData* new_msg,
                                            Pong* pong,
                                            google::protobuf::Closure* done){
  // TODO 1. get password from MySQL and check

  // 2. save session in redis
  brpc::RedisRequest request;
  if (!request.AddCommand("SET %lda %s EX 3600", new_msg->user_id(), new_msg->access_addr().c_str())) {
    LOG(ERROR) << "Fail to add command";
  }
  brpc::RedisResponse response;
  brpc::Controller cntl;
  DbproxyService_Stub stub(&redis_channel_);
  stub.CallMethod(NULL, &cntl, &request, &response, NULL);
  if (cntl.Failed()) {
    LOG(ERROR) << "Fail to access redis, " << cntl.ErrorText();
  } else {
    LOG(INFO) << "redis reply=" << response;
  }

}

void DbproxyServiceImpl::ClearSession(google::protobuf::RpcController* controller,
                                      const UserId*,
                                      Pong*,
                                      google::protobuf::Closure* done){
  // TODO now session will timeout and clear itself
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
      sql << "INSERT INTO msssages(user_id, peer_id, msg_id, group_id, message, client_time, msg_time) "
             "VALUES (:user_id, :peer_id, :msg_id, :group_id, :message, :client_time, :msg_time), "
             "(:user_id2, :peer_id2, :msg_id2, :group_id2, :message2, :client_time2, :msg_time2);",
             soci::use(new_msg->user_id()),
             soci::use(new_msg->peer_id()),
             soci::use(new_msg->msg_id()),

             soci::use(0),
             soci::use(new_msg->message()),
             soci::use(new_msg->client_time()),
             soci::use(new_msg->msg_time());


             soci::use(new_msg->peer_id()),
             soci::use(new_msg->user_id()),
             soci::use(new_msg->msg_id()),

             soci::use(0),
             soci::use(new_msg->message()),
             soci::use(new_msg->client_time()),
             soci::use(new_msg->msg_time());
    }
    else{
      {
        soci::session sql(*pool);
        sql << "INSERT INTO msssages(user_id, peer_id, msg_id, group_id, message, client_time, msg_time) "
               "VALUES (:user_id, :peer_id, :msg_id, :group_id, :message, :client_time, :msg_time);",
               soci::use(new_msg->user_id()),
               soci::use(new_msg->peer_id()),
               soci::use(new_msg->msg_id()),

               soci::use(0),
               soci::use(new_msg->message()),
               soci::use(new_msg->client_time()),
               soci::use(new_msg->msg_time());
      }
      {
        soci::session sql(*peer_pool);
        sql << "INSERT INTO msssages(user_id, peer_id, msg_id, group_id, message, client_time, msg_time) "
               "VALUES (:user_id, :peer_id, :msg_id, :group_id, :message, :client_send, :msg_time);",
               soci::use(new_msg->peer_id()),
               soci::use(new_msg->user_id()),
               soci::use(new_msg->msg_id()),

               soci::use(0),
               soci::use(new_msg->message()),
               soci::use(new_msg->client_time()),
               soci::use(new_msg->msg_time());
      }
    }
  }
  catch (const soci::soci_error& err) {
    LOG(ERROR) << "Fail to insert into messages"
                << " user_id=" << new_msg->user_id()
                << " peer_id=" << new_msg->peer_id()
                << " msg_id=" << new_msg->msg_id()
                << ". " << err.what();
    return;
  }


}

void DbproxyServiceImpl::SaveGroupMsg(google::protobuf::RpcController* controller,
                                      const NewGroupMsg* new_group_msg,
                                      Reply* reply,
                                      google::protobuf::Closure* done){
  try {
    const user_id_t sender_user_id = new_group_msg->sender_user_id();
    const group_id_t group_id = new_group_msg->group_id();

    auto pool = ChooseDatabase(sender_user_id);
    soci::session sql(*pool);
    sql << "INSERT INTO msssages(user_id, peer_id, msg_id, group_id, message, client_send, msg_time) "
            "VALUES (:user_id, :peer_id, :msg_id, :group_id, :message, :client_send, :msg_time) ",
            soci::use(sender_user_id),
            soci::use(sender_user_id),
            soci::use(new_group_msg->sender_msg_id()),

            soci::use(group_id),
            soci::use(new_group_msg->message()),
            soci::use(new_group_msg->client_time()),
            soci::use(new_group_msg->msg_time());

    for (int i = 0, size = new_group_msg->user_and_msgids_size(); i < size; ++i){
      // TODO each db inserts all once
      const auto& user_and_msgid = new_group_msg->user_and_msgids(i);
      auto pool = ChooseDatabase(user_and_msgid.user_id());
      soci::session sql(*pool);
      sql << "INSERT INTO msssages(user_id, peer_id, msg_id, group_id, message, client_send, msg_time) "
              "VALUES (:user_id, :peer_id, :msg_id, :group_id, :message, :client_send, :msg_time) ",
              soci::use(user_and_msgid.user_id()),
              soci::use(sender_user_id),
              soci::use(user_and_msgid.msg_id()),

              soci::use(group_id),
              soci::use(new_group_msg->message()),
              soci::use(new_group_msg->client_time()),
              soci::use(new_group_msg->msg_time());
    }
  }
  catch (const soci::soci_error& err) {
    LOG(ERROR) << "Fail to insert into messages. " << err.what();
    return;
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

void DbproxyServiceImpl::SetUserLastSendData(google::protobuf::RpcController* controller,
                                             const UserLastSendData* user_last_send_data,
                                             Pong*,
                                             google::protobuf::Closure* done){


const char* cmd =
  "eval \""
  "local a = redis.call('GET',KEYS[1], ARGV[1])"
  " if (a == false or tonumber(cjson.decode(a)[2]) < tonumber(ARGV[2])) then"
    " redis.call('SET',KEYS[1], cjson.encode({ARGV[1],ARGV[2],ARGV[3]}), 'EX 3600')"
    " return 1"
  " else"
    " return 0"
  " end\" ";
  std::ostringstream oss;
  oss << cmd << 1 << " "
      << user_last_send_data->user_id() << "u "
      << user_last_send_data->msg_id() << " "
      << user_last_send_data->client_time() << " "
      << user_last_send_data->msg_time();


  brpc::RedisRequest request;
  if (!request.AddCommand(oss.str())) {
    LOG(ERROR) << "Fail to add command";
  }
  brpc::RedisResponse response;
  brpc::Controller cntl;
  DbproxyService_Stub stub(&redis_channel_);
  stub.CallMethod(NULL, &cntl, &request, &response, NULL);
  if (cntl.Failed()) {
    LOG(ERROR) << "Fail to access redis, " << cntl.ErrorText();
  } else {
    LOG(INFO) << "redis reply=" << response;
  }

}

void DbproxyServiceImpl::GetUserLastSendData(google::protobuf::RpcController* controller,
                                             const UserId* userid,
                                             UserLastSendData* user_last_send_data,
                                             google::protobuf::Closure* done){
  const user_id_t user_id = userid->user_id();
  brpc::RedisRequest request;
  if (!request.AddCommand("GET %lldu", user_id)) {
    LOG(ERROR) << "Fail to add command";
  }
  brpc::RedisResponse response;
  brpc::Controller cntl;
  redis_channel_.CallMethod(NULL, &cntl, &request, &response, NULL);
  if (cntl.Failed()) {
    LOG(ERROR) << "Fail to access redis, " << cntl.ErrorText();
  } else {
    LOG(INFO) << "redis reply=" << response;
    int64_t msg_id = 0;
    int client_time = 0;
    int msg_time = 0;
    if (response.reply(0).is_string()){
      sscanf(response.reply(0).c_str(), "[%ld,%d,%d]", &msg_id,
                                                    &client_time,
                                                    &msg_time);
    }
    else{
      auto pool = ChooseDatabase(user_id);
      soci::session sql(*pool);
      soci::indicator ind;
      // get data from MySQL
      sql << "SELECT msg_id, client_time, msg_time "
             "FROM messages "
             "WHERE user_id = :user_id and sender_id = :sender_id and "
               "client_time = (SELECT MAX(client_time) "
                               "FROM messages "
                               "WHERE user_id = :user_id2 AND sender_id = :sender_id2);",
             soci::into(msg_id, ind),
             soci::into(client_time),
             soci::into(msg_time),

             soci::use(user_id),
             soci::use(user_id),
             soci::use(user_id),
             soci::use(user_id);
      if (ind != soci::indicator::i_ok) {
        msg_id = 0;
        client_time = 0;
        msg_time = 0;
      }
    }
    user_last_send_data->set_msg_id(msg_id);
    user_last_send_data->set_client_time(client_time);
    user_last_send_data->set_msg_time(msg_time);
  }
}

}  // namespace tinyim

