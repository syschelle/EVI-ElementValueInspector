#pragma once

#include "DicomUtils.h"
#include "UiLanguage.h"

#include <wx/dialog.h>
#include <wx/listctrl.h>

class wxStaticText;
class wxTextCtrl;

class TagDialog final : public wxDialog {
public:
    TagDialog(wxWindow* parent, UiLanguage language);

    bool HasSelection() const;
    TagEntry SelectedTag() const;

private:
    wxString L(const char* de, const char* en) const;
    bool Matches(const TagEntry& tag, const wxString& filter) const;
    void RebuildList();
    void OnFilter(wxCommandEvent& event);
    void OnActivate(wxListEvent& event);
    void OnChoose(wxCommandEvent& event);

    UiLanguage language_;
    wxTextCtrl* filter_{};
    wxListCtrl* list_{};
    wxStaticText* count_{};
    std::vector<TagEntry> allTags_;
    std::vector<std::size_t> visible_;
    TagEntry selected_{};
    bool hasSelection_{false};
};
