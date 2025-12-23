#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

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

// ================= 全局变量 =================

// 1. 相机控制变量
float camAngleX = 0.0f;      // 摄像机水平旋转
float camAngleY = 20.0f;     // 摄像机垂直旋转
float camDist = 15.0f;       // 摄像机距离
int mouseLeftDown = 0;       // 鼠标左键状态
int mouseX, mouseY;          // 鼠标位置

// 2. 机械臂状态变量（层级建模核心）
float baseRotate = 0.0f;     // 底座旋转 (键盘 A/D)
float arm1Rotate = 0.0f;     // 大臂旋转 (键盘 W/S)
float arm2Rotate = 0.0f;     // 小臂旋转 (键盘 Q/E)
float clawOpen = 0.5f;       // 爪子开合 (暂未做动画，仅作状态)

// 3. 物体与抓取逻辑
bool isGripping = false;     // 是否正在抓取
float objX = 5.0f;           // 物体在世界坐标的 X 位置
float objZ = 0.0f;           // 物体在世界坐标的 Z 位置
// (注意：如果被抓取，物体坐标将由机械臂决定，这里只存落地位置)

// 4. 纹理 ID
GLuint texGround;

// 5. 光照位置
GLfloat lightPos[] = { 10.0f, 20.0f, 10.0f, 1.0f };

// PI 常量
const float PI = 3.1415926535f;

// ================= 纹理生成函数 =================
// 生成一个简单的黑白棋盘纹理，避免依赖外部图片读取库
void makeCheckImage() {
    const int width = 64;
    const int height = 64;
    GLubyte image[height][width][3];
    int i, j, c;
    for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++) {
            c = ((((i & 0x8) == 0) ^ ((j & 0x8)) == 0)) * 255;
            image[i][j][0] = (GLubyte)c;     // R
            image[i][j][1] = (GLubyte)c;     // G
            image[i][j][2] = (GLubyte)c;     // B
        }
    }
    glGenTextures(1, &texGround);
    glBindTexture(GL_TEXTURE_2D, texGround);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
}

// ================= 阴影矩阵算法 =================
// 创建一个将物体压扁到地面的投影矩阵
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

// ================= 绘图辅助函数 =================

// 画地面
void drawGround() {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texGround);
    
    // 设置材质（地面偏暗，哑光）
    GLfloat mat_ambient[] = { 0.5f, 0.5f, 0.5f, 1.0f };
    GLfloat mat_diffuse[] = { 0.6f, 0.6f, 0.6f, 1.0f };
    GLfloat mat_specular[] = { 0.1f, 0.1f, 0.1f, 1.0f }; 
    GLfloat mat_shininess[] = { 10.0f };
    glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
    glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess);

    glBegin(GL_QUADS);
    glNormal3f(0.0f, 1.0f, 0.0f); // 法线向上
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-20.0f, 0.0f, 20.0f);
    glTexCoord2f(10.0f, 0.0f); glVertex3f(20.0f, 0.0f, 20.0f);
    glTexCoord2f(10.0f, 10.0f); glVertex3f(20.0f, 0.0f, -20.0f);
    glTexCoord2f(0.0f, 10.0f); glVertex3f(-20.0f, 0.0f, -20.0f);
    glEnd();

    glDisable(GL_TEXTURE_2D);
}

// 画被抓取的物体 (茶壶)
void drawTargetObject() {
    // 红色材质
    GLfloat mat_ambient[] = { 0.3f, 0.0f, 0.0f, 1.0f };
    GLfloat mat_diffuse[] = { 0.8f, 0.1f, 0.1f, 1.0f };
    GLfloat mat_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat mat_shininess[] = { 50.0f };
    glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
    glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess);

    glutSolidTeapot(1.0); // 茶壶是图形学经典物体
}

// 画机械臂 (层级建模核心)
// isShadowPass: 如果是画阴影，就不设置材质颜色，全部画黑
void drawRobot(bool isShadowPass) {
    if (!isShadowPass) {
        // 金属材质
        GLfloat mat_ambient[] = { 0.2f, 0.2f, 0.2f, 1.0f };
        GLfloat mat_diffuse[] = { 0.5f, 0.5f, 0.6f, 1.0f };
        GLfloat mat_specular[] = { 0.8f, 0.8f, 0.8f, 1.0f };
        GLfloat mat_shininess[] = { 100.0f };
        glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
        glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
        glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
        glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess);
    }

    // --- 第1层：底座 ---
    glPushMatrix();
    glTranslatef(0.0f, 0.5f, 0.0f); // 抬高一点
    glScalef(2.0f, 1.0f, 2.0f);     // 扁圆柱
    glRotatef(-90, 1, 0, 0);        // 旋转圆柱让它立起来
    glutSolidCylinder(1.0, 1.0, 20, 5); 
    glPopMatrix();

    // --- 旋转控制：底座旋转 ---
    glPushMatrix(); // 保存世界坐标
    glRotatef(baseRotate, 0.0f, 1.0f, 0.0f); // 绕Y轴转

        // --- 第2层：大臂 ---
        glTranslatef(0.0f, 1.5f, 0.0f); // 移动到关节处
        // 绘制关节球
        glutSolidSphere(0.8, 20, 20);
        
        // 旋转大臂
        glRotatef(arm1Rotate, 0.0f, 0.0f, 1.0f); // 绕Z轴转
        
        // 绘制大臂杆
        glPushMatrix();
        glTranslatef(0.0f, 2.0f, 0.0f); // 杆的中心
        glScalef(0.6f, 4.0f, 0.6f);     // 拉长
        glutSolidCube(1.0);
        glPopMatrix();

        // --- 第3层：小臂 ---
        glTranslatef(0.0f, 4.0f, 0.0f); // 移动到大臂末端
        glutSolidSphere(0.6, 20, 20);   // 肘关节

        glRotatef(arm2Rotate, 0.0f, 0.0f, 1.0f); // 小臂旋转

        // 绘制小臂杆
        glPushMatrix();
        glTranslatef(0.0f, 1.5f, 0.0f);
        glScalef(0.5f, 3.0f, 0.5f);
        glutSolidCube(1.0);
        glPopMatrix();

        // --- 第4层：爪子/末端 ---
        glTranslatef(0.0f, 3.0f, 0.0f); // 移动到小臂末端
        
        // 绘制一个简单的爪子手掌
        glPushMatrix();
        glScalef(0.8f, 0.2f, 0.8f);
        glutSolidCube(1.0);
        glPopMatrix();

        // [关键逻辑]：如果正在抓取，物体就画在这里（成为爪子的子节点）
        if (isGripping && !isShadowPass) {
            glPushMatrix();
            glTranslatef(0.0f, 1.5f, 0.0f); // 调整物体在手中的位置
            glRotatef(90, 0, 1, 0); // 调整一下茶壶朝向
            drawTargetObject();
            glPopMatrix();
        }
        else if (isGripping && isShadowPass) {
            // 阴影阶段也要画被抓住的物体
             glPushMatrix();
            glTranslatef(0.0f, 1.5f, 0.0f); 
            glRotatef(90, 0, 1, 0); 
            drawTargetObject();
            glPopMatrix();
        }

    glPopMatrix(); // 结束底座旋转层级
}

// ================= 主渲染函数 =================
void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 1. 设置摄像机
    glLoadIdentity();
    // 将球坐标转换为直角坐标，实现绕中心旋转
    float radX = camAngleX * PI / 180.0f;
    float radY = camAngleY * PI / 180.0f;
    float cx = camDist * cos(radY) * sin(radX);
    float cy = camDist * sin(radY);
    float cz = camDist * cos(radY) * cos(radX);
    gluLookAt(cx, cy, cz, 0.0f, 3.0f, 0.0f, 0.0f, 1.0f, 0.0f);

    // 2. 设置光源
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

    // 3. 绘制阴影 (Render Pass 1)
    glDisable(GL_LIGHTING); // 关闭光照，用纯黑画阴影
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);     // 开启混合用于半透明阴影
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glPushMatrix();
    GLfloat shadowMat[4][4];
    GLfloat groundPlane[4] = { 0.0f, 1.0f, 0.0f, 0.01f }; // 地面方程 y=0 (稍微抬高0.01防止重叠闪烁)
    shadowMatrix(shadowMat, groundPlane, lightPos);
    glMultMatrixf((GLfloat*)shadowMat); // 应用投影矩阵

    glColor4f(0.1f, 0.1f, 0.1f, 0.5f); // 黑色半透明阴影
    
    drawRobot(true); // 绘制机器人阴影
    
    // 如果物体没被抓取，它在地上的阴影要单独画
    if (!isGripping) {
        glPushMatrix();
        glTranslatef(objX, 0.0f, objZ); // 地面上的物体只平移
        glTranslatef(0.0f, 0.8f, 0.0f); // 茶壶中心修正
        drawTargetObject(); 
        glPopMatrix();
    }
    glPopMatrix();

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);

    // 4. 绘制真实场景 (Render Pass 2)
    
    // A. 地面
    drawGround();

    // B. 机器人 (如果抓住了物体，物体会在 drawRobot 内部绘制)
    drawRobot(false);

    // C. 地面上的物体 (如果没被抓住，需要单独在世界坐标绘制)
    if (!isGripping) {
        glPushMatrix();
        glTranslatef(objX, 0.8f, objZ); // y=0.8 让茶壶坐在地上
        drawTargetObject();
        glPopMatrix();
    }

    glutSwapBuffers();
}

// ================= 交互回调函数 =================

void reshape(int w, int h) {
    if (h == 0) h = 1;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, (float)w / h, 0.1f, 100.0f);
    glMatrixMode(GL_MODELVIEW);
}

// 键盘控制
void keyboard(unsigned char key, int x, int y) {
    switch (key) {
    // 机械臂控制
    case 'a': baseRotate += 5.0f; break;  // 底座左转
    case 'd': baseRotate -= 5.0f; break;  // 底座右转
    case 'w': if(arm1Rotate < 45) arm1Rotate += 5.0f; break;   // 大臂前倾
    case 's': if(arm1Rotate > -45) arm1Rotate -= 5.0f; break;  // 大臂后仰
    case 'q': if(arm2Rotate < 90) arm2Rotate += 5.0f; break;   // 小臂弯曲
    case 'e': if(arm2Rotate > -90) arm2Rotate -= 5.0f; break;  // 小臂伸直
    
    // 抓取逻辑
    case ' ': // 空格键
        if (isGripping) {
            // 放下物体：简化的逻辑，放在当前机械臂前方的某个位置
            // 这里为了数学简单，我们假设放下时物体回到一个固定半径处，或者你可以做更复杂的正运动学解算
            // 为了作业演示稳定，我们这里简单地让物体落在原位置附近，或者重置
            // 更好的做法：这里我们直接让它掉在地上，不改变 X Z (简化)
            isGripping = false;
            // 简单的“瞬移”逻辑：放下时，物体瞬移到底座前方，模拟放置效果
            float rad = baseRotate * PI / 180.0f;
            objX = sin(rad) * 6.0f; // 放在距离底座6.0的地方
            objZ = cos(rad) * 6.0f;
        } else {
            // 抓取检测：简单判断距离（作弊模式：只要按空格就吸过来）
            // 实际作业可以加一个距离判断: if (distance < threshold) ...
            isGripping = true;
        }
        break;

    case 27: exit(0); break; // ESC 退出
    }
    glutPostRedisplay();
}

// 鼠标处理（控制视角）
void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON) {
        if (state == GLUT_DOWN) {
            mouseLeftDown = 1;
            mouseX = x;
            mouseY = y;
        } else {
            mouseLeftDown = 0;
        }
    }
}

void motion(int x, int y) {
    if (mouseLeftDown) {
        camAngleX -= (x - mouseX) * 0.5f;
        camAngleY += (y - mouseY) * 0.5f;
        
        // 限制垂直角度，防止翻转
        if (camAngleY > 89.0f) camAngleY = 89.0f;
        if (camAngleY < 5.0f) camAngleY = 5.0f;

        mouseX = x;
        mouseY = y;
        glutPostRedisplay();
    }
}

// 初始化
void init() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_NORMALIZE); // 自动归一化法线
    
    // 设置环境光
    GLfloat ambientLight[] = { 0.2f, 0.2f, 0.2f, 1.0f };
    GLfloat diffuseLight[] = { 0.8f, 0.8f, 0.8f, 1.0f };
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambientLight);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLight);

    makeCheckImage(); // 生成纹理
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // 深色背景，显着有科技感
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutCreateWindow("Robot Arm Final Project");

    init();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);

    printf("=========================================\n");
    printf("图形学大作业控制说明:\n");
    printf("[鼠标左键拖拽] : 旋转视角\n");
    printf("[W / S]       : 控制大臂前后\n");
    printf("[Q / E]       : 控制小臂弯曲\n");
    printf("[A / D]       : 控制底座旋转\n");
    printf("[Space 空格]   : 抓取 / 放下茶壶\n");
    printf("=========================================\n");

    glutMainLoop();
    return 0;
}