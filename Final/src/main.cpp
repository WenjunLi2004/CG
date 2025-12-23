#include "Angel.h"
#include "Camera.h"
#include "TriMesh.h"
#include "stb_image.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

struct Material {
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
    float shininess;
};

struct GLObject {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLsizei count = 0;
    bool hasTexture = false;
    GLuint texture = 0;
    glm::vec2 uvScale = glm::vec2(1.0f);
    Material material{};
};

struct RenderItem {
    std::string name;
    TriMesh mesh;
    GLObject gl;
};

struct DrawItem {
    RenderItem* item;
    glm::mat4 model;
};

// window
int WIDTH = 960;
int HEIGHT = 720;
GLFWwindow* window = nullptr;

// camera / light
Camera* camera = new Camera(); // kept for projection params; view will be overridden by free-fly cam
glm::vec3 camPos(0.0f, 1.4f, 2.0f);
float yawDeg = -90.0f;
float pitchDeg = -10.0f;
bool firstMouse = true;
double lastX = 0.0, lastY = 0.0;
float camSpeed = 2.5f; // units per second
bool mouseCaptured = true;
glm::vec3 lightPos(0.0f, 2.5f, 0.0f);
glm::vec3 lightDir(0.0f, -1.0f, 0.0f);
float innerCutoff = glm::cos(glm::radians(15.0f));
float outerCutoff = glm::cos(glm::radians(20.0f));

// shadow map
const GLuint SHADOW_SIZE = 1024;
GLuint depthFBO = 0;
GLuint depthTexture = 0;
GLuint mainProgram = 0;
GLuint depthProgram = 0;

// joint angles
struct JointState {
    float baseYaw = 0.0f;
    float lowerPitch = 20.0f;
    float upperPitch = 15.0f;
    float clawAngle = 25.0f;
} joints;

// arm dimensions
const float BASE_HEIGHT = 0.25f;
const float BASE_RADIUS = 0.4f;
const float LOWER_LEN = 0.5f;
const float LOWER_THICK = 0.12f;
const float UPPER_LEN = 0.4f;
const float UPPER_THICK = 0.10f;
const float CLAW_LEN = 0.15f;
const float CLAW_THICK = 0.04f;
const float CLAW_OFFSET = 0.06f;
const float TABLE_HEIGHT = 0.9f;
bool lampOn = true;

float deltaTime = 0.016f;
float lastFrame = 0.0f;

RenderItem floorItem;
RenderItem wallPosZ;
RenderItem wallNegZ;
RenderItem wallPosX;
RenderItem wallNegX;
RenderItem baseItem;
RenderItem lowerItem;
RenderItem upperItem;
RenderItem clawLeft;
RenderItem clawRight;
RenderItem tableItem;
RenderItem ceilingItem;

std::vector<RenderItem*> sceneItems;

bool fileExists(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (f) { fclose(f); return true; }
    return false;
}

std::string resolvePath(const std::string& rel) {
    // Try several common roots so executable can be run from repo root or build dir
    const std::string absRoot = "/Users/wenjun/Documents/CG/Final/"; // project source root
    std::vector<std::string> bases = {
        "",              // run inside Final/build
        "Final/",        // run at repo root
        "../",           // run inside Final/build subdir
        "../Final/",
        "build/",        // run at Final/
        "Final/build/",
        "../build/",
        "../../Final/",
        absRoot,
        absRoot + "build/"
    };
    for (const auto& b : bases) {
        std::string full = b + rel;
        if (fileExists(full)) return full;
    }
    return rel; // fallback
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    WIDTH = width;
    HEIGHT = height;
    glViewport(0, 0, width, height);
    camera->aspect = static_cast<float>(width) / static_cast<float>(height);
    lastX = width * 0.5;
    lastY = height * 0.5;
}

void resetJoints() {
    joints.baseYaw = 0.0f;
    joints.lowerPitch = 20.0f;
    joints.upperPitch = 15.0f;
    joints.clawAngle = 25.0f;
}

void printHelp() {
    std::cout << "键盘操作:\n"
              << " 1 / 2 : 基座逆/顺时针旋转\n"
              << " 3 / 4 : 下臂俯仰 [-60, 75]\n"
              << " 5 / 6 : 上臂俯仰 [-45, 90]\n"
              << " 7 / 8 : 手爪开合 [10, 60]\n"
              << " L     : 切换吊灯开关\n"
              << " R     : 关节归位\n"
              << " H     : 打印帮助\n"
              << " U/I/O + Shift : 调整相机角度/距离\n"
              << " SPACE : 重置相机\n"
              << " ESC/Q : 退出\n";
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
    switch (key) {
    case GLFW_KEY_ESCAPE:
    case GLFW_KEY_Q:
        glfwSetWindowShouldClose(window, true);
        break;
    case GLFW_KEY_H:
        if (action == GLFW_PRESS) printHelp();
        break;
    case GLFW_KEY_R:
        if (action == GLFW_PRESS) resetJoints();
        break;
    case GLFW_KEY_L:
        if (action == GLFW_PRESS) {
            lampOn = !lampOn;
            std::cout << "Lamp " << (lampOn ? "On" : "Off") << std::endl;
        }
        break;
    case GLFW_KEY_P:
        if (action == GLFW_PRESS) {
            mouseCaptured = !mouseCaptured;
            glfwSetInputMode(window, GLFW_CURSOR, mouseCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            firstMouse = true;
        }
        break;
    case GLFW_KEY_SPACE:
        if (action == GLFW_PRESS) {
            // Reset camera to default view
            delete camera;
            camera = new Camera();
            camera->aspect = static_cast<float>(WIDTH) / static_cast<float>(HEIGHT);
        }
        break;
    case GLFW_KEY_1:
        joints.baseYaw -= 5.0f;
        break;
    case GLFW_KEY_2:
        joints.baseYaw += 5.0f;
        break;
    case GLFW_KEY_3:
        joints.lowerPitch = std::max(-60.0f, joints.lowerPitch - 5.0f);
        break;
    case GLFW_KEY_4:
        joints.lowerPitch = std::min(75.0f, joints.lowerPitch + 5.0f);
        break;
    case GLFW_KEY_5:
        joints.upperPitch = std::max(-45.0f, joints.upperPitch - 5.0f);
        break;
    case GLFW_KEY_6:
        joints.upperPitch = std::min(90.0f, joints.upperPitch + 5.0f);
        break;
    case GLFW_KEY_7:
        joints.clawAngle = std::max(10.0f, joints.clawAngle - 5.0f);
        break;
    case GLFW_KEY_8:
        joints.clawAngle = std::min(60.0f, joints.clawAngle + 5.0f);
        break;
    default:
        break;
    }
}

void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) {
    if (!mouseCaptured) return;
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }
    float xoffset = static_cast<float>(xpos - lastX);
    float yoffset = static_cast<float>(lastY - ypos); // reversed: y ranges top->bottom
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yawDeg += xoffset;
    pitchDeg += yoffset;
    if (pitchDeg > 89.0f) pitchDeg = 89.0f;
    if (pitchDeg < -89.0f) pitchDeg = -89.0f;
}

// Simple P3 PPM loader (ASCII)
unsigned char* loadPPM_P3(const char* filename, int* width, int* height, int* channels) {
    std::ifstream file(filename);
    if (!file.is_open()) return nullptr;

    std::string line;
    std::getline(file, line);
    if (line != "P3") return nullptr;

    // Skip comments
    while (file.peek() == '#') {
        std::getline(file, line);
    }

    file >> *width >> *height;
    int maxVal;
    file >> maxVal;

    *channels = 3;
    size_t size = (*width) * (*height) * 3;
    unsigned char* data = new unsigned char[size];

    int r, g, b;
    for (size_t i = 0; i < size; i += 3) {
        if (!(file >> r >> g >> b)) {
            delete[] data;
            return nullptr;
        }
        data[i] = (unsigned char)r;
        data[i+1] = (unsigned char)g;
        data[i+2] = (unsigned char)b;
    }
    return data;
}

GLuint loadTexture(const std::string& path) {
    // Try several possible relative paths to tolerate different working dirs
    std::vector<std::string> candidates = {
        path,
        resolvePath(path),
        "Final/" + path,
        "../" + path,
        "../Final/" + path,
        "build/" + path,
        "Final/build/" + path,
        "../build/" + path,
        "assets/textures/" + path,
        "Final/assets/textures/" + path
    };

    int width = 0, height = 0, channels = 0;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* pixels = nullptr;
    std::string used;
    bool isP3 = false;

    for (const auto& p : candidates) {
        // Try stbi_load first (handles P6, PNG, JPG, etc.)
        pixels = stbi_load(p.c_str(), &width, &height, &channels, 0);
        if (pixels) {
            used = p;
            break;
        }
        // If failed, try P3 loader
        pixels = loadPPM_P3(p.c_str(), &width, &height, &channels);
        if (pixels) {
            used = p;
            isP3 = true;
            break;
        }
    }
    
    if (!pixels) {
        std::cerr << "Failed to load texture (tried variants) for: " << path << std::endl;
        return 0;
    }

    GLenum format = GL_RGB;
    if (channels == 1) format = GL_RED;
    else if (channels == 3) format = GL_RGB;
    else if (channels == 4) format = GL_RGBA;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    
    // Set alignment to 1 for byte-packed data
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    float border[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_2D);
    
    if (isP3) delete[] pixels;
    else stbi_image_free(pixels);
    
    std::cout << "Loaded texture: " << used << " (" << width << "x" << height << ", ch=" << channels << ")\n";
    return tex;
}

GLObject buildGLObject(TriMesh& mesh, const std::string& texturePath, const Material& mat, const glm::vec2& uvScale = glm::vec2(1.0f)) {
    struct Vertex {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec3 color;
        glm::vec2 uv;
    };

    const auto& pts = mesh.getPoints();
    const auto& nrm = mesh.getNormals();
    const auto& col = mesh.getColors();
    const auto& uv = mesh.getTextures();

    std::vector<Vertex> vertices;
    vertices.reserve(pts.size());
    for (size_t i = 0; i < pts.size(); ++i) {
        Vertex v{};
        v.pos = pts[i];
        v.normal = (i < nrm.size()) ? nrm[i] : glm::vec3(0, 1, 0);
        v.color = (i < col.size()) ? col[i] : glm::vec3(1, 1, 1);
        v.uv = (i < uv.size()) ? uv[i] : glm::vec2(0.0f);
        vertices.push_back(v);
    }

    GLObject obj{};
    obj.count = static_cast<GLsizei>(vertices.size());
    obj.material = mat;
    obj.uvScale = uvScale;
    obj.hasTexture = !texturePath.empty();
    if (obj.hasTexture) {
        obj.texture = loadTexture(texturePath);
        if (obj.texture == 0) {
            obj.hasTexture = false; // fallback to vertex color if texture missing
        }
    }

    glGenVertexArrays(1, &obj.vao);
    glGenBuffers(1, &obj.vbo);
    glBindVertexArray(obj.vao);
    glBindBuffer(GL_ARRAY_BUFFER, obj.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return obj;
}

glm::mat4 computeLightSpaceMatrix() {
    const float nearPlane = 0.3f;
    const float farPlane = 10.0f;
    glm::mat4 lightProj = glm::perspective(glm::radians(30.0f), 1.0f, nearPlane, farPlane);
    glm::mat4 lightView = glm::lookAt(lightPos, lightPos + lightDir, glm::vec3(0.0f, 0.0f, -1.0f));
    return lightProj * lightView;
}

void setupShadowMap() {
    glGenFramebuffers(1, &depthFBO);
    glGenTextures(1, &depthTexture);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_SIZE, SHADOW_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindFramebuffer(GL_FRAMEBUFFER, depthFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Shadow framebuffer not complete" << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void initMeshes() {
    Material metal{glm::vec3(0.32f), glm::vec3(0.92f, 0.9f, 0.88f), glm::vec3(0.7f), 64.0f};
    Material concrete{glm::vec3(0.45f), glm::vec3(0.99f, 0.98f, 0.95f), glm::vec3(0.35f), 22.0f};
    Material matteGrey{glm::vec3(0.4f), glm::vec3(0.92f, 0.88f, 0.84f), glm::vec3(0.35f), 28.0f};
    Material accent{glm::vec3(0.55f), glm::vec3(0.9f, 0.8f, 0.3f), glm::vec3(0.7f), 96.0f};

    std::string floorTex = fileExists("assets/textures/floor_user.png") ? "assets/textures/floor_user.png" : "assets/textures/floor_tile.ppm";
    std::string wallTex = fileExists("assets/textures/wall_user.png") ? "assets/textures/wall_user.png" : "assets/textures/wall_tile.ppm";
    std::string tableTex = fileExists("assets/textures/table_user.png") ? "assets/textures/table_user.png" : "assets/textures/table_top.ppm";

    floorItem.name = "floor";
    floorItem.mesh.setNormalize(false);
    floorItem.mesh.generateSquare(glm::vec3(1.0f));
    // Bright ceramic tile floor
    floorItem.gl = buildGLObject(floorItem.mesh, floorTex, metal, glm::vec2(5.0f, 5.0f));

    wallPosZ.name = "wall+z";
    wallPosZ.mesh.setNormalize(false);
    wallPosZ.mesh.generateSquare(glm::vec3(1.0f));
    wallPosZ.gl = buildGLObject(wallPosZ.mesh, wallTex, concrete, glm::vec2(3.0f, 2.2f));

    wallNegZ.name = "wall-z";
    wallNegZ.mesh.setNormalize(false);
    wallNegZ.mesh.generateSquare(glm::vec3(1.0f));
    wallNegZ.gl = buildGLObject(wallNegZ.mesh, wallTex, concrete, glm::vec2(3.0f, 2.2f));

    wallPosX.name = "wall+x";
    wallPosX.mesh.setNormalize(false);
    wallPosX.mesh.generateSquare(glm::vec3(1.0f));
    wallPosX.gl = buildGLObject(wallPosX.mesh, wallTex, concrete, glm::vec2(3.0f, 2.2f));

    wallNegX.name = "wall-x";
    wallNegX.mesh.setNormalize(false);
    wallNegX.mesh.generateSquare(glm::vec3(1.0f));
    wallNegX.gl = buildGLObject(wallNegX.mesh, wallTex, concrete, glm::vec2(3.0f, 2.2f));

    ceilingItem.name = "ceiling";
    ceilingItem.mesh.setNormalize(false);
    ceilingItem.mesh.generateSquare(glm::vec3(1.0f));
    ceilingItem.gl = buildGLObject(ceilingItem.mesh, "assets/textures/ceiling.ppm", concrete, glm::vec2(3.0f, 3.0f));

    baseItem.name = "base";
    baseItem.mesh.setNormalize(false);
    baseItem.mesh.generateCylinder(48, BASE_RADIUS, BASE_HEIGHT * 0.5f);
    baseItem.gl = buildGLObject(baseItem.mesh, "", accent);

    lowerItem.name = "lower";
    lowerItem.mesh.setNormalize(false);
    lowerItem.mesh.generateCylinder(36, LOWER_THICK * 0.5f, LOWER_LEN * 0.5f);
    lowerItem.gl = buildGLObject(lowerItem.mesh, "", metal);

    upperItem.name = "upper";
    upperItem.mesh.setNormalize(false);
    upperItem.mesh.generateCylinder(36, UPPER_THICK * 0.5f, UPPER_LEN * 0.5f);
    upperItem.gl = buildGLObject(upperItem.mesh, "", metal);

    clawLeft.name = "clawL";
    clawLeft.mesh.setNormalize(false);
    clawLeft.mesh.generateCube(glm::vec3(0.9f, 0.5f, 0.4f));
    clawLeft.gl = buildGLObject(clawLeft.mesh, "", matteGrey);

    clawRight.name = "clawR";
    clawRight.mesh.setNormalize(false);
    clawRight.mesh.generateCube(glm::vec3(0.9f, 0.5f, 0.4f));
    clawRight.gl = buildGLObject(clawRight.mesh, "", matteGrey);

    // Table: generate cylinder (OBJ removed for stability)
    tableItem.name = "table";
    tableItem.mesh.setNormalize(false);
    tableItem.mesh.cleanData();
    tableItem.mesh.generateCylinder(80, 0.8f, TABLE_HEIGHT * 0.5f);
    tableItem.mesh.setScale(glm::vec3(1.0f));
    tableItem.gl = buildGLObject(tableItem.mesh, tableTex, matteGrey, glm::vec2(3.0f, 3.0f));

    sceneItems = {&floorItem, &wallPosZ, &wallNegZ, &wallPosX, &wallNegX, &ceilingItem, &tableItem,
                  &baseItem, &lowerItem, &upperItem, &clawLeft, &clawRight};
}

std::vector<DrawItem> gatherDrawItems() {
    std::vector<DrawItem> items;
    const float roomSize = 5.0f;
    const float wallHeight = 3.0f;

    // floor (rotate to XZ plane)
    glm::mat4 floorModel = glm::mat4(1.0f);
    floorModel = glm::translate(floorModel, glm::vec3(0.0f, 0.0f, 0.0f));
    floorModel = glm::rotate(floorModel, glm::radians(-90.0f), glm::vec3(1, 0, 0));
    floorModel = glm::scale(floorModel, glm::vec3(roomSize, roomSize, 1.0f));
    items.push_back({&floorItem, floorModel});

    // walls
    glm::mat4 wallFront = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, wallHeight / 2.0f, roomSize / 2.0f));
    wallFront = glm::rotate(wallFront, glm::radians(180.0f), glm::vec3(0, 1, 0));
    wallFront = glm::scale(wallFront, glm::vec3(roomSize, wallHeight, 1.0f));
    items.push_back({&wallPosZ, wallFront});

    glm::mat4 wallBack = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, wallHeight / 2.0f, -roomSize / 2.0f));
    wallBack = glm::scale(wallBack, glm::vec3(roomSize, wallHeight, 1.0f));
    items.push_back({&wallNegZ, wallBack});

    glm::mat4 wallRight = glm::translate(glm::mat4(1.0f), glm::vec3(roomSize / 2.0f, wallHeight / 2.0f, 0.0f));
    wallRight = glm::rotate(wallRight, glm::radians(-90.0f), glm::vec3(0, 1, 0));
    wallRight = glm::scale(wallRight, glm::vec3(roomSize, wallHeight, 1.0f));
    items.push_back({&wallPosX, wallRight});

    glm::mat4 wallLeft = glm::translate(glm::mat4(1.0f), glm::vec3(-roomSize / 2.0f, wallHeight / 2.0f, 0.0f));
    wallLeft = glm::rotate(wallLeft, glm::radians(90.0f), glm::vec3(0, 1, 0));
    wallLeft = glm::scale(wallLeft, glm::vec3(roomSize, wallHeight, 1.0f));
    items.push_back({&wallNegX, wallLeft});

    glm::mat4 ceiling = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, wallHeight, 0.0f));
    ceiling = glm::rotate(ceiling, glm::radians(90.0f), glm::vec3(1, 0, 0));
    ceiling = glm::scale(ceiling, glm::vec3(roomSize, roomSize, 1.0f));
    items.push_back({&ceilingItem, ceiling});

    // work table centered (keep OBJ orientation, scale uniformly on XZ, height via Y)
    glm::mat4 tableModel = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, TABLE_HEIGHT * 0.5f, 0.0f));
    tableModel = glm::rotate(tableModel, glm::radians(-90.0f), glm::vec3(1, 0, 0)); // cylinder to stand upright
    tableModel = glm::scale(tableModel, glm::vec3(1.1f, 1.1f, TABLE_HEIGHT));
    items.push_back({&tableItem, tableModel});

    // mechanical arm hierarchy
    glm::mat4 baseFrame = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, TABLE_HEIGHT + 0.05f, 0.0f));
    baseFrame = glm::rotate(baseFrame, glm::radians(joints.baseYaw), glm::vec3(0, 1, 0));
    glm::mat4 baseModel = baseFrame * glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1, 0, 0));
    baseModel = baseModel * glm::scale(glm::mat4(1.0f), glm::vec3(BASE_RADIUS, BASE_RADIUS, BASE_HEIGHT));
    items.push_back({&baseItem, baseModel});

    glm::mat4 lowerFrame = baseFrame;
    lowerFrame = lowerFrame * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, BASE_HEIGHT, 0.0f));
    lowerFrame = lowerFrame * glm::rotate(glm::mat4(1.0f), glm::radians(joints.lowerPitch), glm::vec3(1, 0, 0));
    glm::mat4 lowerModel = lowerFrame * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, LOWER_LEN / 2.0f));
    lowerModel = lowerModel * glm::scale(glm::mat4(1.0f), glm::vec3(LOWER_THICK, LOWER_THICK, LOWER_LEN));
    items.push_back({&lowerItem, lowerModel});

    glm::mat4 upperFrame = lowerFrame;
    upperFrame = upperFrame * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, LOWER_LEN));
    upperFrame = upperFrame * glm::rotate(glm::mat4(1.0f), glm::radians(joints.upperPitch), glm::vec3(1, 0, 0));
    glm::mat4 upperModel = upperFrame * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, UPPER_LEN / 2.0f));
    upperModel = upperModel * glm::scale(glm::mat4(1.0f), glm::vec3(UPPER_THICK, UPPER_THICK, UPPER_LEN));
    items.push_back({&upperItem, upperModel});

    glm::mat4 wristFrame = upperFrame * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, UPPER_LEN));
    glm::mat4 leftModel = wristFrame;
    leftModel = leftModel * glm::rotate(glm::mat4(1.0f), glm::radians(joints.clawAngle), glm::vec3(0, 1, 0));
    leftModel = leftModel * glm::translate(glm::mat4(1.0f), glm::vec3(CLAW_OFFSET, 0.0f, CLAW_LEN / 2.0f));
    leftModel = leftModel * glm::scale(glm::mat4(1.0f), glm::vec3(CLAW_THICK, CLAW_THICK, CLAW_LEN));
    items.push_back({&clawLeft, leftModel});

    glm::mat4 rightModel = wristFrame;
    rightModel = rightModel * glm::rotate(glm::mat4(1.0f), glm::radians(-joints.clawAngle), glm::vec3(0, 1, 0));
    rightModel = rightModel * glm::translate(glm::mat4(1.0f), glm::vec3(-CLAW_OFFSET, 0.0f, CLAW_LEN / 2.0f));
    rightModel = rightModel * glm::scale(glm::mat4(1.0f), glm::vec3(CLAW_THICK, CLAW_THICK, CLAW_LEN));
    items.push_back({&clawRight, rightModel});

    return items;
}

void sendCommonUniforms(GLuint program, const glm::mat4& lightSpace) {
    glUseProgram(program);
    // free-fly camera
    glm::vec3 front;
    front.x = cos(glm::radians(yawDeg)) * cos(glm::radians(pitchDeg));
    front.y = sin(glm::radians(pitchDeg));
    front.z = sin(glm::radians(yawDeg)) * cos(glm::radians(pitchDeg));
    glm::vec3 camFront = glm::normalize(front);
    glm::mat4 view = glm::lookAt(camPos, camPos + camFront, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), static_cast<float>(WIDTH) / static_cast<float>(HEIGHT), 0.05f, 20.0f);
    glUniformMatrix4fv(glGetUniformLocation(program, "view"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE, &proj[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(program, "lightSpaceMatrix"), 1, GL_FALSE, &lightSpace[0][0]);
    glUniform3fv(glGetUniformLocation(program, "light.position"), 1, &lightPos[0]);
    glUniform3fv(glGetUniformLocation(program, "light.direction"), 1, &lightDir[0]);
    glUniform1f(glGetUniformLocation(program, "light.cutOff"), innerCutoff);
    glUniform1f(glGetUniformLocation(program, "light.outerCutOff"), outerCutoff);
    float roomAmbient = lampOn ? 0.35f : 0.08f;
    float fillStrength = lampOn ? 0.35f : 0.12f;
    float ambientBoost = lampOn ? 0.35f : 0.12f;

    glUniform3f(glGetUniformLocation(program, "light.ambient"), 0.8f, 0.75f, 0.7f); // brighter warm ambient
    glUniform3f(glGetUniformLocation(program, "light.diffuse"), lampOn ? 1.6f : 0.2f, lampOn ? 1.45f : 0.2f, lampOn ? 1.25f : 0.2f);
    glUniform3f(glGetUniformLocation(program, "light.specular"), lampOn ? 1.2f : 0.0f, lampOn ? 1.1f : 0.0f, lampOn ? 1.05f : 0.0f);
    glUniform1f(glGetUniformLocation(program, "light.constant"), 1.0f);
    glUniform1f(glGetUniformLocation(program, "light.linear"), 0.04f);   // larger radius
    glUniform1f(glGetUniformLocation(program, "light.quadratic"), 0.015f);
    glUniform3fv(glGetUniformLocation(program, "eyePos"), 1, &camera->eye[0]);
    glUniform1i(glGetUniformLocation(program, "diffuseMap"), 0);
    glUniform1i(glGetUniformLocation(program, "shadowMap"), 1);
    glUniform1i(glGetUniformLocation(program, "lightEnabled"), lampOn ? 1 : 0);
    glUniform1f(glGetUniformLocation(program, "roomAmbient"), roomAmbient);
    glUniform3f(glGetUniformLocation(program, "fillLight"), fillStrength, fillStrength * 0.92f, fillStrength * 0.88f); // soft fill
    // bonus: small emissive boost to mimic indoor GI when textures fallback to vertex color
    glUniform1f(glGetUniformLocation(program, "ambientBoost"), ambientBoost);
}

void renderDepthPass(const std::vector<DrawItem>& items, const glm::mat4& lightSpace) {
    glViewport(0, 0, SHADOW_SIZE, SHADOW_SIZE);
    glBindFramebuffer(GL_FRAMEBUFFER, depthFBO);
    glClear(GL_DEPTH_BUFFER_BIT);
    glUseProgram(depthProgram);
    glUniformMatrix4fv(glGetUniformLocation(depthProgram, "lightSpaceMatrix"), 1, GL_FALSE, &lightSpace[0][0]);
    for (const auto& it : items) {
        glUniformMatrix4fv(glGetUniformLocation(depthProgram, "model"), 1, GL_FALSE, &it.model[0][0]);
        glBindVertexArray(it.item->gl.vao);
        glDrawArrays(GL_TRIANGLES, 0, it.item->gl.count);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void renderMainPass(const std::vector<DrawItem>& items, const glm::mat4& lightSpace) {
    glViewport(0, 0, WIDTH, HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(mainProgram);
    sendCommonUniforms(mainProgram, lightSpace);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, depthTexture);

    for (const auto& it : items) {
        const GLObject& obj = it.item->gl;
        glUniformMatrix4fv(glGetUniformLocation(mainProgram, "model"), 1, GL_FALSE, &it.model[0][0]);
        glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(it.model)));
        glUniformMatrix3fv(glGetUniformLocation(mainProgram, "normalMatrix"), 1, GL_FALSE, &normalMat[0][0]);
        glUniform3fv(glGetUniformLocation(mainProgram, "material.ambient"), 1, &obj.material.ambient[0]);
        glUniform3fv(glGetUniformLocation(mainProgram, "material.diffuse"), 1, &obj.material.diffuse[0]);
        glUniform3fv(glGetUniformLocation(mainProgram, "material.specular"), 1, &obj.material.specular[0]);
        glUniform1f(glGetUniformLocation(mainProgram, "material.shininess"), obj.material.shininess);
        glUniform2fv(glGetUniformLocation(mainProgram, "uvScale"), 1, &obj.uvScale[0]);
        glUniform1i(glGetUniformLocation(mainProgram, "useTexture"), obj.hasTexture ? 1 : 0);
        if (obj.hasTexture) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, obj.texture);
        }
        glBindVertexArray(obj.vao);
        glDrawArrays(GL_TRIANGLES, 0, obj.count);
    }
}

void initGL() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    window = glfwCreateWindow(WIDTH, HEIGHT, "2023150001_李文俊_期末大作业", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);
    glfwSetInputMode(window, GLFW_CURSOR, mouseCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        exit(EXIT_FAILURE);
    }
    glEnable(GL_DEPTH_TEST);
    // Show interior walls without worrying about winding
    glDisable(GL_CULL_FACE);
    glClearColor(0.9f, 0.9f, 0.92f, 1.0f); // brighter background during clear
}

int main(int argc, char** argv) {
    initGL();
    printHelp();
    camera->aspect = static_cast<float>(WIDTH) / static_cast<float>(HEIGHT);
    lastX = WIDTH * 0.5;
    lastY = HEIGHT * 0.5;

    initMeshes();
    std::string vsMain = resolvePath("shaders/main.vs");
    std::string fsMain = resolvePath("shaders/main.fs");
    std::string vsDepth = resolvePath("shaders/depth.vs");
    std::string fsDepth = resolvePath("shaders/depth.fs");
    mainProgram = InitShader(vsMain.c_str(), fsMain.c_str());
    depthProgram = InitShader(vsDepth.c_str(), fsDepth.c_str());
    setupShadowMap();

    while (!glfwWindowShouldClose(window)) {
        float current = glfwGetTime();
        deltaTime = current - lastFrame;
        lastFrame = current;
        int fbw, fbh;
        glfwGetFramebufferSize(window, &fbw, &fbh);
        if (fbw > 0 && fbh > 0) {
            WIDTH = fbw;
            HEIGHT = fbh;
            glViewport(0, 0, fbw, fbh);
        }

        // camera movement (WASD)
        glm::vec3 front;
        front.x = cos(glm::radians(yawDeg)) * cos(glm::radians(pitchDeg));
        front.y = sin(glm::radians(pitchDeg));
        front.z = sin(glm::radians(yawDeg)) * cos(glm::radians(pitchDeg));
        glm::vec3 camFront = glm::normalize(front);
        glm::vec3 camRight = glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f)));
        float velocity = camSpeed * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camPos += camFront * velocity;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camPos -= camFront * velocity;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camPos -= camRight * velocity;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camPos += camRight * velocity;
        // clamp camera inside room
        float limit = 2.2f;
        camPos.x = std::max(-limit, std::min(limit, camPos.x));
        camPos.z = std::max(-limit, std::min(limit, camPos.z));
        camPos.y = std::max(0.4f, std::min(2.6f, camPos.y));

        auto drawList = gatherDrawItems();
        glm::mat4 lightSpace = computeLightSpaceMatrix();
        renderDepthPass(drawList, lightSpace);
        renderMainPass(drawList, lightSpace);
        
        // Simple HUD in title
        std::string title = "2023150001_李文俊_期末大作业 | FPS: " + std::to_string(1.0/0.016) + 
                            " | Base: " + std::to_string((int)joints.baseYaw) + 
                            " | Lower: " + std::to_string((int)joints.lowerPitch) + 
                            " | Upper: " + std::to_string((int)joints.upperPitch) + 
                            " | Claw: " + std::to_string((int)joints.clawAngle);
        glfwSetWindowTitle(window, title.c_str());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
