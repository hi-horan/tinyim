
gflag -log_error_text


幂等

redis 事务 判断是否最大， 最大则赋值 需要原子操作

redis mysql 一致性

分布式锁

# 项目介绍

支持用户间单聊，群聊, 用brpc通讯，

## client

客户端与服务端保持长连接，且客户端空闲时会发送心跳，保证客户端能及时收到消息。
客户端发送信息时，每条信息携带时间字段，可以把它看作版本号，每个用户所有消息的时间单调递增，用来实现消息发送的幂等性。只有一条消息被服务端确认并分配msg_id后客户端才能发送下一条消息，这样可以保证消息不丢失，最终可以保证消息的不重不漏及有序。

客户端可以主动拉取消息(此时消息一般为数据库中存储的消息)，服务端也可主动推送（此时消息一般为发送者正发送过来的消息)，保证消息即时性.

## server


## idgen

idgen负责为每个user分配msg_id，内部用leveldb存储。
leveldb每次批量申请id，如每次申请10000个id，减少访问磁盘次数，这意味着idgen意外重启后id分配会不连续，但能保证单调递增，不保证连续递增。
当客户端发现msg_id不连续时，可以尝试拉取缺失的msg_id区间,即可发现这个区间是否有消息。由于绝大多数时间idgen会正常工作，所以msg_id几乎是连续的。

## access

access负责客户端的连接，然后发送到logic, 客户端连接哪个access保存在redis

## logic

接收access转发过来的msg，然后向dbproxy查询是否已经处理过该消息，
1.是新消息, 则为发送者和接收者分配msg_id,并发送数据到dbproxy，dbproxy返回成功则向发送者响应该条消息的msg_id，并且发送给接收者，返回其他时发送者会重传，直到成功。
2.是重传的消息，且重传前的消息已经处理成功，则向发送者返回msg_id.

## dbproxy

### 数据库-MySQL
消息采用写扩散的形式存储(即对于单聊来说，发送者和接收者各存一份，对于群聊来说，群里每个人都存一份消息。可以极大简化后续读取时的逻辑。),但此时要限制群聊的最大人数，或者超过一定人数的群改用读扩散(即一个群的消息只存一份)。

为了消息不重复，用user_id,sender_id, send_time三个字段建立唯一索引, 对于每个user_id来说, send_id,send_time是唯一的。也可采用其他方式保证唯一。

数据库采用分库分表的方式增加写入查询性能, 根据user_id通过consistent hashing选出要存储的数据库, 方便以后扩容缩容。


TODO 当前为了消息不丢失,将所有消息保存到数据库后才向上游返回成功，数据库会成为瓶颈，后续可以使用消息队列异步存储到数据库。


### 缓存-Redis，

要存储服务端成功处理的发送者最新的一条消息时间, 后续只需处理比当前更新的消息，当与MySQL不一致时，说明落后于MySQL, 此时logic会分配msg_id,并发送数据到dbproxy,dbproxy存数据时会发现数据已存在，则插入失败，即使这条重复消息发送到了接收者，接收者会根据sender，及send_time去重,所以缓存可以接受最终一致性

user_last_send save in redis like 'user_idu:[msg_id,client_time,msg_time]'
access address save in redis like 'user_ida:192.168.0.2:8000'

### run before
export LD_PRELOAD=/lib/libasan.so