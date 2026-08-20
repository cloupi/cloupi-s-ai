/*
cloupi's ai
 */

#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <ctime>
#include <deque>
#include <functional>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ws2_32.lib")

using namespace std;


static volatile bool g_interrupt = false;
static HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
static HANDLE hConsoleIn = GetStdHandle(STD_INPUT_HANDLE);
static WORD defaultAttr = 7;


enum Color {
    DARK_BLUE=1,DARK_GREEN,DARK_CYAN,DARK_RED,DARK_MAGENTA,DARK_YELLOW,GRAY,
    DARK_GRAY,BLUE,GREEN,CYAN,RED,MAGENTA,YELLOW,WHITE
};

void setColor(int c) { SetConsoleTextAttribute(hConsole, c); }
void resetColor() { SetConsoleTextAttribute(hConsole, defaultAttr); }
void printc(int c, const string& s) { setColor(c); cout << s; resetColor(); cout.flush(); }
void printlnc(int c, const string& s) { setColor(c); cout << s << endl; resetColor(); }


string trim(const string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

string toLower(string s) { transform(s.begin(), s.end(), s.begin(), ::tolower); return s; }

string jsonEscape(const string& s) {
    ostringstream oss;
    for (char c : s) {
        switch(c) {
            case '"': oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:
                if ((unsigned char)c < 0x20)
                    oss << "\\u" << uppercase << hex << (int)(unsigned char)c << nouppercase;
                else oss << c;
        }
    }
    return oss.str();
}

// 从 JSON 中提取字符串字段
string jsonGetString(const string& json, const string& key) {
    string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == string::npos) return "";
    pos = json.find(':', pos + search.size());
    if (pos == string::npos) return "";
    pos++;
    while (pos < json.size() && (json[pos]==' '||json[pos]=='\t')) pos++;
    if (pos >= json.size() || json[pos] != '"') return "";
    pos++;
    string result;
    while (pos < json.size()) {
        if (json[pos]=='\\' && pos+1<json.size()) {
            char next = json[pos+1];
            switch(next) {
                case '"': result+='"'; break;
                case '\\': result+='\\'; break;
                case '/': result+='/'; break;
                case 'n': result+='\n'; break;
                case 'r': result+='\r'; break;
                case 't': result+='\t'; break;
                case 'u': {
                    if (pos+5<json.size()) {
                        string hex = json.substr(pos+2,4);
                        unsigned int code = stoul(hex, nullptr, 16);
                        if (code < 0x80) result += (char)code;
                        else if (code < 0x800) {
                            result += (char)(0xC0|(code>>6));
                            result += (char)(0x80|(code&0x3F));
                        } else {
                            result += (char)(0xE0|(code>>12));
                            result += (char)(0x80|((code>>6)&0x3F));
                            result += (char)(0x80|(code&0x3F));
                        }
                        pos += 4;
                    }
                    break;
                }
                default: result += next;
            }
            pos += 2;
        } else if (json[pos]=='"') break;
        else { result += json[pos]; pos++; }
    }
    return result;
}

// 从 JSON 中提取数字字段
long long jsonGetInt(const string& json, const string& key) {
    string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == string::npos) return 0;
    pos = json.find(':', pos + search.size());
    if (pos == string::npos) return 0;
    pos++;
    while (pos < json.size() && (json[pos]==' '||json[pos]=='\t')) pos++;
    string num;
    while (pos < json.size() && (isdigit(json[pos])||json[pos]=='-')) {
        num += json[pos]; pos++;
    }
    return num.empty() ? 0 : stoll(num);
}


struct Config {
    string accountId;
    string apiToken;
    string model;
    string systemPrompt;
    bool typewriter = true; 
    int typewriterDelay = 15; // ms
};

Config g_config;
const string CONFIG_FILE = "cf_ai_config.txt";

void loadConfig() {
    ifstream f(CONFIG_FILE);
    if (!f.is_open()) {
        g_config.model = "@cf/meta/llama-3.1-8b-instruct";
        g_config.systemPrompt = "cloupi"s ai";
        return;
    }
    string line;
    while (getline(f, line)) {
        line = trim(line);
        if (line.empty() || line[0]=='#') continue;
        size_t eq = line.find('=');
        if (eq == string::npos) continue;
        string key = trim(line.substr(0, eq));
        string val = trim(line.substr(eq+1));
        if (key=="account_id") g_config.accountId = val;
        else if (key=="api_token") g_config.apiToken = val;
        else if (key=="model") g_config.model = val;
        else if (key=="system_prompt") g_config.systemPrompt = val;
        else if (key=="typewriter") g_config.typewriter = (val=="1"||val=="true");
        else if (key=="typewriter_delay") g_config.typewriterDelay = stoi(val);
    }
    if (g_config.model.empty()) g_config.model = "@cf/meta/llama-3.1-8b-instruct";
    if (g_config.systemPrompt.empty())
        g_config.systemPrompt = "cloupi"s ai";
}

void saveConfig() {
    ofstream f(CONFIG_FILE);
    f << "# ai\n";
    f << "# Account ID: https://dash.cloudflare.com/ 主页地址栏中获取\n";
    f << "# API Token: https://dash.cloudflare.com/profile/api-tokens 创建 (Workers AI 权限)\n";
    f << "account_id=" << g_config.accountId << "\n";
    f << "api_token=" << g_config.apiToken << "\n";
    f << "model=" << g_config.model << "\n";
    f << "system_prompt=" << g_config.systemPrompt << "\n";
    f << "typewriter=" << (g_config.typewriter?"1":"0") << "\n";
    f << "typewriter_delay=" << g_config.typewriterDelay << "\n";
}

// ========== 对话历史 ==========
struct Message { string role; string content; };
vector<Message> g_history;
int g_totalTokens = 0;
int g_turnCount = 0;

void resetHistory() {
    g_history.clear();
    g_turnCount = 0;
    g_totalTokens = 0;
    if (!g_config.systemPrompt.empty())
        g_history.push_back({"system", g_config.systemPrompt});
}

string buildRequestBody(bool stream) {
    ostringstream oss;
    oss << "{\"messages\":[";
    for (size_t i=0; i<g_history.size(); i++) {
        if (i>0) oss << ",";
        oss << "{\"role\":\"" << g_history[i].role << "\","
            << "\"content\":\"" << jsonEscape(g_history[i].content) << "\"}";
    }
    oss << "]";
    if (stream) oss << ",\"stream\":true";
    oss << "}";
    return oss.str();
}


int estimateTokens(const string& s) { return (int)s.size() / 4 + 1; }

// ========== WinHTTP ==========
struct WinHTTPReq {
    HINTERNET hSession=nullptr, hConnect=nullptr, hRequest=nullptr;
    ~WinHTTPReq() {
        if (hRequest) WinHttpCloseHandle(hRequest);
        if (hConnect) WinHttpCloseHandle(hConnect);
        if (hSession) WinHttpCloseHandle(hSession);
    }
};

bool parseUrl(const string& url, string& host, string& path, bool& https, int& port) {
    size_t pos = 0;
    if (url.substr(0,8)=="https://") { https=true; pos=8; }
    else if (url.substr(0,7)=="http://") { https=false; pos=7; }
    else return false;
    size_t slash = url.find('/', pos);
    if (slash==string::npos) { host=url.substr(pos); path="/"; }
    else { host=url.substr(pos, slash-pos); path=url.substr(slash); }
    size_t colon = host.find(':');
    port = https ? 443 : 80;
    if (colon!=string::npos) { port=stoi(host.substr(colon+1)); host=host.substr(0,colon); }
    return true;
}


string friendlyError(int httpCode, const string& body) {
    if (httpCode == 403) {
        if (body.find("not available") != string::npos || body.find("Free plan") != string::npos)
            return "该模型不在免费计划中，请用 /models 查看免费模型，或用 /model 切换到免费模型。";
        if (body.find("Invalid") != string::npos || body.find("unauthorized") != string::npos)
            return "API Token 无效或已过期，请重新创建。";
        return "权限不足 (403)，请检查 API Token 权限是否包含 Workers AI。";
    }
    if (httpCode == 404 || body.find("No route") != string::npos)
        return "模型不存在或已下架，请用 /models 查看当前可用模型。";
    if (httpCode == 429)
        return "请求过于频繁，免费计划有速率限制，请稍后再试。";
    if (httpCode == 400) {
        string msg = jsonGetString(body, "message");
        if (!msg.empty()) return "请求错误: " + msg;
        return "请求参数错误 (400)。";
    }
    if (httpCode >= 500)
        return "Cloudflare 服务器错误 (" + to_string(httpCode) + ")，请稍后再试。";
    string msg = jsonGetString(body, "message");
    if (!msg.empty()) return msg;
    return "HTTP " + to_string(httpCode) + " 错误";
}


bool sendStreamRequest(const string& url, const string& body,
                       function<void(const string& chunk)> onChunk,
                       string& errorMsg) {
    string host, path; bool https; int port;
    if (!parseUrl(url, host, path, https, port)) { errorMsg="URL解析失败"; return false; }

    WinHTTPReq req;
    req.hSession = WinHttpOpen(L"CF-AI-Chat/2.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!req.hSession) { errorMsg="WinHttpOpen失败"; return false; }
    WinHttpSetTimeouts(req.hSession, 15000, 15000, 15000, 180000);

    wstring wHost(host.begin(), host.end());
    req.hConnect = WinHttpConnect(req.hSession, wHost.c_str(), (INTERNET_PORT)port, 0);
    if (!req.hConnect) { errorMsg="连接服务器失败"; return false; }

    wstring wPath(path.begin(), path.end());
    req.hRequest = WinHttpOpenRequest(req.hConnect, L"POST", wPath.c_str(),
                                       nullptr, WINHTTP_NO_REFERER,
                                       WINHTTP_DEFAULT_ACCEPT_TYPES, https?WINHTTP_FLAG_SECURE:0);
    if (!req.hRequest) { errorMsg="创建请求失败"; return false; }

    wstring headers = L"Content-Type: application/json\r\nAccept: text/event-stream\r\n";
    if (!g_config.apiToken.empty()) {
        wstring token(g_config.apiToken.begin(), g_config.apiToken.end());
        headers += L"Authorization: Bearer " + token + L"\r\n";
    }
    WinHttpAddRequestHeaders(req.hRequest, headers.c_str(), (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD);

    if (!WinHttpSendRequest(req.hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                             (LPVOID)body.c_str(), (DWORD)body.size(), (DWORD)body.size(), 0)) {
        errorMsg = "发送请求失败 (网络错误?)";
        return false;
    }
    if (!WinHttpReceiveResponse(req.hRequest, nullptr)) {
        errorMsg = "接收响应失败 (网络错误?)";
        return false;
    }

    DWORD statusCode = 0, statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(req.hRequest, WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);

    if (statusCode != 200) {
        string errBody;
        DWORD avail = 0;
        while (WinHttpQueryDataAvailable(req.hRequest, &avail) && avail > 0) {
            vector<char> buf(avail); DWORD read = 0;
            if (WinHttpReadData(req.hRequest, buf.data(), avail, &read))
                errBody.append(buf.data(), read);
        }
        errorMsg = friendlyError((int)statusCode, errBody);
        return false;
    }


    DWORD avail = 0;
    string buffer; 
    while (!g_interrupt && WinHttpQueryDataAvailable(req.hRequest, &avail) && avail > 0) {
        vector<char> buf(avail);
        DWORD read = 0;
        if (!WinHttpReadData(req.hRequest, buf.data(), avail, &read)) break;
        buffer.append(buf.data(), read);

    
        size_t lineEnd;
        while ((lineEnd = buffer.find('\n')) != string::npos) {
            string line = buffer.substr(0, lineEnd);
            buffer.erase(0, lineEnd + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;

            if (line.substr(0,6) == "data: ") {
                string data = line.substr(6);
                if (data == "[DONE]") continue;
                onChunk(data);
            } else if (line.substr(0,5) == "data:") {
                string data = trim(line.substr(5));
                if (data == "[DONE]") continue;
                if (!data.empty()) onChunk(data);
            }
        }
    }

    
    if (!g_interrupt && !buffer.empty()) {
        string line = trim(buffer);
        if (line.substr(0,6) == "data: ") {
            string data = line.substr(6);
            if (data != "[DONE]") onChunk(data);
        }
    }

    if (g_interrupt) { errorMsg = "已中断"; return false; }
    return true;
}


bool testConnection(string& resultMsg) {
    string url = "https://api.cloudflare.com/client/v4/accounts/" +
                 g_config.accountId + "/ai/run/" + g_config.model;
    string body = "{\"messages\":[{\"role\":\"user\",\"content\":\"hi\"}],\"max_tokens\":5}";

    string host, path; bool https; int port;
    if (!parseUrl(url, host, path, https, port)) { resultMsg="URL解析失败"; return false; }

    WinHTTPReq req;
    req.hSession = WinHttpOpen(L"CF-AI-Test/2.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!req.hSession) { resultMsg="WinHttpOpen失败"; return false; }
    WinHttpSetTimeouts(req.hSession, 10000, 10000, 10000, 30000);

    wstring wHost(host.begin(), host.end());
    req.hConnect = WinHttpConnect(req.hSession, wHost.c_str(), (INTERNET_PORT)port, 0);
    if (!req.hConnect) { resultMsg="连接失败"; return false; }

    wstring wPath(path.begin(), path.end());
    req.hRequest = WinHttpOpenRequest(req.hConnect, L"POST", wPath.c_str(), nullptr,
                                       WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                       https?WINHTTP_FLAG_SECURE:0);
    if (!req.hRequest) { resultMsg="创建请求失败"; return false; }

    wstring headers = L"Content-Type: application/json\r\n";
    if (!g_config.apiToken.empty()) {
        wstring token(g_config.apiToken.begin(), g_config.apiToken.end());
        headers += L"Authorization: Bearer " + token + L"\r\n";
    }
    WinHttpAddRequestHeaders(req.hRequest, headers.c_str(), (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD);

    if (!WinHttpSendRequest(req.hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                             (LPVOID)body.c_str(), (DWORD)body.size(), (DWORD)body.size(), 0)) {
        resultMsg = "发送失败 (网络错误)"; return false;
    }
    if (!WinHttpReceiveResponse(req.hRequest, nullptr)) {
        resultMsg = "接收失败 (网络错误)"; return false;
    }

    DWORD statusCode = 0, sz = sizeof(statusCode);
    WinHttpQueryHeaders(req.hRequest, WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &sz, WINHTTP_NO_HEADER_INDEX);

    string respBody;
    DWORD avail = 0;
    while (WinHttpQueryDataAvailable(req.hRequest, &avail) && avail > 0) {
        vector<char> buf(avail); DWORD read = 0;
        if (WinHttpReadData(req.hRequest, buf.data(), avail, &read))
            respBody.append(buf.data(), read);
    }

    if (statusCode == 200) {
        resultMsg = "连接成功！模型 " + g_config.model + " 可用。";
        return true;
    } else {
        resultMsg = friendlyError((int)statusCode, respBody);
        return false;
    }
}


bool chat(const string& userInput) {
    g_interrupt = false;
    g_history.push_back({"user", userInput});
    g_turnCount++;

    string url = "https://api.cloudflare.com/client/v4/accounts/" +
                 g_config.accountId + "/ai/run/" + g_config.model;
    string body = buildRequestBody(true);

    string fullResponse;
    int chunkCount = 0;

    printc(CYAN, "\n  AI: ");

    auto onChunk = [&](const string& data) {
        if (g_interrupt) return;
        string chunk = jsonGetString(data, "response");
        if (!chunk.empty()) {
            fullResponse += chunk;
            // 打字机效果
            if (g_config.typewriter) {
                for (char c : chunk) {
                    if (g_interrupt) break;
                    cout << c;
                    cout.flush();
                    if (g_config.typewriterDelay > 0)
                        Sleep(g_config.typewriterDelay);
                }
            } else {
                cout << chunk;
                cout.flush();
            }
            chunkCount++;
        }
    };

    string errorMsg;
    bool ok = sendStreamRequest(url, body, onChunk, errorMsg);
    cout << endl;

    if (!ok) {
        if (g_interrupt) {
            printlnc(YELLOW, "  [已中断]");
        } else {
            printlnc(RED, "  [错误] " + errorMsg);
        }
        g_history.pop_back();
        g_turnCount--;
        return false;
    }

    if (fullResponse.empty()) {
        printlnc(YELLOW, "  (空响应，模型可能不支持该格式)");
    }

    g_history.push_back({"assistant", fullResponse});
    g_totalTokens += estimateTokens(userInput) + estimateTokens(fullResponse);

    // 显示统计
    printc(DARK_GRAY, "  [轮次 " + to_string(g_turnCount) +
           " | 约 " + to_string(g_totalTokens) + " tokens | " +
           to_string(chunkCount) + " 块]");
    cout << endl << endl;
    return true;
}


string readLine() {
    wchar_t buf[4096];
    DWORD read = 0;
    if (!ReadConsoleW(hConsoleIn, buf, 4095, &read, nullptr)) {
        return "";
    }
    buf[read] = 0;
    // 去掉末尾的 \r\n
    while (read > 0 && (buf[read-1] == L'\r' || buf[read-1] == L'\n')) {
        buf[--read] = 0;
    }
    // 宽字符转 UTF-8
    int len = WideCharToMultiByte(CP_UTF8, 0, buf, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return "";
    string result(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, buf, -1, &result[0], len, nullptr, nullptr);
    // 去掉末尾的 null 字符
    while (!result.empty() && result.back() == '\0') result.pop_back();
    return result;
}
void printHelp() {
    printlnc(GREEN, "\n  ════════ 命令列表 ════════");
    cout << "  /help        显示帮助\n";
    cout << "  /clear       清空对话历史\n";
    cout << "  /model       查看当前模型\n";
    cout << "  /model <名>  切换模型\n";
    cout << "  /models      列出常用免费模型\n";
    cout << "  /system      查看系统提示词\n";
    cout << "  /system <词> 修改系统提示词\n";
    cout << "  /config      查看当前配置\n";
    cout << "  /test        测试API连接\n";
    cout << "  /stats       对话统计\n";
    cout << "  /save        保存对话到文件\n";
    cout << "  /typewriter  开关打字机效果\n";
    cout << "  /exit        退出\n";
    cout << "  ↑↓ 浏览历史输入 (系统自带)\n";
    printlnc(GREEN, "  ═══════════════════════════\n");
}

void printModels() {
    printlnc(GREEN, "\n  ════════ 常用免费模型 ════════");
    vector<vector<string>> models = {
        {"@cf/meta/llama-3.1-8b-instruct", "Llama 3.1 8B", "通用，速度快，推荐"},
        {"@cf/meta/llama-3.2-1b-instruct", "Llama 3.2 1B", "超快，简单任务"},
        {"@cf/qwen/qwen2.5-7b-instruct", "通义千问 2.5 7B", "中文效果好"},
        {"@cf/qwen/qwq-32b", "QwQ 32B", "推理模型，复杂任务"},
        {"@cf/mistral/mistral-7b-instruct-v0.2", "Mistral 7B", "轻量快速"},
        {"@cf/google/gemma-3-12b-it", "Gemma 3 12B", "Google 新模型"},
        {"@cf/deepseek-ai/deepseek-r1-distill-qwen-32b", "DeepSeek R1", "推理蒸馏版"},
    };
    for (auto& m : models) {
        printc(YELLOW, "  " + m[0]);
        printc(DARK_GRAY, "  (" + m[1] + ") ");
        printc(GRAY, m[2]);
        cout << endl;
    }
    printlnc(DARK_GRAY, "  完整列表: https://developers.cloudflare.com/workers-ai/models/");
    printlnc(GREEN, "  ════════════════════════════════\n");
}

void printConfig() {
    printlnc(GREEN, "\n  ════════ 当前配置 ════════");
    cout << "  Account ID: ";
    if (g_config.accountId.empty()) printc(RED, "(未设置)");
    else printc(YELLOW, g_config.accountId.substr(0,8)+"..."+g_config.accountId.substr(g_config.accountId.size()-4));
    cout << "\n  API Token:  ";
    if (g_config.apiToken.empty()) printc(RED, "(未设置)");
    else printc(YELLOW, g_config.apiToken.substr(0,6)+"..."+g_config.apiToken.substr(g_config.apiToken.size()-4));
    cout << "\n  Model:      "; printc(YELLOW, g_config.model);
    cout << "\n  打字机:     "; printc(g_config.typewriter?GREEN:RED, g_config.typewriter?"开启":"关闭");
    cout << "\n  System:     "; printc(GRAY, g_config.systemPrompt);
    printlnc(GREEN, "\n  ════════════════════════════\n");
}

void printStats() {
    int userMsgs = 0, assistantMsgs = 0;
    for (auto& m : g_history) {
        if (m.role == "user") userMsgs++;
        if (m.role == "assistant") assistantMsgs++;
    }
    printlnc(GREEN, "\n  ════════ 对话统计 ════════");
    cout << "  对话轮次: " << g_turnCount << "\n";
    cout << "  用户消息: " << userMsgs << "\n";
    cout << "  AI 回复:  " << assistantMsgs << "\n";
    cout << "  约 tokens: " << g_totalTokens << "\n";
    cout << "  当前模型: " << g_config.model << "\n";
    printlnc(GREEN, "  ════════════════════════════\n");
}

void saveConversation() {
    time_t now = time(nullptr);
    struct tm t; localtime_s(&t, &now);
    char fname[64];
    strftime(fname, sizeof(fname), "chat_%Y%m%d_%H%M%S.txt", &t);
    ofstream f(fname);
    f << "Cloudflare Workers AI 对话记录\n";
    f << "模型: " << g_config.model << "\n";
    f << "时间: " << asctime(&t) << "\n";
    f << "========================================\n\n";
    for (auto& msg : g_history) {
        if (msg.role == "system") continue;
        f << "[" << (msg.role=="user"?"你":"AI") << "]\n" << msg.content << "\n\n";
    }
    printlnc(GREEN, "\n  ✓ 对话已保存到: " + string(fname) + "\n");
}

void handleCommand(const string& input) {
    string cmd = input;
    if (!cmd.empty() && cmd[0]=='/') cmd = cmd.substr(1);
    cmd = trim(cmd);
    string args;
    size_t sp = cmd.find(' ');
    if (sp != string::npos) { args = trim(cmd.substr(sp+1)); cmd = cmd.substr(0,sp); }
    cmd = toLower(cmd);

    if (cmd=="help"||cmd=="?") printHelp();
    else if (cmd=="clear"||cmd=="cls") { resetHistory(); printlnc(GREEN, "\n  ✓ 对话历史已清空。\n"); }
    else if (cmd=="model") {
        if (args.empty()) {
            printlnc(YELLOW, "\n  当前模型: " + g_config.model);
            printlnc(DARK_GRAY, "  输入 /models 查看可用模型列表\n");
        } else {
            g_config.model = args;
            saveConfig();
            printlnc(GREEN, "\n  ✓ 模型已切换为: " + g_config.model);
            printlnc(DARK_GRAY, "  输入 /test 测试连接\n");
        }
    }
    else if (cmd=="models") printModels();
    else if (cmd=="system") {
        if (args.empty()) printlnc(YELLOW, "\n  当前系统提示词: " + g_config.systemPrompt + "\n");
        else {
            g_config.systemPrompt = args;
            saveConfig();
            resetHistory();
            printlnc(GREEN, "\n  ✓ 系统提示词已更新，对话已重置。\n");
        }
    }
    else if (cmd=="config") printConfig();
    else if (cmd=="test") {
        printc(YELLOW, "\n  正在测试连接... ");
        cout.flush();
        string msg;
        bool ok = testConnection(msg);
        if (ok) printlnc(GREEN, "✓ " + msg);
        else printlnc(RED, "✗ " + msg);
        cout << endl;
    }
    else if (cmd=="stats") printStats();
    else if (cmd=="save") saveConversation();
    else if (cmd=="typewriter") {
        g_config.typewriter = !g_config.typewriter;
        saveConfig();
        printlnc(GREEN, "\n  打字机效果已" + string(g_config.typewriter?"开启":"关闭") + "\n");
    }
    else if (cmd=="exit"||cmd=="quit") {
        printlnc(CYAN, "\n  再见！\n");
        exit(0);
    }
    else printlnc(RED, "\n  未知命令: /" + cmd + "，输入 /help 查看。\n");
}

// ========== Ctrl+C 处理 ==========
BOOL WINAPI ConsoleHandler(DWORD type) {
    if (type == CTRL_C_EVENT) {
        g_interrupt = true;
        return TRUE;
    }
    return FALSE;
}

// ========== 主函数 ==========
int main() {
    // 初始化控制台
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(hConsole, &csbi)) defaultAttr = csbi.wAttributes;
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    // 标题
    SetConsoleTitleA("Cloudflare Workers AI 聊天助手 v2.0");

    loadConfig();
    resetHistory();

    // 欢迎界面
    system("cls");
    setColor(CYAN);
    cout << "\n\n";
    cout << "         █████╗ ██╗\n";
    cout << "        ██╔══██╗██║\n";
    cout << "        ███████║██║\n";
    cout << "        ██╔══██║██║\n";
    cout << "        ██║  ██║██║\n";
    cout << "        ╚═╝  ╚═╝╚═╝\n";
    cout << "\n";
    resetColor();

    // 首次配置引导
    if (g_config.accountId.empty() || g_config.apiToken.empty()) {
        printlnc(YELLOW, "\n  ⚠ 尚未配置 Cloudflare 凭证！");
        cout << "  请编辑 " << CONFIG_FILE << " 或按引导输入:\n\n";
        printc(GREEN, "  Account ID: "); resetColor();
        string accId; getline(cin, accId); accId = trim(accId);
        if (!accId.empty()) {
            g_config.accountId = accId;
            printc(GREEN, "  API Token:  "); resetColor();
            string token; getline(cin, token); token = trim(token);
            if (!token.empty()) {
                g_config.apiToken = token;
                saveConfig();
                printlnc(GREEN, "\n  ✓ 配置已保存！\n");
            }
        }
    }

    printConfig();
    printlnc(GREEN, "  输入 /help 查看命令，/models 查看模型，直接输入开始聊天。\n");

   
    while (true) {
        printc(GREEN, "  你> ");
        resetColor();
        string input = readLine();
        input = trim(input);
        if (input.empty()) continue;

        if (input[0] == '/') handleCommand(input);
        else chat(input);
    }

    return 0;
}
