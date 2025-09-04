#include "solver.h"
#include "helper.h"
#include <iostream>
#include <chrono>
#include <unordered_map>
#include <map>
#include <algorithm>
using namespace std;
// You can add any helper functions or classes you need here.

/**
 * @brief The main function to implement your search/optimization algorithm.
 * * This is a placeholder implementation. It creates a simple, likely invalid,
 * plan to demonstrate how to build the Solution object. 
 * * TODO: REPLACE THIS ENTIRE FUNCTION WITH YOUR ALGORITHM.
 */

// map<int,int> village_id_to_idx;
// map<int,int> heli_id_to_idx;
/*
    village and helicopter id are index in vector +1
    so village , helicopter index are id-1
*/


double EVALUATE_VALUE(const ProblemData& problem, const Solution& solution){
    double total_value = 0.0;
    double total_cost = 0.0;

    for(const auto &heli_plan:solution){
        int heli_id = heli_plan.helicopter_id;
        const Helicopter* heli_ptr = &problem.helicopters[heli_id-1];
        if (!heli_ptr) continue; // Invalid helicopter ID, skip
        
        for(const auto &trip:heli_plan.trips){
            // Calculate value from drops
            for(const auto &drop:trip.drops){
                const Village* village_ptr = &problem.villages[drop.village_id-1];
                if (!village_ptr) continue; // Invalid village ID, skip
                
                total_value += drop.dry_food * problem.packages[0].value;
                total_value += drop.perishable_food * problem.packages[1].value;
                total_value += drop.other_supplies * problem.packages[2].value;
            }
            // Calculate cost from distance
            total_cost += heli_ptr->alpha * trip.distance_covered;
        }
    }
    return total_value - total_cost;
}


vector<vector<double>> create_village_graph_optimized(ProblemData& Problem) {
    auto villist = Problem.villages;
    int n = villist.size();
    vector<vector<double>> adjacency(n, vector<double>(n, 0.0));
    
    for (int i = 0; i < n; i++) {
        adjacency[i][i] = 0.0;
        for (int j = i + 1; j < n; j++) {
            double dist = distance(villist[i].coords, villist[j].coords);
            adjacency[i][j] = dist;
            adjacency[j][i] = dist;
        }
    } 
    return adjacency;
}

unordered_map<int, int> village_id_to_adj_index(const ProblemData& problem) {
    const auto& villages = problem.villages;
    unordered_map<int, int> id_to_index;
    for (int i = 0; i < villages.size(); i++) {
        id_to_index[villages[i].id] = i;
    }
    
    return id_to_index;
}

pair<Trip, double> extend_with_value(Trip& trip, Helicopter& helicopter, Village& new_village, 
                                    const ProblemData& problem,
                                    const vector<vector<double>>& adjacency,
                                    const unordered_map<int, int>& id_to_index) {
    
    // Store original trip state for rollback if needed
    Trip original_trip = trip;
    
    // Get helicopter info
    Point home_city = problem.cities[helicopter.home_city_id];
    
    // Check distance constraint
    double distance_extension = 0.0;
    
    if (!trip.drops.empty()) {
        // Get last village visited
        int last_village_id = trip.drops.back().village_id;
        int last_idx = id_to_index.at(last_village_id);
        int new_idx = id_to_index.at(new_village.id);
        
        // Calculate distance extension:
        // dist(last -> new) + dist(new -> home) - dist(last -> home)
        double dist_last_to_new = adjacency[last_idx][new_idx];
        double dist_new_to_home = distance(new_village.coords, home_city);
        double dist_last_to_home = distance(
            problem.villages[last_village_id].coords, home_city);
        
        distance_extension = dist_last_to_new + dist_new_to_home - dist_last_to_home;
    } else {
        // First village in trip: round trip distance
        distance_extension = 2.0 * distance(new_village.coords, home_city);
    }
    
    // Check if distance constraint is violated
    if (trip.distance_covered + distance_extension > helicopter.distance_capacity) {
        return {original_trip, -1e9}; // Return very negative value for infeasible extension
    }
    
    // Calculate remaining weight capacity
    double remaining_weight = helicopter.weight_capacity - trip.weight_carried;
    if (remaining_weight <= 0) {
        return {original_trip, -1e9}; // No weight capacity left
    }
    
    // Get value/weight ratios from problem data
    double dry_food_ratio = problem.packages[0].value / problem.packages[0].weight;
    double perishable_ratio = problem.packages[1].value / problem.packages[1].weight;
    double other_ratio = problem.packages[2].value / problem.packages[2].weight;
    
    // Determine which food type has better ratio
    bool prefer_perishable = perishable_ratio > dry_food_ratio;
    double better_food_ratio = prefer_perishable ? perishable_ratio : dry_food_ratio;
    
    // Create priority order: compare best food type with other supplies
    vector<pair<double, int>> supply_priority;
    if (better_food_ratio > other_ratio) {
        supply_priority = {{better_food_ratio, prefer_perishable ? 1 : 0}, {other_ratio, 2}};
    } else {
        supply_priority = {{other_ratio, 2}, {better_food_ratio, prefer_perishable ? 1 : 0}};
    }
    
    // Add the less preferred food type at the end (in case we have excess capacity)
    if (prefer_perishable) {
        supply_priority.push_back({dry_food_ratio, 0});
    } else {
        supply_priority.push_back({perishable_ratio, 1});
    }
    
    // Greedy allocation
    Drop new_drop;
    new_drop.village_id = new_village.id;
    new_drop.dry_food = 0;
    new_drop.perishable_food = 0;
    new_drop.other_supplies = 0;
    
    double weight_to_drop = 0.0;
    double value_gained = 0.0;  // Track value gained from drops
    
    for (const auto& supply : supply_priority) {
        int supply_type = supply.second;
        double supply_weight = problem.packages[supply_type].weight;
        double supply_value = problem.packages[supply_type].value;
        
        if (remaining_weight <= 0) break;
        
        int* drop_target = nullptr;
        int village_need = 0;
        
        switch (supply_type) {
            case 0: // dry food
                drop_target = &new_drop.dry_food;
                village_need = new_village.food_needed;
                break;
            case 1: // perishable food
                drop_target = &new_drop.perishable_food;
                village_need = new_village.food_needed;
                break;
            case 2: // other supplies
                drop_target = &new_drop.other_supplies;
                village_need = new_village.other_supplies_needed;
                break;
        }
        
        if (village_need > 0) {
            // Calculate how many units we can drop (only constrained by weight and need)
            int max_by_weight = (int)(remaining_weight / supply_weight);
            int max_by_need = village_need;
            
            int units_to_drop = min(max_by_weight, max_by_need);
            
            if (units_to_drop > 0) {
                *drop_target = units_to_drop;
                
                // Update village needs
                if (supply_type == 0 || supply_type == 1) {
                    // Food item
                    new_village.food_needed -= units_to_drop;
                } else {
                    // Other supplies
                    new_village.other_supplies_needed -= units_to_drop;
                }
                
                double weight_dropped = units_to_drop * supply_weight;
                weight_to_drop += weight_dropped;
                remaining_weight -= weight_dropped;
                
                // Calculate value gained from this drop
                value_gained += units_to_drop * supply_value;
                
                // Add dropped amounts to pickup amounts (since supplies are unlimited)
                switch (supply_type) {
                    case 0: trip.dry_food_pickup += units_to_drop; break;
                    case 1: trip.perishable_food_pickup += units_to_drop; break;
                    case 2: trip.other_supplies_pickup += units_to_drop; break;
                }
            }
        }
    }
    
    // Calculate net increase in objective value
    double cost_increase = helicopter.alpha * distance_extension;
    double net_value_increase = value_gained - cost_increase;
    
    // Only extend if we're dropping something
    if (weight_to_drop > 0) {
        trip.drops.push_back(new_drop);
        trip.distance_covered += distance_extension;
        trip.weight_carried += weight_to_drop;
        
        return {trip, net_value_increase};
    } else {
        return {original_trip, -1e9}; // No benefit from extension
    }
}

void S1(Solution& solution, ProblemData& problem){
    // extend a trip function
    // selecting a random helicopter
    int heli_index = rand() % problem.helicopters.size();
    auto& heli_plan = solution[heli_index];
    auto& helicopter = problem.helicopters[heli_index];
    // selecting a random trip
    int trip_index = rand() % heli_plan.trips.size();
    auto& trip = heli_plan.trips[trip_index];

    // Get value/weight ratios from problem data
    double dry_food_ratio = problem.packages[0].value / problem.packages[0].weight;
    double perishable_ratio = problem.packages[1].value / problem.packages[1].weight;
    double other_ratio = problem.packages[2].value / problem.packages[2].weight;
    bool prefer_perishable = perishable_ratio > dry_food_ratio;
    double better_food_ratio = prefer_perishable ? perishable_ratio : dry_food_ratio;

    double best_increase = 0.0;
    if(!trip.drops.empty()){
        // adding drop to the back of this current trip drops
        int last_village_id = trip.drops.back().village_id;
        int last_village_idx = last_village_id - 1;

        for(int i=0;i<problem.villages.size();i++){
            auto& new_village = problem.villages[i];

            double dist_last_to_new = distance(problem.villages[last_village_idx].coords, new_village.coords);
            double dist_new_to_home = distance(new_village.coords, problem.cities[helicopter.home_city_id-1]);
            double dist_last_to_home = distance(problem.villages[last_village_idx].coords, problem.cities[helicopter.home_city_id-1]);
            double distance_extension = dist_last_to_new + dist_new_to_home - dist_last_to_home;
            if(trip.distance_covered + distance_extension > helicopter.distance_capacity -0.1){// 0.1 for being safe for precision error will change later accordingly
                continue;
            } 

            

        } 
    }else{
        // adding first drop

    }  
}


void SUCCESSOR_FUNCTION(Solution& solution, ProblemData& problem){
    // choosing a random successor function
    S1(solution, problem);

}



Trip NewTrip()
{
    
}

vector<Solution> Successor(Solution solution){
    vector<Solution> Successor_List;
    auto ans= solution;
    for (auto& Heli:ans)
    {
        for (auto& trip:Heli.trips){
            // extend(&trip);
        }
    }
}


Solution GET_RANDOM_STATE(ProblemData& problem){
    Solution solution;
    for(int i=0;i<problem.helicopters.size();i++){
        HelicopterPlan heli_plan;
        heli_plan.helicopter_id = problem.helicopters[i].id;
        // Create a single trip for each helicopter
        Trip trip;
        trip.dry_food_pickup = 0;
        trip.perishable_food_pickup = 0;
        trip.other_supplies_pickup = 0;
        trip.distance_covered = 0.0;
        trip.weight_carried = 0;
        // Initially no drops
        heli_plan.trips.push_back(trip);
        solution.push_back(heli_plan);
    }

    // returning empty solution for now


    return solution;
}

void RESET_PROBLEM(ProblemData& problem){
    for (auto& village:problem.villages){
        village.food_needed = 9*village.population;
        village.other_supplies_needed = village.population;
    }
    for(auto& heli:problem.helicopters){
        heli.total_distance_covered = 0.0;
    }
}


Solution RANDOM_RESTART_LOCAL_SEARCH(ProblemData& problem) {
    Solution best_solution;
    double best_value = -1e18;
    
    int restarts = 100;
    while(restarts--){
        RESET_PROBLEM(problem);
        Solution current_solution = GET_RANDOM_STATE(problem);
        double current_value = EVALUATE_VALUE(problem, current_solution);
        int local_iterations = 10;
        while(local_iterations--) {
            SUCCESSOR_FUNCTION(current_solution, problem);
            current_value = EVALUATE_VALUE(problem, current_solution);
            if (current_value > best_value) {
                best_value = current_value;
                best_solution = current_solution;
                local_iterations = 10; // Reset local iterations on improvement
            }
        }
    }
    return best_solution;
}


Solution solve(ProblemData& problem) {
    cout << "Starting solver..." << endl;
    RESET_PROBLEM(problem);
    Solution solution; 
    
    solution = RANDOM_RESTART_LOCAL_SEARCH(problem);
    cout<<"Value of solution: "<<EVALUATE_VALUE(problem, solution)<<endl;
    
    // Check feasibility of the solution
    IS_FEASIBLE(solution, problem);
    
    cout << "Solver finished." << endl;
    return solution;
}