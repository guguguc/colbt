#pragma once

#include <jni.h>
#include <string>
#include <vector>

namespace colbt {
namespace jni {

// 缓存 Java 类引用与方法 ID（首次使用时初始化）
class JavaRefs {
public:
    static JavaRefs& instance();

    // 全局 JVM 指针
    void setJvm(JavaVM* jvm) { jvm_ = jvm; }
    JavaVM* jvm() const { return jvm_; }

    // 初始化：缓存 jclass（全局引用）与构造器/方法 ID
    void init(JNIEnv* env, jobject listener);

    bool ready() const { return ready_; }

    jclass msgCls() const { return msgCls_; }
    jclass buddyCls() const { return buddyCls_; }
    jclass memberCls() const { return memberCls_; }
    jclass groupCls() const { return groupCls_; }
    jclass sessionCls() const { return sessionCls_; }
    jclass arrayListCls() const { return arrayListCls_; }
    jmethodID arrayListCtor() const { return arrayListCtor_; }
    jmethodID listAdd() const { return listAdd_; }

    // 数据类构造器
    jmethodID msgCtor() const { return msgCtor_; }
    jmethodID buddyCtor() const { return buddyCtor_; }
    jmethodID memberCtor() const { return memberCtor_; }
    jmethodID groupCtor() const { return groupCtor_; }
    jmethodID sessionCtor() const { return sessionCtor_; }

    // 监听器方法
    jobject listener() const { return listener_; }
    jmethodID onConnectionChanged() const { return onConnectionChanged_; }
    jmethodID onLoginResult() const { return onLoginResult_; }
    jmethodID onRegisterResult() const { return onRegisterResult_; }
    jmethodID onContactsLoaded() const { return onContactsLoaded_; }
    jmethodID onSessionsLoaded() const { return onSessionsLoaded_; }
    jmethodID onHistoryLoaded() const { return onHistoryLoaded_; }
    jmethodID onMessage() const { return onMessage_; }
    jmethodID onMessageSent() const { return onMessageSent_; }
    jmethodID onMessageRecalled() const { return onMessageRecalled_; }
    jmethodID onFriendDeleted() const { return onFriendDeleted_; }
    jmethodID onGroupUpdated() const { return onGroupUpdated_; }
    jmethodID onSearchResults() const { return onSearchResults_; }
    jmethodID onTyping() const { return onTyping_; }
    jmethodID onMessagesRead() const { return onMessagesRead_; }
    jmethodID onReadReceipt() const { return onReadReceipt_; }
    jmethodID onFileDownloaded() const { return onFileDownloaded_; }
    jmethodID onPresenceChanged() const { return onPresenceChanged_; }
    jmethodID onFriendAdded() const { return onFriendAdded_; }
    jmethodID onGroupCreated() const { return onGroupCreated_; }
    jmethodID onGroupMembersLoaded() const { return onGroupMembersLoaded_; }
    jmethodID onError() const { return onError_; }
    jmethodID onProfileUpdated() const { return onProfileUpdated_; }
    jmethodID onProfileChanged() const { return onProfileChanged_; }

private:
    JavaRefs() = default;
    JavaVM* jvm_ = nullptr;
    jobject listener_ = nullptr;
    bool ready_ = false;
    jclass msgCls_ = nullptr;
    jclass buddyCls_ = nullptr;
    jclass memberCls_ = nullptr;
    jclass groupCls_ = nullptr;
    jclass sessionCls_ = nullptr;
    jclass arrayListCls_ = nullptr;
    jmethodID arrayListCtor_ = nullptr;
    jmethodID listAdd_ = nullptr;
    jmethodID msgCtor_ = nullptr;
    jmethodID buddyCtor_ = nullptr;
    jmethodID memberCtor_ = nullptr;
    jmethodID groupCtor_ = nullptr;
    jmethodID sessionCtor_ = nullptr;
    jmethodID onConnectionChanged_ = nullptr;
    jmethodID onLoginResult_ = nullptr;
    jmethodID onRegisterResult_ = nullptr;
    jmethodID onContactsLoaded_ = nullptr;
    jmethodID onSessionsLoaded_ = nullptr;
    jmethodID onHistoryLoaded_ = nullptr;
    jmethodID onMessage_ = nullptr;
    jmethodID onMessageSent_ = nullptr;
    jmethodID onMessageRecalled_ = nullptr;
    jmethodID onFriendDeleted_ = nullptr;
    jmethodID onGroupUpdated_ = nullptr;
    jmethodID onSearchResults_ = nullptr;
    jmethodID onTyping_ = nullptr;
    jmethodID onMessagesRead_ = nullptr;
    jmethodID onReadReceipt_ = nullptr;
    jmethodID onFileDownloaded_ = nullptr;
    jmethodID onPresenceChanged_ = nullptr;
    jmethodID onFriendAdded_ = nullptr;
    jmethodID onGroupCreated_ = nullptr;
    jmethodID onGroupMembersLoaded_ = nullptr;
    jmethodID onError_ = nullptr;
    jmethodID onProfileUpdated_ = nullptr;
    jmethodID onProfileChanged_ = nullptr;
};

// 便捷：把 im::UserInfo / BuddyInfo / GroupInfo / MessageInfo / SessionInfo / MemberInfo
// 转换为 Java 对象（使用 JavaRefs 中的构造器）
jobject toJavaString(JNIEnv* env, const std::string& s);
jobjectArray toJavaStringArray(JNIEnv* env, const std::vector<std::string>& v);

} // namespace jni
} // namespace colbt
