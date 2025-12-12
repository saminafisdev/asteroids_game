#include <iostream>
#include <cmath>      // For fmod
#include <vector>     // For std::vector
#include <cstdlib>    // For std::rand, std::srand, RAND_MAX
#include <ctime>      // For std::time (to seed rand)
#include <algorithm>  // For glm::max

// 1. INCLUDE GLAD FIRST!
#include <glad/glad.h>
// 2. THEN INCLUDE GLFW
#include <GLFW/glfw3.h>

// GLM includes for vector math and transformations
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp> // For glm::pi

// --- Function Prototypes ---
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);
bool checkCollision(glm::vec2 pos1, float rad1, glm::vec2 pos2, float rad2);
void generateStars();
void spawnNewAsteroid();

// --- Settings ---
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// --- Physics Constants ---
const float THRUST_SPEED = 1.0f;
const float ROTATION_SPEED = 2.0f;
const float FRICTION = 0.995f;

// --- Bullet Constants ---
const float BULLET_SPEED = 2.5f;
const float FIRE_RATE = 0.2f;

// --- Spawning Constants ---
const float INITIAL_SPAWN_RATE = 5.0f; // Seconds between spawns at the start
const float MIN_SPAWN_RATE = 1.0f;     // Minimum seconds between spawns
const int MAX_ASTEROIDS = 10;          // Cap the total number of asteroids for performance

// --- Star Constants ---
const int NUM_STARS = 150; // Total number of stars

// --- Global Spawning State ---
float asteroidSpawnTimer = 0.0f;
float currentSpawnRate = INITIAL_SPAWN_RATE;

// --- Global Game State ---
float deltaTime = 0.0f;
float lastFrame = 0.0f;
bool isGameOver = false;
float bulletCooldown = 0.0f;
bool isThrusting = false; // Thrust state flag

// --- GLOBAL GRAPHICS HANDLES (All graphics object IDs are global now) ---
unsigned int shipVAO, shipVBO;
unsigned int bulletVAO, bulletVBO;
unsigned int fireVAO, fireVBO;
unsigned int starVAO, starVBO;
unsigned int gradientVAO, gradientVBO;

// --- SHIP/GAME STATE STRUCT ---
struct Ship {
    glm::vec2 position = glm::vec2(0.0f, 0.0f);
    glm::vec2 velocity = glm::vec2(0.0f, 0.0f);
    float rotation = 0.0f;
    float scale = 0.05f;
    float radius = 0.05f;
};
Ship player;

// --- STAR STRUCT ---
struct Star {
    glm::vec2 position;
    float size;
};
std::vector<Star> stars;

// --- ASTEROID STRUCT ---
struct Asteroid {
    glm::vec2 position;
    glm::vec2 velocity;
    float rotation;
    float rotationSpeed;
    float scale;
    float radius;
    unsigned int VAO, VBO;
    int vertexCount;
};
std::vector<Asteroid> asteroids;

// --- BULLET STRUCT (FIXED: Initialized variables to suppress warnings) ---
struct Bullet {
    glm::vec2 position = glm::vec2(0.0f, 0.0f);
    glm::vec2 velocity = glm::vec2(0.0f, 0.0f);
    float rotation = 0.0f;
    float scale = 0.01f;
    float radius = 0.01f;
    float lifetime = 1.0f;
};
std::vector<Bullet> bullets;

// --- SHADER CODE (FIXED: Consolidated and modified for gradient and vector lines) ---

// Vertex Shader: Passes position and color
const char* vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec2 aPos;
    layout (location = 1) in vec3 aColor; // Color attribute for gradient
    
    uniform mat4 transform;
    out vec3 vColor; // Output color to fragment shader
    
    void main()
    {
        gl_Position = transform * vec4(aPos.x, aPos.y, 0.0, 1.0);
        vColor = aColor; // Pass the color through
    }
)";

// Fragment Shader: Uses uniform lineColor for vector objects, or interpolated vColor for background
const char* fragmentShaderSource = R"(
    #version 330 core
    out vec4 FragColor;
    
    uniform vec3 lineColor; // Used for ship, bullets, asteroids, stars
    in vec3 vColor; // Input color from the vertex shader (interpolated for gradient)
    
    void main()
    {
        // If lineColor is near-black (0.0), we assume it's the background quad 
        // and use the interpolated vertex color (vColor). Otherwise, use the uniform lineColor.
        if (length(lineColor) < 0.01) { 
            FragColor = vec4(vColor, 1.0f);
        } else {
            FragColor = vec4(lineColor, 1.0f);
        }
    }
)";

// --- ASTEROID VERTEX GENERATOR ---
std::vector<float> generateAsteroidVertices(int segments, float radius) {
    std::vector<float> vertices;
    for (int i = 0; i < segments; ++i) {
        float angle = (float)i / (float)segments * 2.0f * glm::pi<float>();
        float currentRadius = radius * (1.0f + ((float)std::rand() / RAND_MAX - 0.5f) * 0.4f);
        float x = currentRadius * cos(angle);
        float y = currentRadius * sin(angle);
        vertices.push_back(x);
        vertices.push_back(y);
    }
    return vertices;
}

// --- STAR GENERATION FUNCTION ---
void generateStars() {
    for (int i = 0; i < NUM_STARS; ++i) {
        Star s;
        s.position.x = ((float)std::rand() / RAND_MAX * 2.0f) - 1.0f;
        s.position.y = ((float)std::rand() / RAND_MAX * 2.0f) - 1.0f;
        s.size = 1.0f + ((float)std::rand() / RAND_MAX * 3.0f);
        stars.push_back(s);
    }
}

// --- ASTEROID SPAWNING FUNCTION ---
void spawnNewAsteroid()
{
    float baseScale = 0.15f;
    float baseRadius = 1.0f;

    // 2. Determine random off-screen position
    glm::vec2 spawnPos;
    float side = (float)std::rand() / RAND_MAX * 4.0f; // 0=Top, 1=Bottom, 2=Left, 3=Right

    if (side < 1.0f) { // Spawn Top
        spawnPos = glm::vec2(((float)std::rand() / RAND_MAX * 2.0f) - 1.0f, 1.1f);
    }
    else if (side < 2.0f) { // Spawn Bottom
        spawnPos = glm::vec2(((float)std::rand() / RAND_MAX * 2.0f) - 1.0f, -1.1f);
    }
    else if (side < 3.0f) { // Spawn Left
        spawnPos = glm::vec2(-1.1f, ((float)std::rand() / RAND_MAX * 2.0f) - 1.0f);
    }
    else { // Spawn Right
        spawnPos = glm::vec2(1.1f, ((float)std::rand() / RAND_MAX * 2.0f) - 1.0f);
    }

    // 3. Determine random velocity aiming generally towards the center (0, 0)
    glm::vec2 target = glm::vec2(0.0f, 0.0f);
    glm::vec2 direction = glm::normalize(target - spawnPos);

    float scatter = 0.2f;
    direction.x += ((float)std::rand() / RAND_MAX - 0.5f) * scatter;
    direction.y += ((float)std::rand() / RAND_MAX - 0.5f) * scatter;
    direction = glm::normalize(direction);

    float speed = 0.1f + ((float)std::rand() / RAND_MAX * 0.2f);
    glm::vec2 spawnVelocity = direction * speed;


    // 4. Create and Initialize New Asteroid Struct
    Asteroid newRock;
    newRock.position = spawnPos;
    newRock.velocity = spawnVelocity;
    newRock.rotation = 0.0f;
    newRock.rotationSpeed = 0.3f + ((float)std::rand() / RAND_MAX * 0.5f);
    newRock.scale = baseScale;
    newRock.radius = newRock.scale * baseRadius;

    // 5. Generate Graphics 
    std::vector<float> rockVertices = generateAsteroidVertices(16, baseRadius);
    newRock.vertexCount = rockVertices.size() / 2;

    glGenVertexArrays(1, &newRock.VAO);
    glGenBuffers(1, &newRock.VBO);

    glBindVertexArray(newRock.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, newRock.VBO);
    glBufferData(GL_ARRAY_BUFFER, rockVertices.size() * sizeof(float), rockVertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // 6. Add to the active list
    asteroids.push_back(newRock);
}


int main()
{
    // FIX: Explicitly cast time to unsigned int to suppress warning
    std::srand(static_cast<unsigned int>(std::time(0)));

    // --- 1. GLFW/GLAD Initialization ---
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Asteroids Clone", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // --- 2. Shader Compilation ---
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // --- 3. Graphics Setup (VAOs/VBOs) ---
    glUseProgram(shaderProgram); // Use the program early to get uniform locations

    // A. Setup Gradient Quad VAO/VBO (Background)
    float gradientVertices[] = {
        // Pos X, Y          // Color R, G, B (from #2c5364 down to #0f2027)
        -1.0f,  1.0f,        0.173f, 0.325f, 0.392f, // Top-Left (Lighter)
         1.0f,  1.0f,        0.125f, 0.227f, 0.263f, // Top-Right (Mid)
        -1.0f, -1.0f,        0.059f, 0.125f, 0.153f, // Bottom-Left (Darkest)
         1.0f, -1.0f,        0.059f, 0.125f, 0.153f  // Bottom-Right (Darkest)
    };

    glGenVertexArrays(1, &gradientVAO);
    glGenBuffers(1, &gradientVBO);

    glBindVertexArray(gradientVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gradientVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(gradientVertices), gradientVertices, GL_STATIC_DRAW);

    // Position attribute (location 0)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Color attribute (location 1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    // B. Ship Setup 
    float shipVertices[] = { 0.0f,  1.0f, -1.0f, -1.0f, 1.0f,  -1.0f };
    glGenVertexArrays(1, &shipVAO);
    glGenBuffers(1, &shipVBO);
    glBindVertexArray(shipVAO);
    glBindBuffer(GL_ARRAY_BUFFER, shipVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(shipVertices), shipVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // C. Bullet Setup
    float bulletVertices[] = { 0.0f, 0.0f };
    glGenVertexArrays(1, &bulletVAO);
    glGenBuffers(1, &bulletVBO);
    glBindVertexArray(bulletVAO);
    glBindBuffer(GL_ARRAY_BUFFER, bulletVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(bulletVertices), bulletVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // D. Setup Stars 
    generateStars();
    float starVertex[] = { 0.0f, 0.0f };

    glGenVertexArrays(1, &starVAO);
    glGenBuffers(1, &starVBO);

    glBindVertexArray(starVAO);
    glBindBuffer(GL_ARRAY_BUFFER, starVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(starVertex), starVertex, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // E. Setup Thrust Fire VAO/VBO
    float fireVertices[] = {
        -0.5f, -1.0f,  // Bottom left point
         0.5f, -1.0f,  // Bottom right point
         0.0f,  0.0f   // Center point (where the flame attaches to the ship)
    };

    glGenVertexArrays(1, &fireVAO);
    glGenBuffers(1, &fireVBO);

    glBindVertexArray(fireVAO);
    glBindBuffer(GL_ARRAY_BUFFER, fireVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(fireVertices), fireVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // Get uniform locations once
    unsigned int transformLoc = glGetUniformLocation(shaderProgram, "transform");
    unsigned int colorLoc = glGetUniformLocation(shaderProgram, "lineColor");


    // --- 4. Render/Game Loop ---
    while (!glfwWindowShouldClose(window))
    {
        // Calculate Delta Time and update cooldown timer
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        bulletCooldown -= deltaTime;

        // --- Input Handling ---
        processInput(window);

        // --- Physics and Collision Update (Only if the game is NOT over) ---
        if (!isGameOver)
        {
            // NEW: Spawning Logic
            asteroidSpawnTimer -= deltaTime;

            if (asteroidSpawnTimer <= 0.0f && asteroids.size() < MAX_ASTEROIDS) {
                spawnNewAsteroid();

                // Reset timer and slightly decrease spawn rate for gradual difficulty increase
                currentSpawnRate = glm::max(MIN_SPAWN_RATE, currentSpawnRate - 0.1f);
                asteroidSpawnTimer = currentSpawnRate;
            }

            // --- Player Physics Update ---
            player.velocity *= FRICTION;
            player.position += player.velocity * deltaTime;
            if (player.position.x > 1.0f) player.position.x = -1.0f;
            else if (player.position.x < -1.0f) player.position.x = 1.0f;
            if (player.position.y > 1.0f) player.position.y = -1.0f;
            else if (player.position.y < -1.0f) player.position.y = 1.0f;

            // --- Asteroid Physics Update ---
            for (auto& asteroid : asteroids) {
                asteroid.position += asteroid.velocity * deltaTime;
                asteroid.rotation += asteroid.rotationSpeed * deltaTime;
                if (asteroid.position.x > 1.0f) asteroid.position.x = -1.0f;
                else if (asteroid.position.x < -1.0f) asteroid.position.x = 1.0f;
                if (asteroid.position.y > 1.0f) asteroid.position.y = -1.0f;
                else if (asteroid.position.y < -1.0f) asteroid.position.y = 1.0f;
            }

            // --- Bullet Physics Update (FIXED: size_t used) ---
            for (size_t i = 0; i < bullets.size(); /* no increment here */) {
                bullets[i].position += bullets[i].velocity * deltaTime;
                bullets[i].lifetime -= deltaTime;

                if (bullets[i].lifetime <= 0.0f ||
                    abs(bullets[i].position.x) > 1.5f || abs(bullets[i].position.y) > 1.5f) {
                    bullets.erase(bullets.begin() + i);
                }
                else {
                    ++i;
                }
            }

            // --- Ship-Asteroid Collision Check ---
            for (const auto& asteroid : asteroids) {
                if (checkCollision(player.position, player.radius, asteroid.position, asteroid.radius)) {
                    std::cout << "COLLISION! GAME OVER." << std::endl;
                    isGameOver = true;
                    break;
                }
            }

            // --- Bullet-Asteroid Collision Check (FIXED: size_t backward loop corrected) ---
            for (size_t i = asteroids.size(); i > 0; --i) {
                size_t index = i - 1; // Use this index to access the vector

                bool asteroidHit = false;

                for (size_t j = 0; j < bullets.size(); /* no increment here */) {
                    if (checkCollision(asteroids[index].position, asteroids[index].radius, bullets[j].position, bullets[j].radius)) {
                        bullets.erase(bullets.begin() + j);
                        asteroidHit = true;
                        break;
                    }
                    else {
                        ++j;
                    }
                }

                if (asteroidHit) {
                    glDeleteVertexArrays(1, &asteroids[index].VAO);
                    glDeleteBuffers(1, &asteroids[index].VBO);
                    asteroids.erase(asteroids.begin() + index);
                }
            }
        } // End of isGameOver check

        // --- Rendering Commands ---
        // Set clear color to the darkest base color of the gradient
        glClearColor(0.059f, 0.125f, 0.153f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shaderProgram);

        // NEW: Draw the Gradient Background Quad
        // Set lineColor to near-black (0.0), triggering the shader to use vColor (gradient).
        glUniform3f(colorLoc, 0.0f, 0.0f, 0.0f);
        glBindVertexArray(gradientVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // --- Drawing the Stars (BACKGROUND) ---
        glUniform3f(colorLoc, 0.9f, 0.9f, 0.9f); // Bright White/Light Grey color
        glBindVertexArray(starVAO);

        for (const auto& star : stars) {
            glm::mat4 starModel = glm::mat4(1.0f);
            starModel = glm::translate(starModel, glm::vec3(star.position, 0.0f));
            glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(starModel));

            glPointSize(star.size);
            glDrawArrays(GL_POINTS, 0, 1);
        }

        glBindVertexArray(0); // Unbind star VAO

        // --- Drawing the Ship (Only draw if the game is NOT over) ---
        if (!isGameOver)
        {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(player.position, 0.0f));
            model = glm::rotate(model, player.rotation, glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::scale(model, glm::vec3(player.scale, player.scale, 1.0f));
            glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(model));
            glUniform3f(colorLoc, 1.0f, 1.0f, 1.0f);

            glBindVertexArray(shipVAO);
            glDrawArrays(GL_LINE_LOOP, 0, 3);
        }

        // NEW: Drawing the Thrust Fire
        if (isThrusting && !isGameOver) {
            glm::mat4 fireModel = glm::mat4(1.0f);
            fireModel = glm::translate(fireModel, glm::vec3(player.position, 0.0f));
            fireModel = glm::rotate(fireModel, player.rotation, glm::vec3(0.0f, 0.0f, 1.0f));
            float fireScaleFactor = player.scale * 1.5f;
            fireModel = glm::scale(fireModel, glm::vec3(fireScaleFactor, fireScaleFactor, 1.0f));

            glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(fireModel));

            glUniform3f(colorLoc, 1.0f, 0.7f, 0.0f); // Bright Yellow/Orange
            glBindVertexArray(fireVAO);
            glDrawArrays(GL_LINE_LOOP, 0, 3);
        }

        // --- Drawing Asteroids ---
        glUniform3f(colorLoc, 0.6f, 0.6f, 0.6f);
        for (const auto& asteroid : asteroids) {
            glm::mat4 asteroidModel = glm::mat4(1.0f);
            asteroidModel = glm::translate(asteroidModel, glm::vec3(asteroid.position, 0.0f));
            asteroidModel = glm::rotate(asteroidModel, asteroid.rotation, glm::vec3(0.0f, 0.0f, 1.0f));
            asteroidModel = glm::scale(asteroidModel, glm::vec3(asteroid.scale, asteroid.scale, 1.0f));
            glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(asteroidModel));

            glBindVertexArray(asteroid.VAO);
            glDrawArrays(GL_LINE_LOOP, 0, asteroid.vertexCount);
        }

        // --- Drawing Bullets ---
        glUniform3f(colorLoc, 1.0f, 0.0f, 0.0f);
        glBindVertexArray(bulletVAO);

        for (const auto& bullet : bullets) {
            glm::mat4 bulletModel = glm::mat4(1.0f);
            bulletModel = glm::translate(bulletModel, glm::vec3(bullet.position, 0.0f));
            bulletModel = glm::scale(bulletModel, glm::vec3(bullet.scale, bullet.scale, 1.0f));
            glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(bulletModel));

            glPointSize(5.0f);
            glDrawArrays(GL_POINTS, 0, 1);
        }

        glBindVertexArray(0);

        // --- Check events and swap buffers ---
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // --- 5. Clean up and terminate ---
    glDeleteVertexArrays(1, &shipVAO);
    glDeleteBuffers(1, &shipVBO);
    glDeleteVertexArrays(1, &bulletVAO);
    glDeleteBuffers(1, &bulletVBO);
    glDeleteVertexArrays(1, &fireVAO);
    glDeleteBuffers(1, &fireVBO);
    glDeleteVertexArrays(1, &starVAO);
    glDeleteBuffers(1, &starVBO);
    glDeleteVertexArrays(1, &gradientVAO);
    glDeleteBuffers(1, &gradientVBO);

    // Clean up remaining dynamically created asteroid VAOs/VBOs
    for (const auto& asteroid : asteroids) {
        glDeleteVertexArrays(1, &asteroid.VAO);
        glDeleteBuffers(1, &asteroid.VBO);
    }

    glDeleteProgram(shaderProgram);
    glfwTerminate();
    return 0;
}

// --- Function Definitions ---

// Handles all user input (Rotation, Thrust, Firing)
void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // 1. Rotation Input
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        player.rotation += ROTATION_SPEED * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        player.rotation -= ROTATION_SPEED * deltaTime;
    player.rotation = fmod(player.rotation, 2.0f * glm::pi<float>());

    // 2. Thrust Input
    isThrusting = false;

    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
    {
        isThrusting = true; // Set flag if thrusting

        /*float dirX = sin(player.rotation);
        float dirY = cos(player.rotation);*/

        // Corrected for typical OpenGL Y-up / Y-axis model misalignment:
        float dirX = -sin(player.rotation);
        float dirY = cos(player.rotation);

        player.velocity.x += dirX * THRUST_SPEED * deltaTime;
        player.velocity.y += dirY * THRUST_SPEED * deltaTime;
    }

    // 3. Firing Input (using SPACE)
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && bulletCooldown <= 0.0f)
    {
        Bullet newBullet;

        float offsetX = sin(player.rotation) * player.scale;
        float offsetY = cos(player.rotation) * player.scale;

        newBullet.position.x = player.position.x + offsetX;
        newBullet.position.y = player.position.y + offsetY;

        float dirX = sin(player.rotation);
        float dirY = cos(player.rotation);

        newBullet.velocity.x = dirX * BULLET_SPEED + player.velocity.x;
        newBullet.velocity.y = dirY * BULLET_SPEED + player.velocity.y;

        bullets.push_back(newBullet);

        bulletCooldown = FIRE_RATE; // Reset cooldown
    }
}

// Collision Detection: Bounding Circle Check
bool checkCollision(glm::vec2 pos1, float rad1, glm::vec2 pos2, float rad2)
{
    glm::vec2 distanceVec = pos1 - pos2;
    float distanceSq = distanceVec.x * distanceVec.x + distanceVec.y * distanceVec.y;
    float radiiSumSq = (rad1 + rad2) * (rad1 + rad2);

    return distanceSq < radiiSumSq;
}

// Callback function to adjust the viewport when the window is resized
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}