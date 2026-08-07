import SwiftUI
import PhotosUI
import UniformTypeIdentifiers

struct ChatView: View {
    @ObservedObject var core: IMCore
    let session: ImSession

    @State private var input = ""
    @State private var replyTarget: ImMessage?
    @State private var imageItem: PhotosPickerItem?
    @State private var showDocPicker = false
    @State private var showGroupInfo = false

    private var targetType: Int { Int(session.targetType) }

    var body: some View {
        VStack(spacing: 0) {
            ScrollViewReader { proxy in
                List(core.messages, id: \.id) { message in
                    MessageRow(core: core, message: message)
                        .listRowSeparator(.hidden)
                        .listRowBackground(Color.clear)
                        .id(message.id)
                        .contextMenu {
                            Button {
                                replyTarget = message
                            } label: {
                                Label("回复", systemImage: "arrowshape.turn.up.left")
                            }
                            if message.isMine {
                                Button(role: .destructive) {
                                    core.recall(messageId: message.id,
                                                targetId: session.targetId,
                                                targetType: targetType)
                                } label: {
                                    Label("撤回", systemImage: "arrow.uturn.backward")
                                }
                            }
                        }
                }
                .listStyle(.plain)
                .discordList(DTheme.bg3)
                .onAppear {
                    core.openSession(targetId: session.targetId, targetType: targetType)
                }
                .onChange(of: core.messages.count) { _ in
                    if let last = core.messages.last {
                        withAnimation { proxy.scrollTo(last.id, anchor: .bottom) }
                    }
                }
            }

            if core.typing {
                HStack(spacing: 6) {
                    Text("对方正在输入…")
                        .font(.system(size: 12))
                        .foregroundColor(DTheme.textMuted)
                    Spacer()
                }
                .padding(.horizontal, 16)
                .padding(.top, 6)
            }

            if let reply = replyTarget {
                HStack(spacing: 8) {
                    Rectangle()
                        .fill(DTheme.accent)
                        .frame(width: 3, height: 34)
                        .cornerRadius(2)
                    VStack(alignment: .leading, spacing: 2) {
                        Text("回复 \(reply.isMine ? "自己" : reply.senderName)")
                            .font(.system(size: 12, weight: .semibold))
                            .foregroundColor(DTheme.accent)
                        Text(reply.content)
                            .font(.system(size: 12))
                            .foregroundColor(DTheme.textMuted)
                            .lineLimit(1)
                    }
                    Spacer()
                    Button {
                        replyTarget = nil
                    } label: {
                        Image(systemName: "xmark.circle.fill")
                            .foregroundColor(DTheme.textMuted)
                    }
                }
                .padding(.horizontal, 12)
                .padding(.vertical, 6)
                .background(DTheme.bg2)
            }

            HStack(spacing: 8) {
                PhotosPicker(selection: $imageItem, matching: .images) {
                    Image(systemName: "photo")
                        .font(.system(size: 18))
                        .foregroundColor(DTheme.textSub)
                }
                Button {
                    showDocPicker = true
                } label: {
                    Image(systemName: "paperclip")
                        .font(.system(size: 18))
                        .foregroundColor(DTheme.textSub)
                }
                TextField("发消息给 \(session.title)", text: $input, axis: .vertical)
                    .font(.system(size: 15))
                    .foregroundColor(DTheme.textMain)
                    .lineLimit(1...5)
                    .padding(.horizontal, 14)
                    .padding(.vertical, 9)
                    .background(
                        RoundedRectangle(cornerRadius: 8)
                            .fill(DTheme.bg4)
                    )
                    .onChange(of: input) { _ in
                        core.sendTyping(targetId: session.targetId, targetType: targetType)
                    }
                Button {
                    send()
                } label: {
                    Image(systemName: "arrow.up")
                        .font(.system(size: 14, weight: .bold))
                        .foregroundColor(.white)
                        .frame(width: 34, height: 34)
                        .background(
                            Circle().fill(input.trimmingCharacters(in: .whitespaces).isEmpty
                                          ? DTheme.bg4
                                          : DTheme.accent)
                        )
                }
                .disabled(input.trimmingCharacters(in: .whitespaces).isEmpty)
            }
            .padding(.horizontal, 12)
            .padding(.vertical, 10)
            .background(DTheme.bg3)
        }
        .navigationTitle(session.title)
        .navigationBarTitleDisplayMode(.inline)
        .toolbarBackground(DTheme.bg2, for: .navigationBar)
        .toolbarBackground(.visible, for: .navigationBar)
        .toolbar {
            if session.targetType == 1 {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button {
                        core.loadGroupMembers(groupId: session.targetId)
                        showGroupInfo = true
                    } label: {
                        Image(systemName: "info.circle")
                            .foregroundColor(DTheme.textSub)
                    }
                }
            }
        }
        .onDisappear {
            core.closeSession()
        }
        .onChange(of: imageItem) { item in
            guard let item else { return }
            Task {
                if let data = try? await item.loadTransferable(type: Data.self) {
                    let url = FileManager.default.temporaryDirectory
                        .appendingPathComponent("colbt_send_\(Date().timeIntervalSince1970).img")
                    do {
                        try data.write(to: url)
                        core.sendImage(targetId: session.targetId,
                                       targetType: targetType,
                                       path: url.path)
                    } catch {}
                }
                imageItem = nil
            }
        }
        .fileImporter(isPresented: $showDocPicker, allowedContentTypes: [.item]) { result in
            switch result {
            case .success(let url):
                let secured = url.startAccessingSecurityScopedResource()
                core.sendFile(targetId: session.targetId,
                              targetType: targetType,
                              path: url.path)
                if secured { url.stopAccessingSecurityScopedResource() }
            case .failure:
                break
            }
        }
        .sheet(isPresented: $showGroupInfo) {
            GroupInfoView(core: core, groupId: session.targetId, groupTitle: session.title)
        }
    }

    private func send() {
        let text = input.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !text.isEmpty else { return }
        if let reply = replyTarget {
            core.sendReply(targetId: session.targetId,
                           targetType: targetType,
                           replyToId: reply.id,
                           text: text)
        } else {
            core.sendText(targetId: session.targetId, targetType: targetType, text: text)
        }
        input = ""
        replyTarget = nil
    }
}
