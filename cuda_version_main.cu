#include <stdio.h>
#include <iostream>
#include <SDL.h>
#include "device_launch_parameters.h"
#include <cuda.h>
#include "cuda_runtime.h"
#include <cuda_runtime_api.h>
#include <device_functions.h>

#define NUM_PARTICLES 1000
#define SHARED_BUFFER_SIZE 1000
#define SCREEN_W 600
#define SCREEN_H 600
#define LINE_LEN 5
#define SPEED 0.25f
#define RADIUS 0.25f
#define PHASE_LAG 1.53f
#define COUPLING 1.0f
#define DT 0.1f

//#define USE_SHARED_BUFFER

__device__ float RADIUS_SQ = RADIUS*RADIUS;

static void HandleError(cudaError_t err, const char *file, int line) {
	if (err != cudaSuccess) {
		printf("%s in %s at line %d\n", cudaGetErrorString(err), file, line);
		exit(EXIT_FAILURE);
	}
}

#define HANDLE_ERROR( err ) (HandleError( err, __FILE__, __LINE__ ))


typedef struct {
	float x, y, phi, dphi;

} particle_t;

__global__ void timestepKernel(particle_t* particles)
{


#ifdef USE_SHARED_BUFFER
	__shared__ particle_t sharedParticles[SHARED_BUFFER_SIZE];
#endif

	unsigned int id = blockIdx.x * blockDim.x + threadIdx.x;

	if (id < NUM_PARTICLES)
	{
		// Get particle to be updated by this thread.
		particle_t particle = particles[id];
		float dphi = 0.0f;
		float nearCount = 0.0f;

#ifdef USE_SHARED_BUFFER
		////
		// Shared memory version.
		////
		for (int i = 0; i < NUM_PARTICLES / SHARED_BUFFER_SIZE; i++) {
			// In each phase (i), one thread loads one particle into shared memory.
			sharedParticles[threadIdx.x] = particles[i*SHARED_BUFFER_SIZE + threadIdx.x];

		    //__syncthreads(); // Dangerous - make sure block size evenly divides particle count.

			// Find nearby particles and update direction.
			for (int j = 0; j < SHARED_BUFFER_SIZE; j++) {
				particle_t other = sharedParticles[j];

				float dx = particle.x - other.x;
				float dy = particle.y - other.y;
				if (dx*dx + dy*dy < RADIUS_SQ)
				{
					dphi += sin(other.phi - particle.phi - PHASE_LAG);
					nearCount += 1.0f;
				}
			}
		}
#else
		////
		// Non-shared memory version.
		////
		for (int j = 0; j < NUM_PARTICLES; j++) {
			particle_t other = particles[j];

			float dx = particle.x - other.x;
			float dy = particle.y - other.y;
			if (dx*dx + dy*dy < RADIUS_SQ)
			{
				dphi += sin(other.phi - particle.phi - PHASE_LAG);
				nearCount += 1.0f;
			}
		}
#endif
		particle.x += SPEED*DT*cos(particle.phi);
		particle.y += SPEED*DT*sin(particle.phi);

		// Wrap around at edges.
		if (particle.x > 1) particle.x -= 1;
		if (particle.x < 0) particle.x += 1;
		if (particle.y > 1) particle.y -= 1;
		if (particle.y < 0) particle.y += 1;

		// Update direction if there were any interactions.
		if (nearCount > 0) {
			particle.phi += DT*(COUPLING / nearCount)*dphi;
		}

		// Write new particle properties back to global memory.
		particles[id] = particle;
	}
}

int main(int argc, char** argv)
{
	// Allocate host buffer to store particle states.
	particle_t* particles;
	cudaMallocHost(&particles, sizeof(particle_t)*NUM_PARTICLES);

	// Random initial positions / directions.
	for (int i = 0; i < NUM_PARTICLES; i++)
	{
		particles[i].x = (float)rand() / (float)RAND_MAX;
		particles[i].y = (float)rand() / (float)RAND_MAX;
		particles[i].phi = ((float)rand() / (float)RAND_MAX) * 2 * M_PI;
	}

	// Initialize SDL and create window.
	if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
		perror("Error initializing SDL.");
		exit(1);
	}
	SDL_Window* window = SDL_CreateWindow("Vicsek",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		SCREEN_W, SCREEN_H,
		SDL_WINDOW_OPENGL);
	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);

	// Allocate device memory for particles.
	particle_t* dev_particles;
	HANDLE_ERROR(cudaMalloc((void**)&dev_particles,
		NUM_PARTICLES * sizeof(particle_t)));

	HANDLE_ERROR(cudaMemcpy(dev_particles,
		particles,
		NUM_PARTICLES*sizeof(particle_t),
		cudaMemcpyHostToDevice));

	int threadsPerBlock = SHARED_BUFFER_SIZE;
	dim3 gridSize(NUM_PARTICLES / threadsPerBlock);
	dim3 blockSize(threadsPerBlock);

	SDL_Event event;
	int running = 1;
	unsigned int lastTicks = SDL_GetTicks();
	float t = 0;

	int prePause = 2;
	float totalUpdate = 0;
	float totalRender = 0;
	int counter = 0;

	while (running)
	{

		unsigned int ticks = SDL_GetTicks();
		float dt = (ticks - lastTicks) / 1000.0f;
		lastTicks = ticks;

		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_QUIT)
			{
				running = 0;
				break;
			}

		}

		// Update particles using the GPU.
		timestepKernel << <gridSize, blockSize >> >(dev_particles);

		// Copy particle positions back to host.
		HANDLE_ERROR(cudaMemcpy(particles,
			dev_particles,
			NUM_PARTICLES*sizeof(particle_t),
			cudaMemcpyDeviceToHost));
		t += DT;

		unsigned int endUpdateTicks = SDL_GetTicks();

		// Draw particles.
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
		SDL_RenderClear(renderer);
		for (int i = 0; i < NUM_PARTICLES; i++)
		{

			SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
			SDL_RenderDrawLine(renderer,
				particles[i].x*SCREEN_W,
				particles[i].y*SCREEN_H,
				particles[i].x*SCREEN_W + LINE_LEN*cos(particles[i].phi),
				particles[i].y*SCREEN_H + LINE_LEN*sin(particles[i].phi));
		}
		SDL_RenderPresent(renderer);
		unsigned int endRenderTicks = SDL_GetTicks();


		std::cout << t << ": Update took " << endUpdateTicks - ticks << "ms. Draw took " << endRenderTicks - endUpdateTicks << "ms." << std::endl;
		 
		// Update the total amount of time
		totalUpdate += endUpdateTicks - ticks;
 		totalRender += endRenderTicks - endUpdateTicks;
		counter++;

		// Stop after 50 updates NOT WORKING YET AS CAN'T COMPARE FLOATS
		if (counter == 500) {

			break;
		}

	}

	// Get the average update and render time across the 50 updates
	float averageUpdate = totalUpdate / 500;
	float averageRender = totalRender / 500;

	std::cout << averageUpdate << " " << averageRender << std::endl;

	return 0;
}
