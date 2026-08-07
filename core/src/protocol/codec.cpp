#include "protocol/codec.h"

#include <cstring>

namespace im {

namespace {
constexpr uint8_t F_U8 = 1;
constexpr uint8_t F_U16 = 2;
constexpr uint8_t F_U32 = 3;
constexpr uint8_t F_I64 = 4;
constexpr uint8_t F_STR = 5;
} // namespace

void Writer::u8(uint8_t v) { buf_.push_back(v); }
void Writer::u16(uint16_t v) {
    buf_.push_back(static_cast<uint8_t>(v & 0xFF));
    buf_.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}
void Writer::u32(uint32_t v) {
    for (int i = 0; i < 4; ++i) buf_.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFF));
}
void Writer::i64(int64_t v) {
    uint64_t u = static_cast<uint64_t>(v);
    for (int i = 0; i < 8; ++i) buf_.push_back(static_cast<uint8_t>((u >> (i * 8)) & 0xFF));
}
void Writer::str(const std::string& s) {
    if (s.size() > 0xFFFF) throw std::runtime_error("string too long");
    u16(static_cast<uint16_t>(s.size()));
    buf_.insert(buf_.end(), s.begin(), s.end());
}
void Writer::bytes(const std::vector<uint8_t>& b) {
    u32(static_cast<uint32_t>(b.size()));
    buf_.insert(buf_.end(), b.begin(), b.end());
}

const uint8_t* Reader::take(size_t n) {
    if (pos_ + n > data_.size()) throw std::runtime_error("packet too short");
    const uint8_t* p = data_.data() + pos_;
    pos_ += n;
    return p;
}

uint8_t Reader::u8() { return *take(1); }
uint16_t Reader::u16() {
    const uint8_t* p = take(2);
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}
uint32_t Reader::u32() {
    const uint8_t* p = take(4);
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
int64_t Reader::i64() {
    const uint8_t* p = take(8);
    uint64_t u = 0;
    for (int i = 0; i < 8; ++i) u |= static_cast<uint64_t>(p[i]) << (i * 8);
    return static_cast<int64_t>(u);
}
std::string Reader::str() {
    uint16_t n = u16();
    const uint8_t* p = take(n);
    return std::string(reinterpret_cast<const char*>(p), n);
}
std::vector<uint8_t> Reader::bytes() {
    uint32_t n = u32();
    const uint8_t* p = take(n);
    return std::vector<uint8_t>(p, p + n);
}

std::vector<uint8_t> encodePacket(const Packet& pkt) {
    std::vector<uint8_t> out;
    out.reserve(12 + pkt.body.size());
    out.push_back(static_cast<uint8_t>(kMagic & 0xFF));
    out.push_back(static_cast<uint8_t>((kMagic >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((kMagic >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((kMagic >> 24) & 0xFF));
    out.push_back(kVersion);
    out.push_back(static_cast<uint8_t>(pkt.cmd & 0xFF));
    out.push_back(static_cast<uint8_t>((pkt.cmd >> 8) & 0xFF));
    out.push_back(0); // reserved
    uint32_t blen = static_cast<uint32_t>(pkt.body.size());
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>((blen >> (i * 8)) & 0xFF));
    out.insert(out.end(), pkt.body.begin(), pkt.body.end());
    return out;
}

bool decodePacket(const uint8_t* data, size_t len, Packet& out) {
    if (len < 12) return false;
    uint32_t magic = 0;
    for (int i = 0; i < 4; ++i) magic |= static_cast<uint32_t>(data[i]) << (i * 8);
    if (magic != kMagic) return false;
    if (data[4] != kVersion) return false;
    out.cmd = static_cast<uint16_t>(data[5]) | (static_cast<uint16_t>(data[6]) << 8);
    uint32_t blen = 0;
    for (int i = 0; i < 4; ++i) blen |= static_cast<uint32_t>(data[8 + i]) << (i * 8);
    if (blen > kMaxBodyLen) return false;
    if (len < 12 + blen) return false;
    out.body.assign(data + 12, data + 12 + blen);
    return true;
}

void encodeLoginReq(Writer& w, const std::string& username, const std::string& password) {
    w.str(username);
    w.str(password);
}

void encodeRegisterReq(Writer& w, const std::string& username, const std::string& password,
                       const std::string& nickname) {
    w.str(username);
    w.str(password);
    w.str(nickname);
}

void decodeLoginResp(const std::vector<uint8_t>& body, int& code, std::string& msg,
                     UserInfo& me) {
    Reader r(body);
    code = r.u8();
    msg = r.str();
    if (code != 0) return; // 失败响应仅包含 code + msg
    me.id = r.i64();
    me.username = r.str();
    me.nickname = r.str();
    me.avatar = r.str();
    me.online = r.u8();
}

void decodeRegisterResp(const std::vector<uint8_t>& body, int& code, std::string& msg) {
    Reader r(body);
    code = r.u8();
    msg = r.str();
}

void encodeSimpleReq(Writer& w) { (void)w; }

void encodeLogoutReq(Writer& w) { (void)w; }

void writeMessage(Writer& w, const MessageInfo& m) {
    w.i64(m.id);
    w.i64(m.fromId);
    w.i64(m.targetId);
    w.u8(static_cast<uint8_t>(m.targetType));
    w.u8(static_cast<uint8_t>(m.msgType));
    w.str(m.content);
    w.i64(m.timestamp);
    w.u8(static_cast<uint8_t>(m.direction));
    w.str(m.senderName);
    w.u8(static_cast<uint8_t>(m.read));
    w.i64(m.replyToId);
    w.str(m.replyContent);
}

MessageInfo readMessage(Reader& r) {
    MessageInfo m;
    m.id = r.i64();
    m.fromId = r.i64();
    m.targetId = r.i64();
    m.targetType = r.u8();
    m.msgType = r.u8();
    m.content = r.str();
    m.timestamp = r.i64();
    m.direction = r.u8();
    m.senderName = r.str();
    m.read = r.u8();
    m.replyToId = r.i64();
    m.replyContent = r.str();
    return m;
}

void writeUser(Writer& w, const UserInfo& u) {
    w.i64(u.id);
    w.str(u.username);
    w.str(u.nickname);
    w.str(u.avatar);
    w.u8(static_cast<uint8_t>(u.online));
}

UserInfo readUser(Reader& r) {
    UserInfo u;
    u.id = r.i64();
    u.username = r.str();
    u.nickname = r.str();
    u.avatar = r.str();
    u.online = r.u8();
    return u;
}

void decodeContactsResp(const std::vector<uint8_t>& body, std::vector<BuddyInfo>& buddies,
                        std::vector<GroupInfo>& groups) {
    Reader r(body);
    uint16_t nb = r.u16();
    buddies.reserve(nb);
    for (uint16_t i = 0; i < nb; ++i) {
        BuddyInfo b;
        b.user = readUser(r);
        b.remark = r.str();
        buddies.push_back(b);
    }
    uint16_t ng = r.u16();
    groups.reserve(ng);
    for (uint16_t i = 0; i < ng; ++i) {
        GroupInfo g;
        g.id = r.i64();
        g.name = r.str();
        g.ownerId = r.i64();
        uint16_t nm = r.u16();
        for (uint16_t j = 0; j < nm; ++j) {
            MemberInfo m;
            m.user = readUser(r);
            m.groupNick = r.str();
            g.members.push_back(m);
        }
        groups.push_back(g);
    }
}

void decodeSessionsResp(const std::vector<uint8_t>& body, std::vector<SessionInfo>& sessions) {
    Reader r(body);
    uint16_t n = r.u16();
    sessions.reserve(n);
    for (uint16_t i = 0; i < n; ++i) {
        SessionInfo s;
        s.targetId = r.i64();
        s.targetType = r.u8();
        s.title = r.str();
        s.avatar = r.str();
        s.lastContent = r.str();
        s.lastTime = r.i64();
        s.unread = r.u32();
        sessions.push_back(s);
    }
}

void encodeAddFriendReq(Writer& w, const std::string& username, const std::string& remark) {
    w.str(username);
    w.str(remark);
}

void decodeAddFriendResp(const std::vector<uint8_t>& body, int& code, std::string& msg,
                         BuddyInfo& buddy) {
    Reader r(body);
    code = r.u8();
    msg = r.str();
    if (code == 0) {
        buddy.user = readUser(r);
        buddy.remark = r.str();
    }
}

void decodeFriendAddedPush(const std::vector<uint8_t>& body, BuddyInfo& buddy) {
    Reader r(body);
    buddy.user = readUser(r);
    buddy.remark = r.str();
}

void encodeCreateGroupReq(Writer& w, const std::string& name, const std::vector<int64_t>& ids) {
    w.str(name);
    w.u16(static_cast<uint16_t>(ids.size()));
    for (int64_t id : ids) w.i64(id);
}

void decodeCreateGroupResp(const std::vector<uint8_t>& body, int& code, std::string& msg,
                           GroupInfo& group) {
    Reader r(body);
    code = r.u8();
    msg = r.str();
    if (code == 0) {
        group.id = r.i64();
        group.name = r.str();
        group.ownerId = r.i64();
        uint16_t nm = r.u16();
        for (uint16_t j = 0; j < nm; ++j) {
            MemberInfo m;
            m.user = readUser(r);
            m.groupNick = r.str();
            group.members.push_back(m);
        }
    }
}

void encodeGroupMembersReq(Writer& w, int64_t groupId) { w.i64(groupId); }

void decodeGroupMembersResp(const std::vector<uint8_t>& body, int64_t& groupId,
                            std::vector<MemberInfo>& members) {
    Reader r(body);
    groupId = r.i64();
    uint16_t n = r.u16();
    members.reserve(n);
    for (uint16_t i = 0; i < n; ++i) {
        MemberInfo m;
        m.user = readUser(r);
        m.groupNick = r.str();
        members.push_back(m);
    }
}

void encodeHistoryReq(Writer& w, int64_t targetId, int targetType, int limit) {
    w.i64(targetId);
    w.u8(static_cast<uint8_t>(targetType));
    w.u16(static_cast<uint16_t>(limit));
}

void decodeHistoryResp(const std::vector<uint8_t>& body, int64_t& targetId, int& targetType,
                       std::vector<MessageInfo>& msgs) {
    Reader r(body);
    targetId = r.i64();
    targetType = r.u8();
    uint16_t n = r.u16();
    msgs.reserve(n);
    for (uint16_t i = 0; i < n; ++i) msgs.push_back(readMessage(r));
}

void encodeSendMsgReq(Writer& w, int64_t targetId, int targetType, int msgType,
                      const std::string& content, int64_t replyToId) {
    w.i64(targetId);
    w.u8(static_cast<uint8_t>(targetType));
    w.u8(static_cast<uint8_t>(msgType));
    w.str(content);
    w.i64(replyToId);
}

void decodeSendMsgResp(const std::vector<uint8_t>& body, MessageInfo& msg) {
    Reader r(body);
    msg = readMessage(r);
}

void decodeMsgPush(const std::vector<uint8_t>& body, MessageInfo& msg) {
    Reader r(body);
    msg = readMessage(r);
}

void decodePresencePush(const std::vector<uint8_t>& body, int64_t& userId, bool& online) {
    Reader r(body);
    userId = r.i64();
    online = r.u8() != 0;
}

void encodeMarkReadReq(Writer& w, int64_t peerId, int targetType) {
    w.i64(peerId);
    w.u8(static_cast<uint8_t>(targetType));
}

void decodeMarkReadReq(const std::vector<uint8_t>& body, int64_t& peerId, int& targetType) {
    Reader r(body);
    peerId = r.i64();
    targetType = r.u8();
}

void decodeReadPush(const std::vector<uint8_t>& body, int64_t& peerId, int& targetType) {
    Reader r(body);
    peerId = r.i64();
    targetType = r.u8();
}

void encodeUploadFileReq(Writer& w, const std::string& name, int64_t size,
                         const std::string& mime, const std::vector<uint8_t>& data) {
    w.str(name);
    w.i64(size);
    w.str(mime);
    w.bytes(data);
}

void decodeUploadFileReq(const std::vector<uint8_t>& body, std::string& name, int64_t& size,
                         std::string& mime, std::vector<uint8_t>& data) {
    Reader r(body);
    name = r.str();
    size = r.i64();
    mime = r.str();
    data = r.bytes();
}

void encodeUploadFileResp(Writer& w, int code, const std::string& fileId) {
    w.u8(static_cast<uint8_t>(code));
    w.str(fileId);
}

void decodeUploadFileResp(const std::vector<uint8_t>& body, int& code, std::string& fileId) {
    Reader r(body);
    code = r.u8();
    fileId = r.str();
}

void encodeDownloadFileReq(Writer& w, const std::string& fileId) { w.str(fileId); }

void decodeDownloadFileReq(const std::vector<uint8_t>& body, std::string& fileId) {
    Reader r(body);
    fileId = r.str();
}

void encodeDownloadFileResp(Writer& w, int code, const std::string& fileId,
                            const std::string& name, int64_t size, const std::string& mime,
                            const std::vector<uint8_t>& data) {
    w.u8(static_cast<uint8_t>(code));
    w.str(fileId);
    w.str(name);
    w.i64(size);
    w.str(mime);
    w.bytes(data);
}

void decodeDownloadFileResp(const std::vector<uint8_t>& body, int& code, std::string& fileId,
                            std::string& name, int64_t& size, std::string& mime,
                            std::vector<uint8_t>& data) {
    Reader r(body);
    code = r.u8();
    fileId = r.str();
    name = r.str();
    size = r.i64();
    mime = r.str();
    data = r.bytes();
}

void encodeRecallMsgReq(Writer& w, int64_t msgId, int64_t targetId, int targetType) {
    w.i64(msgId);
    w.i64(targetId);
    w.u8(static_cast<uint8_t>(targetType));
}

void decodeRecallMsgReq(const std::vector<uint8_t>& body, int64_t& msgId, int64_t& targetId,
                        int& targetType) {
    Reader r(body);
    msgId = r.i64();
    targetId = r.i64();
    targetType = r.u8();
}

void decodeRecallMsgPush(const std::vector<uint8_t>& body, int64_t& msgId, int64_t& targetId,
                         int& targetType) {
    Reader r(body);
    msgId = r.i64();
    targetId = r.i64();
    targetType = r.u8();
}

void encodeDeleteFriendReq(Writer& w, int64_t friendId) { w.i64(friendId); }
void decodeDeleteFriendReq(const std::vector<uint8_t>& body, int64_t& friendId) {
    Reader r(body);
    friendId = r.i64();
}

void decodeDeleteFriendPush(const std::vector<uint8_t>& body, int64_t& friendId,
                            std::string& name) {
    Reader r(body);
    friendId = r.i64();
    name = r.str();
}

void encodeGroupIdReq(Writer& w, int64_t groupId) { w.i64(groupId); }
void decodeGroupIdReq(const std::vector<uint8_t>& body, int64_t& groupId) {
    Reader r(body);
    groupId = r.i64();
}

void encodeKickMemberReq(Writer& w, int64_t groupId, int64_t memberId) {
    w.i64(groupId);
    w.i64(memberId);
}
void decodeKickMemberReq(const std::vector<uint8_t>& body, int64_t& groupId, int64_t& memberId) {
    Reader r(body);
    groupId = r.i64();
    memberId = r.i64();
}

void encodeRenameGroupReq(Writer& w, int64_t groupId, const std::string& name) {
    w.i64(groupId);
    w.str(name);
}
void decodeRenameGroupReq(const std::vector<uint8_t>& body, int64_t& groupId, std::string& name) {
    Reader r(body);
    groupId = r.i64();
    name = r.str();
}

void decodeGroupUpdatedPush(const std::vector<uint8_t>& body, int64_t& groupId) {
    Reader r(body);
    groupId = r.i64();
}

void encodeSearchReq(Writer& w, const std::string& keyword, int limit) {
    w.str(keyword);
    w.u16(static_cast<uint16_t>(limit));
}
void decodeSearchReq(const std::vector<uint8_t>& body, std::string& keyword, int& limit) {
    Reader r(body);
    keyword = r.str();
    limit = r.u16();
}

void decodeSearchResp(const std::vector<uint8_t>& body, std::vector<MessageInfo>& msgs) {
    Reader r(body);
    uint16_t n = r.u16();
    msgs.reserve(n);
    for (uint16_t i = 0; i < n; ++i) msgs.push_back(readMessage(r));
}

void encodeTypingReq(Writer& w, int64_t targetId, int targetType) {
    w.i64(targetId);
    w.u8(static_cast<uint8_t>(targetType));
}
void decodeTypingReq(const std::vector<uint8_t>& body, int64_t& targetId, int& targetType) {
    Reader r(body);
    targetId = r.i64();
    targetType = r.u8();
}
void decodeTypingPush(const std::vector<uint8_t>& body, int64_t& fromId, int64_t& targetId,
                      int& targetType) {
    Reader r(body);
    fromId = r.i64();
    targetId = r.i64();
    targetType = r.u8();
}

void decodeAck(const std::vector<uint8_t>& body, int& code, std::string& msg) {
    Reader r(body);
    code = r.u8();
    msg = r.str();
}

} // namespace im
