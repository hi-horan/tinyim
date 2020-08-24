#ifndef TINYIM_UTIL_CONSISTENT_HASH_H_
#define TINYIM_UTIL_CONSISTENT_HASH_H_

// not use

// reference https://github.com/ioriiod0/consistent_hash

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <ostream>
#include <utility>

#include <butil/crc32c.h>  // brpc

namespace tinyim {

struct vnode_t {
  vnode_t():node_id(0),vnode_id(0) {}
  vnode_t(int n, int v):node_id(n),vnode_id(v) {}

  int node_id;
  int vnode_id;
};

}  // namespace tinyim

namespace std {
template<>
struct hash<tinyim::vnode_t> {
uint32_t operator()(tinyim::vnode_t val) const {
  return butil::crc32c::Value(reinterpret_cast<const char*>(&val), sizeof(val));
}
};

//// not good
// template<>
// struct hash<tinyim::vnode_t> {
// template <typename T>
// inline void hash_combine(uint32_t& seed, T v) const {
  // std::hash<T> hasher;
  // seed ^= hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
// }
// uint32_t operator()(tinyim::vnode_t val) const {
  // uint32_t seed = 0;
  // hash_combine(seed, val.node_id);
  // hash_combine(seed, val.vnode_id);
  // return seed;
// }
// };

}  // namespace std


namespace tinyim {

class ConsistentHash {
 public:
  using iterator = std::map<uint32_t, vnode_t>::iterator;
  using reverse_iterator = std::map<uint32_t, vnode_t>::reverse_iterator;

  ConsistentHash() = default;
  ~ConsistentHash() = default;

  std::size_t size() const {
      return nodes_.size();
  }

  bool empty() const {
    return nodes_.empty();
  }

  std::pair<iterator,bool> insert(vnode_t node) {
    // XXX maybe conflict
    return nodes_.emplace(std::hash<vnode_t>{}(node),node);
  }

  void erase(iterator it) {
    nodes_.erase(it);
  }

  std::size_t erase(vnode_t node) {
    return nodes_.erase(std::hash<vnode_t>{}(node));
  }

  int find(uint32_t hash) {
    assert(nodes_.empty());
    iterator iter = nodes_.lower_bound(hash);
    if (iter == nodes_.end()) {
        iter = nodes_.begin();
    }
    return iter->second.node_id;
  }

  iterator begin() { return nodes_.begin(); }
  iterator end() { return nodes_.end(); }
  reverse_iterator rbegin() { return nodes_.rbegin(); }
  reverse_iterator rend() { return nodes_.rend(); }

  void Describe(std::ostream& os) const {
    auto i = nodes_.begin();
    auto j = nodes_.rbegin();

    int max_node_id = -1;
    for (auto iter = nodes_.begin(); iter != nodes_.end(); ++iter){
      max_node_id = std::max(max_node_id, iter->second.node_id);
    }

    if (max_node_id == -1){
      os << "null" << std::endl;
      return;
    }
    std::unique_ptr<int64_t> ptr(new int64_t[max_node_id + 1]{});
    int64_t* sums = ptr.get();

    std::size_t n = UINT32_MAX - j->first + i->first;

    sums[i->second.node_id] += n;

    uint32_t priv = i->first;
    uint32_t cur;
    auto end = nodes_.end();
    while(++i != end) {
        cur = i->first;
        n = cur - priv;
        sums[i->second.node_id] += n;
        priv = cur;
    }

    for(int i = 0; i <= max_node_id; ++i) {
      os << "node=" << i << " contains=" << sums[i] << " ";
    }
    os << std::endl;
  }

 private:
  std::map<uint32_t, vnode_t> nodes_;
};

std::ostream& operator<<(std::ostream& os, const ConsistentHash& consistent_hash){
  consistent_hash.Describe(os);
  return os;
}

}  // namespace tinyim

#endif  // TINYIM_UTIL_CONSISTENT_HASH_H_
