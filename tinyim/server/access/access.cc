#include "access/access_service.h"

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <brpc/server.h>
// #include <bthread/bthread.h>
// #include <bthread/unstable.h>
#include <brpc/channel.h>
#include <butil/status.h>
// #include <butil/time.h>

#include "util/initialize.h"
#include "type.h"

DEFINE_int32(port, 5000, "TCP Port of this server");
DEFINE_int32(idle_timeout_s, -1, "Connection will be closed if there is no "
             "read/write operations during the last `idle_timeout_s'");
DEFINE_int32(logoff_ms, 2000, "Maximum duration of server's LOGOFF state "
             "(waiting for client to close connection before server stops)");
// DEFINE_int32(vnode_count, 300, "Virtual node count");
DEFINE_string(id_server_addr, "192.168.1.100:8000", "Id server address");
DEFINE_int32(max_retry, 3, "Max retries(not including the first RPC)");

DEFINE_string(logic_server, "file://server_list", "Mapping to servers");
DEFINE_string(db_server, "127.0.0.1:7000", "Mapping to servers");
DEFINE_string(logic_load_balancer, "c_murmurhash", "Name of load balancer");
DEFINE_string(connection_type, "single", "Connection type. Available values: single, pooled, short");
DEFINE_int32(timeout_ms, 100, "RPC timeout in milliseconds");


int main(int argc, char* argv[]) {
  tinyim::Initialize init(argc, &argv);

  brpc::Server server;

  brpc::Channel logic_channel;
  brpc::ChannelOptions options;
  options.protocol = brpc::PROTOCOL_BAIDU_STD;
  options.connection_type = FLAGS_connection_type;
  options.timeout_ms = FLAGS_timeout_ms/*milliseconds*/;
  options.max_retry = FLAGS_max_retry;
  if (logic_channel.Init(FLAGS_logic_server.c_str(),
                         FLAGS_logic_load_balancer.c_str(), &options) != 0) {
    LOG(ERROR) << "Fail to initialize channel";
    return -1;
  }
  brpc::Channel db_channel;
  brpc::ChannelOptions db_options;
  db_options.protocol = brpc::PROTOCOL_BAIDU_STD;
  db_options.connection_type = FLAGS_connection_type;
  db_options.timeout_ms = FLAGS_timeout_ms/*milliseconds*/;
  db_options.max_retry = FLAGS_max_retry;
  if (db_channel.Init(FLAGS_db_server.c_str(), &options) != 0) {
    LOG(ERROR) << "Fail to initialize channel";
    return -1;
  }

  tinyim::AccessServiceImpl access_service_impl(&logic_channel, &db_channel);

  if (server.AddService(&access_service_impl,
                        brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
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
  access_service_impl.Clear();

  server.RunUntilAskedToQuit();

  return 0;
}
