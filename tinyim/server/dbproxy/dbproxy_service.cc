#include "dbproxy/dbproxy_service.h"

#include <cstdio>
#include <sstream>

#include <gflags/gflags.h>
#include <glog/logging.h>

DEFINE_string(db_connect_info, "dbname=tinyim user=root", "Messages database connect information");
DEFINE_string(db_name, "mysql", "Database name");

DEFINE_string(db_group_member_connect_info, "dbname=tinyim user=root", "Group members database connect information");
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

void DbproxyServiceImpl::Test(google::protobuf::RpcController* controller,
                            const Ping* ping,
                            Pong* pong,
                            google::protobuf::Closure* done){
  brpc::ClosureGuard done_guard(done);
  DLOG(INFO) << "Running test";
}

void DbproxyServiceImpl::AuthAndSaveSession(google::protobuf::RpcController* controller,
                                            const SigninData* new_msg,
                                            Pong* pong,
                                            google::protobuf::Closure* done){
  brpc::ClosureGuard done_guard(done);
  brpc::Controller* pcntl = static_cast<brpc::Controller*>(controller);
  // TODO 1. get password from MySQL and check

  // 2. save session in redis
  brpc::RedisRequest request;
  if (!request.AddCommand("SET {%ld}a %s EX 3600", new_msg->user_id(), new_msg->access_addr().c_str())) {
    LOG(ERROR) << "Fail to add command";
  }
  brpc::RedisResponse response;
  brpc::Controller cntl;
  redis_channel_.CallMethod(NULL, &cntl, &request, &response, NULL);
  if (cntl.Failed()) {
    LOG(ERROR) << "Fail to access redis, " << cntl.ErrorText();
    pcntl->SetFailed(cntl.ErrorCode(), cntl.ErrorText().c_str());
  } else {
    LOG(INFO) << "redis reply=" << response;
  }
}

void DbproxyServiceImpl::GetSessions(google::protobuf::RpcController* controller,
                                     const UserIds* user_ids,
                                     Sessions* sessions,
                                     google::protobuf::Closure* done){
  brpc::ClosureGuard done_guard(done);
  brpc::Controller* pcntl = static_cast<brpc::Controller*>(controller);
  std::ostringstream oss;
  oss << "MGET ";
  int size = user_ids->user_id_size();
  for (int i = 0; i < size; ++i){
    oss << "{" << user_ids->user_id(i) << "}a ";
  }
  brpc::RedisRequest request;
  if (!request.AddCommand(oss.str().c_str())) {
    LOG(ERROR) << "Fail to call " << oss.str();
  }
  brpc::RedisResponse response;
  brpc::Controller cntl;
  DLOG(INFO) << "Querying redis. " << oss.str();
  redis_channel_.CallMethod(NULL, &cntl, &request, &response, NULL);
  if (cntl.Failed()) {
    LOG(ERROR) << "Fail to access redis, " << cntl.ErrorText();
    pcntl->SetFailed(cntl.ErrorCode(), cntl.ErrorText().c_str());
  } else {
    DLOG(INFO) << "redis reply(0) size=" << response.reply(0).size();
    DLOG(INFO) << "redis reply=" << response;
    CHECK_EQ(response.reply(0).size(), user_ids->user_id_size());
    if (response.reply(0).is_array()){
      for (size_t i = 0; i < response.reply(0).size(); ++i){
        DLOG(INFO) << "reply(0).[" << i << "]=" << " is_string=" << response.reply(0)[i].is_string();
        DLOG(INFO) << "is_array=" << response.reply(0)[i].is_array();
        DLOG(INFO) << "is_error=" << response.reply(0)[i].is_error();
        DLOG(INFO) << "is_nil=" << response.reply(0)[i].is_nil();
        DLOG(INFO) << "is_integer=" << response.reply(0)[i].is_integer();

        auto session = sessions->add_session();
        session->set_user_id(user_ids->user_id(i));
        if (response.reply(0)[i].is_string()){
          session->set_has_session(true);
          session->set_addr(response.reply(0)[i].c_str());
        }
        else{
          session->set_has_session(false);
        }
        DLOG(INFO) << "session.has_session=" << session->has_session() << " "
                   << "session.user_id=" << session->user_id() << " "
                   << "session.addr="  << session->addr();
      }
    }
  }
}

void DbproxyServiceImpl::ClearSession(google::protobuf::RpcController* controller,
                                      const UserId* user_id,
                                      Pong*,
                                      google::protobuf::Closure* done){
  brpc::ClosureGuard done_guard(done);
  brpc::Controller* pcntl = static_cast<brpc::Controller*>(controller);
  brpc::RedisRequest request;
  if (!request.AddCommand("DEL {%ld}a", user_id->user_id())) {
    LOG(ERROR) << "Fail to del command";
  }
  brpc::RedisResponse response;
  brpc::Controller cntl;
  redis_channel_.CallMethod(NULL, &cntl, &request, &response, NULL);
  if (cntl.Failed()) {
    LOG(ERROR) << "Fail to del " << user_id->user_id() << ". " << cntl.ErrorText();
    pcntl->SetFailed(cntl.ErrorCode(), cntl.ErrorText().c_str());
  } else {
    DLOG(INFO) << "Redis reply=" << response;
  }
}

void DbproxyServiceImpl::SavePrivateMsg(google::protobuf::RpcController* controller,
                                        const NewPrivateMsg* new_msg,
                                        Reply* reply,
                                        google::protobuf::Closure* done){
  brpc::ClosureGuard done_guard(done);
  brpc::Controller* cntl = static_cast<brpc::Controller*>(controller);
  try {
    auto pool = ChooseDatabase(new_msg->sender());
    auto peer_pool = ChooseDatabase(new_msg->receiver());
    if (pool == peer_pool){
      soci::session sql(*pool);
      sql << "INSERT INTO messages(user_id, sender, receiver, msg_id, group_id, message, client_time, msg_time) "
             "VALUES (:user_id, :sender, :receiver, :msg_id, :group_id, :message, FROM_UNIXTIME(:client_time), FROM_UNIXTIME(:msg_time)), "
             "(:receiver2, :sender2, :receiver2, :receiver_msg_id2, :group_id2, :message2, FROM_UNIXTIME(:client_time2), FROM_UNIXTIME(:msg_time2));",
             soci::use(new_msg->sender()),
             soci::use(new_msg->sender()),
             soci::use(new_msg->receiver()),
             soci::use(new_msg->sender_msg_id()),

             soci::use(0),
             soci::use(new_msg->message()),
             soci::use(new_msg->client_time()),
             soci::use(new_msg->msg_time()),


             soci::use(new_msg->receiver()),
             soci::use(new_msg->sender()),
             soci::use(new_msg->receiver()),
             soci::use(new_msg->receiver_msg_id()),

             soci::use(0),
             soci::use(new_msg->message()),
             soci::use(new_msg->client_time()),
             soci::use(new_msg->msg_time());

      LOG(INFO) << "sql= " << sql.get_query();
    }
    else{
      {
        soci::session sql(*pool);
        sql << "INSERT INTO messages(user_id, sender, receiver, msg_id, group_id, message, client_time, msg_time) "
               "VALUES (:user_id, :sender, :receiver, :msg_id, :group_id, :message, FROM_UNIXTIME(:client_time), FROM_UNIXTIME(:msg_time));",
               soci::use(new_msg->sender()),
               soci::use(new_msg->sender()),
               soci::use(new_msg->receiver()),
               soci::use(new_msg->sender_msg_id()),

               soci::use(0),
               soci::use(new_msg->message()),
               soci::use(new_msg->client_time()),
               soci::use(new_msg->msg_time());
      }
      {
        soci::session sql(*peer_pool);
        sql << "INSERT INTO messages(user_id, sender, receiver, msg_id, group_id, message, client_time, msg_time) "
               "VALUES (:user_id, :sender, :receiver, :receiver_msg_id, :group_id, :message, FROM_UNIXTIME(:client_send), FROM_UNIXTIME(:msg_time));",
               soci::use(new_msg->receiver()),
               soci::use(new_msg->sender()),
               soci::use(new_msg->receiver()),
               soci::use(new_msg->receiver_msg_id()),

               soci::use(0),
               soci::use(new_msg->message()),
               soci::use(new_msg->client_time()),
               soci::use(new_msg->msg_time());
      }
    }
  }
  catch (const soci::soci_error& err) {
    LOG(ERROR) << "Fail to insert into messages"
                << " sender=" << new_msg->sender()
                << " receiver=" << new_msg->receiver()
                << " msg_id=" << new_msg->sender_msg_id()
                << ". " << err.what();
    cntl->SetFailed(EINVAL, "Fail to insert into messages.");
    return;
  }

  UserLastSendData user_last_send_data;
  user_last_send_data.set_user_id(new_msg->sender());
  user_last_send_data.set_client_time(new_msg->client_time());
  user_last_send_data.set_msg_time(new_msg->msg_time());
  user_last_send_data.set_msg_id(new_msg->sender_msg_id());
  SetUserLastSendData_(cntl, &user_last_send_data);
}

void DbproxyServiceImpl::SaveGroupMsg(google::protobuf::RpcController* controller,
                                      const NewGroupMsg* new_group_msg,
                                      Reply* reply,
                                      google::protobuf::Closure* done){
  brpc::ClosureGuard done_guard(done);
  brpc::Controller* cntl = static_cast<brpc::Controller*>(controller);
  const user_id_t sender_user_id = new_group_msg->sender_user_id();
  const group_id_t group_id = new_group_msg->group_id();
  try {
    for (int i = 0, size = new_group_msg->user_and_msgids_size(); i < size; ++i){
      // TODO each db inserts all once
      const auto& user_and_msgid = new_group_msg->user_and_msgids(i);
      auto pool = ChooseDatabase(user_and_msgid.user_id());
      soci::session sql(*pool);
      sql << "INSERT INTO messages(user_id, sender, receiver, msg_id, group_id, message, client_time, msg_time) "
              "VALUES (:user_id, :sender, :receiver, :msg_id, :group_id, :message, FROM_UNIXTIME(:client_send), FROM_UNIXTIME(:msg_time)) ",
              soci::use(user_and_msgid.user_id()),
              soci::use(sender_user_id),
              soci::use(group_id),
              soci::use(user_and_msgid.msg_id()),

              soci::use(group_id),
              soci::use(new_group_msg->message()),
              soci::use(new_group_msg->client_time()),
              soci::use(new_group_msg->msg_time());
    }
  }
  catch (const std::exception& err) {
    LOG(ERROR) << "Fail to insert into messages. " << err.what();
    cntl->SetFailed(EINVAL, "Fail to insert into messages.");
    return;
  }

  UserLastSendData user_last_send_data;
  user_last_send_data.set_user_id(sender_user_id);
  user_last_send_data.set_client_time(new_group_msg->client_time());
  user_last_send_data.set_msg_time(new_group_msg->msg_time());
  user_last_send_data.set_msg_id(new_group_msg->sender_msg_id());
  SetUserLastSendData_(cntl, &user_last_send_data);
}

void DbproxyServiceImpl::SetUserLastSendData_(brpc::Controller* pcntl,
                                              const UserLastSendData* user_last_send_data){
  const char* cmd =
    "eval \""
    "local a = redis.call('GET', KEYS[1])"
    " if (a == false or tonumber(cjson.decode(a)[2]) < tonumber(ARGV[2])) then"
      " redis.call('SETEX', KEYS[1], 3600, cjson.encode({ARGV[1],ARGV[2],ARGV[3]}))"
      " return 1"
    " else"
      " return 0"
    " end\" ";
  std::ostringstream oss;
  oss << cmd << 1 << " "
      << "{" << user_last_send_data->user_id() << "}" << "u "
      << user_last_send_data->msg_id() << " "
      << user_last_send_data->client_time() << " "
      << user_last_send_data->msg_time();

  brpc::RedisRequest request;
  if (!request.AddCommand(oss.str())) {
    LOG(ERROR) << "Fail to add command";
  }
  brpc::RedisResponse response;
  brpc::Controller cntl;
  redis_channel_.CallMethod(NULL, &cntl, &request, &response, NULL);
  if (cntl.Failed()) {
    LOG(ERROR) << "Fail to access redis, " << cntl.ErrorText();
    pcntl->SetFailed(EINVAL, "Fail to set user last send data.");
  } else {
    LOG(INFO) << "redis reply=" << response;
  }
}

void DbproxyServiceImpl::SetUserLastSendData(google::protobuf::RpcController* controller,
                                             const UserLastSendData* user_last_send_data,
                                             Pong*,
                                             google::protobuf::Closure* done){
  brpc::ClosureGuard done_guard(done);
  brpc::Controller* pcntl = static_cast<brpc::Controller*>(controller);

  SetUserLastSendData_(pcntl, user_last_send_data);
}

void DbproxyServiceImpl::GetUserLastSendData(google::protobuf::RpcController* controller,
                                             const UserId* userid,
                                             UserLastSendData* user_last_send_data,
                                             google::protobuf::Closure* done){
  brpc::ClosureGuard done_guard(done);
  brpc::Controller* pcntl = static_cast<brpc::Controller*>(controller);

  const user_id_t user_id = userid->user_id();
  brpc::RedisRequest request;
  if (!request.AddCommand("GET {%ld}u", user_id)) {
    LOG(ERROR) << "Fail to add command";
  }
  brpc::RedisResponse response;
  brpc::Controller cntl;
  redis_channel_.CallMethod(NULL, &cntl, &request, &response, NULL);
  if (cntl.Failed()) {
    LOG(ERROR) << "Fail to access redis, " << cntl.ErrorText();
    pcntl->SetFailed(EINVAL, "Fail to get data from redis.");
  } else {
    LOG(INFO) << "redis reply=" << response;
    int64_t msg_id = 0;
    int client_time = 0;
    int msg_time = 0;
    if (response.reply(0).is_string()) {
      sscanf(response.reply(0).c_str(), "[%ld,%d,%d]", &msg_id,
                                                       &client_time,
                                                       &msg_time);
    }
    else{
      try {
        auto pool = ChooseDatabase(user_id);
        soci::session sql(*pool);
        soci::indicator ind;
        sql << "SELECT msg_id, UNIX_TIMESTAMP(client_time), UNIX_TIMESTAMP(msg_time) "
              "FROM messages "
              "WHERE user_id = :user_id and sender = :sender and "
                "client_time = (SELECT MAX(client_time) "
                                "FROM messages "
                                "WHERE user_id = :user_id2 AND sender = :sender2);",
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
      catch (const std::exception& err) {
        LOG(ERROR) << "Fail to insert into messages. " << err.what();
        pcntl->SetFailed(EINVAL, "Fail to select from messages.");
        return;
      }
    }
    user_last_send_data->set_msg_id(msg_id);
    user_last_send_data->set_client_time(client_time);
    user_last_send_data->set_msg_time(msg_time);
  }
}

void DbproxyServiceImpl::GetMsgs(google::protobuf::RpcController* controller,
                                 const MsgIdRange* msg_range,
                                 Msgs* msgs,
                                 google::protobuf::Closure* done) {
  brpc::ClosureGuard done_guard(done);
  brpc::Controller* pcntl = static_cast<brpc::Controller*>(controller);

  const user_id_t user_id = msg_range->user_id();
  auto pool = ChooseDatabase(user_id);
  soci::session sql(*pool);
  try {
    soci::rowset<soci::row> rs = (sql.prepare << "SELECT sender, receiver, msg_id, group_id, message, "
                                                  "UNIX_TIMESTAMP(client_time), UNIX_TIMESTAMP(msg_time) "
                                                "FROM messages "
                                                "WHERE user_id = :user_id AND msg_id BETWEEN :start_msg_id AND :end_msg_id and deleted = 0",
                                                soci::use(msg_range->user_id()),
                                                soci::use(msg_range->start_msg_id()),
                                                soci::use(msg_range->end_msg_id()));

    for (auto it = rs.begin(); it != rs.end(); ++it) {
      soci::row const& row = *it;

      auto msg = msgs->add_msg();
      msg->set_user_id(user_id);
      msg->set_sender(row.get<long long>(0));
      msg->set_receiver(row.get<long long>(1));
      msg->set_msg_id(row.get<long long>(2));
      msg->set_group_id(row.get<long long>(3));
      msg->set_message(row.get<std::string>(4));
      msg->set_client_time(static_cast<int>(row.get<long long>(5)));
      msg->set_msg_time(static_cast<int>(row.get<long long>(6)));

      DLOG(INFO) << "user_id=" << msg->user_id()
                << " sender=" << msg->sender()
                << " receiver=" << msg->receiver()
                << " msg_id=" << msg->msg_id()
                << " group_id=" << msg->group_id()
                << " message=" << msg->message()
                << " client_time=" << msg->client_time()
                << " msg_time=" << msg->msg_time();
    }
  }
  catch (const soci::soci_error& err) {
    LOG(ERROR) << err.what();
    pcntl->SetFailed(EINVAL, "Fail to select from messages.");
  }
  DLOG_IF(INFO, msgs->msg_size() == 0) << "Select return nil. user_id=" << msg_range->user_id()
                                       << "start_msg_id=" << msg_range->start_msg_id()
                                       << "end_msg_id=" << msg_range->end_msg_id();
}

void DbproxyServiceImpl::GetFriends(google::protobuf::RpcController* controller,
                                   const UserId* userid,
                                   UserInfos* user_infos,
                                   google::protobuf::Closure* done) {
  brpc::ClosureGuard done_guard(done);
  brpc::Controller* pcntl = static_cast<brpc::Controller*>(controller);
  const user_id_t user_id = userid->user_id();

  auto pool = ChooseDatabase(user_id);
  soci::session sql(*pool);
  // soci::indicator ind;
  try {
    soci::rowset<soci::row> rs = (sql.prepare << "SELECT peer_id, peer_name "
                                                 "FROM friends "
                                                 "WHERE user_id = :user_id AND deleted = 0",
                                                soci::use(user_id));
    DLOG(INFO) << "sql=" << sql.get_query();

    for (auto it = rs.begin(); it != rs.end(); ++it) {
      soci::row const& row = *it;

      auto user_info = user_infos->add_user_info();
      user_info->set_user_id(row.get<long long>(0));
      user_info->set_name(row.get<std::string>(1));

      DLOG(INFO) << "peer_id=" << user_info->user_id()
                 << " name=" << user_info->name();
    }
  }
  catch (const soci::soci_error& err) {
    LOG(ERROR) << err.what();
    pcntl->SetFailed(EINVAL, "Fail to select from friends.");
  }
  DLOG_IF(INFO, user_infos->user_info_size() == 0) << "Select friends return nil. user_id=" << user_id;
}

void DbproxyServiceImpl::GetGroups(google::protobuf::RpcController* controller,
                                   const UserId* userid,
                                   GroupInfos* group_infos,
                                   google::protobuf::Closure* done) {
  brpc::ClosureGuard done_guard(done);
  brpc::Controller* pcntl = static_cast<brpc::Controller*>(controller);
  const user_id_t user_id = userid->user_id();

  auto pool = ChooseDatabase(user_id);
  soci::session sql(*pool);
  // soci::indicator ind;
  try {
    soci::rowset<soci::row> rs = (sql.prepare << "SELECT group_id, group_name "
                                                 "FROM group_members "
                                                 "WHERE user_id = :user_id",
                                                soci::use(user_id));
    DLOG(INFO) << "sql=" << sql.get_query();

    for (auto it = rs.begin(); it != rs.end(); ++it) {
      soci::row const& row = *it;

      auto group_info = group_infos->add_group_info();
      group_info->set_group_id(row.get<long long>(0));
      group_info->set_name(row.get<std::string>(1));

      DLOG(INFO) << "group_id=" << group_info->group_id()
                 << " name=" << group_info->name();
    }
  }
  catch (const soci::soci_error& err) {
    LOG(ERROR) << err.what();
    pcntl->SetFailed(EINVAL, "Fail to select from group_members.");
  }
  DLOG_IF(INFO, group_infos->group_info_size() == 0) << "Select group_members return nil. user_id=" << user_id;
}

void DbproxyServiceImpl::GetGroupMembers(google::protobuf::RpcController* controller,
                                         const GroupId* groupid,
                                         UserInfos* user_infos,
                                         google::protobuf::Closure* done) {
  brpc::ClosureGuard done_guard(done);
  brpc::Controller* pcntl = static_cast<brpc::Controller*>(controller);
  const group_id_t group_id = groupid->group_id();

  auto pool = ChooseDatabase(group_id);
  soci::session sql(*pool);
  try {
    soci::rowset<soci::row> rs = (sql.prepare << "SELECT user_id, user_name "
                                                 "FROM group_members "
                                                 "WHERE group_id = :group_id",
                                                soci::use(group_id));
    DLOG(INFO) << "sql=" << sql.get_query();

    for (auto it = rs.begin(); it != rs.end(); ++it) {
      soci::row const& row = *it;

      auto user_info = user_infos->add_user_info();
      user_info->set_user_id(row.get<long long>(0));
      user_info->set_name(row.get<std::string>(1));

      DLOG(INFO) << "user_id=" << user_info->user_id()
                 << " name=" << user_info->name();
    }
  }
  catch (const soci::soci_error& err) {
    LOG(ERROR) << err.what();
    pcntl->SetFailed(EINVAL, "Fail to select from group_members.");
  }
  DLOG_IF(INFO, user_infos->user_info_size() == 0) << "Select group_members return nil. group_id=" << group_id;
}

}  // namespace tinyim
