#include "mpi.h"
#include <SDL2/SDL.h>
#include <iostream>

#define NUM_PARTICLES 1000
#define SCREEN_W 600
#define SCREEN_H 480
#define LINE_LEN 5
#define SPEED 0.2f
#define RADIUS 0.25f
#define PHASE_LAG 1.53f
#define COUPLING 1.0f
#define DT 0.1f

// Define radius squared
const float RADIUS_SQRT = RADIUS*RADIUS;

//The SDL window
SDL_Window* window = NULL;

// Initialise renderer
SDL_Renderer* renderer = NULL;

// Start SDL event
SDL_Event event;

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
     * Split the particles into chunks and process like that
     *  - get the comm size
     *  - make it so it splits particles into even chunks for each process
     *  - base process initalises random
     *
     *  - then it does bcast to all processes so they know all other particles positions
     *      - they still need to only update their amount
     *      - potentially in the loop they know they're amount and they need to calculate their start and finish position
     *  - once updated, they can do an allgather
     *      - So root node can render all particles
     *      - and all processes have the updated versions of the particles
     *
     *  - then can split further and make multi threaded
     */


/**
 * Update the position and direction of particle based off of other particles
 */
particle_struct* updateParticles(particle_struct* particles, int particlesPerProcess, int myRank) {

    // Calculate first and last index of this process's particles
    int firstIndex = myRank * particlesPerProcess;
    int lastIndex  = firstIndex + particlesPerProcess;

    // Initialise counter for each processes' particles
    int counter = 0;

    // Create structure for particles
    particle_struct* myParticles;
    myParticles = (particle_struct*)malloc(sizeof(particle_struct)*particlesPerProcess);

    // Iterate all particles
    for (int i = firstIndex; i < lastIndex; i++) {

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

        // Write new particle properties
        myParticles[counter] = particle;

        counter = counter + 1;
    }

    return myParticles;
}

void close() {

    //Destroy window
    SDL_DestroyWindow(window);
    window = NULL;

    //Quit SDL subsystems
    SDL_Quit();

}

int main(int argc, char** argv) {

    // Initialise MPI
    MPI_Init(NULL, NULL);

    // Initialise commSize and myRank
    int myRank, commSize;
    MPI_Comm_rank(MPI_COMM_WORLD, &myRank);
    MPI_Comm_size(MPI_COMM_WORLD, &commSize);

    particle_struct* particles;
    particle_struct* processParticles;
    particles = (particle_struct*)malloc(sizeof(particle_struct)*NUM_PARTICLES);

    // Check if root process
    if (myRank == 0) {

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
        window = SDL_CreateWindow("Vicsek",
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
        renderer = SDL_CreateRenderer(window, -1, 0);
    }

    // Broadcast the particles to all other processes
    MPI_Bcast(particles, 3*NUM_PARTICLES, MPI_FLOAT, 0, MPI_COMM_WORLD);

    // Calculate the how many particles each process should process
    int particlesPerProcess = NUM_PARTICLES / commSize;

    // Initialise bool for quiting program
    bool quit = false;

    while(!quit)
    {
        //Handle events on queue 
        while( SDL_PollEvent(&event) != 0 ) {

            //User requests quit
            if(event.type == SDL_QUIT) {
                quit = true;
            }

        }

        // Update the particles
        processParticles = updateParticles(particles, particlesPerProcess, myRank);

        // All gather so each process has a copy of the updated particles
        MPI_Allgather(processParticles, 3*particlesPerProcess, MPI_FLOAT,
            particles, 3*particlesPerProcess, MPI_FLOAT, MPI_COMM_WORLD);

        // Check if root node
        if (myRank == 0) {

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
        }

    }

    //Free resources and close SDL
    close();
    MPI_Finalize();
    return 0;
}
