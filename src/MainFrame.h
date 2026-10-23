#pragma once

#include "Models.h"
#include "UiLanguage.h"

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
    void ApplyModernStyle();
    void StartScan();
    void StopWorker();
    void PopulateResult();
    void PopulateSeries();
    void PopulateHits(const std::string& seriesKey = {});
    void UpdateStudyLabels();
    void UpdateStatusSummary();
    void ExportCsv();
    wxString L(const char* de, const char* en) const;
    void SetColumnTitle(wxListCtrl* list, int column, const wxString& title);
    void SetStudyValue(wxStaticText* label, const std::string& value);

    void OnChooseFolder(wxCommandEvent& event);
    void OnChooseTag(wxCommandEvent& event);
    void OnStart(wxCommandEvent& event);
    void OnCancel(wxCommandEvent& event);
    void OnShowAll(wxCommandEvent& event);
    void OnExport(wxCommandEvent& event);
    void OnLanguageChanged(wxCommandEvent& event);
    void OnSeriesSelected(wxListEvent& event);
    void OnProgress(wxThreadEvent& event);
    void OnComplete(wxThreadEvent& event);
    void OnScanError(wxThreadEvent& event);
    void OnClose(wxCloseEvent& event);

    UiLanguage language_{UiLanguage::German};

    wxPanel* rootPanel_{};
    wxStaticText* title_{};
    wxStaticText* subtitle_{};
    wxStaticText* languageLabel_{};
    wxChoice* languageChoice_{};

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
    wxStaticText* patientNameLabel_{};
    wxStaticText* patientIdLabel_{};
    wxStaticText* accessionLabel_{};
    wxStaticText* studyDateLabel_{};
    wxStaticText* studyDescriptionLabel_{};
    wxStaticText* studyUidLabel_{};
    wxStaticText* patientName_{};
    wxStaticText* patientId_{};
    wxStaticText* accession_{};
    wxStaticText* studyDate_{};
    wxStaticText* studyDescription_{};
    wxStaticText* studyUid_{};

    wxStaticText* seriesTitle_{};
    wxStaticText* hitsTitle_{};
    wxStaticText* footer_{};

    wxListCtrl* seriesList_{};
    wxListCtrl* hitList_{};

    std::atomic_bool cancelRequested_{false};
    std::thread worker_;
    std::shared_ptr<ScanResult> result_;
    std::vector<std::string> visibleSeriesKeys_;
};
