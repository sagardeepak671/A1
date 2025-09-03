#include "solver.h"
#include <iostream>
#include <chrono>
#include <map>
#include <algorithm>
#include <random>
#include <cstdlib>
#include <ctime>

using namespace std;

// Global variables
double DRY_VAL, WET_VAL, OTHER_VAL;
double DRY_WEIGHT, WET_WEIGHT, OTHER_WEIGHT;
double MAX_DIST_OF_EACH_HELI;
int NUM_HELICOPTERS;
int NUM_VILLAGES;
int NUM_CITIES;
ProblemData PROBLEM;

// Helper function to get current village needs based on solution state
map<int, pair<int, int>> get_current_village_needs(const Solution& solution) {
    map<int, pair<int, int>> current_needs; // village_id -> {meal_need, other_need}
    
    // Initialize with original needs
    for(int i = 0; i < NUM_VILLAGES; i++) {
        current_needs[i] = {PROBLEM.villages[i].population * 9, PROBLEM.villages[i].population * 1};
    }
    
    // Subtract what's already been delivered
    for(const auto& heli_plan : solution) {
        for(const auto& trip : heli_plan.trips) {
            for(const auto& drop : trip.drops) {
                current_needs[drop.village_id].first -= (drop.dry_food + drop.perishable_food);
                current_needs[drop.village_id].second -= drop.other_supplies;
                current_needs[drop.village_id].first = max(0, current_needs[drop.village_id].first);
                current_needs[drop.village_id].second = max(0, current_needs[drop.village_id].second);
            }
        }
    }
    
    return current_needs;
}

// Helper function to get current helicopter usage
map<int, double> get_current_helicopter_distance_used(const Solution& solution) {
    map<int, double> distance_used;
    
    // Initialize with helicopter IDs from problem data
    for(const auto& heli : PROBLEM.helicopters) {
        distance_used[heli.id] = 0.0;
    }
    
    for(const auto& heli_plan : solution) {
        for(const auto& trip : heli_plan.trips) {
            distance_used[heli_plan.helicopter_id] += trip.dist_travelled;
        }
    }
    
    return distance_used;
}
double EVALUATE_COST_OF_SOLUTION(const Solution& solution) {
    double total_value_achieved = 0.0;
    double total_trip_cost = 0.0;
    
    for(const auto& heli_plan : solution){
        for(const auto& trip : heli_plan.trips){ 
            // Find helicopter by ID
            const Helicopter* heli = nullptr;
            for(const auto& h : PROBLEM.helicopters) {
                if(h.id == heli_plan.helicopter_id) {
                    heli = &h;
                    break;
                }
            }
            if(!heli) continue; // Skip if helicopter not found
            
            // Calculate trip cost = F + alpha * distance
            total_trip_cost += heli->fixed_cost + heli->alpha * trip.dist_travelled;
            
            // Calculate value achieved from packages delivered
            for(const auto& drop : trip.drops){
                total_value_achieved += drop.dry_food * DRY_VAL;
                total_value_achieved += drop.perishable_food * WET_VAL;
                total_value_achieved += drop.other_supplies * OTHER_VAL;
            }
        }
    }
    return total_value_achieved - total_trip_cost;
}

Solution GET_A_RANDOM_STATE(){
    Solution solution;
    
    // Initialize empty helicopter plans
    for(int i = 0; i < NUM_HELICOPTERS; i++){
        HelicopterPlan plan;
        plan.helicopter_id = PROBLEM.helicopters[i].id;  // Use actual helicopter ID, not array index
        solution.push_back(plan);
    }
    
    // Create random trips for each helicopter
    for(int h = 0; h < NUM_HELICOPTERS; h++) {
        const Helicopter& heli = PROBLEM.helicopters[h];
        Point home_city = PROBLEM.cities[heli.home_city_id - 1];  // Convert to 0-based index
        double used_distance = 0.0;
        
        // Add 1-3 random trips per helicopter
        int num_trips = 1 + rand() % 3;
        
        for(int t = 0; t < num_trips; t++) {
            // Try to create a feasible trip
            vector<int> village_candidates;
            for(int v = 0; v < NUM_VILLAGES; v++) {
                double dist_to_village = distance(home_city, PROBLEM.villages[v].coords);
                double round_trip_dist = dist_to_village * 2;
                
                // Check if trip is feasible
                if(used_distance + round_trip_dist <= MAX_DIST_OF_EACH_HELI && 
                   round_trip_dist <= heli.distance_capacity) {
                    village_candidates.push_back(v);
                }
            }
            
            if(village_candidates.empty()) break;
            
            // Select random village
            int selected_village = village_candidates[rand() % village_candidates.size()];
            const Village& village = PROBLEM.villages[selected_village];
            
            double dist_to_village = distance(home_city, village.coords);
            double round_trip_dist = dist_to_village * 2;
            
            // Calculate random delivery amounts within weight capacity
            double remaining_weight = heli.weight_capacity;
            
            // Random amounts (but reasonable)
            int max_perishable = min((int)(remaining_weight / WET_WEIGHT), village.population * 9);
            int perishable_amount = rand() % (max_perishable + 1);
            remaining_weight -= perishable_amount * WET_WEIGHT;
            
            int max_dry = min((int)(remaining_weight / DRY_WEIGHT), village.population * 9 - perishable_amount);
            int dry_amount = rand() % (max_dry + 1);
            remaining_weight -= dry_amount * DRY_WEIGHT;
            
            int max_other = min((int)(remaining_weight / OTHER_WEIGHT), village.population * 1);
            int other_amount = rand() % (max_other + 1);
            
            // Only add trip if we're delivering something
            if(perishable_amount > 0 || dry_amount > 0 || other_amount > 0) {
                Trip trip;
                trip.dry_food_pickup = dry_amount;
                trip.perishable_food_pickup = perishable_amount;
                trip.other_supplies_pickup = other_amount;
                trip.dist_travelled = round_trip_dist;
                
                Drop drop;
                drop.village_id = PROBLEM.villages[selected_village].id;  // Use actual village ID, not index
                drop.dry_food = dry_amount;
                drop.perishable_food = perishable_amount;
                drop.other_supplies = other_amount;
                
                trip.drops.push_back(drop);
                solution[h].trips.push_back(trip);
                
                used_distance += round_trip_dist;
            }
        }
    }
    
    return solution;
}

Solution S1(const Solution& current_solution) {
    // S1: Add a new trip to a random helicopter
    Solution new_solution = current_solution;
    
    // Get current state
    auto current_needs = get_current_village_needs(current_solution);
    auto current_distances = get_current_helicopter_distance_used(current_solution);
    
    // Try to add a trip to a random helicopter
    vector<int> helicopter_candidates;
    for(const auto& heli : PROBLEM.helicopters) {
        if(current_distances[heli.id] < MAX_DIST_OF_EACH_HELI - 10) { // Leave some margin
            helicopter_candidates.push_back(heli.id);
        }
    }

    if(helicopter_candidates.empty()) return current_solution; // No feasible helicopter
    
    int selected_heli_id = helicopter_candidates[rand() % helicopter_candidates.size()];
    
    // Find the helicopter object
    const Helicopter* heli = nullptr;
    for(const auto& h : PROBLEM.helicopters) {
        if(h.id == selected_heli_id) {
            heli = &h;
            break;
        }
    }
    if(!heli) return current_solution;
    
    Point home_city = PROBLEM.cities[heli->home_city_id - 1];  // Convert to 0-based index
    
    // Find which index this helicopter has in the solution
    int solution_index = -1;
    for(int i = 0; i < new_solution.size(); i++) {
        if(new_solution[i].helicopter_id == selected_heli_id) {
            solution_index = i;
            break;
        }
    }
    if(solution_index == -1) return current_solution;
    
    // Find villages with remaining needs
    vector<int> needy_villages;
    for(int v = 0; v < NUM_VILLAGES; v++) {
        if(current_needs[v].first > 0 || current_needs[v].second > 0) {
            double dist_to_village = distance(home_city, PROBLEM.villages[v].coords);
            double round_trip_dist = dist_to_village * 2;
            
            if(current_distances[selected_heli_id] + round_trip_dist <= MAX_DIST_OF_EACH_HELI &&
               round_trip_dist <= heli->distance_capacity) {
                needy_villages.push_back(v);
            }
        }
    }
    
    if(needy_villages.empty()) return current_solution; // No feasible village
    
    int selected_village = needy_villages[rand() % needy_villages.size()];
    const Village& village = PROBLEM.villages[selected_village];
    
    double dist_to_village = distance(home_city, village.coords);
    double round_trip_dist = dist_to_village * 2;
    
    // Calculate delivery amounts
    int meal_need = current_needs[selected_village].first;
    int other_need = current_needs[selected_village].second;
    
    double remaining_weight = heli->weight_capacity;
    
    // Prefer perishable food
    int perishable_delivery = min(meal_need, (int)(remaining_weight / WET_WEIGHT));
    remaining_weight -= perishable_delivery * WET_WEIGHT;
    meal_need -= perishable_delivery;
    
    int dry_delivery = min(meal_need, (int)(remaining_weight / DRY_WEIGHT));
    remaining_weight -= dry_delivery * DRY_WEIGHT;
    
    int other_delivery = min(other_need, (int)(remaining_weight / OTHER_WEIGHT));
    
    // Create and add the trip
    if(perishable_delivery > 0 || dry_delivery > 0 || other_delivery > 0) {
        Trip new_trip;
        new_trip.dry_food_pickup = dry_delivery;
        new_trip.perishable_food_pickup = perishable_delivery;
        new_trip.other_supplies_pickup = other_delivery;
        new_trip.dist_travelled = round_trip_dist;
        
        Drop drop;
        drop.village_id = PROBLEM.villages[selected_village].id;  // Use actual village ID, not index
        drop.dry_food = dry_delivery;
        drop.perishable_food = perishable_delivery;
        drop.other_supplies = other_delivery;
        
        new_trip.drops.push_back(drop);
        new_solution[solution_index].trips.push_back(new_trip);
    }
    
    return new_solution;
}

Solution S2(const Solution& current_solution){
    // S2: Add a village to an existing trip
    Solution new_solution = current_solution;
    
    // Get current state
    auto current_needs = get_current_village_needs(current_solution);
    auto current_distances = get_current_helicopter_distance_used(current_solution);
    
    // Find helicopters with existing trips
    vector<pair<int, int>> trip_candidates; // {solution_index, trip_index}
    for(size_t h = 0; h < new_solution.size(); h++) {
        for(size_t t = 0; t < new_solution[h].trips.size(); t++) {
            trip_candidates.push_back({(int)h, (int)t});
        }
    }
    
    if(trip_candidates.empty()) return current_solution; // No existing trips
    
    // Select random trip to extend
    auto selected = trip_candidates[rand() % trip_candidates.size()];
    int solution_index = selected.first;
    int trip_id = selected.second;
    
    int heli_id = new_solution[solution_index].helicopter_id;
    
    // Find the helicopter object
    const Helicopter* heli = nullptr;
    for(const auto& h : PROBLEM.helicopters) {
        if(h.id == heli_id) {
            heli = &h;
            break;
        }
    }
    if(!heli) return current_solution;
    
    Trip& trip = new_solution[solution_index].trips[trip_id];
    Point home_city = PROBLEM.cities[heli->home_city_id - 1];  // Convert to 0-based index
    
    // Calculate current trip route and remaining capacity
    double current_weight = trip.dry_food_pickup * DRY_WEIGHT + 
                           trip.perishable_food_pickup * WET_WEIGHT + 
                           trip.other_supplies_pickup * OTHER_WEIGHT;
    double remaining_weight = heli->weight_capacity - current_weight;
    
    if(remaining_weight < 1.0) return current_solution; // No weight capacity
    
    // Find last village in current trip
    Point last_point = home_city;
    if(!trip.drops.empty()) {
        int last_village = trip.drops.back().village_id;
        last_point = PROBLEM.villages[last_village].coords;
    }
    
    // Find villages that can be added
    vector<int> candidate_villages;
    for(int v = 0; v < NUM_VILLAGES; v++) {
        if(current_needs[v].first > 0 || current_needs[v].second > 0) {
            // Check if village is already in this trip
            bool already_visited = false;
            for(const auto& drop : trip.drops) {
                if(drop.village_id == v) {
                    already_visited = true;
                    break;
                }
            }
            
            if(!already_visited) {
                Point village_coords = PROBLEM.villages[v].coords;
                double dist_to_village = distance(last_point, village_coords);
                double dist_home = distance(village_coords, home_city);
                
                // Calculate new total trip distance
                double new_trip_dist = trip.dist_travelled - distance(last_point, home_city) + 
                                      dist_to_village + dist_home;
                
                // Check feasibility
                if(new_trip_dist <= heli->distance_capacity) {
                    double new_total_dist = current_distances[heli_id] - trip.dist_travelled + new_trip_dist;
                    if(new_total_dist <= MAX_DIST_OF_EACH_HELI) {
                        candidate_villages.push_back(v);
                    }
                }
            }
        }
    }
    
    if(candidate_villages.empty()) return current_solution; // No feasible villages
    
    int selected_village = candidate_villages[rand() % candidate_villages.size()];
    
    // Calculate delivery amounts for this village
    int meal_need = current_needs[selected_village].first;
    int other_need = current_needs[selected_village].second;
    
    int perishable_delivery = min(meal_need, (int)(remaining_weight / WET_WEIGHT));
    remaining_weight -= perishable_delivery * WET_WEIGHT;
    meal_need -= perishable_delivery;
    
    int dry_delivery = min(meal_need, (int)(remaining_weight / DRY_WEIGHT));
    remaining_weight -= dry_delivery * DRY_WEIGHT;
    
    int other_delivery = min(other_need, (int)(remaining_weight / OTHER_WEIGHT));
    
    if(perishable_delivery > 0 || dry_delivery > 0 || other_delivery > 0) {
        // Update trip pickups
        trip.dry_food_pickup += dry_delivery;
        trip.perishable_food_pickup += perishable_delivery;
        trip.other_supplies_pickup += other_delivery;
        
        // Add new drop
        Drop new_drop;
        new_drop.village_id = PROBLEM.villages[selected_village].id;  // Use actual village ID, not index
        new_drop.dry_food = dry_delivery;
        new_drop.perishable_food = perishable_delivery;
        new_drop.other_supplies = other_delivery;
        
        trip.drops.push_back(new_drop);
        
        // Update trip distance
        Point village_coords = PROBLEM.villages[selected_village].coords;
        double dist_to_village = distance(last_point, village_coords);
        double dist_home = distance(village_coords, home_city);
        trip.dist_travelled = trip.dist_travelled - distance(last_point, home_city) + 
                             dist_to_village + dist_home;
    }
    
    return new_solution;
}

Solution SUCCESSOR_FUNCTION(const Solution& current_solution){
    // Choose between different successor functions randomly
    int choice = rand() % 2;
    
    if(choice == 0) {
        return S1(current_solution);  // Add new trip
    } else {
        return S2(current_solution);  // Extend existing trip
    }
}

Solution RANDOM_RESTART_LOCAL_SEARCH(){
    srand(time(NULL)); // Initialize random seed
    
    Solution best_solution;
    double best_value = -1e18;
    
    // Time-based termination (run for a reasonable time)
    auto start_time = chrono::steady_clock::now();
    auto max_duration = chrono::seconds(int(PROBLEM.time_limit_minutes * 60 * 0.8)); // Use 80% of time limit
    
    int restart_count = 0;
    
    while(chrono::steady_clock::now() - start_time < max_duration && restart_count < 100) {
        Solution current_solution = GET_A_RANDOM_STATE();
        double current_value = EVALUATE_COST_OF_SOLUTION(current_solution);
        
        // Local search
        bool improved = true;
        int local_iterations = 0;
        while(improved && local_iterations < 50) { // Limit local search iterations
            improved = false;
            
            // Try multiple neighbors
            for(int i = 0; i < 5; i++) {
                Solution neighbor = SUCCESSOR_FUNCTION(current_solution);
                double neighbor_value = EVALUATE_COST_OF_SOLUTION(neighbor);
                
                if(neighbor_value > current_value) {
                    current_solution = neighbor;
                    current_value = neighbor_value;
                    improved = true;
                    break;
                }
            }
            local_iterations++;
        }
        
        if(current_value > best_value) {
            best_value = current_value;
            best_solution = current_solution;
            cout << "New best solution found with value: " << best_value << endl;
        }
        
        restart_count++;
    }
    
    cout << "Random restart completed with " << restart_count << " restarts" << endl;
    return best_solution;
}


Solution solve(const ProblemData& problem) {
    cout << "Starting solver..." << endl;
    
    // Initialize global variables
    DRY_VAL = problem.packages[0].value;
    WET_VAL = problem.packages[1].value;
    OTHER_VAL = problem.packages[2].value;
    DRY_WEIGHT = problem.packages[0].weight;
    WET_WEIGHT = problem.packages[1].weight;
    OTHER_WEIGHT = problem.packages[2].weight;
    MAX_DIST_OF_EACH_HELI = problem.d_max;
    MAX_DIST_OF_EACH_HELI-=1;
    NUM_HELICOPTERS = problem.helicopters.size();
    NUM_VILLAGES = problem.villages.size();
    NUM_CITIES = problem.cities.size();
    PROBLEM = problem;
    
    cout << "Problem parameters:" << endl;
    cout << "Villages: " << NUM_VILLAGES << ", Helicopters: " << NUM_HELICOPTERS << endl;
    cout << "Package values (dry, wet, other): " << DRY_VAL << ", " << WET_VAL << ", " << OTHER_VAL << endl;
    cout << "Package weights (dry, wet, other): " << DRY_WEIGHT << ", " << WET_WEIGHT << ", " << OTHER_WEIGHT << endl;
    cout << "Max distance per helicopter: " << MAX_DIST_OF_EACH_HELI << endl;
    
    // Debug: print cities and helicopters
    cout << "Cities: ";
    for(size_t i = 0; i < PROBLEM.cities.size(); i++) {
        cout << i << ":(" << PROBLEM.cities[i].x << "," << PROBLEM.cities[i].y << ") ";
    }
    cout << endl;
    cout << "Helicopters: ";
    for(size_t i = 0; i < PROBLEM.helicopters.size(); i++) {
        cout << "ID:" << PROBLEM.helicopters[i].id << " home_city:" << PROBLEM.helicopters[i].home_city_id << " ";
    }
    cout << endl;
    
    // Use random restart local search
    Solution solution = RANDOM_RESTART_LOCAL_SEARCH();
    
    cout << "Final solution value: " << EVALUATE_COST_OF_SOLUTION(solution) << endl;
    cout << "Solver finished." << endl;
    return solution;
}