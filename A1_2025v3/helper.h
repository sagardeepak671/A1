#ifndef HELPER_H
#define HELPER_H

#include "structures.h"
#include <vector>

struct random_stats{
    int num_trips;
    std::vector<int> num_drops_per_trip;
};

extern std::vector<random_stats> RAND_INIT_STATS;

/**
 * @brief Checks if a solution is feasible and provides detailed debugging information.
 * @param solution The solution to check for feasibility.
 * @param problem The problem data containing constraints.
 * @return True if the solution is feasible, false otherwise.
 */
bool IS_FEASIBLE(const Solution& solution, const ProblemData& problem);

/**
 * @brief Generates random state 1 for a helicopter
 */
void RANDOM1(Solution& solution, ProblemData& problem, int heli_index);

/**
 * @brief Generates random state 2 for a helicopter
 */
void RANDOM2(Solution& solution, ProblemData& problem, int heli_index);

/**
 * @brief Gets a random initial state for the problem
 */
Solution GET_RANDOM_STATE(ProblemData& problem, int restart_counter);

/**
 * @brief Updates random statistics based on current solution
 */
void UPDATE_RANDOM_STATS(ProblemData& problem, Solution& solution);

/**
 * @brief Resets problem to initial state
 */
void RESET_PROBLEM(ProblemData& problem);

#endif // HELPER_H
