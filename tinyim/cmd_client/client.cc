#include "access/access.pb.h"

#include <iostream>

#include <gflags/gflags.h>
#include <brpc/channel.h>
#include <bthread/bthread.h>
#include <bthread/unstable.h>
#include <brpc/stream.h>
#include <butil/time.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "type.h"
#include "util/initialize.h"


DEFINE_string(connection_type, "single", "Connection type. Available values: single, pooled, short");
DEFINE_string(server, "0.0.0.0:5000", "IP Address of server");
DEFINE_int32(timeout_ms, 100, "RPC timeout in milliseconds");
DEFINE_int32(max_retry, 3, "Max retries(not including the first RPC)");
DEFINE_int32(internal_send_heartbeat_s, 300, "Internal time that send heartbeat");
DEFINE_int32(user_id, 123, "Internal time that send heartbeat");
DEFINE_string(password, "xxxxxx", "user password");

namespace tinyim {

struct Freer {
  void operator()(char* mem) {
    ::free(mem);
  }
};

static bool g_canceled = false;
static int cli_getc(FILE *stream) {
    int c = getc(stream);
    if (c == EOF && errno == EINTR) {
        g_canceled = true;
        return '\n';
    }
    return c;
}

// void completion(const char *buf, linenoiseCompletions *lc) {
  // if (buf[0] == 's') {
    // linenoiseAddCompletion(lc,"sendmsgto");
  // }
// }

// const char *hints(const char *buf, int *color, int *bold) {
  // if (!strcasecmp(buf,"sendmsgto")) {
    // *color = 35;
    // *bold = 0;
    // return " <userid> <msg>";
  // }
  // return NULL;
// }

struct PullDataArgs {
  tinyim::user_id_t user_id;
  brpc::Channel* channel;
};

// run in bthread
void* PullData(void *arg){
  std::cout << "PullData bthread running" << std::endl;
  auto args = static_cast<PullDataArgs*>(arg);
  auto pchannel = args->channel;
  auto user_id = args->user_id;
  delete args;

  while (!brpc::IsAskedToQuit()) {
    brpc::Controller cntl;
    tinyim::Ping ping;
    ping.set_user_id(user_id);
    tinyim::Msgs msgs;
    tinyim::AccessService_Stub stub(pchannel);

    std::cout << "Calling PullData" << std::endl;
    cntl.set_timeout_ms(0x7fffffff);
    stub.PullData(&cntl, &ping, &msgs, nullptr);
    if (cntl.Failed()){
      std::cout << "Fail to call PullData. " << cntl.ErrorText() << std::endl;
      bthread_usleep(1000000L);
      continue;
    }
    // bthread_usleep(1000000L);
    const size_t msg_size = msgs.msg_size();
    if (msg_size > 0){
      std::cout << std::endl;
      std::cout << "  Received msg:" << std::endl;
    }
    for (size_t i = 0; i < msg_size; ++i){
      const Msg& msg = msgs.msg(i);
      std::cout << "    userid=" << msg.user_id()
                << " sender=" << msg.sender()
                << " receiver=" << msg.receiver()
                << " msgid=" << msg.msg_id()
                << " message=" << msg.message()
                << " client_time=" << msg.client_time()
                << " msg_time=" << msg.msg_time() << std::endl;
      }
    }
  return nullptr;
}

// void* SendHeartBeat(void* arg){
  // auto pchannel = static_cast<brpc::Channel*>(arg);
  // static const uint64_t sleep_time_s = FLAGS_internal_send_heartbeat_s * 1000 * 1000;

  // brpc::Controller cntl;
  // tinyim::Ping ping;
  // ping.set_user_id(user_id);
  // tinyim::Pong pong;
  // auto msg_last_msg_id = pong.msg_last_msg_id();
  // tinyim::AccessService_Stub stub(pchannel);
  // stub.HeartBeat(&cntl, &ping, &ping, nullptr);
  // if (cntl.Failed()){
    // LOG(ERROR) << "Fail to call heartbeat. " << cntl.ErrorText();
    // return nullptr;
  // }
  // else{
    // bthread_usleep(sleep_time_s);
  // }
// }

// class StreamReceiveHandler: public brpc::StreamInputHandler{
  // virtual int on_received_messages(brpc::StreamId id,
                                   // butil::IOBuf *const messages[],
                                   // size_t size) {
    // std::ostringstream os;
    // for (size_t i = 0; i < size; ++i) {
        // os << "msg[" << i << "]=" << *messages[i];
    // }
    // LOG(INFO) << "Received from Stream=" << id << ": " << os.str();
    // return 0;
  // }
  // virtual void on_idle_timeout(brpc::StreamId id) {
    // LOG(INFO) << "Stream=" << id << " has no data transmission for a while";
  // }

  // virtual void on_closed(brpc::StreamId id) {
    // LOG(INFO) << "Stream=" << id << " is closed";
    // // brpc::StreamClose(id);
  // }
// };

// FIXME maybe need lock
struct HeartBeatArg{
  brpc::Channel* channel;
  tinyim::user_id_t user_id;
  msg_id_t cur_user_id;
  bthread_timer_t heartbeat_timeout_id;
};

void HeartBeatTimeOutHeadler(void* arg){
  std::cout << "Running heartbeat send" << std::endl;
  const auto parg = static_cast<HeartBeatArg*>(arg);
  // parg->channel;
  brpc::Controller cntl;
  tinyim::Ping ping;
  ping.set_user_id(parg->user_id);
  tinyim::Pong pong;
  tinyim::AccessService_Stub stub(parg->channel);

  stub.HeartBeat(&cntl, &ping, &pong, nullptr);
  if (cntl.Failed()){
    std::cout << "Fail to call heartbeat. " << cntl.ErrorText() << std::endl;
  }
  else{
    auto last_msg_id = pong.last_msg_id();
    if (last_msg_id > parg->cur_user_id){
      // TODO pull msg
    }
  }
  const int64_t cur_time_us = butil::gettimeofday_us();
  static const int64_t heartbeat_timeout_us = FLAGS_internal_send_heartbeat_s * 1000000;
  bthread_timer_add(&(parg->heartbeat_timeout_id),
                    butil::microseconds_to_timespec(cur_time_us + heartbeat_timeout_us),
                    tinyim::HeartBeatTimeOutHeadler,
                    arg);
}

// class Client{



 // private:
  // bthread_timer_t heartbeat_timeout_id_;

// }

}  // namespace tinyim

int main(int argc, char* argv[]) {
  tinyim::Initialize init(argc, &argv);
  rl_getc_function = tinyim::cli_getc;

  // char *line;
  // char *prgname = argv[0];

  brpc::Channel channel;
  brpc::ChannelOptions options;
  options.protocol = brpc::PROTOCOL_BAIDU_STD;
  options.connection_type = FLAGS_connection_type;
  options.timeout_ms = FLAGS_timeout_ms/*milliseconds*/;
  options.max_retry = FLAGS_max_retry;
  if (channel.Init(FLAGS_server.c_str(), NULL) != 0) {
    std::cout << "Fail to initialize channel" << std::endl;
    return -1;
  }


  const tinyim::user_id_t user_id = FLAGS_user_id;
  {
    tinyim::SigninData signin_data;
    signin_data.set_user_id(user_id);
    signin_data.set_password(FLAGS_password);
    tinyim::Pong pong;

    std::cout << "Calling Signin" << std::endl;
    brpc::Controller cntl;
    tinyim::AccessService_Stub stub(&channel);
    stub.SignIn(&cntl, &signin_data, &pong, nullptr);
    if (cntl.Failed()){
      std::cout << "Fail to call Signin. " << cntl.ErrorText() << std::endl;
      return 0;
    }
  }
  {
    tinyim::UserId userid;
    userid.set_user_id(user_id);
    tinyim::UserInfos user_infos;

    std::cout << "Calling GetFriends" << std::endl;
    brpc::Controller cntl;
    tinyim::AccessService_Stub stub(&channel);
    stub.GetFriends(&cntl, &userid, &user_infos, nullptr);
    if (cntl.Failed()){
      std::cout << "Fail to call GetFriends. " << cntl.ErrorText() << std::endl;
      return 0;
    }
    std::cout << "  friends:" << std::endl;
    for (int i = 0; i < user_infos.user_info_size(); ++i){
      std::cout << "    user_id=" << user_infos.user_info(i).user_id()
                << " user_name=" << user_infos.user_info(i).name() << std::endl;

    }
  }

  tinyim::GroupInfos group_infos;
  {
    tinyim::UserId userid;
    userid.set_user_id(user_id);

    std::cout << "Calling GetGroups" << std::endl;
    brpc::Controller cntl;
    tinyim::AccessService_Stub stub(&channel);
    stub.GetGroups(&cntl, &userid, &group_infos, nullptr);
    if (cntl.Failed()){
      std::cout << "Fail to call GetGroups. " << cntl.ErrorText() << std::endl;
      return 0;
    }
    std::cout << "  groups:" << cntl.ErrorText() << std::endl;
    for (int i = 0; i < group_infos.group_info_size(); ++i){
      std::cout << "    group_id=" << group_infos.group_info(i).group_id()
                << " group_name=" << group_infos.group_info(i).name() << std::endl;

    }
  }
  {

    for (int i = 0; i < group_infos.group_info_size(); ++i){
      tinyim::GroupId group_id;
      group_id.set_group_id(group_infos.group_info(i).group_id());
      tinyim::UserInfos user_infos;

      std::cout << "Calling GetGroups" << std::endl;
      brpc::Controller cntl;
      tinyim::AccessService_Stub stub(&channel);
      stub.GetGroupMembers(&cntl, &group_id, &user_infos, nullptr);
      if (cntl.Failed()){
        std::cout << "Fail to call GetGroups. " << cntl.ErrorText() << std::endl;
        return 0;
      }
      std::cout << "  group_id=" << group_id.group_id() << std::endl;
      for (int i = 0; i < user_infos.user_info_size(); ++i){
        std::cout << "    user_id=" << user_infos.user_info(i).user_id()
                  << " user_name=" << user_infos.user_info(i).name() << std::endl;

      }
    }
  }

  {
    tinyim::MsgIdRange msg_range;
    msg_range.set_user_id(user_id);
    msg_range.set_start_msg_id(0);
    msg_range.set_end_msg_id(100000);
    tinyim::Msgs msgs;

    std::cout << "Calling GetMsgs" << std::endl;
    brpc::Controller cntl;
    tinyim::AccessService_Stub stub(&channel);
    stub.GetMsgs(&cntl, &msg_range, &msgs, nullptr);
    if (cntl.Failed()){
      std::cout << "Fail to call GetMsgs. " << cntl.ErrorText() << std::endl;
      return 0;
    }
    std::cout << "  total msg=" << msgs.msg_size() << std::endl;
    for (int i = 0; i < msgs.msg_size(); ++i){
      if (i == 0 || i == msgs.msg_size() - 1){
        std::cout << "    msg_id=" << msgs.msg(i).msg_id()
                  << " user_id=" << msgs.msg(i).user_id()
                  << " message=" << msgs.msg(i).message()
                  << " msg_time=" << msgs.msg(i).msg_time()
                  << " client_time=" << msgs.msg(i).client_time()
                  << " receiver=" << msgs.msg(i).receiver()
                  << " group_id=" << msgs.msg(i).group_id()
                  << " sender=" << msgs.msg(i).sender() << std::endl;
      }

    }
  }


  // brpc::Controller stream_cntl;
  // brpc::StreamId stream;

  // tinyim::StreamReceiveHandler stream_receive_handler;
  // brpc::StreamOptions stream_options;
  // stream_options.handler = &stream_receive_handler;

  // if (brpc::StreamCreate(&stream, stream_cntl, &stream_options) != 0) {
      // LOG(ERROR) << "Fail to create stream";
      // return -1;
  // }
  // tinyim::AccessService_Stub stub(&channel);

  auto pull_data_args = new tinyim::PullDataArgs{user_id, &channel};
  bthread_t pull_data_bt;
  bthread_start_background(&pull_data_bt, nullptr, tinyim::PullData, pull_data_args);
  // bthread_t heart_beat_bt;
  // bthread_start_background(&heart_beat_bt), nullptr, tinyim::SendHeartBeat, &channel);
  // tinyim::Pong tmp_pong;
  // tinyim::Ping ping;
  // ping.set_user_id(user_id);
  // stub.CreateStream(&stream_cntl, &ping, &tmp_pong, NULL);
  // if (stream_cntl.Failed()) {
      // LOG(ERROR) << "Fail to CreateStream, " << stream_cntl.ErrorText();
      // return -1;
  // }
  tinyim::msg_id_t cur_msg_id = 0;

  tinyim::HeartBeatArg heartbeatarg = {&channel, user_id, cur_msg_id, 0};
  static const int64_t heartbeat_timeout_us = FLAGS_internal_send_heartbeat_s * 1000000;
  bthread_timer_add(&heartbeatarg.heartbeat_timeout_id,
                    butil::microseconds_to_timespec(heartbeat_timeout_us),
                    tinyim::HeartBeatTimeOutHeadler,
                    &heartbeatarg);

  // LOG(INFO) << "Created Stream=" << stream;
  // const tinyim::MsgId tmp_msg_id = 12345;

  size_t str_len = strlen("sendmsgto ");
  int last_send_time = 0;


  while (!brpc::IsAskedToQuit()) {
    char prompt[64];
    snprintf(prompt, sizeof(prompt), "user_id(%ld) access(%s)> ", user_id, FLAGS_server.c_str());
    std::unique_ptr<char, tinyim::Freer> command(::readline(prompt));
    if (command == NULL || *command == '\0') {
      if (tinyim::g_canceled) {
        // No input after the prompt and user pressed Ctrl-C,
        // quit the CLI.
        return 0;
      }
      // User entered an empty command by just pressing Enter.
      continue;
    }
    if (tinyim::g_canceled) {
      // User entered sth. and pressed Ctrl-C, start a new prompt.
      tinyim::g_canceled = false;
      continue;
    }
    // Add user's command to history so that it's browse-able by
    // UP-key and search-able by Ctrl-R.
    ::add_history(command.get());

    if (!strcmp(command.get(), "help")) {
      printf("This is a redis CLI written in brpc.\n");
      continue;
    }
    if (!strcmp(command.get(), "quit")) {
      // Although quit is a valid redis command, it does not make
      // too much sense to run it in this CLI, just quit.
      return 0;
    }

    const std::string lne(command.get());

    if (lne[0] != '\0' && lne[0] != '/' && lne.size() > str_len
        && !strncmp(lne.c_str(), "sendmsgto ", str_len)) {

      const tinyim::user_id_t peer_id = strtoll(command.get() + str_len, nullptr, 10);
      assert(peer_id != user_id);

      auto msg_type = tinyim::MsgType::PRIVATE;
      for (int i = 0; i < group_infos.group_info_size(); ++i){
        if (peer_id == group_infos.group_info(i).group_id()){
          msg_type = tinyim::MsgType::GROUP;
          break;
        }
      }


      auto offset = lne.find_last_of(' ');
      const std::string msg(lne.begin() + offset + 1, lne.end());

      tinyim::NewMsg new_msg;
      new_msg.set_user_id(user_id);
      new_msg.set_peer_id(peer_id);
      new_msg.set_message(msg);
      new_msg.set_msg_type(msg_type);
      // TODO time must be monotonically increasing
      int msg_time = std::time(nullptr);
      if (msg_time == last_send_time){
        ++msg_time;
      }
      last_send_time = msg_time;

      new_msg.set_client_time(msg_time);
      std::cout << "userid=" << user_id
                << " peer_id=" << peer_id
                << " msg_type=" << msg_type
                << " msg=" << msg
                << " timestamp=" << new_msg.client_time() << std::endl;

      brpc::Controller cntl;
      tinyim::AccessService_Stub stub(&channel);

      bthread_timer_del(heartbeatarg.heartbeat_timeout_id);

      std::cout << "Calling SendMsg" << std::endl;

      while (!brpc::IsAskedToQuit()) {
        tinyim::MsgReply msg_reply;
        stub.SendMsg(&cntl, &new_msg, &msg_reply, nullptr);
        if (cntl.Failed()){
          std::cout << "Fail to SendMsg, " << cntl.ErrorText() << std::endl;
          usleep(1000000L);
        }
        else{
          std::cout << "Received msgreply."
                    << " msgid=" << msg_reply.msg_id()
                    << " last_msg_id=" << msg_reply.last_msg_id()
                    << " msg_time=" << msg_reply.msg_time() << std::endl;
          cur_msg_id = msg_reply.msg_id();
          heartbeatarg.cur_user_id = cur_msg_id;
          if (msg_reply.last_msg_id() > cur_msg_id){
            // TODO need pull msg
          }
          const int64_t cur_time_us = butil::gettimeofday_us();
          bthread_timer_add(&heartbeatarg.heartbeat_timeout_id,
                            butil::microseconds_to_timespec(cur_time_us + heartbeat_timeout_us),
                            tinyim::HeartBeatTimeOutHeadler,
                            &heartbeatarg);
          break;
        }
      }
    }
  }
  std::cout << "client is going to quit" << std::endl;
  return 0;
}
