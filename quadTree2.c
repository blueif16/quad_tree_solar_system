#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

// Constants
#define G 6.67430e-11  // Gravitational constant (m^3 kg^-1 s^-2)
#define EPSILON 1e-9   // Small value to prevent division by zero

// Structure to represent a celestial body
typedef struct {
    double x, y;        // Position coordinates
    double vx, vy;      // Velocity components
    double mass;        // Mass of the body
    double radius;      // Radius of the body (for collision detection)
    // Can add more properties like color, name, etc.
} CelestialBody;

// Quad-Tree node structure
typedef struct QuadTreeNode {
    double x, y, width, height;  // Boundaries of the node
    CelestialBody* body;         // Pointer to a body (if leaf)
    struct QuadTreeNode *nw, *ne, *sw, *se;  // Child nodes
    double total_mass;           // Sum of masses in this region
    double center_x, center_y;   // Center of mass of this node
} QuadTreeNode;

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
        double force_magnitude = G * body->mass * node->body->mass / distance_squared;
        
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
            double force_magnitude = G * body->mass * node->total_mass / (distance * distance);
            
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
