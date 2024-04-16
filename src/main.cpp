#define CROW_MAIN
#define CROW_STATIC_DIR "../public"

#include "crow_all.h"
#include "json.hpp"
#include <random>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <latch>

static const uint32_t NUM_ROWS = 15;

// Constants
const uint32_t PLANT_MAXIMUM_AGE = 10;
const uint32_t HERBIVORE_MAXIMUM_AGE = 50;
const uint32_t CARNIVORE_MAXIMUM_AGE = 80;
const uint32_t MAXIMUM_ENERGY = 200;
const uint32_t THRESHOLD_ENERGY_FOR_REPRODUCTION = 20;
const uint32_t MOVE_ENERGY = 5;
const uint32_t CARNIVORE_ENERGY_GAIN = 20;
const uint32_t HERBIVORE_ENERGY_GAIN = 30;
const uint32_t REPRODUCTION_ENERGY = 10;
const uint32_t START_ENERGY = 100;


// Probabilities
const double PLANT_REPRODUCTION_PROBABILITY = 0.2;
const double HERBIVORE_REPRODUCTION_PROBABILITY = 0.075;
const double CARNIVORE_REPRODUCTION_PROBABILITY = 0.025;
const double HERBIVORE_MOVE_PROBABILITY = 0.7;
const double HERBIVORE_EAT_PROBABILITY = 0.9;
const double CARNIVORE_MOVE_PROBABILITY = 0.5;
const double CARNIVORE_EAT_PROBABILITY = 1.0;

// Type definitions
enum entity_type_t
{
    empty,
    plant,
    herbivore,
    carnivore
};

struct pos_t
{
    uint32_t i;
    uint32_t j;
};

struct entity_t
{
    entity_type_t type;
    int32_t energy;
    int32_t age;
};

// Auxiliary code to convert the entity_type_t enum to a string
NLOHMANN_JSON_SERIALIZE_ENUM(entity_type_t, {
                                                {empty, " "},
                                                {plant, "P"},
                                                {herbivore, "H"},
                                                {carnivore, "C"},
                                            })

// Auxiliary code to convert the entity_t struct to a JSON object
namespace nlohmann
{
    void to_json(nlohmann::json &j, const entity_t &e)
    {
        j = nlohmann::json{{"type", e.type}, {"energy", e.energy}, {"age", e.age}};
    }
}

// Grid that contains the entities
static std::vector<std::vector<entity_t>> entity_grid;

std::default_random_engine gen;
std::uniform_int_distribution<> rand_pos(0, NUM_ROWS);
std::uniform_real_distribution<> mp_rand(0.0, 1.0);

std::atomic<int> thread_counter = 0;

std::mutex exec_it_mtx;
std::latch *sync_it;

void plant_routine(pos_t pos) {
    entity_t& plant = entity_grid[pos.i][pos.j];

    while(true) {
        (*sync_it).arrive_and_wait();
        std::unique_lock lk(exec_it_mtx);
        if(plant.type == empty or plant.age == PLANT_MAXIMUM_AGE) {
            thread_counter --;
            entity_grid[pos.i][pos.j] = {empty, 0, 0};
            return;
        }

        std::vector<pos_t> empty_pos;
        for(const pos_t& pos_to_verify : {pos_t(pos.i, pos.j+1), pos_t(pos.i,pos.j-1), pos_t(pos.i+1,pos.j), pos_t(pos.i-1,pos.j)}) {

            if(!(pos_to_verify.i >= 0 and pos_to_verify.i < NUM_ROWS and pos_to_verify.j >= 0 and pos_to_verify.j < NUM_ROWS)) {
				continue;
            }
            
            if(entity_grid[pos_to_verify.i][pos_to_verify.j].type == empty)
                empty_pos.push_back(pos_to_verify);
        }


        if(mp_rand(gen) < PLANT_REPRODUCTION_PROBABILITY) {

            size_t idx = mp_rand(gen) * empty_pos.size();
            std::vector<pos_t>::iterator idx_it = empty_pos.begin() + idx;

            pos_t child_pos = empty_pos[idx];

            entity_grid[child_pos.i][child_pos.j] = {entity_type_t::plant, 0, 0};
            std::thread t(plant_routine, child_pos);
            t.detach();
            thread_counter ++;
        }
        
        plant.age ++;
    }
}

void herbi_routine(pos_t pos) {
    entity_t& herbi = entity_grid[pos.i][pos.j];

    while(true) {
        (*sync_it).arrive_and_wait();
        std::unique_lock lk(exec_it_mtx);
        if(herbi.type == empty or herbi.energy == 0 or herbi.age == HERBIVORE_MAXIMUM_AGE) {
            thread_counter --;
            entity_grid[pos.i][pos.j] = {empty, 0, 0};
            return;
        }

        std::vector<pos_t> empty_pos;
        for(const pos_t& pos_to_verify : {pos_t(pos.i, pos.j+1), pos_t(pos.i,pos.j-1), pos_t(pos.i+1,pos.j), pos_t(pos.i-1,pos.j)}) {

            if(!(pos_to_verify.i >= 0 and pos_to_verify.i < NUM_ROWS and pos_to_verify.j >= 0 and pos_to_verify.j < NUM_ROWS)) {
				continue;
            }

            if(entity_grid[pos_to_verify.i][pos_to_verify.j].type == plant and mp_rand(gen) < HERBIVORE_EAT_PROBABILITY) {
                entity_grid[pos_to_verify.i][pos_to_verify.j] = {empty, 0, 0};
                herbi.energy += HERBIVORE_ENERGY_GAIN;
            }
            
            if(entity_grid[pos_to_verify.i][pos_to_verify.j].type == empty)
                empty_pos.push_back(pos_to_verify);
        }

        if(mp_rand(gen) < HERBIVORE_REPRODUCTION_PROBABILITY and herbi.energy > THRESHOLD_ENERGY_FOR_REPRODUCTION) {

            size_t idx = mp_rand(gen) * empty_pos.size();
            std::vector<pos_t>::iterator idx_it = empty_pos.begin() + idx;

            pos_t child_pos = empty_pos[idx];

            entity_grid[child_pos.i][child_pos.j] = {entity_type_t::herbivore, START_ENERGY, 0};
            std::thread t(herbi_routine, child_pos);
            t.detach();
            thread_counter ++;

            empty_pos.erase(idx_it);

            herbi.energy -= REPRODUCTION_ENERGY;
        }

        if(mp_rand(gen) < HERBIVORE_MOVE_PROBABILITY) {

            size_t idx = mp_rand(gen) * empty_pos.size();

            entity_grid[pos.i][pos.j] = {empty, 0, 0};
            pos = empty_pos[idx];
            herbi.energy -= MOVE_ENERGY;
        }

        herbi.age ++;
    }
    
}

void carni_routine(pos_t pos) {
    entity_t& carni = entity_grid[pos.i][pos.j];

    while(true) {

        (*sync_it).arrive_and_wait();
        std::unique_lock lk(exec_it_mtx);
        if(carni.type == empty or carni.energy == 0 or carni.age == CARNIVORE_MAXIMUM_AGE) {
            thread_counter --;
            entity_grid[pos.i][pos.j] = {empty, 0, 0};
            return;
        }

        std::vector<pos_t> empty_pos;
        for(const pos_t& pos_to_verify : {pos_t(pos.i, pos.j+1), pos_t(pos.i,pos.j-1), pos_t(pos.i+1,pos.j), pos_t(pos.i-1,pos.j)}) {

            if(!(pos_to_verify.i >= 0 and pos_to_verify.i < NUM_ROWS and pos_to_verify.j >= 0 and pos_to_verify.j < NUM_ROWS)) {
				continue;
            }

            if(entity_grid[pos_to_verify.i][pos_to_verify.j].type == herbivore and mp_rand(gen) < CARNIVORE_EAT_PROBABILITY) {
                entity_grid[pos_to_verify.i][pos_to_verify.j] = {empty, 0, 0};
                carni.energy += CARNIVORE_ENERGY_GAIN;
            }
            
            if(entity_grid[pos_to_verify.i][pos_to_verify.j].type == empty)
                empty_pos.push_back(pos_to_verify);
        }

        if(mp_rand(gen) < CARNIVORE_REPRODUCTION_PROBABILITY and carni.energy > THRESHOLD_ENERGY_FOR_REPRODUCTION) {

            size_t idx = mp_rand(gen) * empty_pos.size();
            std::vector<pos_t>::iterator idx_it = empty_pos.begin() + idx;

            pos_t child_pos = empty_pos[idx];

            entity_grid[child_pos.i][child_pos.j] = {entity_type_t::carnivore, START_ENERGY, 0};
            std::thread t(carni_routine, child_pos);
            t.detach();
            thread_counter ++;

            empty_pos.erase(idx_it);

            carni.energy -= REPRODUCTION_ENERGY;
        }

        if(mp_rand(gen) < CARNIVORE_MOVE_PROBABILITY) {

            size_t idx = mp_rand(gen) * empty_pos.size();

            entity_grid[pos.i][pos.j] = {empty, 0, 0};
            pos = empty_pos[idx];
            carni.energy -= MOVE_ENERGY;
        }

        carni.age ++;
    }

}

int main()
{
    crow::SimpleApp app;

    // Endpoint to serve the HTML page
    CROW_ROUTE(app, "/")
    ([](crow::request &, crow::response &res)
     {
        // Return the HTML content here
        res.set_static_file_info_unsafe("../public/index.html");
        res.end(); });

    CROW_ROUTE(app, "/start-simulation")
        .methods("POST"_method)([](crow::request &req, crow::response &res)
                                { 
        // Parse the JSON request body
        nlohmann::json request_body = nlohmann::json::parse(req.body);

       // Validate the request body
        uint32_t num_plant = request_body["plants"],
                 num_herbi = request_body["herbivores"],
                 num_carni = request_body["carnivores"];

        uint32_t total_entinties = num_plant + num_herbi + num_carni;
        if (total_entinties > NUM_ROWS * NUM_ROWS) {
        res.code = 400;
        res.body = "Too many entities";
        res.end();
        return;
        }

        // Clear the entity grid
        entity_grid.clear();
        entity_grid.assign(NUM_ROWS, std::vector<entity_t>(NUM_ROWS, { empty, 0, 0}));
        
        // Create the entities
        pos_t creation_pos;

        for(size_t idx = 0; idx != num_plant; idx ++) {
            creation_pos.i = rand_pos(gen);
            creation_pos.j = rand_pos(gen);
            entity_grid[creation_pos.i][creation_pos.j] = {plant, START_ENERGY, 0};
            std::thread t(plant_routine, creation_pos);
            t.detach();
            thread_counter ++;
        }

        for(size_t idx = 0; idx != num_herbi; idx ++) {
            creation_pos.i = rand_pos(gen);
            creation_pos.j = rand_pos(gen);
            entity_grid[creation_pos.i][creation_pos.j] = {herbivore, START_ENERGY, 0};
            std::thread t(herbi_routine, creation_pos);
            t.detach();
            thread_counter ++;
        }

        for(size_t idx = 0; idx != num_carni; idx ++) {
            creation_pos.i = rand_pos(gen);
            creation_pos.j = rand_pos(gen);
            entity_grid[creation_pos.i][creation_pos.j] = {carnivore, 100, 0};
            std::thread t(carni_routine, creation_pos);
            t.detach();
            thread_counter ++;
        }

        // Return the JSON representation of the entity grid
        nlohmann::json json_grid = entity_grid; 
        res.body = json_grid.dump();
        res.end(); });

    // Endpoint to process HTTP GET requests for the next simulation iteration
    CROW_ROUTE(app, "/next-iteration")
        .methods("GET"_method)([]()
                               {
        // Simulate the next iteration
        // Iterate over the entity grid and simulate the behaviour of each entity
        
        std::latch lt(thread_counter);
        sync_it = &lt;
        (*sync_it).wait();
        
        // Return the JSON representation of the entity grid
        nlohmann::json json_grid = entity_grid; 
        return json_grid.dump(); });
    app.port(8080).run();

    return 0;
}