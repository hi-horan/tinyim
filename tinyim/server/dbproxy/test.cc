#include "dbproxy/dbproxy_service.h"

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <brpc/server.h>
#include <brpc/channel.h>
#include <butil/status.h>

#include "util/initialize.h"
#include "type.h"

#include <cstdio>
#include <sstream>

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <soci/soci.h>

DEFINE_int32(port, 7000, "TCP Port of this server");
DEFINE_int32(idle_timeout_s, -1, "Connection will be closed if there is no "
             "read/write operations during the last `idle_timeout_s'");
DEFINE_int32(logoff_ms, 2000, "Maximum duration of server's LOGOFF state "
             "(waiting for client to close connection before server stops)");

DEFINE_string(id_server_addr, "127.0.0.1:9000", "Id server address");
DEFINE_int32(max_retry, 3, "Max retries(not including the first RPC)");
DEFINE_string(connection_type, "single", "Connection type. Available values: single, pooled, short");
DEFINE_int32(timeout_ms, 100, "RPC timeout in milliseconds");

DEFINE_string(db_addr, "127.0.0.1:8000", "Id server address");


DEFINE_string(db_connect_info, "dbname=tinyim user=root", "Messages database connect information");
DEFINE_string(db_name, "mysql", "Database name");

DEFINE_string(db_group_member_connect_info, "dbname=test user=root", "Group members database connect information");
DEFINE_string(db_group_member_name, "mysql", "Database name");

DEFINE_string(redis_connection_type, "", "Connection type. Available values: single, pooled, short");
DEFINE_string(redis_server, "127.0.0.1:6379", "IP Address of server");
DEFINE_int32(redis_timeout_ms, 1000, "RPC timeout in milliseconds");
DEFINE_int32(redis_max_retry, 3, "Max retries(not including the first RPC)");

using namespace tinyim;



int test1() {
  using namespace tinyim;
  soci::connection_pool pool(10);
  for (int i = 0; i < 10; ++i){
    pool.at(i).open(FLAGS_db_name, FLAGS_db_connect_info);
  }
  auto msgs = std::make_unique<Msgs>();

  user_id_t user_id = 123;
  msg_id_t start_msg_id = 0;
  msg_id_t end_msg_id = 100000;

  try {
    soci::session sql(pool);
    // soci::indicator ind;
    soci::rowset<soci::row> rs = (sql.prepare << "SELECT sender, receiver, msg_id, group_id, message, "
                                                  "UNIX_TIMESTAMP(client_time), UNIX_TIMESTAMP(msg_time) "
                                                "FROM messages "
                                                "WHERE user_id = :user_id AND msg_id BETWEEN :start_msg_id AND :end_msg_id and deleted = 0",
                                                soci::use(user_id),
                                                soci::use(start_msg_id),
                                                soci::use(end_msg_id));

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
  }

  return 0;
}

int test2(int argc, char* argv[]) {
  tinyim::Initialize init(argc, &argv);

  soci::connection_pool pool(10);
  for (int i = 0; i < 10; ++i){
    pool.at(i).open(FLAGS_db_name, FLAGS_db_connect_info);
  }
  soci::session sql(pool);
  soci::indicator ind;
  // get data from MySQL
  msg_id_t msg_id = 0;
  user_id_t user_id = 123;
  int client_time = 0;
  int msg_time = 0;

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
  LOG(INFO) << "user_id=" << user_id << " "
            << "client_time=" << client_time << " "
            << "msg_time=" << msg_time << " "
            << "msg_id=" << msg_id << " ";


  return 0;
}

int main(int argc, char* argv[]) {
  test1();

  return 0;
}