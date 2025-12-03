/**
 * Linux OpenGL 2.0 FPS Game (Programmable Pipeline / Shaders)
 * 依赖: freeglut3-dev, libglew-dev, mesa-common-dev
 * 编译需要链接 GLEW: -lGLEW
 */

#include <GL/glew.h> // 必须在 gl.h/glut.h 之前包含
#include <GL/glut.h>
#include <iostream>
#include <vector>
#include <cmath>

// --- GLSL 着色器代码 ---
// OpenGL 2.0 使用 #version 110 或 120
const char* vertexShaderSource = R"(
    #version 120
    
    // 从 C++ 传进来的 Uniform 变量
    uniform vec3 lightPos; 

    // 传递给片段着色器的变量
    varying vec3 normal;
    varying vec3 fragPos;

    void main() {
        // gl_Vertex 是固定管线属性，GL 2.0 允许混合使用
        // 将顶点转换到世界空间
        fragPos = vec3(gl_ModelViewMatrix * gl_Vertex);
        
        // 处理法线 (简单的法线变换)
        normal = gl_NormalMatrix * gl_Normal;
        
        // 内置的 mvp 矩阵变换顶点位置
        gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
    }
)";

const char* fragmentShaderSource = R"(
    #version 120
    
    uniform vec3 objectColor;
    uniform vec3 lightPos;

    varying vec3 normal;
    varying vec3 fragPos;

    void main() {
        // 环境光 (Ambient)
        float ambientStrength = 0.2;
        vec3 ambient = ambientStrength * vec3(1.0, 1.0, 1.0);

        // 漫反射 (Diffuse) - 手动计算光照
        vec3 norm = normalize(normal);
        vec3 lightDir = normalize(lightPos - fragPos);
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * vec3(1.0, 1.0, 1.0);

        // 最终颜色 = (环境光 + 漫反射) * 物体本色
        vec3 result = (ambient + diffuse) * objectColor;
        
        gl_FragColor = vec4(result, 1.0);
    }
)";

// --- 全局变量 ---
GLuint shaderProgram;
GLuint colorLoc, lightPosLoc; // Uniform 位置 ID

// 摄像机变量
float angle = 0.0f, pitch = 0.0f;
float lx = 0.0f, ly = 0.0f, lz = -1.0f;
float x = 0.0f, y = 1.0f, z = 5.0f;
int centerX = 400, centerY = 300;
bool firstMouse = true;

struct Enemy { float x, y, z; bool active; };
std::vector<Enemy> enemies;

// --- Shader 辅助函数 ---
GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    // 检查编译错误
    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cerr << "Shader Compilation Error:\n" << infoLog << std::endl;
    }
    return shader;
}

void initShaders() {
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // 检查链接错误
    int success;
    char infoLog[512];
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cerr << "Program Linking Error:\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // 获取 Uniform 变量的位置 ID，以便后续传值
    colorLoc = glGetUniformLocation(shaderProgram, "objectColor");
    lightPosLoc = glGetUniformLocation(shaderProgram, "lightPos");
}

// --- 游戏逻辑 ---
void updateCamera() {
    lx = sin(angle) * cos(pitch);
    ly = sin(pitch);
    lz = -cos(angle) * cos(pitch);
}

void changeSize(int w, int h) {
    if (h == 0) h = 1;
    float ratio = w * 1.0 / h;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glViewport(0, 0, w, h);
    gluPerspective(45.0f, ratio, 0.1f, 100.0f);
    glMatrixMode(GL_MODELVIEW);
    centerX = w/2; centerY = h/2;
}

void renderScene() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    gluLookAt(x, y, z, x + lx, y + ly, z + lz, 0.0f, 1.0f, 0.0f);

    // 启用我们编写的 Shader 程序
    glUseProgram(shaderProgram);

    // 设置光源位置 (Uniform)
    glUniform3f(lightPosLoc, 0.0f, 10.0f, 0.0f);

    // 1. 绘制地面 (灰色)
    glUniform3f(colorLoc, 0.5f, 0.5f, 0.5f); // 传颜色给 Shader
    glBegin(GL_QUADS);
    glNormal3f(0.0f, 1.0f, 0.0f); // 法线向上
    glVertex3f(-20.0f, 0.0f, -20.0f);
    glVertex3f(-20.0f, 0.0f,  20.0f);
    glVertex3f( 20.0f, 0.0f,  20.0f);
    glVertex3f( 20.0f, 0.0f, -20.0f);
    glEnd();

    // 2. 绘制敌人 (红色)
    for(const auto& e : enemies) {
        if(!e.active) continue;
        
        glPushMatrix();
        glTranslatef(e.x, e.y, e.z);
        
        glUniform3f(colorLoc, 1.0f, 0.2f, 0.2f); // 红色
        glutSolidSphere(0.8f, 20, 20); // GLUT 几何体自带法线数据
        
        glPopMatrix();
    }
    
    // 3. 绘制准星 (不需要光照 Shader，暂时切回固定管线或简单绘制)
    glUseProgram(0); // 0 表示禁用 Shader，使用固定管线绘制 2D UI
    
    glMatrixMode(GL_PROJECTION);
    glPushMatrix(); glLoadIdentity();
    gluOrtho2D(0, glutGet(GLUT_WINDOW_WIDTH), 0, glutGet(GLUT_WINDOW_HEIGHT));
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix(); glLoadIdentity();
    
    glDisable(GL_DEPTH_TEST);
    glColor3f(0.0f, 1.0f, 0.0f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2i(centerX - 10, centerY); glVertex2i(centerX + 10, centerY);
        glVertex2i(centerX, centerY - 10); glVertex2i(centerX, centerY + 10);
    glEnd();
    glEnable(GL_DEPTH_TEST);
    
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW); glPopMatrix();

    glutSwapBuffers();
}

void processKeys(unsigned char key, int xx, int yy) {
    if (key == 27) exit(0);
    float speed = 0.5f;
    if (key == 'w') { x += lx * speed; z += lz * speed; }
    if (key == 's') { x -= lx * speed; z -= lz * speed; }
    if (key == 'a') { x += lz * speed; z -= lx * speed; }
    if (key == 'd') { x -= lz * speed; z += lx * speed; }
}

void processMouse(int mx, int my) {
    if (firstMouse) { centerX = mx; centerY = my; firstMouse = false; return; }
    int dx = mx - centerX; int dy = my - centerY;
    if (dx == 0 && dy == 0) return;
    angle += dx * 0.002f;
    pitch -= dy * 0.002f;
    if(pitch > 1.5f) pitch = 1.5f; if(pitch < -1.5f) pitch = -1.5f;
    updateCamera();
    glutWarpPointer(centerX, centerY);
}

void mouseClick(int button, int state, int mx, int my) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        // 简单的射击逻辑
        for(auto& e : enemies) {
            if(!e.active) continue;
            float ex = e.x - x, ey = e.y - y, ez = e.z - z;
            float dist = sqrt(ex*ex + ey*ey + ez*ez);
            float dot = (lx * (ex/dist) + ly * (ey/dist) + lz * (ez/dist));
            if(dot > 0.98f) { 
                e.active = false; 
                std::cout << "Target Eliminated with Shader Rendering!" << std::endl;
            }
        }
    }
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutCreateWindow("OpenGL 2.0 Shader FPS");

    // === 关键：初始化 GLEW ===
    // 必须在创建窗口之后调用
    GLenum err = glewInit();
    if (GLEW_OK != err) {
        std::cerr << "GLEW Error: " << glewGetErrorString(err) << std::endl;
        return 1;
    }
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

    initShaders(); // 编译和链接着色器

    enemies.push_back({0.0f, 1.0f, -10.0f, true});
    enemies.push_back({-5.0f, 1.0f, -15.0f, true});
    enemies.push_back({5.0f, 1.0f, -8.0f, true});

    glEnable(GL_DEPTH_TEST);
    glutSetCursor(GLUT_CURSOR_NONE);

    glutDisplayFunc(renderScene);
    glutIdleFunc(renderScene);
    glutReshapeFunc(changeSize);
    glutKeyboardFunc(processKeys);
    glutPassiveMotionFunc(processMouse);
    glutMouseFunc(mouseClick);

    glutMainLoop();
    return 0;
}