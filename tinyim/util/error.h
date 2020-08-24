#ifndef TINYIM_UTIL_ERROR_H_
#define TINYIM_UTIL_ERROR_H_

namespace tinyim{

enum Errno: int {
  ECONFLICT = 10000,

  EHAVENOTHIS = 10001
};

}  // namespace tinyim




#endif  // TINYIM_UTIL_ERROR_H_