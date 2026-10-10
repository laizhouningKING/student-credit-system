# 学生学分管理系统 —— Makefile
# 用法:
#   make          编译，生成可执行文件 scms（Windows 下为 scms.exe）
#   make run      编译并运行
#   make clean    删除编译产物

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2 -finput-charset=UTF-8 -fexec-charset=UTF-8
SRC       = src/main.cpp

# Windows(MinGW) 生成 .exe，其他平台不加后缀
ifeq ($(OS),Windows_NT)
    TARGET = scms.exe
else
    TARGET = scms
endif

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f scms scms.exe