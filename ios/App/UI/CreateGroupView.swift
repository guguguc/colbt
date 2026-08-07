import SwiftUI

struct CreateGroupView: View {
    @ObservedObject var core: IMCore
    @Environment(\.dismiss) private var dismiss

    @State private var name = ""
    @State private var selected: Set<Int64> = []

    var body: some View {
        NavigationStack {
            Form {
                Section("群名称") {
                    TextField("群聊名称", text: $name)
                }
                Section("选择成员 (\(selected.count))") {
                    if core.buddies.isEmpty {
                        Text("请先添加好友")
                            .font(.footnote)
                            .foregroundColor(.secondary)
                    }
                    ForEach(core.buddies, id: \.user.id) { buddy in
                        let isOn = Binding(
                            get: { selected.contains(buddy.user.id) },
                            set: { on in
                                if on { selected.insert(buddy.user.id) }
                                else { selected.remove(buddy.user.id) }
                            }
                        )
                        Toggle(buddy.displayName, isOn: isOn)
                    }
                }
            }
            .navigationTitle("创建群聊")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("取消") { dismiss() }
                }
                ToolbarItem(placement: .confirmationAction) {
                    Button("创建") {
                        let groupName = name.trimmingCharacters(in: .whitespaces)
                        guard !groupName.isEmpty else { return }
                        core.createGroup(name: groupName, memberIds: Array(selected))
                        dismiss()
                    }
                    .disabled(name.trimmingCharacters(in: .whitespaces).isEmpty || selected.isEmpty)
                }
            }
        }
    }
}
