#pragma once

#include "Models.h"
#include "UiLanguage.h"

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>

enum class DicomSearchMode {
    Tag,
    Value
};

class DicomScanner {
public:
    using ProgressCallback = std::function<void(std::size_t, std::size_t, const std::filesystem::path&)>;

    std::shared_ptr<ScanResult> Scan(
        const std::filesystem::path& root,
        DicomSearchMode mode,
        uint16_t group,
        uint16_t element,
        const std::string& valueQuery,
        std::atomic_bool& cancel,
        UiLanguage language,
        ProgressCallback progress);
};
