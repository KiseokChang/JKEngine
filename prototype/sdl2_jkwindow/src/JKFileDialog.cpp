#include <JKFileDialog.h>
#include <JKApplication.h>
#include <JKButton.h>
#include <JKListBox.h>
#include <JKEdit.h>
#include <JKStatic.h>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cstdio>

namespace jk {

namespace fs = std::filesystem;

namespace {

std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

bool IEquals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

bool MatchFilter(const std::string& name, const std::string& filter) {
    std::string f = Trim(filter);
    if (f.empty() || f == "*" || f == "*.*") return true;

    std::string ext;
    size_t dot = name.find_last_of('.');
    if (dot != std::string::npos) ext = name.substr(dot);

    size_t start = 0;
    while (start < f.size()) {
        size_t sep = f.find(';', start);
        std::string pat = (sep == std::string::npos) ? f.substr(start) : f.substr(start, sep - start);
        pat = Trim(pat);
        if (pat == "*" || pat == "*.*") return true;
        if (!pat.empty() && pat[0] == '*') {
            std::string suffix = pat.substr(1);
            if (suffix.empty()) return true;
            if (ext.size() >= suffix.size() &&
                IEquals(ext.substr(ext.size() - suffix.size()), suffix)) {
                return true;
            }
        } else if (IEquals(name, pat)) {
            return true;
        }
        start = (sep == std::string::npos) ? f.size() : sep + 1;
    }
    return false;
}

} // anonymous namespace

JKFileDialog::JKFileDialog() : JKFileDialog("Open") {
}

JKFileDialog::JKFileDialog(const std::string& title) : JKWindow(title) {
    SetWindowRect(JKRect{ 120, 90, 400, 300 });
    SetAttrFlags(WA_TITLEMOVEABLE);
    OnInitControls();
    if (currentDir_.empty()) {
        currentDir_ = fs::current_path().string();
    }
}
void JKFileDialog::OnInitControls() {
    const JKRect client = GetClientRect();

    auto listBox = std::make_unique<JKListBox>(JKRect{ 10, 10, client.w - 20, client.h - 80 }, 101);
    listBox_ = listBox.get();
    listBox_->SetOnSelect([this](int32_t index) {
        if (index < 0 || index >= static_cast<int32_t>(listBox_->GetCount())) return;
        std::string name = listBox_->GetString(index);
        if (!name.empty() && name.back() == '/') {
            name.pop_back();
        }
        fileEdit_->SetText(name);
    });
    AddControl(std::move(listBox));

    auto fileEdit = std::make_unique<JKEdit>(JKRect{ 10, client.h - 60, client.w - 120, 22 }, 102, 256);
    fileEdit_ = fileEdit.get();
    AddControl(std::move(fileEdit));

    auto okBtn = std::make_unique<JKButton>(JKRect{ client.w - 100, client.h - 60, 80, 22 }, 103);
    okBtn->SetText("OK");
    okBtn->SetOnClick([this]() { OnOk(); });
    AddControl(std::move(okBtn));

    auto cancelBtn = std::make_unique<JKButton>(JKRect{ client.w - 100, client.h - 30, 80, 22 }, 104);
    cancelBtn->SetText("Cancel");
    cancelBtn->SetOnClick([this]() { OnCancel(); });
    AddControl(std::move(cancelBtn));
}

void JKFileDialog::RefreshList() {
    if (!listBox_) return;
    listBox_->Clear();

    try {
        fs::path dir(currentDir_);
        if (!fs::exists(dir) || !fs::is_directory(dir)) {
            std::fprintf(stderr, "JKFileDialog: directory does not exist: %s\n", currentDir_.c_str());
            return;
        }

        std::vector<fs::directory_entry> entries;
        for (const auto& entry : fs::directory_iterator(dir, fs::directory_options::skip_permission_denied)) {
            entries.push_back(entry);
        }
        std::sort(entries.begin(), entries.end(), [](const fs::directory_entry& a, const fs::directory_entry& b) {
            if (a.is_directory() != b.is_directory()) return a.is_directory();
            return a.path().filename().string() < b.path().filename().string();
        });

        if (dir.has_parent_path() && dir.parent_path() != dir) {
            listBox_->AddString("..");
        }

        for (const auto& entry : entries) {
            std::string name = entry.path().filename().string();
            if (entry.is_directory()) {
                listBox_->AddString(name + "/");
            } else if (MatchFilter(name, filter_)) {
                listBox_->AddString(name);
            }
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "JKFileDialog::RefreshList failed: %s\n", e.what());
    }
}

void JKFileDialog::OnOk() {
    std::string name = Trim(fileEdit_->GetText());
    if (name.empty() && listBox_->GetSelectedIndex() >= 0) {
        name = Trim(listBox_->GetString(listBox_->GetSelectedIndex()));
    }
    if (name.empty()) return;
    if (!name.empty() && name.back() == '/') {
        name.pop_back();
    }

    fs::path selected = fs::path(currentDir_) / name;

    if (name == "..") {
        fs::path dir(currentDir_);
        if (dir.has_parent_path() && dir.parent_path() != dir) {
            currentDir_ = dir.parent_path().string();
            fileEdit_->SetText("");
            RefreshList();
        }
        return;
    }

    if (fs::is_directory(selected)) {
        currentDir_ = selected.string();
        fileEdit_->SetText("");
        RefreshList();
        return;
    }

    fileName_ = selected.string();
    if (onOk_) onOk_(fileName_);

    RequestClose();
    if (g_currentJKApp) g_currentJKApp->SetModalWindow(nullptr);
}

void JKFileDialog::OnCancel() {
    if (onCancel_) onCancel_();
    RequestClose();
    if (g_currentJKApp) g_currentJKApp->SetModalWindow(nullptr);
}

void JKFileDialog::Show() {
    if (currentDir_.empty()) {
        currentDir_ = fs::current_path().string();
    }
    RefreshList();
    if (g_currentJKApp) g_currentJKApp->SetModalWindow(this);
    Open();
}

void JKFileDialog::RespondMessage(const JKEvent& ev) {
    if (ev.type == JKEventType::KeyDown) {
        if (ev.keyCode == SDLK_ESCAPE) {
            OnCancel();
            return;
        }
        if (ev.keyCode == SDLK_RETURN || ev.keyCode == SDLK_KP_ENTER) {
            OnOk();
            return;
        }
    }
    JKWindow::RespondMessage(ev);
    if (IsCloseRequested() && g_currentJKApp && g_currentJKApp->GetModalWindow() == this) {
        g_currentJKApp->SetModalWindow(nullptr);
    }
}

} // namespace jk

