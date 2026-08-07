#import <Foundation/Foundation.h>
#import "IMModels.h"

NS_ASSUME_NONNULL_BEGIN

@protocol ImClientListener <NSObject>
- (void)onConnectionChanged:(BOOL)connected;
- (void)onLoginResult:(int)code message:(NSString *)message me:(ImUser *)me;
- (void)onRegisterResult:(int)code message:(NSString *)message;
- (void)onContactsLoaded:(NSArray<ImBuddy *> *)buddies groups:(NSArray<ImGroup *> *)groups;
- (void)onSessionsLoaded:(NSArray<ImSession *> *)sessions;
- (void)onHistoryLoaded:(int64_t)targetId targetType:(int)targetType messages:(NSArray<ImMessage *> *)messages;
- (void)onMessage:(ImMessage *)message;
- (void)onMessageSent:(ImMessage *)message;
- (void)onMessageRecalled:(int64_t)messageId targetId:(int64_t)targetId targetType:(int)targetType;
- (void)onFriendDeleted:(int64_t)friendId name:(NSString *)name;
- (void)onGroupUpdated:(int64_t)groupId;
- (void)onSearchResults:(NSArray<ImMessage *> *)messages;
- (void)onTyping:(int64_t)fromId targetId:(int64_t)targetId targetType:(int)targetType;
- (void)onMessagesRead:(int64_t)peerId targetType:(int)targetType;
- (void)onReadReceipt:(int64_t)peerId targetType:(int)targetType;
- (void)onFileDownloaded:(NSString *)fileId name:(NSString *)name size:(int64_t)size mime:(NSString *)mime data:(NSData *)data;
- (void)onPresenceChanged:(int64_t)userId online:(BOOL)online;
- (void)onFriendAdded:(ImBuddy *)buddy;
- (void)onGroupCreated:(ImGroup *)group;
- (void)onGroupMembersLoaded:(int64_t)groupId members:(NSArray<ImMember *> *)members;
- (void)onError:(int)code message:(NSString *)message;
@end

@interface ImClientBridge : NSObject
- (instancetype)initWithListener:(id<ImClientListener>)listener;
- (BOOL)start:(NSString *)host port:(int)port;
- (void)stop;

- (void)login:(NSString *)username password:(NSString *)password;
- (void)registerUser:(NSString *)username password:(NSString *)password nickname:(NSString *)nickname;
- (void)logout;
- (void)loadContacts;
- (void)loadSessions;
- (void)loadHistory:(int64_t)targetId targetType:(int)targetType limit:(int)limit;
- (void)sendText:(int64_t)targetId targetType:(int)targetType text:(NSString *)text;
- (void)sendReply:(int64_t)targetId targetType:(int)targetType replyToId:(int64_t)replyToId text:(NSString *)text;
- (void)markRead:(int64_t)peerId targetType:(int)targetType;
- (void)recallMessage:(int64_t)messageId targetId:(int64_t)targetId targetType:(int)targetType;
- (void)deleteFriend:(int64_t)friendId;
- (void)addFriend:(NSString *)username;
- (void)kickMember:(int64_t)groupId memberId:(int64_t)memberId;
- (void)leaveGroup:(int64_t)groupId;
- (void)dismissGroup:(int64_t)groupId;
- (void)renameGroup:(int64_t)groupId name:(NSString *)name;
- (void)searchMessages:(NSString *)keyword;
- (void)sendTyping:(int64_t)targetId targetType:(int)targetType;
- (void)sendImage:(int64_t)targetId targetType:(int)targetType path:(NSString *)path;
- (void)sendFile:(int64_t)targetId targetType:(int)targetType path:(NSString *)path;
- (void)downloadFile:(NSString *)fileId;
- (void)createGroup:(NSString *)name memberIds:(NSArray<NSNumber *> *)memberIds;
- (void)loadGroupMembers:(int64_t)groupId;
@end

NS_ASSUME_NONNULL_END
