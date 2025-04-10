// ***********************
// Includes and Constants
// ***********************

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "planet.h"   // Your existing planet structure and definitions

// Simulation window dimensions
#define WIDTH 2400
#define HEIGHT 2400

// Gravitational constant for simulation (tuned/scaled for your system)
#define G 6.67430e-11  
#define EPSILON 1e-9

// Barnes-Hut opening angle threshold (tweak for accuracy/speed tradeoff)
#define THETA 0.5

// Simulation region for the quad tree (in simulation units; adjust as needed)
#define SIMULATION_REGION 100.0

// Number of asteroids to simulate
#define NUM_ASTEROIDS 200

// ***********************
// Data Structures
// ***********************

// Use the CelestialBody structure for the quad tree and asteroid simulation.
// We add two extra fields (fx, fy) for the last computed force for logging.
typedef struct {
    double x, y;        // Position coordinates
    double vx, vy;      // Velocity components
    double mass;        // Mass of the body
    double radius;      // Radius (for collision/rendering purposes)
    double fx, fy;      // Force components (for logging/monitoring)
} CelestialBody;

// Quad Tree node structure (as provided)
typedef struct QuadTreeNode {
    double x, y, width, height;  // Boundaries of the node
    CelestialBody* body;         // Pointer to a body (if leaf)
    struct QuadTreeNode *nw, *ne, *sw, *se;  // Child nodes
    double total_mass;           // Sum of masses in this region
    double center_x, center_y;   // Center of mass of this node
} QuadTreeNode;

// ***********************
// Quad Tree and Barnes-Hut Functions 
// (Use your provided implementations)
// ***********************
// (See your code for create_quadtree(), is_in_bounds(), subdivide(),
//  get_quadrant(), insert_body(), calculate_center_of_mass(), free_quadtree(),
//  calculate_force_from_quadtree(), update_body(), etc.)

// Creates a new quad tree node covering the given region.
QuadTreeNode* create_quadtree(double x, double y, double width, double height) {
    QuadTreeNode* node = (QuadTreeNode*)malloc(sizeof(QuadTreeNode));
    if (node == NULL) {
        fprintf(stderr, "Memory allocation failed for quad-tree node\n");
        exit(EXIT_FAILURE);
    }
    node->x = x;
    node->y = y;
    node->width = width;
    node->height = height;
    node->body = NULL;
    node->nw = node->ne = node->sw = node->se = NULL;
    node->total_mass = 0.0;
    node->center_x = 0.0;
    node->center_y = 0.0;
    return node;
}

bool is_in_bounds(QuadTreeNode* node, CelestialBody* body) {
    return (body->x >= node->x &&
            body->x < node->x + node->width &&
            body->y >= node->y &&
            body->y < node->y + node->height);
}

void subdivide(QuadTreeNode* node) {
    double half_width = node->width / 2.0;
    double half_height = node->height / 2.0;
    node->nw = create_quadtree(node->x, node->y, half_width, half_height);
    node->ne = create_quadtree(node->x + half_width, node->y, half_width, half_height);
    node->sw = create_quadtree(node->x, node->y + half_height, half_width, half_height);
    node->se = create_quadtree(node->x + half_width, node->y + half_height, half_width, half_height);
}

QuadTreeNode* get_quadrant(QuadTreeNode* node, CelestialBody* body) {
    double mid_x = node->x + node->width / 2.0;
    double mid_y = node->y + node->height / 2.0;
    if (body->y < mid_y) {
        return (body->x < mid_x) ? node->nw : node->ne;
    } else {
        return (body->x < mid_x) ? node->sw : node->se;
    }
}

void insert_body(QuadTreeNode* node, CelestialBody* body) {
    if (!is_in_bounds(node, body)) {
        return; // Out of bounds
    }
    
    // Case 1: Node is empty (leaf with no body)
    if (node->body == NULL && node->nw == NULL) {
        node->body = body;
        return;
    }
    
    // Case 2: Leaf node already has a body → subdivide
    if (node->body != NULL && node->nw == NULL) {
        subdivide(node);
        CelestialBody* existing_body = node->body;
        node->body = NULL;
        insert_body(get_quadrant(node, existing_body), existing_body);
    }
    
    // Case 3: Internal node
    insert_body(get_quadrant(node, body), body);
}

void calculate_center_of_mass(QuadTreeNode* node) {
    if (node == NULL) return;
    
    // Leaf node with a body
    if (node->body != NULL && node->nw == NULL) {
        node->total_mass = node->body->mass;
        node->center_x = node->body->x;
        node->center_y = node->body->y;
        return;
    }
    
    // Empty leaf node
    if (node->body == NULL && node->nw == NULL) {
        node->total_mass = 0.0;
        node->center_x = node->x + node->width / 2.0;
        node->center_y = node->y + node->height / 2.0;
        return;
    }
    
    // Internal node: compute for children
    calculate_center_of_mass(node->nw);
    calculate_center_of_mass(node->ne);
    calculate_center_of_mass(node->sw);
    calculate_center_of_mass(node->se);
    
    node->total_mass = 0.0;
    node->center_x = 0.0;
    node->center_y = 0.0;
    
    QuadTreeNode* children[4] = { node->nw, node->ne, node->sw, node->se };
    for (int i = 0; i < 4; i++) {
        if (children[i]->total_mass > 0) {
            node->total_mass += children[i]->total_mass;
            node->center_x += children[i]->center_x * children[i]->total_mass;
            node->center_y += children[i]->center_y * children[i]->total_mass;
        }
    }
    
    if (node->total_mass > 0) {
        node->center_x /= node->total_mass;
        node->center_y /= node->total_mass;
    } else {
        node->center_x = node->x + node->width / 2.0;
        node->center_y = node->y + node->height / 2.0;
    }
}

void free_quadtree(QuadTreeNode* node) {
    if (node == NULL) return;
    free_quadtree(node->nw);
    free_quadtree(node->ne);
    free_quadtree(node->sw);
    free_quadtree(node->se);
    free(node);
}

// Calculates the gravitational force on a body using Barnes-Hut approach.
void calculate_force_from_quadtree(CelestialBody* body, QuadTreeNode* node, double theta, double* fx, double* fy) {
    if (node == NULL || node->total_mass == 0) return;
    
    // If leaf node (and not the same body)
    if (node->body != NULL && node->body != body) {
        double dx = node->body->x - body->x;
        double dy = node->body->y - body->y;
        double dist_sq = dx * dx + dy * dy;
        double dist = sqrt(dist_sq);
        if (dist < EPSILON) return;
        double force = G * body->mass * node->body->mass / dist_sq;
        *fx += force * dx / dist;
        *fy += force * dy / dist;
        return;
    }
    
    // For internal node: decide whether to approximate
    double dx = node->center_x - body->x;
    double dy = node->center_y - body->y;
    double dist = sqrt(dx * dx + dy * dy);
    double s = fmax(node->width, node->height);
    if (s / dist < theta) {
        // Approximate as a single body
        if (dist < EPSILON) return;
        double force = G * body->mass * node->total_mass / (dist * dist);
        *fx += force * dx / dist;
        *fy += force * dy / dist;
    } else {
        calculate_force_from_quadtree(body, node->nw, theta, fx, fy);
        calculate_force_from_quadtree(body, node->ne, theta, fx, fy);
        calculate_force_from_quadtree(body, node->sw, theta, fx, fy);
        calculate_force_from_quadtree(body, node->se, theta, fx, fy);
    }
}

// Updates a body's velocity and position based on the provided force and timestep.
void update_body(CelestialBody* body, double fx, double fy, double dt) {
    double ax = fx / body->mass;
    double ay = fy / body->mass;
    body->vx += ax * dt;
    body->vy += ay * dt;
    body->x += body->vx * dt;
    body->y += body->vy * dt;
    // Store the force for logging/monitoring:
    body->fx = fx;
    body->fy = fy;
}

// ***********************
// Asteroid Handling and Logging Functions
// ***********************

// Function to log asteroid data to a CSV file.
// The CSV format: Time, PosX, PosY, VelX, VelY, ForceX, ForceY, |Force|
void log_asteroid_data(FILE* fp, CelestialBody* asteroid, double time) {
    double force_mag = sqrt(asteroid->fx * asteroid->fx + asteroid->fy * asteroid->fy);
    fprintf(fp, "%.3f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f\n",
            time, asteroid->x, asteroid->y, asteroid->vx, asteroid->vy, asteroid->fx, asteroid->fy, force_mag);
}

// Global array for asteroids
CelestialBody asteroids[NUM_ASTEROIDS];

// Initializes the asteroid array with random positions and velocities.
// For simplicity, positions are randomly distributed in a specified range,
// and velocities are set to a small random value.
void initialize_asteroids() {
    for (int i = 0; i < NUM_ASTEROIDS; i++) {
        // Random position in a sub-region (adjust range as needed)
        asteroids[i].x = ((double)rand() / RAND_MAX) * 40.0 - 20.0;
        asteroids[i].y = ((double)rand() / RAND_MAX) * 40.0 - 20.0;
        // Small random initial velocities
        asteroids[i].vx = ((double)rand() / RAND_MAX) * 0.01 - 0.005;
        asteroids[i].vy = ((double)rand() / RAND_MAX) * 0.01 - 0.005;
        // Set a small mass (smaller than planets)
        asteroids[i].mass = 1e-6;
        // Set a small radius for visualization
        asteroids[i].radius = 2.0;
        // Initialize force fields to zero
        asteroids[i].fx = 0.0;
        asteroids[i].fy = 0.0;
    }
}

// Updates asteroids using the Barnes-Hut quad tree.
// It builds a quad tree including both planets and asteroids, computes forces,
// updates asteroid states, and logs their data.
void update_asteroids_with_quadtree(Planet planets[], int num_planets, double dt, double current_time, FILE* log_file) {
    // Create a quad tree for the simulation region.
    QuadTreeNode* root = create_quadtree(-SIMULATION_REGION, -SIMULATION_REGION,
                                          2 * SIMULATION_REGION, 2 * SIMULATION_REGION);
    
    // Allocate temporary array to hold planet bodies (converted from Planet)
    CelestialBody* planetBodies[num_planets];
    for (int i = 0; i < num_planets; i++) {
        planetBodies[i] = (CelestialBody*)malloc(sizeof(CelestialBody));
        // Convert planet data to CelestialBody.
        // Note: Since Planet's first field is name (char[20]), we access later fields accordingly.
        // Assuming the layout as defined in planet.h:
        planetBodies[i]->mass = planets[i].mass;
        planetBodies[i]->x = planets[i].x;
        planetBodies[i]->y = planets[i].y;
        planetBodies[i]->vx = planets[i].vx;
        planetBodies[i]->vy = planets[i].vy;
        planetBodies[i]->radius = planets[i].radius;
        planetBodies[i]->fx = 0.0;
        planetBodies[i]->fy = 0.0;
        // Insert this planet into the quad tree
        insert_body(root, planetBodies[i]);
    }
    
    // Insert all asteroids into the quad tree.
    for (int i = 0; i < NUM_ASTEROIDS; i++) {
        insert_body(root, &asteroids[i]);
    }
    
    // Compute the center of mass for the quad tree.
    calculate_center_of_mass(root);
    
    // For each asteroid, calculate the total gravitational force from the quad tree,
    // update its state, and log its data.
    for (int i = 0; i < NUM_ASTEROIDS; i++) {
        double fx = 0.0, fy = 0.0;
        calculate_force_from_quadtree(&asteroids[i], root, THETA, &fx, &fy);
        update_body(&asteroids[i], fx, fy, dt);
        log_asteroid_data(log_file, &asteroids[i], current_time);
    }
    
    // Free the dynamically allocated planet bodies.
    for (int i = 0; i < num_planets; i++) {
        free(planetBodies[i]);
    }
    free_quadtree(root);
}

// ***********************
// Asteroid Rendering Function (SDL2)
// ***********************

// Renders asteroids distinctly (smaller circles, different color)
void render_asteroids(SDL_Renderer* renderer, double pixels_per_AU) {
    // Set a distinct color for asteroids (e.g., light gray)
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    for (int i = 0; i < NUM_ASTEROIDS; i++) {
        int screen_x = WIDTH / 2 + (int)(asteroids[i].x * pixels_per_AU);
        int screen_y = HEIGHT / 2 - (int)(asteroids[i].y * pixels_per_AU);
        int r = 3;  // Asteroid radius for rendering
        // Draw a filled circle (using simple point drawing)
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                if ((dx*dx + dy*dy) <= r*r) {
                    SDL_RenderDrawPoint(renderer, screen_x + dx, screen_y + dy);
                }
            }
        }
    }
}

// ***********************
// Main Simulation and Rendering Loop
// ***********************

// Assume that your planets are updated using your current update_simulation() function.
// Here we add our asteroid update and rendering.

#define NUM_PLANETS 9   // As in your original simulation

Planet planets[NUM_PLANETS];
char* names[] = {"Sun", "Mercury", "Venus", "Earth", "Mars", "Jupiter", "Saturn", "Uranus", "Neptune"};
double semi_major_axes[] = {0.0, 0.387, 0.723, 1.0, 1.524, 5.203, 9.539, 19.191, 30.069};
double masses[] = {1.0, 1.659e-7, 2.447e-6, 3.003e-6, 3.227e-7, 9.545e-4, 2.856e-4, 4.365e-5, 5.127e-5};
Uint32 colors[] = {0xFFFF00, 0x808080, 0xFFA500, 0x0000FF, 0xFF0000, 0xA52A2A, 0xFFFF00, 0xADD8E6, 0xADD8E6};

void initialize_planets() {
    for (int i = 0; i < NUM_PLANETS; i++) {
        strcpy(planets[i].name, names[i]);
        planets[i].mass = masses[i];
        planets[i].x = semi_major_axes[i];
        planets[i].y = 0.0;
        planets[i].vx = 0.0;
        // Set orbital velocity for planets (except Sun)
        planets[i].vy = (i == 0) ? 0.0 : sqrt(1.0 / semi_major_axes[i]);
        planets[i].ax = 0.0;
        planets[i].ay = 0.0;
        planets[i].radius = 15.0;
        planets[i].color = colors[i];
        planets[i].trajectory_count = 0;
    }
}

int main() {
    double pixels_per_AU = 120.0;
    double dt = 0.001;
    int frame_count = 0;
    int trajectory_interval = 10;

    // Open a CSV file to log asteroid data.
    FILE* asteroid_log = fopen("asteroid_log.csv", "w");
    if (!asteroid_log) {
        fprintf(stderr, "Error opening asteroid_log.csv for writing\n");
        exit(EXIT_FAILURE);
    }
    // Write CSV header.
    fprintf(asteroid_log, "Time,PosX,PosY,VelX,VelY,ForceX,ForceY,ForceMag\n");

    // Initialize SDL2 and TTF
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    SDL_Window* window = SDL_CreateWindow("Solar System Simulation", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, 0);
    if (!window) {
        fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
        exit(EXIT_FAILURE);
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        fprintf(stderr, "Renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        exit(EXIT_FAILURE);
    }
    
    // Load font for UI elements.
    TTF_Font* font = TTF_OpenFont("./fonts/Arial.ttf", 30);
    if (font == NULL) {
        printf("Failed to load font: %s\n", TTF_GetError());
        exit(EXIT_FAILURE);
    }
    
    // Initialize planets and asteroids.
    initialize_planets();
    initialize_asteroids();
    
    int running = 1;
    double current_time = 0.0;
    
    // Main simulation loop
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = 0;
            // (Add any additional event handling as needed)
        }
        
        // Update planets (using your existing update_simulation function).
        update_simulation(planets, NUM_PLANETS, dt, &frame_count, trajectory_interval);
        
        // Update asteroids using the Barnes-Hut quad tree.
        update_asteroids_with_quadtree(planets, NUM_PLANETS, dt, current_time, asteroid_log);
        
        // Render scene: first clear, then render planets, then asteroids.
        render_planets(renderer, planets, NUM_PLANETS, pixels_per_AU, font);
        render_asteroids(renderer, pixels_per_AU);
        
        current_time += dt;
    }
    
    fclose(asteroid_log);
    
    // Clean up SDL2/TTF.
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}
