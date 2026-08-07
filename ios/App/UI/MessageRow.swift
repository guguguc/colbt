import SwiftUI
import UIKit

struct MessageRow: View {
    @ObservedObject var core: IMCore
    let message: ImMessage

    private var isGroupIncoming: Bool {
        message.targetType == 1 && !message.isMine
    }

    var body: some View {
        HStack(alignment: .top, spacing: 12) {
            if message.isMine { Spacer(minLength: 56) }

            if isGroupIncoming {
                DiscordAvatar(name: message.senderName, size: 40)
            }

            VStack(alignment: message.isMine ? .trailing : .leading, spacing: 4) {
                if isGroupIncoming {
                    Text(message.senderName)
                        .font(.system(size: 13, weight: .semibold))
                        .foregroundColor(DTheme.accent)
                }
                bubble
            }

            if !message.isMine { Spacer(minLength: 56) }
        }
        .frame(maxWidth: .infinity, alignment: message.isMine ? .trailing : .leading)
        .padding(.vertical, 3)
    }

    @ViewBuilder
    private var bubble: some View {
        VStack(alignment: .trailing, spacing: 3) {
            if message.replyToId != 0 && !message.replyContent.isEmpty {
                HStack(spacing: 4) {
                    Rectangle()
                        .fill(message.isMine ? Color.white.opacity(0.4) : DTheme.accent)
                        .frame(width: 3)
                        .cornerRadius(2)
                    Text(message.replyContent)
                        .font(.system(size: 11))
                        .foregroundColor(message.isMine ? .white.opacity(0.85) : DTheme.textMuted)
                        .lineLimit(1)
                }
                .padding(4)
                .background(
                    RoundedRectangle(cornerRadius: 4)
                        .fill(message.isMine ? Color.white.opacity(0.12) : DTheme.bg3)
                )
            }

            content
                .foregroundColor(.white)

            if message.isMine {
                HStack(spacing: 4) {
                    Text(message.read != 0 ? "已读" : "未读")
                        .font(.system(size: 10))
                        .foregroundColor(message.read != 0 ? .white.opacity(0.8) : .white.opacity(0.55))
                    Text(timeString(message.timestamp))
                        .font(.system(size: 10))
                        .foregroundColor(.white.opacity(0.6))
                }
            }
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 8)
        .background(
            RoundedRectangle(cornerRadius: 10)
                .fill(message.isMine ? DTheme.accent : DTheme.bg4)
        )
        .frame(maxWidth: 300, alignment: message.isMine ? .trailing : .leading)
    }

    @ViewBuilder
    private var content: some View {
        switch message.msgType {
        case 1:
            if let data = core.imageCache[message.fileId ?? ""], let image = UIImage(data: data) {
                Image(uiImage: image)
                    .resizable()
                    .scaledToFit()
                    .frame(maxWidth: 220, maxHeight: 260)
                    .clipShape(RoundedRectangle(cornerRadius: 8))
            } else {
                HStack(spacing: 8) {
                    ProgressView()
                    Text("图片加载中…")
                        .font(.system(size: 13))
                        .foregroundColor(.white.opacity(0.85))
                }
                .frame(minWidth: 100)
            }
        case 2:
            HStack(spacing: 8) {
                Image(systemName: "doc.fill")
                VStack(alignment: .leading, spacing: 2) {
                    Text(displayFileName)
                        .font(.system(size: 13))
                        .lineLimit(1)
                    Text("点击查看")
                        .font(.system(size: 10))
                        .opacity(0.75)
                }
            }
        case 3:
            Text(message.content)
                .font(.system(size: 13))
                .opacity(0.9)
        default:
            Text(message.content)
        }
    }

    private var displayFileName: String {
        let parts = message.content.components(separatedBy: "|")
        return parts.count > 1 ? parts[1] : message.content
    }

    private func timeString(_ ts: Int64) -> String {
        let formatter = DateFormatter()
        formatter.dateFormat = "HH:mm"
        return formatter.string(from: Date(timeIntervalSince1970: TimeInterval(ts)))
    }
}
