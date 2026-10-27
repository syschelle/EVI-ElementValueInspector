#include "DicomScanner.h"
#include "DicomUtils.h"

#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcmetinf.h>
#include <dcmtk/dcmdata/dcelem.h>
#include <dcmtk/dcmdata/dcsequen.h>

#include <algorithm>
#include <system_error>

#include <wx/string.h>

namespace {


bool ContainsCaseInsensitiveUtf8(const std::string& value, const std::string& query) {
    if (query.empty()) return false;
    wxString haystack = wxString::FromUTF8(value.c_str());
    wxString needle = wxString::FromUTF8(query.c_str());
    haystack.MakeLower();
    needle.MakeLower();
    return haystack.Find(needle) != wxNOT_FOUND;
}

bool TryGetSearchableValue(DcmObject* object, std::string& value) {
    value.clear();
    if (!object || object->getTag().getTagKey() == DCM_PixelData) return false;
    if (dynamic_cast<DcmSequenceOfItems*>(object)) return false;

    auto* element = dynamic_cast<DcmElement*>(object);
    if (!element) return false;

    // Do not pull large binary/deferred values into memory for a text search.
    constexpr Uint32 kMaxSearchValue = 1024 * 1024;
    if (element->getLength() == 0 || element->getLength() > kMaxSearchValue) return false;

    OFString text;
    if (element->getOFStringArray(text).bad()) return false;
    value = text.c_str();
    return !value.empty();
}

bool LooksLikeDicom(DcmFileFormat& file) {
    DcmDataset* ds = file.getDataset();
    if (!ds) return false;
    return !DicomUtils::GetString(ds, DCM_SOPInstanceUID).empty()
        || !DicomUtils::GetString(ds, DCM_StudyInstanceUID).empty()
        || !DicomUtils::GetString(ds, DCM_SeriesInstanceUID).empty()
        || !DicomUtils::GetString(ds, DCM_PatientID).empty()
        || !DicomUtils::GetString(ds, DCM_Modality).empty();
}

std::string PatientKey(DcmDataset* ds) {
    const std::string id = DicomUtils::GetString(ds, DCM_PatientID);
    const std::string name = DicomUtils::GetString(ds, DCM_PatientName);
    if (!id.empty() || !name.empty()) return id + "|" + name;
    return "UNKNOWN_PATIENT";
}

std::string StudyKey(DcmDataset* ds, const std::filesystem::path& file, const std::string& patientKey) {
    const std::string uid = DicomUtils::GetString(ds, DCM_StudyInstanceUID);
    if (!uid.empty()) return patientKey + "|" + uid;

    const std::string date = DicomUtils::GetString(ds, DCM_StudyDate);
    const std::string desc = DicomUtils::GetString(ds, DCM_StudyDescription);
    const std::string accession = DicomUtils::GetString(ds, DCM_AccessionNumber);
    if (!date.empty() || !desc.empty() || !accession.empty()) {
        return patientKey + "|" + date + "|" + accession + "|" + desc;
    }
    return patientKey + "|NO_STUDY_UID|" + DicomUtils::ToUtf8Path(file.parent_path());
}

std::string SeriesKey(DcmDataset* ds, const std::filesystem::path& file, const std::string& studyKey) {
    const std::string uid = DicomUtils::GetString(ds, DCM_SeriesInstanceUID);
    if (!uid.empty()) return studyKey + "|" + uid;

    const std::string number = DicomUtils::GetString(ds, DCM_SeriesNumber);
    const std::string desc = DicomUtils::GetString(ds, DCM_SeriesDescription);
    const std::string modality = DicomUtils::GetString(ds, DCM_Modality);
    if (!number.empty() || !desc.empty() || !modality.empty()) {
        return studyKey + "|" + number + "|" + desc + "|" + modality;
    }
    return studyKey + "|NO_SERIES_UID|" + DicomUtils::ToUtf8Path(file.parent_path());
}

PatientInfo& GetPatient(ScanResult& result, DcmDataset* ds) {
    const std::string key = PatientKey(ds);
    auto [it, inserted] = result.patients.try_emplace(key);
    PatientInfo& info = it->second;
    if (inserted) {
        info.patientKey = key;
        info.patientName = DicomUtils::GetString(ds, DCM_PatientName);
        info.patientId = DicomUtils::GetString(ds, DCM_PatientID);
    }
    return info;
}

StudyInfo& GetStudy(ScanResult& result,
                    DcmDataset* ds,
                    const std::filesystem::path& file,
                    const std::string& patientKey,
                    std::uintmax_t fileSize) {
    const std::string key = StudyKey(ds, file, patientKey);
    auto [it, inserted] = result.studies.try_emplace(key);
    StudyInfo& info = it->second;
    if (inserted) {
        info.studyKey = key;
        info.patientKey = patientKey;
        info.accessionNumber = DicomUtils::GetString(ds, DCM_AccessionNumber);
        info.studyDate = DicomUtils::GetString(ds, DCM_StudyDate);
        info.studyDescription = DicomUtils::GetString(ds, DCM_StudyDescription);
        info.studyInstanceUid = DicomUtils::GetString(ds, DCM_StudyInstanceUID);
    }
    info.sizeBytes += fileSize;
    result.patients[patientKey].studyKeys.insert(key);
    return info;
}

SeriesInfo& GetSeries(ScanResult& result,
                      DcmDataset* ds,
                      const std::filesystem::path& file,
                      const std::string& patientKey,
                      const std::string& studyKey,
                      std::uintmax_t fileSize) {
    const std::string key = SeriesKey(ds, file, studyKey);
    auto [it, inserted] = result.series.try_emplace(key);
    SeriesInfo& info = it->second;
    if (inserted) {
        info.seriesKey = key;
        info.patientKey = patientKey;
        info.studyKey = studyKey;
        info.seriesNumber = DicomUtils::GetString(ds, DCM_SeriesNumber);
        info.modality = DicomUtils::GetString(ds, DCM_Modality);
        info.seriesDescription = DicomUtils::GetString(ds, DCM_SeriesDescription);
        info.protocolName = DicomUtils::GetString(ds, DCM_ProtocolName);
        info.bodyPart = DicomUtils::GetString(ds, DCM_BodyPartExamined);
        info.seriesInstanceUid = DicomUtils::GetString(ds, DCM_SeriesInstanceUID);
    }

    std::string instance = DicomUtils::GetString(ds, DCM_SOPInstanceUID);
    if (instance.empty()) instance = DicomUtils::ToUtf8Path(file);
    info.instances.insert(std::move(instance));
    info.sizeBytes += fileSize;
    result.studies[studyKey].seriesKeys.insert(key);
    return info;
}

void CollectMatches(
    DcmItem* container,
    const DcmTagKey& target,
    const std::string& pathPrefix,
    const std::filesystem::path& root,
    const std::filesystem::path& file,
    DcmDataset* ds,
    const std::string& patientKey,
    const std::string& studyKey,
    const std::string& seriesKey,
    std::uintmax_t fileSize,
    UiLanguage language,
    std::vector<ScanHit>& hits)
{
    if (!container) return;

    DcmStack stack;
    while (container->nextObject(stack, OFTrue).good()) {
        DcmObject* obj = stack.top();
        if (!obj || obj->getTag().getTagKey() != target) continue;

        ScanHit hit;
        hit.patientKey = patientKey;
        hit.studyKey = studyKey;
        hit.seriesKey = seriesKey;
        hit.seriesNumber = DicomUtils::GetString(ds, DCM_SeriesNumber);
        hit.instanceNumber = DicomUtils::GetString(ds, DCM_InstanceNumber);
        hit.modality = DicomUtils::GetString(ds, DCM_Modality);
        hit.tag = DicomUtils::FormatTag(target);
        hit.tagName = DicomUtils::TagName(obj->getTag().getTagKey());
        hit.vr = DcmVR(obj->getVR()).getVRName();
        hit.value = DicomUtils::SafeElementValue(obj, language);
        hit.sequencePath = DicomUtils::SequencePath(stack, pathPrefix);

        std::error_code ec;
        const auto relative = std::filesystem::relative(file, root, ec);
        hit.relativeFile = DicomUtils::ToUtf8Path(ec ? file.filename() : relative);
        hit.fullFile = DicomUtils::ToUtf8Path(file);
        hit.sopInstanceUid = DicomUtils::GetString(ds, DCM_SOPInstanceUID);
        hit.instanceSizeBytes = fileSize;
        hits.push_back(std::move(hit));
    }
}

void CollectValueMatches(
    DcmItem* container,
    const std::string& query,
    const std::string& pathPrefix,
    const std::filesystem::path& root,
    const std::filesystem::path& file,
    DcmDataset* ds,
    const std::string& patientKey,
    const std::string& studyKey,
    const std::string& seriesKey,
    std::uintmax_t fileSize,
    UiLanguage language,
    std::vector<ScanHit>& hits)
{
    if (!container || query.empty()) return;

    DcmStack stack;
    while (container->nextObject(stack, OFTrue).good()) {
        DcmObject* obj = stack.top();
        std::string searchableValue;
        if (!TryGetSearchableValue(obj, searchableValue)) continue;
        if (!ContainsCaseInsensitiveUtf8(searchableValue, query)) continue;

        const DcmTagKey key = obj->getTag().getTagKey();
        ScanHit hit;
        hit.patientKey = patientKey;
        hit.studyKey = studyKey;
        hit.seriesKey = seriesKey;
        hit.seriesNumber = DicomUtils::GetString(ds, DCM_SeriesNumber);
        hit.instanceNumber = DicomUtils::GetString(ds, DCM_InstanceNumber);
        hit.modality = DicomUtils::GetString(ds, DCM_Modality);
        hit.tag = DicomUtils::FormatTag(key);
        hit.tagName = DicomUtils::TagName(key);
        hit.vr = DcmVR(obj->getVR()).getVRName();
        hit.value = DicomUtils::SafeElementValue(obj, language);
        hit.sequencePath = DicomUtils::SequencePath(stack, pathPrefix);

        std::error_code ec;
        const auto relative = std::filesystem::relative(file, root, ec);
        hit.relativeFile = DicomUtils::ToUtf8Path(ec ? file.filename() : relative);
        hit.fullFile = DicomUtils::ToUtf8Path(file);
        hit.sopInstanceUid = DicomUtils::GetString(ds, DCM_SOPInstanceUID);
        hit.instanceSizeBytes = fileSize;
        hits.push_back(std::move(hit));
    }
}


} // namespace

std::shared_ptr<ScanResult> DicomScanner::Scan(
    const std::filesystem::path& root,
    DicomSearchMode mode,
    uint16_t group,
    uint16_t element,
    const std::string& valueQuery,
    std::atomic_bool& cancel,
    UiLanguage language,
    ProgressCallback progress)
{
    auto result = std::make_shared<ScanResult>();
    const DcmTagKey target(group, element);

    std::vector<std::filesystem::path> files;
    std::error_code ec;
    for (std::filesystem::recursive_directory_iterator it(
             root, std::filesystem::directory_options::skip_permission_denied, ec), end;
         it != end && !ec; it.increment(ec)) {
        if (it->is_regular_file(ec)) files.push_back(it->path());
    }

    result->stats.totalFiles = files.size();

    for (std::size_t index = 0; index < files.size(); ++index) {
        if (cancel.load()) {
            result->stats.cancelled = true;
            break;
        }

        const auto& file = files[index];
        if (progress && (index == 0 || index + 1 == files.size() || index % 5 == 0)) {
            progress(index + 1, files.size(), file);
        }

        DcmFileFormat dcm;
        OFCondition status = DicomUtils::LoadDicomFile(dcm, file, 4096);
        if (status.bad()) {
            ++result->stats.readErrors;
            continue;
        }
        if (!LooksLikeDicom(dcm)) continue;

        ++result->stats.dicomFiles;
        DcmDataset* ds = dcm.getDataset();

#ifdef DCMTK_ENABLE_CHARSET_CONVERSION
        dcm.convertToUTF8();
#endif

        std::error_code sizeEc;
        const std::uintmax_t fileSize = std::filesystem::file_size(file, sizeEc);
        const std::uintmax_t safeFileSize = sizeEc ? 0 : fileSize;

        PatientInfo& patient = GetPatient(*result, ds);
        StudyInfo& study = GetStudy(*result, ds, file, patient.patientKey, safeFileSize);
        SeriesInfo& series = GetSeries(*result, ds, file, patient.patientKey, study.studyKey, safeFileSize);

        const std::size_t before = result->hits.size();
        if (mode == DicomSearchMode::Tag) {
            CollectMatches(ds, target, {}, root, file, ds,
                           patient.patientKey, study.studyKey, series.seriesKey,
                           safeFileSize, language, result->hits);
            CollectMatches(dcm.getMetaInfo(), target,
                           language == UiLanguage::German ? "Datei-Metainformationen" : "File Meta Information",
                           root, file, ds,
                           patient.patientKey, study.studyKey, series.seriesKey,
                           safeFileSize, language, result->hits);
        } else {
            CollectValueMatches(ds, valueQuery, {}, root, file, ds,
                                patient.patientKey, study.studyKey, series.seriesKey,
                                safeFileSize, language, result->hits);
            CollectValueMatches(dcm.getMetaInfo(), valueQuery,
                                language == UiLanguage::German ? "Datei-Metainformationen" : "File Meta Information",
                                root, file, ds,
                                patient.patientKey, study.studyKey, series.seriesKey,
                                safeFileSize, language, result->hits);
        }
        const std::size_t added = result->hits.size() - before;

        if (added > 0) {
            ++result->stats.filesWithMatches;
            result->stats.matches += added;
            series.hits += added;
        }
    }

    return result;
}
