import SwiftUI

struct LoginView: View {
    @ObservedObject var core: IMCore

    @State private var host = "127.0.0.1"
    @State private var port = "9000"
    @State private var username = ""
    @State private var password = ""
    @State private var nickname = ""
    @State private var registerMode = false

    var body: some View {
        NavigationStack {
            Form {
                Section("服务器") {
                    TextField("IP / 域名", text: $host)
                        .autocapitalization(.none)
                        .keyboardType(.numbersAndPunctuation)
                    TextField("端口", text: $port)
                        .keyboardType(.numberPad)
                }

                Section(registerMode ? "注册" : "登录") {
                    TextField("用户名", text: $username)
                        .autocapitalization(.none)
                        .autocorrectionDisabled()
                    SecureField("密码", text: $password)
                    if registerMode {
                        TextField("昵称", text: $nickname)
                    }
                }

                Section {
                    Button(registerMode ? "注 册" : "登 录") {
                        core.start(host: host, port: Int(port) ?? 9000)
                        let user = username.trimmingCharacters(in: .whitespaces)
                        if registerMode {
                            let nick = nickname.isEmpty ? "你" : nickname
                            core.register(username: user, password: password, nickname: nick)
                        } else {
                            core.login(username: user, password: password)
                        }
                    }
                    .disabled(username.isEmpty || password.isEmpty)
                    .frame(maxWidth: .infinity)

                    Button(registerMode ? "已有账号？去登录" : "没有账号？立即注册") {
                        registerMode.toggle()
                    }
                    .font(.footnote)
                    .frame(maxWidth: .infinity)
                }
                .listRowBackground(Color.clear)
                .listRowSeparator(.hidden)

                if !core.status.isEmpty {
                    Section {
                        Text(core.status)
                            .font(.footnote)
                            .foregroundColor(core.connected || core.status.contains("成功") ? .secondary : .red)
                    }
                }
            }
            .navigationTitle("COLBT")
        }
    }
}
