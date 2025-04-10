#ifndef PLANET_H
#define PLANET_H

#define MAX_TRAJECTORY_POINTS 1000

typedef struct {
    char name[20];
    double mass;    // Mass of the planet
    double x, y;    // Position
    double vx, vy;  // Velocity
    double ax, ay;  // Acceleration
    double radius;  
    Uint32 color;   

    double trajectory_x[MAX_TRAJECTORY_POINTS];
    double trajectory_y[MAX_TRAJECTORY_POINTS];

    int trajectory_count;
} Planet;

#endif