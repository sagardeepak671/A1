#include "random_generator.h"
#include <iostream>
#include <algorithm>
#include <cstdlib>
using namespace std;

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
    int actual_trips = max(1, (int)(RAND_INIT_STATS[heli_index].num_trips * 0.5)); // 50% of planned trips
    
    for(int t = 0; t < actual_trips && current_village_index < elected_villages.size(); t++){
        Trip trip;
        trip.dry_food_pickup = 0;
        trip.perishable_food_pickup = 0;
        trip.other_supplies_pickup = 0;
        trip.distance_covered = 0.0;
        trip.weight_carried = 0;
        
        Point current_pos = home_city;
        int drops_for_this_trip = max(1, (int)(RAND_INIT_STATS[heli_index].num_drops_per_trip[t] * 0.4)); // 40% of planned drops
        
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
    // Placeholder for RANDOM2 implementation
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
                RANDOM1(solution, problem,h);
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
