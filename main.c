/**
 * Solar System Simulation with Barnes-Hut Algorithm
 * 
 * This program simulates a solar system with planets and asteroids
 * using the Barnes-Hut algorithm for efficient N-body gravitational calculations.
 * It uses the quadtree implementation from quadtree2.c to track all objects.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "planet.h"

// Simulation window dimensions - matching sdl_render.c
#define WIDTH 2400
#define HEIGHT 2400
#define COLOR_WHITE 0xffffffff
#define COLOR_BLACK 0x00000000

// Gravitational constant for simulation
#define G 6.67430e-11  // Actual G value (will be scaled appropriately)
#define EPSILON 1e-9   // Small value to prevent division by zero
#define SIMULATION_SCALE 1.0 // Scale factor for G in our simulation

// Barnes-Hut opening angle threshold
#define THETA 0.5

// Simulation region for the quad tree
#define SIMULATION_REGION 50.0

// Number of planets and asteroids
#define NUM_PLANETS 9
#define NUM_ASTEROIDS 200
#define MAX_BODIES (NUM_PLANETS + NUM_ASTEROIDS)

// Structure to represent a celestial body (from quadtree2.c)
typedef struct {
    double x, y;        // Position coordinates
    double vx, vy;      // Velocity components
    double mass;        // Mass of the body
    double radius;      // Radius of the body (for collision detection)
    // Additional fields for our simulation
    char name[20];      // Name of the body
    Uint32 color;       // Color for rendering
    double trajectory_x[MAX_TRAJECTORY_POINTS];
    double trajectory_y[MAX_TRAJECTORY_POINTS];
    int trajectory_count;
} CelestialBody;

// Quad-Tree node structure (from quadtree2.c)
typedef struct QuadTreeNode {
    double x, y, width, height;  // Boundaries of the node
    CelestialBody* body;         // Pointer to a body (if leaf)
    struct QuadTreeNode *nw, *ne, *sw, *se;  // Child nodes
    double total_mass;           // Sum of masses in this region
    double center_x, center_y;   // Center of mass of this node
} QuadTreeNode;

// Function declarations
void initialize_simulation(CelestialBody bodies[], int *body_count);
TTF_Font* load_font(const char* font_path, int font_size);
void draw_circle_border(SDL_Renderer* renderer, int cx, int cy, int radius, 
                        Uint8 r, Uint8 g, Uint8 b, Uint8 a, int border_thickness);
void render_bodies(SDL_Renderer* renderer, CelestialBody bodies[], int body_count, 
                  double pixels_per_AU, TTF_Font* font);
void DrawButton(SDL_Renderer* renderer, int x, int y, int w, int h, 
                TTF_Font* font, const char* text, SDL_Color text_color);
void log_simulation_data(FILE* log_file, CelestialBody bodies[], int body_count, double time);

// Quad tree functions from quadtree2.c
CelestialBody* create_body(double x, double y, double vx, double vy, double mass, double radius);
QuadTreeNode* create_quadtree(double x, double y, double width, double height);
bool is_in_bounds(QuadTreeNode* node, CelestialBody* body);
void subdivide(QuadTreeNode* node);
QuadTreeNode* get_quadrant(QuadTreeNode* node, CelestialBody* body);
void insert_body(QuadTreeNode* node, CelestialBody* body);
void calculate_center_of_mass(QuadTreeNode* node);
void free_quadtree(QuadTreeNode* node);
void calculate_force_from_quadtree(CelestialBody* body, QuadTreeNode* node, double theta, double* fx, double* fy);
void update_body(CelestialBody* body, double fx, double fy, double dt);

// Global data for celestial bodies
CelestialBody bodies[MAX_BODIES];
int body_count = 0;

// Solar system data
char* planet_names[] = {"Sun", "Mercury", "Venus", "Earth", "Mars", "Jupiter", "Saturn", "Uranus", "Neptune"};
double semi_major_axes[] = {0.0, 0.387, 0.723, 1.0, 1.524, 5.203, 9.539, 19.191, 30.069};
double planet_masses[] = {1.0, 1.659e-7, 2.447e-6, 3.003e-6, 3.227e-7, 9.545e-4, 2.856e-4, 4.365e-5, 5.127e-5};
Uint32 planet_colors[] = {0xFFFF00, 0x808080, 0xFFA500, 0x0000FF, 0xFF0000, 0xA52A2A, 0xFFFF00, 0xADD8E6, 0xADD8E6};

int main() {
    // Simulation parameters
    double pixels_per_AU = 120.0;  // Scale factor for display
    double zoom_step = 20.0;       // How much to zoom in/out
    double min_zoom = 40.0;        // Minimum zoom level
    double max_zoom = 400.0;       // Maximum zoom level

    double dt = 0.001;             // Time step
    double dt_step = 0.001;        // How much to change time step
    double min_dt = 0.0001;        // Minimum time step
    double max_dt = 0.1;           // Maximum time step

    int frame_count = 0;
    int trajectory_interval = 10;
    double current_time = 0.0;
    
    // Initialize SDL and TTF
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }
    
    if (TTF_Init() < 0) {
        fprintf(stderr, "TTF could not initialize! TTF_Error: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }
    
    // Create window
    SDL_Window* window = SDL_CreateWindow("Solar System with Barnes-Hut",
                                         SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                         WIDTH, HEIGHT, 0);
    if (!window) {
        fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    
    // Create renderer
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        fprintf(stderr, "Renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    
    // Load font for UI elements
    TTF_Font* font = load_font("./fonts/Arial.ttf", 30);
    if (!font) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    
    // Initialize simulation bodies
    initialize_simulation(bodies, &body_count);
    
    // Open log file to track simulation data
    FILE* log_file = fopen("simulation_log.csv", "w");
    if (log_file) {
        fprintf(log_file, "Time,Name,PosX,PosY,VelX,VelY,Mass\n");
    }
    
    // Main simulation loop
    int running = 1;
    SDL_Event event;
    
    while (running) {
        // Process events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                int x = event.button.x;
                int y = event.button.y;
                
                // Zoom buttons (top-right corner) - exact coordinates from sdl_render.c
                if (x >= WIDTH - 100 && x <= WIDTH - 50) {
                    if (y >= 20 && y <= 60) {  // Zoom In
                        pixels_per_AU += zoom_step;
                        if (pixels_per_AU > max_zoom) pixels_per_AU = max_zoom;
                    } else if (y >= 80 && y <= 120) {  // Zoom Out
                        pixels_per_AU -= zoom_step;
                        if (pixels_per_AU < min_zoom) pixels_per_AU = min_zoom;
                    }
                }
                
                // Time step buttons - exact coordinates from sdl_render.c
                if (x >= WIDTH - 100 && x <= WIDTH - 50) {
                    if (y >= 200 && y <= 240) {  // Increase time step
                        dt += dt_step;
                        if (dt > max_dt) dt = max_dt;
                    } else if (y >= 260 && y <= 300) {  // Decrease time step
                        dt -= dt_step;
                        if (dt < min_dt) dt = min_dt;
                    }
                }
            }
        }
        
        // Create a quadtree for the current frame
        QuadTreeNode* root = create_quadtree(-SIMULATION_REGION, -SIMULATION_REGION, 
                                           2 * SIMULATION_REGION, 2 * SIMULATION_REGION);
        
        // Insert all bodies into the quadtree
        for (int i = 0; i < body_count; i++) {
            insert_body(root, &bodies[i]);
        }
        
        // Calculate center of mass for the quadtree
        calculate_center_of_mass(root);
        
        // Calculate forces and update all bodies
        for (int i = 0; i < body_count; i++) {
            double fx = 0.0, fy = 0.0;
            calculate_force_from_quadtree(&bodies[i], root, THETA, &fx, &fy);
            update_body(&bodies[i], fx, fy, dt);
        }
        
        // Update trajectories
        if (frame_count % trajectory_interval == 0) {
            for (int i = 0; i < body_count; i++) {
                if (bodies[i].trajectory_count < MAX_TRAJECTORY_POINTS) {
                    bodies[i].trajectory_x[bodies[i].trajectory_count] = bodies[i].x;
                    bodies[i].trajectory_y[bodies[i].trajectory_count] = bodies[i].y;
                    bodies[i].trajectory_count++;
                } else {
                    // Shift array to discard oldest point
                    for (int j = 0; j < MAX_TRAJECTORY_POINTS - 1; j++) {
                        bodies[i].trajectory_x[j] = bodies[i].trajectory_x[j + 1];
                        bodies[i].trajectory_y[j] = bodies[i].trajectory_y[j + 1];
                    }
                    bodies[i].trajectory_x[MAX_TRAJECTORY_POINTS - 1] = bodies[i].x;
                    bodies[i].trajectory_y[MAX_TRAJECTORY_POINTS - 1] = bodies[i].y;
                }
            }
        }
        
        // Log data periodically
        if (frame_count % 100 == 0 && log_file) {
            log_simulation_data(log_file, bodies, body_count, current_time);
        }
        
        // Render the scene
        render_bodies(renderer, bodies, body_count, pixels_per_AU, font);
        
        // Free the quadtree for this frame
        free_quadtree(root);
        
        // Update simulation time and frame count
        current_time += dt;
        frame_count++;
    }
    
    // Clean up
    if (log_file) fclose(log_file);
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    
    return 0;
}

// Initialize planets and asteroids
void initialize_simulation(CelestialBody bodies[], int *body_count) {
    // Initialize planets
    for (int i = 0; i < NUM_PLANETS; i++) {
        strcpy(bodies[i].name, planet_names[i]);
        bodies[i].mass = planet_masses[i];
        bodies[i].x = semi_major_axes[i];
        bodies[i].y = 0.0;
        bodies[i].vx = 0.0;
        // Set orbital velocity for circular orbits
        bodies[i].vy = (i == 0) ? 0.0 : sqrt(G * SIMULATION_SCALE * bodies[0].mass / semi_major_axes[i]);
        bodies[i].radius = (i == 0) ? 25.0 : 15.0;  // Sun is larger
        bodies[i].color = planet_colors[i];
        bodies[i].trajectory_count = 0;
        (*body_count)++;
    }
    
    // Initialize asteroids
    srand(time(NULL));  // Seed random number generator
    
    // Use asteroid belt region between Mars and Jupiter
    double inner_radius = 2.2;  // Just outside Mars
    double outer_radius = 3.2;  // Before Jupiter
    
    for (int i = 0; i < NUM_ASTEROIDS && *body_count < MAX_BODIES; i++) {
        int idx = *body_count;
        
        // Generate name
        sprintf(bodies[idx].name, "Ast%d", i);
        
        // Random radius within asteroid belt
        double radius = inner_radius + (outer_radius - inner_radius) * ((double)rand() / RAND_MAX);
        
        // Random angle
        double angle = 2.0 * M_PI * ((double)rand() / RAND_MAX);
        
        // Position in circular coordinates
        bodies[idx].x = radius * cos(angle);
        bodies[idx].y = radius * sin(angle);
        
        // Small random mass (much smaller than planets)
        bodies[idx].mass = 1e-10 + 1e-9 * ((double)rand() / RAND_MAX);
        
        // Orbital velocity for circular orbit around the Sun (with small random variation)
        double v_orbital = sqrt(G * SIMULATION_SCALE * bodies[0].mass / radius);
        double variation = 0.95 + 0.1 * ((double)rand() / RAND_MAX);  // 0.95 to 1.05
        
        // Velocity perpendicular to radius
        bodies[idx].vx = -v_orbital * variation * sin(angle);
        bodies[idx].vy = v_orbital * variation * cos(angle);
        
        // Small radius for rendering
        bodies[idx].radius = 3.0;
        
        // Gray color for asteroids with slight variation
        int gray = 150 + (rand() % 80);
        bodies[idx].color = (gray << 16) | (gray << 8) | gray;
        
        // Empty trajectory
        bodies[idx].trajectory_count = 0;
        
        (*body_count)++;
    }
}

// Load a font for UI rendering
TTF_Font* load_font(const char* font_path, int font_size) {
    TTF_Font* font = TTF_OpenFont(font_path, font_size);
    if (!font) {
        fprintf(stderr, "Failed to load font: %s\n", TTF_GetError());
    }
    return font;
}

// Draw a circle border for celestial bodies - matching sdl_render.c implementation
void draw_circle_border(SDL_Renderer* renderer, int cx, int cy, int radius, 
                        Uint8 r, Uint8 g, Uint8 b, Uint8 a, int border_thickness) {
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    
    for (int y = cy - radius; y <= cy + radius; y++) {
        for (int x = cx - radius; x <= cx + radius; x++) {
            int dist_sq = (x - cx) * (x - cx) + (y - cy) * (y - cy);
            if (dist_sq >= (radius - border_thickness) * (radius - border_thickness) && 
                dist_sq <= radius * radius) {
                SDL_RenderDrawPoint(renderer, x, y);
            }
        }
    }
}

// Draw UI buttons with text - matching sdl_render.c implementation
void DrawButton(SDL_Renderer* renderer, int x, int y, int w, int h, 
                TTF_Font* font, const char* text, SDL_Color text_color) {
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

// Render celestial bodies with their trajectories
void render_bodies(SDL_Renderer* renderer, CelestialBody bodies[], int body_count, 
                  double pixels_per_AU, TTF_Font* font) {
    // Clear the screen
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    
    // Draw trajectories for planets only (not asteroids to reduce clutter)
    for (int i = 0; i < NUM_PLANETS; i++) {
        if (bodies[i].trajectory_count > 1) {
            // Set white color for trajectories
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 100);  // Partially transparent
            
            for (int j = 1; j < bodies[i].trajectory_count; j++) {
                int x1 = WIDTH / 2 + (int)(bodies[i].trajectory_x[j - 1] * pixels_per_AU);
                int y1 = HEIGHT / 2 - (int)(bodies[i].trajectory_y[j - 1] * pixels_per_AU);
                int x2 = WIDTH / 2 + (int)(bodies[i].trajectory_x[j] * pixels_per_AU);
                int y2 = HEIGHT / 2 - (int)(bodies[i].trajectory_y[j] * pixels_per_AU);
                
                SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
            }
        }
    }
    
    // Draw celestial bodies
    for (int i = 0; i < body_count; i++) {
        int screen_x = WIDTH / 2 + (int)(bodies[i].x * pixels_per_AU);
        int screen_y = HEIGHT / 2 - (int)(bodies[i].y * pixels_per_AU);
        int radius = (int)bodies[i].radius;
        
        // Skip if outside visible area (with margin)
        if (screen_x < -radius || screen_x >= WIDTH + radius || 
            screen_y < -radius || screen_y >= HEIGHT + radius) {
            continue;
        }
        
        // Extract color components
        Uint8 r = (bodies[i].color >> 16) & 0xFF;
        Uint8 g = (bodies[i].color >> 8) & 0xFF;
        Uint8 b = bodies[i].color & 0xFF;
        
        if (i < NUM_PLANETS) {
            // Draw planets with border
            draw_circle_border(renderer, screen_x, screen_y, radius, r, g, b, 255, 2);
        } else {
            // Draw asteroids as simple points for performance
            SDL_SetRenderDrawColor(renderer, r, g, b, 255);
            for (int dy = -radius; dy <= radius; dy++) {
                for (int dx = -radius; dx <= radius; dx++) {
                    if (dx*dx + dy*dy <= radius*radius) {
                        SDL_RenderDrawPoint(renderer, screen_x + dx, screen_y + dy);
                    }
                }
            }
        }
    }
    
    // Draw UI buttons and labels
    SDL_Color text_color = {255, 255, 255, 255};
    
    // Draw button labels
    SDL_Surface* zoom_surface = TTF_RenderText_Solid(font, "Zoom", text_color);
    if (zoom_surface) {
        SDL_Texture* zoom_texture = SDL_CreateTextureFromSurface(renderer, zoom_surface);
        if (zoom_texture) {
            int text_w, text_h;
            SDL_QueryTexture(zoom_texture, NULL, NULL, &text_w, &text_h);
            int x = WIDTH - 100 - text_w - 10; // 10 pixels padding from buttons
            int y = 60 - text_h / 2;           // Center vertically between y=20 and y=80
            if (y < 0) y = 0;                  // Prevent going off-screen
            SDL_Rect text_rect = {x, y, text_w, text_h};
            SDL_RenderCopy(renderer, zoom_texture, NULL, &text_rect);
            SDL_DestroyTexture(zoom_texture);
        }
        SDL_FreeSurface(zoom_surface);
    }
    
    SDL_Surface* speed_surface = TTF_RenderText_Solid(font, "Speed", text_color);
    if (speed_surface) {
        SDL_Texture* speed_texture = SDL_CreateTextureFromSurface(renderer, speed_surface);
        if (speed_texture) {
            int text_w, text_h;
            SDL_QueryTexture(speed_texture, NULL, NULL, &text_w, &text_h);
            int x = WIDTH - 100 - text_w - 10; // 10 pixels padding from buttons
            int y = 240 - text_h / 2;          // Center vertically between y=200 and y=260
            if (y < 0) y = 0;                  // Prevent going off-screen
            SDL_Rect text_rect = {x, y, text_w, text_h};
            SDL_RenderCopy(renderer, speed_texture, NULL, &text_rect);
            SDL_DestroyTexture(speed_texture);
        }
        SDL_FreeSurface(speed_surface);
    }
    
    // Draw buttons - using exact placement from sdl_render.c
    DrawButton(renderer, WIDTH - 100, 20, 50, 40, font, "+", text_color);  // Zoom In
    DrawButton(renderer, WIDTH - 100, 80, 50, 40, font, "-", text_color);  // Zoom Out
    DrawButton(renderer, WIDTH - 100, 200, 50, 40, font, "+", text_color); // Increase dt
    DrawButton(renderer, WIDTH - 100, 260, 50, 40, font, "-", text_color); // Decrease dt
    
    // Present the rendered frame
    SDL_RenderPresent(renderer);
}

// Log simulation data for analysis
void log_simulation_data(FILE* log_file, CelestialBody bodies[], int body_count, double time) {
    for (int i = 0; i < body_count; i++) {
        // Log only planets and a subset of asteroids to keep file size manageable
        if (i < NUM_PLANETS || (i % 20 == 0)) {
            fprintf(log_file, "%.3f,%s,%.6f,%.6f,%.6f,%.6f,%.6e\n",
                    time, bodies[i].name, bodies[i].x, bodies[i].y,
                    bodies[i].vx, bodies[i].vy, bodies[i].mass);
        }
    }
}

// Implementations of the quadtree functions from quadtree2.c would go here
// For brevity, the following is a minimal implementation that should work with
// the CelestialBody structure that we're using.

// Creates a new celestial body
CelestialBody* create_body(double x, double y, double vx, double vy, double mass, double radius) {
    CelestialBody* body = (CelestialBody*)malloc(sizeof(CelestialBody));
    if (body == NULL) {
        fprintf(stderr, "Memory allocation failed for celestial body\n");
        exit(EXIT_FAILURE);
    }
    
    body->x = x;
    body->y = y;
    body->vx = vx;
    body->vy = vy;
    body->mass = mass;
    body->radius = radius;
    sprintf(body->name, "Body");  // Default name
    body->color = 0xFFFFFF;       // Default color (white)
    body->trajectory_count = 0;
    
    return body;
}

// Creates a new quadtree node covering the given region
QuadTreeNode* create_quadtree(double x, double y, double width, double height) {
    QuadTreeNode* node = (QuadTreeNode*)malloc(sizeof(QuadTreeNode));
    if (node == NULL) {
        fprintf(stderr, "Memory allocation failed for quad-tree node\n");
        exit(EXIT_FAILURE);
    }
    
    // Initialize node properties
    node->x = x;
    node->y = y;
    node->width = width;
    node->height = height;
    node->body = NULL;
    node->nw = NULL;
    node->ne = NULL;
    node->sw = NULL;
    node->se = NULL;
    node->total_mass = 0.0;
    node->center_x = 0.0;
    node->center_y = 0.0;
    
    return node;
}

// Checks if a body is within the boundaries of a quadtree node
bool is_in_bounds(QuadTreeNode* node, CelestialBody* body) {
    return (body->x >= node->x && 
            body->x < node->x + node->width &&
            body->y >= node->y && 
            body->y < node->y + node->height);
}

// Subdivides a quadtree node into four quadrants
void subdivide(QuadTreeNode* node) {
    double half_width = node->width / 2.0;
    double half_height = node->height / 2.0;
    
    // Create the four child nodes
    node->nw = create_quadtree(node->x, node->y, half_width, half_height);
    node->ne = create_quadtree(node->x + half_width, node->y, half_width, half_height);
    node->sw = create_quadtree(node->x, node->y + half_height, half_width, half_height);
    node->se = create_quadtree(node->x + half_width, node->y + half_height, half_width, half_height);
}

// Determines which quadrant a body belongs to
QuadTreeNode* get_quadrant(QuadTreeNode* node, CelestialBody* body) {
    double mid_x = node->x + node->width / 2.0;
    double mid_y = node->y + node->height / 2.0;
    
    // Check which quadrant the body belongs to
    if (body->y < mid_y) {
        if (body->x < mid_x) {
            return node->nw;  // Northwest
        } else {
            return node->ne;  // Northeast
        }
    } else {
        if (body->x < mid_x) {
            return node->sw;  // Southwest
        } else {
            return node->se;  // Southeast
        }
    }
}

// Inserts a celestial body into the quadtree
void insert_body(QuadTreeNode* node, CelestialBody* body) {
    // Check if the body is within the bounds of this node
    if (!is_in_bounds(node, body)) {
        return;  // Body is out of bounds
    }
    
    // Case 1: Empty node (leaf with no body)
    if (node->body == NULL && node->nw == NULL) {
        node->body = body;
        return;
    }
    
    // Case 2: Leaf node with a body
    if (node->body != NULL && node->nw == NULL) {
        // Create four children
        subdivide(node);
        
        // Move the existing body to the appropriate quadrant
        CelestialBody* existing_body = node->body;
        node->body = NULL;  // Remove body from this node
        
        // Insert the existing body into the appropriate child
        insert_body(get_quadrant(node, existing_body), existing_body);
        
        // Continue with inserting the new body
    }
    
    // Case 3: Internal node (already subdivided)
    // Insert the new body into the appropriate quadrant
    insert_body(get_quadrant(node, body), body);
}

// Recursively calculates the center of mass for the node
void calculate_center_of_mass(QuadTreeNode* node) {
    if (node == NULL) {
        return;
    }
    
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
    
    // Internal node: calculate center of mass for each child
    calculate_center_of_mass(node->nw);
    calculate_center_of_mass(node->ne);
    calculate_center_of_mass(node->sw);
    calculate_center_of_mass(node->se);
    
    // Reset values
    node->total_mass = 0.0;
    node->center_x = 0.0;
    node->center_y = 0.0;
    
    // Add contributions from each non-empty child
    if (node->nw->total_mass > 0) {
        node->total_mass += node->nw->total_mass;
        node->center_x += node->nw->center_x * node->nw->total_mass;
        node->center_y += node->nw->center_y * node->nw->total_mass;
    }
    
    if (node->ne->total_mass > 0) {
        node->total_mass += node->ne->total_mass;
        node->center_x += node->ne->center_x * node->ne->total_mass;
        node->center_y += node->ne->center_y * node->ne->total_mass;
    }
    
    if (node->sw->total_mass > 0) {
        node->total_mass += node->sw->total_mass;
        node->center_x += node->sw->center_x * node->sw->total_mass;
        node->center_y += node->sw->center_y * node->sw->total_mass;
    }
    
    if (node->se->total_mass > 0) {
        node->total_mass += node->se->total_mass;
        node->center_x += node->se->center_x * node->se->total_mass;
        node->center_y += node->se->center_y * node->se->total_mass;
    }
    
    // Normalize to get the actual center of mass
    if (node->total_mass > 0) {
        node->center_x /= node->total_mass;
        node->center_y /= node->total_mass;
    } else {
        // Default to geometric center if no mass
        node->center_x = node->x + node->width / 2.0;
        node->center_y = node->y + node->height / 2.0;
    }
}

// Recursively frees the quadtree
void free_quadtree(QuadTreeNode* node) {
    if (node == NULL) {
        return;
    }
    
    // Recursively free children
    free_quadtree(node->nw);
    free_quadtree(node->ne);
    free_quadtree(node->sw);
    free_quadtree(node->se);
    
    // Free the node itself (but not the body - bodies are managed separately)
    free(node);
}

// Calculates force on a body using the quadtree (Barnes-Hut approach)
void calculate_force_from_quadtree(CelestialBody* body, QuadTreeNode* node, double theta, double* fx, double* fy) {
    if (node == NULL || node->total_mass == 0) {
        return;  // Empty node
    }
    
    // If this is a leaf with a body
    if (node->body != NULL && node->body != body) {
        // Calculate distance between bodies
        double dx = node->body->x - body->x;
        double dy = node->body->y - body->y;
        double distance_squared = dx*dx + dy*dy;
        double distance = sqrt(distance_squared);
        
        // Prevent division by zero or extremely small values
        if (distance < EPSILON) {
            return;
        }
        
        // Calculate gravitational force (F = G * m1 * m2 / r^2)
        double force_magnitude = G * SIMULATION_SCALE * body->mass * node->body->mass / distance_squared;
        
        // Resolve force into x and y components
        *fx += force_magnitude * dx / distance;
        *fy += force_magnitude * dy / distance;
        return;
    }
    
    // For internal nodes, check if we can use the center of mass approximation
    if (node->nw != NULL) {  // This is an internal node
        // Calculate distance to center of mass
        double dx = node->center_x - body->x;
        double dy = node->center_y - body->y;
        double distance = sqrt(dx*dx + dy*dy);
        
        // Calculate the ratio s/d (node size / distance)
        double s = fmax(node->width, node->height);
        double ratio = s / distance;
        
        // If ratio is less than theta, treat this node as a single body
        if (ratio < theta) {
            // Prevent division by zero
            if (distance < EPSILON) {
                return;
            }
            
            // Calculate gravitational force
            double force_magnitude = G * SIMULATION_SCALE * body->mass * node->total_mass / (distance * distance);
            
            // Resolve force into x and y components
            *fx += force_magnitude * dx / distance;
            *fy += force_magnitude * dy / distance;
        } else {
            // Otherwise, recursively calculate forces from each child
            calculate_force_from_quadtree(body, node->nw, theta, fx, fy);
            calculate_force_from_quadtree(body, node->ne, theta, fx, fy);
            calculate_force_from_quadtree(body, node->sw, theta, fx, fy);
            calculate_force_from_quadtree(body, node->se, theta, fx, fy);
        }
    }
}

// Updates the position and velocity of a body based on forces
void update_body(CelestialBody* body, double fx, double fy, double dt) {
    // Calculate acceleration (F = ma -> a = F/m)
    double ax = fx / body->mass;
    double ay = fy / body->mass;
    
    // Update velocity (v = v0 + a*t)
    body->vx += ax * dt;
    body->vy += ay * dt;
    
    // Update position (x = x0 + v*t)
    body->x += body->vx * dt;
    body->y += body->vy * dt;
}