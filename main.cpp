#include "eui_neo.h"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#define CMD_LIT(x) L##x
#else
#define CMD_LIT(x) x
#endif

namespace fs = std::filesystem;

namespace app {

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}
        .title("osu!lazer 谱面导出器")
        .pageId("osz_export_gui")
        .windowSize(1240, 820)
        .minWindowSize(1040, 680)
        .clearColor({0.043f, 0.051f, 0.067f, 1.0f})
        .fps(60.0);
    return config;
}

namespace {

enum class SortKey { Title = 0, Artist = 1, Date = 2 };

struct Row {
    std::string id;
    std::string date;
    std::string artist;
    std::string title;
    std::string creator;
    std::string audio;
    int diffs = 0;
    int files = 0;
    long long bytes = 0;
    int modes[4] = {0, 0, 0, 0};
    bool selected = false;
};

struct State {
    std::vector<Row> rows;
    eui::Signal<std::string> search{std::string()};
    eui::Signal<float> scroll{0.0f};
    bool started = false;
    bool scanning = false;
    bool exporting = false;
    bool ready = false;
    SortKey sortKey = SortKey::Date;
    bool descending = true;
    std::string status = "正在读取 osu!lazer 游戏数据库…";
    std::string detail;
} g;

fs::path gOutDir;

std::string lowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string sizeText(long long bytes) {
    if (bytes >= 1024LL * 1024LL * 1024LL)
        return std::to_string(bytes / (1024LL * 1024LL * 1024LL)) + " GB";
    if (bytes >= 1024LL * 1024LL)
        return std::to_string(bytes / (1024LL * 1024LL)) + " MB";
    return std::to_string(std::max<long long>(1, bytes / 1024)) + " KB";
}

std::string readAll(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

fs::path exeDir() {
#ifdef _WIN32
    wchar_t buffer[MAX_PATH * 4]{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
    if (length == 0) return fs::current_path();
    return fs::path(std::wstring(buffer, length)).parent_path();
#else
    char buffer[4096]{};
    const ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (length <= 0) return fs::current_path();
    return fs::path(std::string(buffer, static_cast<std::size_t>(length))).parent_path();
#endif
}

std::string helperDirString() {
    return (exeDir() / "helper").u8string();
}

#ifdef _WIN32
using Cmd = std::wstring;
Cmd quoted(const fs::path& path) { return L"\"" + path.wstring() + L"\""; }
Cmd quoted(const std::wstring& value) { return L"\"" + value + L"\""; }

int runCommand(const Cmd& command, const fs::path& logPath) {
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE log = CreateFileW(logPath.wstring().c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (log == INVALID_HANDLE_VALUE) return -1;

    HANDLE nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = log;
    si.hStdError = log;
    si.hStdInput = nul;

    PROCESS_INFORMATION pi{};
    std::wstring mutableCommand = command;
    const BOOL created = CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, TRUE,
                                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    if (!created) {
        CloseHandle(log);
        if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
        return -2;
    }

    DWORD exitCode = 0;
    if (WaitForSingleObject(pi.hProcess, 15 * 60 * 1000) == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        exitCode = static_cast<DWORD>(-3);
    } else {
        GetExitCodeProcess(pi.hProcess, &exitCode);
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(log);
    if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
    return static_cast<int>(exitCode);
}

std::wstring dotnetPath() {
    const wchar_t* candidates[] = {
        L"C:\\Program Files\\dotnet\\dotnet.exe",
        L"C:\\Program Files (x86)\\dotnet\\dotnet.exe",
    };
    for (const wchar_t* candidate : candidates)
        if (fs::exists(candidate)) return candidate;
    return L"dotnet.exe";
}

#else
using Cmd = std::string;
Cmd quoted(const fs::path& path) { return "'" + path.string() + "'"; }
Cmd quoted(const std::string& value) { return "'" + value + "'"; }

int runCommand(const Cmd& command, const fs::path& logPath) {
    const std::string full = command + " > " + quoted(logPath) + " 2>&1";
    const int rc = std::system(full.c_str());
    if (rc == -1) return -1;
    if (WIFEXITED(rc)) return WEXITSTATUS(rc);
    return -2;
}

std::string dotnetPath() {
    return "dotnet";
}
#endif

fs::path osuDir() {
#ifdef _WIN32
    const char* appData = std::getenv("APPDATA");
    if (!appData) return {};
    return fs::path(appData) / "osu";
#else
    const char* dataHome = std::getenv("XDG_DATA_HOME");
    if (dataHome && *dataHome) return fs::path(dataHome) / "osu";
    const char* home = std::getenv("HOME");
    if (!home) return {};
    return fs::path(home) / ".local" / "share" / "osu";
#endif
}

fs::path workDir() { return fs::temp_directory_path() / "osu-export-gui"; }
fs::path realmCopyPath() { return workDir() / "client.realm"; }
fs::path helperPath() { return exeDir() / "helper" / "OszExporter.dll"; }

void setEnv(const char* name, const std::string& value) {
#ifdef _WIN32
    SetEnvironmentVariableA(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

void openFolder(const fs::path& path) {
#ifdef _WIN32
    ShellExecuteW(nullptr, L"open", path.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
    const std::string command = "xdg-open " + quoted(path) + " >/dev/null 2>&1 &";
    std::system(command.c_str());
#endif
}

void parseTsv(const std::string& text) {
    g.rows.clear();
    std::istringstream stream(text);
    std::string line;
    bool header = true;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (header) { header = false; continue; }
        if (line.empty()) continue;

        std::vector<std::string> fields;
        std::size_t start = 0;
        while (true) {
            const std::size_t tab = line.find('\t', start);
            if (tab == std::string::npos) {
                fields.push_back(line.substr(start));
                break;
            }
            fields.push_back(line.substr(start, tab - start));
            start = tab + 1;
        }
        if (fields.size() < 9) continue;

        Row row;
        row.id = fields[0];
        row.date = fields[1];
        row.artist = fields[2];
        row.title = fields[3];
        row.creator = fields[4];
        row.diffs = std::atoi(fields[5].c_str());
        row.files = std::atoi(fields[6].c_str());
        row.bytes = std::atoll(fields[7].c_str());
        row.audio = fields[8];
        if (fields.size() >= 13) {
            for (int m = 0; m < 4; ++m)
                row.modes[m] = std::atoi(fields[9 + m].c_str());
        } else {
            row.modes[0] = row.diffs;
        }
        g.rows.push_back(std::move(row));
    }
}

app::async::Result<std::string> scanDatabase() {
    const fs::path osu = osuDir();
    if (osu.empty() || !fs::exists(osu / "client.realm"))
        return app::async::failure<std::string>("找不到 osu!lazer 数据目录（client.realm）");

    const fs::path tmp = workDir();
    std::error_code ec;
    fs::create_directories(tmp, ec);
    if (ec) return app::async::failure<std::string>("无法创建临时目录：" + ec.message());

    fs::copy_file(osu / "client.realm", realmCopyPath(), fs::copy_options::overwrite_existing, ec);
    if (ec) return app::async::failure<std::string>("复制数据库失败：" + ec.message());

    const fs::path tsv = tmp / "beatmaps.tsv";
    fs::create_directories(gOutDir, ec);

    const Cmd command = quoted(fs::path(dotnetPath())) + CMD_LIT(" exec ") + quoted(helperPath()) + CMD_LIT(" ") +
                        quoted(realmCopyPath()) + CMD_LIT(" ") + quoted(osu / "files") + CMD_LIT(" ") +
                        quoted(gOutDir) + CMD_LIT(" --tsv ") + quoted(tsv);

    const fs::path log = tmp / "scan.log";
    const int code = runCommand(command, log);
    if (code != 0) {
        std::string detail = readAll(log);
        if (detail.size() > 400) detail = detail.substr(detail.size() - 400);
        return app::async::failure<std::string>("读取数据库失败（退出码 " + std::to_string(code) + "）\n" + detail);
    }

    const std::string text = readAll(tsv);
    if (text.empty()) return app::async::failure<std::string>("数据库读取结果为空");
    return app::async::success(text);
}

void startScan() {
    g.scanning = true;
    g.ready = false;
    g.status = "正在读取 osu!lazer 游戏数据库…";
    g.detail.clear();
    app::async::restart(
        "scan",
        [] { return scanDatabase(); },
        [](const app::async::Result<std::string>& result) {
            g.scanning = false;
            if (!result.ok) {
                g.status = result.error;
                g.detail = result.error;
                return;
            }
            parseTsv(result.value);
            g.ready = true;
            g.status = "已读取 " + std::to_string(g.rows.size()) + " 套已导入谱面";
            g.detail = "点击行或复选框选择谱面；导出不会修改游戏数据";
        });
}

app::async::Result<std::string> exportSelected(std::vector<std::string> ids) {
    const fs::path tmp = workDir();
    std::error_code ec;
    fs::create_directories(tmp, ec);
    fs::create_directories(gOutDir, ec);

    const fs::path idsFile = tmp / "selected-ids.txt";
    {
        std::ofstream out(idsFile, std::ios::binary | std::ios::trunc);
        for (const std::string& id : ids) out << id << "\n";
    }

    const Cmd command = quoted(fs::path(dotnetPath())) + CMD_LIT(" exec ") + quoted(helperPath()) + CMD_LIT(" ") +
                        quoted(realmCopyPath()) + CMD_LIT(" ") + quoted(osuDir() / "files") + CMD_LIT(" ") +
                        quoted(gOutDir) + CMD_LIT(" --ids-file ") + quoted(idsFile);

    const fs::path log = tmp / "export.log";
    const int code = runCommand(command, log);
    std::string text = readAll(log);
    if (code != 0) {
        if (text.size() > 500) text = text.substr(text.size() - 500);
        return app::async::failure<std::string>("导出进程失败（退出码 " + std::to_string(code) + "）\n" + text);
    }
    return app::async::success(text);
}

void startExport() {
    std::vector<std::string> ids;
    for (const Row& row : g.rows)
        if (row.selected) ids.push_back(row.id);
    if (ids.empty()) return;

    g.exporting = true;
    g.status = "正在导出 " + std::to_string(ids.size()) + " 套谱面…";
    app::async::restart(
        "export",
        [ids] { return exportSelected(ids); },
        [](const app::async::Result<std::string>& result) {
            g.exporting = false;
            if (!result.ok) {
                g.status = result.error;
                return;
            }
            int ok = 0, fail = 0;
            std::istringstream stream(result.value);
            std::string line;
            while (std::getline(stream, line)) {
                if (line.rfind("OK ", 0) == 0) ++ok;
                else if (line.rfind("FAIL ", 0) == 0) ++fail;
            }
            g.status = "导出完成：" + std::to_string(ok) + " 个 .osz" +
                       (fail > 0 ? "，" + std::to_string(fail) + " 个失败" : "") +
                       " → " + gOutDir.u8string();
        });
}

int compareRows(const Row& a, const Row& b) {
    switch (g.sortKey) {
        case SortKey::Title: {
            const int c = lowerAscii(a.title).compare(lowerAscii(b.title));
            if (c != 0) return c;
            return lowerAscii(a.artist).compare(lowerAscii(b.artist));
        }
        case SortKey::Artist: {
            const int c = lowerAscii(a.artist).compare(lowerAscii(b.artist));
            if (c != 0) return c;
            return lowerAscii(a.title).compare(lowerAscii(b.title));
        }
        case SortKey::Date:
        default:
            return a.date.compare(b.date);
    }
}

std::vector<int> visibleIndices() {
    std::vector<int> indices;
    const std::string needle = lowerAscii(g.search.get());
    for (int i = 0; i < static_cast<int>(g.rows.size()); ++i) {
        if (needle.empty()) {
            indices.push_back(i);
            continue;
        }
        const Row& row = g.rows[i];
        const std::string haystack = lowerAscii(row.artist + " " + row.title + " " + row.creator);
        if (haystack.find(needle) != std::string::npos) indices.push_back(i);
    }

    std::stable_sort(indices.begin(), indices.end(), [](int left, int right) {
        int c = compareRows(g.rows[left], g.rows[right]);
        if (g.descending) c = -c;
        if (c == 0) c = left - right;
        return c < 0;
    });
    return indices;
}

int selectedCount() {
    int count = 0;
    for (const Row& row : g.rows) if (row.selected) ++count;
    return count;
}

void ensureOutputDir() {
    std::error_code ec;
    fs::create_directories(gOutDir, ec);
}

void init() {
#ifdef _WIN32
    const char* profile = std::getenv("USERPROFILE");
    gOutDir = (profile ? fs::path(profile) : fs::path(".")) / "Documents" / "osu-lazer-osz-export";
#else
    const char* home = std::getenv("HOME");
    gOutDir = (home ? fs::path(home) : fs::path(".")) / "osu-lazer-osz-export";
#endif

    std::error_code ec;
    fs::create_directories(workDir(), ec);

    setEnv("DOTNET_CLI_TELEMETRY_OPTOUT", "1");
    setEnv("DOTNET_NOLOGO", "1");
    setEnv("DOTNET_SKIP_FIRST_TIME_EXPERIENCE", "1");
    setEnv("DOTNET_CLI_HOME", workDir().u8string());
#ifndef _WIN32
    const char* ld = std::getenv("LD_LIBRARY_PATH");
    const std::string value = helperDirString() + (ld && *ld ? ":" + std::string(ld) : "");
    setEnv("LD_LIBRARY_PATH", value);
#endif
    ensureOutputDir();
}

const unsigned int kModeIcons[4] = {0xF192, 0xF569, 0xF256, 0xF11C};
const eui::Color kModeColors[4] = {
    {1.00f, 0.40f, 0.67f, 1.0f},
    {1.00f, 0.42f, 0.28f, 1.0f},
    {0.55f, 0.78f, 0.25f, 1.0f},
    {0.80f, 0.44f, 0.86f, 1.0f},
};

} // namespace

void compose(eui::Ui& ui, const eui::Screen& screen) {
    if (!g.started) {
        g.started = true;
        init();
        startScan();
    }

    const components::theme::ThemeColorTokens tokens = components::theme::dark();
    const eui::Color muted = components::theme::withOpacity(tokens.text, 0.62f);
    const eui::Color dim = components::theme::withOpacity(tokens.text, 0.45f);
    const eui::Color border = components::theme::withOpacity(tokens.border, 0.85f);

    const float margin = 22.0f;
    const float gap = 10.0f;
    const float headerHeight = 64.0f;
    const float toolbarHeight = 46.0f;
    const float footerHeight = 28.0f;
    const float contentWidth = std::max(320.0f, screen.width - margin * 2.0f);
    const float listHeight = std::max(160.0f, screen.height - margin * 2.0f - headerHeight -
                                                toolbarHeight * 2.0f - footerHeight - gap * 4.0f);

    const std::vector<int> visible = visibleIndices();
    const int selected = selectedCount();
    const float searchWidth = std::clamp(contentWidth - 3.0f * 88.0f - 4.0f * 8.0f, 200.0f, 460.0f);

    ui.column("root")
        .size(screen.width, screen.height)
        .padding(margin)
        .gap(gap)
        .content([&] {
            ui.row("header")
                .width(contentWidth)
                .height(headerHeight)
                .alignItems(eui::Align::CENTER)
                .content([&] {
                    ui.column("header.text")
                        .width(std::max(280.0f, contentWidth - 240.0f))
                        .height(headerHeight)
                        .gap(2.0f)
                        .content([&] {
                            ui.text("header.title")
                                .height(36.0f)
                                .text("osu!lazer 谱面导出器")
                                .fontSize(28.0f)
                                .lineHeight(36.0f)
                                .color(tokens.text)
                                .build();
                            ui.text("header.status")
                                .height(20.0f)
                                .text(g.status)
                                .fontSize(13.0f)
                                .lineHeight(20.0f)
                                .color(muted)
                                .build();
                        })
                        .build();

                    ui.column("header.count")
                        .width(210.0f)
                        .height(headerHeight)
                        .gap(2.0f)
                        .alignItems(eui::Align::END)
                        .content([&] {
                            ui.text("header.count.value")
                                .height(34.0f)
                                .text(std::to_string(selected) + " / " + std::to_string(g.rows.size()))
                                .fontSize(26.0f)
                                .lineHeight(34.0f)
                                .color(tokens.primary)
                                .horizontalAlign(eui::HorizontalAlign::Right)
                                .build();
                            ui.text("header.count.label")
                                .height(16.0f)
                                .text("已选择 / 已导入")
                                .fontSize(12.0f)
                                .lineHeight(16.0f)
                                .color(muted)
                                .horizontalAlign(eui::HorizontalAlign::Right)
                                .build();
                        })
                        .build();
                })
                .build();

            ui.row("toolbar.search")
                .width(contentWidth)
                .height(toolbarHeight)
                .gap(8.0f)
                .alignItems(eui::Align::CENTER)
                .content([&] {
                    components::input(ui, "toolbar.input")
                        .size(searchWidth, 40.0f)
                        .placeholder("搜索标题 / 艺术家 / 谱师…")
                        .fontSize(15.0f)
                        .theme(tokens)
                        .value(g.search.get())
                        .onChange([](const std::string& value) {
                            g.search.set(value);
                            app::requestUpdate();
                        })
                        .build();

                    components::button(ui, "toolbar.selectall")
                        .size(88.0f, 40.0f)
                        .text("全选")
                        .fontSize(14.0f)
                        .theme(tokens, false)
                        .disabled(g.rows.empty())
                        .onClick([visible] {
                            for (int index : visible) g.rows[index].selected = true;
                            app::requestUpdate();
                        })
                        .build();

                    components::button(ui, "toolbar.clear")
                        .size(88.0f, 40.0f)
                        .text("清空")
                        .fontSize(14.0f)
                        .theme(tokens, false)
                        .disabled(selected == 0)
                        .onClick([] {
                            for (Row& row : g.rows) row.selected = false;
                            app::requestUpdate();
                        })
                        .build();

                    components::button(ui, "toolbar.refresh")
                        .size(88.0f, 40.0f)
                        .text(g.scanning ? "读取中…" : "刷新")
                        .fontSize(14.0f)
                        .theme(tokens, false)
                        .disabled(g.scanning || g.exporting)
                        .onClick([] {
                            if (!g.scanning && !g.exporting) startScan();
                        })
                        .build();
                })
                .build();

            ui.row("toolbar.sort")
                .width(contentWidth)
                .height(toolbarHeight)
                .gap(8.0f)
                .alignItems(eui::Align::CENTER)
                .content([&] {
                    ui.text("sort.label")
                        .width(40.0f)
                        .height(toolbarHeight)
                        .text("排序")
                        .fontSize(14.0f)
                        .lineHeight(toolbarHeight)
                        .color(muted)
                        .build();

                    const auto sortButton = [&](const char* id, const char* label, float width, SortKey key) {
                        components::button(ui, id)
                            .size(width, 40.0f)
                            .text(label)
                            .fontSize(14.0f)
                            .theme(tokens, g.sortKey == key)
                            .onClick([key] {
                                if (g.sortKey == key) g.descending = !g.descending;
                                else { g.sortKey = key; g.descending = key == SortKey::Date; }
                                app::requestUpdate();
                            })
                            .build();
                    };
                    sortButton("sort.title", "标题", 84.0f, SortKey::Title);
                    sortButton("sort.artist", "艺术家", 96.0f, SortKey::Artist);
                    sortButton("sort.date", "导入时间", 110.0f, SortKey::Date);

                    components::button(ui, "sort.direction")
                        .size(102.0f, 40.0f)
                        .text(g.descending ? "↓ 降序" : "↑ 升序")
                        .fontSize(14.0f)
                        .theme(tokens, false)
                        .onClick([] {
                            g.descending = !g.descending;
                            app::requestUpdate();
                        })
                        .build();

                    ui.rect("sort.spacer")
                        .flexGrow(1.0f)
                        .height(1.0f)
                        .color(eui::Color{0.0f, 0.0f, 0.0f, 0.0f})
                        .build();

                    components::button(ui, "toolbar.export")
                        .size(156.0f, 40.0f)
                        .text(g.exporting ? "导出中…" : "导出选中 .osz")
                        .fontSize(14.0f)
                        .theme(tokens, true)
                        .disabled(g.exporting || g.scanning || selected == 0)
                        .onClick([] {
                            if (!g.exporting && !g.scanning) startExport();
                        })
                        .build();

                    components::button(ui, "toolbar.folder")
                        .size(112.0f, 40.0f)
                        .text("打开目录")
                        .fontSize(14.0f)
                        .theme(tokens, false)
                        .onClick([] {
                            ensureOutputDir();
                            openFolder(gOutDir);
                        })
                        .build();
                })
                .build();

            ui.stack("panel")
                .size(contentWidth, listHeight)
                .content([&] {
                    ui.rect("panel.background")
                        .size(contentWidth, listHeight)
                        .color(tokens.surface)
                        .radius(12.0f)
                        .border(1.0f, border)
                        .build();

                    components::scrollView(ui, "list")
                        .position(1.0f, 1.0f)
                        .size(std::max(0.0f, contentWidth - 2.0f), std::max(0.0f, listHeight - 2.0f))
                        .theme(tokens)
                        .bind(g.scroll)
                        .step(76.0f)
                        .scrollbarWidth(8.0f)
                        .scrollbarGap(8.0f)
                        .contentKey("list." + std::to_string(static_cast<int>(g.sortKey)) +
                                    (g.descending ? "d" : "a") + "." + std::to_string(visible.size()) +
                                    "." + g.search.get())
                        .content([&](eui::Ui& body, float width, float) {
                            const float rowHeight = 76.0f;
                            const float total = rowHeight * static_cast<float>(visible.size());
                            body.stack("list.canvas")
                                .size(width, std::max(total, rowHeight))
                                .content([&] {
                                    for (std::size_t k = 0; k < visible.size(); ++k) {
                                        const int index = visible[k];
                                        const Row& row = g.rows[index];
                                        const float y = rowHeight * static_cast<float>(k);
                                        const eui::Color rowColor = row.selected
                                            ? eui::mixColor(tokens.surfaceHover, tokens.primary, 0.18f)
                                            : (k % 2 == 0 ? tokens.surface : tokens.surfaceHover);

                                        body.rect("row." + row.id)
                                            .position(0.0f, y)
                                            .size(width, rowHeight)
                                            .color(rowColor)
                                            .build();

                                        const float box = 22.0f;
                                        const float boxX = 16.0f;
                                        const float boxY = y + (rowHeight - box) * 0.5f;
                                        body.rect("check." + row.id)
                                            .position(boxX, boxY)
                                            .size(box, box)
                                            .radius(5.0f)
                                            .color(row.selected ? tokens.primary : tokens.surface)
                                            .border(1.4f, row.selected ? tokens.primary : border)
                                            .build();
                                        if (row.selected) {
                                            body.rect("check.mark." + row.id)
                                                .position(boxX + 6.0f, boxY + 6.0f)
                                                .size(box - 12.0f, box - 12.0f)
                                                .radius(2.0f)
                                                .color(tokens.text)
                                                .build();
                                        }

                                        body.text("row.title." + row.id)
                                            .position(52.0f, y + 10.0f)
                                            .size(std::max(160.0f, width - 380.0f), 26.0f)
                                            .text(row.title)
                                            .fontSize(18.0f)
                                            .lineHeight(26.0f)
                                            .color(tokens.text)
                                            .build();

                                        body.text("row.artist." + row.id)
                                            .position(52.0f, y + 36.0f)
                                            .size(std::max(160.0f, width - 380.0f), 18.0f)
                                            .text(row.artist)
                                            .fontSize(13.5f)
                                            .lineHeight(18.0f)
                                            .color(muted)
                                            .build();

                                        body.text("row.meta." + row.id)
                                            .position(52.0f, y + 56.0f)
                                            .size(std::max(160.0f, width - 380.0f), 16.0f)
                                            .text(row.creator + " · " + std::to_string(row.files) + " 文件 · " +
                                                  sizeText(row.bytes))
                                            .fontSize(11.5f)
                                            .lineHeight(16.0f)
                                            .color(dim)
                                            .build();

                                        body.text("row.date." + row.id)
                                            .position(std::max(0.0f, width - 330.0f), y + 12.0f)
                                            .size(310.0f, 16.0f)
                                            .text(row.date)
                                            .fontSize(12.0f)
                                            .lineHeight(16.0f)
                                            .color(dim)
                                            .horizontalAlign(eui::HorizontalAlign::Right)
                                            .build();

                                        float badgeX = std::max(0.0f, width - 330.0f);
                                        for (int mode = 0; mode < 4; ++mode) {
                                            if (row.modes[mode] <= 0) continue;
                                            body.text("row.mode." + row.id + "." + std::to_string(mode))
                                                .position(badgeX, y + 42.0f)
                                                .size(20.0f, 20.0f)
                                                .icon(kModeIcons[mode])
                                                .fontSize(16.0f)
                                                .lineHeight(20.0f)
                                                .color(kModeColors[mode])
                                                .build();
                                            body.text("row.modecount." + row.id + "." + std::to_string(mode))
                                                .position(badgeX + 22.0f, y + 42.0f)
                                                .size(24.0f, 20.0f)
                                                .text(std::to_string(row.modes[mode]))
                                                .fontSize(13.0f)
                                                .lineHeight(20.0f)
                                                .color(muted)
                                                .build();
                                            badgeX += 48.0f;
                                        }

                                        if (k + 1 < visible.size()) {
                                            body.rect("row.divider." + row.id)
                                                .position(0.0f, y + rowHeight - 1.0f)
                                                .size(width, 1.0f)
                                                .color(components::theme::withOpacity(border, 0.5f))
                                                .build();
                                        }

                                        body.rect("row.hit." + row.id)
                                            .position(0.0f, y)
                                            .size(width, rowHeight)
                                            .color(eui::Color{0.0f, 0.0f, 0.0f, 0.0f})
                                            .onClick([index] {
                                                g.rows[index].selected = !g.rows[index].selected;
                                                app::requestUpdate();
                                            })
                                            .build();
                                    }
                                })
                                .build();
                        })
                        .build();
                })
                .build();

            ui.text("footer")
                .width(contentWidth)
                .height(footerHeight)
                .text(g.detail.empty()
                          ? (g.ready ? "点击行或复选框选择谱面，然后导出为 .osz。" : " ")
                          : g.detail)
                .fontSize(12.0f)
                .lineHeight(footerHeight)
                .color(dim)
                .build();
        })
        .build();
}

} // namespace app


