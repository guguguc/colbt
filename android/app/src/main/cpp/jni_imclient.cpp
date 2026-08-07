#include <jni.h>
#include <vector>
#include <string>

#include "im/core/icclient.h"
#include "im/core/types.h"
#include "jni_bridge.h"

using namespace im;

namespace {

// 把 IClientListener 回调转发到 Java 监听器（工作线程调用，先 attach 到 JVM）
class JniListener : public IClientListener {
public:
    static JNIEnv* env() {
        JavaVM* jvm = colbt::jni::JavaRefs::instance().jvm();
        if (!jvm) return nullptr;
        JNIEnv* e = nullptr;
        if (jvm->GetEnv(reinterpret_cast<void**>(&e), JNI_VERSION_1_6) == JNI_EDETACHED) {
            jvm->AttachCurrentThread(&e, nullptr);
        }
        return e;
    }

    static jobject toJavaString(JNIEnv* e, const std::string& s) {
        return e->NewStringUTF(s.c_str());
    }

    static jobject toJavaMessage(JNIEnv* e, const MessageInfo& m) {
        const auto& r = colbt::jni::JavaRefs::instance();
        return e->NewObject(r.msgCls(), r.msgCtor(), (jlong)m.id, (jlong)m.fromId,
                            (jlong)m.targetId, (jint)m.targetType, (jint)m.msgType,
                            toJavaString(e, m.content), (jlong)m.timestamp, (jint)m.direction,
                            (jint)m.read, toJavaString(e, m.senderName), (jlong)m.replyToId,
                            toJavaString(e, m.replyContent));
    }

    static jobject toJavaBuddy(JNIEnv* e, const BuddyInfo& b) {
        const auto& r = colbt::jni::JavaRefs::instance();
        return e->NewObject(r.buddyCls(), r.buddyCtor(), (jlong)b.user.id,
                            toJavaString(e, b.user.username), toJavaString(e, b.user.nickname),
                            toJavaString(e, b.user.avatar), (jint)b.user.online,
                            toJavaString(e, b.remark));
    }

    static jobject toJavaMember(JNIEnv* e, const MemberInfo& m) {
        const auto& r = colbt::jni::JavaRefs::instance();
        return e->NewObject(r.memberCls(), r.memberCtor(), (jlong)m.user.id,
                            toJavaString(e, m.user.username), toJavaString(e, m.user.nickname),
                            toJavaString(e, m.user.avatar), (jint)m.user.online,
                            toJavaString(e, m.groupNick));
    }

    static jobject toJavaGroup(JNIEnv* e, const GroupInfo& g) {
        const auto& r = colbt::jni::JavaRefs::instance();
        jobject list = e->NewObject(r.arrayListCls(), r.arrayListCtor());
        for (const auto& m : g.members) {
            e->CallBooleanMethod(list, r.listAdd(), toJavaMember(e, m));
        }
        return e->NewObject(r.groupCls(), r.groupCtor(), (jlong)g.id, toJavaString(e, g.name),
                            (jlong)g.ownerId, list);
    }

    static jobject toJavaSession(JNIEnv* e, const SessionInfo& s) {
        const auto& r = colbt::jni::JavaRefs::instance();
        return e->NewObject(r.sessionCls(), r.sessionCtor(), (jlong)s.targetId, (jint)s.targetType,
                            toJavaString(e, s.title), toJavaString(e, s.avatar),
                            toJavaString(e, s.lastContent), (jlong)s.lastTime, (jint)s.unread);
    }

    static jobject toJavaList(JNIEnv* e, const std::vector<jobject>& items) {
        const auto& r = colbt::jni::JavaRefs::instance();
        jobject list = e->NewObject(r.arrayListCls(), r.arrayListCtor());
        for (const auto& it : items) e->CallBooleanMethod(list, r.listAdd(), it);
        return list;
    }

    static jobject toJavaBuddyUser(JNIEnv* e, const UserInfo& u) {
        const auto& r = colbt::jni::JavaRefs::instance();
        return e->NewObject(r.buddyCls(), r.buddyCtor(), (jlong)u.id,
                            toJavaString(e, u.username), toJavaString(e, u.nickname),
                            toJavaString(e, u.avatar), (jint)u.online, toJavaString(e, ""));
    }

    void callVoid(const char* name, ...) { /* 未使用 */ }

public:
    void onConnectionChanged(bool connected) override {
        JNIEnv* e = env();
        if (!e) return;
        const auto& r = colbt::jni::JavaRefs::instance();
        e->CallVoidMethod(r.listener(), r.onConnectionChanged(), (jboolean)connected);
    }
    void onLoginResult(int code, const std::string& msg, const UserInfo& me) override {
        JNIEnv* e = env();
        if (!e) return;
        const auto& r = colbt::jni::JavaRefs::instance();
        e->CallVoidMethod(r.listener(), r.onLoginResult(), (jint)code, toJavaString(e, msg),
                          toJavaBuddyUser(e, me));
    }
    void onRegisterResult(int code, const std::string& msg) override {
        JNIEnv* e = env();
        if (!e) return;
        const auto& r = colbt::jni::JavaRefs::instance();
        e->CallVoidMethod(r.listener(), r.onRegisterResult(), (jint)code, toJavaString(e, msg));
    }
    void onContactsLoaded(const std::vector<BuddyInfo>& buddies,
                          const std::vector<GroupInfo>& groups) override {
        JNIEnv* e = env();
        if (!e) return;
        const auto& r = colbt::jni::JavaRefs::instance();
        std::vector<jobject> bl, gl;
        for (const auto& b : buddies) bl.push_back(toJavaBuddy(e, b));
        for (const auto& g : groups) gl.push_back(toJavaGroup(e, g));
        e->CallVoidMethod(r.listener(), r.onContactsLoaded(), toJavaList(e, bl), toJavaList(e, gl));
    }
    void onSessionsLoaded(const std::vector<SessionInfo>& sessions) override {
        JNIEnv* e = env();
        if (!e) return;
        const auto& r = colbt::jni::JavaRefs::instance();
        std::vector<jobject> sl;
        for (const auto& s : sessions) sl.push_back(toJavaSession(e, s));
        e->CallVoidMethod(r.listener(), r.onSessionsLoaded(), toJavaList(e, sl));
    }
    void onHistoryLoaded(int64_t targetId, int targetType,
                         const std::vector<MessageInfo>& msgs) override {
        JNIEnv* e = env();
        if (!e) return;
        const auto& r = colbt::jni::JavaRefs::instance();
        std::vector<jobject> ml;
        for (const auto& m : msgs) ml.push_back(toJavaMessage(e, m));
        e->CallVoidMethod(r.listener(), r.onHistoryLoaded(), (jlong)targetId, (jint)targetType,
                          toJavaList(e, ml));
    }
    void onMessage(const MessageInfo& msg) override {
        JNIEnv* e = env();
        if (!e) return;
        const auto& r = colbt::jni::JavaRefs::instance();
        e->CallVoidMethod(r.listener(), r.onMessage(), toJavaMessage(e, msg));
    }
    void onMessageSent(const MessageInfo& msg) override {
        JNIEnv* e = env();
        if (!e) return;
        const auto& r = colbt::jni::JavaRefs::instance();
        e->CallVoidMethod(r.listener(), r.onMessageSent(), toJavaMessage(e, msg));
    }
    void onMessageRecalled(int64_t msgId, int64_t targetId, int targetType) override {
        JNIEnv* e = env();
        if (!e) return;
        const auto& r = colbt::jni::JavaRefs::instance();
        e->CallVoidMethod(r.listener(), r.onMessageRecalled(), (jlong)msgId, (jlong)targetId,
                          (jint)targetType);
    }
    void onFriendDeleted(int64_t friendId, const std::string& name) override {
        JNIEnv* e = env();
        if (!e) return;
        const auto& r = colbt::jni::JavaRefs::instance();
        e->CallVoidMethod(r.listener(), r.onFriendDeleted(), (jlong)friendId,
                          toJavaString(e, name));
    }
    void onGroupUpdated(int64_t groupId) override {
        JNIEnv* e = env();
        if (!e) return;
        const auto& r = colbt::jni::JavaRefs::instance();
        e->CallVoidMethod(r.listener(), r.onGroupUpdated(), (jlong)groupId);
    }
    void onSearchResults(const std::vector<MessageInfo>& msgs) override {
        JNIEnv* e = env();
        if (!e) return;
        const auto& r = colbt::jni::JavaRefs::instance();
        std::vector<jobject> ml;
        for (const auto& m : msgs) ml.push_back(toJavaMessage(e, m));
        e->CallVoidMethod(r.listener(), r.onSearchResults(), toJavaList(e, ml));
    }
    void onTyping(int64_t fromId, int64_t targetId, int targetType) override {
        JNIEnv* e = env();
        if (!e) return;
        const auto& r = colbt::jni::JavaRefs::instance();
        e->CallVoidMethod(r.listener(), r.onTyping(), (jlong)fromId, (jlong)targetId,
                          (jint)targetType);
    }
    void onMessagesRead(int64_t peerId, int targetType) override {
        JNIEnv* e = env();
        if (!e) return;
        const auto& r = colbt::jni::JavaRefs::instance();
        e->CallVoidMethod(r.listener(), r.onMessagesRead(), (jlong)peerId, (jint)targetType);
    }
    void onReadReceipt(int64_t peerId, int targetType) override {
        JNIEnv* e = env();
        if (!e) return;
        const auto& r = colbt::jni::JavaRefs::instance();
        e->CallVoidMethod(r.listener(), r.onReadReceipt(), (jlong)peerId, (jint)targetType);
    }
    void onFileDownloaded(const std::string& fileId, const std::string& name, int64_t size,
                          const std::string& mime,
                          const std::vector<uint8_t>& data) override {
        JNIEnv* e = env();
        if (!e) return;
        const auto& r = colbt::jni::JavaRefs::instance();
        jbyteArray arr = e->NewByteArray(static_cast<jsize>(data.size()));
        if (!data.empty())
            e->SetByteArrayRegion(arr, 0, static_cast<jsize>(data.size()),
                                  reinterpret_cast<const jbyte*>(data.data()));
        e->CallVoidMethod(r.listener(), r.onFileDownloaded(), toJavaString(e, fileId),
                          toJavaString(e, name), (jlong)size, toJavaString(e, mime), arr);
        e->DeleteLocalRef(arr);
    }
    void onPresenceChanged(int64_t userId, bool online) override {
        JNIEnv* e = env();
        if (!e) return;
        const auto& r = colbt::jni::JavaRefs::instance();
        e->CallVoidMethod(r.listener(), r.onPresenceChanged(), (jlong)userId, (jboolean)online);
    }
    void onFriendAdded(const BuddyInfo& buddy) override {
        JNIEnv* e = env();
        if (!e) return;
        const auto& r = colbt::jni::JavaRefs::instance();
        e->CallVoidMethod(r.listener(), r.onFriendAdded(), toJavaBuddy(e, buddy));
    }
    void onGroupCreated(const GroupInfo& group) override {
        JNIEnv* e = env();
        if (!e) return;
        const auto& r = colbt::jni::JavaRefs::instance();
        e->CallVoidMethod(r.listener(), r.onGroupCreated(), toJavaGroup(e, group));
    }
    void onGroupMembersLoaded(int64_t groupId, const std::vector<MemberInfo>& members) override {
        JNIEnv* e = env();
        if (!e) return;
        const auto& r = colbt::jni::JavaRefs::instance();
        std::vector<jobject> ml;
        for (const auto& m : members) ml.push_back(toJavaMember(e, m));
        e->CallVoidMethod(r.listener(), r.onGroupMembersLoaded(), (jlong)groupId,
                          toJavaList(e, ml));
    }
    void onError(int code, const std::string& msg) override {
        JNIEnv* e = env();
        if (!e) return;
        const auto& r = colbt::jni::JavaRefs::instance();
        e->CallVoidMethod(r.listener(), r.onError(), (jint)code, toJavaString(e, msg));
    }
    void onProfileUpdated(int code, const std::string& msg, const UserInfo& me) override {
        JNIEnv* e = env();
        if (!e) return;
        const auto& r = colbt::jni::JavaRefs::instance();
        e->CallVoidMethod(r.listener(), r.onProfileUpdated(), (jint)code, toJavaString(e, msg),
                          toJavaBuddyUser(e, me));
    }
    void onProfileChanged(int64_t userId, const std::string& nickname,
                          const std::string& avatar) override {
        JNIEnv* e = env();
        if (!e) return;
        const auto& r = colbt::jni::JavaRefs::instance();
        e->CallVoidMethod(r.listener(), r.onProfileChanged(), (jlong)userId,
                          toJavaString(e, nickname), toJavaString(e, avatar));
    }
};

struct NativeClient {
    ClientCore core;
    JniListener listener;
};

NativeClient* fromHandle(jlong h) {
    return reinterpret_cast<NativeClient*>(h);
}

} // namespace

// ================= 注册 JNI 初始化 =================
extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
    colbt::jni::JavaRefs::instance().setJvm(vm);
    return JNI_VERSION_1_6;
}

#define CLS "com/colbt/im/core/NativeImClient"

extern "C" JNIEXPORT jlong JNICALL Java_com_colbt_im_core_NativeImClient_nativeCreate(
    JNIEnv* env, jclass, jobject listener) {
    colbt::jni::JavaRefs::instance().init(env, listener);
    return reinterpret_cast<jlong>(new NativeClient());
}

extern "C" JNIEXPORT void JNICALL Java_com_colbt_im_core_NativeImClient_nativeDestroy(
    JNIEnv*, jclass, jlong handle) {
    delete fromHandle(handle);
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_colbt_im_core_NativeImClient_nativeStart(
    JNIEnv* env, jclass, jlong handle, jstring host, jint port) {
    NativeClient* nc = fromHandle(handle);
    const char* h = env->GetStringUTFChars(host, nullptr);
    bool ok = nc->core.start(h, static_cast<uint16_t>(port), &nc->listener);
    env->ReleaseStringUTFChars(host, h);
    return ok;
}

extern "C" JNIEXPORT void JNICALL Java_com_colbt_im_core_NativeImClient_nativeStop(JNIEnv*,
                                                                                  jclass,
                                                                                  jlong handle) {
    fromHandle(handle)->core.stop();
}

extern "C" JNIEXPORT void JNICALL Java_com_colbt_im_core_NativeImClient_nativeLogin(
    JNIEnv* env, jclass, jlong handle, jstring user, jstring pass) {
    NativeClient* nc = fromHandle(handle);
    const char* u = env->GetStringUTFChars(user, nullptr);
    const char* p = env->GetStringUTFChars(pass, nullptr);
    nc->core.login(u, p);
    env->ReleaseStringUTFChars(user, u);
    env->ReleaseStringUTFChars(pass, p);
}

extern "C" JNIEXPORT void JNICALL Java_com_colbt_im_core_NativeImClient_nativeRegister(
    JNIEnv* env, jclass, jlong handle, jstring user, jstring pass, jstring nick) {
    NativeClient* nc = fromHandle(handle);
    const char* u = env->GetStringUTFChars(user, nullptr);
    const char* p = env->GetStringUTFChars(pass, nullptr);
    const char* n = env->GetStringUTFChars(nick, nullptr);
    nc->core.registerUser(u, p, n);
    env->ReleaseStringUTFChars(user, u);
    env->ReleaseStringUTFChars(pass, p);
    env->ReleaseStringUTFChars(nick, n);
}

#define NATIVE_CALL(JNI_NAME, CORE_NAME)                                              \
    extern "C" JNIEXPORT void JNICALL Java_com_colbt_im_core_NativeImClient_##JNI_NAME( \
        JNIEnv*, jclass, jlong handle) {                                                \
        fromHandle(handle)->core.CORE_NAME();                                           \
    }

NATIVE_CALL(nativeLogout, logout)
NATIVE_CALL(nativeLoadContacts, loadContacts)
NATIVE_CALL(nativeLoadSessions, loadSessions)

extern "C" JNIEXPORT void JNICALL Java_com_colbt_im_core_NativeImClient_nativeLoadHistory(
    JNIEnv*, jclass, jlong handle, jlong targetId, jint targetType, jint limit) {
    fromHandle(handle)->core.loadHistory(targetId, targetType, limit);
}

extern "C" JNIEXPORT void JNICALL Java_com_colbt_im_core_NativeImClient_nativeSendText(
    JNIEnv* env, jclass, jlong handle, jlong targetId, jint targetType, jstring text) {
    NativeClient* nc = fromHandle(handle);
    const char* t = env->GetStringUTFChars(text, nullptr);
    nc->core.sendText(targetId, targetType, t);
    env->ReleaseStringUTFChars(text, t);
}

extern "C" JNIEXPORT void JNICALL Java_com_colbt_im_core_NativeImClient_nativeSendReply(
    JNIEnv* env, jclass, jlong handle, jlong targetId, jint targetType, jlong replyToId,
    jstring text) {
    NativeClient* nc = fromHandle(handle);
    const char* t = env->GetStringUTFChars(text, nullptr);
    nc->core.sendReply(targetId, targetType, replyToId, t);
    env->ReleaseStringUTFChars(text, t);
}

extern "C" JNIEXPORT void JNICALL Java_com_colbt_im_core_NativeImClient_nativeMarkRead(
    JNIEnv*, jclass, jlong handle, jlong peerId, jint targetType) {
    fromHandle(handle)->core.markRead(peerId, targetType);
}

extern "C" JNIEXPORT void JNICALL Java_com_colbt_im_core_NativeImClient_nativeRecallMessage(
    JNIEnv*, jclass, jlong handle, jlong msgId, jlong targetId, jint targetType) {
    fromHandle(handle)->core.recallMessage(msgId, targetId, targetType);
}

extern "C" JNIEXPORT void JNICALL Java_com_colbt_im_core_NativeImClient_nativeDeleteFriend(
    JNIEnv*, jclass, jlong handle, jlong friendId) {
    fromHandle(handle)->core.deleteFriend(friendId);
}

extern "C" JNIEXPORT void JNICALL Java_com_colbt_im_core_NativeImClient_nativeAddFriend(
    JNIEnv* env, jclass, jlong handle, jstring username) {
    NativeClient* nc = fromHandle(handle);
    const char* u = env->GetStringUTFChars(username, nullptr);
    nc->core.addFriend(u, "");
    env->ReleaseStringUTFChars(username, u);
}

extern "C" JNIEXPORT void JNICALL Java_com_colbt_im_core_NativeImClient_nativeKickMember(
    JNIEnv*, jclass, jlong handle, jlong groupId, jlong memberId) {
    fromHandle(handle)->core.kickMember(groupId, memberId);
}

extern "C" JNIEXPORT void JNICALL Java_com_colbt_im_core_NativeImClient_nativeLeaveGroup(
    JNIEnv*, jclass, jlong handle, jlong groupId) {
    fromHandle(handle)->core.leaveGroup(groupId);
}

extern "C" JNIEXPORT void JNICALL Java_com_colbt_im_core_NativeImClient_nativeDismissGroup(
    JNIEnv*, jclass, jlong handle, jlong groupId) {
    fromHandle(handle)->core.dismissGroup(groupId);
}

extern "C" JNIEXPORT void JNICALL Java_com_colbt_im_core_NativeImClient_nativeRenameGroup(
    JNIEnv* env, jclass, jlong handle, jlong groupId, jstring name) {
    NativeClient* nc = fromHandle(handle);
    const char* n = env->GetStringUTFChars(name, nullptr);
    nc->core.renameGroup(groupId, n);
    env->ReleaseStringUTFChars(name, n);
}

extern "C" JNIEXPORT void JNICALL Java_com_colbt_im_core_NativeImClient_nativeSearchMessages(
    JNIEnv* env, jclass, jlong handle, jstring keyword) {
    NativeClient* nc = fromHandle(handle);
    const char* k = env->GetStringUTFChars(keyword, nullptr);
    nc->core.searchMessages(k, 50);
    env->ReleaseStringUTFChars(keyword, k);
}

extern "C" JNIEXPORT void JNICALL Java_com_colbt_im_core_NativeImClient_nativeSendTyping(
    JNIEnv*, jclass, jlong handle, jlong targetId, jint targetType) {
    fromHandle(handle)->core.sendTyping(targetId, targetType);
}

extern "C" JNIEXPORT void JNICALL Java_com_colbt_im_core_NativeImClient_nativeSendFileMessage(
    JNIEnv* env, jclass, jlong handle, jlong targetId, jint targetType, jint msgType,
    jstring path) {
    NativeClient* nc = fromHandle(handle);
    const char* p = env->GetStringUTFChars(path, nullptr);
    nc->core.sendFileMessage(targetId, targetType, msgType, p);
    env->ReleaseStringUTFChars(path, p);
}

extern "C" JNIEXPORT void JNICALL Java_com_colbt_im_core_NativeImClient_nativeDownloadFile(
    JNIEnv* env, jclass, jlong handle, jstring fileId) {
    NativeClient* nc = fromHandle(handle);
    const char* f = env->GetStringUTFChars(fileId, nullptr);
    nc->core.downloadFile(f);
    env->ReleaseStringUTFChars(fileId, f);
}

extern "C" JNIEXPORT void JNICALL Java_com_colbt_im_core_NativeImClient_nativeUpdateProfile(
    JNIEnv* env, jclass, jlong handle, jstring nickname, jstring avatarPath, jstring oldPass,
    jstring newPass) {
    NativeClient* nc = fromHandle(handle);
    const char* n = env->GetStringUTFChars(nickname, nullptr);
    const char* a = env->GetStringUTFChars(avatarPath, nullptr);
    const char* o = env->GetStringUTFChars(oldPass, nullptr);
    const char* np = env->GetStringUTFChars(newPass, nullptr);
    if (std::string(a).empty())
        nc->core.updateProfile(n, "", o, np);
    else
        nc->core.updateProfileWithAvatarUpload(a, n, o, np);
    env->ReleaseStringUTFChars(nickname, n);
    env->ReleaseStringUTFChars(avatarPath, a);
    env->ReleaseStringUTFChars(oldPass, o);
    env->ReleaseStringUTFChars(newPass, np);
}
