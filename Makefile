# 学生学分管理系统 —— Makefile
# 用法:
#   make          编译，生成可执行文件 scms（Windows 下为 scms.exe）
#   make run      编译并运行
#   make clean    删除编译产物

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2 -finput-charset=UTF-8 -fexec-charset=UTF-8
SRC       = src/main.cpp

# Windows(MinGW) 生成 .exe，并改用 del 清理；其他平台用 rm
ifeq ($(OS),Windows_NT)
    TARGET   = scms.exe
    RM       = del /Q
    RM_FILES = scms.exe scms 2>NUL
else
    TARGET   = scms
    RM       = rm -f
    RM_FILES = scms scms.exe
endif

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

clean:
	-$(RM) $(RM_FILES)