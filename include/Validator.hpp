#pragma once

#include <vector>

#include "BoxType.hpp"
#include "Constraints.hpp"
#include "Item.hpp"
#include "PackingSolution.hpp"
#include "Violation.hpp"

struct ValidationResult {
    bool valid = true;
    std::vector<Violation> violations;
};

// Checks a finished solution is physically legal and adds up against the input. This runs
// after the packer rather than filtering what goes into it, so anything reported here is a
// bug in the solver, not bad data from the caller.
namespace Validator {
    ValidationResult validate(
        const PackingSolution& solution,
        const std::vector<Item>& items,
        const std::vector<BoxType>& boxes,
        const Constraints& constraints = Constraints{}
    );
}
