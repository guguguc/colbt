package com.colbt.im.ui

import android.net.Uri
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.colbt.im.core.*
import java.io.File

// Discord 风格深色配色
private val Bg1 = Color(0xFF1E1F22)
private val Bg2 = Color(0xFF2B2D31)
private val Bg3 = Color(0xFF313338)
private val Accent = Color(0xFF5865F2)
private val TextMain = Color(0xFFF2F3F5)
private val TextSub = Color(0xFFB5BAC1)

class MainActivity : ComponentActivity() {

    private lateinit var core: ImCore
    private var sessions by mutableStateOf(emptyList<ImSession>())
    private var buddies by mutableStateOf(emptyList<ImBuddy>())
    private var groups by mutableStateOf(emptyList<ImGroup>())
    private var messages by mutableStateOf(emptyList<ImMessage>())
    private var activeSession by mutableStateOf<ImSession?>(null)
    private var loggedIn by mutableStateOf(false)
    private var status by mutableStateOf("")
    private var typing by mutableStateOf(false)
    private var me by mutableStateOf<ImBuddy?>(null)

    private val listener = object : ImListenerAdapter() {
        override fun onLoginResult(code: Int, msg: String, me: ImBuddy) {
            if (code == 0) {
                loggedIn = true
                this@MainActivity.me = me
                core.loadSessions()
                core.loadContacts()
            } else status = msg
        }
        override fun onRegisterResult(code: Int, msg: String) { status = msg }
        override fun onSessionsLoaded(list: List<ImSession>) { sessions = list }
        override fun onContactsLoaded(b: List<ImBuddy>, g: List<ImGroup>) {
            buddies = b; groups = g
        }
        override fun onHistoryLoaded(targetId: Long, targetType: Int, msgs: List<ImMessage>) {
            if (activeSession?.targetId == targetId && activeSession?.targetType == targetType)
                messages = msgs
        }
        override fun onMessage(msg: ImMessage) {
            val cur = activeSession
            if (cur != null && msg.targetType == cur.targetType && msg.targetId == cur.targetId) {
                messages = messages + msg
            } else {
                core.loadSessions()
            }
        }
        override fun onMessageSent(msg: ImMessage) {
            if (activeSession?.targetId == msg.targetId) messages = messages + msg
            core.loadSessions()
        }
        override fun onTyping(fromId: Long, targetId: Long, targetType: Int) {
            val cur = activeSession
            if (cur != null && cur.targetType == targetType &&
                (targetType == 1 && cur.targetId == targetId || targetType == 0 && cur.targetId == fromId)
            ) typing = true
        }
        override fun onMessageRecalled(msgId: Long, targetId: Long, targetType: Int) {
            if (activeSession?.targetId == targetId) messages = messages.filter { it.id != msgId }
        }
        override fun onError(code: Int, msg: String) { status = msg }
        override fun onConnectionChanged(connected: Boolean) {
            if (!connected && !loggedIn) status = "无法连接服务器"
        }
        override fun onFriendAdded(buddy: ImBuddy) {
            status = "添加好友成功: ${buddy.nickname.ifEmpty { buddy.username }}"
            core.loadContacts()
        }
        override fun onFriendDeleted(friendId: Long, name: String) {
            core.loadContacts()
        }
        override fun onGroupUpdated(groupId: Long) {
            core.loadContacts()
            core.loadSessions()
        }
        override fun onProfileUpdated(code: Int, msg: String, me: ImBuddy) {
            if (code == 0) {
                this@MainActivity.me = me
                status = "资料已更新"
            } else status = msg
        }
        override fun onProfileChanged(userId: Long, nickname: String, avatar: String) {
            core.loadContacts()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        core = ImCore(listener)
        setContent {
            var addFriendDialog by remember { mutableStateOf(false) }
            var profileDialog by remember { mutableStateOf(false) }
            MaterialTheme(
                colorScheme = darkColorScheme(
                    primary = Accent, background = Bg3, surface = Bg2, onBackground = TextMain
                )
            ) {
                if (!loggedIn) {
                    LoginScreen(core, status, onStatus = { status = it })
                } else {
                    MainScreen(
                        sessions = sessions, buddies = buddies, groups = groups,
                        messages = messages, active = activeSession, typing = typing,
                        onOpenSession = {
                            activeSession = it
                            messages = emptyList()
                            core.loadHistory(it.targetId, it.targetType, 50)
                            core.markRead(it.targetId, it.targetType)
                        },
                        onCloseSession = {
                            activeSession = null
                            messages = emptyList()
                        },
                        onSend = { text ->
                            val cur = activeSession
                            if (cur != null) core.sendText(cur.targetId, cur.targetType, text)
                        },
                        onTyping = {
                            val cur = activeSession
                            if (cur != null) core.sendTyping(cur.targetId, cur.targetType)
                        },
                        onAddFriend = { addFriendDialog = true },
                        onProfile = { profileDialog = true },
                        onLogout = {
                            core.logout(); loggedIn = false
                        }
                    )
                }
                if (addFriendDialog) {
                    var name by remember { mutableStateOf("") }
                    AlertDialog(
                        onDismissRequest = { addFriendDialog = false },
                        title = { Text("添加好友") },
                        text = {
                            OutlinedTextField(name, { name = it },
                                label = { Text("对方用户名") }, singleLine = true)
                        },
                        confirmButton = {
                            TextButton(onClick = {
                                if (name.isNotBlank()) core.addFriend(name.trim())
                                addFriendDialog = false
                            }) { Text("确定") }
                        },
                        dismissButton = {
                            TextButton(onClick = { addFriendDialog = false }) { Text("取消") }
                        }
                    )
                }
                if (profileDialog) {
                    ProfileDialog(core, me?.nickname ?: "", status,
                        onClose = { profileDialog = false })
                }
            }
        }
    }
}

@Composable
private fun ProfileDialog(core: ImCore, currentNickname: String, status: String, onClose: () -> Unit) {
    val context = LocalContext.current
    var nickname by remember { mutableStateOf(currentNickname) }
    var oldPass by remember { mutableStateOf("") }
    var newPass by remember { mutableStateOf("") }
    var avatarPath by remember { mutableStateOf("") }
    var err by remember { mutableStateOf("") }

    val avatarLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.OpenDocument()
    ) { uri: Uri? ->
        if (uri != null) {
            val file = File(context.cacheDir, "avatar_${System.currentTimeMillis()}.img")
            try {
                context.contentResolver.openInputStream(uri)?.use { input ->
                    file.outputStream().use { input.copyTo(it) }
                }
                avatarPath = file.absolutePath
            } catch (e: Exception) {
                err = "读取头像失败"
            }
        }
    }

    AlertDialog(
        onDismissRequest = onClose,
        title = { Text("个人资料") },
        text = {
            Column {
                OutlinedTextField(nickname, { nickname = it },
                    label = { Text("昵称（留空不修改）") }, singleLine = true)
                Spacer(Modifier.height(8.dp))
                OutlinedTextField(oldPass, { oldPass = it },
                    label = { Text("旧密码") }, singleLine = true,
                    visualTransformation = PasswordVisualTransformation())
                Spacer(Modifier.height(8.dp))
                OutlinedTextField(newPass, { newPass = it },
                    label = { Text("新密码（留空不修改）") }, singleLine = true,
                    visualTransformation = PasswordVisualTransformation())
                Spacer(Modifier.height(8.dp))
                TextButton(onClick = { avatarLauncher.launch(arrayOf("image/*")) }) {
                    Text(if (avatarPath.isNotEmpty()) "已选择头像，点击更换" else "选择头像图片")
                }
                if (err.isNotEmpty() || (status.isNotEmpty() && nickname.isEmpty())) {
                    Text(err.ifEmpty { status }, color = Color(0xFFF23F42), fontSize = 13.sp)
                }
            }
        },
        confirmButton = {
            TextButton(onClick = {
                if (newPass.isNotEmpty() && newPass.length < 4) { err = "新密码至少4位"; return@TextButton }
                core.updateProfile(nickname.trim(), avatarPath, oldPass, newPass)
                onClose()
            }) { Text("保存") }
        },
        dismissButton = { TextButton(onClick = onClose) { Text("取消") } }
    )
}

@Composable
private fun LoginScreen(core: ImCore, status: String, onStatus: (String) -> Unit) {
    var host by remember { mutableStateOf("127.0.0.1") }
    var port by remember { mutableStateOf("9000") }
    var user by remember { mutableStateOf("") }
    var pass by remember { mutableStateOf("") }
    var registerMode by remember { mutableStateOf(false) }

    Column(
        modifier = Modifier.fillMaxSize().background(Bg3).padding(32.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center
    ) {
        Text("COLBT", color = TextMain, fontSize = 32.sp)
        Spacer(Modifier.height(24.dp))
        OutlinedTextField(host, { host = it }, label = { Text("服务器") }, singleLine = true)
        Spacer(Modifier.height(8.dp))
        OutlinedTextField(port, { port = it }, label = { Text("端口") }, singleLine = true)
        Spacer(Modifier.height(8.dp))
        OutlinedTextField(user, { user = it }, label = { Text("用户名") }, singleLine = true)
        Spacer(Modifier.height(8.dp))
        OutlinedTextField(pass, { pass = it }, label = { Text("密码") }, singleLine = true)
        if (registerMode) {
            Spacer(Modifier.height(8.dp))
            var nick by remember { mutableStateOf("") }
            OutlinedTextField(nick, { nick = it }, label = { Text("昵称") }, singleLine = true)
        }
        if (status.isNotEmpty()) {
            Spacer(Modifier.height(8.dp))
            Text(status, color = Color(0xFFF23F42), fontSize = 13.sp)
        }
        Spacer(Modifier.height(16.dp))
        Button(
            onClick = {
                core.start(host, port.toIntOrNull() ?: 9000)
                if (registerMode) core.register(user.trim(), pass, "你")
                else core.login(user.trim(), pass)
            },
            modifier = Modifier.fillMaxWidth()
        ) { Text(if (registerMode) "注 册" else "登 录") }
        TextButton(onClick = { registerMode = !registerMode }) {
            Text(if (registerMode) "已有账号？去登录" else "没有账号？立即注册")
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun MainScreen(
    sessions: List<ImSession>,
    buddies: List<ImBuddy>,
    groups: List<ImGroup>,
    messages: List<ImMessage>,
    active: ImSession?,
    typing: Boolean,
    onOpenSession: (ImSession) -> Unit,
    onCloseSession: () -> Unit,
    onSend: (String) -> Unit,
    onTyping: () -> Unit,
    onAddFriend: () -> Unit,
    onProfile: () -> Unit,
    onLogout: () -> Unit,
) {
    var tab by remember { mutableStateOf(0) }

    if (active != null) {
        ChatView(active, messages, typing, onCloseSession, onSend, onTyping)
    } else {
        Row(modifier = Modifier.fillMaxSize()) {
            // 左侧栏
            Column(
                modifier = Modifier.width(64.dp).fillMaxHeight().background(Bg1),
                horizontalAlignment = Alignment.CenterHorizontally
            ) {
                Spacer(Modifier.height(12.dp))
                Tab(selected = tab == 0, text = "消息") { tab = 0 }
                Tab(selected = tab == 1, text = "好友") { tab = 1 }
                Spacer(Modifier.weight(1f))
                Tab(selected = false, text = "资料", onClick = onProfile)
                Tab(selected = false, text = "退出", onClick = onLogout)
                Spacer(Modifier.height(12.dp))
            }
            // 列表区
            Column(modifier = Modifier.weight(1f).fillMaxHeight().background(Bg2)) {
                if (tab == 0) {
                    LazyColumn(modifier = Modifier.weight(1f)) {
                        items(sessions, key = { it.targetId.toString() + it.targetType }) { s ->
                            SessionRow(s, false) { onOpenSession(s) }
                        }
                    }
                } else {
                    LazyColumn(modifier = Modifier.weight(1f)) {
                        item {
                            Row(
                                modifier = Modifier.fillMaxWidth().clickable { onAddFriend() }
                                    .padding(12.dp),
                                verticalAlignment = Alignment.CenterVertically
                            ) {
                                Text("＋ 添加好友", color = Accent, fontSize = 14.sp)
                            }
                        }
                        item { Text("我的好友 (${buddies.size})", color = TextSub, modifier = Modifier.padding(12.dp)) }
                        items(buddies, key = { it.id }) { b ->
                            ContactRow(b.nickname.ifEmpty { b.username }, b.online == 1) {
                                onOpenSession(ImSession(b.id, 0, b.nickname.ifEmpty { b.username }, "", "", 0, 0))
                            }
                        }
                        item { Text("我的群组 (${groups.size})", color = TextSub, modifier = Modifier.padding(12.dp)) }
                        items(groups, key = { it.id }) { g ->
                            ContactRow(g.name, true) {
                                onOpenSession(ImSession(g.id, 1, g.name, "", "", 0, 0))
                            }
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun ChatView(
    active: ImSession,
    messages: List<ImMessage>,
    typing: Boolean,
    onBack: () -> Unit,
    onSend: (String) -> Unit,
    onTyping: () -> Unit,
) {
    Column(modifier = Modifier.fillMaxSize().background(Bg3)) {
        Row(
            modifier = Modifier.fillMaxWidth().background(Bg3).padding(horizontal = 4.dp, vertical = 6.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            TextButton(onClick = onBack) { Text("‹", color = TextMain, fontSize = 24.sp) }
            Text(active.title, color = TextMain, fontSize = 16.sp, fontWeight = FontWeight.Bold)
            Spacer(Modifier.weight(1f))
            Text(if (typing) "对方正在输入…" else "", color = TextSub, fontSize = 12.sp)
        }
        HorizontalDivider(color = Bg1)
        LazyColumn(
            modifier = Modifier.weight(1f).fillMaxWidth(),
            contentPadding = PaddingValues(12.dp)
        ) {
            items(messages, key = { it.id }) { msg ->
                MessageRow(msg)
            }
        }
        HorizontalDivider(color = Bg1)
        var input by remember { mutableStateOf("") }
        Row(modifier = Modifier.fillMaxWidth().padding(12.dp), verticalAlignment = Alignment.Bottom) {
            OutlinedTextField(
                value = input, onValueChange = { input = it; onTyping() },
                modifier = Modifier.weight(1f), placeholder = { Text("输入消息") }
            )
            Spacer(Modifier.width(8.dp))
            Button(onClick = {
                if (input.isNotBlank()) { onSend(input.trim()); input = "" }
            }) { Text("发送") }
        }
    }
}

@Composable
private fun Tab(selected: Boolean, text: String, onClick: () -> Unit) {
    Box(
        modifier = Modifier
            .padding(4.dp)
            .clip(RoundedCornerShape(8.dp))
            .background(if (selected) Accent else Color.Transparent)
            .clickable { onClick() }
            .padding(horizontal = 8.dp, vertical = 10.dp)
    ) {
        Text(text, color = if (selected) Color.White else TextSub, fontSize = 12.sp)
    }
}

@Composable
private fun SessionRow(s: ImSession, selected: Boolean, onClick: () -> Unit) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .background(if (selected) Color(0xFF404249) else Color.Transparent)
            .clickable { onClick() }
            .padding(12.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Box(
            modifier = Modifier.size(40.dp).clip(RoundedCornerShape(20.dp)).background(Accent),
            contentAlignment = Alignment.Center
        ) { Text(s.title.firstOrNull()?.toString() ?: "?", color = Color.White) }
        Spacer(Modifier.width(10.dp))
        Column(modifier = Modifier.weight(1f)) {
            Text(s.title, color = TextMain, maxLines = 1, overflow = TextOverflow.Ellipsis)
            Text(
                s.lastContent.ifEmpty { " " }, color = TextSub, fontSize = 12.sp,
                maxLines = 1, overflow = TextOverflow.Ellipsis
            )
        }
        if (s.unread > 0) {
            Box(
                modifier = Modifier
                    .clip(RoundedCornerShape(9.dp))
                    .background(Color(0xFFED4245))
                    .padding(horizontal = 6.dp, vertical = 2.dp)
            ) { Text(if (s.unread > 99) "99+" else s.unread.toString(), color = Color.White, fontSize = 11.sp) }
        }
    }
}

@Composable
private fun ContactRow(name: String, online: Boolean, onClick: () -> Unit) {
    Row(
        modifier = Modifier.fillMaxWidth().clickable { onClick() }.padding(horizontal = 12.dp, vertical = 8.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Box(
            modifier = Modifier.size(34.dp).clip(RoundedCornerShape(17.dp)).background(Accent),
            contentAlignment = Alignment.Center
        ) { Text(name.firstOrNull()?.toString() ?: "?", color = Color.White, fontSize = 14.sp) }
        Spacer(Modifier.width(10.dp))
        Text(name, color = if (online) TextMain else TextSub)
    }
}

@Composable
private fun MessageRow(msg: ImMessage) {
    val mine = msg.direction == 0
    Row(
        modifier = Modifier.fillMaxWidth().padding(vertical = 4.dp),
        horizontalArrangement = if (mine) Arrangement.End else Arrangement.Start
    ) {
        Column(
            modifier = Modifier
                .widthIn(max = 300.dp)
                .clip(RoundedCornerShape(8.dp))
                .background(if (mine) Accent else Color(0xFF383A40))
                .padding(10.dp)
        ) {
            if (msg.msgType == 1) Text("[图片]", color = Color.White)
            else if (msg.msgType == 2) Text("[文件] ${msg.content}", color = Color.White)
            else Text(msg.content, color = Color.White)
            if (mine) {
                Text(
                    if (msg.read != 0) "已读" else "未读",
                    fontSize = 10.sp,
                    color = if (msg.read != 0) Color(0xFF23A559) else Color(0xFF949BA4),
                    modifier = Modifier.align(Alignment.End)
                )
            }
        }
    }
}
