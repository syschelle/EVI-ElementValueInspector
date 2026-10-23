#pragma once

#include "DicomUtils.h"
#include "UiLanguage.h"
#include "UiTheme.h"

#include <wx/dialog.h>
#include <wx/listctrl.h>

class wxStaticText;
class wxTextCtrl;

class TagDialog final : public wxDialog {
public:
    TagDialog(wxWindow* parent, UiLanguage language, UiTheme theme);

    bool HasSelection() const;
    TagEntry SelectedTag() const;

private:
    wxString L(const wchar_t* de, const wchar_t* en) const;
    bool Matches(const TagEntry& tag, const wxString& filter) const;
    void RebuildList();
    void ApplyTheme();
    void OnFilter(wxCommandEvent& event);
    void OnActivate(wxListEvent& event);
    void OnChoose(wxCommandEvent& event);

    UiLanguage language_;
    UiTheme theme_;
    wxStaticText* title_{};
    wxStaticText* hint_{};
    wxTextCtrl* filter_{};
    wxListCtrl* list_{};
    wxStaticText* count_{};
    std::vector<TagEntry> allTags_;
    std::vector<std::size_t> visible_;
    TagEntry selected_{};
    bool hasSelection_{false};
};
