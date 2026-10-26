#pragma once

#include "Models.h"
#include "UiLanguage.h"
#include "UiTheme.h"

#include <atomic>
#include <filesystem>
#include <memory>
#include <thread>

#include <wx/frame.h>
#include <wx/listctrl.h>

class wxButton;
class wxChoice;
class wxGauge;
class wxPanel;
class wxSplitterWindow;
class wxStaticText;
class wxTextCtrl;
class wxThreadEvent;

class MainFrame : public wxFrame {
public:
    MainFrame();
    ~MainFrame() override;

private:
    void BuildUi();
    void BindEvents();
    void ApplyLanguage();
    void ApplyTheme();
    void ApplyNativeChrome();
    void ApplyListRowTheme(wxListCtrl* list);
    void StartScan();
    void StopWorker();
    void PopulateResult();
    void PopulatePatients();
    void PopulateStudies();
    void PopulateSeries();
    void PopulateHits(const std::string& seriesKey = {});
    void UpdateStudyLabels();
    void RefreshSelectedView();
    bool MatchesSelection(const std::string& patientKey, const std::string& studyKey) const;
    void UpdateStatusSummary();
    void ExportCsv();
    wxString L(const wchar_t* de, const wchar_t* en) const;
    void SetColumnTitle(wxListCtrl* list, int column, const wxString& title);
    void SetStudyValue(wxStaticText* label, const std::string& value);

    void OnChooseFolder(wxCommandEvent& event);
    void OnChooseTag(wxCommandEvent& event);
    void OnStart(wxCommandEvent& event);
    void OnCancel(wxCommandEvent& event);
    void OnShowAll(wxCommandEvent& event);
    void OnExport(wxCommandEvent& event);
    void OnLanguageChanged(wxCommandEvent& event);
    void OnThemeChanged(wxCommandEvent& event);
    void OnPatientChanged(wxCommandEvent& event);
    void OnStudyChanged(wxCommandEvent& event);
    void OnSeriesSelected(wxListEvent& event);
    void OnHitActivated(wxListEvent& event);
    void OnProgress(wxThreadEvent& event);
    void OnComplete(wxThreadEvent& event);
    void OnScanError(wxThreadEvent& event);
    void OnClose(wxCloseEvent& event);

    UiLanguage language_{UiLanguage::German};
    UiTheme theme_{UiTheme::Light};

    wxPanel* rootPanel_{};
    wxPanel* headerPanel_{};
    wxPanel* searchCard_{};
    wxPanel* studyCard_{};
    wxPanel* seriesPanel_{};
    wxPanel* hitsPanel_{};
    wxSplitterWindow* splitter_{};

    wxStaticText* title_{};
    wxStaticText* version_{};
    wxStaticText* subtitle_{};
    wxStaticText* languageLabel_{};
    wxChoice* languageChoice_{};
    wxStaticText* themeLabel_{};
    wxChoice* themeChoice_{};

    wxStaticText* sourceTitle_{};
    wxTextCtrl* folder_{};
    wxButton* browse_{};

    wxStaticText* tagTitle_{};
    wxTextCtrl* tag_{};
    wxButton* chooseTag_{};
    wxStaticText* tagInfo_{};
    wxButton* start_{};
    wxButton* cancel_{};
    wxButton* export_{};
    wxButton* showAll_{};
    wxGauge* gauge_{};
    wxStaticText* status_{};

    wxStaticText* studySectionTitle_{};
    wxStaticText* patientChoiceLabel_{};
    wxChoice* patientChoice_{};
    wxStaticText* patientIdLabel_{};
    wxStaticText* patientId_{};
    wxStaticText* studyChoiceLabel_{};
    wxChoice* studyChoice_{};
    wxStaticText* accessionLabel_{};
    wxStaticText* studyDateLabel_{};
    wxStaticText* studySizeLabel_{};
    wxStaticText* studyUidLabel_{};
    wxStaticText* accession_{};
    wxStaticText* studyDate_{};
    wxStaticText* studySize_{};
    wxStaticText* studyUid_{};

    wxStaticText* seriesTitle_{};
    wxStaticText* hitsTitle_{};
    wxStaticText* footer_{};

    wxListCtrl* seriesList_{};
    wxListCtrl* hitList_{};

    std::atomic_bool cancelRequested_{false};
    std::thread worker_;
    std::shared_ptr<ScanResult> result_;
    std::vector<std::string> visiblePatientKeys_;
    std::vector<std::string> visibleStudyKeys_;
    std::vector<std::string> visibleSeriesKeys_;
    std::vector<std::size_t> visibleHitIndices_;
    std::string selectedPatientKey_;
    std::string selectedStudyKey_;
};
