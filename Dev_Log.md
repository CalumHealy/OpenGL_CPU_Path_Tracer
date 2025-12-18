# CPU OpenGL Path Tracer Dev Log

## 3/12/2025 22:47
ChatGPT has done pretty much everything up to this point. I came up with some of the logic and was familiar with some of the techniques, but ChatGPT did most of the implementation. I left it off last night after implementing the noise generation and averaging, and must now implement a more efficient randomo generator, and clean up the code.  
With the old random_device/distrib random generation, 1920x1080 is getting about 40FPS. 

## 3/12/2025 22:57
The new random generator is a pseudo-random number generator (PRNG) based on an xorshift algorithm. The seed is a "random" number. The lines with "seed ^= seed << x;" then change the seed by shifting left/right (indicated by direction of arrows) by x bits, then XORs with itself. The function then takes the lowest 8 bits of the updated seed, casts it to uint8_t, and therefore returns a random number between 0 and 255.  
With this new bitwise PRNG, a resolution of 1920x1080 is getting about 85FPS. 

## 16/12/2025 15:32
Last night I added the scene structure with triangles, vertices, materials, camera, etc. I modified calculatePixel() to scale a single triangle so that its world space coordinates matched the screen space coordinates such that it was as large as possible while filling the screen, with (0,0) at the bottom left of the screen.  
I also started implementing a config file system using nlohmann's JSON library, all in a single header file. I must finish this integration now and decide on a format for the config file. I'll probably have a list of vertices, a lift of triangles, list of materials, etc. Much easier to define then there than in code. OBJ file parsing would be better, but I might leave that for the next version of this project (shader support), but I might add it into this one.  
An issue that arose last night is I can no longer launch the program from Visual Studio, I must do the usual configuring and generating in CMake, build it in Visual Studio, but then launch it from the command line by going to "C:\Users\calum\Random Documents\Code Projects\OpenGL\OpenGL_CPU_Path_Tracer", and then run "build\Release\OpenGLProject.exe". Not an ideal situation, but I'll try again to fix it at some point in the future, maybe. 

## 18/12/2025 1:27
I'm currently experimenting with accessing config file variables. I have the following config file: 
```
{
    "WIDTH": 1920,
    "HEIGHT": 1080,
    "FOV": 90,
    "VERTICES": [
        [0.0, 0.0, 0.0],
        [1.0, 0.0, 0.0],
        [1.0, 1.0, 1.0],
        [2.0, 2.0, 2.0],
        [3.0, 3.0, 3.0]
    ],
    "TRIANGLES": [
        [0, 1, 2, [0.0, 0.0, 1.0], 0]
    ],
    "CAMERA": {
        "POSITION": [0.0, 0.0, -10.0],
        "FORWARD": [0.0, 0.0, 1.0],
        "RIGHT": [],
        "UP": []
    }
}
```
and the following code
```
    int width = config["WIDTH"];
    int height = config["HEIGHT"];
    float fov = config["FOV"];
    std::cout << width << std::endl;
    // TODO: Modify code to use config file
    float cameraPositionZ = config["CAMERA"]["POSITION"][2];
    std::cout << "Camera Position Z: " << cameraPositionZ << std::endl;
    int verticesListLength = config["VERTICES"].size();
    std::cout << "Vertices List Length: " << verticesListLength << std::endl;
```
and with these I got the following output: 
```
1920
Camera Position Z: -10
Vertices List Length: 5
```
Excellent! 

## 18/12/2025 2:21
Running the code using config["THING"] every time resulted in 1FPS (or likely lower, but the counter showed 1). This is because accessing the JSON object is very slow, and I was doing it for every pixel. I was also copying the data which is bad. I am now working on a RenderState struct which will be much better. ChatGPT described the optimal mental model in phases: 
- Phase 1 - Configuration:
    - Read JSON
    - Validate
    - Build Scene, Camera, Materials
- Phase 2 - Freeze State:
    - Build RenderState
    - Everything becomes read-only
- Phase 3 - Rendering:
    - Threads read from RenderState
    - No mutation except output buffers

## 18/12/2025 3:08
I replaced the config["THING"] with the RenderState and the FPS is now up to ~21FPS. The current state of the code is in commit 