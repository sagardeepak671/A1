#include "solver.h"
#include <iostream>
#include <chrono>
#include <unordered_map>
#include <algorithm>
using namespace std;
// You can add any helper functions or classes you need here.

/**
 * @brief The main function to implement your search/optimization algorithm.
 * * This is a placeholder implementation. It creates a simple, likely invalid,
 * plan to demonstrate how to build the Solution object. 
 * * TODO: REPLACE THIS ENTIRE FUNCTION WITH YOUR ALGORITHM.
 */

vector<vector<double>> create_village_graph_optimized(const ProblemData& Problem) {
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


Solution solve(const ProblemData& problem) {
    cout << "Starting solver..." << endl;

    Solution solution;

    // --- START OF PLACEHOLDER LOGIC ---
    // This is a naive example: send each helicopter on one trip to the first village.
    // This will definitely violate constraints but shows the structure.
    
    for (const auto& helicopter : problem.helicopters) {
        HelicopterPlan plan;
        plan.helicopter_id = helicopter.id;

        if (!problem.villages.empty()) {
            Trip trip;
            // Pickup 1 of each package type
            trip.dry_food_pickup = 1;
            trip.perishable_food_pickup = 1;
            trip.other_supplies_pickup = 1;

            // Drop them at the first village
            Drop drop;
            drop.village_id = problem.villages[0].id;
            drop.dry_food = 1;
            drop.perishable_food = 1;
            drop.other_supplies = 1;

            trip.drops.push_back(drop);
            plan.trips.push_back(trip);
        }
        solution.push_back(plan);
    }
    
    // --- END OF PLACEHOLDER LOGIC ---

    cout << "Solver finished." << endl;
    return solution;
}