# Cloudflare Workers AI 命令行聊天助手

纯 C++ 编写，基于 Windows WinHTTP，无任何第三方依赖。

## 文件说明

- `cf_ai_chat.cpp` — 源代码（单文件）
- `cf_ai_chat.exe` — 编译好的可执行文件
- `cf_ai_config.txt` — 配置文件（首次运行后自动生成）

## 快速开始

### 1. 获取 Cloudflare 凭证

1. 注册/登录 Cloudflare: https://dash.cloudflare.com/
2. 查看 **Account ID**（在主页右侧栏，或任意域名的 Overview 页面）
3. 创建 **API Token**:
   - 访问 https://dash.cloudflare.com/profile/api-tokens
   - 点击 "Create Token"
   - 模板选 "Workers AI (Read)"，或自定义权限：
     - Account > Workers AI > Read
   - 复制生成的 Token（只显示一次）

### 2. 运行程序

双击 `cf_ai_chat.exe`，首次运行会引导你输入 Account ID 和 API Token。

也可以手动编辑 `cf_ai_config.txt`：

```
account_id=你的AccountID
api_token=你的APIToken
model=@cf/zai-org/glm-5.2
system_prompt=你是一个有帮助的AI助手。
```

### 3. 开始聊天

直接输入文字回车即可，AI 会流式输出回复。

## 内置命令

| 命令 | 说明 |
|------|------|
| `/help` | 显示帮助 |
| `/clear` | 清空对话历史 |
| `/model` | 查看当前模型和可用模型列表 |
| `/model <模型名>` | 切换模型（如 `/model @cf/meta/llama-3.1-8b-instruct`） |
| `/system` | 查看当前系统提示词 |
| `/system <提示词>` | 修改系统提示词并重置对话 |
| `/config` | 查看当前配置 |
| `/save` | 保存对话到文件 |
| `/exit` 或 `/quit` | 退出程序 |

## 常用模型

| 模型 ID | 说明 |
|---------|------|
| `@cf/zai-org/glm-5.2` | 智谱 GLM-5.2（默认，中文好） |
| `@cf/meta/llama-3.1-8b-instruct` | Meta Llama 3.1 8B |
| `@cf/meta/llama-3.3-70b-instruct-fp8` | Meta Llama 3.3 70B |
| `@cf/qwen/qwen2.5-7b-instruct` | 通义千问 2.5 7B |
| `@cf/deepseek-ai/deepseek-r1-distill-qwen-32b` | DeepSeek R1 蒸馏版 |
| `@cf/google/gemma-2-9b-it` | Google Gemma 2 9B |

完整模型列表：https://developers.cloudflare.com/workers-ai/models/

## 自行编译

```bash
g++ cf_ai_chat.cpp -o cf_ai_chat.exe -lwinhttp -lws2_32 -O2 -s -static-libgcc -static-libstdc++
```

需要 MinGW-w64 或 MSYS2 环境。

## 技术特点

- **零第三方依赖**：只用 Windows 原生 API（WinHTTP）
- **流式输出**：SSE 逐字打印，体验流畅
- **彩色界面**：16 色控制台输出
- **多轮对话**：自动维护上下文历史
- **UTF-8 支持**：中文输入输出正常
- **配置持久化**：凭证和设置保存到本地文件
