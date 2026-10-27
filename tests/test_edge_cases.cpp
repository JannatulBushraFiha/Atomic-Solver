#include "test_helpers.h"

#include "Constraints.h"
#include "PackingSolver.h"
#include "json.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

using json = nlohmann::json;

struct ProcessResult {
    int exitCode;
    std::string output;
};

ProcessResult runSolver(const std::string& input) {
    const auto uniqueId = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path tempDir = std::filesystem::temp_directory_path();
    const std::filesystem::path inputPath = tempDir / ("atomic-solver-" + std::to_string(uniqueId) + ".in");
    const std::filesystem::path outputPath = tempDir / ("atomic-solver-" + std::to_string(uniqueId) + ".out");

    {
        std::ofstream inputFile(inputPath, std::ios::binary);
        inputFile << input;
        if (!inputFile) {
            return { -1, "" };
        }
    }

    int exitCode = -1;
#ifdef _WIN32
    SECURITY_ATTRIBUTES securityAttributes{};
    securityAttributes.nLength = sizeof(securityAttributes);
    securityAttributes.bInheritHandle = TRUE;

    HANDLE inputHandle = CreateFileA(inputPath.string().c_str(), GENERIC_READ, FILE_SHARE_READ,
                                     &securityAttributes, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    HANDLE outputHandle = CreateFileA(outputPath.string().c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                                      &securityAttributes, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (inputHandle != INVALID_HANDLE_VALUE && outputHandle != INVALID_HANDLE_VALUE) {
        STARTUPINFOA startupInfo{};
        startupInfo.cb = sizeof(startupInfo);
        startupInfo.dwFlags = STARTF_USESTDHANDLES;
        startupInfo.hStdInput = inputHandle;
        startupInfo.hStdOutput = outputHandle;
        startupInfo.hStdError = outputHandle;

        PROCESS_INFORMATION processInfo{};
        std::string commandLine = "\"" SOLVER_EXECUTABLE_PATH "\"";
        const BOOL started = CreateProcessA(SOLVER_EXECUTABLE_PATH, commandLine.data(),
                                            nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                                            nullptr, nullptr, &startupInfo, &processInfo);
        if (started) {
            WaitForSingleObject(processInfo.hProcess, INFINITE);
            DWORD processExitCode = 1;
            if (GetExitCodeProcess(processInfo.hProcess, &processExitCode)) {
                exitCode = static_cast<int>(processExitCode);
            }
            CloseHandle(processInfo.hThread);
            CloseHandle(processInfo.hProcess);
        }
    }

    if (inputHandle != INVALID_HANDLE_VALUE) CloseHandle(inputHandle);
    if (outputHandle != INVALID_HANDLE_VALUE) CloseHandle(outputHandle);
#else
    const std::string command = "\"" SOLVER_EXECUTABLE_PATH "\" < \"" +
                                inputPath.string() + "\" > \"" + outputPath.string() + "\"";
    exitCode = std::system(command.c_str());
#endif

    std::string output;
    {
        std::ifstream outputFile(outputPath, std::ios::binary);
        output.assign(std::istreambuf_iterator<char>(outputFile), std::istreambuf_iterator<char>());
    }

    std::error_code ignored;
    std::filesystem::remove(inputPath, ignored);
    std::filesystem::remove(outputPath, ignored);
    return { exitCode, output };
}

json validInput() {
    return {
        { "boxTypes", {
            {
                { "Reference", "BOX" },
                { "Width", 100 },
                { "Length", 100 },
                { "Depth", 100 },
                { "MaxWeight", 100.0 }
            }
        } },
        { "items", {
            {
                { "ItemCode", "ITEM" },
                { "ItemReference", "Item" },
                { "Width", 1 },
                { "Length", 1 },
                { "Depth", 1 },
                { "Weight", 1.0 }
            }
        } }
    };
}

TEST(cli_rejects_malformed_json) {
    const ProcessResult result = runSolver("{not valid json");
    CHECK(result.exitCode != 0);

    const json output = json::parse(result.output, nullptr, false);
    CHECK(!output.is_discarded());
    if (!output.is_discarded()) {
        CHECK(output.contains("error"));
        if (output.contains("error")) {
            CHECK(output["error"].get<std::string>().find("Invalid JSON:") == 0);
        }
    }
}

TEST(cli_rejects_missing_required_fields) {
    const ProcessResult result = runSolver(R"({"items":[]})");
    CHECK(result.exitCode != 0);

    const json output = json::parse(result.output, nullptr, false);
    CHECK(!output.is_discarded());
    if (!output.is_discarded()) {
        CHECK(output.contains("error"));
        if (output.contains("error")) {
            CHECK(output["error"].get<std::string>().find("Bad input:") == 0);
        }
    }
}

TEST(cli_rejects_fields_with_wrong_types) {
    json input = validInput();
    input["boxTypes"][0]["Width"] = "wide";

    const ProcessResult result = runSolver(input.dump());
    CHECK(result.exitCode != 0);

    const json output = json::parse(result.output, nullptr, false);
    CHECK(!output.is_discarded());
    if (!output.is_discarded()) {
        CHECK(output.contains("error"));
        if (output.contains("error")) {
            CHECK(output["error"].get<std::string>().find("Bad input:") == 0);
        }
    }
}

TEST(cli_quantity_defaults_to_one_and_preserves_item_code) {
    const ProcessResult result = runSolver(validInput().dump());
    CHECK(result.exitCode == 0);

    const json output = json::parse(result.output, nullptr, false);
    CHECK(!output.is_discarded());
    if (!output.is_discarded()) {
        CHECK(output.contains("placements"));
        if (output.contains("placements")) {
            CHECK(output["placements"].size() == 1);
            if (output["placements"].size() == 1) {
                CHECK(output["placements"][0]["itemCode"] == "ITEM");
            }
        }
        CHECK(output.contains("validation"));
        if (output.contains("validation")) {
            CHECK(output["validation"]["valid"] == true);
        }
    }
}

TEST(cli_quantity_expands_units_and_maps_codes_back_to_input) {
    json input = validInput();
    input["items"][0]["Quantity"] = 3;

    const ProcessResult result = runSolver(input.dump());
    CHECK(result.exitCode == 0);

    const json output = json::parse(result.output, nullptr, false);
    CHECK(!output.is_discarded());
    if (!output.is_discarded()) {
        CHECK(output.contains("placements"));
        if (output.contains("placements")) {
            CHECK(output["placements"].size() == 3);
            for (const auto& placement : output["placements"]) {
                CHECK(placement["itemCode"] == "ITEM");
            }
        }
        CHECK(output.contains("validation"));
        if (output.contains("validation")) {
            CHECK(output["validation"]["valid"] == true);
        }
    }
}

TEST(cli_rejects_zero_quantity) {
    json input = validInput();
    input["items"][0]["Quantity"] = 0;

    const ProcessResult result = runSolver(input.dump());
    CHECK(result.exitCode != 0);

    const json output = json::parse(result.output, nullptr, false);
    CHECK(!output.is_discarded());
    if (!output.is_discarded()) {
        CHECK(output.contains("error"));
        if (output.contains("error")) {
            CHECK(output["error"].get<std::string>().find("Bad input:") == 0);
        }
    }
}

TEST(cli_rejects_negative_quantity) {
    json input = validInput();
    input["items"][0]["Quantity"] = -1;

    const ProcessResult result = runSolver(input.dump());
    CHECK(result.exitCode != 0);

    const json output = json::parse(result.output, nullptr, false);
    CHECK(!output.is_discarded());
    if (!output.is_discarded()) {
        CHECK(output.contains("error"));
        if (output.contains("error")) {
            CHECK(output["error"].get<std::string>().find("Bad input:") == 0);
        }
    }
}

TEST(cli_rejects_non_finite_weights) {
    for (const std::string& weight : { "NaN", "Infinity", "-Infinity", "1e309" }) {
        json input = validInput();
        input["items"][0]["Weight"] = nullptr;
        std::string text = input.dump();
        const std::string nullValue = "\"Weight\":null";
        const size_t weightPosition = text.find(nullValue);
        CHECK(weightPosition != std::string::npos);
        if (weightPosition == std::string::npos) {
            continue;
        }
        text.replace(weightPosition, nullValue.size(), "\"Weight\":" + weight);

        const ProcessResult result = runSolver(text);
        CHECK(result.exitCode != 0);
        const json output = json::parse(result.output, nullptr, false);
        CHECK(!output.is_discarded());
        if (!output.is_discarded()) {
            CHECK(output.contains("error"));
        }
    }
}

TEST(cli_rejects_dimensions_outside_integer_range) {
    json input = validInput();
    input["items"][0]["Width"] = 2147483648LL;

    const ProcessResult itemResult = runSolver(input.dump());
    CHECK(itemResult.exitCode != 0);
    const json itemOutput = json::parse(itemResult.output, nullptr, false);
    CHECK(!itemOutput.is_discarded());
    if (!itemOutput.is_discarded()) {
        CHECK(itemOutput.contains("error"));
    }

    input = validInput();
    input["boxTypes"][0]["Depth"] = 2147483648LL;
    const ProcessResult boxResult = runSolver(input.dump());
    CHECK(boxResult.exitCode != 0);
    const json boxOutput = json::parse(boxResult.output, nullptr, false);
    CHECK(!boxOutput.is_discarded());
    if (!boxOutput.is_discarded()) {
        CHECK(boxOutput.contains("error"));
    }
}

TEST(constraints_reject_large_item_volume_above_configured_limit) {
    Constraints constraints;
    constraints.maxItemVolume = 999999999999999999LL;

    Item item = makeItem("LARGE", 1000000, 1000000, 1000000, 1.0);
    BoxType box = makeBox("LARGE-BOX", 1000000, 1000000, 1000000, 10.0);
    Violation violation;

    CHECK(!constraints.checkItem(item, { box }, violation));
    CHECK(violation.code == "ITEM_TOO_LARGE");
}

TEST(solver_allows_fragile_support_load_below_and_at_limit) {
    for (const double loadWeight : { 4.9, 5.0 }) {
        PackingSolver solver;
        const Item fragileBase = makeFragile("FRAGILE", 4, 4, 2, 10.0);
        const Item topItem = makeFragile("TOP", 2, 2, 1, loadWeight);
        const BoxType box = makeBox("BOX", 4, 4, 3, 100.0);

        const PackingSolution solution = solver.solve({ fragileBase, topItem }, { box });
        CHECK(solution.unplacedItems.empty());
        CHECK(solution.placements.size() == 2);
    }
}

TEST(solver_rejects_fragile_support_load_above_limit) {
    PackingSolver solver;
    const Item fragileBase = makeFragile("FRAGILE", 4, 4, 2, 10.0);
    const Item topItem = makeFragile("TOP", 2, 2, 1, 5.1);
    const BoxType box = makeBox("BOX", 4, 4, 3, 100.0);

    const PackingSolution solution = solver.solve({ fragileBase, topItem }, { box });
    CHECK(solution.placements.size() == 2);
    CHECK(solution.unplacedItems.empty());
    if (solution.placements.size() == 2) {
        CHECK(solution.placements[0].boxInstance != solution.placements[1].boxInstance);
    }
}

}
