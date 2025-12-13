#include <iostream>
#include <cmath>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <stddef.h> 


// 1. INCLUDE GLAD FIRST!
#include <glad/glad.h>
// 2. THEN INCLUDE GLFW
#include <GLFW/glfw3.h>

// GLM includes for vector math and transformations
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp> 

// ============================ SETTINGS ============================
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// ============================ ASTEROID SIZE DEFINITIONS ============================
enum AsteroidSize { SMALL, MEDIUM, LARGE };

// Helper function to get the scale factor based on size
float getScaleFactor(AsteroidSize size) {
    switch (size) {
    case LARGE: return 0.15f;
    case MEDIUM: return 0.08f;
    case SMALL: return 0.04f;
    default: return 0.15f;
    }
}

// Helper function to get the radius based on size (used for collision)
float getRadiusFactor(AsteroidSize size) {
    // These factors are relative to the internal 1.0f base radius of the generated shape
    switch (size) {
    case LARGE: return 0.15f;
    case MEDIUM: return 0.08f;
    case SMALL: return 0.04f;
    default: return 0.15f;
    }
}

// ============================ SHIP/GAME STATE STRUCT ============================
struct Ship {
    glm::vec2 position = glm::vec2(0.0f, 0.0f);
    glm::vec2 velocity = glm::vec2(0.0f, 0.0f);
    float rotation = 0.0f;
    float scale = getScaleFactor(SMALL); // Ship size is now based on a small scale
    float radius = getRadiusFactor(SMALL);
};
Ship player;

// ============================ PHYSICS CONSTANTS ============================
const float THRUST_SPEED = 1.0f;
const float ROTATION_SPEED = 2.0f;
const float FRICTION = 0.995f;

// ============================ BULLET CONSTANTS ============================
const float BULLET_SPEED = 2.5f;
const float FIRE_RATE = 0.2f;

// ============================ SPAWNING CONSTANTS ============================
const float INITIAL_SPAWN_RATE = 5.0f;
const float MIN_SPAWN_RATE = 1.0f;
const int MAX_ASTEROIDS = 20;

// ============================ GLOBAL STATE ============================
float deltaTime = 0.0f;
float lastFrame = 0.0f;
float bulletCooldown = 0.0f;
bool isGameOver = false;
bool isThrusting = false;
float asteroidSpawnTimer = 0.0f;
float currentSpawnRate = INITIAL_SPAWN_RATE;

// ============================ GLOBAL GRAPHICS HANDLES ============================
unsigned int bulletVAO, bulletVBO;
unsigned int fireVAO, fireVBO;
unsigned int gradientVAO, gradientVBO;
unsigned int shipFillVAO, shipFillVBO;
unsigned int bresenhamShipVAO, bresenhamShipVBO;
unsigned int gameOverTextVAO, gameOverTextVBO;

// ============================ GLOBAL SHADER PROGRAMS ============================
unsigned int backgroundProgram;
unsigned int shaderProgram;

// ============================ GLOBAL DATA BUFFERS ============================
std::vector<float> bresenhamOutputBuffer;

// ============================ STRUCTS ============================
struct Asteroid {
    glm::vec2 position, velocity;
    float rotation, rotationSpeed;
    AsteroidSize size;
    float scale;
    float radius;
    glm::vec3 color;
    unsigned int VAO_Fill = 0, VBO_Fill = 0; // Initialize handles
    int vertexCount = 0;
};
std::vector<Asteroid> asteroids;

struct Bullet {
    glm::vec2 position = glm::vec2(0.0f, 0.0f);
    glm::vec2 velocity = glm::vec2(0.0f, 0.0f);
    float scale = 0.01f;
    float radius = 0.01f;
    float lifetime = 1.0f;
};
std::vector<Bullet> bullets;

// ============================ FUNCTION PROTOTYPES ============================
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);
bool checkCollision(glm::vec2 pos1, float rad1, glm::vec2 pos2, float rad2);
void spawnNewAsteroid(glm::vec2 pos, AsteroidSize size);
void splitAsteroid(const Asteroid& rock, size_t index);
void drawBresenhamLine(int x0, int y0, int x1, int y1, std::vector<float>& vertexBuffer);
void drawBresenhamShip(const Ship& player, unsigned int vbo, std::vector<float>& vertexBuffer);

// ============================ SHADERS ============================
const char* vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec2 aPos;
    
    uniform mat4 transform;
    
    void main()
    {
        gl_Position = transform * vec4(aPos.x, aPos.y, 0.0, 1.0);
    }
)";

const char* fragmentShaderSource = R"(
    #version 330 core
    out vec4 FragColor;
    
    uniform vec3 lineColor; 
    
    void main()
    {
        FragColor = vec4(lineColor, 1.0f);
    }
)";

const char* bgVertexShader = R"(
    #version 330 core
    layout (location = 0) in vec2 aPos;
    out vec2 uv;
    void main() {
        uv = aPos * 0.5 + 0.5; 
        gl_Position = vec4(aPos, 0.0, 1.0);
    }
)";

const char* bgFragmentShader = R"(
    #version 330 core
    out vec4 FragColor;
    in vec2 uv;
    uniform float time;

    // Pseudo-random hash function
    float hash(vec2 p) {
        // Uses a standard magic number hash to produce a random float [0, 1]
        return fract(sin(dot(p, vec2(127.1,311.7))) * 43758.5453123);
    }
    
    // Perlin noise generation
    float noise(vec2 p) {
        vec2 i=floor(p), f=fract(p);
        f=f*f*(3.0-2.0*f);
        float a=hash(i), b=hash(i+vec2(1,0));
        float c=hash(i+vec2(0,1)), d=hash(i+vec2(1,1));
        return mix(mix(a,b,f.x), mix(c,d,f.x), f.y);
    }
    float fbm(vec2 p){
        float v=0.0,a=0.5;
        for(int i=0;i<5;i++){ v+=a*noise(p); p*=2.0; a*=0.5; } 
        return v;
    }

    void main(){
        vec2 p = uv*2.0-1.0;
        p.x *= 1.6;

        float n = fbm(p*2.5 + time*0.02);
        
        // Nebula colors (Dark and deep purple)
        vec3 darkBlue = vec3(0.02, 0.01, 0.05);  
        vec3 deepPurple = vec3(0.3, 0.1, 0.5);   
        vec3 nebula = mix(darkBlue, deepPurple, n * 0.7); 

        float d = length(p);
        vec3 sun = vec3(1.0,0.9,0.6) * exp(-d*8.0) * 3.0;

        // FINAL STAR FIX FOR UNIFORMITY AND NO BIAS:
        // Adds a large offset and time to starCoords to ensure the hash function is sampled
        // randomly across the entire UV space, eliminating spatial clumping/bias.
        vec2 starCoords = uv * 512.0 + vec2(123.45, 543.21) + time * 1.0; 
        
        // Threshold set to 0.999 for low density (0.1% chance).
        float stars = step(0.999, hash(starCoords)); 
        
        FragColor = vec4(nebula + sun + vec3(stars), 1.0);
    }
)";

// ============================ BRESENHAM (FOR SHIP OUTLINE) ============================
void drawBresenhamLine(int x0, int y0, int x1, int y1, std::vector<float>& vertexBuffer) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    int x = x0;
    int y = y0;

    while (true) {
        float glX = (float)x / (SCR_WIDTH / 2.0f) - 1.0f;
        float glY = (float)y / (SCR_HEIGHT / 2.0f) - 1.0f;

        vertexBuffer.push_back(glX);
        vertexBuffer.push_back(glY);

        if (x == x1 && y == y1) break;

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
}

void drawBresenhamShip(const Ship& player, unsigned int vbo, std::vector<float>& vertexBuffer) {
    glm::vec2 localVertices[] = {
        glm::vec2(0.0f,  1.0f),
        glm::vec2(-1.0f, -1.0f),
        glm::vec2(1.0f, -1.0f)
    };

    vertexBuffer.clear();

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(player.position, 0.0f));
    model = glm::rotate(model, player.rotation, glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, glm::vec3(player.scale, player.scale, 1.0f));

    int pixelVertices[6];

    for (int i = 0; i < 3; ++i) {
        glm::vec4 temp = model * glm::vec4(localVertices[i], 0.0f, 1.0f);

        pixelVertices[i * 2] = static_cast<int>((temp.x + 1.0f) * (SCR_WIDTH / 2.0f));
        pixelVertices[i * 2 + 1] = static_cast<int>((temp.y + 1.0f) * (SCR_HEIGHT / 2.0f));
    }

    drawBresenhamLine(pixelVertices[0], pixelVertices[1], pixelVertices[2], pixelVertices[3], vertexBuffer);
    drawBresenhamLine(pixelVertices[2], pixelVertices[3], pixelVertices[4], pixelVertices[5], vertexBuffer);
    drawBresenhamLine(pixelVertices[4], pixelVertices[5], pixelVertices[0], pixelVertices[1], vertexBuffer);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(vertexBuffer.size() * sizeof(float)), vertexBuffer.data());
}

// ============================ ASTEROID VERTEX GENERATION ============================
std::vector<float> generateFilledAsteroidVertices(int segments, float radius) {
    std::vector<float> vertices;
    vertices.push_back(0.0f); // Center point (Index 0 for TRIANGLE_FAN)
    vertices.push_back(0.0f);

    // Ensure we have enough segments for a detailed shape
    int actualSegments = std::max(segments, 20);

    for (int i = 0; i <= actualSegments; ++i) { // Note: Loop goes to <= segments to close the fan
        float angle = (float)i / (float)actualSegments * 2.0f * glm::pi<float>();

        // Add irregularity (radius factor between 0.8 and 1.2)
        float currentRadius = radius * (1.0f + ((float)std::rand() / RAND_MAX - 0.5f) * 0.4f);

        float x = currentRadius * cos(angle);
        float y = currentRadius * sin(angle);

        vertices.push_back(x);
        vertices.push_back(y);
    }
    return vertices;
}

void setupAsteroidGraphics(Asteroid& rock, int segments) {
    float baseRadius = 1.0f; // Internal normalized radius

    // --- [C] UPDATE FUNCTION CALL ---
    std::vector<float> fillVertices = generateFilledAsteroidVertices(segments, baseRadius);

    // The vertex count is now for GL_TRIANGLE_FAN (includes center + boundary)
    rock.vertexCount = static_cast<int>(fillVertices.size() / 2);

    // Use GL_LINES or GL_POINTS for rendering (not fill)
    glGenVertexArrays(1, &rock.VAO_Fill);
    glGenBuffers(1, &rock.VBO_Fill);

    glBindVertexArray(rock.VAO_Fill);
    glBindBuffer(GL_ARRAY_BUFFER, rock.VBO_Fill);

    glBufferData(GL_ARRAY_BUFFER, fillVertices.size() * sizeof(float), fillVertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

// ============================ ASTEROID LOGIC ============================

void spawnNewAsteroid(glm::vec2 pos, AsteroidSize size)
{
    if (asteroids.size() >= static_cast<size_t>(MAX_ASTEROIDS)) return;

    Asteroid newRock;
    newRock.size = size;
    newRock.scale = getScaleFactor(size);
    newRock.radius = getRadiusFactor(size);
    newRock.rotation = 0.0f;
    newRock.rotationSpeed = 0.3f + ((float)std::rand() / RAND_MAX * 0.5f);

    // --- [D] ADD COLOR PALETTE & ASSIGNMENT ---
    glm::vec3 palette[] = {
        glm::vec3(1.0f, 0.4f, 0.0f),  // Orange
        glm::vec3(0.0f, 0.8f, 0.8f),  // Cyan
        glm::vec3(0.8f, 0.0f, 0.8f),  // Magenta
        glm::vec3(1.0f, 1.0f, 0.0f),  // Yellow
        glm::vec3(0.1f, 1.0f, 0.1f)   // Green
    };
    int colorIndex = std::rand() % (sizeof(palette) / sizeof(glm::vec3));
    newRock.color = palette[colorIndex];

    // If splitting (internal spawn)
    if (pos != glm::vec2(0.0f, 0.0f)) {
        newRock.position = pos;

        // Give new asteroids a random velocity
        float angle = ((float)std::rand() / RAND_MAX) * 2.0f * glm::pi<float>();
        glm::vec2 direction = glm::vec2(cos(angle), sin(angle));
        float speed = 0.3f + ((float)std::rand() / RAND_MAX * 0.4f);
        newRock.velocity = direction * speed;
    }
    // If external spawn (off screen)
    else {
        float side = (float)std::rand() / RAND_MAX * 4.0f;
        if (side < 1.0f) { newRock.position = glm::vec2(((float)std::rand() / RAND_MAX * 2.0f) - 1.0f, 1.1f); }
        else if (side < 2.0f) { newRock.position = glm::vec2(((float)std::rand() / RAND_MAX * 2.0f) - 1.0f, -1.1f); }
        else if (side < 3.0f) { newRock.position = glm::vec2(-1.1f, ((float)std::rand() / RAND_MAX * 2.0f) - 1.0f); }
        else { newRock.position = glm::vec2(1.1f, ((float)std::rand() / RAND_MAX * 2.0f) - 1.0f); }

        glm::vec2 target = glm::vec2(0.0f, 0.0f);
        glm::vec2 direction = glm::normalize(target - newRock.position);
        float scatter = 0.2f;
        direction.x += ((float)std::rand() / RAND_MAX - 0.5f) * scatter;
        direction.y += ((float)std::rand() / RAND_MAX - 0.5f) * scatter;
        direction = glm::normalize(direction);
        float speed = 0.1f + ((float)std::rand() / RAND_MAX * 0.2f);
        newRock.velocity = direction * speed;
    }

    setupAsteroidGraphics(newRock, 16);
    asteroids.push_back(newRock);
}

void splitAsteroid(const Asteroid& rock, size_t index) {
    AsteroidSize nextSize;

    if (rock.size == LARGE) nextSize = MEDIUM;
    else if (rock.size == MEDIUM) nextSize = SMALL;
    else return; // Small asteroids are destroyed, not split

    // Delete the original asteroid's graphics handles
    glDeleteVertexArrays(1, &asteroids[index].VAO_Fill);
    glDeleteBuffers(1, &asteroids[index].VBO_Fill);

    // Remove the original rock from the vector
    asteroids.erase(asteroids.begin() + index);

    // Spawn two new, smaller rocks
    for (int i = 0; i < 2; ++i) {
        if (asteroids.size() < static_cast<size_t>(MAX_ASTEROIDS)) {
            // Spawn new asteroids slightly offset from the collision point
            float offsetX = ((float)std::rand() / RAND_MAX - 0.5f) * rock.scale * 0.5f;
            float offsetY = ((float)std::rand() / RAND_MAX - 0.5f) * rock.scale * 0.5f;
            glm::vec2 spawnPos = rock.position + glm::vec2(offsetX, offsetY);

            spawnNewAsteroid(spawnPos, nextSize);
        }
    }
}

// ============================ INPUT & CALLBACK DEFINITIONS ============================

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow* window)
{
    // ... (Rotation and Escape checks remain the same) ...

    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        player.rotation += ROTATION_SPEED * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        player.rotation -= ROTATION_SPEED * deltaTime;
    player.rotation = fmod(player.rotation, 2.0f * glm::pi<float>());

    isThrusting = false;

    // --- Calculate the Ship's Facing Direction (Unit Vector) ---
    // The ship's model (rotation=0) points UP (+Y).
    // To use standard trigonometry (angle from +X axis), we must offset the angle by 90 degrees (pi/2).
    float angleFromXAxis = player.rotation + glm::half_pi<float>();

    // Now use standard math for direction: X=cos, Y=sin
    float dirX = cos(angleFromXAxis);
    float dirY = sin(angleFromXAxis);

    // Note: The previous logic (dirX=sin, dirY=cos) was effectively doing this offset,
    // but the `glm::rotate` in rendering likely expects the standard angle,
    // creating the mismatch you observed when rotating left/right.
    // By using the standard X=cos, Y=sin, we align the physics direction.

    // --- Thrust Movement ---
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
    {
        isThrusting = true;

        // Ship movement: Apply acceleration (thrust) in the direction the ship is facing.
        player.velocity.x += dirX * THRUST_SPEED * deltaTime;
        player.velocity.y += dirY * THRUST_SPEED * deltaTime;
    }

    // --- Firing Bullet ---
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && bulletCooldown <= 0.0f)
    {
        Bullet newBullet;

        // Spawn the bullet slightly ahead of the ship's center.
        float spawnDistance = player.radius * 1.5f;

        newBullet.position.x = player.position.x + dirX * spawnDistance;
        newBullet.position.y = player.position.y + dirY * spawnDistance;

        // The bullet velocity is its own speed in the direction of fire, 
        // PLUS the ship's current velocity (momentum).
        newBullet.velocity.x = dirX * BULLET_SPEED + player.velocity.x;
        newBullet.velocity.y = dirY * BULLET_SPEED + player.velocity.y;

        bullets.push_back(newBullet);
        bulletCooldown = FIRE_RATE;
    }
}

bool checkCollision(glm::vec2 pos1, float rad1, glm::vec2 pos2, float rad2)
{
    glm::vec2 distanceVec = pos1 - pos2;
    float distanceSq = distanceVec.x * distanceVec.x + distanceVec.y * distanceVec.y;
    float radiiSumSq = (rad1 + rad2) * (rad1 + rad2);

    return distanceSq < radiiSumSq;
}


// ============================ MAIN FUNCTION ============================
int main()
{
    std::srand(static_cast<unsigned int>(std::time(0)));

    // --- 1. GLFW/GLAD Initialization ---
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Asteroids Clone (Fixed Star Uniformity)", NULL, NULL);
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

    // A. Game Object Shader
    unsigned int vShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vShader);
    unsigned int fShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fShader);
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vShader);
    glAttachShader(shaderProgram, fShader);
    glLinkProgram(shaderProgram);
    glDeleteShader(vShader);
    glDeleteShader(fShader);

    // B. Background Shader (Modified)
    unsigned int bgVS = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(bgVS, 1, &bgVertexShader, NULL);
    glCompileShader(bgVS);
    unsigned int bgFS = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(bgFS, 1, &bgFragmentShader, NULL);
    glCompileShader(bgFS);
    backgroundProgram = glCreateProgram();
    glAttachShader(backgroundProgram, bgVS);
    glAttachShader(backgroundProgram, bgFS);
    glLinkProgram(backgroundProgram);
    glDeleteShader(bgVS);
    glDeleteShader(bgFS);

    // --- 3. Graphics Setup (VAOs/VBOs) ---

    // A. Setup Background Quad
    float quad[] = { -1,-1, 1,-1, -1,1, 1,1 };
    glGenVertexArrays(1, &gradientVAO);
    glGenBuffers(1, &gradientVBO);
    glBindVertexArray(gradientVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gradientVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // B. Ship FILL Setup (GL_TRIANGLE_FAN)
    float shipFillVertices[] = {
        0.0f, 0.0f,
        0.0f, 1.0f,
        -1.0f, -1.0f,
         1.0f, -1.0f,
        0.0f, 1.0f
    };
    glGenVertexArrays(1, &shipFillVAO);
    glGenBuffers(1, &shipFillVBO);
    glBindVertexArray(shipFillVAO);
    glBindBuffer(GL_ARRAY_BUFFER, shipFillVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(shipFillVertices), shipFillVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // C. Ship OUTLINE Setup (Bresenham - Dynamic Buffer)
    const size_t MAX_BRESENHAM_VERTICES = 3 * (SCR_WIDTH + SCR_HEIGHT) * 2 * sizeof(float);
    glGenVertexArrays(1, &bresenhamShipVAO);
    glGenBuffers(1, &bresenhamShipVBO);
    glBindVertexArray(bresenhamShipVAO);
    glBindBuffer(GL_ARRAY_BUFFER, bresenhamShipVBO);
    glBufferData(GL_ARRAY_BUFFER, MAX_BRESENHAM_VERTICES, NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // D. Bullet Setup (Point)
    float bulletVertices[] = { 0.0f, 0.0f };
    glGenVertexArrays(1, &bulletVAO);
    glGenBuffers(1, &bulletVBO);
    glBindVertexArray(bulletVAO);
    glBindBuffer(GL_ARRAY_BUFFER, bulletVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(bulletVertices), bulletVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // E. Thrust Fire VAO/VBO (Filled Triangle)
    float fireVertices[] = {
        0.0f, 0.0f,
        -0.5f, -1.0f,
         0.5f, -1.0f,
         0.0f, 0.0f
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
    unsigned int timeLoc = glGetUniformLocation(backgroundProgram, "time");


    // --- 4. Render/Game Loop ---
    while (!glfwWindowShouldClose(window))
    {
        float t = (float)glfwGetTime();
        deltaTime = t - lastFrame;
        lastFrame = t;
        bulletCooldown -= deltaTime;

        // --- Input Handling ---
        processInput(window);

        // --- Physics and Collision Update ---
        if (!isGameOver)
        {
            asteroidSpawnTimer -= deltaTime;

            // Spawn initial LARGE asteroids
            if (asteroidSpawnTimer <= 0.0f && asteroids.size() < static_cast<size_t>(MAX_ASTEROIDS)) {
                spawnNewAsteroid(glm::vec2(0.0f, 0.0f), LARGE);
                currentSpawnRate = glm::max(MIN_SPAWN_RATE, currentSpawnRate - 0.1f);
                asteroidSpawnTimer = currentSpawnRate;
            }

            // Player Physics Update
            player.velocity *= FRICTION;
            player.position += player.velocity * deltaTime;
            if (player.position.x > 1.0f) player.position.x = -1.0f;
            else if (player.position.x < -1.0f) player.position.x = 1.0f;
            if (player.position.y > 1.0f) player.position.y = -1.0f;
            else if (player.position.y < -1.0f) player.position.y = 1.0f;

            // Asteroid Physics Update
            for (auto& asteroid : asteroids) {
                asteroid.position += asteroid.velocity * deltaTime;
                asteroid.rotation += asteroid.rotationSpeed * deltaTime;
                if (asteroid.position.x > 1.0f) asteroid.position.x = -1.0f;
                else if (asteroid.position.x < -1.0f) asteroid.position.x = 1.0f;
                if (asteroid.position.y > 1.0f) asteroid.position.y = -1.0f;
                else if (asteroid.position.y < -1.0f) asteroid.position.y = 1.0f;
            }

            // Bullet Physics Update
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

            // Ship-Asteroid Collision Check
            for (const auto& asteroid : asteroids) {
                if (checkCollision(player.position, player.radius, asteroid.position, asteroid.radius)) {
                    std::cout << "COLLISION! GAME OVER." << std::endl;
                    isGameOver = true;
                    break;
                }
            }

            // Bullet-Asteroid Collision Check (Handle splitting/destruction)
            for (size_t i = asteroids.size(); i > 0; --i) {
                size_t index = i - 1;
                bool bulletHit = false;

                for (size_t j = 0; j < bullets.size(); /* no increment here */) {
                    if (checkCollision(asteroids[index].position, asteroids[index].radius, bullets[j].position, bullets[j].radius)) {
                        bullets.erase(bullets.begin() + j);
                        bulletHit = true;
                        break;
                    }
                    else {
                        ++j;
                    }
                }

                if (bulletHit) {
                    if (asteroids[index].size == SMALL) {
                        // Destroy small asteroid
                        glDeleteVertexArrays(1, &asteroids[index].VAO_Fill);
                        glDeleteBuffers(1, &asteroids[index].VBO_Fill);
                        asteroids.erase(asteroids.begin() + index);
                    }
                    else {
                        // Split and shrink the larger asteroid
                        splitAsteroid(asteroids[index], index);
                    }
                }
            }
        }

        // --- Rendering Commands ---
        glClear(GL_COLOR_BUFFER_BIT);

        // 1. Draw the Dynamic Nebula Background
        glUseProgram(backgroundProgram);
        glUniform1f(timeLoc, t);
        glBindVertexArray(gradientVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // 2. Switch to the Main Game Object Shader
        glUseProgram(shaderProgram);

        // --- Drawing the Ship (Filled + Bresenham Outline) ---
        if (!isGameOver)
        {
            glm::mat4 shipModel = glm::mat4(1.0f);
            shipModel = glm::translate(shipModel, glm::vec3(player.position, 0.0f));
            shipModel = glm::rotate(shipModel, player.rotation, glm::vec3(0.0f, 0.0f, 1.0f));
            shipModel = glm::scale(shipModel, glm::vec3(player.scale, player.scale, 1.0f));
            glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(shipModel));

            // Draw FILL (GL_TRIANGLE_FAN) - Darker cyan
            glUniform3f(colorLoc, 0.2f, 0.7f, 0.7f);
            glBindVertexArray(shipFillVAO);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 5);

            // Draw OUTLINE (Bresenham) - Bright Cyan
            drawBresenhamShip(player, bresenhamShipVBO, bresenhamOutputBuffer);
            glm::mat4 identityModel = glm::mat4(1.0f);
            glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(identityModel));
            glUniform3f(colorLoc, 0.5f, 1.0f, 1.0f); // Bright Outline Color
            glPointSize(2.0f);
            glBindVertexArray(bresenhamShipVAO);
            glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(bresenhamOutputBuffer.size() / 2));
        }

        // --- Drawing the Thrust Fire (Filled) ---
        if (isThrusting && !isGameOver) {
            glm::mat4 fireModel = glm::mat4(1.0f);
            fireModel = glm::translate(fireModel, glm::vec3(player.position, 0.0f));
            fireModel = glm::rotate(fireModel, player.rotation, glm::vec3(0.0f, 0.0f, 1.0f));
            float fireScaleFactor = player.scale * 1.5f;
            fireModel = glm::scale(fireModel, glm::vec3(fireScaleFactor, fireScaleFactor, 1.0f));

            glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(fireModel));

            // THRUST COLOR: Yellow (Filled)
            glUniform3f(colorLoc, 1.0f, 1.0f, 0.0f);
            glBindVertexArray(fireVAO);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        }

        // --- Drawing Asteroids (Filled and Scaled) ---
        // ASTEROID COLOR: Orange/Red (Filled)
        // ASTEROID COLOR: Classic White/Gray Outline
        glUniform3f(colorLoc, 0.8f, 0.8f, 0.8f); // Use a bright outline color

        // Set point size to draw them like the classic arcade vector graphics
        glPointSize(2.0f);
        glLineWidth(2.0f); // Set line thickness for the outline

        for (const auto& asteroid : asteroids) {
            glm::mat4 asteroidModel = glm::mat4(1.0f);
            asteroidModel = glm::translate(asteroidModel, glm::vec3(asteroid.position, 0.0f));
            asteroidModel = glm::rotate(asteroidModel, asteroid.rotation, glm::vec3(0.0f, 0.0f, 1.0f));
            asteroidModel = glm::scale(asteroidModel, glm::vec3(asteroid.scale, asteroid.scale, 1.0f));
            glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(asteroidModel));

            glBindVertexArray(asteroid.VAO_Fill);

            // 1. Draw the FILL (Darker Shade of the base color)
            glm::vec3 fillColor = asteroid.color * 0.5f; // Darken for filled look
            glUniform3f(colorLoc, fillColor.x, fillColor.y, fillColor.z);
            glDrawArrays(GL_TRIANGLE_FAN, 0, asteroid.vertexCount); // Draw the filled body

            // 2. Draw the OUTLINE (Brighter Shade of the base color)
            glm::vec3 outlineColor = asteroid.color * 1.5f; // Brighten for outline
            outlineColor = glm::clamp(outlineColor, 0.0f, 1.0f); // Ensure color doesn't exceed 1.0

            glUniform3f(colorLoc, outlineColor.x, outlineColor.y, outlineColor.z);
            // Draw the line loop starting at index 1 to skip the center point
            glDrawArrays(GL_LINE_LOOP, 1, asteroid.vertexCount - 1);
        }

        // --- Drawing Bullets (Points) ---
        // BULLET COLOR: Red
        glUniform3f(colorLoc, 1.0f, 0.0f, 0.0f);
        glBindVertexArray(bulletVAO);

        for (const auto& bullet : bullets) {
            glm::mat4 bulletModel = glm::mat4(1.0f);
            bulletModel = glm::translate(bulletModel, glm::vec3(bullet.position, 0.0f));
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
    glDeleteVertexArrays(1, &bulletVAO);
    glDeleteBuffers(1, &bulletVBO);
    glDeleteVertexArrays(1, &fireVAO);
    glDeleteBuffers(1, &fireVBO);
    glDeleteVertexArrays(1, &gradientVAO);
    glDeleteBuffers(1, &gradientVBO);
    glDeleteVertexArrays(1, &bresenhamShipVAO);
    glDeleteBuffers(1, &bresenhamShipVBO);
    glDeleteVertexArrays(1, &shipFillVAO);
    glDeleteBuffers(1, &shipFillVBO);

    for (const auto& asteroid : asteroids) {
        glDeleteVertexArrays(1, &asteroid.VAO_Fill);
        glDeleteBuffers(1, &asteroid.VBO_Fill);
    }

    glDeleteProgram(shaderProgram);
    glDeleteProgram(backgroundProgram);
    glfwTerminate();
    return 0;
}