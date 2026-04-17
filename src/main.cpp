#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>

std::vector<glm::vec3> genVertices(int step) {
    int triangleCount = step - 2;
    float angleStep = 360.0f / step;
    std::vector<glm::vec3> temp;
    std::vector<glm::vec3> vertices;

    for(int i = 0; i < step; i++) {
        float currentAngle = i * angleStep;
        float x = cos(glm::radians(currentAngle));
        float y = sin(glm::radians(currentAngle));
        float z = 0.0f;
        temp.push_back(glm::vec3(x, y, z));
    }

    for(int i = 0; i < triangleCount; i++) {
        vertices.push_back(temp[0]);
        vertices.push_back(temp[i + 1]);
        vertices.push_back(temp[i + 2]);
    }

    return vertices;
}

class Ball {
private: 
    unsigned int lineVAO, lineVBO;


public:
    glm::vec2 pos, vel, acl;
    float radius;
    float scale = 1.0f / 20.0f;
    float energy;
    float previousTime = 0;

    Ball(glm::vec2 pos, glm::vec2 vel) {
        this->pos = pos;
        this->vel = vel;
        radius = 0.05f;
        acl = glm::vec2(0.0f, -9.8f * scale);
    }

    void genBuffers() {
        glGenVertexArrays(1, &lineVAO);
        glGenBuffers(1, &lineVBO);
    }

    void eulerUpdate() {
        float dt = deltaTime();

        pos += vel * dt;
        vel += acl * dt;
    }

    void verletUpdate() {
        float dt = deltaTime();

        pos += vel * dt + 0.5f * acl * dt * dt;
        vel += acl * dt;
    }

    float deltaTime() {
        float currentTime = glfwGetTime();
        float dt = currentTime - previousTime;
        previousTime = currentTime;

        return dt;
    }

    void rk4Update() {
        float dt = deltaTime();

        glm::vec4 s(pos.x, pos.y, vel.x, vel.y);

        glm::vec4 k1 = f(s);
        glm::vec4 k2 = f(s + 0.5f * dt * k1);
        glm::vec4 k3 = f(s + 0.5f * dt * k2);
        glm::vec4 k4 = f(s + dt * k3);

        s += dt * (k1 + 2.0f * k2 + 2.0f * k3 + k4) / 6.0f;

        pos = glm::vec2(s.x, s.y);
        vel = glm::vec2(s.z, s.w);
    }

    void updateEnergy() {
        float speed = glm::length(vel) / scale;
        float height = 20.0f + pos.y / scale;

        energy = 0.5f * speed * speed + 9.80f * height;
    }

    glm::vec4 f(const glm::vec4& s) {
        float vx = s.z;
        float vy = s.w;

        float ax = acl.x;
        float ay = acl.y;

        return glm::vec4(vx, vy, ax, ay);
    }
    
    void drawHeightLine(unsigned int &shaderProgram, glm::vec4 color) {
        float y = pos.y + radius;
        float vertices[] {
            -1.0f, y, 0.0f,
            1.0f, y, 0.0f
        };

        glBindVertexArray(lineVAO);

        glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glUseProgram(shaderProgram);

        glUniform4f(glGetUniformLocation(shaderProgram, "inputColor"), color.x, color.y, color.z, color.w);
        
        glm::mat4 transform = glm::mat4(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "transform"), 1, GL_FALSE, glm::value_ptr(transform));

        glDrawArrays(GL_LINES, 0, 2);
    }

    void check_collision() {
        if(pos.y < -1.0f + radius) {
            vel.y = -vel.y;
        }
    }
};

struct Engine {
    GLFWwindow* window;
    unsigned int shaderProgram, VAO;
    int vertSize;

    Ball eulerBall, verletBall, rk4Ball; 
    std::vector<Ball> balls;

    Engine() : eulerBall(glm::vec2(-0.5f, 0.0f), glm::vec2(0.0f, 0.0f)), verletBall(glm::vec2(0.0f, 0.0f), glm::vec2(0.0f, 0.0f)), rk4Ball(glm::vec2(0.5f, 0.0f), glm::vec2(0.0f, 0.0f)) {
        if(!glfwInit()) {
            std::cout << "Failed to initialize GLFW\n";
            exit(EXIT_FAILURE);
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        window = glfwCreateWindow(1000, 1000, "Bouncing Balls", NULL, NULL);
        if(!window) {
            std::cout << "Failed to create GLFW window\n";
            glfwTerminate();
            exit(EXIT_FAILURE);
        }
        glfwMakeContextCurrent(window);

        if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            std::cout << "Failed to initialize GLAD\n";
            glfwTerminate();
            exit(EXIT_FAILURE);
        }
        glViewport(0, 0, 1000, 1000);

        balls.push_back(eulerBall);
        balls.push_back(verletBall);
        balls.push_back(rk4Ball);

        for(auto& ball : balls) {
            ball.genBuffers();
        }
    }

    void createShaderProgram() { 
        const char* vertexShaderSource = R"(#version 330 core
        layout (location = 0) in vec3 aPos;
        uniform mat4 transform;
        void main() {
            gl_Position = transform * vec4(aPos.x, aPos.y, aPos.z, 1.0);
        })";

        const char* fragmentShaderSource = R"(#version 330 core
        out vec4 FragColor;
        uniform vec4 inputColor;
        void main() {
            FragColor = inputColor;
        })";

        unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
        glCompileShader(vertexShader);

        unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
        glCompileShader(fragmentShader);

        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }

    void createVAO() {
        std::vector<glm::vec3> vertices = genVertices(30);
        vertSize = vertices.size();

        unsigned int VBO;
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * vertSize, vertices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
    }

    void loop() {
        glClearColor(0.25f, 0.54f, 0.82f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glm::vec4 colors[] = {glm::vec4(0.90f, 0.37f, 0.21f, 1.0f), glm::vec4(0.27f, 0.72f, 0.37f, 1.0f), glm::vec4(0.85f, 0.67f, 0.17f, 1.0f)};
        int i = 0;

        for(auto& ball : balls) {
            glm::vec4 color = colors[i];

            ball.drawHeightLine(shaderProgram, color);

            glUseProgram(shaderProgram);
            glBindVertexArray(VAO);

            ball.check_collision();

            if(i == 0) {
                ball.eulerUpdate();
            }
            else if(i == 1) {
                ball.verletUpdate();
            }
            else if(i == 2) {
                ball.rk4Update();
            }

            ball.updateEnergy();

            i++;

            glUniform4f(glGetUniformLocation(shaderProgram, "inputColor"), color.x, color.y, color.z, color.w);
            glm::mat4 transform = glm::mat4(1.0f);
            transform = glm::translate(transform, glm::vec3(ball.pos.x, ball.pos.y, 0.0f));
            transform = glm::scale(transform, glm::vec3(ball.radius, ball.radius, 0.5f));
            glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "transform"), 1, GL_FALSE, glm::value_ptr(transform));

            glDrawArrays(GL_TRIANGLES, 0, vertSize);

            std::cout << "Euler Energy: " << balls[0].energy << ", " << "Verlet Energy: " << balls[1].energy << ", " << "RK4 Energy: " << balls[2].energy << ", " << "Real Time Elapsed: " << glfwGetTime() << " s" << '\n';
        }

        glfwPollEvents();
        glfwSwapBuffers(window); 
    }
};


int main() {
    Engine engine;

    engine.createShaderProgram();
    engine.createVAO();

    while(!glfwWindowShouldClose(engine.window)) {
        engine.loop();
    }

    return 0;
}