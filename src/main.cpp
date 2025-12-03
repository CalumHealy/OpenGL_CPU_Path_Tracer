#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <random>
#include <vector>
#include <array>
#include <string>
#include "font8x8_basic.h"

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

uint32_t seed = 314159265;
inline uint8_t rand8() {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return (uint8_t)(seed & 0xFF);
}

void calculatePixel(int x, int y, unsigned char& r, unsigned char& g, unsigned char& b) {
    r = rand8();
    g = rand8();
    b = rand8();
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "CPU Pixel Buffer with PBOs", nullptr, nullptr);
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

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        frames++;
        if (now - lastTime >= 1.0) {
            fps = frames / (now - lastTime);
            frames = 0;
            lastTime = now;
        }

        // std::array<int,3> currentPixel = {255,255,255};
        // std::array<int,3> prevPixel = {255,255,255};
        unsigned char r, g, b;

        // --- Fill pixel buffer ---
        for (int y=0;y<HEIGHT;y++) {
            for (int x=0;x<WIDTH;x++) {
                int i = (y*WIDTH+x)*3;
                calculatePixel(x, y, r, g, b);
                // Accumulate in float buffer
                accumPixels[i + 0] += (float)r;
                accumPixels[i + 1] += (float)g;
                accumPixels[i + 2] += (float)b;
                // Compute average to display
                pixels[i + 0] = (unsigned char)(accumPixels[i + 0] / (iterations + 1));
                pixels[i + 1] = (unsigned char)(accumPixels[i + 1] / (iterations + 1));
                pixels[i + 2] = (unsigned char)(accumPixels[i + 2] / (iterations + 1));
            }
        }
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
