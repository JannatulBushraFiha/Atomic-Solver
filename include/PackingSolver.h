#ifndef PACKINGSOLVER_H
#define PACKINGSOLVER_H

#include "Problem.h"
#include "PackingSolution.h"
#include "Constraints.h"

class PackingSolver {
public:
<<<<<<< HEAD
    PackingSolution solve(const Problem& problem);
};

#endif
=======
    Constraints constraints;

    PackingSolution solve(
        const std::vector<Item>& items,
        const std::vector<BoxType>& boxes
    );
};
>>>>>>> origin/jannatul-final-branch
