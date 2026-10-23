#include "TagDialog.h"

#include <wx/button.h>
#include <wx/font.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/tokenzr.h>

#ifdef _WIN32
#include <windows.h>
#include <dwmapi.h>
#include <uxtheme.h>
#endif

#include <algorithm>

namespace {

wxString FromUtf8(const std::string& value) {
    return wxString::FromUTF8(value.c_str());
}

std::vector<wxString> SplitTerms(wxString filter) {
    filter.MakeLower();
    std::vector<wxString> result;
    wxStringTokenizer tokenizer(filter, wxString(L" \t"));
    while (tokenizer.HasMoreTokens()) result.push_back(tokenizer.GetNextToken());
    return result;
}

#ifdef _WIN32
void SetNativeDarkMode(wxWindow* window, bool dark) {
    if (!window || !window->GetHandle()) return;
    HWND hwnd = reinterpret_cast<HWND>(window->GetHandle());
    const BOOL value = dark ? TRUE : FALSE;
    constexpr DWORD kImmersiveDarkMode = 20;
    DwmSetWindowAttribute(hwnd, kImmersiveDarkMode, &value, sizeof(value));
}
#endif

} // namespace

TagDialog::TagDialog(wxWindow* parent, UiLanguage language, UiTheme theme)
    : wxDialog(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(1000, 700),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      language_(language),
      theme_(theme),
      allTags_(DicomUtils::LoadDictionary())
{
    SetFont(wxFont(wxFontInfo(10).FaceName(wxString(L"Segoe UI"))));
    SetTitle(L(L"DICOM-Tag auswählen", L"Select DICOM tag"));

    auto* root = new wxBoxSizer(wxVERTICAL);

    title_ = new wxStaticText(this, wxID_ANY,
        L(L"DICOM-Tag auswählen", L"Select a DICOM tag"));
    wxFont titleFont = title_->GetFont();
    titleFont.SetPointSize(titleFont.GetPointSize() + 3);
    titleFont.MakeBold();
    title_->SetFont(titleFont);
    root->Add(title_, 0, wxLEFT | wxRIGHT | wxTOP, 18);

    hint_ = new wxStaticText(this, wxID_ANY,
        L(L"Suche nach Tagnummer, Name, VR oder VM.", L"Search by tag number, name, VR or VM."));
    root->Add(hint_, 0, wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 18);

    filter_ = new wxTextCtrl(this, wxID_ANY);
    filter_->SetHint(L(L"z. B. 0018 1030 Protocol Name SQ", L"e.g. 0018 1030 Protocol Name SQ"));
    root->Add(filter_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 14);

    list_ = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                           wxLC_REPORT | wxLC_SINGLE_SEL);
    list_->AppendColumn(wxString(L"Tag"), wxLIST_FORMAT_LEFT, 170);
    list_->AppendColumn(L(L"Name / Keyword", L"Name / Keyword"), wxLIST_FORMAT_LEFT, 470);
    list_->AppendColumn(wxString(L"VR"), wxLIST_FORMAT_LEFT, 70);
    list_->AppendColumn(wxString(L"VM"), wxLIST_FORMAT_LEFT, 80);
    list_->AppendColumn(L(L"Standard", L"Standard"), wxLIST_FORMAT_LEFT, 130);
    root->Add(list_, 1, wxEXPAND | wxLEFT | wxRIGHT, 18);

    count_ = new wxStaticText(this, wxID_ANY, wxEmptyString);
    root->Add(count_, 0, wxLEFT | wxRIGHT | wxTOP, 18);

    auto* buttons = new wxBoxSizer(wxHORIZONTAL);
    buttons->AddStretchSpacer();
    auto* choose = new wxButton(this, wxID_OK, L(L"Tag übernehmen", L"Use tag"));
    choose->SetMinSize(wxSize(130, 34));
    auto* cancel = new wxButton(this, wxID_CANCEL, L(L"Abbrechen", L"Cancel"));
    cancel->SetMinSize(wxSize(105, 34));
    buttons->Add(choose, 0, wxALL, 6);
    buttons->Add(cancel, 0, wxALL, 6);
    root->Add(buttons, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    SetSizer(root);
    ApplyTheme();
    RebuildList();

    filter_->Bind(wxEVT_TEXT, &TagDialog::OnFilter, this);
    list_->Bind(wxEVT_LIST_ITEM_ACTIVATED, &TagDialog::OnActivate, this);
    choose->Bind(wxEVT_BUTTON, &TagDialog::OnChoose, this);
    filter_->SetFocus();
}

wxString TagDialog::L(const wchar_t* de, const wchar_t* en) const {
    return wxString(language_ == UiLanguage::German ? de : en);
}

void TagDialog::ApplyTheme() {
    const bool dark = theme_ == UiTheme::Dark;
    const wxColour background = dark ? wxColour(24, 27, 32) : wxColour(244, 246, 249);
    const wxColour table = dark ? wxColour(28, 32, 38) : wxColour(255, 255, 255);
    const wxColour alternate = dark ? wxColour(34, 39, 46) : wxColour(248, 250, 252);
    const wxColour input = dark ? wxColour(42, 47, 55) : wxColour(255, 255, 255);
    const wxColour text = dark ? wxColour(235, 238, 242) : wxColour(30, 35, 42);
    const wxColour muted = dark ? wxColour(164, 173, 184) : wxColour(100, 109, 120);

    SetBackgroundColour(background);
    title_->SetForegroundColour(text);
    hint_->SetForegroundColour(muted);
    count_->SetForegroundColour(muted);
    filter_->SetBackgroundColour(input);
    filter_->SetForegroundColour(text);
    list_->SetBackgroundColour(table);
    list_->SetForegroundColour(text);

#ifdef _WIN32
    SetWindowTheme(reinterpret_cast<HWND>(list_->GetHandle()), dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    SetNativeDarkMode(this, dark);
#endif

    for (long row = 0; row < list_->GetItemCount(); ++row) {
        list_->SetItemBackgroundColour(row, (row % 2) ? alternate : table);
        list_->SetItemTextColour(row, text);
    }
    Refresh();
}

bool TagDialog::HasSelection() const { return hasSelection_; }
TagEntry TagDialog::SelectedTag() const { return selected_; }

bool TagDialog::Matches(const TagEntry& tag, const wxString& filter) const {
    wxString trimmed = filter;
    trimmed.Trim(true).Trim(false);
    if (trimmed.empty()) return true;

    wxString haystack = FromUtf8(tag.tagDisplay + " " + tag.name + " " + tag.vr + " " + tag.vm + " " + tag.version);
    haystack.Replace(wxString(L"("), wxString(L" "));
    haystack.Replace(wxString(L")"), wxString(L" "));
    haystack.Replace(wxString(L","), wxString(L" "));
    haystack.Replace(wxString(L"-"), wxString(L" "));
    haystack.MakeLower();

    for (const auto& term : SplitTerms(trimmed)) {
        if (haystack.Find(term) == wxNOT_FOUND) return false;
    }
    return true;
}

void TagDialog::RebuildList() {
    const wxString query = filter_ ? filter_->GetValue() : wxString{};
    list_->Freeze();
    list_->DeleteAllItems();
    visible_.clear();

    const bool dark = theme_ == UiTheme::Dark;
    const wxColour table = dark ? wxColour(28, 32, 38) : wxColour(255, 255, 255);
    const wxColour alternate = dark ? wxColour(34, 39, 46) : wxColour(248, 250, 252);
    const wxColour text = dark ? wxColour(235, 238, 242) : wxColour(30, 35, 42);

    for (std::size_t i = 0; i < allTags_.size(); ++i) {
        const auto& tag = allTags_[i];
        if (!Matches(tag, query)) continue;

        const long row = list_->InsertItem(list_->GetItemCount(), FromUtf8(tag.tagDisplay));
        list_->SetItem(row, 1, FromUtf8(tag.name));
        list_->SetItem(row, 2, FromUtf8(tag.vr));
        list_->SetItem(row, 3, FromUtf8(tag.vm));
        list_->SetItem(row, 4, FromUtf8(tag.version));
        list_->SetItemBackgroundColour(row, (row % 2) ? alternate : table);
        list_->SetItemTextColour(row, text);
        visible_.push_back(i);
    }
    list_->Thaw();

    if (language_ == UiLanguage::German) {
        count_->SetLabel(wxString::Format(L"%zu von %zu Dictionary-Einträgen", visible_.size(), allTags_.size()));
    } else {
        count_->SetLabel(wxString::Format(L"%zu of %zu dictionary entries", visible_.size(), allTags_.size()));
    }
}

void TagDialog::OnFilter(wxCommandEvent&) { RebuildList(); }

void TagDialog::OnActivate(wxListEvent&) {
    wxCommandEvent dummy;
    OnChoose(dummy);
}

void TagDialog::OnChoose(wxCommandEvent&) {
    const long row = list_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (row < 0 || static_cast<std::size_t>(row) >= visible_.size()) {
        wxMessageBox(L(L"Bitte zuerst einen DICOM-Tag auswählen.", L"Please select a DICOM tag first."),
                     L(L"Keine Auswahl", L"No selection"), wxOK | wxICON_INFORMATION, this);
        return;
    }
    selected_ = allTags_[visible_[static_cast<std::size_t>(row)]];
    hasSelection_ = true;
    EndModal(wxID_OK);
}
