import SwiftUI
import UIKit

struct MessageRow: View {
    @ObservedObject var core: IMCore
    let message: ImMessage

    private var isGroupIncoming: Bool {
        message.targetType == 1 && !message.isMine
    }

    var body: some View {
        HStack(alignment: .bottom, spacing: 8) {
            if message.isMine { Spacer(minLength: 48) }

            if isGroupIncoming {
                VStack(alignment: .leading, spacing: 2) {
                    Text(message.senderName)
                        .font(.caption2)
                        .foregroundColor(.secondary)
                    bubble
                }
            } else {
                bubble
            }

            if !message.isMine { Spacer(minLength: 48) }
        }
        .frame(maxWidth: .infinity, alignment: message.isMine ? .trailing : .leading)
        .padding(.vertical, 2)
    }

    @ViewBuilder
    private var bubble: some View {
        VStack(alignment: .trailing, spacing: 3) {
            if message.replyToId != 0 && !message.replyContent.isEmpty {
                Text(message.replyContent)
                    .font(.caption2)
                    .foregroundColor(.secondary)
                    .padding(4)
                    .background(
                        RoundedRectangle(cornerRadius: 4)
                            .fill(message.isMine ? Color.white.opacity(0.15) : Color(.systemGray5))
                    )
            }

            content

            if message.isMine {
                HStack(spacing: 3) {
                    Text(message.read != 0 ? "已读" : "未读")
                        .font(.caption2)
                        .foregroundColor(message.read != 0 ? .green : .secondary)
                    Text(timeString(message.timestamp))
                        .font(.caption2)
                        .foregroundColor(.secondary)
                }
            } else if message.msgType == 0 {
                Text(timeString(message.timestamp))
                    .font(.caption2)
                    .foregroundColor(.secondary)
            }
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 8)
        .background(
            RoundedRectangle(cornerRadius: 14)
                .fill(message.isMine ? Color.blue : Color(.secondarySystemBackground))
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
                HStack {
                    ProgressView()
                    Text("图片")
                        .font(.caption)
                        .foregroundColor(.secondary)
                }
                .frame(minWidth: 80)
            }
        case 2:
            HStack(spacing: 6) {
                Image(systemName: "doc.fill")
                Text(displayFileName)
                    .font(.caption)
                    .lineLimit(1)
            }
        case 3:
            Text(message.content)
                .font(.footnote)
                .foregroundColor(.secondary)
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
