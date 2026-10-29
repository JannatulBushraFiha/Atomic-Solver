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

json boxJson(const std::string& reference, double width, double length, double depth, double maxWeight) {
    return {
        { "Reference", reference },
        { "Width", width },
        { "Length", length },
        { "Depth", depth },
        { "MaxWeight", maxWeight }
    };
}

json itemJson(const std::string& code, double width, double length, double depth, double weight) {
    return {
        { "ItemCode", code },
        { "ItemReference", code },
        { "Width", width },
        { "Length", length },
        { "Depth", depth },
        { "Weight", weight }
    };
}

json jobJson(const std::vector<json>& boxes, const std::vector<json>& items) {
    return { { "boxTypes", boxes }, { "items", items } };
}

int sumOfSides(const json& placement) {
    const json& size = placement.at("placedDimension");
    return size.at("width").get<int>() + size.at("length").get<int>() + size.at("depth").get<int>();
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

// None of these are valid JSON numbers, so the JSON parser is what rejects them.
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
            if (output.contains("error")) {
                CHECK(output["error"].get<std::string>().find("Invalid JSON:") == 0);
            }
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

TEST(cli_rejects_items_with_missing_or_wrong_fields) {
    json missingWeight = validInput();
    missingWeight["items"][0].erase("Weight");
    json wrongWeight = validInput();
    wrongWeight["items"][0]["Weight"] = "heavy";

    for (const json& input : std::vector<json>{ missingWeight, wrongWeight }) {
        const ProcessResult result = runSolver(input.dump());
        CHECK(result.exitCode != 0);
        const json output = json::parse(result.output, nullptr, false);
        REQUIRE(!output.is_discarded());
        CHECK(output.at("error").get<std::string>().find("Bad input:") == 0);
    }
}

TEST(cli_rejects_box_types_or_items_that_are_not_lists) {
    json boxesAsObject = validInput();
    boxesAsObject["boxTypes"] = { { "first", validInput()["boxTypes"][0] } };
    json itemsAsObject = validInput();
    itemsAsObject["items"] = { { "first", validInput()["items"][0] } };

    for (const json& input : std::vector<json>{ boxesAsObject, itemsAsObject }) {
        const ProcessResult result = runSolver(input.dump());
        CHECK(result.exitCode != 0);
        const json output = json::parse(result.output, nullptr, false);
        REQUIRE(!output.is_discarded());
        CHECK(output.at("error").get<std::string>().find("Bad input:") == 0);
    }
}

TEST(cli_rejects_fractional_quantity) {
    json input = validInput();
    input["items"][0]["Quantity"] = 2.5;

    const ProcessResult result = runSolver(input.dump());
    CHECK(result.exitCode != 0);
    const json output = json::parse(result.output, nullptr, false);
    REQUIRE(!output.is_discarded());
    CHECK(output.at("error").get<std::string>().find("Bad input:") == 0);
}

TEST(cli_accepts_a_whole_quantity_written_as_a_decimal) {
    json input = validInput();
    input["items"][0]["Quantity"] = 3.0;

    const ProcessResult result = runSolver(input.dump());
    CHECK(result.exitCode == 0);
    const json output = json::parse(result.output, nullptr, false);
    REQUIRE(!output.is_discarded());
    CHECK(output.at("placements").size() == 3);
}

TEST(cli_quantity_maps_unplaced_items_back_to_input_code) {
    json input = validInput();
    input["items"][0]["Width"] = 500;
    input["items"][0]["Quantity"] = 2;

    const ProcessResult result = runSolver(input.dump());
    CHECK(result.exitCode == 0);
    const json output = json::parse(result.output, nullptr, false);
    REQUIRE(!output.is_discarded());
    CHECK(output.at("unplacedItems") == json::array({ "ITEM", "ITEM" }));
    REQUIRE(output.at("violations").size() == 2);
    for (const auto& violation : output.at("violations")) {
        CHECK(violation.at("itemCode") == "ITEM");
    }
    CHECK(output.at("validation").at("valid") == true);
}

// BOOK x2 is split into units internally, which must not clash with the item really called BOOK-1.
TEST(cli_quantity_codes_do_not_clash_with_other_item_codes) {
    json book = itemJson("BOOK", 10, 10, 10, 1.0);
    book["Quantity"] = 2;
    const json input = jobJson({ boxJson("BOX", 100, 100, 100, 100.0) }, { book, itemJson("BOOK-1", 50, 50, 50, 3.0) });

    const ProcessResult result = runSolver(input.dump());
    CHECK(result.exitCode == 0);
    const json output = json::parse(result.output, nullptr, false);
    REQUIRE(!output.is_discarded());
    CHECK(output.at("validation").at("valid") == true);

    int books = 0;
    int bigBooks = 0;
    for (const auto& placement : output.at("placements")) {
        if (placement.at("itemCode") == "BOOK") {
            books++;
            CHECK(sumOfSides(placement) == 30);
        }
        if (placement.at("itemCode") == "BOOK-1") {
            bigBooks++;
            CHECK(sumOfSides(placement) == 150);
        }
    }
    CHECK(books == 2);
    CHECK(bigBooks == 1);
}

// Two input lines with the same code are still packed; the validation output flags the duplicate.
TEST(cli_duplicate_item_codes_are_packed_but_flagged) {
    const json input = jobJson({ boxJson("BOX", 100, 100, 100, 100.0) },
                               { itemJson("A", 10, 10, 10, 1.0), itemJson("A", 10, 10, 10, 1.0) });

    const ProcessResult result = runSolver(input.dump());
    CHECK(result.exitCode == 0);
    const json output = json::parse(result.output, nullptr, false);
    REQUIRE(!output.is_discarded());
    CHECK(output.at("placements").size() == 2);
    CHECK(output.at("validation").at("valid") == false);

    bool flagged = false;
    for (const auto& issue : output.at("validation").at("issues")) {
        if (issue.at("code") == "DUPLICATE_ITEM_CODE" && issue.at("itemCode") == "A") flagged = true;
    }
    CHECK(flagged);
}

// Box sizes round down: a 10.9 box only has 10 whole units of room.
TEST(cli_rounds_box_dimensions_down) {
    const json input = jobJson({ boxJson("BOX", 10.9, 10.9, 10.9, 100.0) },
                               { itemJson("FITS", 10, 10, 10, 1.0), itemJson("TOO-BIG", 11, 1, 1, 1.0) });

    const ProcessResult result = runSolver(input.dump());
    CHECK(result.exitCode == 0);
    const json output = json::parse(result.output, nullptr, false);
    REQUIRE(!output.is_discarded());
    REQUIRE(output.at("placements").size() == 1);
    CHECK(output.at("placements").at(0).at("itemCode") == "FITS");
    CHECK(output.at("unplacedItems") == json::array({ "TOO-BIG" }));
}

TEST(cli_does_not_round_away_tiny_floating_point_noise) {
    const json input = jobJson({ boxJson("BOX", 9.9999999999, 10, 10, 100.0) },
                               { itemJson("A", 10.0000000001, 10, 10, 1.0) });

    const ProcessResult result = runSolver(input.dump());
    CHECK(result.exitCode == 0);
    const json output = json::parse(result.output, nullptr, false);
    REQUIRE(!output.is_discarded());
    CHECK(output.at("placements").size() == 1);
    CHECK(output.at("unplacedItems").empty());
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
        CHECK(solution.usedBoxes.size() == 1);
        for (const auto& placement : solution.placements) {
            if (placement.itemCode == "TOP") CHECK(placement.position.z == 2);
        }
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
