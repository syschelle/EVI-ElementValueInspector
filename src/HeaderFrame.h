#pragma once

#include "Models.h"
#include "UiLanguage.h"
#include "UiTheme.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <wx/frame.h>
#include <wx/treelist.h>

class DcmItem;
class wxButton;
class wxPanel;
class wxStaticText;
class wxTextCtrl;

class HeaderFrame final : public wxFrame {
public:
    HeaderFrame(wxWindow* parent,
                const ScanHit& sourceHit,
                UiLanguage language,
                UiTheme theme);

private:
    struct HeaderRow {
        uint16_t group{};
        uint16_t element{};
        std::string tag;
        std::string name;
        std::string vr;
        std::string value;
        std::string sequencePath;
        bool fileMeta{};
        bool sequence{};
    };

    wxString L(const wchar_t* de, const wchar_t* en) const;
    void BuildUi();
    void ApplyTheme();
    void LoadHeader();
    void CollectRows(DcmItem* container, const std::string& prefix, bool fileMeta);
    void PopulateTree();
    void SelectInitialMatch();
    void RunSearch(bool advance);
    void SelectRow(std::size_t row);
    bool RowMatchesSearch(const HeaderRow& row, const wxString& query) const;
    void UpdateSearchStatus();

    void OnSearch(wxCommandEvent& event);
    void OnFindNext(wxCommandEvent& event);

    UiLanguage language_;
    UiTheme theme_;
    std::filesystem::path filePath_;
    std::string initialTag_;
    std::string initialValue_;
    std::string initialSequencePath_;
    std::string seriesNumber_;
    std::string instanceNumber_;
    std::string modality_;

    wxPanel* rootPanel_{};
    wxStaticText* title_{};
    wxStaticText* context_{};
    wxStaticText* file_{};
    wxStaticText* searchLabel_{};
    wxTextCtrl* search_{};
    wxButton* findNext_{};
    wxStaticText* hint_{};
    wxTreeListCtrl* tree_{};
    wxStaticText* status_{};

    std::vector<HeaderRow> rows_;
    std::vector<wxTreeListItem> rowItems_;
    std::vector<std::size_t> searchMatches_;
    std::size_t searchPosition_{};
    wxString lastQuery_;
};
