#import "IMModels.h"

@implementation ImUser
- (instancetype)initWithId:(int64_t)userId
                  username:(NSString *)username
                  nickname:(NSString *)nickname
                    avatar:(NSString *)avatar
                    online:(int)online {
    self = [super init];
    if (self) {
        _id = userId;
        _username = username ?: @"";
        _nickname = nickname ?: @"";
        _avatar = avatar ?: @"";
        _online = online;
    }
    return self;
}
- (NSString *)displayName {
    return _nickname.length > 0 ? _nickname : _username;
}
@end

@implementation ImBuddy
- (instancetype)initWithUser:(ImUser *)user remark:(NSString *)remark {
    self = [super init];
    if (self) {
        _user = user;
        _remark = remark ?: @"";
    }
    return self;
}
- (NSString *)displayName {
    return _remark.length > 0 ? _remark : _user.displayName;
}
- (BOOL)isOnline { return _user.online == 1; }
@end

@implementation ImMember
- (instancetype)initWithUser:(ImUser *)user groupNick:(NSString *)groupNick {
    self = [super init];
    if (self) {
        _user = user;
        _groupNick = groupNick ?: @"";
    }
    return self;
}
- (NSString *)displayName {
    return _groupNick.length > 0 ? _groupNick : _user.displayName;
}
@end

@implementation ImGroup
- (instancetype)initWithId:(int64_t)groupId
                      name:(NSString *)name
                   ownerId:(int64_t)ownerId
                   members:(NSArray<ImMember *> *)members {
    self = [super init];
    if (self) {
        _id = groupId;
        _name = name ?: @"";
        _ownerId = ownerId;
        _members = members ?: @[];
    }
    return self;
}
@end

@implementation ImMessage
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
              replyContent:(NSString *)replyContent {
    self = [super init];
    if (self) {
        _id = messageId;
        _fromId = fromId;
        _targetId = targetId;
        _targetType = targetType;
        _msgType = msgType;
        _content = content ?: @"";
        _timestamp = timestamp;
        _direction = direction;
        _senderName = senderName ?: @"";
        _read = read;
        _replyToId = replyToId;
        _replyContent = replyContent ?: @"";
    }
    return self;
}
- (BOOL)isMine { return _direction == 0; }
- (NSString *)fileId {
    NSRange range = [_content rangeOfString:@"|"];
    if (range.location == NSNotFound) return nil;
    NSString *fid = [_content substringToIndex:range.location];
    return fid.length > 0 ? fid : nil;
}
@end

@implementation ImSession
- (instancetype)initWithTargetId:(int64_t)targetId
                      targetType:(int)targetType
                           title:(NSString *)title
                          avatar:(NSString *)avatar
                     lastContent:(NSString *)lastContent
                        lastTime:(int64_t)lastTime
                          unread:(int)unread {
    self = [super init];
    if (self) {
        _targetId = targetId;
        _targetType = targetType;
        _title = title ?: @"";
        _avatar = avatar ?: @"";
        _lastContent = lastContent ?: @"";
        _lastTime = lastTime;
        _unread = unread;
    }
    return self;
}
@end
