#include "jni_bridge.h"

#include <android/log.h>
#include <cstring>
#include <string>
#include <vector>

namespace colbt {
namespace jni {

namespace {
constexpr const char* kMsgClass = "com/colbt/im/core/ImMessage";
constexpr const char* kBuddyClass = "com/colbt/im/core/ImBuddy";
constexpr const char* kMemberClass = "com/colbt/im/core/ImMember";
constexpr const char* kGroupClass = "com/colbt/im/core/ImGroup";
constexpr const char* kSessionClass = "com/colbt/im/core/ImSession";
constexpr const char* kListenerClass = "com/colbt/im/core/ImListener";
constexpr const char* kListClass = "java/util/ArrayList";

jmethodID getMid(JNIEnv* env, jclass cls, const char* name, const char* sig) {
    jmethodID id = env->GetMethodID(cls, name, sig);
    if (!id) env->ExceptionClear(); // 返回 null，由调用方处理
    return id;
}
} // namespace

JavaRefs& JavaRefs::instance() {
    static JavaRefs refs;
    return refs;
}

jobject toJavaString(JNIEnv* env, const std::string& s) {
    return env->NewStringUTF(s.c_str());
}

void JavaRefs::init(JNIEnv* env, jobject listener) {
    if (ready_) return;
    jclass msgCls = env->FindClass(kMsgClass);
    jclass buddyCls = env->FindClass(kBuddyClass);
    jclass memberCls = env->FindClass(kMemberClass);
    jclass groupCls = env->FindClass(kGroupClass);
    jclass sessionCls = env->FindClass(kSessionClass);
    jclass listenerCls = env->FindClass(kListenerClass);
    jclass listCls = env->FindClass(kListClass);
    if (!msgCls || !buddyCls || !memberCls || !groupCls || !sessionCls || !listenerCls ||
        !listCls) {
        env->ExceptionClear();
        return;
    }
    // 缓存全局引用
    msgCls_ = (jclass)env->NewGlobalRef(msgCls);
    buddyCls_ = (jclass)env->NewGlobalRef(buddyCls);
    memberCls_ = (jclass)env->NewGlobalRef(memberCls);
    groupCls_ = (jclass)env->NewGlobalRef(groupCls);
    sessionCls_ = (jclass)env->NewGlobalRef(sessionCls);
    arrayListCls_ = (jclass)env->NewGlobalRef(listCls);
    listenerCls = (jclass)env->NewGlobalRef(listenerCls);
    listener_ = env->NewGlobalRef(listener);
    arrayListCtor_ = getMid(env, arrayListCls_, "<init>", "()V");
    listAdd_ = getMid(env, arrayListCls_, "add", "(Ljava/lang/Object;)Z");

    msgCtor_ = getMid(env, msgCls, "<init>",
        "(JJJIILjava/lang/String;JIILjava/lang/String;JLjava/lang/String;)V");
    buddyCtor_ = getMid(env, buddyCls, "<init>",
        "(JLjava/lang/String;Ljava/lang/String;Ljava/lang/String;ILjava/lang/String;)V");
    memberCtor_ = getMid(env, memberCls, "<init>",
        "(JLjava/lang/String;Ljava/lang/String;Ljava/lang/String;ILjava/lang/String;)V");
    groupCtor_ = getMid(env, groupCls, "<init>",
        "(JLjava/lang/String;JLjava/util/List;)V");
    sessionCtor_ = getMid(env, sessionCls, "<init>",
        "(JILjava/lang/String;Ljava/lang/String;Ljava/lang/String;JI)V");

    onConnectionChanged_ = getMid(env, listenerCls, "onConnectionChanged", "(Z)V");
    onLoginResult_ = getMid(env, listenerCls, "onLoginResult",
        "(ILjava/lang/String;Lcom/colbt/im/core/ImBuddy;)V");
    onRegisterResult_ = getMid(env, listenerCls, "onRegisterResult", "(ILjava/lang/String;)V");
    onContactsLoaded_ = getMid(env, listenerCls, "onContactsLoaded",
        "(Ljava/util/List;Ljava/util/List;)V");
    onSessionsLoaded_ = getMid(env, listenerCls, "onSessionsLoaded", "(Ljava/util/List;)V");
    onHistoryLoaded_ = getMid(env, listenerCls, "onHistoryLoaded", "(JILjava/util/List;)V");
    onMessage_ = getMid(env, listenerCls, "onMessage",
        "(Lcom/colbt/im/core/ImMessage;)V");
    onMessageSent_ = getMid(env, listenerCls, "onMessageSent",
        "(Lcom/colbt/im/core/ImMessage;)V");
    onMessageRecalled_ = getMid(env, listenerCls, "onMessageRecalled", "(JJI)V");
    onFriendDeleted_ = getMid(env, listenerCls, "onFriendDeleted", "(JLjava/lang/String;)V");
    onGroupUpdated_ = getMid(env, listenerCls, "onGroupUpdated", "(J)V");
    onSearchResults_ = getMid(env, listenerCls, "onSearchResults", "(Ljava/util/List;)V");
    onTyping_ = getMid(env, listenerCls, "onTyping", "(JJI)V");
    onMessagesRead_ = getMid(env, listenerCls, "onMessagesRead", "(JI)V");
    onReadReceipt_ = getMid(env, listenerCls, "onReadReceipt", "(JI)V");
    onFileDownloaded_ = getMid(env, listenerCls, "onFileDownloaded",
        "(Ljava/lang/String;Ljava/lang/String;JLjava/lang/String;[B)V");
    onPresenceChanged_ = getMid(env, listenerCls, "onPresenceChanged", "(JZ)V");
    onFriendAdded_ = getMid(env, listenerCls, "onFriendAdded",
        "(Lcom/colbt/im/core/ImBuddy;)V");
    onGroupCreated_ = getMid(env, listenerCls, "onGroupCreated",
        "(Lcom/colbt/im/core/ImGroup;)V");
    onGroupMembersLoaded_ = getMid(env, listenerCls, "onGroupMembersLoaded",
        "(JLjava/util/List;)V");
    onError_ = getMid(env, listenerCls, "onError", "(ILjava/lang/String;)V");
    onProfileUpdated_ = getMid(env, listenerCls, "onProfileUpdated",
        "(ILjava/lang/String;Lcom/colbt/im/core/ImBuddy;)V");
    onProfileChanged_ = getMid(env, listenerCls, "onProfileChanged",
        "(JLjava/lang/String;Ljava/lang/String;)V");

    ready_ = true;
    env->ExceptionClear();
}

} // namespace jni
} // namespace colbt
