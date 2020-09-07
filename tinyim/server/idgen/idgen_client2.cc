
#include <gflags/gflags.h>
#include <butil/logging.h>
#include <butil/time.h>
#include <brpc/channel.h>

#include <cstdlib>

#include "idgen.pb.h"
#include "util/initialize.h"

DEFINE_string(protocol, "baidu_std", "Protocol type. Defined in src/brpc/options.proto");
DEFINE_string(connection_type, "", "Connection type. Available values: single, pooled, short");
DEFINE_string(server, "0.0.0.0:8000", "IP Address of server");
DEFINE_string(load_balancer, "", "The algorithm for load balancing");
DEFINE_int32(timeout_ms, 100, "RPC timeout in milliseconds");
DEFINE_int32(max_retry, 3, "Max retries(not including the first RPC)");
DEFINE_int32(interval_ms, 1000, "Milliseconds between consecutive requests");
DEFINE_int32(each_request_msgid_num, 1, "Each request msgid num");
DEFINE_int32(change_logid, 0, "Change logid");
DEFINE_int32(unchange_user_id, 10, "Change logid");
DEFINE_int32(user_id, 1234, "user id");

int main(int argc, char* argv[]) {
  tinyim::Initialize init(argc, &argv);

  brpc::ChannelOptions options;
  options.protocol = FLAGS_protocol;
  options.connection_type = FLAGS_connection_type;
  options.timeout_ms = FLAGS_timeout_ms/*milliseconds*/;
  options.max_retry = FLAGS_max_retry;
  brpc::Channel channel;
  if (channel.Init(FLAGS_server.c_str(), FLAGS_load_balancer.c_str(), &options) != 0) {
      LOG(ERROR) << "Fail to initialize channel";
      return -1;
  }

  tinyim::IdGenService_Stub stub(&channel);

  int log_id = 0;
  while (!brpc::IsAskedToQuit()) {
      tinyim::MsgIdRequest request;
      tinyim::MsgIdReply reply;
      brpc::Controller cntl;

      auto user_and_id_num = request.add_user_ids();
      int user_id = FLAGS_user_id;
      LOG(INFO) << "Requesting user_id=" << user_id << " msgid num="
                << FLAGS_each_request_msgid_num;
      user_and_id_num->set_need_msgid_num(FLAGS_each_request_msgid_num);
      user_and_id_num->set_user_id(user_id);

      if (FLAGS_change_logid){
        cntl.set_log_id(log_id++);  // set by user
      }
      else{
        cntl.set_log_id(log_id);  // set by user
      }
      LOG(INFO) << "log id=" << log_id;

      stub.IdGenerate(&cntl, &request, &reply, nullptr);
      if (!cntl.Failed()) {
          LOG(INFO) << "Received response from " << cntl.remote_side()
              << " to " << cntl.local_side()
              << " reply msg ids size=" << reply.msg_ids_size() << " (attached="
              << " user_id=" << reply.msg_ids(0).user_id()
              << " start migid=" << reply.msg_ids(0).start_msg_id()
              << " msgid_num=" << reply.msg_ids(0).msg_id_num()
              << cntl.response_attachment() << ")"
              << " latency=" << cntl.latency_us() << "us";
      } else {
          LOG(WARNING) << cntl.ErrorText();
      }
      usleep(FLAGS_interval_ms * 1000L);
  }
  return 0;
}
