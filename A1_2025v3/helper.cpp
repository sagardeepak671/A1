#include "helper.h"
#include <iostream>
#include <vector>
using namespace std;

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
