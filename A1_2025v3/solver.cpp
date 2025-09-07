#include "solver.h"
#include "helper.h"
#include <iostream>
#include <chrono>
#include <unordered_map>
#include <map>
#include <algorithm>
#include <random>

std::random_device rd;
std::mt19937 gen(rd()); // Mersenne Twister engine
std::uniform_real_distribution<> dist(0.0, 1.0);

using namespace std;
// You can add any helper functions or classes you need here.
constexpr double EPSILON = 1e-6;
/**
 * @brief The main function to implement your search/optimization algorithm.
 * * This is a placeholder implementation. It creates a simple, likely invalid,
 * plan to demonstrate how to build the Solution object. 
 * * TODO: REPLACE THIS ENTIRE FUNCTION WITH YOUR ALGORITHM.
 */
 
struct ExtResult {
    Drop drop;
    double value_increase;
    double distance_extension;
    
    // Constructor
    ExtResult(Drop d, double v, double dist)
        : drop(d), value_increase(v), distance_extension(dist) {}
};

int TOT_VILLAGES,TOT_HELICOPTERS,D_MAX;
int DRY_WT,WET_WT,OTHER_WT;
int DRY_VAL,WET_VAL,OTHER_VAL;


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
                
                total_value += drop.dry_food * DRY_VAL;
                total_value += drop.perishable_food * WET_VAL;
                total_value += drop.other_supplies * OTHER_VAL;
            }
            // Calculate cost from distance
            total_cost += heli_ptr->alpha * trip.distance_covered + heli_ptr->fixed_cost;
        }
    }
    return total_value - total_cost;
}

//BOOOOOOOOOOOOOOOOOOSTERRRRRRRRRRRRRR
std::vector<int> solveSimpleTSP(const std::vector<int>& village_ids, const ProblemData& problem) {
    if (village_ids.size() <= 1) return village_ids;
    
    std::vector<int> tour;
    std::vector<bool> visited(village_ids.size(), false);
    
    // Start from first village
    int current_idx = 0;
    tour.push_back(village_ids[current_idx]);
    visited[current_idx] = true;
    
    // Nearest neighbor heuristic
    for (int step = 1; step < (int)village_ids.size(); step++) {
        double min_dist = std::numeric_limits<double>::max();
        int next_idx = -1;
        
        Point current_pos = problem.villages[village_ids[current_idx] - 1].coords;
        
        for (int i = 0; i < (int)village_ids.size(); i++) {
            if (!visited[i]) {
                Point candidate_pos = problem.villages[village_ids[i] - 1].coords;
                double dist = distance(current_pos, candidate_pos);
                
                if (dist < min_dist) {
                    min_dist = dist;
                    next_idx = i;
                }
            }
        }
        
        if (next_idx != -1) {
            tour.push_back(village_ids[next_idx]);
            visited[next_idx] = true;
            current_idx = next_idx;
        }
    }
    
    return tour;
}

// Advanced TSP solver placeholder (replace with Christofides implementation)
std::vector<int> solveChristofidesTSP(const std::vector<int>& village_ids, const ProblemData& problem) {
    // TODO: Implement Christofides algorithm here
    // For now, use simple nearest neighbor
    return solveSimpleTSP(village_ids, problem);
}

void BOOSTER_FUNCTION(Solution& solution, ProblemData& problem) {
    for (auto& heli_plan : solution) {
        for (auto& trip : heli_plan.trips) {
            
            // Skip empty trips
            if (trip.drops.empty()) continue;
            
            // **PHASE 1: Route Optimization using TSP**
            
            // Extract village IDs from drops
            std::vector<int> village_ids;
            for (const auto& drop : trip.drops) {
                village_ids.push_back(drop.village_id);
            }
            
            // Get optimized route using TSP solver
            std::vector<int> optimized_route = solveChristofidesTSP(village_ids, problem);
            
            // Reorder drops according to optimized route
            std::vector<Drop> reordered_drops;
            for (int village_id : optimized_route) {
                auto it = std::find_if(trip.drops.begin(), trip.drops.end(),
                                     [&](const Drop& d) { return d.village_id == village_id; });
                if (it != trip.drops.end()) {
                    reordered_drops.push_back(*it);
                }
            }
            trip.drops = reordered_drops;
            
            // Recalculate trip distance based on optimized route
            const Helicopter& helicopter = problem.helicopters[heli_plan.helicopter_id - 1];
            Point home_city = problem.cities[helicopter.home_city_id - 1];
            Point current_pos = home_city;
            double total_distance = 0.0;
            
            for (const auto& drop : trip.drops) {
                const Village& village = problem.villages[drop.village_id - 1];
                total_distance += distance(current_pos, village.coords);
                current_pos = village.coords;
            }
            total_distance += distance(current_pos, home_city);  // Return to home
            
            // Update helicopter's total distance
            double old_distance = trip.distance_covered;
            trip.distance_covered = total_distance;
            const_cast<Helicopter&>(helicopter).total_distance_covered += (total_distance - old_distance);
            
            // **PHASE 2: Convert Perishable to Dry Food**
            
            // **PHASE 2: Convert Perishable to Dry Food (Simple Approach)**

            for (auto& drop : trip.drops) {
                // Village& village = problem.villages[drop.village_id - 1];
                
                // Keep converting while we have perishable food and village needs more food
                while (drop.perishable_food > 0 && problem.villages[drop.village_id - 1].food_needed > 0) {
                    double perishable_weight = problem.packages[1].weight;
                    double dry_weight = problem.packages[0].weight;
                    
                    // How many dry packets can we get by removing 1 perishable packet?
                    int dry_packets_gained = static_cast<int>(perishable_weight / dry_weight);
                    
                    // Only convert if we gain more dry packets than we lose perishable packets
                    if (dry_packets_gained > 1) {
                        // Limit by village need
                        int actual_dry_to_add = std::min(dry_packets_gained, problem.villages[drop.village_id - 1].food_needed);
                        
                        // Make the conversion: remove 1 perishable, add multiple dry
                        drop.perishable_food -= 1;
                        drop.dry_food += actual_dry_to_add;
                        
                        // Update trip pickups
                        trip.perishable_food_pickup -= 1;
                        trip.dry_food_pickup += actual_dry_to_add;
                        
                        // Update village needs
                        problem.villages[drop.village_id - 1].food_needed -= (actual_dry_to_add);
                        
                        // Update weight (trip gets lighter since dry is lighter)
                        trip.weight_carried -= perishable_weight;
                        trip.weight_carried += actual_dry_to_add * dry_weight;
                    } else {
                        break; // No beneficial conversion possible
                    }
                }
            }

            
            // Recalculate trip weight to ensure accuracy
            trip.weight_carried = 0.0;
            for (const auto& drop : trip.drops) {
                trip.weight_carried += drop.dry_food * problem.packages[0].weight +
                                     drop.perishable_food * problem.packages[1].weight +
                                     drop.other_supplies * problem.packages[2].weight;
            }
        }
    }
}

 

ExtResult evaluate_extension(const Trip& trip, const Helicopter& helicopter, 
                           const Village& new_village, ProblemData& problem, 
                           double food_percentage) {
    
    // Get helicopter info
    Point home_city = problem.cities[helicopter.home_city_id -1];
    bool empty_trip = false;
    
    // Check distance constraint
    double distance_extension = 0.0;
    
    if (!trip.drops.empty()) {
        int last_village_id = trip.drops.back().village_id;
        int last_index = last_village_id -1;
        Village* last_village = &problem.villages[last_index];
        
        double dist_last_to_new = distance(last_village->coords, new_village.coords);
        double dist_new_to_home = distance(new_village.coords, home_city);
        double dist_last_to_home = distance(last_village->coords, home_city);
        
        distance_extension = dist_last_to_new + dist_new_to_home - dist_last_to_home;
    } else {
        distance_extension = 2.0 * distance(new_village.coords, home_city);
        empty_trip = true;
    }
    
    // Check feasibility constraints
    if (trip.distance_covered + distance_extension > helicopter.distance_capacity ) {
        return ExtResult({-1, 0, 0, 0}, -1e9, 0.0);
    }

    if (helicopter.total_distance_covered + distance_extension > problem.d_max ) {
        return ExtResult({-1, 0, 0, 0}, -1e9, 0.0);
    }
    
    double remaining_weight = helicopter.weight_capacity - trip.weight_carried;
    if (remaining_weight <= 0) {
        return ExtResult({-1, 0, 0, 0}, -1e9, 0.0);
    }
    
    // Get package weights and values 
    // Calculate value/weight ratios
    double dry_ratio = (double)problem.packages[0].value / problem.packages[0].weight;
    double perishable_ratio = (double)problem.packages[1].value / problem.packages[1].weight;
    double other_ratio = (double)problem.packages[2].value / problem.packages[2].weight;
    
    // Calculate expected food ratio based on weight percentage
    double expected_food_ratio = food_percentage * dry_ratio + (1.0 - food_percentage) * perishable_ratio;
    
    // Prioritize food if it has better or equal value/weight ratio
    bool prioritize_food = expected_food_ratio >= other_ratio;
    
    // Initialize drop
    Drop new_drop;
    new_drop.village_id = new_village.id;
    new_drop.dry_food = 0;
    new_drop.perishable_food = 0;
    new_drop.other_supplies = 0;
    
    double weight_to_drop = 0.0;
    double value_gained = 0.0;
    double available_weight = remaining_weight;
    
    int temp_food_needed = new_village.food_needed;
    int temp_other_needed = new_village.other_supplies_needed;
    
    // Allocate based on priority
    if (prioritize_food && temp_food_needed > 0 && available_weight > 0) {
        // Allocate food first - split by WEIGHT percentage
        
        // Calculate target weights for dry and perishable food
        double target_dry_weight = available_weight * food_percentage;
        double target_perishable_weight = available_weight * (1.0 - food_percentage);
        
        // Calculate packet counts based on weight targets
        int dry_packets = (int)(target_dry_weight / problem.packages[0].weight);
        int perishable_packets = (int)(target_perishable_weight / problem.packages[1].weight);
        
        // Constrain by village needs
        dry_packets = min(dry_packets, temp_food_needed);
        perishable_packets = min(perishable_packets, temp_food_needed - dry_packets);
        
        // Calculate actual weights
        double actual_dry_weight = dry_packets * problem.packages[0].weight;
        double actual_perishable_weight = perishable_packets * problem.packages[1].weight;
        double total_food_weight = actual_dry_weight + actual_perishable_weight;
        
        // Check if it fits in available weight
        if (total_food_weight <= available_weight && (dry_packets > 0 || perishable_packets > 0)) {
            new_drop.dry_food = dry_packets;
            new_drop.perishable_food = perishable_packets;
            
            weight_to_drop += total_food_weight;
            value_gained += dry_packets * problem.packages[0].value + perishable_packets * problem.packages[1].value;
            
            temp_food_needed -= (dry_packets + perishable_packets);
            available_weight -= total_food_weight;
        }
    }
    
    // Allocate other supplies with remaining capacity
    if (temp_other_needed > 0 && available_weight >= problem.packages[2].weight) {
        int other_packets = min(temp_other_needed, (int)(available_weight / problem.packages[2].weight));
        
        if (other_packets > 0) {
            new_drop.other_supplies = other_packets;
            double actual_other_weight = other_packets * problem.packages[2].weight;
            
            weight_to_drop += actual_other_weight;
            value_gained += other_packets * problem.packages[2].value;
            available_weight -= actual_other_weight;
        }
    }
    
    // If we didn't prioritize food initially, try to allocate remaining food
    if (!prioritize_food && temp_food_needed > 0 && available_weight > 0) {
        // Use remaining weight for food with same percentage split
        double remaining_dry_weight = available_weight * food_percentage;
        double remaining_perishable_weight = available_weight * (1.0 - food_percentage);
        
        int additional_dry = min(temp_food_needed, (int)(remaining_dry_weight / problem.packages[0].weight));
        int additional_perishable = min(temp_food_needed - additional_dry, 
                                       (int)(remaining_perishable_weight / problem.packages[1].weight));
        
        double additional_weight = additional_dry * problem.packages[0].weight + additional_perishable * problem.packages[1].weight;
        
        if (additional_weight <= available_weight && (additional_dry > 0 || additional_perishable > 0)) {
            new_drop.dry_food += additional_dry;
            new_drop.perishable_food += additional_perishable;
            
            weight_to_drop += additional_weight;
            value_gained += additional_dry * problem.packages[0].value + additional_perishable * problem.packages[1].value;
        }
    }
    
    // Calculate net value increase
    double cost_increase = helicopter.alpha * distance_extension;
    double net_value_increase = value_gained - cost_increase;
    
    if (empty_trip) {
        net_value_increase -= helicopter.fixed_cost;
    }
    
    if (weight_to_drop > 0) {
        return ExtResult(new_drop, net_value_increase, distance_extension);
    } else {
        return ExtResult({-1, 0, 0, 0}, -1e9, 0.0);
    }
}


bool find_best_extension(Trip& current_trip, Helicopter& helicopter,
                                    ProblemData& problem, double percent) {
    
    double max_value_increase = 0;
    Drop best_drop = {-1, 0, 0, 0}; // Invalid drop initially
    int best_village_id = -1;
    ExtResult best_result = ExtResult({-1, 0, 0, 0}, 0.0, 0.0);
    
    // Evaluate all villages
    for (const auto& village : problem.villages) {
        // Skip if already visited in this trip
        bool already_visited = false;
        for (const auto& drop : current_trip.drops) {
            if (drop.village_id == village.id) {
                already_visited = true;
                break;
            }
        }
        if (already_visited) continue;
        
        // Evaluate this extension
        auto result = evaluate_extension(
            current_trip, helicopter, village, problem, percent);
        
        auto potential_drop=result.drop;
        auto value_increase=result.value_increase;
        if (value_increase > max_value_increase) {
            max_value_increase = value_increase;
            best_drop = potential_drop;
            best_village_id = village.id;
            best_result=result;
        }
    }
    int ret=false;
    if (best_village_id == -1) {
        return ret; // No valid extension found
    }

    current_trip.distance_covered+=best_result.distance_extension;
    // current_trip.weight_carried+=best_result.weight_dropped;
    helicopter.total_distance_covered+=best_result.distance_extension;
    problem.villages[best_village_id-1].food_needed-= (best_drop.dry_food + best_drop.perishable_food);
    problem.villages[best_village_id-1].other_supplies_needed-= best_drop.other_supplies;
    current_trip.dry_food_pickup += best_drop.dry_food;
    current_trip.perishable_food_pickup += best_drop.perishable_food;  
    current_trip.other_supplies_pickup += best_drop.other_supplies;
    if(current_trip.drops.empty()){
        ret=true;
    }
    current_trip.drops.push_back(best_drop);


    // Helper function to compute total weight for a trip
    auto compute_trip_weight = [](const Trip &trip, const ProblemData &problem) {
        double total_weight = 0.0;

        total_weight += trip.dry_food_pickup * problem.packages[0].weight + trip.perishable_food_pickup * problem.packages[1].weight + trip.other_supplies_pickup * problem.packages[2].weight;
        
        return total_weight;
    };

    current_trip.weight_carried = compute_trip_weight(current_trip, problem);
    return ret;

}

void S1(Solution& solution, ProblemData& problem, vector<double> ratio_arr ){
    // extend a trip function
    // selecting a random helicopter
    int heli_index = rand() % problem.helicopters.size();
    auto& heli_plan = solution[heli_index];
    auto& helicopter = problem.helicopters[heli_index];
    // selecting a random trip
    int trip_index = rand() % heli_plan.trips.size();
    auto& trip = heli_plan.trips[trip_index];
    double percent = ratio_arr[heli_index];
    bool create= find_best_extension(trip, helicopter, problem, percent);
    if (create){
        Trip empty_trip = {
                            0,              // dry_food_pickup
                            0,              // perishable_food_pickup
                            0,              // other_supplies_pickup
                            {},             // drops (empty vector)
                            0.0,            // distance_covered
                            0.0             // weight_carried
};

        heli_plan.trips.push_back(empty_trip);
    }
}


void SUCCESSOR_FUNCTION(Solution& solution, ProblemData& problem, vector<double> ratio_arr ){
    // choosing a random successor function
    S1(solution, problem, ratio_arr);

}



double computeTemperature(double deltaV,
                          double p0, double pend,
                          chrono::high_resolution_clock::time_point start,
                          chrono::high_resolution_clock::time_point end,
                          chrono::high_resolution_clock::time_point now) {
    // Initial and final temperatures from ΔV and acceptance probabilities
    double T0   = -deltaV / log(p0);
    double Tend = -deltaV / log(pend);

    // Total time (seconds)
    chrono::duration<double> total = end - start;
    chrono::duration<double> elapsed = now - start;

    double frac = elapsed / total; // progress fraction [0,1]

    // Exponential cooling schedule
    // T(t) = T0 * (Tend / T0)^frac
    double T = T0 * pow(Tend / T0, frac);

    return T;
}



// using list = vector<int>;

Solution RANDOM_RESTART_LOCAL_SEARCH(ProblemData& problem,
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time,
    std::chrono::time_point<std::chrono::high_resolution_clock, std::chrono::duration<double>> end_time)
{
    Solution best_solution;
    double best_value = -1e18;
    const int helios=problem.helicopters.size();
    std::vector<double> ratio_list(helios,1.0), old_ratio_list(helios,1.0);
    std::vector<double> best_ratio_list(helios, 0.0);
    int restarts = 1;
    bool improved=false; 
    double Vmax = 0.0;
    double Vmin = 0.0;
    double delV = 0.0;
    double current_value = 0.0;
    std::chrono::high_resolution_clock::time_point end_time2 = std::chrono::time_point_cast<std::chrono::high_resolution_clock::duration>(end_time);
    
    bool is_empty_peak = false;
    while (std::chrono::high_resolution_clock::now() < end_time){ //
        // cout<<restarts; 
        
        if (!improved){

            if (restarts>12){
                if (restarts==13){
                    start_time=std::chrono::high_resolution_clock::now();
                }
                double Temperature = computeTemperature(delV, 0.8, 0.01,
                                                start_time, end_time2, std::chrono::high_resolution_clock::now());

                // ΔE = how much worse the new solution is compared to best
                double deltaE = current_value - best_value;
                if (deltaE < 0) { 
                    // Acceptance probability
                    double p = exp(deltaE / Temperature);
                    // Draw random [0,1]
                    double r = (double) rand() / RAND_MAX;
                    ratio_list=old_ratio_list;
                    if (r < p) {
                    } else {
                        ratio_list = old_ratio_list;
                    }
                }
            }
            else{
                ratio_list=old_ratio_list;
            }
            
        }

        if (restarts==2){
            ratio_list.assign(helios, 0.9);
            Vmax=current_value;
            Vmin=current_value;
        }
        // if (restarts==3){
        //     ratio_list.assign(helios, 0.0);
        //     if (current_value> Vmax){
        //         Vmax= current_value;
        //     }
        //     if (current_value< Vmin){
        //         Vmin= current_value;
        //     }
        // }
        if (restarts<=11 && restarts>2){
            ratio_list.assign(helios, 1.0-0.1*(restarts-1));
            if (current_value> Vmax){
                Vmax= current_value;
            }
            if (current_value< Vmin){
                Vmin= current_value;
            }
        }
        if (restarts==12){
            if (current_value> Vmax){
                Vmax= current_value;
            }
            if (current_value< Vmin){
                Vmin= current_value;
            }
            delV=Vmax-Vmin;
            
        }
        
        if (restarts>=12){
            int rat_index = rand() % helios;
            double new_ratio = dist(gen);
            old_ratio_list = ratio_list;
            ratio_list[rat_index]=new_ratio;
        } 
        RESET_PROBLEM(problem);
        Solution current_solution;
        current_solution = GET_RANDOM_STATE(problem,restarts,ratio_list,is_empty_peak);

        current_value = EVALUATE_VALUE(problem, current_solution);
        int a=(250*int(problem.villages.size()+ problem.helicopters.size()));
        int local_iterations = a;
        improved=false;
        while(local_iterations--) {
            SUCCESSOR_FUNCTION(current_solution, problem, ratio_list);        

            current_value = EVALUATE_VALUE(problem, current_solution);
            if (current_value > best_value) {
                
                best_value = current_value;
                improved=true;
                best_solution = current_solution;
                best_ratio_list = ratio_list;
                local_iterations = a; // Reset local iterations on improvement
            }
        }

        // BOOSTER_FUNCTION(best_solution, problem);
        // current_value = EVALUATE_VALUE(problem, current_solution);
        //     if (current_value > best_value) {
                
        //         best_value = current_value;
        //         improved=true;
        //         best_solution = current_solution;
        //         best_ratio_list = ratio_list;
        //     }
        UPDATE_RANDOM_STATS(problem, best_solution);
        // if(restarts==1){
        //     // getting the random soultion constarins for each helicopter
        // }
        if(best_value==0){
            // still best is zero mean zero state is the best state..
            // lets try i will try again with randowm assigning near villages
            is_empty_peak = true;
        }
        cout<<best_value<<": ";
        for (auto&e:best_ratio_list){
            cout << e << " "; 
        }
        cout<<"\n";
        restarts++;
    }
    // for (auto&e:best_ratio_list){
    // cout << e << "\n"; 
    // }
    // Remove empty trips from the solution
    for (auto& heli_plan : best_solution) {
        heli_plan.trips.erase(
            std::remove_if(heli_plan.trips.begin(), heli_plan.trips.end(),
                [](const Trip& trip) {
                    // Remove trips with zero pickups and no drops
                    return (trip.dry_food_pickup == 0 && 
                            trip.perishable_food_pickup == 0 && 
                            trip.other_supplies_pickup == 0 && 
                            trip.drops.empty());
                }),
            heli_plan.trips.end()
        );
    }

    return best_solution;
}

Solution solve(ProblemData& problem) {
    auto start_time = std::chrono::high_resolution_clock::now();
    double seconds = 58.0 * problem.time_limit_minutes; //
    auto time_limit = std::chrono::duration<double>(seconds);
    auto end_time= time_limit+start_time;

    TOT_VILLAGES=problem.villages.size();
    TOT_HELICOPTERS=problem.helicopters.size();
    D_MAX=problem.d_max;
    DRY_WT=problem.packages[0].weight;
    DRY_VAL=problem.packages[0].value;
    WET_WT=problem.packages[1].weight;
    WET_VAL=problem.packages[1].value;
    OTHER_WT=problem.packages[2].weight;
    OTHER_VAL=problem.packages[2].value;

    cout << "Starting solver..." << endl;
    RESET_PROBLEM(problem);
    RAND_INIT_STATS.resize(problem.helicopters.size());
    Solution solution; 
    
    solution = RANDOM_RESTART_LOCAL_SEARCH(problem,start_time,end_time);
    cout<<"Value of solution: "<<EVALUATE_VALUE(problem, solution)<<endl;
    
    // Check feasibility of the solution
    // IS_FEASIBLE(solution, problem);
    
    cout << "Solver finished." << endl;
    return solution;
}

 