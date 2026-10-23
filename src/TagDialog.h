#pragma once

#include "Models.h"

#include <wx/dialog.h>
#include <wx/listctrl.h>
#include <vector>

class wxStaticText;
class wxTextCtrl;

class TagDialog : public wxDialog {
public:
    explicit TagDialog(wxWindow* parent);

    bool HasSelection() const;
    TagEntry SelectedTag() const;

private:
    void RebuildList();
    void OnFilter(wxCommandEvent& event);
    void OnActivate(wxListEvent& event);
    void OnChoose(wxCommandEvent& event);
    bool Matches(const TagEntry& tag, const wxString& filter) const;

    wxTextCtrl* filter_{};
    wxListCtrl* list_{};
    wxStaticText* count_{};
    std::vector<TagEntry> allTags_;
    std::vector<std::size_t> visible_;
    bool hasSelection_{};
    TagEntry selected_;
};
