#define CROW_MAIN
#define CROW_STATIC_DIR "../public"

#include "crow_all.h"
#include "json.hpp"
#include <random>
#include <thread>
#include <condition_variable>
#include <mutex>

static const uint32_t NUM_ROWS = 15;

// Constants
const uint32_t PLANT_MAXIMUM_AGE = 10;
const uint32_t HERBIVORE_MAXIMUM_AGE = 50;
const uint32_t CARNIVORE_MAXIMUM_AGE = 80;
const uint32_t MAXIMUM_ENERGY = 200;
const uint32_t THRESHOLD_ENERGY_FOR_REPRODUCTION = 20;

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

std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<> dis(0, NUM_ROWS);

std::atomic<int> completion_counter = 0;
std::atomic<int> thread_counter = 0;

std::mutex exec_it_mtx;
std::condition_variable executa_iteracao;

void plant_routine(entity_t& plant) {
    while(plant.energy > 0) {

        if(plant.age == 10) plant.energy = 0;
    }

}

void herbi_routine(entity_t& herbi) {
    while(herbi.energy > 0) {

        if(herbi.age == 50) herbi.energy = 0;
    }
    
}

void carni_routine(entity_t& carni) {
       while(carni.energy > 0) {

        if(carni.age == 80) carni.energy = 0;
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
            creation_pos.i = dis(gen);
            creation_pos.j = dis(gen);
            entity_grid[creation_pos.i][creation_pos.j] = {plant, 100, 0};
            std::thread t(plant_routine, entity_grid[creation_pos.i][creation_pos.j]);
            thread_counter ++;
        }

        for(size_t idx = 0; idx != num_herbi; idx ++) {
            creation_pos.i = dis(gen);
            creation_pos.j = dis(gen);
            entity_grid[creation_pos.i][creation_pos.j] = {herbivore, 100, 0};
            std::thread t(herbi_routine, entity_grid[creation_pos.i][creation_pos.j]);
            thread_counter ++;
        }

        for(size_t idx = 0; idx != num_carni; idx ++) {
            creation_pos.i = dis(gen);
            creation_pos.j = dis(gen);
            entity_grid[creation_pos.i][creation_pos.j] = {carnivore, 100, 0};
            std::thread t(carni_routine, entity_grid[creation_pos.i][creation_pos.j]);
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
        
        completion_counter = 0;
        executa_iteracao.notify_all();
        while(completion_counter < thread_counter);
        
        // Return the JSON representation of the entity grid
        nlohmann::json json_grid = entity_grid; 
        return json_grid.dump(); });
    app.port(8080).run();

    return 0;
}