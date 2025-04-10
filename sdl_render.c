#include <stdio.h>
#include <SDL2/SDL.h>
#include <math.h>
#include <SDL2/SDL_ttf.h>

#include "planet.h"

#define WIDTH 2400
#define HEIGHT 2400
#define COLOR_WHITE 0xffffffff
#define COLOR_BLACK 0x00000000

#define G 1.0                   // Gravitational constant (scaled for simulation)

TTF_Font* load_font(const char* font_path, int font_size) {

    TTF_Font* font = TTF_OpenFont(font_path, font_size);  
    if (font == NULL) {
        printf("Failed to load font: %s\n", TTF_GetError());
    }
    return font;
}

void DrawButton(SDL_Renderer* renderer, int x, int y, int w, int h, TTF_Font* font, const char* text, SDL_Color text_color) {
    // Draw button background
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); // Gray
    SDL_Rect button_rect = {x, y, w, h};
    SDL_RenderFillRect(renderer, &button_rect);

    SDL_Color label_color = {255, 255, 255, 255};

    // Render "Zoom" label
    SDL_Surface* zoom_surface = TTF_RenderText_Solid(font, "Zoom", label_color);
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

    // Render "Speed" label
    SDL_Surface* speed_surface = TTF_RenderText_Solid(font, "Speed", label_color);
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

    // Render text
    SDL_Surface* text_surface = TTF_RenderText_Solid(font, text, text_color);
    // if (!text_surface) {
    //     printf("Text surface error: %s\n", TTF_GetError());
    //     return;
    // }
    SDL_Texture* text_texture = SDL_CreateTextureFromSurface(renderer, text_surface);
    // if (!text_texture) {
    //     printf("Text texture error: %s\n", SDL_GetError());
    //     SDL_FreeSurface(text_surface);
    //     return;
    // }

    int text_w, text_h;
    SDL_QueryTexture(text_texture, NULL, NULL, &text_w, &text_h);
    SDL_Rect text_rect = {x + (w - text_w) / 2, y + (h - text_h) / 2, text_w, text_h};
    SDL_RenderCopy(renderer, text_texture, NULL, &text_rect);

    SDL_FreeSurface(text_surface);
    SDL_DestroyTexture(text_texture);
}

void draw_circle_border(SDL_Renderer* renderer, int cx, int cy, int radius, Uint8 r, Uint8 g, Uint8 b, Uint8 a, int border_thickness) {
    SDL_SetRenderDrawColor(renderer, r, g, b, a);

    for (int y = cy - radius; y <= cy + radius; y++) {
        for (int x = cx - radius; x <= cx + radius; x++) {
            if ((x - cx) * (x - cx) + (y - cy) * (y - cy) >= (radius - border_thickness) * (radius - border_thickness) && (x - cx) * (x - cx) + (y - cy) * (y - cy) <= radius * radius) {
                SDL_RenderDrawPoint(renderer, x, y);
            }
        }
    }
}

void render_planets(SDL_Renderer* renderer, Planet planets[], int num_planets, double pixels_per_AU, TTF_Font* font) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);


    // Draw trajectories
    for (int i = 0; i < num_planets; i++) {
        if (planets[i].trajectory_count > 1) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            for (int j = 1; j < planets[i].trajectory_count; j++) {
                int x1 = WIDTH / 2 + (int)(planets[i].trajectory_x[j - 1] * pixels_per_AU);
                int y1 = HEIGHT / 2 - (int)(planets[i].trajectory_y[j - 1] * pixels_per_AU);
                int x2 = WIDTH / 2 + (int)(planets[i].trajectory_x[j] * pixels_per_AU);
                int y2 = HEIGHT / 2 - (int)(planets[i].trajectory_y[j] * pixels_per_AU);
                SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
            }
        }
    }

    for (int i = 0; i < num_planets; i++) {
        int screen_x = WIDTH / 2 + (int)(planets[i].x * pixels_per_AU);
        int screen_y = HEIGHT / 2 - (int)(planets[i].y * pixels_per_AU);
        int radius = (int)planets[i].radius;

        Uint8 r = (planets[i].color >> 16) & 0xFF;
        Uint8 g = (planets[i].color >> 8) & 0xFF;
        Uint8 b = planets[i].color & 0xFF;
        Uint8 a = 255;  // Fully opaque

        draw_circle_border(renderer, screen_x, screen_y, radius, r, g, b, a, 2);
    }

    SDL_Color text_color = {255, 255, 255, 255}; // White buttons
    DrawButton(renderer, WIDTH - 100, 20, 50, 40, font, "+", text_color);  // Zoom In
    DrawButton(renderer, WIDTH - 100, 80, 50, 40, font, "-", text_color);  // Zoom Out
    DrawButton(renderer, WIDTH - 100, 200, 50, 40, font, "+", text_color); // Increase dt
    DrawButton(renderer, WIDTH - 100, 260, 50, 40, font, "-", text_color); // Decrease dt

    SDL_RenderPresent(renderer);
}

void calculate_force(Planet* planet, Planet* sun, double* fx, double* fy) {
    double dx = sun->x - planet->x;
    double dy = sun->y - planet->y;
    double r_squared = dx * dx + dy * dy;
    if (r_squared == 0) {
        *fx = 0.0;
        *fy = 0.0;
        return;
    }
    double r = sqrt(r_squared);
    double F = (G * planet->mass * sun->mass) / r_squared;  // Newton’s law
    *fx = F * (dx / r);  // Force in x-direction
    *fy = F * (dy / r);  // Force in y-direction
}

void update_simulation(Planet planets[], int num_planets, double dt, int* frame_count, int trajectory_interval) {
    
    Planet* sun = &planets[0];  // Sun is at index 0
    for (int i = 1; i < num_planets; i++) {
        double fx, fy;
        calculate_force(&planets[i], sun, &fx, &fy);
        planets[i].ax = fx / planets[i].mass;  // a = F/m
        planets[i].ay = fy / planets[i].mass;
        planets[i].vx += planets[i].ax * dt;   // Update velocity
        planets[i].vy += planets[i].ay * dt;
        planets[i].x += planets[i].vx * dt;    // Update position
        planets[i].y += planets[i].vy * dt;
    }

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
    (*frame_count)++;
}

#define NUM_PLANETS 9

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
        planets[i].vy = (i == 0) ? 0.0 : sqrt(1.0 / semi_major_axes[i]);
        planets[i].ax = 0.0;
        planets[i].ay = 0.0;
        planets[i].radius = 15.0;
        planets[i].color = colors[i];
    }
}

int main() {
    double pixels_per_AU = 120.0;  
    double zoom_step = 20.0;      
    double min_zoom = 40;        
    double max_zoom = 400.0;

    double dt = 0.001;            
    double dt_step = 0.001;        
    double min_dt = 0.0001;        
    double max_dt = 0.1;          

    int frame_count = 0;
    int trajectory_interval = 10;

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("Solar System", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, 0);

    TTF_Init();

    if (!window) {
        printf("Window creation failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        printf("Renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    int running = 1;
    initialize_planets();

    TTF_Font* font = load_font("./fonts/Arial.ttf", 30);    
    

    
     while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            running = 0;
        }  else if (event.type == SDL_MOUSEBUTTONDOWN) {
                int x = event.button.x;
                int y = event.button.y;

                // Zoom buttons (top-right corner)
                if (x >= WIDTH - 100 && x <= WIDTH - 50 && y >= 20 && y <= 60) {  // Zoom In (Plus)
                    pixels_per_AU += zoom_step;
                    if (pixels_per_AU > max_zoom) pixels_per_AU = max_zoom;
                } else if (x >= WIDTH - 100 && x <= WIDTH - 50 && y >= 80 && y <= 120) {  // Zoom Out (Minus)
                    pixels_per_AU -= zoom_step;
                    if (pixels_per_AU < min_zoom) pixels_per_AU = min_zoom;
                }

                // Delta time buttons (below zoom controls)
                if (x >= WIDTH - 100 && x <= WIDTH - 50 && y >= 200 && y <= 240) {  // Increase delta time (Plus)
                    dt += dt_step;
                    if (dt > max_dt) dt = max_dt;
                } else if (x >= WIDTH - 100 && x <= WIDTH - 50 && y >= 260 && y <= 300) {  // Decrease delta time (Minus)
                    dt -= dt_step;
                    if (dt < min_dt) dt = min_dt;
                }
            }
        }

        update_simulation(planets, NUM_PLANETS, dt, &frame_count, trajectory_interval);
        render_planets(renderer, planets, NUM_PLANETS, pixels_per_AU, font);

    }

    TTF_CloseFont(font);
    TTF_Quit();
    SDL_DestroyRenderer(renderer);

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}