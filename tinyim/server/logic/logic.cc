#include "logic/logic_service.h"

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <brpc/server.h>
#include <brpc/channel.h>
#include <butil/status.h>

#include "util/initialize.h"
#include "type.h"

DEFINE_int32(port, 6000, "TCP Port of this server");
DEFINE_int32(idle_timeout_s, -1, "Connection will be closed if there is no "
             "read/write operations during the last `idle_timeout_s'");
DEFINE_int32(logoff_ms, 2000, "Maximum duration of server's LOGOFF state "
             "(waiting for client to close connection before server stops)");

DEFINE_string(id_server_addr, "127.0.0.1:8000", "Id server address");
DEFINE_int32(max_retry, 3, "Max retries(not including the first RPC)");
DEFINE_string(connection_type, "single", "Connection type. Available values: single, pooled, short");
DEFINE_int32(timeout_ms, 100, "RPC timeout in milliseconds");

DEFINE_string(db_addr, "127.0.0.1:7000", "Id server address");

// DEFINE_string(logic_server, "file://server_list", "Mapping to servers");
// DEFINE_string(logic_load_balancer, "c_murmurhash", "Name of load balancer");
// DEFINE_string(connection_type, "single", "Connection type. Available values: single, pooled, short");
// DEFINE_int32(timeout_ms, 100, "RPC timeout in milliseconds");


int main(int argc, char* argv[]) {
  tinyim::Initialize init(argc, &argv);

  brpc::Channel id_channel;
  brpc::ChannelOptions options;
  options.protocol = brpc::PROTOCOL_BAIDU_STD;
  options.connection_type = FLAGS_connection_type;
  options.timeout_ms = FLAGS_timeout_ms/*milliseconds*/;
  options.max_retry = FLAGS_max_retry;
  id_channel.Init(FLAGS_id_server_addr.c_str(), &options);

  brpc::Channel db_channel;
  brpc::ChannelOptions db_options;
  db_options.protocol = brpc::PROTOCOL_BAIDU_STD;
  db_options.connection_type = FLAGS_connection_type;
  db_options.timeout_ms = FLAGS_timeout_ms/*milliseconds*/;
  db_options.max_retry = FLAGS_max_retry;
  db_channel.Init(FLAGS_db_addr.c_str(), &db_options);
  tinyim::LogicServiceImpl logic_service_impl(&id_channel, &db_channel);
  brpc::Server server;
  if (server.AddService(&logic_service_impl, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
    LOG(ERROR) << "Fail to add service";
    return -1;
  }

  brpc::ServerOptions server_options;
  server_options.idle_timeout_sec = FLAGS_idle_timeout_s;
  if (server.Start(FLAGS_port, &server_options) != 0) {
    LOG(ERROR) << "Fail to start access";
    return -1;
  }

  while (!brpc::IsAskedToQuit()) {
    bthread_usleep(1000000L);
  }
  // access_service_impl.Clear();

  server.RunUntilAskedToQuit();

  return 0;
}
