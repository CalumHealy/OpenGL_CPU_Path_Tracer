#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <atomic>
#include <vector>
#include <array>
#include <string>
#include <nlohmann/json.hpp>
#include "font8x8_basic.h"
#include <glm/glm.hpp>
using json = nlohmann::json;

const unsigned int WIDTH = 1920;
const unsigned int HEIGHT = 1080;

const char* vertexShaderSource = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
out vec2 TexCoord;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    TexCoord = aTexCoord;
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D screenTex;
void main() {
    FragColor = texture(screenTex, TexCoord);
}
)";

GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    return s;
}

GLuint createProgram() {
    GLuint v = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint f = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    return p;
}

struct Camera {
    glm::vec3 position;
    glm::vec3 forward;
    glm::vec3 right;
    glm::vec3 up;
    float fov;
};

struct Triangle {
    int v0, v1, v2;
    glm::vec3 normal;
    int materialID;
};

enum class MaterialType { Lambertian, Metal, Dielectric, Emissive };

struct Material {
    MaterialType type;
    glm::vec3 albedo;
    float roughness;
    float refractiveIndex;
    glm::vec3 emission;
};

// std::vector<glm::vec3> vertices;
// std::vector<Triangle> triangles;
// std::vector<Material> materials;

struct Scene {
    std::vector<glm::vec3> vertices;
    std::vector<Triangle> triangles;
    std::vector<Material> materials;
};

struct Ray {
    glm::vec3 origin;
    glm::vec3 direction; // Normalized
};

glm::vec3 normalizeRay(glm::vec3 direction) {
    return glm::normalize(direction);
}

uint32_t seed = 314159265;
inline uint8_t rand8() {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return (uint8_t)(seed & 0xFF);
}

void calculatePixel(int x, int y, unsigned char& r, unsigned char& g, unsigned char& b, Camera camera, Scene scene) {
    // X is number of pixel across length of screen (0 - WIDTH)
    // Y is number of pixel down screen (0 - HEIGHT)
    // r, g, and b are references to the values for the pixel colour
    // Ray direction and position can be derived from camera values
    r = rand8();
    g = rand8();
    b = rand8();
    // r = 180;
    // g = 180; 
    // b = 180;
    // std::cout << "Camera Position: " << camera.position[0] << std::endl;
    // std::cout << "X: " << x << std::endl;
    // std::cout << "Y: " << y << std::endl;
    // std::cout << "v0 x: " << scene.vertices[scene.triangles[0].v0][0] << std::endl;
    // std::cout << "v0 y: " << scene.vertices[scene.triangles[0].v0][1] << std::endl;
    // std::cout << "v0 z: " << scene.vertices[scene.triangles[0].v0][2] << std::endl;
    // std::cout << "v1 x: " << scene.vertices[scene.triangles[0].v1][0] << std::endl;
    // std::cout << "v1 y: " << scene.vertices[scene.triangles[0].v1][1] << std::endl;
    // std::cout << "v1 z: " << scene.vertices[scene.triangles[0].v1][2] << std::endl;
    // std::cout << "v2 x: " << scene.vertices[scene.triangles[0].v2][0] << std::endl;
    // std::cout << "v2 y: " << scene.vertices[scene.triangles[0].v2][1] << std::endl;
    // std::cout << "v2 z: " << scene.vertices[scene.triangles[0].v2][2] << std::endl;
    // --- Convert world space to screen space [?] ---
    float minX = std::min(
        scene.vertices[scene.triangles[0].v0][0],
        std::min(
            scene.vertices[scene.triangles[0].v1][0],
            scene.vertices[scene.triangles[0].v2][0]
        )
    );
    float maxX = std::max(
        scene.vertices[scene.triangles[0].v0][0],
        std::max(
            scene.vertices[scene.triangles[0].v1][0],
            scene.vertices[scene.triangles[0].v2][0]
        )
    );
    float minY = std::min(
        scene.vertices[scene.triangles[0].v0][1],
        std::min(
            scene.vertices[scene.triangles[0].v1][1],
            scene.vertices[scene.triangles[0].v2][1]
        )
    );
    float maxY = std::max(
        scene.vertices[scene.triangles[0].v0][1],
        std::max(
            scene.vertices[scene.triangles[0].v1][1],
            scene.vertices[scene.triangles[0].v2][1]
        )
    );
    float triangleWidth = maxX - minX;
    float triangleHeight = maxY - minY;
    float scaleX = WIDTH / triangleWidth;
    float scaleY = HEIGHT / triangleHeight;
    float scale = std::min(scaleX, scaleY);
    std::vector<glm::vec2> screenVertices;
    for (const glm::vec3& vertex : {
        scene.vertices[scene.triangles[0].v0],
        scene.vertices[scene.triangles[0].v1],
        scene.vertices[scene.triangles[0].v2]
    }) {
        float screenX = (vertex.x - minX) * scale;
        float screenY = (vertex.y - minY) * scale;
        screenVertices.push_back(glm::vec2(screenX, screenY));
    }
    glm::vec2 A = screenVertices[0];
    glm::vec2 B = screenVertices[1];
    glm::vec2 C = screenVertices[2];

    glm::vec2 P(x, y);
    // Vectors from A
    glm::vec2 v0 = B - A;
    glm::vec2 v1 = C - A;
    glm::vec2 v2 = P - A;
    // Compute dot products
    float d00 = glm::dot(v0, v0);
    float d01 = glm::dot(v0, v1);
    float d11 = glm::dot(v1, v1);
    float d20 = glm::dot(v2, v0);
    float d21 = glm::dot(v2, v1);
    // Compute barycentric coordinates
    float denom = d00 * d11 - d01 * d01;
    float u = (d11 * d20 - d01 * d21) / denom;
    float v = (d00 * d21 - d01 * d20) / denom;
    float w = 1.0f - u - v;

    bool inside = (u >= 0) && (v >= 0) && (w >= 0);

    if (inside) {
        r = 255;
        g = 255;
        b = 255;
    } else {
        r = 0;
        g = 0;
        b = 0;
    }
}

void renderChunk(
    int startY, int endY,
    std::vector<float>& accumPixels,
    std::vector<unsigned char>& pixels,
    int iterations,
    Camera camera,
    Scene scene)
{
    unsigned char r, g, b;

    for (int y = startY; y < endY; y++) {
        for (int x = 0; x < WIDTH; x++) {

            int i = (y * WIDTH + x) * 3;

            calculatePixel(x, y, r, g, b, camera, scene);

            accumPixels[i + 0] += (float)r;
            accumPixels[i + 1] += (float)g;
            accumPixels[i + 2] += (float)b;

            pixels[i + 0] = (unsigned char)(accumPixels[i + 0] / (iterations + 1));
            pixels[i + 1] = (unsigned char)(accumPixels[i + 1] / (iterations + 1));
            pixels[i + 2] = (unsigned char)(accumPixels[i + 2] / (iterations + 1));
        }
    }
}

int main() {
    // std::ifstream test("config.json");
    // --- Read Configuration File ---
    // std::ifstream configFile("config.txt");
    // if (!configFile.is_open()) {
    //     std::cerr << "Failed to open config.txt" << std::endl;
    //     return 1;
    // }
    // std::unordered_map<std::string, std::string> config;
    // std::string key, value;
    // while (configFile >> key >> value) {
    //     config[key] = value;
    // }
    // configFile.close();
    std::ifstream file("config.json");
    if (!file.is_open()) {
        std::cerr << "Failed to open config.json" << std::endl;
        return 1;
    }
    json config;
    file >> config;
    if (!config.contains("WIDTH") || !config.contains("HEIGHT") || !config.contains("FOV")) {
        std::cerr << "Config file missing required fields!" << std::endl;
        return 1;
    }
    int width = config["WIDTH"];
    int height = config["HEIGHT"];
    float fov = config["FOV"];
    std::cout << width << std::endl;
    // TODO: Modify code to use config file

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "CPU Path Tracer", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    std::vector<unsigned char> pixels(WIDTH * HEIGHT * 3, 0);

    // --- Create texture ---
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, WIDTH, HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

    // --- Set up quad ---
    float verts[] = {-1, -1, 0, 0, 1, -1, 1, 0, 1, 1, 1, 1, -1, 1, 0, 1};
    unsigned int idx[] = {0,1,2, 2,3,0};
    GLuint VAO,VBO,EBO;
    glGenVertexArrays(1,&VAO);
    glGenBuffers(1,&VBO);
    glGenBuffers(1,&EBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER,VBO);
    glBufferData(GL_ARRAY_BUFFER,sizeof(verts),verts,GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(idx),idx,GL_STATIC_DRAW);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)(2*sizeof(float)));
    glEnableVertexAttribArray(1);

    GLuint program = createProgram();
    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "screenTex"), 0);

    // --- Create double PBOs ---
    GLuint pboIds[2];
    glGenBuffers(2, pboIds);
    for (int i=0;i<2;i++) {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pboIds[i]);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, WIDTH*HEIGHT*3, nullptr, GL_STREAM_DRAW);
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    int index = 0;      // current PBO
    int nextIndex = 1;  // previous PBO

    double lastTime = glfwGetTime();
    int frames = 0;
    float fps = 0.0f;
    
    std::vector<float> accumPixels(WIDTH * HEIGHT * 3, 0.0f);
    int iterations = 0;

    // --- Create The Scene And Add Shapes ---
    Scene scene;
    scene.materials.push_back({
        MaterialType::Lambertian,
        {1.0f, 0.2f, 0.2f},
        0.0f,
        1.0f,
        {0, 0, 0}
    });
    glm::vec3 vertex1 = {0,0,0};
    scene.vertices.push_back(vertex1);
    glm::vec3 vertex2 = {1,0,0};
    scene.vertices.push_back(vertex2);
    glm::vec3 vertex3 = {1,1,1};
    scene.vertices.push_back(vertex3);

    glm::vec3 normal = {0,0,1};
    Triangle triangle1 = {0,1,2, normal, 0};
    scene.triangles.push_back(triangle1);

    // --- Create camera ---
    Camera camera{
        glm::vec3{0,0,-10}, // Position
        glm::vec3{0,0,1}, // Forward
        glm::vec3{1,0,0}, // Right
        glm::vec3{0,1,0}, // Up
        90.0 // FOV
    };

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        frames++;
        if (now - lastTime >= 1.0) {
            fps = frames / (now - lastTime);
            frames = 0;
            lastTime = now;
        }

        unsigned char r, g, b;

        // --- Fill pixel buffer with multithreading ---
        int numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 4;  // fallback

        std::vector<std::thread> threads;
        threads.reserve(numThreads);

        int rowsPerThread = HEIGHT / numThreads;

        for (int t = 0; t < numThreads; t++) {
            int startY = t * rowsPerThread;
            int endY = (t == numThreads - 1) ? HEIGHT : startY + rowsPerThread;

            threads.emplace_back(
                renderChunk,
                startY,
                endY,
                std::ref(accumPixels),
                std::ref(pixels),
                iterations,
                camera,
                scene
            );
        }
        // Wait for all threads
        for (auto& th : threads) th.join();
        iterations++;

        // --- Draw FPS ---
        std::string fpsText = "FPS: " + std::to_string((int)fps);
        drawText(10,10,fpsText,pixels,WIDTH,HEIGHT);

        // --- Upload pixels using PBO ---
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pboIds[index]);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, WIDTH*HEIGHT*3, nullptr, GL_STREAM_DRAW); // orphan previous data
        void* ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
        if(ptr) {
            memcpy(ptr, pixels.data(), WIDTH*HEIGHT*3);
            glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
        }

        glBindTexture(GL_TEXTURE_2D, tex);
        glTexSubImage2D(GL_TEXTURE_2D,0,0,0,WIDTH,HEIGHT,GL_RGB,GL_UNSIGNED_BYTE,nullptr);

        // Swap PBOs
        std::swap(index,nextIndex);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);

        // --- Draw quad ---
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_INT,0);
        glfwSwapInterval(0);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteBuffers(2,pboIds);
    glfwTerminate();
}
