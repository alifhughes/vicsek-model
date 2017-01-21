#include <SDL2/SDL.h>
#include <iostream>

#define NUM_PARTICLES 10000
#define SCREEN_W 600
#define SCREEN_H 480
#define LINE_LEN 5
#define SPEED 0.25f
#define RADIUS 0.25f
#define PHASE_LAG 1.53f
#define COUPLING 1.0f
#define DT 0.1f

// Define radius squared
const float RADIUS_SQRT = RADIUS*RADIUS;

/*
 * Struct for each particle contains:
 *  - x
 *  - y
 *  - phi direction phi is angle in radions
 */
typedef struct {
    float x, y, phi;
} particle_struct;

/**
 * Update the position and direction of particle based off of other particles
 */
void updateParticles(particle_struct* particles) {

    // Iterate all particles
    for (int i = 0; i < NUM_PARTICLES; i++) {

        // Get particle to be updated by this iteration
        particle_struct particle = particles[i];

        // Initialise dphi and near count
        float dphi = 0.0f;
        float near_count = 0.0f;

        // Loop through all particles
        for (int j = 0; j < NUM_PARTICLES; j++) {

            // Initialise other particle
            particle_struct other = particles[j];

            // Calculate the distance between current particle and other particle
            float dx = particle.x - other.x;
            float dy = particle.y - other.y;

            // Check to see if it is in the radius
            if (dx*dx + dy*dy < RADIUS_SQRT)
            {
                // Is in radius update direction and near count
                dphi += sin(other.phi - particle.phi - PHASE_LAG);
                near_count += 1.0f;
            }
        }

        // Update speed and direction
        particle.x += SPEED*DT*cos(particle.phi);
        particle.y += SPEED*DT*sin(particle.phi);

        // Wrap around at edges
        if (particle.x > 1) particle.x -= 1;
        if (particle.x < 0) particle.x += 1;
        if (particle.y > 1) particle.y -= 1;
        if (particle.y < 0) particle.y += 1;

        // Update direction if there were any interactions.
        if (near_count > 0) {
            particle.phi += DT*(COUPLING/near_count)*dphi;
        }

        // Write new particle properties back to global memory.
        particles[i] = particle;
    }

}

int main(int argc, char** argv) {

    // Create structure for particles
    particle_struct* particles;
    particles = (particle_struct*)malloc(sizeof(particle_struct)*NUM_PARTICLES);

    // Initialise random positions and direction
    for (int i = 0; i < NUM_PARTICLES; i++)
    {
        particles[i].x = (float)rand() / (float)RAND_MAX;
        particles[i].y = (float)rand() / (float)RAND_MAX;
        particles[i].phi = ((float)rand() / (float)RAND_MAX) * 2 * M_PI;
    }

    // Initialise window and check for error
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
        std::cout << "Error initializing SDL: " << SDL_GetError() << std::endl;
    }

    // Create window
    SDL_Window* window = SDL_CreateWindow("Vicsek",
      SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED,
      SCREEN_W, SCREEN_H,
      SDL_WINDOW_SHOWN);

    // Check if window was created
    if (window == NULL) {
        // Error whilst creating
        std::cout << "Error creating window: " << SDL_GetError() << std::endl;
    }

    // Initialise renderer
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);

    // Start SDL event
    SDL_Event event;
    int running = 1;
    unsigned int lastTicks = SDL_GetTicks();
    double t = 0;
    float totalUpdateTime = 0.0f;
    float totalRenderTime = 0.0f;
    int counter = 0;

    while(running)
    {

        unsigned int ticks = SDL_GetTicks();
        float dt = (ticks - lastTicks) / 1000.0f;
        lastTicks = ticks;

        // Update the particles
        updateParticles(particles);

        t += dt;

        unsigned int endUpdateTicks = SDL_GetTicks();

        // Draw particles
        SDL_RenderClear(renderer);
        for (int i = 0; i < NUM_PARTICLES; i++)
        {
            SDL_RenderDrawLine(renderer,
               particles[i].x*SCREEN_W,
               particles[i].y*SCREEN_H,
               particles[i].x*SCREEN_W + LINE_LEN*cos(particles[i].phi),
               particles[i].y*SCREEN_H + LINE_LEN*sin(particles[i].phi));
        }
        SDL_RenderPresent(renderer);

       unsigned int endRenderTicks = SDL_GetTicks();

       std::cout << t << ": Update took " << endUpdateTicks - ticks << "ms. Draw took " << endRenderTicks - endUpdateTicks << "ms." << std::endl;

       totalUpdateTime += endUpdateTicks - ticks;
       totalRenderTime += endRenderTicks - endUpdateTicks;


       counter++;

       if (counter == 50) {
           break;
       }


    }

    float averageUpdate = totalUpdateTime / 50;
    float averageRender = totalRenderTime / 50;

    std::cout << averageUpdate << " " << averageRender << std::endl;

    return 0;
}
