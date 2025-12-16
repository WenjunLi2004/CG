#include "Angel.h"
#include "TriMesh.h"
#include "Camera.h"
#include "MeshPainter.h"

#include <vector>
#include <string>

/*
实验补充1 层级建模-机械手臂

要点：
- 层级模型中子节点的变换由父节点传递：子模型矩阵 = 父模型矩阵 × 子局部矩阵
- 正确选择旋转枢轴（关节）：先将坐标系移动到关节位置，再旋转，最后进行几何居中与缩放
- 绘制顺序采用深度优先：底座 → 大臂 → 小臂

文件概览：
- base()：底座的几何居中与缩放，并在父矩阵下绘制
- upper_arm()：大臂的几何居中与缩放，旋转在 display() 的父矩阵中完成
- lower_arm()：小臂的几何居中与缩放，父矩阵在 display() 中先平移至大小臂关节再旋转
- display()：构建底座/大臂/小臂的层级变换并调用绘制
- 键盘交互：1/2/3 选择部件，A/S 调整当前部件角度；u/i/o 调整相机
*/

#define White	glm::vec3(1.0, 1.0, 1.0)
#define Yellow	glm::vec3(1.0, 1.0, 0.0)
#define Green	glm::vec3(0.0, 1.0, 0.0)
#define Cyan	glm::vec3(0.0, 1.0, 1.0)
#define Magenta	glm::vec3(1.0, 0.0, 1.0)
#define Red		glm::vec3(1.0, 0.0, 0.0)
#define Black	glm::vec3(0.0, 0.0, 0.0)
#define Blue	glm::vec3(0.0, 0.0, 1.0)
#define Brown	glm::vec3(0.5, 0.5, 0.5)


int WIDTH = 600;
int HEIGHT = 600;

int mainWindow;

TriMesh* cube = new TriMesh();

Camera* camera = new Camera();
Light* light = new Light();
MeshPainter *painter = new MeshPainter();

// 这个用来回收和删除我们创建的物体对象
std::vector<TriMesh *> meshList;


// 关节角
enum{
    Base      = 0,
    LowerArm  = 1,
    UpperArm  = 2,
    NumAngles = 3
};
int Axis = Base;
GLfloat Theta[NumAngles] = {0.0};

// 菜单选项值
const int Quit = 4;

// 尺寸参数
const GLfloat BASE_HEIGHT      = 0.2;
const GLfloat BASE_WIDTH       = 0.5;
const GLfloat UPPER_ARM_HEIGHT = 0.3;
const GLfloat UPPER_ARM_WIDTH  = 0.2;
const GLfloat LOWER_ARM_HEIGHT = 0.4;
const GLfloat LOWER_ARM_WIDTH  = 0.1;

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}

// 绘制底座
void base(glm::mat4 modelView)
{
    // 局部几何变换：将单位立方体抬至底座高度中点并按底座尺寸缩放
    // 最终模型矩阵为 parent(modelView) * instance，实现父子层级传递
	glm::mat4 instance = glm::mat4(1.0);
	instance = glm::translate(instance, glm::vec3(0.0, BASE_HEIGHT / 2, 0.0));
	instance = glm::scale(instance, glm::vec3(BASE_WIDTH, BASE_HEIGHT, BASE_WIDTH));

	// 绘制，由于我们只有一个立方体数据，所以这里可以直接指定绘制painter中存储的第0个立方体
	painter->drawMesh(0, modelView * instance, light, camera);
	
}


// 绘制大臂
void upper_arm(glm::mat4 modelView)
{
    // 局部几何变换：将单位立方体上移半高，使下端与关节对齐；再按大臂尺寸缩放
    // 大臂的绕 z 轴旋转在 display() 的父矩阵中实现，此处仅负责几何居中与缩放
    glm::mat4 instance = glm::mat4(1.0);
    instance = glm::translate(instance, glm::vec3(0.0, UPPER_ARM_HEIGHT / 2, 0.0));
    instance = glm::scale(instance, glm::vec3(UPPER_ARM_WIDTH, UPPER_ARM_HEIGHT, UPPER_ARM_WIDTH));

    painter->drawMesh(0, modelView * instance, light, camera);

}


// 绘制小臂
void lower_arm(glm::mat4 modelView)
{
    // 局部几何变换：将单位立方体上移半高，使下端与大小臂关节对齐；再按小臂尺寸缩放
    // 小臂的父矩阵在 display() 中先从大臂末端平移到关节位置，再执行绕 z 轴旋转
    glm::mat4 instance = glm::mat4(1.0);
    instance = glm::translate(instance, glm::vec3(0.0, LOWER_ARM_HEIGHT / 2, 0.0));
    instance = glm::scale(instance, glm::vec3(LOWER_ARM_WIDTH, LOWER_ARM_HEIGHT, LOWER_ARM_WIDTH));

    painter->drawMesh(0, modelView * instance, light, camera);
    
}


void init()
{
	std::string vshader, fshader;
	// 指定顶点/片元着色器；示例使用顶点颜色，不使用纹理图片
	vshader = "shaders/vshader.glsl";
	fshader = "shaders/fshader.glsl";

	// 创建基础立方体网格（底座/大臂/小臂均复用）
	cube->setNormalize(false);
	cube->generateCube();
	cube->setTranslation(glm::vec3(0.0, 0.0, 0.0));
	cube->setRotation(glm::vec3(0.0, 0.0, 0.0));
	cube->setScale(glm::vec3(1.0, 1.0, 1.0));
	// 添加到绘制器并绑定着色器；纹理路径留空
	painter->addMesh(cube, "Cube", "", vshader, fshader);
	meshList.push_back(cube);
	
	glClearColor(1.0, 1.0, 1.0, 1.0);
}


void display()
{

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// 1) 底座父矩阵：整体下移以居中，底座绕 y 轴旋转
	glm::mat4 modelView = glm::mat4(1.0);
	modelView = glm::translate(modelView, glm::vec3(0.0, -BASE_HEIGHT, 0.0));// 稍微下移一下，让机器人整体居中在原点
	modelView = glm::rotate(modelView, glm::radians(Theta[Base]), glm::vec3(0.0, 1.0, 0.0));// 底座旋转矩阵
	base(modelView); // 首先绘制底座（层级根）

	// 2) 大臂父矩阵：以底座上端关节为枢轴，绕 z 轴旋转
	glm::mat4 upper = modelView;
	upper = glm::rotate(upper, glm::radians(Theta[UpperArm]), glm::vec3(0.0, 0.0, 1.0));
	// 调用大臂绘制（其内部完成几何的居中与缩放）
	upper_arm(upper);

	// 3) 小臂父矩阵：先沿 y 轴平移到大小臂关节（大臂末端），再绕 z 轴旋转
	glm::mat4 lower = upper;
	lower = glm::translate(lower, glm::vec3(0.0, UPPER_ARM_HEIGHT, 0.0));
	lower = glm::rotate(lower, glm::radians(Theta[LowerArm]), glm::vec3(0.0, 0.0, 1.0));
	// 调用小臂绘制（其内部完成几何的居中与缩放）
	lower_arm(lower); 


}


void printHelp()
{
	std::cout << "Keyboard Usage" << std::endl;
	std::cout <<
		"[Window]" << std::endl <<
		"ESC:		Exit" << std::endl <<
		"h:		Print help message" << std::endl <<
		std::endl <<

		"[Part]" << std::endl <<
		"1:		Base" << std::endl <<
		"2:		LowerArm" << std::endl <<
		"3:		UpperArm" << std::endl <<
		std::endl <<

		"[Model]" << std::endl <<
		"a/A:	Increase rotate angle" << std::endl <<
		"s/S:	Decrease rotate angle" << std::endl <<

		std::endl <<
		"[Camera]" << std::endl <<
		"SPACE:		Reset camera parameters" << std::endl <<
		"u/U:		Increase/Decrease the rotate angle" << std::endl <<
		"i/I:		Increase/Decrease the up angle" << std::endl <<
		"o/O:		Increase/Decrease the camera radius" << std::endl << std::endl;
		
}


void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
	float tmp;
	glm::vec4 ambient;
	if (action == GLFW_PRESS) {
		switch (key)
		{
		case GLFW_KEY_ESCAPE: exit(EXIT_SUCCESS); break;
		case GLFW_KEY_Q: exit(EXIT_SUCCESS); break;
		// 选择当前可操作的层级节点（底座/大臂/小臂）
		case GLFW_KEY_1: Axis = Base; break;
		case GLFW_KEY_2: Axis = UpperArm; break;
		case GLFW_KEY_3: Axis = LowerArm; break;
		// A/S 以 5° 步进增减当前 Axis 对应的关节角 Theta
		case GLFW_KEY_A: 
			Theta[Axis] += 5.0;
			if (Theta[Axis] > 360.0)
				Theta[Axis] -= 360.0;
			break;
		case GLFW_KEY_S:
			Theta[Axis] -= 5.0;
			if (Theta[Axis] < 0.0)
				Theta[Axis] += 360.0;
			break;
		default:
			camera->keyboard(key, action, mode); // 其它按键用于调整观察视角（u/U, i/I, o/O）
			break;
		}
	}
}


void cleanData() {
	delete camera;
	camera = NULL;

	delete light;
	light = NULL;

	painter->cleanMeshes();

	delete painter;
	painter = NULL;
	
	for (int i=0; i<meshList.size(); i++) {
		delete meshList[i];
	}
	meshList.clear();
}


void framebuffer_size_callback(GLFWwindow* window, int width, int height);

int main(int argc, char **argv)
{
	glfwInit();
	// 配置 GLFW 的 OpenGL 上下文版本与属性
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	// 设置多重采样，具体的数值可以按照显示效果进行调整。
	glfwWindowHint(GLFW_SAMPLES, 100);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
	// 创建窗口并注册输入与窗口大小回调
	GLFWwindow* window = glfwCreateWindow(600, 600, "2023150001-李文俊-实验补充一", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSetKeyCallback(window, key_callback);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}
	init();        // 资源初始化：着色器、立方体网格、绘制器
	printHelp();   // 打印键盘使用说明
	glEnable(GL_DEPTH_TEST); // 开启深度测试保证前后遮挡关系
	glEnable(GL_BLEND);      // 开启混合（如需透明等效果）
	while (!glfwWindowShouldClose(window))
	{
		display();
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
	cleanData();
	return 0;
}
