#ifndef TINYIM_IDGEN_IDGEN_H_
#define TINYIM_IDGEN_IDGEN_H_

#include <leveldb/status.h>


namespace tinyim {

class IdGen {
 public:
  IdGen() = default;

  IdGen(const IdGen&) = delete;
  IdGen& operator=(const IdGen&) = delete;
  // TODO user_id批量申请

  virtual leveldb::Status IdGenerate(int64_t user_id, int64_t need_msgid_num,
                                     int64_t& start_msgid) = 0;
  static IdGen* Default();

  virtual ~IdGen();
};

}  // namespace tinyim



#endif  // TINYIM_IDGEN_IDGEN_H_