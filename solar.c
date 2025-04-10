#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <SDL2/SDL.h>

// ************************
// Constants and Definitions
// ************************

#define WIDTH 2400
#define HEIGHT 2400
#define G 1.0                    // Gravitational constant (scaled for simulation)
#define EPSILON 1e-9            // Small value to prevent division by zero
#define THETA 0.5               // Barnes-Hut opening angle threshold
#define SIMULATION_REGION 50.0  // Size of the simulation region
#define NUM_ASTEROIDS 200       // Number of asteroids to simulate
#define NUM_PLANETS 9           // Number of planets (including Sun)
#define MAX_TRAJECTORY_POINTS 1000

// ************************
// Data Structures
// ************************

// Planet structure
typedef struct {
    char name[20];
    double mass;       // Mass of the planet
    double x, y;       // Position
    double vx, vy;     // Velocity
    double ax, ay;     // Acceleration
    double radius;     // Display radius
    Uint32 color;      // RGB color

    double trajectory_x[MAX_TRAJECTORY_POINTS];
    double trajectory_y[MAX_TRAJECTORY_POINTS];
    int trajectory_count;
} Planet;

// Celestial body structure (used for quad tree)
typedef struct {
    double x, y;         // Position coordinates
    double vx, vy;       // Velocity components
    double mass;         // Mass of the body
    double radius;       // Radius for display
    double fx, fy;       // Force components (for logging)
    int type;            // 0 for planet, 1 for asteroid
    int planet_index;    // Index in the planets array (if type == 0)
} CelestialBody;

// Quad tree node structure
typedef struct QuadTreeNode {
    double x, y, width, height;   // Boundaries of the node
    CelestialBody* body;          // Pointer to a body (if leaf)
    struct QuadTreeNode *nw, *ne, *sw, *se;  // Child nodes
    double total_mass;            // Sum of masses in this region
    double center_x, center_y;    // Center of mass of this node
} QuadTreeNode;

// ************************
// Global Variables
// ************************

Planet planets[NUM_PLANETS];
CelestialBody asteroids[NUM_ASTEROIDS];

// Planet initialization data
char* names[] = {"Sun", "Mercury", "Venus", "Earth", "Mars", "Jupiter", "Saturn", "Uranus", "Neptune"};
double semi_major_axes[] = {0.0, 0.387, 0.723, 1.0, 1.524, 5.203, 9.539, 19.191, 30.069};
double masses[] = {1.0, 1.659e-7, 2.447e-6, 3.003e-6, 3.227e-7, 9.545e-4, 2.856e-4, 4.365e-5, 5.127e-5};
Uint32 colors[] = {0xFFFF00, 0x808080, 0xFFA500, 0x0000FF, 0xFF0000, 0xA52A2A, 0xFFFF00, 0xADD8E6, 0xADD8E6};

// ************************
// Quad Tree Functions
// ************************

// Creates a new quad tree node
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

// Checks if a body is within boundaries
bool is_in_bounds(QuadTreeNode* node, CelestialBody* body) {
    return (body->x >= node->x &&
            body->x < node->x + node->width &&
            body->y >= node->y &&
            body->y < node->y + node->height);
}

// Subdivides a node into four quadrants
void subdivide(QuadTreeNode* node) {
    double half_width = node->width / 2.0;
    double half_height = node->height / 2.0;
    node->nw = create_quadtree(node->x, node->y, half_width, half_height);
    node->ne = create_quadtree(node->x + half_width, node->y, half_width, half_height);
    node->sw = create_quadtree(node->x, node->y + half_height, half_width, half_height);
    node->se = create_quadtree(node->x + half_width, node->y + half_height, half_width, half_height);
}

// Gets the appropriate quadrant for a body
QuadTreeNode* get_quadrant(QuadTreeNode* node, CelestialBody* body) {
    double mid_x = node->x + node->width / 2.0;
    double mid_y = node->y + node->height / 2.0;
    if (body->y < mid_y) {
        return (body->x < mid_x) ? node->nw : node->ne;
    } else {
        return (body->x < mid_x) ? node->sw : node->se;
    }
}

// Inserts a body into the quad tree
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

// Calculates center of mass for the node
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

// Frees the quad tree
void free_quadtree(QuadTreeNode* node) {
    if (node == NULL) return;
    free_quadtree(node->nw);
    free_quadtree(node->ne);
    free_quadtree(node->sw);
    free_quadtree(node->se);
    free(node);
}

// Calculates gravitational force using Barnes-Hut
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

// Updates a body's position and velocity
void update_body(CelestialBody* body, double fx, double fy, double dt) {
    double ax = fx / body->mass;
    double ay = fy / body->mass;
    body->vx += ax * dt;
    body->vy += ay * dt;
    body->x += body->vx * dt;
    body->y += body->vy * dt;
    body->fx = fx;
    body->fy = fy;
}

// ************************
// Initialization Functions
// ************************

// Initialize planets
void initialize_planets() {
    for (int i = 0; i < NUM_PLANETS; i++) {
        strcpy(planets[i].name, names[i]);
        planets[i].mass = masses[i];
        planets[i].x = semi_major_axes[i];
        planets[i].y = 0.0;
        planets[i].vx = 0.0;
        // Set orbital velocity for planets (except Sun)
        planets[i].vy = (i == 0) ? 0.0 : sqrt(G / semi_major_axes[i]);
        planets[i].ax = 0.0;
        planets[i].ay = 0.0;
        planets[i].radius = (i == 0) ? 20.0 : 10.0; // Sun is bigger
        planets[i].color = colors[i];
        planets[i].trajectory_count = 0;
    }
}

// Initialize asteroids
void initialize_asteroids() {
    for (int i = 0; i < NUM_ASTEROIDS; i++) {
        // Random position in asteroid belt (between Mars and Jupiter)
        double r = 2.0 + ((double)rand() / RAND_MAX) * 2.5; // 2.0-4.5 AU
        double angle = ((double)rand() / RAND_MAX) * 2.0 * M_PI;
        
        asteroids[i].x = r * cos(angle);
        asteroids[i].y = r * sin(angle);
        
        // Orbital velocity (perpendicular to radius vector)
        double v_orbital = sqrt(G / r);
        asteroids[i].vx = -v_orbital * sin(angle);
        asteroids[i].vy = v_orbital * cos(angle);
        
        // Set a small mass and radius
        asteroids[i].mass = 1e-8;
        asteroids[i].radius = 2.0;
        asteroids[i].fx = 0.0;
        asteroids[i].fy = 0.0;
        asteroids[i].type = 1; // 1 for asteroid
    }
}

// ************************
// Simulation Update Functions
// ************************

// Updates the entire simulation using Barnes-Hut
void update_simulation_barnes_hut(double dt, int* frame_count, int trajectory_interval) {
    // Create CelestialBody array for all objects
    int total_bodies = NUM_PLANETS + NUM_ASTEROIDS;
    CelestialBody* all_bodies = (CelestialBody*)malloc(total_bodies * sizeof(CelestialBody));
    
    // Add planets to the array
    for (int i = 0; i < NUM_PLANETS; i++) {
        all_bodies[i].x = planets[i].x;
        all_bodies[i].y = planets[i].y;
        all_bodies[i].vx = planets[i].vx;
        all_bodies[i].vy = planets[i].vy;
        all_bodies[i].mass = planets[i].mass;
        all_bodies[i].radius = planets[i].radius;
        all_bodies[i].fx = 0.0;
        all_bodies[i].fy = 0.0;
        all_bodies[i].type = 0; // 0 for planet
        all_bodies[i].planet_index = i;
    }
    
    // Add asteroids to the array
    for (int i = 0; i < NUM_ASTEROIDS; i++) {
        all_bodies[NUM_PLANETS + i] = asteroids[i];
    }
    
    // Create quad tree
    QuadTreeNode* root = create_quadtree(-SIMULATION_REGION, -SIMULATION_REGION, 
                                        2 * SIMULATION_REGION, 2 * SIMULATION_REGION);
    
    // Insert all bodies into the quad tree
    for (int i = 0; i < total_bodies; i++) {
        insert_body(root, &all_bodies[i]);
    }
    
    // Calculate center of mass for all nodes
    calculate_center_of_mass(root);
    
    // Calculate forces and update positions
    for (int i = 0; i < total_bodies; i++) {
        double fx = 0.0, fy = 0.0;
        calculate_force_from_quadtree(&all_bodies[i], root, THETA, &fx, &fy);
        update_body(&all_bodies[i], fx, fy, dt);
        
        // Update original data structures
        if (all_bodies[i].type == 0) {
            int idx = all_bodies[i].planet_index;
            planets[idx].x = all_bodies[i].x;
            planets[idx].y = all_bodies[i].y;
            planets[idx].vx = all_bodies[i].vx;
            planets[idx].vy = all_bodies[i].vy;
            planets[idx].ax = fx / planets[idx].mass;
            planets[idx].ay = fy / planets[idx].mass;
        } else {
            int idx = i - NUM_PLANETS;
            asteroids[idx] = all_bodies[i];
        }
    }
    
    // Update planet trajectories
    if (*frame_count % trajectory_interval == 0) {
        for (int i = 0; i < NUM_PLANETS; i++) {
            if (planets[i].trajectory_count < MAX_TRAJECTORY_POINTS) {
                planets[i].trajectory_x[planets[i].trajectory_count] = planets[i].x;
                planets[i].trajectory_y[planets[i].trajectory_count] = planets[i].y;
                planets[i].trajectory_count++;
            } else {
                // Shift array to make room for new point
                for (int j = 0; j < MAX_TRAJECTORY_POINTS - 1; j++) {
                    planets[i].trajectory_x[j] = planets[i].trajectory_x[j + 1];
                    planets[i].trajectory_y[j] = planets[i].trajectory_y[j + 1];
                }
                planets[i].trajectory_x[MAX_TRAJECTORY_POINTS - 1] = planets[i].x;
                planets[i].trajectory_y[MAX_TRAJECTORY_POINTS - 1] = planets[i].y;
            }
        }
    }
    
    (*frame_count)++;
    
    // Clean up
    free_quadtree(root);
    free(all_bodies);
}

// ************************
// Rendering Functions
// ************************

// Draw button on the interface
void DrawButton(SDL_Renderer* renderer, int x, int y, int w, int h, TTF_Font* font, const char* text, SDL_Color text_color) {
    // Draw button background
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); // Gray
    SDL_Rect button_rect = {x, y, w, h};
    SDL_RenderFillRect(renderer, &button_rect);

    // Render text
    SDL_Surface* text_surface = TTF_RenderText_Solid(font, text, text_color);
    if (text_surface) {
        SDL_Texture* text_texture = SDL_CreateTextureFromSurface(renderer, text_surface);
        if (text_texture) {
            int text_w, text_h;
            SDL_QueryTexture(text_texture, NULL, NULL, &text_w, &text_h);
            SDL_Rect text_rect = {x + (w - text_w) / 2, y + (h - text_h) / 2, text_w, text_h};
            SDL_RenderCopy(renderer, text_texture, NULL, &text_rect);
            SDL_DestroyTexture(text_texture);
        }
        SDL_FreeSurface(text_surface);
    }
}

// Draw a circle with border
void draw_circle(SDL_Renderer* renderer, int cx, int cy, int radius, Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x*x + y*y <= radius*radius) {
                SDL_RenderDrawPoint(renderer, cx + x, cy + y);
            }
        }
    }
}

// Render planets
void render_planets(SDL_Renderer* renderer, double pixels_per_AU) {
    // Draw trajectories
    for (int i = 0; i < NUM_PLANETS; i++) {
        if (planets[i].trajectory_count > 1) {
            SDL_SetRenderDrawColor(renderer, 100, 100, 100, 100); // Gray for trajectories
            for (int j = 1; j < planets[i].trajectory_count; j++) {
                int x1 = WIDTH / 2 + (int)(planets[i].trajectory_x[j - 1] * pixels_per_AU);
                int y1 = HEIGHT / 2 - (int)(planets[i].trajectory_y[j - 1] * pixels_per_AU);
                int x2 = WIDTH / 2 + (int)(planets[i].trajectory_x[j] * pixels_per_AU);
                int y2 = HEIGHT / 2 - (int)(planets[i].trajectory_y[j] * pixels_per_AU);
                SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
            }
        }
    }

    // Draw planets
    for (int i = 0; i < NUM_PLANETS; i++) {
        int screen_x = WIDTH / 2 + (int)(planets[i].x * pixels_per_AU);
        int screen_y = HEIGHT / 2 - (int)(planets[i].y * pixels_per_AU);
        int radius = (int)planets[i].radius;

        Uint8 r = (planets[i].color >> 16) & 0xFF;
        Uint8 g = (planets[i].color >> 8) & 0xFF;
        Uint8 b = planets[i].color & 0xFF;
        
        draw_circle(renderer, screen_x, screen_y, radius, r, g, b, 255);
    }
}

// Render asteroids
void render_asteroids(SDL_Renderer* renderer, double pixels_per_AU) {
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 150); // Light gray for asteroids
    for (int i = 0; i < NUM_ASTEROIDS; i++) {
        int screen_x = WIDTH / 2 + (int)(asteroids[i].x * pixels_per_AU);
        int screen_y = HEIGHT / 2 - (int)(asteroids[i].y * pixels_per_AU);
        
        // Draw small dots for asteroids
        for (int y = -1; y <= 1; y++) {
            for (int x = -1; x <= 1; x++) {
                if (x*x + y*y <= 1) {
                    SDL_RenderDrawPoint(renderer, screen_x + x, screen_y + y);
                }
            }
        }
    }
}

// Draw UI elements and render the scene
void render_scene(SDL_Renderer* renderer, double pixels_per_AU, TTF_Font* font) {
    // Clear screen to black
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    
    // Draw celestial objects
    render_planets(renderer, pixels_per_AU);
    render_asteroids(renderer, pixels_per_AU);
    
    // Draw UI elements
    SDL_Color text_color = {255, 255, 255, 255}; // White text
    DrawButton(renderer, WIDTH - 100, 20, 50, 40, font, "+", text_color);  // Zoom In
    DrawButton(renderer, WIDTH - 100, 80, 50, 40, font, "-", text_color);  // Zoom Out
    DrawButton(renderer, WIDTH - 100, 200, 50, 40, font, "+", text_color); // Increase dt
    DrawButton(renderer, WIDTH - 100, 260, 50, 40, font, "-", text_color); // Decrease dt
    
    // Display info
    char info_text[100];
    sprintf(info_text, "Zoom: %.1f", pixels_per_AU);
    SDL_Surface* info_surface = TTF_RenderText_Solid(font, info_text, text_color);
    if (info_surface) {
        SDL_Texture* info_texture = SDL_CreateTextureFromSurface(renderer, info_surface);
        if (info_texture) {
            SDL_Rect info_rect = {10, 10, info_surface->w, info_surface->h};
            SDL_RenderCopy(renderer, info_texture, NULL, &info_rect);
            SDL_DestroyTexture(info_texture);
        }
        SDL_FreeSurface(info_surface);
    }
    
    // Present the rendered scene
    SDL_RenderPresent(renderer);
}

// ************************
// Main Function
// ************************

int main() {
    // Simulation parameters
    double pixels_per_AU = 120.0;  // Display scale
    double zoom_step = 20.0;       // Zoom increment
    double min_zoom = 40.0;        // Minimum zoom level
    double max_zoom = 400.0;       // Maximum zoom level
    
    double dt = 0.001;             // Time step
    double dt_step = 0.001;        // Time step increment
    double min_dt = 0.0001;        // Minimum time step
    double max_dt = 0.1;           // Maximum time step
    
    int frame_count = 0;
    int trajectory_interval = 10;
    
    // Initialize SDL and TTF
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        return 1;
    }
    
    if (TTF_Init() < 0) {
        fprintf(stderr, "TTF initialization failed: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }
    
    // Create window and renderer
    SDL_Window* window = SDL_CreateWindow("Solar System Simulation with Barnes-Hut", 
                                         SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                                         WIDTH, HEIGHT, 0);
    if (!window) {
        fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        fprintf(stderr, "Renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    
    // Load font
    TTF_Font* font = TTF_OpenFont("./fonts/Arial.ttf", 24);
    if (!font) {
        fprintf(stderr, "Font loading failed: %s\n", TTF_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    
    // Initialize planets and asteroids
    initialize_planets();
    initialize_asteroids();
    
    // Main loop
    int running = 1;
    while (running) {
        // Handle events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                int x = event.button.x;
                int y = event.button.y;
                
                // Zoom buttons
                if (x >= WIDTH - 100 && x <= WIDTH - 50 && y >= 20 && y <= 60) {
                    // Zoom in
                    pixels_per_AU += zoom_step;
                    if (pixels_per_AU > max_zoom) pixels_per_AU = max_zoom;
                } else if (x >= WIDTH - 100 && x <= WIDTH - 50 && y >= 80 && y <= 120) {
                    // Zoom out
                    pixels_per_AU -= zoom_step;
                    if (pixels_per_AU < min_zoom) pixels_per_AU = min_zoom;
                }
                
                // Time step buttons
                if (x >= WIDTH - 100 && x <= WIDTH - 50 && y >= 200 && y <= 240) {
                    // Increase time step
                    dt += dt_step;
                    if (dt > max_dt) dt = max_dt;
                } else if (x >= WIDTH - 100 && x <= WIDTH - 50 && y >= 260 && y <= 300) {
                    // Decrease time step
                    dt -= dt_step;
                    if (dt < min_dt) dt = min_dt;
                }
            }
        }
        
        // Update simulation using Barnes-Hut
        update_simulation_barnes_hut(dt, &frame_count, trajectory_interval);
        
        // Render the scene
        render_scene(renderer, pixels_per_AU, font);
        
        // Cap frame rate
        SDL_Delay(10);
    }
    
    // Clean up
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    
    return 0;
} 