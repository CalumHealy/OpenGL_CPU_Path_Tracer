#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <string>

// Window size
const unsigned int WIDTH = 640;
const unsigned int HEIGHT = 480;

// Vertex Shader (positions + texture coords)
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

// Fragment Shader (sample texture)
const char* fragmentShaderSource = R"(

#version 330 core
out vec4 FragColor;
in vec2 TexCoord;

struct Sphere {
    vec4 posRad; // xyz = center, w = radius
    vec3 color;
};

uniform int sphereCount;
uniform Sphere spheres[16]; // max 16 spheres

// Simple ray-sphere intersection
bool intersectSphere(vec3 ro, vec3 rd, Sphere s, out float t, out vec3 normal) {
    vec3 oc = ro - s.posRad.xyz;
    float b = dot(oc, rd);
    float c = dot(oc, oc) - s.posRad.w * s.posRad.w;
    float h = b*b - c;
    if (h < 0.0) return false;
    h = sqrt(h);
    t = -b - h;
    if (t < 0.0) t = -b + h;
    if (t < 0.0) return false;

    vec3 hitPoint = ro + rd * t;
    normal = normalize(hitPoint - s.posRad.xyz);
    return true;
}

void main() {
    // Generate camera ray
    vec2 uv = TexCoord * 2.0 - 1.0;
    vec3 ro = vec3(0.0, 0.0, 0.0);   // ray origin (camera)
    vec3 rd = normalize(vec3(uv, -1.0)); // ray direction

    // Light setup
    vec3 lightPos = vec3(-2.0, 2.0, -2.0);
    vec3 lightColor = vec3(1.0);

    vec3 color = vec3(0.0);
    float closest = 1e9;

    for (int i = 0; i < sphereCount; i++) {
        float t;
        vec3 normal;
        if (intersectSphere(ro, rd, spheres[i], t, normal) && t < closest) {
            closest = t;

            vec3 hitPoint = ro + rd * t;
            vec3 lightDir = normalize(lightPos - hitPoint);

            // Lambertian diffuse shading
            float diff = max(dot(normal, lightDir), 0.0);
            vec3 diffuse = spheres[i].color * diff * lightColor;

            // Ambient term
            vec3 ambient = 0.1 * spheres[i].color;

            color = ambient + diffuse;
        }
    }

    FragColor = vec4(color, 1.0);
}


)";

// Utility: compile shader and check for errors
GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if(!success) {
        char info[512];
        glGetShaderInfoLog(shader, 512, nullptr, info);
        std::cerr << "Shader compilation error:\n" << info << std::endl;
    }
    return shader;
}

// Utility: create shader program
GLuint createShaderProgram() {
    GLuint vertex = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragment = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if(!success) {
        char info[512];
        glGetProgramInfoLog(program, 512, nullptr, info);
        std::cerr << "Shader linking error:\n" << info << std::endl;
    }

    glDeleteShader(vertex);
    glDeleteShader(fragment);

    return program;
}

int main() {
    // Initialize GLFW
    if(!glfwInit()) {
        std::cerr << "Failed to init GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "OpenGL Window", nullptr, nullptr);
    if(!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }

    // --- Create pixel buffer (RGB) ---
    std::vector<unsigned char> pixels(WIDTH * HEIGHT * 3, 0);
    for(int y = 0; y < HEIGHT; ++y) {
        for(int x = 0; x < WIDTH; ++x) {
            int idx = (y * WIDTH + x) * 3;
            pixels[idx + 0] = static_cast<unsigned char>((float)x / WIDTH * 255);   // Red
            pixels[idx + 1] = static_cast<unsigned char>((float)y / HEIGHT * 255);  // Green
            pixels[idx + 2] = 128;                                                  // Blue
        }
    }

    // --- Create OpenGL texture ---
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, WIDTH, HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // --- Fullscreen quad data ---
    float vertices[] = {
        // positions   // tex coords
        -1.0f, -1.0f,  0.0f, 0.0f, // bottom-left
         1.0f, -1.0f,  1.0f, 0.0f, // bottom-right
         1.0f,  1.0f,  1.0f, 1.0f, // top-right
        -1.0f,  1.0f,  0.0f, 1.0f  // top-left
    };
    unsigned int indices[] = {
        0, 1, 2,  // first triangle
        2, 3, 0   // second triangle
    };

    GLuint VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // texcoord attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    GLuint shaderProgram = createShaderProgram();
    glUseProgram(shaderProgram);
    glUniform1i(glGetUniformLocation(shaderProgram, "screenTexture"), 0);

    // Shapes
    struct Sphere {
        float x, y, z, radius;
        float r, g, b;
    };

    std::vector<Sphere> spheres = {
        {0.0f, 0.0f, -3.0f, 1.0f, 1.0f, 0.0f, 0.0f}, // red sphere
        {2.0f, 0.0f, -4.0f, 1.0f, 0.0f, 1.0f, 0.0f}, // green sphere
        {-4.0f, -1.0f, -6.0f, 0.5f, 1.0f, 1.0f, 0.0f} // yellow sphere
    };

    // Send spheres to shader
    GLuint sphereLoc = glGetUniformLocation(shaderProgram, "spheres");
    glUniform1i(glGetUniformLocation(shaderProgram, "sphereCount"), spheres.size());

    // Upload each sphere individually (not efficient for large scenes)
    for (int i = 0; i < spheres.size(); i++) {
        std::string base = "spheres[" + std::to_string(i) + "]";
        glUniform4f(glGetUniformLocation(shaderProgram, (base + ".posRad").c_str()),
            spheres[i].x, spheres[i].y, spheres[i].z, spheres[i].radius);
        glUniform3f(glGetUniformLocation(shaderProgram, (base + ".color").c_str()),
            spheres[i].r, spheres[i].g, spheres[i].b);
    }

    // --- Render loop ---
    while(!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // --- Cleanup ---
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteTextures(1, &texture);
    glDeleteProgram(shaderProgram);

    glfwTerminate();
    return 0;
}
