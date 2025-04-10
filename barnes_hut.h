#ifndef BARNES_HUT_H
#define BARNES_HUT_H

#include <stdbool.h>
#include "planet.h"  // For the Planet structure

// Forward declaration of QuadTreeNode from your provided code
typedef struct QuadTreeNode QuadTreeNode;

// Barnes-Hut algorithm accuracy parameter (theta)
// Lower values = more accurate but slower, higher values = less accurate but faster
#define THETA 0.7

// Creates a quadtree covering the given region
QuadTreeNode* bh_create_quadtree(double x, double y, double width, double height);

// Inserts a planet into the quadtree
void bh_insert_planet(QuadTreeNode* node, Planet* planet);

// Calculates the center of mass for all nodes in the quadtree
void bh_calculate_center_of_mass(QuadTreeNode* node);

// Calculate force on a planet using the Barnes-Hut approximation
void bh_calculate_force(QuadTreeNode* node, Planet* planet, double G, double* fx, double* fy);

// Update the simulation using Barnes-Hut algorithm
void update_simulation_barnes_hut(Planet planets[], int num_planets, double dt, int* frame_count, int trajectory_interval, double G);

// Free memory used by the quadtree
void bh_free_quadtree(QuadTreeNode* node);

#endif