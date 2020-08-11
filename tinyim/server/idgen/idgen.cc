#include "idgen/idgen.h"

#include <mutex>
#include <string>
#include <unordered_map>

#include <butil/synchronization/lock.h>
#include <leveldb/db.h>
#include <gflags/gflags.h>
#include <glog/logging.h>

DEFINE_string(leveldb_file, "./data/data.db", "Leveldb db file");
DEFINE_int64(each_gen_id_num, 1024, "Each generate id num from db");
DEFINE_int64(hash_bucket_num, 12, "Each generate id num from db");

namespace tinyim{

class LevelDbIdGen final : public IdGen{
 public:
  LevelDbIdGen() = default;

  void Init(){
    leveldb::Options options;
    options.create_if_missing = true;

    auto status = leveldb::DB::Open(options, FLAGS_leveldb_file, &db_);
    if (!status.ok()) {
      LOG(ERROR) << "Fail to open leveldb file=" << FLAGS_leveldb_file
                 << ". " << status.ToString();
      exit(0);
    }
  }

  virtual leveldb::Status IdGenerate(int64_t user_id, int64_t need_msgid_num,
                                     int64_t& start_id) override {
    const int bucket = user_id % kBucketNum;
    auto& id_cache = id_cache_[bucket];
    leveldb::Status status;

    std::unique_lock<butil::Mutex> ul(mutex_[bucket]);
    int64_t& original_id = id_cache[user_id];
    if (original_id == 0){
      std::string db_id;
      auto status = db_->Get(leveldb::ReadOptions(), leveldb::Slice(std::to_string(user_id)), &db_id);
      if (status.ok()){
        original_id = std::stoll(db_id);
      }
      else if (status.IsNotFound()){
        original_id = 0;
      }
      else{
        LOG(ERROR) << "Fail to get id=" << user_id << " from leveldb" << ". " << status.ToString();
        return status;
      }
    }

                    // TODO 是否是移位?
    if (original_id % FLAGS_each_gen_id_num + need_msgid_num > FLAGS_each_gen_id_num){
      int64_t db_id = (original_id + need_msgid_num + FLAGS_each_gen_id_num - 1) & (FLAGS_each_gen_id_num - 1);
      auto write_options = leveldb::WriteOptions();
      // XXX 同步写
      write_options.sync = true;
      status = db_->Put(write_options, leveldb::Slice(std::to_string(user_id)),
                                            leveldb::Slice(std::to_string(db_id)));
      if (!status.ok()){
        LOG(ERROR) << "Fail to generate db_id=" << db_id << " for user_id=" << user_id
                  << ". " << status.ToString();
        return status;
      }
    }
    start_id = original_id + 1;
    original_id += need_msgid_num;
    return status;
  }

  virtual ~LevelDbIdGen(){
    delete db_;
  }

 private:
  leveldb::DB* db_;

  enum { kBucketNum = 16 };
  butil::Mutex mutex_[kBucketNum];
  std::unordered_map<int64_t, int64_t> id_cache_[kBucketNum];
  //                 user_id  msg_id
};

IdGen* IdGen::Default(){
  LevelDbIdGen* id_gen =  new LevelDbIdGen;
  id_gen->Init();
  return id_gen;
}

IdGen::~IdGen() {}

}  // namespace tinyim