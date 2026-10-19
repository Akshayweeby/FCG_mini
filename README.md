# Akshayanagara Lake F1 OpenGL Simulator

This is a C++17 OpenGL 3.3 simulator based on the supplied Akshayanagara Lake aerial map. The four road segments form the lake perimeter loop, with marked lanes, lake surface, sidewalks/buildings, and a procedural 3D F1 car. The supplied map and car lineup are loaded as OpenGL textures (`lake_map_reference.png`, `car_reference.png`).

## Build on Windows

Install Visual Studio 2022 and CMake, then from this folder run:

```powershell
cmake -S . -B build
cmake --build build --config Release
.
build\Release\LakeF1.exe
```

CMake fetches GLFW, GLM, and GLAD on the first configure. Internet access is required once.

## Controls

W/Up accelerates, S/Down brakes/reverses, A/D or arrow keys steer, and left mouse drag orbits the camera. The track and buildings are intentionally simple and editable in `drawTrack()`.
