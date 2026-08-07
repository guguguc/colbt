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
            fileBubble
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

    // MARK: 文件气泡（下载 + 保存到文件）

    @ViewBuilder
    private var fileBubble: some View {
        let fileId = message.fileId ?? ""
        let downloaded = fileId.isEmpty ? nil : core.downloadedFiles[fileId]

        HStack(spacing: 10) {
            Image(systemName: "doc.fill")
                .font(.system(size: 20))
                .foregroundColor(message.isMine ? .white : DTheme.accent)

            VStack(alignment: .leading, spacing: 3) {
                Text(displayFileName)
                    .font(.system(size: 13, weight: .medium))
                    .foregroundColor(.white)
                    .lineLimit(1)
                if let d = downloaded {
                    Text("\(byteString(d.size)) · 已下载")
                        .font(.system(size: 10))
                        .foregroundColor(.white.opacity(0.8))
                } else {
                    Text("点按下载")
                        .font(.system(size: 10))
                        .foregroundColor(.white.opacity(0.7))
                }
            }

            Spacer(minLength: 6)

            if let d = downloaded {
                if let url = core.tempFileURL(for: d) {
                    ShareLink(item: url) {
                        Image(systemName: "square.and.arrow.down")
                            .font(.system(size: 18))
                            .foregroundColor(message.isMine ? .white : DTheme.accent)
                    }
                } else {
                    Image(systemName: "checkmark.circle")
                        .foregroundColor(.white.opacity(0.8))
                }
            } else if !fileId.isEmpty {
                Button {
                    core.downloadFile(fileId: fileId)
                } label: {
                    Image(systemName: "arrow.down.circle")
                        .font(.system(size: 20))
                        .foregroundColor(message.isMine ? .white : DTheme.accent)
                }
            }
        }
        .frame(minWidth: 170)
    }

    private func byteString(_ size: Int64) -> String {
        if size >= 1024 * 1024 { return String(format: "%.1f MB", Double(size) / 1048576.0) }
        if size >= 1024 { return String(format: "%.1f KB", Double(size) / 1024.0) }
        return "\(size) B"
    }

    private func timeString(_ ts: Int64) -> String {
        let formatter = DateFormatter()
        formatter.dateFormat = "HH:mm"
        return formatter.string(from: Date(timeIntervalSince1970: TimeInterval(ts)))
    }
}
