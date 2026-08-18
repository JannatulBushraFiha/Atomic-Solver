#ifndef PACKINGSOLVER_H
#define PACKINGSOLVER_H

#include "Problem.h"
#include "PackingSolution.h"

class PackingSolver {
public:
    PackingSolution solve(const Problem& problem);
};

#endif
