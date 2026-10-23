#pragma once

#include "Models.h"

#include <atomic>
#include <filesystem>
#include <memory>
#include <thread>

#include <wx/frame.h>
#include <wx/listctrl.h>

class wxButton;
class wxGauge;
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
    void StartScan();
    void StopWorker();
    void PopulateResult();
    void PopulateSeries();
    void PopulateHits(const std::string& seriesKey = {});
    void UpdateStudyLabels();
    void ExportCsv();

    void OnChooseFolder(wxCommandEvent& event);
    void OnChooseTag(wxCommandEvent& event);
    void OnStart(wxCommandEvent& event);
    void OnCancel(wxCommandEvent& event);
    void OnShowAll(wxCommandEvent& event);
    void OnExport(wxCommandEvent& event);
    void OnSeriesSelected(wxListEvent& event);
    void OnProgress(wxThreadEvent& event);
    void OnComplete(wxThreadEvent& event);
    void OnScanError(wxThreadEvent& event);
    void OnClose(wxCloseEvent& event);

    wxTextCtrl* folder_{};
    wxTextCtrl* tag_{};
    wxStaticText* tagInfo_{};
    wxButton* start_{};
    wxButton* cancel_{};
    wxButton* export_{};
    wxGauge* gauge_{};
    wxStaticText* status_{};

    wxStaticText* patientName_{};
    wxStaticText* patientId_{};
    wxStaticText* accession_{};
    wxStaticText* studyDate_{};
    wxStaticText* studyDescription_{};
    wxStaticText* studyUid_{};

    wxListCtrl* seriesList_{};
    wxListCtrl* hitList_{};

    std::atomic_bool cancelRequested_{false};
    std::thread worker_;
    std::shared_ptr<ScanResult> result_;
    std::vector<std::string> visibleSeriesKeys_;
};
