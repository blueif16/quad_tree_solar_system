#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include "barnes_hut.h"
#include "planet.h"

// Define the quadtree node structure based on your provided code
// Modified to work with Planet structure instead of CelestialBody
typedef struct QuadTreeNode {
    double x, y, width, height;  // Boundaries of the node
    Planet* body;                // Pointer to a planet (if leaf)
    struct QuadTreeNode *nw, *ne, *sw, *se;  // Child nodes
    double total_mass;           // Sum of masses in this region
    double center_x, center_y;   // Center of mass of this node
} QuadTreeNode;

// Small value to prevent division by zero
#define EPSILON 1e-9

// Creates a new quadtree node covering the given region
QuadTreeNode* bh_create_quadtree(double x, double y, double width, double height) {
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

// Checks if a planet is within the boundaries of a quadtree node
bool bh_is_in_bounds(QuadTreeNode* node, Planet* planet) {
    return (planet->x >= node->x && 
            planet->x < node->x + node->width &&
            planet->y >= node->y && 
            planet->y < node->y + node->height);
}

// Subdivides a quadtree node into four quadrants
void bh_subdivide(QuadTreeNode* node) {
    double half_width = node->width / 2.0;
    double half_height = node->height / 2.0;
    
    // Create the four child nodes
    node->nw = bh_create_quadtree(node->x, node->y, half_width, half_height);
    node->ne = bh_create_quadtree(node->x + half_width, node->y, half_width, half_height);
    node->sw = bh_create_quadtree(node->x, node->y + half_height, half_width, half_height);
    node->se = bh_create_quadtree(node->x + half_width, node->y + half_height, half_width, half_height);
}

// Determines which quadrant a planet belongs to
QuadTreeNode* bh_get_quadrant(QuadTreeNode* node, Planet* planet) {
    double mid_x = node->x + node->width / 2.0;
    double mid_y = node->y + node->height / 2.0;
    
    // Check which quadrant the planet belongs to
    if (planet->y < mid_y) {
        if (planet->x < mid_x) {
            return node->nw;  // Northwest
        } else {
            return node->ne;  // Northeast
        }
    } else {
        if (planet->x < mid_x) {
            return node->sw;  // Southwest
        } else {
            return node->se;  // Southeast
        }
    }
}

// Inserts a planet into the quadtree
void bh_insert_planet(QuadTreeNode* node, Planet* planet) {
    // Check if the planet is within the bounds of this node
    if (!bh_is_in_bounds(node, planet)) {
        return;  // Planet is out of bounds
    }
    
    // Case 1: Empty node (leaf with no body)
    if (node->body == NULL && node->nw == NULL) {
        node->body = planet;
        return;
    }
    
    // Case 2: Leaf node with a body
    if (node->body != NULL && node->nw == NULL) {
        // Create four children
        bh_subdivide(node);
        
        // Move the existing body to the appropriate quadrant
        Planet* existing_body = node->body;
        node->body = NULL;  // Remove body from this node
        
        // Insert the existing body into the appropriate child
        bh_insert_planet(bh_get_quadrant(node, existing_body), existing_body);
        
        // Continue with inserting the new body
    }
    
    // Case 3: Internal node (already subdivided)
    // Insert the new body into the appropriate quadrant
    bh_insert_planet(bh_get_quadrant(node, planet), planet);
}

// Recursively calculates the center of mass for the node
void bh_calculate_center_of_mass(QuadTreeNode* node) {
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
    bh_calculate_center_of_mass(node->nw);
    bh_calculate_center_of_mass(node->ne);
    bh_calculate_center_of_mass(node->sw);
    bh_calculate_center_of_mass(node->se);
    
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
void bh_free_quadtree(QuadTreeNode* node) {
    if (node == NULL) {
        return;
    }
    
    // Recursively free children
    bh_free_quadtree(node->nw);
    bh_free_quadtree(node->ne);
    bh_free_quadtree(node->sw);
    bh_free_quadtree(node->se);
    
    // Free the node itself (but not the body - bodies are managed separately)
    free(node);
}

// Calculates force on a planet using the quadtree (Barnes-Hut approach)
void bh_calculate_force(QuadTreeNode* node, Planet* planet, double G, double* fx, double* fy) {
    if (node == NULL || node->total_mass == 0) {
        return;  // Empty node
    }
    
    // If this is a leaf with a body
    if (node->body != NULL && node->body != planet) {
        // Calculate distance between bodies
        double dx = node->body->x - planet->x;
        double dy = node->body->y - planet->y;
        double distance_squared = dx*dx + dy*dy;
        double distance = sqrt(distance_squared);
        
        // Prevent division by zero or extremely small values
        if (distance < EPSILON) {
            return;
        }
        
        // Calculate gravitational force (F = G * m1 * m2 / r^2)
        double force_magnitude = G * planet->mass * node->body->mass / distance_squared;
        
        // Resolve force into x and y components
        *fx += force_magnitude * dx / distance;
        *fy += force_magnitude * dy / distance;
        return;
    }
    
    // For internal nodes, check if we can use the center of mass approximation
    if (node->nw != NULL) {  // This is an internal node
        // Calculate distance to center of mass
        double dx = node->center_x - planet->x;
        double dy = node->center_y - planet->y;
        double distance = sqrt(dx*dx + dy*dy);
        
        // Calculate the ratio s/d (node size / distance)
        double s = fmax(node->width, node->height);
        double ratio = s / distance;
        
        // If ratio is less than theta, treat this node as a single body
        if (ratio < THETA) {
            // Prevent division by zero
            if (distance < EPSILON) {
                return;
            }
            
            // Calculate gravitational force
            double force_magnitude = G * planet->mass * node->total_mass / (distance * distance);
            
            // Resolve force into x and y components
            *fx += force_magnitude * dx / distance;
            *fy += force_magnitude * dy / distance;
        } else {
            // Otherwise, recursively calculate forces from each child
            bh_calculate_force(node->nw, planet, G, fx, fy);
            bh_calculate_force(node->ne, planet, G, fx, fy);
            bh_calculate_force(node->sw, planet, G, fx, fy);
            bh_calculate_force(node->se, planet, G, fx, fy);
        }
    }
}

// Update the simulation using Barnes-Hut for all gravitational interactions
void update_simulation_barnes_hut(Planet planets[], int num_planets, double dt, int* frame_count, int trajectory_interval, double G) {
    // Find boundaries for the quadtree (with some padding)
    double min_x = planets[0].x;
    double max_x = planets[0].x;
    double min_y = planets[0].y;
    double max_y = planets[0].y;
    
    for (int i = 0; i < num_planets; i++) {
        if (planets[i].x < min_x) min_x = planets[i].x;
        if (planets[i].x > max_x) max_x = planets[i].x;
        if (planets[i].y < min_y) min_y = planets[i].y;
        if (planets[i].y > max_y) max_y = planets[i].y;
    }
    
    // Add padding to ensure all planets fit
    double padding = fmax(max_x - min_x, max_y - min_y) * 0.1;
    min_x -= padding;
    max_x += padding;
    min_y -= padding;
    max_y += padding;
    
    // Make the region square to avoid distortion
    double width = max_x - min_x;
    double height = max_y - min_y;
    double max_dim = fmax(width, height);
    
    // Create the quadtree
    QuadTreeNode* root = bh_create_quadtree(min_x, min_y, max_dim, max_dim);
    
    // Insert all planets into the quadtree
    for (int i = 0; i < num_planets; i++) {
        bh_insert_planet(root, &planets[i]);
    }
    
    // Calculate center of mass for all nodes
    bh_calculate_center_of_mass(root);
    
    // Calculate forces on all planets
    for (int i = 0; i < num_planets; i++) {
        double fx = 0.0;
        double fy = 0.0;
        
        // Calculate gravitational forces using Barnes-Hut approximation
        bh_calculate_force(root, &planets[i], G, &fx, &fy);
        
        // Update acceleration (a = F/m)
        planets[i].ax = fx / planets[i].mass;
        planets[i].ay = fy / planets[i].mass;
    }
    
    // Update positions and velocities for all planets
    for (int i = 0; i < num_planets; i++) {
        // Update velocity (v = v + a*dt)
        planets[i].vx += planets[i].ax * dt;
        planets[i].vy += planets[i].ay * dt;
        
        // Update position (x = x + v*dt)
        planets[i].x += planets[i].vx * dt;
        planets[i].y += planets[i].vy * dt;
    }
    
    // Update trajectories
    if (*frame_count % trajectory_interval == 0) {
        for (int i = 0; i < num_planets; i++) {
            if (planets[i].trajectory_count < MAX_TRAJECTORY_POINTS) {
                planets[i].trajectory_x[planets[i].trajectory_count] = planets[i].x;
                planets[i].trajectory_y[planets[i].trajectory_count] = planets[i].y;
                planets[i].trajectory_count++;
            } else {
                // Shift array to discard oldest point
                for (int j = 0; j < MAX_TRAJECTORY_POINTS - 1; j++) {
                    planets[i].trajectory_x[j] = planets[i].trajectory_x[j + 1];
                    planets[i].trajectory_y[j] = planets[i].trajectory_y[j + 1];
                }
                planets[i].trajectory_x[MAX_TRAJECTORY_POINTS - 1] = planets[i].x;
                planets[i].trajectory_y[MAX_TRAJECTORY_POINTS - 1] = planets[i].y;
            }
        }
    }
    
    // Free the quadtree
    bh_free_quadtree(root);
    
    // Increment frame counter
    (*frame_count)++;
}