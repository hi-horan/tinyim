
#include "idgen/idgen.h"
#include "idgen.pb.h"

#include <brpc/server.h>
#include <glog/logging.h>
#include <leveldb/status.h>

#include "util/initialize.h"

namespace tinyim{

class IdGenServiceImpl final : public IdGenService {
 public:
  IdGenServiceImpl(IdGen* id_gen):id_gen_(id_gen) {};
  virtual ~IdGenServiceImpl() {};

  virtual void IdGenerate(google::protobuf::RpcController* cntl_base,
                    const MsgIdRequest* request,
                    MsgIdReply* response,
                    google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    brpc::Controller* cntl =
        static_cast<brpc::Controller*>(cntl_base);

    LOG(INFO) << "Received request[log_id=" << cntl->log_id()
              << "] from " << cntl->remote_side()
              << " to " << cntl->local_side()
              << ": user_ids_size" << request->user_ids_size()
              << " (attached=" << cntl->request_attachment() << ")";

    // cntl->response_attachment().append(cntl->request_attachment());

    for (int i = 0; i < request->user_ids_size(); ++i){
        const int64_t user_id = request->user_ids(i).user_id();
        const int64_t need_msgid_num = request->user_ids(i).need_msgid_num();
        int64_t start_msgid = 0;
        auto status = id_gen_->IdGenerate(user_id, need_msgid_num, start_msgid);
        if (status.ok()){
          auto pmsg_id = response->add_msg_ids();
          pmsg_id->set_user_id(user_id);
          pmsg_id->set_start_msg_id(start_msgid);
          pmsg_id->set_msg_id_num(need_msgid_num);
        }
        else{
          cntl->SetFailed(status.ToString());
          return;
        }
    }
  }
 private:
  IdGen * id_gen_;
};

}  // namespace tinyim


int main(int argc, char* argv[]) {
  tinyim::Initialize init(argc, &argv);

  brpc::Server server;

  tinyim::IdGenServiceImpl id_gen_service_impl(tinyim::IdGen::Default());

  if (server.AddService(&id_gen_service_impl,
                        brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
      LOG(ERROR) << "Fail to add service";
      return -1;
  }

  // Start the server.
  // brpc::ServerOptions options;
  // options.idle_timeout_sec = FLAGS_idle_timeout_s;
  // if (server.Start(FLAGS_port, &options) != 0) {
      // LOG(ERROR) << "Fail to start EchoServer";
      // return -1;
  // }
  server.RunUntilAskedToQuit();

  return 0;
}
