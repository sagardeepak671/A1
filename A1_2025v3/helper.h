#ifndef HELPER_H
#define HELPER_H

#include "structures.h"

/**
 * @brief Checks if a solution is feasible and provides detailed debugging information.
 * @param solution The solution to check for feasibility.
 * @param problem The problem data containing constraints.
 * @return True if the solution is feasible, false otherwise.
 */
bool IS_FEASIBLE(const Solution& solution, const ProblemData& problem);

#endif // HELPER_H
