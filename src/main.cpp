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

// const unsigned int WIDTH = 1920;
// const unsigned int HEIGHT = 1080;

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

struct Scene {
    std::vector<glm::vec3> vertices;
    std::vector<Triangle> triangles;
    std::vector<Material> materials;
};

struct Ray {
    glm::vec3 origin;
    glm::vec3 direction; // Normalized
};

struct RenderState {
    int width;
    int height;
    Camera camera;
    const Scene* scene;
    int iterations;
    float invWidth;
    float invHeight;
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

void calculatePixel(int x, int y, unsigned char& r, unsigned char& g, unsigned char& b, const RenderState& state) {
    // X is number of pixel across length of screen (0 - WIDTH)
    // Y is number of pixel down screen (0 - HEIGHT)
    // r, g, and b are references to the values for the pixel colour
    // Ray direction and position can be derived from camera values
    // r = rand8();
    // g = rand8();
    // b = rand8();
    // --- Empty triangles list protection ---
    if (state.scene->triangles.empty()) {
        r = 0;
        g = 0;
        b = 0;
        return;
    }
    // --- Convert world space to screen space [?] ---
    float minX = std::min(
        state.scene->vertices[state.scene->triangles[0].v0][0],
        std::min(
            state.scene->vertices[state.scene->triangles[0].v1][0],
            state.scene->vertices[state.scene->triangles[0].v2][0]
        )
    );
    float maxX = std::max(
        state.scene->vertices[state.scene->triangles[0].v0][0],
        std::max(
            state.scene->vertices[state.scene->triangles[0].v1][0],
            state.scene->vertices[state.scene->triangles[0].v2][0]
        )
    );
    float minY = std::min(
        state.scene->vertices[state.scene->triangles[0].v0][1],
        std::min(
            state.scene->vertices[state.scene->triangles[0].v1][1],
            state.scene->vertices[state.scene->triangles[0].v2][1]
        )
    );
    float maxY = std::max(
        state.scene->vertices[state.scene->triangles[0].v0][1],
        std::max(
            state.scene->vertices[state.scene->triangles[0].v1][1],
            state.scene->vertices[state.scene->triangles[0].v2][1]
        )
    );
    float triangleWidth = maxX - minX;
    float triangleHeight = maxY - minY;
    float scaleX = state.width / triangleWidth;
    float scaleY = state.height / triangleHeight;
    float scale = std::min(scaleX, scaleY);
    std::vector<glm::vec2> screenVertices;
    for (const glm::vec3& vertex : {
        state.scene->vertices[state.scene->triangles[0].v0],
        state.scene->vertices[state.scene->triangles[0].v1],
        state.scene->vertices[state.scene->triangles[0].v2]
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
    if (std::abs(denom) < 1e-6f) {
        r = 0;
        g = 0;
        b = 0;
        return;
    }
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
    const RenderState& state
) {
    const int width = state.width;
    float inv = 1.0f / (state.iterations + 1);

    for (int y = startY; y < endY; y++) {
        for (int x = 0; x < width; x++) {
            int i = (y * width + x) * 3;
            unsigned char r, g, b;

            calculatePixel(x, y, r, g, b, state);

            accumPixels[i + 0] += (float)r;
            accumPixels[i + 1] += (float)g;
            accumPixels[i + 2] += (float)b;

            for (int c = 0; c < 3; c++) {
                float val = accumPixels[i + c] * inv;
                val = std::min(std::max(val, 0.0f), 255.0f);
                // pixels[i + c] = static_cast<unsigned char>(val);
                pixels.at(i + c) = static_cast<unsigned char>(val);
            }  

        }
    }
}

int main() {
    // --- Read Configuration File ---
    std::ifstream file("config.json");
    if (!file.is_open()) {
        std::cerr << "Failed to open config.json" << std::endl;
        return 1;
    }
    json config;
    file >> config;
    // TODO: Implement better config error handling
    RenderState state;
    state.width = config["WIDTH"];
    state.height = config["HEIGHT"];
    state.iterations = 0;
    state.invWidth = 1.0f / state.width;
    state.invHeight = 1.0f / state.height;
    // if (!config.contains("WIDTH")) {
    //     std::cout << "Width parameter missing from config file. Defaulting to 1920." << std::endl;
    //     state.width = 1920;
    // } else if (!config["WIDTH"].is_number_integer() || config["WIDTH"].get<std::string>().empty()) {
    //     std::cout << "WIDTH.type(): " << config["WIDTH"].type() << std::endl;
    // } else {
    //     state.width = config["WIDTH"];
    // }

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(state.width, state.height, "CPU Path Tracer", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    std::vector<unsigned char> pixels(state.width * state.height * 3, 0);

    // --- Create texture ---
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, state.width, state.height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

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
        glBufferData(GL_PIXEL_UNPACK_BUFFER, state.width*state.height*3, nullptr, GL_STREAM_DRAW);
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    int index = 0;      // current PBO
    int nextIndex = 1;  // previous PBO

    double lastTime = glfwGetTime();
    int frames = 0;
    float fps = 0.0f;
    
    std::vector<float> accumPixels(state.width * state.height * 3, 0.0f);

    // --- Create The Scene And Add Shapes ---
    Scene scene;
    state.scene = &scene;
    // scene.materials.push_back({
    //     MaterialType::Lambertian,
    //     {1.0f, 0.2f, 0.2f},
    //     0.0f,
    //     1.0f,
    //     {0, 0, 0}
    // });
    // glm::vec3 vertex1 = {0,0,0};
    // scene.vertices.push_back(vertex1);
    // glm::vec3 vertex2 = {1,0,0};
    // scene.vertices.push_back(vertex2);
    // glm::vec3 vertex3 = {1,1,1};
    // scene.vertices.push_back(vertex3);

    for (const auto& v : config["SCENE"]["VERTICES"]) {
        glm::vec3 vertex(
            v[0].get<float>(), 
            v[1].get<float>(), 
            v[2].get<float>());
        scene.vertices.push_back(vertex);
    }

    // glm::vec3 normal = {0,0,1};
    // Triangle triangle1 = {0,1,2, normal, 0};
    // scene.triangles.push_back(triangle1);

    // --- Create camera ---
    Camera camera{
        glm::vec3{0,0,-10}, // Position
        glm::vec3{0,0,1}, // Forward
        glm::vec3{1,0,0}, // Right
        glm::vec3{0,1,0}, // Up
        90.0 // FOV
    };
    state.camera = camera;

    // --- Debug ---
    std::cout << "Hello" << std::endl;
    if (!scene.vertices.empty()) {
        // glm::vec3 v = scene.vertices[0];
        // std::cout << v.x << ", " << v.y << ", " << v.z << std::endl;
        for (const auto& v : scene.vertices) {
            std::cout << v.x << ", " << v.y << ", " << v.z << std::endl;
        }
    } else {
        std::cout << "The vertices vector is empty" << std::endl;
    }

    std::cout << "Starting application loop" << std::endl;
    while (!glfwWindowShouldClose(window)) {
        std::cout << "Calculating FPS" << std::endl;
        double now = glfwGetTime();
        frames++;
        if (now - lastTime >= 1.0) {
            fps = frames / (now - lastTime);
            frames = 0;
            lastTime = now;
        }

        unsigned char r, g, b;

        // --- Fill pixel buffer with multithreading ---
        std::cout << "Handling multithreading" << std::endl;
        int numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 4;  // fallback

        std::vector<std::thread> threads;
        threads.reserve(numThreads);

        int rowsPerThread = state.height / numThreads;

        for (int t = 0; t < numThreads; t++) {
            std::cout << "Creating thread" << std::endl;
            int startY = t * rowsPerThread;
            int endY = (t == numThreads - 1) ? state.height : startY + rowsPerThread;

            // threads.emplace_back(
            //     renderChunk,
            //     startY,
            //     endY,
            //     std::ref(accumPixels),
            //     std::ref(pixels),
            //     std::cref(state)
            // );
            threads.emplace_back(
                [startY, endY, &accumPixels, &pixels, &state]() {
                    try {
                        std::cout << "Thread started: " << startY << " -> " << endY << std::endl;

                        renderChunk(
                            startY,
                            endY,
                            accumPixels,
                            pixels,
                            state
                        );
                        std::cout << "Thread finished" << std::endl;
                    }
                    catch (const std::exception& e) {
                        std::cout << "Thread exception: " << e.what() << std::endl;
                    }
                    catch (...) {
                        std::cout << "Unknown thread exception" << std::endl;
                    }
                }
            );
        }
        // Wait for all threads
        std::cout << "Waiting for threads" << std::endl;
        for (auto& th : threads) {
            if (th.joinable()) {
                th.join();
            }
        }
        state.iterations++;

        // --- Draw FPS ---
        std::cout << "Drawing FPS" << std::endl;
        std::string fpsText = "FPS: " + std::to_string((int)fps) + "\nHello";
        drawText(10,10,fpsText,pixels,state.width,state.height);

        // --- Upload pixels using PBO ---
        std::cout << "Upload pixels using PBO" << std::endl;
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pboIds[index]);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, state.width*state.height*3, nullptr, GL_STREAM_DRAW); // orphan previous data
        void* ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
        if(ptr) {
            memcpy(ptr, pixels.data(), state.width*state.height*3);
            glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
        }

        glBindTexture(GL_TEXTURE_2D, tex);
        glTexSubImage2D(GL_TEXTURE_2D,0,0,0,state.width,state.height,GL_RGB,GL_UNSIGNED_BYTE,nullptr);

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
