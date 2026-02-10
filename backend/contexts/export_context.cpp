#include "export_context.h"

#include "../database/sqlserver_driver.h"
#include "../exporters/csv_exporter.h"
#include "../exporters/excel_exporter.h"
#include "../exporters/json_exporter.h"
#include "../interfaces/database_context.h"
#include "../parsers/sql_parser.h"
#include "../utils/json_utils.h"
#include "simdjson.h"

#include <format>

namespace velocitydb {

ExportContext::ExportContext() = default;
ExportContext::~ExportContext() = default;
ExportContext::ExportContext(ExportContext&&) noexcept = default;
ExportContext& ExportContext::operator=(ExportContext&&) noexcept = default;

std::vector<std::string> ExportContext::getSupportedFormats() const {
    return {"csv", "json", "excel"};
}

std::string ExportContext::exportWithDriver(IDatabaseContext& db, std::string_view params, std::string_view format) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params);

        auto connectionIdResult = doc["connectionId"].get_string();
        auto filepathResult = doc["filepath"].get_string();
        auto sqlQueryResult = doc["sql"].get_string();
        if (connectionIdResult.error() || filepathResult.error() || sqlQueryResult.error()) [[unlikely]] {
            return JsonUtils::errorResponse("Missing required fields: connectionId, filepath, or sql");
        }
        auto connectionId = std::string(connectionIdResult.value());
        auto filepath = std::string(filepathResult.value());
        auto sqlQuery = std::string(sqlQueryResult.value());

        if (!SQLParser::isReadOnlyQuery(sqlQuery)) [[unlikely]] {
            return JsonUtils::errorResponse("Export only supports SELECT queries");
        }

        auto driver = db.getQueryDriver(connectionId);
        if (!driver) [[unlikely]] {
            return JsonUtils::errorResponse(std::format("Connection not found: {}", connectionId));
        }

        auto queryResult = driver->execute(sqlQuery);
        ExportOptions options{};

        if (format == "csv") {
            if (auto delimiter = doc["delimiter"].get_string(); !delimiter.error()) {
                options.delimiter = std::string(delimiter.value());
            }
            if (auto includeHeader = doc["includeHeader"].get_bool(); !includeHeader.error()) {
                options.includeHeader = includeHeader.value();
            }
            if (auto nullValue = doc["nullValue"].get_string(); !nullValue.error()) {
                options.nullValue = std::string(nullValue.value());
            }
            CSVExporter exporter{};
            if (exporter.exportData(queryResult, filepath, options)) {
                return JsonUtils::successResponse(std::format(R"({{"filepath":"{}"}})", JsonUtils::escapeString(filepath)));
            }
            return JsonUtils::errorResponse("Failed to export CSV");
        }

        if (format == "json") {
            JSONExporter exporter{};
            if (auto prettyPrint = doc["prettyPrint"].get_bool(); !prettyPrint.error()) {
                exporter.setPrettyPrint(prettyPrint.value());
            }
            if (exporter.exportData(queryResult, filepath, options)) {
                return JsonUtils::successResponse(std::format(R"({{"filepath":"{}"}})", JsonUtils::escapeString(filepath)));
            }
            return JsonUtils::errorResponse("Failed to export JSON");
        }

        if (format == "excel") {
            ExcelExporter exporter{};
            if (exporter.exportData(queryResult, filepath, options)) {
                return JsonUtils::successResponse(std::format(R"({{"filepath":"{}"}})", JsonUtils::escapeString(filepath)));
            }
            return JsonUtils::errorResponse("Excel export not yet implemented");
        }

        return JsonUtils::errorResponse(std::format("Unsupported export format: {}", format));
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string ExportContext::handleExportCSV(IDatabaseContext& db, std::string_view params) {
    return exportWithDriver(db, params, "csv");
}

std::string ExportContext::handleExportJSON(IDatabaseContext& db, std::string_view params) {
    return exportWithDriver(db, params, "json");
}

std::string ExportContext::handleExportExcel(IDatabaseContext& db, std::string_view params) {
    return exportWithDriver(db, params, "excel");
}

}  // namespace velocitydb
