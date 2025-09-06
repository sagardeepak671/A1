#include "helper.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <cstdlib>
using namespace std;


// Random Generation Functions
vector<random_stats> RAND_INIT_STATS;

void RANDOM1(Solution& solution, ProblemData& problem,int heli_index){
    // i have RAND_INIT_STATS for this helicopter
    auto& heli_plan = solution[heli_index];
    int total_villages_needed=0;
    for(auto &c:RAND_INIT_STATS[heli_index].num_drops_per_trip){
        total_villages_needed+=c;
    }
    // so i need total these many villages in total i will select randomly from the villages needed and add them to the trips
    // i will select the total_villages_needed the near ones
    vector<int> elected_villages;
    for(int i=0;i<problem.villages.size();i++){
        if(problem.villages[i].food_needed>0 || problem.villages[i].other_supplies_needed>0){
            elected_villages.push_back(problem.villages[i].id);
        }
    }
    // sorting elected villages based on distance from home city
    auto& helicopter = problem.helicopters[heli_index];
    Point home_city = problem.cities[helicopter.home_city_id-1];
    sort(elected_villages.begin(),elected_villages.end(),[&](int a,int b){
        auto& village_a = problem.villages[a-1];
        auto& village_b = problem.villages[b-1];
        double dist_a = distance(village_a.coords, home_city);
        double dist_b = distance(village_b.coords, home_city);
        return dist_a<dist_b;
    });
    //now keeping only total_villages_needed
    if(elected_villages.size()>total_villages_needed){
        elected_villages.resize(total_villages_needed);
    }
    // now randomly shuffling the elected villages 
    random_shuffle(elected_villages.begin(),elected_villages.end());
    
    // now adding these villages to the trips with percentage allocation
    int current_village_index = 0;
    int actual_trips = std::max(1, (int)(RAND_INIT_STATS[heli_index].num_trips * 0.5)); // 50% of planned trips
    
    for(int t = 0; t < actual_trips && current_village_index < elected_villages.size(); t++){
        Trip trip;
        trip.dry_food_pickup = 0;
        trip.perishable_food_pickup = 0;
        trip.other_supplies_pickup = 0;
        trip.distance_covered = 0.0;
        trip.weight_carried = 0;
        
        Point current_pos = home_city;
        int drops_for_this_trip = std::max(1, (int)(RAND_INIT_STATS[heli_index].num_drops_per_trip[t] * 0.4)); // 40% of planned drops
        
        for(int d = 0; d < drops_for_this_trip && current_village_index < elected_villages.size(); d++){
            int village_id = elected_villages[current_village_index];
            current_village_index++;
            
            auto& village = problem.villages[village_id-1];
            
            // Calculate distance feasibility first
            double dist_to_village = distance(current_pos, village.coords);
            double dist_village_to_home = distance(village.coords, home_city);
            double total_trip_dist = trip.distance_covered + dist_to_village + dist_village_to_home;
            
            // Check distance constraint
            if(total_trip_dist > helicopter.distance_capacity){
                break; // Can't add more villages to this trip
            }
            
            Drop drop;
            drop.village_id = village_id;
            drop.dry_food = 0;
            drop.perishable_food = 0;
            drop.other_supplies = 0;

            // Check weight feasibility and add supplies
            double remaining_weight = helicopter.weight_capacity - trip.weight_carried;
            if(remaining_weight <= 0){
                break; // No weight capacity left
            }
            
            // Get value/weight ratios from problem data
            double dry_food_ratio = problem.packages[0].value / problem.packages[0].weight;
            double perishable_ratio = problem.packages[1].value / problem.packages[1].weight;
            double other_ratio = problem.packages[2].value / problem.packages[2].weight;

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

            double weight_used = 0.0;
            bool drop_added = false;
            
            for (const auto& supply : supply_priority) {
                int supply_type = supply.second;
                double supply_weight = problem.packages[supply_type].weight;
                
                if (remaining_weight <= 0) break;
                
                int village_need = 0;
                int* drop_target = nullptr;
                
                switch (supply_type) {
                    case 0: // dry food
                        village_need = village.food_needed;
                        drop_target = &drop.dry_food;
                        break;
                    case 1: // perishable food  
                        village_need = village.food_needed;
                        drop_target = &drop.perishable_food;
                        break;
                    case 2: // other supplies
                        village_need = village.other_supplies_needed;
                        drop_target = &drop.other_supplies;
                        break;
                }
                
                if (village_need > 0) {
                    int max_by_weight = (int)(remaining_weight / supply_weight);
                    int max_by_need = village_need;
                    int units_to_drop = min(max_by_weight, max_by_need);
                    
                    if (units_to_drop > 0) {
                        *drop_target = units_to_drop;
                        
                        // Update village needs
                        if (supply_type == 0 || supply_type == 1) {
                            village.food_needed -= units_to_drop;
                        } else {
                            village.other_supplies_needed -= units_to_drop;
                        }
                        
                        double weight_dropped = units_to_drop * supply_weight;
                        weight_used += weight_dropped;
                        remaining_weight -= weight_dropped;
                        
                        // Update trip pickups
                        switch (supply_type) {
                            case 0: trip.dry_food_pickup += units_to_drop; break;
                            case 1: trip.perishable_food_pickup += units_to_drop; break;
                            case 2: trip.other_supplies_pickup += units_to_drop; break;
                        }
                        
                        drop_added = true;
                    }
                }
            }
            
            // Only add drop if we're actually delivering something
            if (drop_added && (drop.dry_food > 0 || drop.perishable_food > 0 || drop.other_supplies > 0)) {
                trip.drops.push_back(drop);
                trip.distance_covered += dist_to_village;
                trip.weight_carried += weight_used;
                current_pos = village.coords;
            }
        }
        
        // Add return distance to home and save trip if it has drops
        if (!trip.drops.empty()) {
            trip.distance_covered += distance(current_pos, home_city);
            
            // Final feasibility check
            if (trip.distance_covered <= helicopter.distance_capacity && 
                trip.weight_carried <= helicopter.weight_capacity) {
                heli_plan.trips.push_back(trip);
            }
        }
    }
}

void RANDOM2(Solution& solution, ProblemData& problem,int heli_index){
    // Route-first, Load-second approach
    auto& heli_plan = solution[heli_index];
    const Helicopter& helicopter = problem.helicopters[heli_index];
    Point home_city = problem.cities[helicopter.home_city_id-1];
    
    // Find villages with remaining needs
    vector<int> needy_villages;
    for(int i = 0; i < problem.villages.size(); i++){
        const Village& village = problem.villages[i];
        if(village.food_needed > 0 || village.other_supplies_needed > 0){
            needy_villages.push_back(i);
        }
    }
    
    if(needy_villages.empty()) return;
    
    // Generate 1-3 trips for this helicopter
    int num_trips = 1 + (rand() % 3);
    
    for(int trip_num = 0; trip_num < num_trips && !needy_villages.empty(); trip_num++){
        // Step 1: Generate feasible route (respecting distance capacity)
        vector<int> route_villages;
        double route_distance = 0.0;
        
        // Randomly select 2-4 villages for this route
        int max_villages = min(4, (int)needy_villages.size());
        int villages_in_route = 1 + (rand() % max_villages);
        
        // Try to build a feasible route
        vector<int> candidate_villages = needy_villages;
        random_shuffle(candidate_villages.begin(), candidate_villages.end());
        
        Point current_pos = home_city;
        
        for(int v = 0; v < villages_in_route && !candidate_villages.empty(); v++){
            double best_distance = 1e9;
            int best_village_idx = -1;
            int best_candidate_idx = -1;
            
            // Find nearest village from current position
            for(int c = 0; c < candidate_villages.size(); c++){
                const Village& village = problem.villages[candidate_villages[c]];
                double dist_to_village = distance(current_pos, village.coords);
                
                if(dist_to_village < best_distance){
                    best_distance = dist_to_village;
                    best_village_idx = candidate_villages[c];
                    best_candidate_idx = c;
                }
            }
            
            if(best_village_idx != -1){
                // Check if adding this village keeps route feasible
                double return_distance = distance(problem.villages[best_village_idx].coords, home_city);
                double total_route_dist = route_distance + best_distance + return_distance;
                
                if(total_route_dist <= helicopter.distance_capacity){
                    route_villages.push_back(best_village_idx);
                    route_distance += best_distance;
                    current_pos = problem.villages[best_village_idx].coords;
                    candidate_villages.erase(candidate_villages.begin() + best_candidate_idx);
                } else {
                    break; // Route would exceed distance capacity
                }
            }
        }
        
        if(route_villages.empty()) continue;
        
        // Add return distance to home
        route_distance += distance(current_pos, home_city);
        
        // Step 2: Solve knapsack for this route (load optimization)
        Trip trip;
        trip.distance_covered = route_distance;
        trip.dry_food_pickup = 0;
        trip.perishable_food_pickup = 0;
        trip.other_supplies_pickup = 0;
        trip.weight_carried = 0;
        
        double remaining_weight = helicopter.weight_capacity;
        
        // Randomized greedy knapsack: process villages in random order
        random_shuffle(route_villages.begin(), route_villages.end());
        
        for(int village_idx : route_villages){
            Village& village = problem.villages[village_idx];
            
            if(remaining_weight <= 0) break;
            
            Drop drop;
            drop.village_id = village.id;
            drop.dry_food = 0;
            drop.perishable_food = 0;
            drop.other_supplies = 0;
            
            // Create priority list for supplies (randomized)
            vector<pair<int, double>> supply_options; // (type, value_per_weight)
            
            if(village.food_needed > 0){
                double dry_value = 10.0 / problem.packages[0].weight; // dry food value/weight
                double perishable_value = 15.0 / problem.packages[1].weight; // perishable value/weight
                supply_options.push_back({0, dry_value});
                supply_options.push_back({1, perishable_value});
            }
            
            if(village.other_supplies_needed > 0){
                double other_value = 8.0 / problem.packages[2].weight; // other supplies value/weight
                supply_options.push_back({2, other_value});
            }
            
            // Add some randomness to value calculations
            for(auto& option : supply_options){
                option.second += (rand() % 5 - 2); // Add random noise ±2
            }
            
            // Sort by value per weight (with randomness)
            sort(supply_options.begin(), supply_options.end(), 
                 [](const pair<int,double>& a, const pair<int,double>& b){
                     return a.second > b.second;
                 });
            
            // Greedy allocation with randomized priorities
            for(auto& supply : supply_options){
                int supply_type = supply.first;
                double supply_weight = problem.packages[supply_type].weight;
                
                if(remaining_weight < supply_weight) continue;
                
                int village_need = 0;
                int* drop_target = nullptr;
                int* pickup_target = nullptr;
                
                switch(supply_type){
                    case 0: // dry food
                        village_need = village.food_needed;
                        drop_target = &drop.dry_food;
                        pickup_target = &trip.dry_food_pickup;
                        break;
                    case 1: // perishable food
                        village_need = village.food_needed;
                        drop_target = &drop.perishable_food;
                        pickup_target = &trip.perishable_food_pickup;
                        break;
                    case 2: // other supplies
                        village_need = village.other_supplies_needed;
                        drop_target = &drop.other_supplies;
                        pickup_target = &trip.other_supplies_pickup;
                        break;
                }
                
                if(village_need > 0){
                    int max_by_weight = (int)(remaining_weight / supply_weight);
                    int max_by_need = village_need;
                    
                    // Add some randomness: sometimes deliver 70-100% of what we could
                    int randomness_factor = 70 + (rand() % 31); // 70-100%
                    int units_to_drop = min(max_by_weight, max_by_need);
                    units_to_drop = (units_to_drop * randomness_factor) / 100;
                    units_to_drop = max(1, units_to_drop); // At least 1 unit if possible
                    
                    if(units_to_drop > 0 && units_to_drop * supply_weight <= remaining_weight){
                        *drop_target = units_to_drop;
                        *pickup_target += units_to_drop;
                        
                        // Update village needs
                        if(supply_type == 0 || supply_type == 1){
                            village.food_needed -= units_to_drop;
                        } else {
                            village.other_supplies_needed -= units_to_drop;
                        }
                        
                        double weight_used = units_to_drop * supply_weight;
                        remaining_weight -= weight_used;
                        trip.weight_carried += weight_used;
                    }
                }
            }
            
            // Add drop if we're delivering something
            if(drop.dry_food > 0 || drop.perishable_food > 0 || drop.other_supplies > 0){
                trip.drops.push_back(drop);
            }
        }
        
        // Only add trip if it has drops and is feasible
        if(!trip.drops.empty() && trip.distance_covered <= helicopter.distance_capacity){
            heli_plan.trips.push_back(trip);
            
            // Remove villages that no longer need supplies from needy_villages
            needy_villages.erase(
                remove_if(needy_villages.begin(), needy_villages.end(),
                    [&problem](int village_idx){
                        const Village& v = problem.villages[village_idx];
                        return v.food_needed <= 0 && v.other_supplies_needed <= 0;
                    }),
                needy_villages.end()
            );
        }
    }
}

Solution GET_RANDOM_STATE(ProblemData& problem,int restart_counter){
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
    if(restart_counter==1){
        return solution;// returning the empty solution for now
    }

    int NUMRANDOMFUNCTIONS=3; 
    for(int h =0;h<problem.helicopters.size();h++){
        
        int random_selection = rand() % NUMRANDOMFUNCTIONS;
        switch(random_selection){
            case 0:
                // random 1
                RANDOM1(solution, problem,h);
                break;
            case 1:
                // random 2
                RANDOM2(solution, problem,h);
                break;
            case 2:
                // random 3
                RANDOM1(solution, problem,h);
                break;
        }  
    }
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

void UPDATE_RANDOM_STATS(ProblemData& problem, Solution& solution){
    for(int h =0;h<problem.helicopters.size();h++){
        auto& heli_plan = solution[h];
        RAND_INIT_STATS[h].num_trips = std::max(1, (int)(heli_plan.trips.size()));// taking half of number of trips

        RAND_INIT_STATS[h].num_drops_per_trip.clear();
        for(const auto& trip:heli_plan.trips){
            RAND_INIT_STATS[h].num_drops_per_trip.push_back(std::max(1, (int)(trip.drops.size())));
        }
    } 
}




bool IS_FEASIBLE(const Solution& solution, const ProblemData& problem) {
    cout << "\n=== FEASIBILITY CHECK START ===" << endl;
    bool is_feasible = true;
    
    // Track total supplies delivered to each village for capping logic
    vector<double> food_delivered(problem.villages.size() + 1, 0.0);
    vector<double> other_delivered(problem.villages.size() + 1, 0.0);
    vector<double> helicopter_total_distances(problem.helicopters.size() + 1, 0.0);
    
    for (const auto& heli_plan : solution) {
        int heli_id = heli_plan.helicopter_id;
        
        // Check helicopter ID validity
        if (heli_id <= 0 || heli_id > problem.helicopters.size()) {
            cout << "❌ INFEASIBLE: Invalid helicopter ID " << heli_id << endl;
            is_feasible = false;
            continue;
        }
        
        const auto& helicopter = problem.helicopters[heli_id - 1];
        Point home_city_coords = problem.cities[helicopter.home_city_id - 1];
        
        cout << "\n--- Checking Helicopter " << heli_id << " ---" << endl;
        cout << "Weight capacity: " << helicopter.weight_capacity << endl;
        cout << "Distance capacity per trip: " << helicopter.distance_capacity << endl;
        cout << "Max total distance (d_max): " << problem.d_max << endl;
        
        int trip_num = 0;
        for (const auto& trip : heli_plan.trips) {
            trip_num++;
            cout << "\n  Trip " << trip_num << ":" << endl;
            
            // Check weight constraint
            double trip_weight = (trip.dry_food_pickup * problem.packages[0].weight) + 
                                (trip.perishable_food_pickup * problem.packages[1].weight) + 
                                (trip.other_supplies_pickup * problem.packages[2].weight);
            
            cout << "    Picked up: " << trip.dry_food_pickup << " dry, " 
                 << trip.perishable_food_pickup << " perishable, " 
                 << trip.other_supplies_pickup << " other" << endl;
            cout << "    Total weight: " << trip_weight << " / " << helicopter.weight_capacity << endl;
            
            if (trip_weight > helicopter.weight_capacity + 1e-9) {
                cout << "    ❌ WEIGHT VIOLATION: Trip weight " << trip_weight 
                     << " exceeds helicopter capacity " << helicopter.weight_capacity << endl;
                is_feasible = false;
            } else {
                cout << "    ✅ Weight constraint satisfied" << endl;
            }
            
            // Calculate actual trip distance and check drops consistency
            Point current_location = home_city_coords;
            double calculated_distance = 0.0;
            int total_d_dropped = 0, total_p_dropped = 0, total_o_dropped = 0;
            
            cout << "    Visiting villages: ";
            for (const auto& drop : trip.drops) {
                cout << drop.village_id << " ";
                
                // Check village ID validity
                if (drop.village_id <= 0 || drop.village_id > problem.villages.size()) {
                    cout << "\n    ❌ INVALID VILLAGE ID: " << drop.village_id << endl;
                    is_feasible = false;
                    continue;
                }
                
                const auto& village = problem.villages[drop.village_id - 1];
                
                // Calculate distance to this village
                calculated_distance += distance(current_location, village.coords);
                current_location = village.coords;
                
                // Track drops
                total_d_dropped += drop.dry_food;
                total_p_dropped += drop.perishable_food;
                total_o_dropped += drop.other_supplies;
                
                // Track total delivered for value capping
                food_delivered[drop.village_id] += drop.dry_food + drop.perishable_food;
                other_delivered[drop.village_id] += drop.other_supplies;
            }
            cout << endl;
            
            // Return to home city
            calculated_distance += distance(current_location, home_city_coords);
            
            cout << "    Calculated distance: " << calculated_distance << " / " << helicopter.distance_capacity << endl;
            cout << "    Stored distance: " << trip.distance_covered << endl;
            
            // Check distance constraint
            if (calculated_distance > helicopter.distance_capacity + 1e-9) {
                cout << "    ❌ DISTANCE VIOLATION: Trip distance " << calculated_distance 
                     << " exceeds helicopter capacity " << helicopter.distance_capacity << endl;
                is_feasible = false;
            } else {
                cout << "    ✅ Distance constraint satisfied" << endl;
            }
            
            // Check that drops don't exceed pickups
            if (total_d_dropped > trip.dry_food_pickup || 
                total_p_dropped > trip.perishable_food_pickup || 
                total_o_dropped > trip.other_supplies_pickup) {
                cout << "    ❌ DROP VIOLATION: Dropped more than picked up!" << endl;
                cout << "      Dry: dropped " << total_d_dropped << " vs picked " << trip.dry_food_pickup << endl;
                cout << "      Perishable: dropped " << total_p_dropped << " vs picked " << trip.perishable_food_pickup << endl;
                cout << "      Other: dropped " << total_o_dropped << " vs picked " << trip.other_supplies_pickup << endl;
                is_feasible = false;
            } else {
                cout << "    ✅ Drop consistency satisfied" << endl;
            }
            
            helicopter_total_distances[heli_id] += calculated_distance;
        }
        
        // Check total distance constraint (d_max)
        cout << "\n  Total distance for helicopter " << heli_id << ": " 
             << helicopter_total_distances[heli_id] << " / " << problem.d_max << endl;
        
        if (helicopter_total_distances[heli_id] > problem.d_max + 1e-9) {
            cout << "  ❌ TOTAL DISTANCE VIOLATION: Helicopter " << heli_id 
                 << " total distance " << helicopter_total_distances[heli_id] 
                 << " exceeds d_max " << problem.d_max << endl;
            is_feasible = false;
        } else {
            cout << "  ✅ Total distance constraint satisfied" << endl;
        }
    }
    
    // Check village delivery capping logic
    cout << "\n--- Village Delivery Analysis ---" << endl;
    for (int i = 1; i <= problem.villages.size(); i++) {
        const auto& village = problem.villages[i - 1];
        double max_food_needed = village.population * 9.0;
        double max_other_needed = village.population * 1.0;
        
        if (food_delivered[i] > 0 || other_delivered[i] > 0) {
            cout << "Village " << i << " (pop " << village.population << "):" << endl;
            cout << "  Food delivered: " << food_delivered[i] << " / " << max_food_needed << " needed" << endl;
            cout << "  Other delivered: " << other_delivered[i] << " / " << max_other_needed << " needed" << endl;
            
            if (food_delivered[i] > max_food_needed + 1e-9) {
                cout << "  ⚠️  WARNING: Excess food delivery (will be capped in scoring)" << endl;
            }
            if (other_delivered[i] > max_other_needed + 1e-9) {
                cout << "  ⚠️  WARNING: Excess other supplies (will be capped in scoring)" << endl;
            }
        }
    }
    
    cout << "\n=== FEASIBILITY CHECK RESULT ===" << endl;
    if (is_feasible) {
        cout << "✅ SOLUTION IS FEASIBLE - All constraints satisfied!" << endl;
    } else {
        cout << "❌ SOLUTION IS INFEASIBLE - Constraint violations found!" << endl;
    }
    cout << "=================================\n" << endl;
    
    return is_feasible;
}
