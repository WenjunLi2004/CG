#define _CRT_SECURE_NO_WARNINGS // VS需要这个来忽略fopen警告
#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <cmath>
#include <iomanip>

// 引入 stb_image 用于加载真实图片
//#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifdef __APPLE__
void glutSolidCylinder(GLdouble radius, GLdouble height, GLint slices, GLint stacks) {
    GLUquadric *quad = gluNewQuadric();
    gluQuadricDrawStyle(quad, GLU_FILL);
    gluCylinder(quad, radius, radius, height, slices, stacks);
    glPushMatrix();
    glRotatef(180, 1, 0, 0);
    gluDisk(quad, 0, radius, slices, 1);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(0, 0, height);
    gluDisk(quad, 0, radius, slices, 1);
    glPopMatrix();
    gluDeleteQuadric(quad);
}
#endif

// ================= 基础结构体 =================
struct Vertex { float x, y, z; };
struct TexCoord { float u, v; };
struct Normal { float nx, ny, nz; };
struct Face {
    int vIndices[3];
    int tIndices[3];
    int nIndices[3];
};

// 模型类
class Model {
public:
    std::vector<Vertex> vertices;
    std::vector<TexCoord> texCoords;
    std::vector<Normal> normals;
    std::vector<Face> faces;
    GLuint displayListId; // 使用显示列表优化性能

    void load(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "无法打开文件: " << filename << std::endl;
            return;
        }

        std::string line;
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string prefix;
            ss >> prefix;

            if (prefix == "v") {
                Vertex v; ss >> v.x >> v.y >> v.z;
                vertices.push_back(v);
            }
            else if (prefix == "vt") {
                TexCoord t; ss >> t.u >> t.v;
                texCoords.push_back(t);
            }
            else if (prefix == "vn") {
                Normal n; ss >> n.nx >> n.ny >> n.nz;
                normals.push_back(n);
            }
            else if (prefix == "f") {
                Face f;
                char slash;
                for (int i = 0; i < 3; ++i) {
                    ss >> f.vIndices[i];
                    // 处理 obj 的 f v/vt/vn 格式
                    if (ss.peek() == '/') {
                        ss.ignore(); // 忽略第一个 /
                        if (ss.peek() != '/') { // 如果不是 //，说明有纹理
                            ss >> f.tIndices[i];
                        }
                        if (ss.peek() == '/') {
                            ss.ignore(); // 忽略第二个 /
                            ss >> f.nIndices[i];
                        }
                    }
                    // OBJ 索引从1开始，我们要减1
                    f.vIndices[i]--;
                    if (f.tIndices[i] > 0) f.tIndices[i]--;
                    if (f.nIndices[i] > 0) f.nIndices[i]--;
                }
                faces.push_back(f);
            }
        }
        file.close();
        
        // 生成显示列表
        displayListId = glGenLists(1);
        glNewList(displayListId, GL_COMPILE);
        drawImmediate();
        glEndList();
        std::cout << "模型加载成功: " << filename << " (面数: " << faces.size() << ")" << std::endl;
    }

    void draw() {
        if (displayListId) glCallList(displayListId);
        else drawImmediate();
    }

private:
    void drawImmediate() {
        glBegin(GL_TRIANGLES);
        for (const auto& face : faces) {
            for (int i = 0; i < 3; ++i) {
                if (!normals.empty() && face.nIndices[i] >= 0) {
                    const auto& n = normals[face.nIndices[i]];
                    glNormal3f(n.nx, n.ny, n.nz);
                }
                if (!texCoords.empty() && face.tIndices[i] >= 0) {
                    const auto& t = texCoords[face.tIndices[i]];
                    glTexCoord2f(t.u, t.v);
                }
                const auto& v = vertices[face.vIndices[i]];
                glVertex3f(v.x, v.y, v.z);
            }
        }
        glEnd();
    }
};

// ================= 全局变量 =================

// 场景模型
Model mdlLamp;
Model mdlCube; // 如果你有cube.obj，或者我们用代码画

// 纹理ID
GLuint texFloor, texWall, texWood;

// 相机与控制
float camAngleX = 0.0f, camAngleY = 20.0f, camDist = 20.0f;
int mouseLeftDown = 0, mouseX = 0, mouseY = 0;

// 机械臂状态
float baseRot = 0.0f;    // 底座旋转
float arm1Rot = 30.0f;   // 大臂角度
float arm2Rot = -45.0f;  // 小臂角度
float clawAngle = 0.0f;  // 爪子开合角度 (0闭合, 30张开)
float clawRot = 0.0f;    // 爪子整体旋转

// 物理与抓取
struct ObjectState {
    float x, y, z;
    bool isCaught;
    float velocityY; // 用于简单的重力下落
};
ObjectState targetObj = { 5.0f, 0.5f, 5.0f, false, 0.0f };

// 机械臂末端坐标 (计算得出)
float clawWorldX, clawWorldY, clawWorldZ;

// 灯光位置 (对应屋顶的灯)
GLfloat lightPos[] = { 0.0f, 9.5f, 0.0f, 1.0f }; // 假设屋顶高10

// PI
const float PI = 3.1415926535f;

// ================= 辅助函数 =================

// 加载纹理
GLuint loadTexture(const char* filename) {
    int width, height, nrChannels;
    unsigned char* data = stbi_load(filename, &width, &height, &nrChannels, 0);
    GLuint textureID = 0;
    if (data) {
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        // 设置纹理环绕方式
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        // 过滤方式
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
        std::cout << "纹理加载成功: " << filename << std::endl;
    } else {
        std::cout << "纹理加载失败: " << filename << std::endl;
    }
    return textureID;
}

// 绘制房间 (包含地板、墙壁、天花板)
void drawRoom() {
    float roomSize = 15.0f; // 房间半径
    float height = 10.0f;

    // 1. 地板
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texFloor);
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glTexCoord2f(0, 0); glVertex3f(-roomSize, 0, roomSize);
    glTexCoord2f(5, 0); glVertex3f(roomSize, 0, roomSize);
    glTexCoord2f(5, 5); glVertex3f(roomSize, 0, -roomSize);
    glTexCoord2f(0, 5); glVertex3f(-roomSize, 0, -roomSize);
    glEnd();

    // 2. 墙壁 (画3面，留一面看进去)
    glBindTexture(GL_TEXTURE_2D, texWall);
    glBegin(GL_QUADS);
    // 后墙
    glNormal3f(0, 0, 1);
    glTexCoord2f(0, 0); glVertex3f(-roomSize, 0, -roomSize);
    glTexCoord2f(2, 0); glVertex3f(roomSize, 0, -roomSize);
    glTexCoord2f(2, 1); glVertex3f(roomSize, height, -roomSize);
    glTexCoord2f(0, 1); glVertex3f(-roomSize, height, -roomSize);
    
    // 左墙
    glNormal3f(1, 0, 0);
    glTexCoord2f(0, 0); glVertex3f(-roomSize, 0, roomSize);
    glTexCoord2f(2, 0); glVertex3f(-roomSize, 0, -roomSize);
    glTexCoord2f(2, 1); glVertex3f(-roomSize, height, -roomSize);
    glTexCoord2f(0, 1); glVertex3f(-roomSize, height, roomSize);

    // 右墙
    glNormal3f(-1, 0, 0);
    glTexCoord2f(0, 0); glVertex3f(roomSize, 0, -roomSize);
    glTexCoord2f(2, 0); glVertex3f(roomSize, 0, roomSize);
    glTexCoord2f(2, 1); glVertex3f(roomSize, height, roomSize);
    glTexCoord2f(0, 1); glVertex3f(roomSize, height, -roomSize);
    glEnd();

    // 3. 天花板 (简单白色)
    glDisable(GL_TEXTURE_2D);
    glColor3f(0.9f, 0.9f, 0.9f);
    glBegin(GL_QUADS);
    glNormal3f(0, -1, 0);
    glVertex3f(-roomSize, height, -roomSize);
    glVertex3f(roomSize, height, -roomSize);
    glVertex3f(roomSize, height, roomSize);
    glVertex3f(-roomSize, height, roomSize);
    glEnd();
    
    // 绘制屋顶的灯具模型
    glPushMatrix();
    glTranslatef(lightPos[0], lightPos[1] - 0.5f, lightPos[2]); // 稍微下来一点
    glScalef(0.1f, 0.1f, 0.1f); // 根据你的OBJ大小调整缩放
    glColor3f(1.0f, 1.0f, 0.8f); // 暖光颜色
    mdlLamp.draw();
    glPopMatrix();
}

// 阴影矩阵
void shadowMatrix(GLfloat shadowMat[4][4], GLfloat groundplane[4], GLfloat lightpos[4]) {
    GLfloat dot;
    dot = groundplane[0] * lightpos[0] + groundplane[1] * lightpos[1] +
          groundplane[2] * lightpos[2] + groundplane[3] * lightpos[3];
    shadowMat[0][0] = dot - lightpos[0] * groundplane[0];
    shadowMat[1][0] = 0.f - lightpos[0] * groundplane[1];
    shadowMat[2][0] = 0.f - lightpos[0] * groundplane[2];
    shadowMat[3][0] = 0.f - lightpos[0] * groundplane[3];
    shadowMat[0][1] = 0.f - lightpos[1] * groundplane[0];
    shadowMat[1][1] = dot - lightpos[1] * groundplane[1];
    shadowMat[2][1] = 0.f - lightpos[1] * groundplane[2];
    shadowMat[3][1] = 0.f - lightpos[1] * groundplane[3];
    shadowMat[0][2] = 0.f - lightpos[2] * groundplane[0];
    shadowMat[1][2] = 0.f - lightpos[2] * groundplane[1];
    shadowMat[2][2] = dot - lightpos[2] * groundplane[2];
    shadowMat[3][2] = 0.f - lightpos[2] * groundplane[3];
    shadowMat[0][3] = 0.f - lightpos[3] * groundplane[0];
    shadowMat[1][3] = 0.f - lightpos[3] * groundplane[1];
    shadowMat[2][3] = 0.f - lightpos[3] * groundplane[2];
    shadowMat[3][3] = dot - lightpos[3] * groundplane[3];
}

// ================= 机械臂绘制与逻辑 =================

// 计算末端位置 (正运动学) - 简易版
// 我们需要知道爪子中心现在在哪里，以便做碰撞检测
void updateClawPosition() {
    // 这是一个近似计算，因为完全的矩阵变换比较复杂
    // 这里我们用三角函数手动追踪坐标
    // 1. 底座旋转影响 x, z
    // 2. 臂长假设：底座高 1.5, 大臂长 4.0, 小臂长 3.0
    
    float radBase = baseRot * PI / 180.0f;
    float radArm1 = arm1Rot * PI / 180.0f;
    // 注意：小臂的旋转是基于大臂的坐标系的，所以角度是累加的 (这里简化处理)
    // 在OpenGL变换中，我们是先转Arm1，再移动，再转Arm2
    float radArm2 = (arm1Rot + arm2Rot) * PI / 180.0f; // 绝对角度

    float L1 = 4.0f; // 大臂长度
    float L2 = 3.0f; // 小臂长度

    // 在 Y-R 平面 (垂直切面) 的投影
    // 起点 (0, 1.5)
    float r1 = L1 * sin(radArm1); // 大臂水平投影
    float y1 = L1 * cos(radArm1); // 大臂垂直投影
    
    float r2 = L2 * sin(radArm2);
    float y2 = L2 * cos(radArm2);

    float R_total = r1 + r2; // 总水平距离
    float Y_total = 1.5f + y1 + y2; // 总高度

    // 转换回 3D 世界坐标
    clawWorldX = R_total * sin(radBase); // 注意：这里sin/cos取决于你的初始朝向
    clawWorldZ = R_total * cos(radBase);
    clawWorldY = Y_total;

    // 修正：上面的数学模型可能和OpenGL的 draw 顺序有出入
    // 最准确的方法是利用 glGetFloatv(GL_MODELVIEW_MATRIX)
    // 但为了作业简单，我们可以直接在 Draw 的时候保存坐标
}

void drawClaw() {
    // 爪子底座
    glutSolidCube(0.8);

    // 左指
    glPushMatrix();
    glTranslatef(0.3f, -0.4f, 0.0f); // 移到边缘
    glRotatef(-clawAngle, 0, 0, 1); // 张开
    glTranslatef(0.0f, -0.4f, 0.0f); // 指长中心
    glScalef(0.1f, 0.8f, 0.4f);
    glutSolidCube(1.0);
    glPopMatrix();

    // 右指
    glPushMatrix();
    glTranslatef(-0.3f, -0.4f, 0.0f);
    glRotatef(clawAngle, 0, 0, 1);
    glTranslatef(0.0f, -0.4f, 0.0f);
    glScalef(0.1f, 0.8f, 0.4f);
    glutSolidCube(1.0);
    glPopMatrix();
}

void drawRobot(bool isShadow) {
    if(!isShadow) glColor3f(0.7f, 0.7f, 0.7f); // 机械臂灰色

    // 1. 底座
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 0.0f);
    glRotatef(-90, 1, 0, 0);
    glutSolidCylinder(1.0, 1.5, 20, 5);
    glPopMatrix();

    // 变换链开始
    glPushMatrix();
    
    // --- 关节1：底座旋转 ---
    glTranslatef(0.0f, 1.5f, 0.0f); 
    glRotatef(baseRot, 0, 1, 0);

        // --- 关节2：肩部 ---
        glutSolidSphere(0.8, 16, 16); // 关节球
        glRotatef(arm1Rot, 0, 0, 1); // 绕Z轴

        // 大臂
        glPushMatrix();
        glTranslatef(0.0f, 2.0f, 0.0f); // 往上长
        glScalef(0.6f, 4.0f, 0.6f);
        glutSolidCube(1.0);
        glPopMatrix();

        // --- 关节3：肘部 ---
        glTranslatef(0.0f, 4.0f, 0.0f);
        glutSolidSphere(0.7, 16, 16);
        glRotatef(arm2Rot, 0, 0, 1); // 小臂弯曲

        // 小臂
        glPushMatrix();
        glTranslatef(0.0f, 1.5f, 0.0f);
        glScalef(0.5f, 3.0f, 0.5f);
        glutSolidCube(1.0);
        glPopMatrix();

        // --- 关节4：手腕与爪子 ---
        glTranslatef(0.0f, 3.0f, 0.0f);
        glRotatef(clawRot, 0, 1, 0); // 爪子自旋

        drawClaw();

        // [核心逻辑]：在这里获取爪子的世界坐标用于物理检测
        if (!isShadow) {
            float modelview[16];
            glGetFloatv(GL_MODELVIEW_MATRIX, modelview);
            // 矩阵第4列就是当前的平移位置 (x, y, z)
            clawWorldX = modelview[12];
            clawWorldY = modelview[13];
            clawWorldZ = modelview[14];
            
            // 如果抓住了物体，在这里绘制物体（跟随爪子移动）
            if (targetObj.isCaught) {
                glPushMatrix();
                glTranslatef(0.0f, -1.0f, 0.0f); // 挂在爪子下面
                glColor3f(1.0f, 0.0f, 0.0f);
                glutSolidCube(1.0); // 或者 drawObj()
                glPopMatrix();
            }
        }
        else if (targetObj.isCaught) {
            // 阴影Pass也要画
            glPushMatrix();
            glTranslatef(0.0f, -1.0f, 0.0f);
            glutSolidCube(1.0); 
            glPopMatrix();
        }

    glPopMatrix();
}

// 绘制目标物体
void drawObject() {
    if (targetObj.isCaught) return; // 如果被抓住了，在drawRobot里画

    glPushMatrix();
    glTranslatef(targetObj.x, targetObj.y, targetObj.z);
    
    // 红色物体
    GLfloat mat_ambient[] = { 0.3f, 0.0f, 0.0f, 1.0f };
    GLfloat mat_diffuse[] = { 0.8f, 0.1f, 0.1f, 1.0f };
    glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
    
    glColor3f(1.0f, 0.0f, 0.0f); 
    glutSolidCube(1.0); // 后续可替换为 mdlCube.draw()
    glPopMatrix();
}

// ================= 主循环 =================

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    
    // 相机
    float radX = camAngleX * PI / 180.0f;
    float radY = camAngleY * PI / 180.0f;
    gluLookAt(camDist * cos(radY) * sin(radX), camDist * sin(radY), camDist * cos(radY) * cos(radX),
              0, 3, 0, 0, 1, 0);

    // 设置光源
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

    // 1. 绘制不透明的房间场景
    drawRoom();

    // 2. 绘制阴影 (使用平面投影)
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glPushMatrix();
    GLfloat shadowMat[4][4];
    GLfloat ground[4] = {0, 1, 0, 0.01}; // 地面平面
    shadowMatrix(shadowMat, ground, lightPos);
    glMultMatrixf((GLfloat*)shadowMat);
    
    glColor4f(0.1f, 0.1f, 0.1f, 0.5f); // 黑色半透明
    drawRobot(true); // 绘制机器人阴影
    if(!targetObj.isCaught) {
        glPushMatrix();
        glTranslatef(targetObj.x, 0.5f, targetObj.z);
        glutSolidCube(1.0);
        glPopMatrix();
    }
    glPopMatrix();
    
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);

    // 3. 绘制物体和机器人
    drawObject();
    drawRobot(false);

    glutSwapBuffers();
}

// 物理碰撞检测
void checkCollision() {
    if (targetObj.isCaught) return; // 已经抓着了就不检侧

    // 计算距离
    float dx = clawWorldX - targetObj.x;
    float dy = (clawWorldY - 1.0f) - targetObj.y; // 爪子中心比末端高，修正一下
    float dz = clawWorldZ - targetObj.z;
    float dist = sqrt(dx*dx + dy*dy + dz*dz);

    // 阈值：假设爪子张开且距离小于 1.5
    if (dist < 1.5f && clawAngle < 10.0f) { // 只有爪子闭合时才触发抓取
        targetObj.isCaught = true;
        std::cout << "抓取成功!" << std::endl;
    }
}

void keyboard(unsigned char key, int x, int y) {
    switch (key) {
    case 'w': if(arm1Rot < 90) arm1Rot += 2.0f; break;
    case 's': if(arm1Rot > -90) arm1Rot -= 2.0f; break;
    case 'q': if(arm2Rot < 90) arm2Rot += 2.0f; break;
    case 'e': if(arm2Rot > -90) arm2Rot -= 2.0f; break;
    case 'a': baseRot += 2.0f; break;
    case 'd': baseRot -= 2.0f; break;
    case 'r': clawRot += 5.0f; break; // 爪子旋转
    
    // 爪子开合 (数字键)
    case '1': clawAngle = 30.0f; // 张开 (放下)
              if(targetObj.isCaught) {
                  targetObj.isCaught = false;
                  // 放下时更新物体坐标为当前爪子下方
                  targetObj.x = clawWorldX;
                  targetObj.z = clawWorldZ;
                  targetObj.y = 0.5f; // 落地
              }
              break; 
    case '2': clawAngle = 0.0f; // 闭合 (尝试抓取)
              checkCollision();
              break;

    case 27: exit(0);
    }
    glutPostRedisplay();
}

void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON) {
        mouseLeftDown = (state == GLUT_DOWN);
        mouseX = x; mouseY = y;
    }
}

void motion(int x, int y) {
    if (mouseLeftDown) {
        camAngleX -= (x - mouseX) * 0.5f;
        camAngleY += (y - mouseY) * 0.5f;
        if(camAngleY > 89) camAngleY = 89;
        if(camAngleY < 5) camAngleY = 5;
        mouseX = x; mouseY = y;
        glutPostRedisplay();
    }
}

void init() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_NORMALIZE);

    // 加载纹理 (请确保文件存在 assets/textures 文件夹下)
    texFloor = loadTexture("assets/textures/floor.jpg");
    texWall = loadTexture("assets/textures/wall.jpg");
    
    // 加载OBJ
    mdlLamp.load("assets/lamp.obj");
    mdlCube.load("assets/toy.obj"); 

    // 设置全局环境光
    GLfloat ambient[] = { 0.3f, 0.3f, 0.3f, 1.0f };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1024, 768);
    glutCreateWindow("Final Project: Robot Arm in Room");

    init();

    glutDisplayFunc(display);
    glutReshapeFunc([](int w, int h) {
        glViewport(0, 0, w, h);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(45, (float)w/h, 0.1, 100);
        glMatrixMode(GL_MODELVIEW);
    });
    glutKeyboardFunc(keyboard);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);

    std::cout << "操作说明:\n WASD: 控制手臂\n Q/E: 控制小臂\n 1: 张开爪子(放下)\n 2: 闭合爪子(抓取)\n 鼠标拖拽: 旋转视角" << std::endl;

    glutMainLoop();
    return 0;
}