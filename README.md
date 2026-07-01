# ShellWindows-BOF

一个简单的 Cobalt Strike Beacon Object File (BOF) 工具。主要用于 Shellcode 上线后的权限维持与会话断链转移。工具通过调用 Explorer 的 `ShellWindows` COM 接口来启动新进程，使其父进程强制变成 `explorer.exe`。
<img width="754" height="70" alt="0880c08b0110879ea546" src="https://github.com/user-attachments/assets/55fe3a56-1cf0-4101-9348-dc15dc5be774" />


## 功能特点

- **进程树断链**：新进程由 `explorer.exe` 派生，从进程树上看类似用户手动双击打开，避开常规的父子进程关联审计。

- **自动降权与环境加载**：如果在 `SYSTEM` 权限下运行，由于 COM 路由机制，新进程通常会跑在当前活跃的普通用户 Session 下，继承其 Token 并加载对应的用户 Profile 注册表与 DPAPI 环境（适合需要读取当前用户浏览器数据等场景）。

- **全内存执行**：纯 C 语言实现的 BOF 内存文件，在 Beacon 进程内直接调用 COM 指针，不落盘。

- **一键调用**：CNA 脚本自动加载对应架构的 `.o` 文件，支持可选参数安全传递。

## 使用方法

将 `shellwindows.cna` 导入 Cobalt Strike 的 Script Manager。

### 命令语法

```text
shellwindows <target_exe> [args] [dir] [show]
```

| 参数         | 说明                                                         |
| ------------ | ------------------------------------------------------------ |
| `target_exe` | 目标程序的全路径（**必填**）。                               |
| `args`       | 命令行参数（选填，默认空）。                                 |
| `dir`        | 工作目录（选填，默认 `C:\`）。                               |
| `show`       | 窗口显示状态（选填，`0` 为隐藏运行，`1` 为正常显示。默认 `0`）。 |

### 示例

**基础断链运行**（用于权限维持会话转移）：

```text
shellwindows C:\Windows\Temp\beacon_x64.exe
```

**带参数运行系统工具**：

```text
shellwindows C:\Windows\System32\cmd.exe "/c whoami /all > C:\Windows\Temp\1.txt"
```

**SYSTEM 权限下切换到活动用户环境执行**：

```text
shellwindows C:\Windows\Temp\beacon_x64.exe "" C:\Users\Public 0
```


## 注意事项与局限性

- **依赖活跃会话**：目标系统中必须存在处于运行状态的 `explorer.exe`。如果目标用户点了"注销"（Logoff），或系统刚重启且没有任何用户登录过，该 COM 调用会失败。

- **支持断开连接状态**：若目标用户仅是关闭了 RDP 远程桌面窗口（按叉号退出造成的 Disconnected 状态），只要其桌面会话及 `explorer.exe` 仍驻留于内存中，本工具即可正常调用并托管进程。

- **EDR 动态审计**：某些对 RPC/COM 跨进程调用监控非常严格的高级 EDR 可能会对该行为进行动态审计。

## 免责声明（Disclaimer）

本工具及相关代码**仅用于获得合法授权的企业安全演练、漏洞验证及网络安全技术研究**。

在使用本工具进行任何测试时，您应当确保该行为符合当地的法律法规，并已获得目标系统的**明确书面授权**。作者不承担因任何滥用、非法使用或不当操作此工具而导致的直接或间接损失及法律责任。

若您下载、复制或使用本仓库中的任何内容，即代表您已接受此免责声明。
