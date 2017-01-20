### C++ Vicsek particle simulation model using MPI

This project is to re-create the Vicsek particle simulation model in C++ both serially and multi-procces using MPI. Vicsek model simulates the collective motion of particles when they interact, similar to flocking behaviour. More info found here (https://en.wikipedia.org/wiki/Vicsek_model)

## To set up cuda project
https://libsdl.org/download-2.0.php  - download SDL2-devel-2.0.5-VC.zip

1. Create new cuda runtime project
2. Copy in code from cuda_version_main.cu into the main.cu file
3. Go project/project properties
4. On VC++ directories add to the Include Directories the include folder from download
5. On VC++ directories add to the Library Directories the lib/x86 version
6. On Linker add to Additional Dependencies the files SDL2.lib and SDL2main.lib
7. Open up the project in file explorer and add to the top level Debug folder in the project the SDL2.dll from lib/x86 folder
