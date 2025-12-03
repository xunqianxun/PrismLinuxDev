# 编译器
CXX = g++

# 编译选项
# -Wall: 显示所有警告
# -O2:   开启二级优化
CXXFLAGS = -Wall -O2

# 链接选项 (Libraries)
# -lGLEW:  OpenGL Extension Wrangler (处理 Shader 必须)
# -lglut:  FreeGLUT (窗口管理)
# -lGLU:   OpenGL Utility Library
# -lGL:    OpenGL Core Library
# -lm:     Math Library
LDFLAGS = -lGLEW -lglut -lGLU -lGL -lm

# 目标文件名
TARGET = fps_game

# 源文件名
SRC = main.cpp

# 默认目标 (输入 make 时执行)
all: $(TARGET)

# 编译规则
$(TARGET): $(SRC)
	@echo "正在编译 $(TARGET) ..."
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)
	@echo "编译成功！运行 ./$(TARGET) 启动游戏"

# 运行目标 (输入 make run 时执行)
run: $(TARGET)
	./$(TARGET)

# 清理目标 (输入 make clean 时执行)
clean:
	rm -f $(TARGET)

# 依赖安装帮助 (输入 make install_deps 时显示)
install_deps:
	@echo "正在为 Fedora 安装依赖..."
	sudo dnf install freeglut-devel glew-devel mesa-libGL-devel mesa-libGLU-devel gcc-c++
	@echo "依赖安装完成。"

.PHONY: all clean run install_deps