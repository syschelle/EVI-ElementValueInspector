#include "TagDialog.h"

#include <wx/button.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/tokenzr.h>

#include <algorithm>

namespace {

wxString FromUtf8(const std::string& value) {
    return wxString::FromUTF8(value.c_str());
}

std::vector<wxString> SplitTerms(wxString filter) {
    filter.MakeLower();
    std::vector<wxString> result;
    wxStringTokenizer tokenizer(filter, " \t");
    while (tokenizer.HasMoreTokens()) result.push_back(tokenizer.GetNextToken());
    return result;
}

void SetColumnTitle(wxListCtrl* list, int column, const wxString& title) {
    wxListItem item;
    item.SetMask(wxLIST_MASK_TEXT);
    item.SetText(title);
    list->SetColumn(column, item);
}

} // namespace

TagDialog::TagDialog(wxWindow* parent, UiLanguage language)
    : wxDialog(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(980, 680),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      language_(language),
      allTags_(DicomUtils::LoadDictionary())
{
    SetTitle(L("DICOM-Tag auswählen", "Select DICOM Tag"));
    SetBackgroundColour(wxColour(247, 248, 250));

    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* title = new wxStaticText(this, wxID_ANY,
        L("Standard-DICOM-Tags aus dem eingebetteten DCMTK-Dictionary",
          "Standard DICOM tags from the embedded DCMTK dictionary"));
    wxFont titleFont = title->GetFont();
    titleFont.SetPointSize(titleFont.GetPointSize() + 2);
    titleFont.MakeBold();
    title->SetFont(titleFont);
    root->Add(title, 0, wxLEFT | wxRIGHT | wxTOP, 16);

    auto* hint = new wxStaticText(this, wxID_ANY,
        L("Suche nach Tagnummer, Name, VR oder VM.", "Search by tag number, name, VR or VM."));
    hint->SetForegroundColour(wxColour(95, 99, 104));
    root->Add(hint, 0, wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 16);

    filter_ = new wxTextCtrl(this, wxID_ANY);
    filter_->SetHint(L("z. B. 0018 1030 Protocol Name SQ", "e.g. 0018 1030 Protocol Name SQ"));
    root->Add(filter_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);

    list_ = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                           wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES | wxLC_VRULES);
    list_->AppendColumn("Tag", wxLIST_FORMAT_LEFT, 170);
    list_->AppendColumn(L("Name / Keyword", "Name / Keyword"), wxLIST_FORMAT_LEFT, 450);
    list_->AppendColumn("VR", wxLIST_FORMAT_LEFT, 70);
    list_->AppendColumn("VM", wxLIST_FORMAT_LEFT, 80);
    list_->AppendColumn(L("Standard", "Standard"), wxLIST_FORMAT_LEFT, 130);
    root->Add(list_, 1, wxEXPAND | wxLEFT | wxRIGHT, 16);

    count_ = new wxStaticText(this, wxID_ANY, "");
    count_->SetForegroundColour(wxColour(95, 99, 104));
    root->Add(count_, 0, wxLEFT | wxRIGHT | wxTOP, 16);

    auto* buttons = new wxBoxSizer(wxHORIZONTAL);
    buttons->AddStretchSpacer();
    auto* choose = new wxButton(this, wxID_OK, L("Tag übernehmen", "Use Tag"));
    auto* cancel = new wxButton(this, wxID_CANCEL, L("Abbrechen", "Cancel"));
    buttons->Add(choose, 0, wxALL, 6);
    buttons->Add(cancel, 0, wxALL, 6);
    root->Add(buttons, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    SetSizer(root);
    RebuildList();

    filter_->Bind(wxEVT_TEXT, &TagDialog::OnFilter, this);
    list_->Bind(wxEVT_LIST_ITEM_ACTIVATED, &TagDialog::OnActivate, this);
    choose->Bind(wxEVT_BUTTON, &TagDialog::OnChoose, this);
    filter_->SetFocus();
}

wxString TagDialog::L(const char* de, const char* en) const {
    return wxString::FromUTF8(language_ == UiLanguage::German ? de : en);
}

bool TagDialog::HasSelection() const { return hasSelection_; }
TagEntry TagDialog::SelectedTag() const { return selected_; }

bool TagDialog::Matches(const TagEntry& tag, const wxString& filter) const {
    wxString trimmed = filter;
    trimmed.Trim(true).Trim(false);
    if (trimmed.empty()) return true;

    wxString haystack = FromUtf8(tag.tagDisplay + " " + tag.name + " " + tag.vr + " " + tag.vm + " " + tag.version);
    haystack.Replace("(", " ");
    haystack.Replace(")", " ");
    haystack.Replace(",", " ");
    haystack.Replace("-", " ");
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

    for (std::size_t i = 0; i < allTags_.size(); ++i) {
        const auto& tag = allTags_[i];
        if (!Matches(tag, query)) continue;

        const long row = list_->InsertItem(list_->GetItemCount(), FromUtf8(tag.tagDisplay));
        list_->SetItem(row, 1, FromUtf8(tag.name));
        list_->SetItem(row, 2, FromUtf8(tag.vr));
        list_->SetItem(row, 3, FromUtf8(tag.vm));
        list_->SetItem(row, 4, FromUtf8(tag.version));
        visible_.push_back(i);
    }
    list_->Thaw();

    if (language_ == UiLanguage::German) {
        count_->SetLabel(wxString::Format("%zu von %zu Dictionary-Einträgen", visible_.size(), allTags_.size()));
    } else {
        count_->SetLabel(wxString::Format("%zu of %zu dictionary entries", visible_.size(), allTags_.size()));
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
        wxMessageBox(L("Bitte zuerst einen DICOM-Tag auswählen.", "Please select a DICOM tag first."),
                     L("Keine Auswahl", "No selection"), wxOK | wxICON_INFORMATION, this);
        return;
    }
    selected_ = allTags_[visible_[static_cast<std::size_t>(row)]];
    hasSelection_ = true;
    EndModal(wxID_OK);
}
