#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include "font8x8_basic.h"

const unsigned int WIDTH = 640;
const unsigned int HEIGHT = 480;

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

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "CPU Pixel Buffer with FPS", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    std::vector<unsigned char> pixels(WIDTH * HEIGHT * 3, 0);

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, WIDTH, HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    float verts[] = {
        -1, -1, 0, 0,
         1, -1, 1, 0,
         1,  1, 1, 1,
        -1,  1, 0, 1
    };
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

    double lastTime = glfwGetTime();
    int frames = 0;
    float fps = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        // --- FPS counter ---
        double now = glfwGetTime();
        frames++;
        if (now - lastTime >= 1.0) {
            fps = frames / (now - lastTime);
            frames = 0;
            lastTime = now;
        }

        // --- Draw simple gradient ---
        for (int y = 0; y < HEIGHT; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                int i = (y * WIDTH + x) * 3;
                // pixels[i+0] = (unsigned char)((x + (int)(now*50)) % 256);
                // pixels[i+1] = (unsigned char)((y + (int)(now*20)) % 256);
                // pixels[i+2] = 100;
                pixels[i+0] = 150;
                pixels[i+1] = 255;
                pixels[i+2] = 0;
            }
        }

        // --- Draw FPS text into pixel buffer ---
        std::string fpsText = "FPS: " + std::to_string((int)fps);
        drawText(10, 10, fpsText, pixels, WIDTH, HEIGHT);

        // --- Upload and display ---
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, WIDTH, HEIGHT,
                        GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glfwSwapInterval(1); // Enable/Disable VSync (0: off, 1: on)
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwTerminate();
}
