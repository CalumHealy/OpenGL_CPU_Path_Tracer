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
I replaced the config["THING"] with the RenderState and the FPS is now up to ~21FPS. The current state of the code is in commit 99a70f2513802460de1bd50ced8bc569e22bb9d8 (Forgot to add main.cpp changes to previous commit, adding now (I need to learn how to change/undo git commits)). I just noticed that the triangle starts to fade after a while, and goes completely black, then reappears at full brightness, only to fade again repeatedly. Something is going on with the iterations/scene data. 

## 18/12/2025 3:15
I just replaced 
```
threads.emplace_back(
                renderChunk,
                startY,
                endY,
                std::ref(accumPixels),
                std::ref(pixels),
                state
            );
```
with 
```
threads.emplace_back(
                renderChunk,
                startY,
                endY,
                std::ref(accumPixels),
                std::ref(pixels),
                std::cref(state)
            );
```
and the fading is now much slower. This indicates that the state object is the cause of the issue. I will ask Chet Jeopardy. One thing to note thought is that the FPS was the same. 

## 18/12/2025 4:19
I asked ChatGPT. I pointed out that I was incrementing the old iterations variable and not state.iterations, but I was using state.iterations in the averaging. I fixed this but the issue was still there. ChatGPT also said that clamping is essential in the pixel assigning lines. The last of these four methods uses clamping. 
```
            // pixels[i + 0] = accumPixels[i + 0] / (state.iterations + 1);
            // pixels[i + 1] = accumPixels[i + 1] / (state.iterations + 1);
            // pixels[i + 2] = accumPixels[i + 2] / (state.iterations + 1);

            // pixels[i + 0] = (unsigned char)(accumPixels[i + 0] / (state.iterations + 1));
            // pixels[i + 1] = (unsigned char)(accumPixels[i + 1] / (state.iterations + 1));
            // pixels[i + 2] = (unsigned char)(accumPixels[i + 2] / (state.iterations + 1));

            // pixels[i + 0] = static_cast<unsigned char>((accumPixels[i + 0] / (state.iterations + 1)));
            // pixels[i + 1] = static_cast<unsigned char>((accumPixels[i + 1] / (state.iterations + 1)));
            // pixels[i + 2] = static_cast<unsigned char>((accumPixels[i + 2] / (state.iterations + 1)));
            
            float inv = 1.0f / state.iterations;
            pixels[i + 0] = (unsigned char)glm::clamp(accumPixels[i + 0] * inv, 0.0f, 255.0f);
            pixels[i + 1] = (unsigned char)glm::clamp(accumPixels[i + 1] * inv, 0.0f, 255.0f);
            pixels[i + 2] = (unsigned char)glm::clamp(accumPixels[i + 2] * inv, 0.0f, 255.0f);
```
ChatGPT says that the first method is fine if the values are definitely between 0 and 255, but when I implement emissive materials or multiple samples, I can get values below 0 or above 255. The clamping solves this, so I will keep the clamping. 

## 18/12/2025 4:33
Chet gave me a very different form of this which removes the glm::clamp and just uses conditional checks to keep it between 0 and 255. It now also puts the assigning in a loop, which probably helps reduce duplication with new multiple lines of conditionals. 
```
    for (int y = startY; y < endY; y++) {
        for (int x = 0; x < width; x++) {

            int i = (y * width + x) * 3;

            calculatePixel(x, y, r, g, b, state);

            accumPixels[i + 0] += (float)r;
            accumPixels[i + 1] += (float)g;
            accumPixels[i + 2] += (float)b;

            float inv = 1.0f / (state.iterations + 1);

            for (int c = 0; c < 3; c++) {
                float val = accumPixels[i + c] * inv;
                if (val < 0.0f) val = 0.0f;
                else if (val > 255.0f) val = 255.0f;
                pixels[i + c] = static_cast<unsigned char>(val);
            }  

        }
    }
```
FPS is now around 33. 


## 18/12/2025 4:44
If else has now been replaced with std::min/max. The two of these apparently compile to similarly fast instructions so neither is objectively better than the other, but min/max shows intention better than if/else and it removes a line of code, so I'll go with it. It'ls also kind of cooler. 
```
val = std::min(std::max(val, 0.0f), 255.0f);
```
I'm not sure how I ended up doing all this when I'm supposed to be working on config file implementation. I think it was state.scene causing issues with pointers, and then I fell down an optimization rabbit hole. I think it might be time to clean up some code a little, remove unnecessary comments. 

## 18/12/2025 5:07
I did a little work on the structure of the config file and now closer resembles the structure of the code with the scene object and stuff, and it is more readable now with named variables inside Triangle and stuff instead of a single list containing multiple numbers for different things. 