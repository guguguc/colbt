#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface ImUser : NSObject
@property(nonatomic, assign) int64_t id;
@property(nonatomic, copy) NSString *username;
@property(nonatomic, copy) NSString *nickname;
@property(nonatomic, copy) NSString *avatar;
@property(nonatomic, assign) int online;
@property(nonatomic, readonly) NSString *displayName;
- (instancetype)initWithId:(int64_t)userId
                  username:(NSString *)username
                  nickname:(NSString *)nickname
                    avatar:(NSString *)avatar
                    online:(int)online;
@end

@interface ImBuddy : NSObject
@property(nonatomic, strong) ImUser *user;
@property(nonatomic, copy) NSString *remark;
@property(nonatomic, readonly) NSString *displayName;
@property(nonatomic, readonly) BOOL isOnline;
- (instancetype)initWithUser:(ImUser *)user remark:(NSString *)remark;
@end

@interface ImMember : NSObject
@property(nonatomic, strong) ImUser *user;
@property(nonatomic, copy) NSString *groupNick;
@property(nonatomic, readonly) NSString *displayName;
- (instancetype)initWithUser:(ImUser *)user groupNick:(NSString *)groupNick;
@end

@interface ImGroup : NSObject
@property(nonatomic, assign) int64_t id;
@property(nonatomic, copy) NSString *name;
@property(nonatomic, assign) int64_t ownerId;
@property(nonatomic, copy) NSArray<ImMember *> *members;
- (instancetype)initWithId:(int64_t)groupId
                      name:(NSString *)name
                   ownerId:(int64_t)ownerId
                   members:(NSArray<ImMember *> *)members;
@end

@interface ImMessage : NSObject
@property(nonatomic, assign) int64_t id;
@property(nonatomic, assign) int64_t fromId;
@property(nonatomic, assign) int64_t targetId;
@property(nonatomic, assign) int targetType;
@property(nonatomic, assign) int msgType;
@property(nonatomic, copy) NSString *content;
@property(nonatomic, assign) int64_t timestamp;
@property(nonatomic, assign) int direction;
@property(nonatomic, copy) NSString *senderName;
@property(nonatomic, assign) int read;
@property(nonatomic, assign) int64_t replyToId;
@property(nonatomic, copy) NSString *replyContent;
@property(nonatomic, readonly) BOOL isMine;
@property(nonatomic, readonly, nullable) NSString *fileId;
- (instancetype)initWithId:(int64_t)messageId
                    fromId:(int64_t)fromId
                  targetId:(int64_t)targetId
                targetType:(int)targetType
                   msgType:(int)msgType
                   content:(NSString *)content
                 timestamp:(int64_t)timestamp
                 direction:(int)direction
                senderName:(NSString *)senderName
                      read:(int)read
                replyToId:(int64_t)replyToId
              replyContent:(NSString *)replyContent;
@end

@interface ImSession : NSObject
@property(nonatomic, assign) int64_t targetId;
@property(nonatomic, assign) int targetType;
@property(nonatomic, copy) NSString *title;
@property(nonatomic, copy) NSString *avatar;
@property(nonatomic, copy) NSString *lastContent;
@property(nonatomic, assign) int64_t lastTime;
@property(nonatomic, assign) int unread;
- (instancetype)initWithTargetId:(int64_t)targetId
                      targetType:(int)targetType
                           title:(NSString *)title
                          avatar:(NSString *)avatar
                     lastContent:(NSString *)lastContent
                        lastTime:(int64_t)lastTime
                          unread:(int)unread;
@end

NS_ASSUME_NONNULL_END
