#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

struct TagEntry {
    uint16_t group{};
    uint16_t element{};
    uint16_t upperGroup{};
    uint16_t upperElement{};
    bool repeating{};
    std::string tagDisplay;
    std::string name;
    std::string vr;
    std::string vm;
    std::string version;
};

struct StudyInfo {
    std::string patientName;
    std::string patientId;
    std::string accessionNumber;
    std::string studyDate;
    std::string studyDescription;
    std::string studyInstanceUid;
};

struct SeriesInfo {
    std::string seriesKey;
    std::string seriesNumber;
    std::string modality;
    std::string seriesDescription;
    std::string protocolName;
    std::string bodyPart;
    std::string seriesInstanceUid;
    std::set<std::string> instances;
    std::size_t hits{};
};

struct ScanHit {
    std::string seriesKey;
    std::string seriesNumber;
    std::string instanceNumber;
    std::string modality;
    std::string tag;
    std::string tagName;
    std::string vr;
    std::string value;
    std::string sequencePath;
    std::string relativeFile;
    std::string fullFile;
    std::string sopInstanceUid;
};

struct ScanStats {
    std::size_t totalFiles{};
    std::size_t dicomFiles{};
    std::size_t filesWithTag{};
    std::size_t tagHits{};
    std::size_t readErrors{};
    bool cancelled{};
};

struct ScanResult {
    StudyInfo study;
    bool hasStudy{};
    std::map<std::string, SeriesInfo> series;
    std::vector<ScanHit> hits;
    ScanStats stats;
};
