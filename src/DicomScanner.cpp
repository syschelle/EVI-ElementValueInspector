#include "DicomScanner.h"
#include "DicomUtils.h"

#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcmetinf.h>

#include <algorithm>
#include <system_error>

namespace {

bool LooksLikeDicom(DcmFileFormat& file) {
    DcmDataset* ds = file.getDataset();
    if (!ds) return false;
    return !DicomUtils::GetString(ds, DCM_SOPInstanceUID).empty()
        || !DicomUtils::GetString(ds, DCM_StudyInstanceUID).empty()
        || !DicomUtils::GetString(ds, DCM_SeriesInstanceUID).empty()
        || !DicomUtils::GetString(ds, DCM_PatientID).empty()
        || !DicomUtils::GetString(ds, DCM_Modality).empty();
}

std::string SeriesKey(DcmDataset* ds, const std::filesystem::path& file) {
    std::string uid = DicomUtils::GetString(ds, DCM_SeriesInstanceUID);
    if (!uid.empty()) return uid;

    std::string number = DicomUtils::GetString(ds, DCM_SeriesNumber);
    std::string desc = DicomUtils::GetString(ds, DCM_SeriesDescription);
    std::string modality = DicomUtils::GetString(ds, DCM_Modality);
    if (!number.empty() || !desc.empty() || !modality.empty()) {
        return number + "|" + desc + "|" + modality;
    }
    return "NO_SERIES_UID|" + DicomUtils::ToUtf8Path(file.parent_path());
}

void PopulateStudy(ScanResult& result, DcmDataset* ds) {
    if (result.hasStudy || !ds) return;
    result.study.patientName = DicomUtils::GetString(ds, DCM_PatientName);
    result.study.patientId = DicomUtils::GetString(ds, DCM_PatientID);
    result.study.accessionNumber = DicomUtils::GetString(ds, DCM_AccessionNumber);
    result.study.studyDate = DicomUtils::GetString(ds, DCM_StudyDate);
    result.study.studyDescription = DicomUtils::GetString(ds, DCM_StudyDescription);
    result.study.studyInstanceUid = DicomUtils::GetString(ds, DCM_StudyInstanceUID);
    result.hasStudy = true;
}

SeriesInfo& GetSeries(ScanResult& result, DcmDataset* ds, const std::filesystem::path& file) {
    const std::string key = SeriesKey(ds, file);
    auto [it, inserted] = result.series.try_emplace(key);
    SeriesInfo& info = it->second;
    if (inserted) {
        info.seriesKey = key;
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
    return info;
}

void CollectMatches(
    DcmItem* container,
    const DcmTagKey& target,
    const std::string& pathPrefix,
    const std::filesystem::path& root,
    const std::filesystem::path& file,
    DcmDataset* ds,
    const std::string& seriesKey,
    std::vector<ScanHit>& hits)
{
    if (!container) return;

    DcmStack stack;
    while (container->nextObject(stack, OFTrue).good()) {
        DcmObject* obj = stack.top();
        if (!obj || obj->getTag().getTagKey() != target) continue;

        ScanHit hit;
        hit.seriesKey = seriesKey;
        hit.seriesNumber = DicomUtils::GetString(ds, DCM_SeriesNumber);
        hit.instanceNumber = DicomUtils::GetString(ds, DCM_InstanceNumber);
        hit.modality = DicomUtils::GetString(ds, DCM_Modality);
        hit.tag = DicomUtils::FormatTag(target);
        hit.tagName = DicomUtils::TagName(obj->getTag().getTagKey());
        hit.vr = DcmVR(obj->getVR()).getVRName();
        hit.value = DicomUtils::SafeElementValue(obj);
        hit.sequencePath = DicomUtils::SequencePath(stack, pathPrefix);

        std::error_code ec;
        const auto relative = std::filesystem::relative(file, root, ec);
        hit.relativeFile = DicomUtils::ToUtf8Path(ec ? file.filename() : relative);
        hit.fullFile = DicomUtils::ToUtf8Path(file);
        hit.sopInstanceUid = DicomUtils::GetString(ds, DCM_SOPInstanceUID);
        hits.push_back(std::move(hit));
    }
}

} // namespace

std::shared_ptr<ScanResult> DicomScanner::Scan(
    const std::filesystem::path& root,
    uint16_t group,
    uint16_t element,
    std::atomic_bool& cancel,
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

        PopulateStudy(*result, ds);
        SeriesInfo& series = GetSeries(*result, ds, file);

        const std::size_t before = result->hits.size();
        CollectMatches(ds, target, {}, root, file, ds, series.seriesKey, result->hits);
        CollectMatches(dcm.getMetaInfo(), target, "File Meta Information", root, file, ds, series.seriesKey, result->hits);
        const std::size_t added = result->hits.size() - before;

        if (added > 0) {
            ++result->stats.filesWithTag;
            result->stats.tagHits += added;
            series.hits += added;
        }
    }

    return result;
}
