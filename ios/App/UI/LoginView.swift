import SwiftUI

struct LoginView: View {
    @ObservedObject var core: IMCore

    @State private var host = "192.168.1.4"
    @State private var port = "9000"
    @State private var username = ""
    @State private var password = ""
    @State private var nickname = ""
    @State private var registerMode = false

    var body: some View {
        ZStack {
            DTheme.bg1.ignoresSafeArea()

            ScrollView {
                VStack(spacing: 28) {
                    Spacer().frame(height: 40)

                    VStack(spacing: 6) {
                        ZStack {
                            RoundedRectangle(cornerRadius: 16)
                                .fill(DTheme.accent)
                                .frame(width: 72, height: 72)
                            Text("C")
                                .font(.system(size: 40, weight: .bold))
                                .foregroundColor(.white)
                        }
                        Text("COLBT")
                            .font(.system(size: 30, weight: .bold))
                            .foregroundColor(DTheme.textMain)
                            .tracking(5)
                        Text("简单、高效的即时通讯")
                            .font(.system(size: 13))
                            .foregroundColor(DTheme.textMuted)
                    }

                    VStack(spacing: 10) {
                        HStack(spacing: 8) {
                            DiscordTextField(placeholder: "服务器 IP", text: $host)
                            DiscordTextField(placeholder: "端口", text: $port)
                                .frame(width: 96)
                        }

                        DiscordTextField(placeholder: registerMode ? "用户名（唯一）" : "用户名",
                                         text: $username)
                        DiscordTextField(placeholder: "密码", text: $password, isSecure: true)
                        if registerMode {
                            DiscordTextField(placeholder: "昵称", text: $nickname)
                        }

                        if !core.status.isEmpty {
                            Text(core.status)
                                .font(.system(size: 13))
                                .foregroundColor(DTheme.danger)
                                .frame(maxWidth: .infinity, alignment: .leading)
                                .padding(.top, 2)
                        }

                        DiscordButton(title: registerMode ? "注 册" : "登 录",
                                      disabled: username.isEmpty || password.isEmpty) {
                            core.start(host: host, port: Int(port) ?? 9000)
                            let user = username.trimmingCharacters(in: .whitespaces)
                            if registerMode {
                                let nick = nickname.isEmpty ? "你" : nickname
                                core.register(username: user, password: password, nickname: nick)
                            } else {
                                core.login(username: user, password: password)
                            }
                        }
                        .padding(.top, 8)

                        Button {
                            registerMode.toggle()
                        } label: {
                            Text(registerMode ? "已有账号？去登录" : "没有账号？立即注册")
                                .font(.system(size: 13))
                                .foregroundColor(DTheme.textSub)
                                .underline()
                        }
                        .padding(.top, 2)
                    }
                    .padding(.horizontal, 24)

                    Spacer().frame(height: 20)
                }
                .frame(maxWidth: 420)
                .frame(maxWidth: .infinity)
            }
            .scrollDismissesKeyboard(.interactively)
        }
    }
}
