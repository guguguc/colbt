#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "im/core/protocol.h"
#include "im/core/types.h"

namespace im {

// 二进制编解码工具
class Writer {
public:
    void u8(uint8_t v);
    void u16(uint16_t v);
    void u32(uint32_t v);
    void i64(int64_t v);
    void str(const std::string& s); // u16长度 + bytes
    void bytes(const std::vector<uint8_t>& b);
    const std::vector<uint8_t>& data() const { return buf_; }

private:
    std::vector<uint8_t> buf_;
};

class Reader {
public:
    explicit Reader(const std::vector<uint8_t>& data) : data_(data) {}

    uint8_t u8();
    uint16_t u16();
    uint32_t u32();
    int64_t i64();
    std::string str();
    std::vector<uint8_t> bytes();

    bool atEnd() const { return pos_ == data_.size(); }

private:
    const std::vector<uint8_t>& data_;
    size_t pos_ = 0;

    const uint8_t* take(size_t n);
};

// 组装/解析完整帧
std::vector<uint8_t> encodePacket(const Packet& pkt);
bool decodePacket(const uint8_t* data, size_t len, Packet& out);

// 命令正文编解码
void encodeLoginReq(Writer& w, const std::string& username, const std::string& password);
void encodeRegisterReq(Writer& w, const std::string& username, const std::string& password,
                       const std::string& nickname);
void decodeLoginResp(const std::vector<uint8_t>& body, int& code, std::string& msg, UserInfo& me);
void decodeRegisterResp(const std::vector<uint8_t>& body, int& code, std::string& msg);

void encodeSimpleReq(Writer& w); // 无参数请求
void encodeLogoutReq(Writer& w);

void decodeContactsResp(const std::vector<uint8_t>& body, std::vector<BuddyInfo>& buddies,
                        std::vector<GroupInfo>& groups);
void decodeSessionsResp(const std::vector<uint8_t>& body, std::vector<SessionInfo>& sessions);
void encodeAddFriendReq(Writer& w, const std::string& username, const std::string& remark);
void decodeAddFriendResp(const std::vector<uint8_t>& body, int& code, std::string& msg,
                         BuddyInfo& buddy);
void decodeFriendAddedPush(const std::vector<uint8_t>& body, BuddyInfo& buddy);

void encodeCreateGroupReq(Writer& w, const std::string& name, const std::vector<int64_t>& ids);
void decodeCreateGroupResp(const std::vector<uint8_t>& body, int& code, std::string& msg,
                           GroupInfo& group);
void encodeGroupMembersReq(Writer& w, int64_t groupId);
void decodeGroupMembersResp(const std::vector<uint8_t>& body, int64_t& groupId,
                            std::vector<MemberInfo>& members);

void encodeHistoryReq(Writer& w, int64_t targetId, int targetType, int limit);
void decodeHistoryResp(const std::vector<uint8_t>& body, int64_t& targetId, int& targetType,
                       std::vector<MessageInfo>& msgs);

void encodeSendMsgReq(Writer& w, int64_t targetId, int targetType, int msgType,
                      const std::string& content, int64_t replyToId = 0);
// 服务端回执完整消息
void decodeSendMsgResp(const std::vector<uint8_t>& body, MessageInfo& msg);
void decodeMsgPush(const std::vector<uint8_t>& body, MessageInfo& msg);
void decodePresencePush(const std::vector<uint8_t>& body, int64_t& userId, bool& online);

// 撤回：msgId + targetId + targetType
void encodeRecallMsgReq(Writer& w, int64_t msgId, int64_t targetId, int targetType);
void decodeRecallMsgReq(const std::vector<uint8_t>& body, int64_t& msgId, int64_t& targetId,
                        int& targetType);
void decodeRecallMsgPush(const std::vector<uint8_t>& body, int64_t& msgId, int64_t& targetId,
                         int& targetType);

// 好友/群管理
void encodeDeleteFriendReq(Writer& w, int64_t friendId);
void decodeDeleteFriendReq(const std::vector<uint8_t>& body, int64_t& friendId);
void decodeDeleteFriendPush(const std::vector<uint8_t>& body, int64_t& friendId,
                            std::string& name);
void encodeGroupIdReq(Writer& w, int64_t groupId); // 退群/解散
void decodeGroupIdReq(const std::vector<uint8_t>& body, int64_t& groupId);
void encodeKickMemberReq(Writer& w, int64_t groupId, int64_t memberId);
void decodeKickMemberReq(const std::vector<uint8_t>& body, int64_t& groupId, int64_t& memberId);
void encodeRenameGroupReq(Writer& w, int64_t groupId, const std::string& name);
void decodeRenameGroupReq(const std::vector<uint8_t>& body, int64_t& groupId, std::string& name);
void decodeGroupUpdatedPush(const std::vector<uint8_t>& body, int64_t& groupId);

// 搜索
void encodeSearchReq(Writer& w, const std::string& keyword, int limit);
void decodeSearchReq(const std::vector<uint8_t>& body, std::string& keyword, int& limit);
void decodeSearchResp(const std::vector<uint8_t>& body, std::vector<MessageInfo>& msgs);

// 输入中
void encodeTypingReq(Writer& w, int64_t targetId, int targetType);
void decodeTypingReq(const std::vector<uint8_t>& body, int64_t& targetId, int& targetType);
void decodeTypingPush(const std::vector<uint8_t>& body, int64_t& fromId, int64_t& targetId,
                      int& targetType);

// 已读上报：peerId + targetType
void encodeMarkReadReq(Writer& w, int64_t peerId, int targetType);
void decodeMarkReadReq(const std::vector<uint8_t>& body, int64_t& peerId, int& targetType);
void decodeReadPush(const std::vector<uint8_t>& body, int64_t& peerId, int& targetType);

// 资料修改：nickname/avatar/newPassword 为空表示不修改；改密码需旧密码
void encodeUpdateProfileReq(Writer& w, const std::string& nickname, const std::string& avatar,
                            const std::string& oldPassword, const std::string& newPassword);
void decodeUpdateProfileReq(const std::vector<uint8_t>& body, std::string& nickname,
                            std::string& avatar, std::string& oldPassword,
                            std::string& newPassword);
void encodeUpdateProfileResp(Writer& w, int code, const std::string& msg, const UserInfo& me);
void decodeUpdateProfileResp(const std::vector<uint8_t>& body, int& code, std::string& msg,
                             UserInfo& me);
void encodeProfileUpdatedPush(Writer& w, int64_t userId, const std::string& nickname,
                              const std::string& avatar);
void decodeProfileUpdatedPush(const std::vector<uint8_t>& body, int64_t& userId,
                              std::string& nickname, std::string& avatar);

// 文件上传/下载
void encodeUploadFileReq(Writer& w, const std::string& name, int64_t size,
                         const std::string& mime, const std::vector<uint8_t>& data);
void decodeUploadFileReq(const std::vector<uint8_t>& body, std::string& name, int64_t& size,
                         std::string& mime, std::vector<uint8_t>& data);
void encodeUploadFileResp(Writer& w, int code, const std::string& fileId);
void decodeUploadFileResp(const std::vector<uint8_t>& body, int& code, std::string& fileId);
void encodeDownloadFileReq(Writer& w, const std::string& fileId);
void decodeDownloadFileReq(const std::vector<uint8_t>& body, std::string& fileId);
void encodeDownloadFileResp(Writer& w, int code, const std::string& fileId,
                            const std::string& name, int64_t size, const std::string& mime,
                            const std::vector<uint8_t>& data);
void decodeDownloadFileResp(const std::vector<uint8_t>& body, int& code, std::string& fileId,
                            std::string& name, int64_t& size, std::string& mime,
                            std::vector<uint8_t>& data);

// 通用：code(0成功) + msg
void decodeAck(const std::vector<uint8_t>& body, int& code, std::string& msg);
// 序列化消息体（写入时共用）
void writeMessage(Writer& w, const MessageInfo& msg);
MessageInfo readMessage(Reader& r);

void writeUser(Writer& w, const UserInfo& u);
UserInfo readUser(Reader& r);

} // namespace im
