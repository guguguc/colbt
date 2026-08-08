// SQLite 持久化层实现
// 所有方法线程安全（内部用 mutex_ 串行化），表结构在 open() 中自建并做列迁移。
// 表：users / friends / groups / group_members / messages / files
// 文件本体存在 <db目录>/files，files 表只存元数据。
#include "server/storage.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include <openssl/sha.h>
#include <sqlite3.h>

namespace im {

std::string sha256Hex(const std::string& input) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(input.data()), input.size(), digest);
    char out[SHA256_DIGEST_LENGTH * 2 + 1];
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
        std::snprintf(out + i * 2, 3, "%02x", digest[i]);
    return std::string(out, SHA256_DIGEST_LENGTH * 2);
}

Storage::Storage() = default;
Storage::~Storage() { close(); }

static int busyHandler(void*, int count) {
    return count < 100 ? 1 : 0;
}

// 打开数据库：建表 + WAL 模式 + 旧库列迁移
bool Storage::open(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (db_) return true;
    int rc = sqlite3_open_v2(path.c_str(), &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                                                   SQLITE_OPEN_FULLMUTEX, nullptr);
    if (rc != SQLITE_OK) {
        if (db_) sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }
    sqlite3_busy_handler(db_, busyHandler, nullptr);
    std::string err;
    exec("PRAGMA journal_mode=WAL;", err);
    exec("CREATE TABLE IF NOT EXISTS users ("
         "id INTEGER PRIMARY KEY AUTOINCREMENT,"
         "username TEXT UNIQUE NOT NULL,"
         "pwd_hash TEXT NOT NULL,"
         "nickname TEXT NOT NULL DEFAULT '',"
         "avatar TEXT NOT NULL DEFAULT '',"
         "created_at INTEGER NOT NULL);", err);
    exec("CREATE TABLE IF NOT EXISTS friends ("
         "user_id INTEGER NOT NULL,"
         "friend_id INTEGER NOT NULL,"
         "remark TEXT NOT NULL DEFAULT '',"
         "created_at INTEGER NOT NULL,"
         "PRIMARY KEY(user_id, friend_id));", err);
    exec("CREATE TABLE IF NOT EXISTS groups ("
         "id INTEGER PRIMARY KEY AUTOINCREMENT,"
         "name TEXT NOT NULL,"
         "owner_id INTEGER NOT NULL,"
         "created_at INTEGER NOT NULL);", err);
    exec("CREATE TABLE IF NOT EXISTS group_members ("
         "group_id INTEGER NOT NULL,"
         "user_id INTEGER NOT NULL,"
         "group_nick TEXT NOT NULL DEFAULT '',"
         "joined_at INTEGER NOT NULL,"
         "PRIMARY KEY(group_id, user_id));", err);
    exec("CREATE TABLE IF NOT EXISTS messages ("
         "id INTEGER PRIMARY KEY AUTOINCREMENT,"
         "from_id INTEGER NOT NULL,"
         "target_id INTEGER NOT NULL,"
         "target_type INTEGER NOT NULL,"
         "msg_type INTEGER NOT NULL DEFAULT 0,"
         "content TEXT NOT NULL DEFAULT '',"
         "ts INTEGER NOT NULL,"
         "is_read INTEGER NOT NULL DEFAULT 0,"
         "reply_to_id INTEGER NOT NULL DEFAULT 0);", err);
    exec("CREATE INDEX IF NOT EXISTS idx_msg_target ON messages(target_id, target_type, id);",
         err);
    exec("CREATE TABLE IF NOT EXISTS files ("
         "id TEXT PRIMARY KEY,"
         "name TEXT NOT NULL,"
         "size INTEGER NOT NULL,"
         "mime TEXT NOT NULL DEFAULT '',"
         "created_at INTEGER NOT NULL);", err);
    // 迁移：旧库补充 is_read 列
    {
        bool hasRead = false;
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, "PRAGMA table_info(messages);", -1, &stmt, nullptr) ==
            SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                if (name && std::strcmp(name, "is_read") == 0) hasRead = true;
            }
            sqlite3_finalize(stmt);
        }
        if (!hasRead) {
            exec("ALTER TABLE messages ADD COLUMN is_read INTEGER NOT NULL DEFAULT 0;", err);
        }
        // 迁移 reply_to_id
        bool hasReply = false;
        stmt = nullptr;
        if (sqlite3_prepare_v2(db_, "PRAGMA table_info(messages);", -1, &stmt, nullptr) ==
            SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                if (name && std::strcmp(name, "reply_to_id") == 0) hasReply = true;
            }
            sqlite3_finalize(stmt);
        }
        if (!hasReply) {
            exec("ALTER TABLE messages ADD COLUMN reply_to_id INTEGER NOT NULL DEFAULT 0;", err);
        }
    }
    return db_ != nullptr;
}

// 关闭数据库
void Storage::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

// 执行无参数 SQL（建表用）
bool Storage::exec(const std::string& sql, std::string& err) {
    char* zErr = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &zErr);
    if (rc != SQLITE_OK) {
        err = zErr ? zErr : "sqlite error";
        sqlite3_free(zErr);
        return false;
    }
    return true;
}

// 创建用户；用户名唯一，返回新 id（失败返回 -1，err 含原因）
int64_t Storage::createUser(const std::string& username, const std::string& pwdHash,
                            const std::string& nickname, std::string& err) {
    std::lock_guard<std::mutex> lock(mutex_);
    sqlite3_stmt* stmt = nullptr;
    int64_t id = -1;
    const char* sql = "INSERT INTO users(username, pwd_hash, nickname, avatar, created_at) "
                      "VALUES(?,?,?,?,strftime('%s','now'));";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        err = "db error";
        return -1;
    }
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, pwdHash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, nickname.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, "", -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        err = sqlite3_errmsg(db_);
        sqlite3_finalize(stmt);
        return -1;
    }
    id = sqlite3_last_insert_rowid(db_);
    sqlite3_finalize(stmt);
    return id;
}

// 按用户名查找用户及其密码哈希
bool Storage::findUserByName(const std::string& username, UserInfo& out, std::string& pwdHash) {
    std::lock_guard<std::mutex> lock(mutex_);
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id, username, nickname, avatar, pwd_hash FROM users WHERE username=?;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        out.id = sqlite3_column_int64(stmt, 0);
        out.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        out.nickname = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        out.avatar = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        out.online = 0;
        pwdHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        ok = true;
    }
    sqlite3_finalize(stmt);
    return ok;
}

// 按 id 查找用户
bool Storage::findUserById(int64_t id, UserInfo& out) {
    std::lock_guard<std::mutex> lock(mutex_);
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id, username, nickname, avatar FROM users WHERE id=?;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int64(stmt, 1, id);
    bool ok = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        out.id = sqlite3_column_int64(stmt, 0);
        out.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        out.nickname = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        out.avatar = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        out.online = 0;
        ok = true;
    }
    sqlite3_finalize(stmt);
    return ok;
}

// 更新资料：仅更新非空字段（newPwdHash 为空则不改密码）
bool Storage::updateUser(int64_t id, const std::string& nickname, const std::string& avatar,
                         const std::string& newPwdHash, std::string& err) {
    std::lock_guard<std::mutex> lock(mutex_);
    // 动态拼接 SET：只更新非空字段
    std::string sets;
    std::vector<std::string> binds;
    if (!nickname.empty()) { sets += "nickname=?, "; binds.push_back(nickname); }
    if (!avatar.empty()) { sets += "avatar=?, "; binds.push_back(avatar); }
    if (!newPwdHash.empty()) { sets += "pwd_hash=?, "; binds.push_back(newPwdHash); }
    if (sets.empty()) return true; // 没有要更新的字段
    sets.pop_back(); // 去掉末尾 ", "
    sets.pop_back();
    std::string sql = "UPDATE users SET " + sets + " WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        err = sqlite3_errmsg(db_);
        return false;
    }
    for (size_t i = 0; i < binds.size(); ++i)
        sqlite3_bind_text(stmt, static_cast<int>(i + 1), binds[i].c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, static_cast<int>(binds.size() + 1), id);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    if (!ok) err = sqlite3_errmsg(db_);
    return ok;
}

// 添加好友（单向，调用方需双向插入）
bool Storage::addFriend(int64_t userId, int64_t friendId, const std::string& remark) {
    std::lock_guard<std::mutex> lock(mutex_);
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT OR IGNORE INTO friends(user_id, friend_id, remark, created_at) "
                      "VALUES(?,?,?,strftime('%s','now'));";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int64(stmt, 1, userId);
    sqlite3_bind_int64(stmt, 2, friendId);
    sqlite3_bind_text(stmt, 3, remark.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    // 双向好友
    if (ok) {
        sqlite3_stmt* s2 = nullptr;
        const char* sql2 = "INSERT OR IGNORE INTO friends(user_id, friend_id, remark, created_at) "
                           "VALUES(?,?,?,strftime('%s','now'));";
        if (sqlite3_prepare_v2(db_, sql2, -1, &s2, nullptr) == SQLITE_OK) {
            sqlite3_bind_int64(s2, 1, friendId);
            sqlite3_bind_int64(s2, 2, userId);
            sqlite3_bind_text(s2, 3, "", -1, SQLITE_TRANSIENT);
            sqlite3_step(s2);
            sqlite3_finalize(s2);
        }
    }
    return ok;
}

// 取某用户的好友列表
std::vector<BuddyInfo> Storage::getFriends(int64_t userId) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<BuddyInfo> out;
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT u.id, u.username, u.nickname, u.avatar, f.remark "
        "FROM friends f JOIN users u ON f.friend_id = u.id WHERE f.user_id=? ORDER BY u.id;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return out;
    sqlite3_bind_int64(stmt, 1, userId);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        BuddyInfo b;
        b.user.id = sqlite3_column_int64(stmt, 0);
        b.user.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        b.user.nickname = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        b.user.avatar = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        b.remark = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        out.push_back(b);
    }
    sqlite3_finalize(stmt);
    return out;
}

// 判断两人是否互为好友
bool Storage::areFriends(int64_t a, int64_t b) {
    if (a == b) return false;
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT 1 FROM friends WHERE user_id=? AND friend_id=?;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int64(stmt, 1, a);
    sqlite3_bind_int64(stmt, 2, b);
    bool ok = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return ok;
}

// 删除好友（单向）
bool Storage::deleteFriend(int64_t a, int64_t b) {
    std::lock_guard<std::mutex> lock(mutex_);
    bool ok = true;
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "DELETE FROM friends WHERE (user_id=? AND friend_id=?) OR "
                      "(user_id=? AND friend_id=?);";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, a);
        sqlite3_bind_int64(stmt, 2, b);
        sqlite3_bind_int64(stmt, 3, b);
        sqlite3_bind_int64(stmt, 4, a);
        ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
    } else {
        ok = false;
    }
    return ok;
}

// 创建群，返回群 id
int64_t Storage::createGroup(const std::string& name, int64_t ownerId, std::string& err) {
    std::lock_guard<std::mutex> lock(mutex_);
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO groups(name, owner_id, created_at) VALUES(?,?,strftime('%s','now'));";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        err = "db error";
        return -1;
    }
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, ownerId);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        err = sqlite3_errmsg(db_);
        sqlite3_finalize(stmt);
        return -1;
    }
    int64_t id = sqlite3_last_insert_rowid(db_);
    sqlite3_finalize(stmt);
    return id;
}

// 拉成员进群
bool Storage::addGroupMember(int64_t groupId, int64_t userId, const std::string& groupNick) {
    std::lock_guard<std::mutex> lock(mutex_);
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT OR IGNORE INTO group_members(group_id, user_id, group_nick, "
                      "joined_at) VALUES(?,?,?,strftime('%s','now'));";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int64(stmt, 1, groupId);
    sqlite3_bind_int64(stmt, 2, userId);
    sqlite3_bind_text(stmt, 3, groupNick.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

// 取用户加入的所有群（含成员）
std::vector<GroupInfo> Storage::getGroups(int64_t userId) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<GroupInfo> out;
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT g.id, g.name, g.owner_id FROM groups g "
        "JOIN group_members m ON m.group_id = g.id WHERE m.user_id=? ORDER BY g.id;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return out;
    sqlite3_bind_int64(stmt, 1, userId);
    std::vector<std::pair<int64_t, std::string>> pending; // id, name, owner
    struct Row {
        int64_t id;
        std::string name;
        int64_t owner;
    };
    std::vector<Row> rows;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Row r;
        r.id = sqlite3_column_int64(stmt, 0);
        r.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        r.owner = sqlite3_column_int64(stmt, 2);
        rows.push_back(r);
    }
    sqlite3_finalize(stmt);
    for (auto& r : rows) {
        GroupInfo g;
        g.id = r.id;
        g.name = r.name;
        g.ownerId = r.owner;
        g.members = getGroupMembersUnlocked(r.id);
        out.push_back(g);
    }
    return out;
}

std::vector<MemberInfo> Storage::getGroupMembers(int64_t groupId) {
    std::lock_guard<std::mutex> lock(mutex_);
    return getGroupMembersUnlocked(groupId);
}

// 取群成员（调用方已持有锁）
std::vector<MemberInfo> Storage::getGroupMembersUnlocked(int64_t groupId) {
    std::vector<MemberInfo> out;
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT u.id, u.username, u.nickname, u.avatar, m.group_nick "
        "FROM group_members m JOIN users u ON m.user_id = u.id WHERE m.group_id=? ORDER BY u.id;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return out;
    sqlite3_bind_int64(stmt, 1, groupId);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        MemberInfo m;
        m.user.id = sqlite3_column_int64(stmt, 0);
        m.user.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        m.user.nickname = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        m.user.avatar = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        m.groupNick = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        out.push_back(m);
    }
    sqlite3_finalize(stmt);
    return out;
}

// 判断是否为群成员
bool Storage::isGroupMember(int64_t groupId, int64_t userId) {
    std::lock_guard<std::mutex> lock(mutex_);
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT 1 FROM group_members WHERE group_id=? AND user_id=?;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int64(stmt, 1, groupId);
    sqlite3_bind_int64(stmt, 2, userId);
    bool ok = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return ok;
}

// 按群 id 查群（0=不存在）
int64_t Storage::findGroupById(int64_t id, GroupInfo& out) {
    std::lock_guard<std::mutex> lock(mutex_);
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id, name, owner_id FROM groups WHERE id=?;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return -1;
    sqlite3_bind_int64(stmt, 1, id);
    int64_t ret = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        out.id = sqlite3_column_int64(stmt, 0);
        out.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        out.ownerId = sqlite3_column_int64(stmt, 2);
        ret = out.id;
    }
    sqlite3_finalize(stmt);
    if (ret > 0) out.members = getGroupMembersUnlocked(id);
    return ret;
}

// 踢人：删除群成员记录
bool Storage::kickMember(int64_t groupId, int64_t memberId) {
    std::lock_guard<std::mutex> lock(mutex_);
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "DELETE FROM group_members WHERE group_id=? AND user_id=?;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int64(stmt, 1, groupId);
    sqlite3_bind_int64(stmt, 2, memberId);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db_) > 0;
    sqlite3_finalize(stmt);
    return ok;
}

// 退群
bool Storage::leaveGroup(int64_t groupId, int64_t userId) {
    return kickMember(groupId, userId);
}

// 解散群：删群并清空成员
bool Storage::dismissGroup(int64_t groupId) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string err;
    bool ok = true;
    {
        sqlite3_stmt* stmt = nullptr;
        const char* sql = "DELETE FROM group_members WHERE group_id=?;";
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int64(stmt, 1, groupId);
            ok = sqlite3_step(stmt) == SQLITE_DONE;
            sqlite3_finalize(stmt);
        } else ok = false;
    }
    if (ok) {
        sqlite3_stmt* stmt = nullptr;
        const char* sql = "DELETE FROM groups WHERE id=?;";
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int64(stmt, 1, groupId);
            ok = sqlite3_step(stmt) == SQLITE_DONE;
            sqlite3_finalize(stmt);
        } else ok = false;
    }
    return ok;
}

// 改群名
bool Storage::renameGroup(int64_t groupId, const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "UPDATE groups SET name=? WHERE id=?;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, groupId);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

// 保存一条消息，返回消息 id
int64_t Storage::saveMessage(int64_t fromId, int64_t targetId, int targetType, int msgType,
                             const std::string& content, int64_t ts, int isRead,
                             int64_t replyToId) {
    std::lock_guard<std::mutex> lock(mutex_);
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO messages(from_id, target_id, target_type, msg_type, content, "
                      "ts, is_read, reply_to_id) VALUES(?,?,?,?,?,?,?,?);";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return -1;
    sqlite3_bind_int64(stmt, 1, fromId);
    sqlite3_bind_int64(stmt, 2, targetId);
    sqlite3_bind_int64(stmt, 3, targetType);
    sqlite3_bind_int64(stmt, 4, msgType);
    sqlite3_bind_text(stmt, 5, content.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 6, ts);
    sqlite3_bind_int(stmt, 7, isRead);
    sqlite3_bind_int64(stmt, 8, replyToId);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return -1;
    }
    int64_t id = sqlite3_last_insert_rowid(db_);
    sqlite3_finalize(stmt);
    return id;
}

// 按 id 查消息（撤回校验用）
bool Storage::findMessage(int64_t msgId, MessageInfo& out) {
    std::lock_guard<std::mutex> lock(mutex_);
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id, from_id, target_id, target_type, msg_type, content, ts, "
                      "is_read, reply_to_id FROM messages WHERE id=?;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int64(stmt, 1, msgId);
    bool ok = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        out.id = sqlite3_column_int64(stmt, 0);
        out.fromId = sqlite3_column_int64(stmt, 1);
        out.targetId = sqlite3_column_int64(stmt, 2);
        out.targetType = sqlite3_column_int(stmt, 3);
        out.msgType = sqlite3_column_int(stmt, 4);
        out.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        out.timestamp = sqlite3_column_int64(stmt, 6);
        out.read = sqlite3_column_int(stmt, 7);
        out.replyToId = sqlite3_column_int64(stmt, 8);
        ok = true;
    }
    sqlite3_finalize(stmt);
    return ok;
}

// 撤回：仅发送者可删，返回是否成功
bool Storage::recallMessage(int64_t msgId, int64_t fromId) {
    std::lock_guard<std::mutex> lock(mutex_);
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "DELETE FROM messages WHERE id=? AND from_id=?;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int64(stmt, 1, msgId);
    sqlite3_bind_int64(stmt, 2, fromId);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db_) > 0;
    sqlite3_finalize(stmt);
    return ok;
}

// 把 readerId 与 peerId 会话中对方发来的消息标记已读，返回条数
int Storage::markMessagesRead(int64_t readerId, int64_t peerId, int targetType) {
    std::lock_guard<std::mutex> lock(mutex_);
    sqlite3_stmt* stmt = nullptr;
    if (targetType == TARGET_GROUP) {
        // 群消息：把该群里"别人发给我的"标记已读
        const char* sql = "UPDATE messages SET is_read=1 "
                          "WHERE target_type=1 AND target_id=? AND from_id!=? AND is_read=0;";
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0;
        sqlite3_bind_int64(stmt, 1, peerId);
        sqlite3_bind_int64(stmt, 2, readerId);
    } else {
        const char* sql = "UPDATE messages SET is_read=1 "
                          "WHERE target_type=0 AND from_id=? AND target_id=? AND is_read=0;";
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0;
        sqlite3_bind_int64(stmt, 1, peerId);
        sqlite3_bind_int64(stmt, 2, readerId);
    }
    int rc = sqlite3_step(stmt);
    int changed = rc == SQLITE_DONE ? sqlite3_changes(db_) : 0;
    sqlite3_finalize(stmt);
    return changed;
}

// 登记文件元数据（文件本体已由 servercore 写入磁盘）
bool Storage::createFile(const std::string& fileId, const std::string& name, int64_t size,
                         const std::string& mime) {
    std::lock_guard<std::mutex> lock(mutex_);
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT OR REPLACE INTO files(id, name, size, mime, created_at) "
                      "VALUES(?,?,?,?,strftime('%s','now'));";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, fileId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, size);
    sqlite3_bind_text(stmt, 4, mime.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

// 按 fileId 查文件元数据
bool Storage::findFile(const std::string& fileId, std::string& name, int64_t& size,
                       std::string& mime) {
    std::lock_guard<std::mutex> lock(mutex_);
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT name, size, mime FROM files WHERE id=?;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, fileId.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        size = sqlite3_column_int64(stmt, 1);
        mime = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        ok = true;
    }
    sqlite3_finalize(stmt);
    return ok;
}

// 取会话历史：单聊双向查询、群聊按群查，支持 beforeId 分页
std::vector<MessageInfo> Storage::getHistory(int64_t myUserId, int64_t targetId, int targetType,
                                             int limit, int64_t beforeId) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<MessageInfo> out;
    sqlite3_stmt* stmt = nullptr;
    std::string sql;
    if (targetType == TARGET_GROUP) {
        sql = "SELECT m.id, m.from_id, m.target_id, m.target_type, m.msg_type, m.content, m.ts, "
              "u.nickname, m.is_read, m.reply_to_id, r.content AS rc "
              "FROM messages m LEFT JOIN users u ON m.from_id = u.id "
              "LEFT JOIN messages r ON r.id = m.reply_to_id "
              "WHERE m.target_id=? AND m.target_type=1 ";
    } else {
        // 单聊：双方互为 peer，查 "我发给对方" 与 "对方发给我"
        sql = "SELECT m.id, m.from_id, m.target_id, m.target_type, m.msg_type, m.content, m.ts, "
              "u.nickname, m.is_read, m.reply_to_id, r.content AS rc "
              "FROM messages m LEFT JOIN users u ON m.from_id = u.id "
              "LEFT JOIN messages r ON r.id = m.reply_to_id "
              "WHERE m.target_type=0 AND ((m.from_id=? AND m.target_id=?) OR "
              "(m.from_id=? AND m.target_id=?)) ";
    }
    if (beforeId > 0) sql += "AND m.id < " + std::to_string(beforeId) + " ";
    sql += "ORDER BY m.id DESC LIMIT ?;";
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return out;
    if (targetType == TARGET_GROUP) {
        sqlite3_bind_int64(stmt, 1, targetId);
        sqlite3_bind_int(stmt, 2, limit);
    } else {
        sqlite3_bind_int64(stmt, 1, myUserId);
        sqlite3_bind_int64(stmt, 2, targetId);
        sqlite3_bind_int64(stmt, 3, targetId);
        sqlite3_bind_int64(stmt, 4, myUserId);
        sqlite3_bind_int(stmt, 5, limit);
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        MessageInfo m;
        m.id = sqlite3_column_int64(stmt, 0);
        m.fromId = sqlite3_column_int64(stmt, 1);
        m.targetId = sqlite3_column_int64(stmt, 2);
        m.targetType = sqlite3_column_int(stmt, 3);
        m.msgType = sqlite3_column_int(stmt, 4);
        m.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        m.timestamp = sqlite3_column_int64(stmt, 6);
        m.senderName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        m.read = sqlite3_column_int(stmt, 8);
        m.replyToId = sqlite3_column_int64(stmt, 9);
        const char* rc = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
        if (rc) m.replyContent = rc;
        out.push_back(m);
    }
    sqlite3_finalize(stmt);
    std::reverse(out.begin(), out.end());
    return out;
}

// 搜索消息：只搜用户参与的会话
std::vector<MessageInfo> Storage::searchMessages(int64_t userId, const std::string& keyword,
                                                 int limit) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<MessageInfo> out;
    if (keyword.empty()) return out;
    std::string pat = "%" + keyword + "%";
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT m.id, m.from_id, m.target_id, m.target_type, m.msg_type, m.content, m.ts, "
        "u.nickname, m.is_read, m.reply_to_id, r.content AS rc "
        "FROM messages m "
        "LEFT JOIN users u ON u.id = m.from_id "
        "LEFT JOIN messages r ON r.id = m.reply_to_id "
        "WHERE m.content LIKE ? AND ("
        "  (m.target_type=0 AND (m.from_id=? OR m.target_id=?)) OR "
        "  (m.target_type=1 AND m.target_id IN "
        "    (SELECT group_id FROM group_members WHERE user_id=?))"
        ") ORDER BY m.id DESC LIMIT ?;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return out;
    sqlite3_bind_text(stmt, 1, pat.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, userId);
    sqlite3_bind_int64(stmt, 3, userId);
    sqlite3_bind_int64(stmt, 4, userId);
    sqlite3_bind_int(stmt, 5, limit);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        MessageInfo m;
        m.id = sqlite3_column_int64(stmt, 0);
        m.fromId = sqlite3_column_int64(stmt, 1);
        m.targetId = sqlite3_column_int64(stmt, 2);
        m.targetType = sqlite3_column_int(stmt, 3);
        m.msgType = sqlite3_column_int(stmt, 4);
        m.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        m.timestamp = sqlite3_column_int64(stmt, 6);
        m.senderName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        m.read = sqlite3_column_int(stmt, 8);
        m.replyToId = sqlite3_column_int64(stmt, 9);
        const char* rc = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
        if (rc) m.replyContent = rc;
        out.push_back(m);
    }
    sqlite3_finalize(stmt);
    std::reverse(out.begin(), out.end());
    return out;
}

std::vector<SessionInfo> Storage::getSessions(int64_t userId, int limit) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<SessionInfo> out;
    // 单聊会话：与我有消息往来的好友
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT peer, title, avatar, last_content, last_ts, unread FROM ("
        "SELECT CASE WHEN m.from_id=? THEN m.target_id ELSE m.from_id END AS peer, "
        "m.ts AS last_ts, m.content AS last_content, m.id AS mid, "
        "u.nickname AS title, u.avatar AS avatar, 0 AS unread "
        "FROM messages m LEFT JOIN users u ON u.id = CASE WHEN m.from_id=? THEN m.target_id "
        "ELSE m.from_id END "
        "WHERE m.target_type=0 AND (m.from_id=? OR m.target_id=?) "
        "ORDER BY m.id DESC) t GROUP BY peer ORDER BY last_ts DESC LIMIT ?;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, userId);
        sqlite3_bind_int64(stmt, 2, userId);
        sqlite3_bind_int64(stmt, 3, userId);
        sqlite3_bind_int64(stmt, 4, userId);
        sqlite3_bind_int(stmt, 5, limit);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            SessionInfo s;
            s.targetId = sqlite3_column_int64(stmt, 0);
            s.targetType = TARGET_FRIEND;
            s.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            s.avatar = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            s.lastContent = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            s.lastTime = sqlite3_column_int64(stmt, 4);
            s.unread = sqlite3_column_int(stmt, 5);
            if (!s.title.empty()) out.push_back(s);
        }
        sqlite3_finalize(stmt);
    }
    // 群聊会话
    sqlite3_stmt* stmt2 = nullptr;
    const char* sql2 =
        "SELECT g.id, g.name, m.id AS mid, m.content, m.ts FROM "
        "(SELECT * FROM messages WHERE target_type=1 AND target_id IN "
        "(SELECT group_id FROM group_members WHERE user_id=?) ORDER BY id DESC) m "
        "JOIN groups g ON g.id = m.target_id "
        "GROUP BY m.target_id ORDER BY m.id DESC LIMIT ?;";
    if (sqlite3_prepare_v2(db_, sql2, -1, &stmt2, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt2, 1, userId);
        sqlite3_bind_int(stmt2, 2, limit);
        while (sqlite3_step(stmt2) == SQLITE_ROW) {
            SessionInfo s;
            s.targetId = sqlite3_column_int64(stmt2, 0);
            s.targetType = TARGET_GROUP;
            s.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt2, 1));
            s.lastContent = reinterpret_cast<const char*>(sqlite3_column_text(stmt2, 3));
            s.lastTime = sqlite3_column_int64(stmt2, 4);
            s.unread = 0;
            out.push_back(s);
        }
        sqlite3_finalize(stmt2);
    }
    std::sort(out.begin(), out.end(),
              [](const SessionInfo& a, const SessionInfo& b) { return a.lastTime > b.lastTime; });
    return out;
}

} // namespace im
