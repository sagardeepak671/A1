#include "helper.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <cstdlib>
using namespace std;


// STATISTICS FOR THE RANDOM INITIALIZATION
vector<random_stats> RAND_INIT_STATS;

void RANDOM1(Solution& solution, ProblemData& problem, int heli_index, double food_percentage){
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
            elected_villages.push_back(i);
        }
    }
    // sorting elected villages based on distance from home city
    auto& helicopter = problem.helicopters[heli_index];
    Point home_city = problem.cities[helicopter.home_city_id-1];

    // sorting by distance from home city
    sort(elected_villages.begin(),elected_villages.end(),[&](int a,int b){return distance(problem.villages[a].coords, home_city)<distance(problem.villages[b].coords, home_city);});
    
    //now keeping only total_villages_needed
    total_villages_needed = total_villages_needed*0.5;
    if(elected_villages.size() > (size_t)total_villages_needed){
        elected_villages.resize(total_villages_needed);
    }
    
    // now randomly shuffling the elected villages 
    random_shuffle(elected_villages.begin(),elected_villages.end());
    
    // now adding these villages to the trips with percentage allocation
    int current_village_index = 0;
    // Be extremely conservative with trip count to avoid d_max violations
    int actual_trips = std::max(1, std::min(2, (int)(RAND_INIT_STATS[heli_index].num_trips * 0.5))); // 50% of planned trips, max 2
    double HELI_TOTO_TRACK = 0.0; // Track total distance for this helicopter
    
    for(int t = 0; t < actual_trips && current_village_index < elected_villages.size() && HELI_TOTO_TRACK < problem.d_max *0.999; t++){
        Trip trip;
        trip.dry_food_pickup = 0;
        trip.perishable_food_pickup = 0;
        trip.other_supplies_pickup = 0;
        trip.distance_covered = 0.0;
        trip.weight_carried = 0.0;
        
        Point current_pos = home_city;
        int drops_for_this_trip = (int)(RAND_INIT_STATS[heli_index].num_drops_per_trip[t] * 0.5);

        for(int d = 0; d < drops_for_this_trip && current_village_index < elected_villages.size(); d++){
            int curr_village_id = elected_villages[current_village_index];
            current_village_index++;
            
            auto& village = problem.villages[curr_village_id];
            
            // Calculate distance to this village from current position
            double dist_to_village = distance(current_pos, village.coords);
            
            // Calculate what the complete round-trip distance would be if we add this village
            // This should be: home -> village1 -> village2 -> ... -> new_village -> home
            double potential_trip_dist;
            if(trip.drops.empty()) {
                // First village: home -> village -> home
                potential_trip_dist = 2 * distance(home_city, village.coords);
            } else {
                // Additional village: current_trip_without_return + distance_to_new_village + return_from_new_village
                double trip_without_return = trip.distance_covered - distance(current_pos, home_city);
                potential_trip_dist = trip_without_return + dist_to_village + distance(village.coords, home_city);
            }
            
            // Check distance constraint for single trip
            if(potential_trip_dist > helicopter.distance_capacity){
                break; // Can't add more villages to this trip
            }
            
            // Check total distance constraint (d_max) for helicopter - be very strict
            if(HELI_TOTO_TRACK + potential_trip_dist > problem.d_max * 0.95){
                break; // Would exceed helicopter's total distance limit
            }
            
            
            // Create drop for this village
            Drop drop;
            drop.village_id = problem.villages[curr_village_id].id;
            drop.dry_food = 0;
            drop.perishable_food = 0;
            drop.other_supplies = 0;
            
            // Check weight feasibility and add supplies
            double remaining_weight = helicopter.weight_capacity - trip.weight_carried;
            if(remaining_weight <= 0){
                break; // No weight capacity left
            }
            // Use food_percentage to distribute supplies like in successor function
            double dry_weight = problem.packages[0].weight;
            double perishable_weight = problem.packages[1].weight;
            double other_weight = problem.packages[2].weight;
            
            double dry_value = problem.packages[0].value;
            double perishable_value = problem.packages[1].value;
            double other_value = problem.packages[2].value;
            
            // Calculate value/weight ratios
            double dry_ratio = dry_value / dry_weight;
            double perishable_ratio = perishable_value / perishable_weight;
            double other_ratio = other_value / other_weight;
            
            // Calculate expected food ratio based on weight percentage
            double expected_food_ratio = food_percentage * dry_ratio + (1.0 - food_percentage) * perishable_ratio;
            
            // Prioritize food if it has better or equal value/weight ratio
            bool prioritize_food = expected_food_ratio >= other_ratio;
            
            double weight_used = 0.0;
            bool drop_added = false;
            double available_weight = remaining_weight;
            
            
            int temp_food_needed = village.food_needed;
            int temp_other_needed = village.other_supplies_needed;
            
            // Allocate based on priority
            if (prioritize_food && temp_food_needed > 0 && available_weight > 0) {
                // Allocate food first - split by WEIGHT percentage
                
                // Calculate target weights for dry and perishable food
                double target_dry_weight = available_weight * food_percentage;
                double target_perishable_weight = available_weight * (1.0 - food_percentage);
                
                // Calculate packet counts based on weight targets
                int dry_packets = (int)(target_dry_weight / dry_weight);
                int perishable_packets = (int)(target_perishable_weight / perishable_weight);
                
                // Constrain by village needs
                dry_packets = min(dry_packets, temp_food_needed);
                perishable_packets = min(perishable_packets, temp_food_needed - dry_packets);
                
                // Calculate actual weights
                double actual_dry_weight = dry_packets * dry_weight;
                double actual_perishable_weight = perishable_packets * perishable_weight;
                double total_food_weight = actual_dry_weight + actual_perishable_weight;
                
                // Check if it fits in available weight
                if (total_food_weight <= available_weight && (dry_packets > 0 || perishable_packets > 0)) {
                    drop.dry_food = dry_packets;
                    drop.perishable_food = perishable_packets;
                    
                    weight_used += total_food_weight;
                    
                    // Update village needs
                    village.food_needed -= (dry_packets + perishable_packets);
                    available_weight -= total_food_weight;
                    
                    // Update trip pickups
                    trip.dry_food_pickup += dry_packets;
                    trip.perishable_food_pickup += perishable_packets;
                    
                    drop_added = true;
                }
            }
            
            // Allocate other supplies with remaining capacity
            if (temp_other_needed > 0 && available_weight >= other_weight) {
                int other_packets = min(temp_other_needed, (int)(available_weight / other_weight));
                
                if (other_packets > 0) {
                    drop.other_supplies = other_packets;
                    double actual_other_weight = other_packets * other_weight;
                    
                    weight_used += actual_other_weight;
                    
                    // Update village needs
                    village.other_supplies_needed -= other_packets;
                    available_weight -= actual_other_weight;
                    
                    // Update trip pickups
                    trip.other_supplies_pickup += other_packets;
                    
                    drop_added = true;
                }
            }
            
            // If we didn't prioritize food initially, try to allocate remaining food
            if (!prioritize_food && temp_food_needed > 0 && available_weight > 0) {
                // Use remaining weight for food with same percentage split
                double remaining_dry_weight = available_weight * food_percentage;
                double remaining_perishable_weight = available_weight * (1.0 - food_percentage);
                
                int additional_dry = min(temp_food_needed, (int)(remaining_dry_weight / dry_weight));
                int additional_perishable = min(temp_food_needed - additional_dry, 
                                               (int)(remaining_perishable_weight / perishable_weight));
                
                double additional_weight = additional_dry * dry_weight + additional_perishable * perishable_weight;
                
                if (additional_weight <= available_weight && (additional_dry > 0 || additional_perishable > 0)) {
                    drop.dry_food += additional_dry;
                    drop.perishable_food += additional_perishable;
                    
                    weight_used += additional_weight;
                    
                    // Update village needs
                    village.food_needed -= (additional_dry + additional_perishable);
                    
                    // Update trip pickups
                    trip.dry_food_pickup += additional_dry;
                    trip.perishable_food_pickup += additional_perishable;
                    
                    drop_added = true;
                }
            }
            
            // Only add drop if we're actually delivering something
            if (drop_added && (drop.dry_food > 0 || drop.perishable_food > 0 || drop.other_supplies > 0)) {
                trip.drops.push_back(drop);
                // Update trip distance to maintain round-trip calculation
                if(trip.drops.size() == 1) {
                    // First village: home -> village + return distance
                    trip.distance_covered = distance(home_city, village.coords) + distance(village.coords, home_city);
                } else {
                    // Additional villages: remove old return distance, add segment to new village, add new return distance
                    trip.distance_covered = trip.distance_covered - distance(current_pos, home_city) + dist_to_village + distance(village.coords, home_city);
                }
                trip.weight_carried += weight_used;
                current_pos = village.coords;
                
            }
        }
        
        // Trip distance already includes return to home, so just do final feasibility check
        if (!trip.drops.empty()) {
            // Final feasibility check including total distance constraint - be very strict
            if (trip.distance_covered <= helicopter.distance_capacity && 
                trip.weight_carried <= helicopter.weight_capacity &&
                HELI_TOTO_TRACK + trip.distance_covered <= problem.d_max * 0.95) {
                heli_plan.trips.push_back(trip);
                HELI_TOTO_TRACK += trip.distance_covered; // Update total distance
                helicopter.total_distance_covered += trip.distance_covered; // Update helicopter.total_distance_covered
            } else {
                // If this trip would violate constraints, stop creating more trips
                break;
            }
        }
    }
} 

void RANDOM_NEARBY_VILLAGE(Solution& solution, ProblemData& problem, vector<double> ratio_arr){
    // extend a trip function
    // selecting a random helicopter
    int heli_index = rand() % problem.helicopters.size();
    auto& heli_plan = solution[heli_index];
    auto& helicopter = problem.helicopters[heli_index];
    double food_percentage = ratio_arr[heli_index];
    // selecting a random trip
    int trip_index = rand() % heli_plan.trips.size();
    auto& trip = heli_plan.trips[trip_index];
    // adding a random nearby village
    vector<int> elected_villages;
    for(int i=0;i<problem.villages.size();i++){
        if(problem.villages[i].food_needed>0 || problem.villages[i].other_supplies_needed>0){
            elected_villages.push_back(i);
        }
    }
    // sorting elected villages based on distance from home city
    sort(elected_villages.begin(),elected_villages.end(),[&](int a,int b){return distance(problem.villages[a].coords, problem.cities[helicopter.home_city_id-1])<distance(problem.villages[b].coords, problem.cities[helicopter.home_city_id-1]);});
    if(elected_villages.empty()) return;
    // seceting random village from the first 30 percent
    bool got_a_village=false;
    int limit = max(1, (int)(elected_villages.size()*0.3));
    int cnt=0;
    while(got_a_village && cnt<limit){
        // select a random villag efrom from 0 to limit in the sorted list
        int rand_index = rand() % limit;
        int curr_village_id = elected_villages[rand_index];
        auto& village = problem.villages[curr_village_id];
        // check distance feasibility
        int trip_distance = 2*distance(problem.cities[helicopter.home_city_id-1], village.coords);
        if(trip_distance + trip.distance_covered > helicopter.distance_capacity || trip_distance + helicopter.total_distance_covered > problem.d_max){
            cnt++;
            continue;
        }
        // supply as much as possible to the first village
        Drop drop;
        drop.village_id = village.id;
        drop.dry_food = 0;
        drop.perishable_food = 0;
        drop.other_supplies = 0;
        //i know vilalge.food_needed and village.other_supplies_needed
        double remaining_weight = helicopter.weight_capacity - trip.weight_carried;
        if(remaining_weight <= 0) return; // no weight capacity left
        // Use food_percentage to distribute supplies like in successor function
        double dry_weight = problem.packages[0].weight;
        double perishable_weight = problem.packages[1].weight;
        double other_weight = problem.packages[2].weight;
        double dry_value = problem.packages[0].value;
        double perishable_value = problem.packages[1].value;
        double other_value = problem.packages[2].value;
        // Calculate value/weight ratios
        double dry_ratio = dry_value / dry_weight;
        double perishable_ratio = perishable_value / perishable_weight;
        double other_ratio = other_value / other_weight;
        // Calculate expected food ratio based on weight percentage
        double expected_food_ratio = food_percentage * dry_ratio + (1.0 - food_percentage) * perishable_ratio;
        // Prioritize food if it has better or equal value/weight ratio
        bool prioritize_food = expected_food_ratio >= other_ratio;
        double weight_used = 0.0;
        bool drop_added = false;
        double available_weight = remaining_weight;
        int temp_food_needed = village.food_needed;
        int temp_other_needed = village.other_supplies_needed;
        // Allocate based on priority
        if (prioritize_food && temp_food_needed > 0 && available_weight > 0
        ) {
            // Allocate food first - split by WEIGHT percentage
            // Calculate target weights for dry and perishable food
            double target_dry_weight = available_weight * food_percentage;
            double target_perishable_weight = available_weight * (1.0 - food_percentage);
            // Calculate packet counts based on weight targets
            int dry_packets = (int)(target_dry_weight / dry_weight);
            int perishable_packets = (int)(target_perishable_weight / perishable_weight);
            // Constrain by village needs
            dry_packets = min(dry_packets, temp_food_needed);
            perishable_packets = min(perishable_packets, temp_food_needed - dry_packets);
            // Calculate actual weights
            double actual_dry_weight = dry_packets * dry_weight;
            double actual_perishable_weight = perishable_packets * perishable_weight;
            double total_food_weight = actual_dry_weight + actual_perishable_weight;
            // Check if it fits in available weight
            if (total_food_weight <= available_weight && (dry_packets > 0 || perishable_packets > 0)) {
                drop.dry_food = dry_packets;
                drop.perishable_food = perishable_packets;
                weight_used += total_food_weight;
                // Update village needs
                village.food_needed -= (dry_packets + perishable_packets);
                available_weight -= total_food_weight;
                // Update trip pickups
                trip.dry_food_pickup += dry_packets;
                trip.perishable_food_pickup += perishable_packets;
                drop_added = true;
            }
        }
        // Allocate other supplies with remaining capacity
        if (temp_other_needed > 0 && available_weight >= other_weight) {
            int other_packets = min(temp_other_needed, (int)(available_weight / other_weight));
            if (other_packets > 0) {
                drop.other_supplies = other_packets;
                double actual_other_weight = other_packets * other_weight;
                weight_used += actual_other_weight;
                // Update village needs
                village.other_supplies_needed -= other_packets;
                available_weight -= actual_other_weight;
                // Update trip pickups
                trip.other_supplies_pickup += other_packets;
                drop_added = true;
            }
        }

        // If we didn't prioritize food initially, try to allocate remaining food
        if (!prioritize_food && temp_food_needed > 0 && available_weight > 0) {
            // Use remaining weight for food with same percentage split
            double remaining_dry_weight = available_weight * food_percentage;
            double remaining_perishable_weight = available_weight * (1.0 - food_percentage);
            int additional_dry = min(temp_food_needed, (int)(remaining_dry_weight / dry_weight));
            int additional_perishable = min(temp_food_needed - additional_dry, 
                                           (int)(remaining_perishable_weight / perishable_weight));
            double additional_weight = additional_dry * dry_weight + additional_perishable * perishable_weight;
            if (additional_weight <= available_weight && (additional_dry > 0 || additional_perishable > 0)) {
                drop.dry_food += additional_dry;
                drop.perishable_food += additional_perishable;
                weight_used += additional_weight;
                // Update village needs
                village.food_needed -= (additional_dry + additional_perishable);
                // Update trip pickups
                trip.dry_food_pickup += additional_dry;
                trip.perishable_food_pickup += additional_perishable;
                drop_added = true;
            }
        }

        // Only add drop if we're actually delivering something
        if (drop_added && (drop.dry_food > 0 || drop.perishable_food > 0 || drop.other_supplies > 0)) {
            trip.drops.push_back(drop);
            // Update trip distance to maintain round-trip calculation
            if(trip.drops.size() == 1) {
                // First village: home -> village + return distance
                trip.distance_covered = distance(problem.cities[helicopter.home_city_id-1], village.coords) + distance(village.coords, problem.cities[helicopter.home_city_id-1]);
            } else {
                // Additional villages: remove old return distance, add segment to new village, add new return distance
                Point current_pos = problem.villages[trip.drops[trip.drops.size()-2].village_id -1].coords;
                double dist_to_village = distance(current_pos, village.coords);
                trip.distance_covered = trip.distance_covered - distance(current_pos, problem.cities[helicopter.home_city_id-1]) + dist_to_village + distance(village.coords, problem.cities[helicopter.home_city_id-1]);
            }
            trip.weight_carried += weight_used;
            got_a_village=true;
        }

        if(!trip.drops.empty()){ 
            helicopter.total_distance_covered += trip.distance_covered;
        }
        break;
    }
}

Solution GET_RANDOM_STATE(ProblemData& problem, int restart_counter, const std::vector<double>& ratio_arr, bool random_near_village){
    Solution solution;
    for(int i=0;i<problem.helicopters.size();i++){
        HelicopterPlan heli_plan;
        heli_plan.helicopter_id = problem.helicopters[i].id;
        Trip empty_trip;
        empty_trip.dry_food_pickup = 0;
        empty_trip.perishable_food_pickup = 0;
        empty_trip.other_supplies_pickup = 0;
        empty_trip.distance_covered = 0.0;
        empty_trip.weight_carried = 0.0;
        heli_plan.trips.push_back(empty_trip);
        solution.push_back(heli_plan);
    } 
    // returning empty solution for now
    if(restart_counter==1) return solution;  
    if(random_near_village){
        RANDOM_NEARBY_VILLAGE(solution, problem, ratio_arr); 
        return solution; 
    } 
    for(int h =0;h<problem.helicopters.size();h++){
        // Get the distribution ratio for this helicopter from ratio_arr
        double food_percentage = ratio_arr[h];
        RANDOM1(solution, problem, h, food_percentage); 
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
        RAND_INIT_STATS[h].num_trips = std::max(1, (int)(heli_plan.trips.size()));

        RAND_INIT_STATS[h].num_drops_per_trip.clear();
        for(const auto& trip:heli_plan.trips){
            RAND_INIT_STATS[h].num_drops_per_trip.push_back(std::max(1, (int)(trip.drops.size())));
        }
    } 
}

void RESET_PROBLEM_WITH_THIS_SOLUTION(Solution& solution, ProblemData& problem){
    RESET_PROBLEM(problem);
    for(auto& heli_plan:solution){
        
        for(auto& trip:heli_plan.trips){
            for(auto& drop:trip.drops){
                problem.villages[drop.village_id-1].food_needed -= (drop.dry_food + drop.perishable_food);
                problem.villages[drop.village_id-1].other_supplies_needed -= drop.other_supplies;   
            }
            problem.helicopters[heli_plan.helicopter_id-1].total_distance_covered -= trip.distance_covered;
        }
    }
}

bool IS_FEASIBLE(const Solution& solution, const ProblemData& problem) {
    cout << "\n=== FEASIBILITY CHECK START ===" << endl;
    bool is_feasible = true;
    
    // Track total supplies delivered to each village for capping logic
    vector<double> food_delivered(problem.villages.size() + 1, 0.0);
    vector<double> other_delivered(problem.villages.size() + 1, 0.0);
    vector<double> HELI_TOTO_TRACKs(problem.helicopters.size() + 1, 0.0);
    
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
            
            HELI_TOTO_TRACKs[heli_id] += calculated_distance;
        }
        
        // Check total distance constraint (d_max)
        cout << "\n  Total distance for helicopter " << heli_id << ": " 
             << HELI_TOTO_TRACKs[heli_id] << " / " << problem.d_max << endl;
        
        if (HELI_TOTO_TRACKs[heli_id] > problem.d_max + 1e-9) {
            cout << "  ❌ TOTAL DISTANCE VIOLATION: Helicopter " << heli_id 
                 << " total distance " << HELI_TOTO_TRACKs[heli_id] 
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
            cout << "  Food needed var: " << village.food_needed<<endl;
            cout << "  Other needed var: " << village.other_supplies_needed<<endl;
            if (food_delivered[i] > max_food_needed + 1e-9) {
                cout << "  ⚠  WARNING: Excess food delivery (will be capped in scoring)" << endl;
            }
            if (other_delivered[i] > max_other_needed + 1e-9) {
                cout << "  ⚠  WARNING: Excess other supplies (will be capped in scoring)" << endl;
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