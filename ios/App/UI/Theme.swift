import SwiftUI
import UIKit

// MARK: - Discord 色板
enum DTheme {
    static let bg1 = Color(hex: 0x1E1F22)    // 最深的侧栏 / 列表底
    static let bg2 = Color(hex: 0x2B2D31)    // 列表项
    static let bg3 = Color(hex: 0x313338)    // 主聊天区
    static let bg4 = Color(hex: 0x383A40)    // 收到的气泡 / 输入框底
    static let bgHover = Color(hex: 0x35363C)
    static let accent = Color(hex: 0x5865F2) // Blurple
    static let accentHover = Color(hex: 0x4752C4)
    static let textMain = Color(hex: 0xF2F3F5)
    static let textSub = Color(hex: 0xB5BAC1)
    static let textMuted = Color(hex: 0x949BA4)
    static let danger = Color(hex: 0xED4245)
    static let online = Color(hex: 0x23A559)
    static let warn = Color(hex: 0xFAA61A)
}

extension Color {
    init(hex: UInt32) {
        self.init(.sRGB,
                  red: Double((hex >> 16) & 0xFF) / 255,
                  green: Double((hex >> 8) & 0xFF) / 255,
                  blue: Double(hex & 0xFF) / 255,
                  opacity: 1)
    }
}

// MARK: - 列表 / 页头样式

struct DiscordListStyle: ViewModifier {
    var background: Color = DTheme.bg3
    func body(content: Content) -> some View {
        content
            .scrollContentBackground(.hidden)
            .background(background)
    }
}

extension View {
    func discordList(_ bg: Color = DTheme.bg3) -> some View {
        modifier(DiscordListStyle(background: bg))
    }
}

extension Text {
    func discordHeader() -> some View {
        self
            .font(.system(size: 12, weight: .bold))
            .textCase(.uppercase)
            .foregroundColor(DTheme.textMuted)
    }
}

// MARK: - Discord 风格按钮

struct DiscordButton: View {
    enum Style { case primary, secondary, danger }

    let title: String
    var style: Style = .primary
    var disabled = false
    var action: () -> Void

    var body: some View {
        Button(action: action) {
            Text(title)
                .font(.system(size: 15, weight: .medium))
                .foregroundColor(fg)
                .padding(.vertical, 11)
                .padding(.horizontal, 16)
                .frame(maxWidth: .infinity)
                .background(
                    RoundedRectangle(cornerRadius: 8)
                        .fill(bg)
                )
        }
        .buttonStyle(.plain)
        .opacity(disabled ? 0.45 : 1)
        .disabled(disabled)
    }

    private var bg: Color {
        switch style {
        case .primary: return DTheme.accent
        case .secondary: return DTheme.bg4
        case .danger: return DTheme.danger
        }
    }

    private var fg: Color {
        switch style {
        case .primary: return .white
        case .secondary: return DTheme.textMain
        case .danger: return .white
        }
    }
}

// MARK: - Discord 风格输入框

struct DiscordTextField: View {
    var placeholder: String
    @Binding var text: String
    var isSecure = false

    var body: some View {
        Group {
            if isSecure {
                SecureField(placeholder, text: $text)
            } else {
                TextField(placeholder, text: $text)
            }
        }
        .font(.system(size: 15))
        .foregroundColor(DTheme.textMain)
        .textInputAutocapitalization(.never)
        .autocorrectionDisabled()
        .padding(.horizontal, 12)
        .padding(.vertical, 10)
        .background(
            RoundedRectangle(cornerRadius: 8)
                .fill(DTheme.bg4)
        )
    }
}

// MARK: - Discord 风格头像（按名字哈希取色）

struct DiscordAvatar: View {
    let name: String
    var size: CGFloat = 40
    var online: Bool?
    var uiImage: UIImage?

    private static let palette: [UInt32] = [
        0x5865F2, 0xEB459E, 0xFAA61A, 0x57F287,
        0x00B0F4, 0xED4245, 0x9B59B6, 0x1ABC9C,
    ]

    var body: some View {
        ZStack(alignment: .bottomTrailing) {
            if let uiImage {
                Image(uiImage: uiImage)
                    .resizable()
                    .scaledToFill()
                    .frame(width: size, height: size)
                    .clipShape(Circle())
            } else {
                Circle()
                    .fill(backgroundColor)
                    .frame(width: size, height: size)
                    .overlay(
                        Text(String(name.prefix(1)).uppercased())
                            .font(.system(size: size * 0.45, weight: .semibold))
                            .foregroundColor(.white)
                    )
            }
            if let online {
                Circle()
                    .fill(online ? DTheme.online : DTheme.textMuted)
                    .frame(width: size * 0.28, height: size * 0.28)
                    .overlay(Circle().stroke(DTheme.bg2, lineWidth: 2))
                    .offset(x: -1, y: -1)
            }
        }
    }

    private var backgroundColor: Color {
        var hash: UInt64 = 5381
        for b in name.utf8 { hash = hash &* 33 &+ UInt64(b) }
        let idx = Int(hash % UInt64(Self.palette.count))
        return Color(hex: Self.palette[idx])
    }
}
