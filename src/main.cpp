// ============================================================
//  学生学分管理系统 (Student Credit Management System)
//  一个零依赖的 C++17 控制台程序：学生 / 课程 / 成绩 / 学分统计
//
//  编译:  g++ -std=c++17 -O2 -o scms src/main.cpp
//  运行:  ./scms          （数据默认保存在当前目录的 data.txt）
//         ./scms my.txt  （也可以指定数据文件名）
// ============================================================
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

// ============================================================
//  一、数据结构
// ============================================================

// 课程：课程号 + 课程名 + 学分
struct Course {
    std::string id;
    std::string name;
    double      credit = 0.0;
};

// 学生：学号 + 姓名 + 专业
struct Student {
    std::string id;
    std::string name;
    std::string major;
};

// 成绩：某学生某门课的百分制成绩（一条记录 = 一对 学生-课程）
struct Grade {
    std::string studentId;
    std::string courseId;
    double      score = 0.0;
};

// 学分统计结果
struct CreditReport {
    double earnedCredits = 0.0; // 已获学分：及格课程的学分之和
    double totalCredits  = 0.0; // 已修学分：所有已录入成绩课程的学分之和
    double gpa           = 0.0; // 学分加权平均绩点（4.0 制）
    double averageScore  = 0.0; // 学分加权平均分
    int    courseCount   = 0;   // 已录入成绩的门数
    int    failedCount   = 0;   // 不及格门数
};

// ============================================================
//  二、CreditManager：核心管理类
// ============================================================
class CreditManager {
public:
    // ---------- 学生 ----------
    bool addStudent(const std::string& id, const std::string& name, const std::string& major) {
        if (trim(id).empty() || trim(name).empty()) return false;
        if (findStudent(id) != nullptr) return false; // 学号已存在
        students_.push_back(Student{trim(id), trim(name), trim(major)});
        return true;
    }

    bool removeStudent(const std::string& id) {
        const auto it = std::find_if(students_.begin(), students_.end(),
                                     [&](const Student& s) { return s.id == id; });
        if (it == students_.end()) return false;
        students_.erase(it);
        // 级联删除该学生的所有成绩
        grades_.erase(std::remove_if(grades_.begin(), grades_.end(),
                                     [&](const Grade& g) { return g.studentId == id; }),
                      grades_.end());
        return true;
    }

    Student* findStudent(const std::string& id) {
        for (auto& s : students_)
            if (s.id == id) return &s;
        return nullptr;
    }
    const Student* findStudent(const std::string& id) const {
        for (const auto& s : students_)
            if (s.id == id) return &s;
        return nullptr;
    }
    const std::vector<Student>& students() const { return students_; }

    // ---------- 课程 ----------
    bool addCourse(const std::string& id, const std::string& name, double credit) {
        if (trim(id).empty() || trim(name).empty()) return false;
        if (!isValidCredit(credit)) return false;
        if (findCourse(id) != nullptr) return false; // 课程号已存在
        courses_.push_back(Course{trim(id), trim(name), credit});
        return true;
    }

    bool removeCourse(const std::string& id) {
        const auto it = std::find_if(courses_.begin(), courses_.end(),
                                     [&](const Course& c) { return c.id == id; });
        if (it == courses_.end()) return false;
        courses_.erase(it);
        // 级联删除该课程的所有成绩
        grades_.erase(std::remove_if(grades_.begin(), grades_.end(),
                                     [&](const Grade& g) { return g.courseId == id; }),
                      grades_.end());
        return true;
    }

    Course* findCourse(const std::string& id) {
        for (auto& c : courses_)
            if (c.id == id) return &c;
        return nullptr;
    }
    const Course* findCourse(const std::string& id) const {
        for (const auto& c : courses_)
            if (c.id == id) return &c;
        return nullptr;
    }
    const std::vector<Course>& courses() const { return courses_; }

    // ---------- 成绩 ----------
    bool setScore(const std::string& studentId, const std::string& courseId, double score) {
        if (findStudent(studentId) == nullptr) return false; // 学生不存在
        if (findCourse(courseId) == nullptr) return false;   // 课程不存在
        if (!isValidScore(score)) return false;              // 成绩必须在 0~100
        for (auto& g : grades_) {
            if (g.studentId == studentId && g.courseId == courseId) {
                g.score = score; // 已存在则覆盖（可用于重修 / 补考）
                return true;
            }
        }
        grades_.push_back(Grade{studentId, courseId, score});
        return true;
    }

    bool removeScore(const std::string& studentId, const std::string& courseId) {
        const auto it = std::find_if(grades_.begin(), grades_.end(), [&](const Grade& g) {
            return g.studentId == studentId && g.courseId == courseId;
        });
        if (it == grades_.end()) return false;
        grades_.erase(it);
        return true;
    }

    const Grade* findGrade(const std::string& studentId, const std::string& courseId) const {
        for (const auto& g : grades_)
            if (g.studentId == studentId && g.courseId == courseId) return &g;
        return nullptr;
    }

    std::vector<Grade> gradesOf(const std::string& studentId) const {
        std::vector<Grade> out;
        for (const auto& g : grades_)
            if (g.studentId == studentId) out.push_back(g);
        return out;
    }

    // ---------- 学分统计 ----------
    CreditReport report(const std::string& studentId) const {
        CreditReport r;
        double weightedGpa   = 0.0; // Σ(学分 × 绩点)
        double weightedScore = 0.0; // Σ(学分 × 成绩)

        for (const auto& g : gradesOf(studentId)) {
            const Course* c = findCourse(g.courseId);
            if (c == nullptr) continue; // 课程已删除，跳过脏数据

            ++r.courseCount;
            r.totalCredits += c->credit;
            weightedGpa    += c->credit * scoreToGpa(g.score);
            weightedScore  += c->credit * g.score;

            if (g.score >= 60.0)
                r.earnedCredits += c->credit; // 及格才计入已获学分
            else
                ++r.failedCount;
        }

        if (r.totalCredits > 0.0) {
            r.gpa          = weightedGpa / r.totalCredits;
            r.averageScore = weightedScore / r.totalCredits;
        }
        return r;
    }

    // 学分排行榜：按已获学分降序，学分相同按学号升序
    std::vector<std::pair<std::string, double>> ranking() const {
        std::vector<std::pair<std::string, double>> out;
        out.reserve(students_.size());
        for (const auto& s : students_)
            out.emplace_back(s.id, report(s.id).earnedCredits);

        std::sort(out.begin(), out.end(), [](const std::pair<std::string, double>& a,
                                            const std::pair<std::string, double>& b) {
            if (a.second != b.second) return a.second > b.second;
            return a.first < b.first;
        });
        return out;
    }

    // ---------- 文件持久化 ----------
    // 纯文本格式，便于手工编辑与版本控制：
    //   STUDENT <学号> <专业> <姓名>
    //   COURSE  <课程号> <学分> <课程名>
    //   GRADE   <学号> <课程号> <成绩>
    bool load(const std::string& file) {
        std::ifstream in(file);
        if (!in.is_open()) return false;

        students_.clear();
        courses_.clear();
        grades_.clear();

        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream iss(line);
            std::string tag;
            iss >> tag;

            if (tag == "STUDENT") {
                std::string id, major, name;
                iss >> id >> major;
                std::getline(iss, name);
                addStudent(id, trim(name), major);
            } else if (tag == "COURSE") {
                std::string id, name;
                double credit = 0.0;
                iss >> id >> credit;
                std::getline(iss, name);
                addCourse(id, trim(name), credit);
            } else if (tag == "GRADE") {
                std::string sid, cid;
                double score = 0.0;
                if (iss >> sid >> cid >> score) {
                    bool replaced = false;
                    for (auto& g : grades_) {
                        if (g.studentId == sid && g.courseId == cid) {
                            g.score = score;
                            replaced = true;
                            break;
                        }
                    }
                    if (!replaced) grades_.push_back(Grade{sid, cid, score});
                }
            }
        }
        return true;
    }

    bool save(const std::string& file) const {
        std::ofstream out(file, std::ios::trunc);
        if (!out.is_open()) return false;

        out << "# 学生学分管理系统数据文件\n";
        out << "# STUDENT <学号> <专业> <姓名>\n";
        out << "# COURSE  <课程号> <学分> <课程名>\n";
        out << "# GRADE   <学号> <课程号> <成绩>\n";

        for (const auto& s : students_)
            out << "STUDENT " << s.id << ' ' << s.major << ' ' << s.name << '\n';
        for (const auto& c : courses_)
            out << "COURSE " << c.id << ' '
                << std::fixed << std::setprecision(1) << c.credit << ' ' << c.name << '\n';
        for (const auto& g : grades_)
            out << "GRADE " << g.studentId << ' ' << g.courseId << ' '
                << std::fixed << std::setprecision(1) << g.score << '\n';
        return true;
    }

    // ---------- 工具函数 ----------
    // 4.0 制绩点换算：90 分及以上 4.0，60 分及格 1.0，不及格 0
    static double scoreToGpa(double score) {
        if (score >= 90.0) return 4.0;
        if (score >= 85.0) return 3.7;
        if (score >= 82.0) return 3.3;
        if (score >= 78.0) return 3.0;
        if (score >= 75.0) return 2.7;
        if (score >= 72.0) return 2.3;
        if (score >= 68.0) return 2.0;
        if (score >= 64.0) return 1.5;
        if (score >= 60.0) return 1.0;
        return 0.0;
    }

    static bool isValidScore(double score)   { return score >= 0.0 && score <= 100.0; }
    static bool isValidCredit(double credit) { return credit > 0.0 && credit <= 20.0; }

private:
    std::vector<Student> students_;
    std::vector<Course>  courses_;
    std::vector<Grade>   grades_;

    // 去掉字符串首尾空白
    static std::string trim(const std::string& s) {
        const char* ws = " \t\r\n";
        const std::size_t b = s.find_first_not_of(ws);
        if (b == std::string::npos) return "";
        const std::size_t e = s.find_last_not_of(ws);
        return s.substr(b, e - b + 1);
    }
};

// ============================================================
//  三、控制台交互界面
// ============================================================

// 输入流是否已经结束（EOF）。用于管道 / 重定向输入时避免死循环。
static bool g_inputClosed = false;

// 让 Windows 控制台正确显示 UTF-8 中文
static void setupConsole() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    std::cout << std::fixed << std::setprecision(2);
}

// 读取一个整数：输入非法时提示并重试；输入流结束返回 -1。
static int readInt(const std::string& prompt) {
    while (true) {
        std::cout << prompt;
        std::string line;
        if (!std::getline(std::cin, line)) { g_inputClosed = true; return -1; }
        try {
            std::size_t pos = 0;
            const int v = std::stoi(line, &pos);
            while (pos < line.size() && std::isspace(static_cast<unsigned char>(line[pos]))) ++pos;
            if (pos == line.size()) return v;
        } catch (...) {
            // 落到下面统一提示
        }
        std::cout << "  [!] 请输入有效的整数。\n";
    }
}

// 读取一个浮点数：输入非法时提示并重试；输入流结束返回 0。
static double readDouble(const std::string& prompt) {
    while (true) {
        std::cout << prompt;
        std::string line;
        if (!std::getline(std::cin, line)) { g_inputClosed = true; return 0.0; }
        try {
            std::size_t pos = 0;
            const double v = std::stod(line, &pos);
            while (pos < line.size() && std::isspace(static_cast<unsigned char>(line[pos]))) ++pos;
            if (pos == line.size()) return v;
        } catch (...) {
            // 落到下面统一提示
        }
        std::cout << "  [!] 请输入有效的数字。\n";
    }
}

// 读取一行文本（允许包含空格）；输入流结束时返回空串
static std::string readLine(const std::string& prompt) {
    std::cout << prompt;
    std::string line;
    if (!std::getline(std::cin, line)) { g_inputClosed = true; return ""; }
    return line;
}

static void printLine(char c = '-', int n = 62) {
    std::cout << std::string(static_cast<std::size_t>(n), c) << '\n';
}

static void pause() {
    std::cout << "\n按回车键返回主菜单...";
    std::string dummy;
    if (!std::getline(std::cin, dummy)) g_inputClosed = true;
}

static void showMenu() {
    std::cout << "\n";
    printLine('=');
    std::cout << "            学生学分管理系统  v1.0\n";
    printLine('=');
    std::cout << "  1. 添加学生            2. 显示全部学生\n";
    std::cout << "  3. 删除学生            4. 添加课程\n";
    std::cout << "  5. 显示全部课程        6. 删除课程\n";
    std::cout << "  7. 录入 / 修改成绩     8. 查询学生成绩单\n";
    std::cout << "  9. 学分排行榜         10. 载入演示数据\n";
    std::cout << " 11. 保存数据            0. 退出\n";
    printLine('-');
}

// 说明：表格中的中文列用制表符对齐（中文占 2 个显示宽度，
// std::setw 按字节数补齐会导致列错位），数字列仍用 setw。

// 1. 添加学生
static void uiAddStudent(CreditManager& cm) {
    printLine();
    std::cout << "【添加学生】\n";
    const std::string id    = readLine("  学号: ");
    const std::string name  = readLine("  姓名: ");
    const std::string major = readLine("  专业: ");
    if (cm.addStudent(id, name, major))
        std::cout << "  [OK] 学生 " << id << " 添加成功。\n";
    else
        std::cout << "  [X] 添加失败：学号/姓名不能为空，或学号已存在。\n";
    pause();
}

// 2. 显示全部学生
static void uiListStudents(const CreditManager& cm) {
    printLine();
    std::cout << "【学生列表】共 " << cm.students().size() << " 人\n";
    printLine();
    std::cout << std::left << std::setw(12) << "学号" << "姓名\t\t专业\t\t\t已获学分\t已修门数\n";
    printLine();
    for (const auto& s : cm.students()) {
        const CreditReport r = cm.report(s.id);
        std::cout << std::left << std::setw(12) << s.id << s.name << "\t\t" << s.major
                  << "\t\t\t" << r.earnedCredits << "\t\t" << r.courseCount << '\n';
    }
    pause();
}

// 3. 删除学生
static void uiRemoveStudent(CreditManager& cm) {
    printLine();
    std::cout << "【删除学生】\n";
    const std::string id = readLine("  学号: ");
    if (cm.removeStudent(id))
        std::cout << "  [OK] 学生 " << id << " 及其成绩已删除。\n";
    else
        std::cout << "  [X] 未找到学号为 " << id << " 的学生。\n";
    pause();
}

// 4. 添加课程
static void uiAddCourse(CreditManager& cm) {
    printLine();
    std::cout << "【添加课程】\n";
    const std::string id   = readLine("  课程号: ");
    const std::string name = readLine("  课程名: ");
    const double credit    = readDouble("  学分(0~20): ");
    if (cm.addCourse(id, name, credit))
        std::cout << "  [OK] 课程 " << id << " 添加成功。\n";
    else
        std::cout << "  [X] 添加失败：课程号/课程名不能为空、学分非法或课程号已存在。\n";
    pause();
}

// 5. 显示全部课程
static void uiListCourses(const CreditManager& cm) {
    printLine();
    std::cout << "【课程列表】共 " << cm.courses().size() << " 门\n";
    printLine();
    std::cout << std::left << std::setw(14) << "课程号" << "课程名\t\t\t学分\n";
    printLine();
    for (const auto& c : cm.courses())
        std::cout << std::left << std::setw(14) << c.id << c.name << "\t\t\t" << c.credit << '\n';
    pause();
}

// 6. 删除课程
static void uiRemoveCourse(CreditManager& cm) {
    printLine();
    std::cout << "【删除课程】\n";
    const std::string id = readLine("  课程号: ");
    if (cm.removeCourse(id))
        std::cout << "  [OK] 课程 " << id << " 及其相关成绩已删除。\n";
    else
        std::cout << "  [X] 未找到课程号为 " << id << " 的课程。\n";
    pause();
}

// 7. 录入 / 修改成绩
static void uiSetScore(CreditManager& cm) {
    printLine();
    std::cout << "【录入 / 修改成绩】\n";
    const std::string sid = readLine("  学号: ");
    const std::string cid = readLine("  课程号: ");
    const double score    = readDouble("  成绩(0~100): ");
    if (cm.setScore(sid, cid, score))
        std::cout << "  [OK] 成绩已保存（重复录入会自动覆盖，可用于补考/重修）。\n";
    else
        std::cout << "  [X] 录入失败：请确认学生和课程都已存在，且成绩在 0~100 之间。\n";
    pause();
}

// 8. 查询学生成绩单
static void uiReport(const CreditManager& cm) {
    printLine();
    std::cout << "【学生成绩单】\n";
    const std::string sid = readLine("  学号: ");
    const Student* s = cm.findStudent(sid);
    if (s == nullptr) {
        std::cout << "  [X] 未找到该学生。\n";
        pause();
        return;
    }

    std::cout << "\n  学号: " << s->id << "    姓名: " << s->name
              << "    专业: " << s->major << '\n';
    printLine();
    std::cout << std::left << std::setw(14) << "课程号" << "课程名\t\t\t学分\t成绩\t绩点\n";
    printLine();

    for (const auto& g : cm.gradesOf(sid)) {
        const Course* c = cm.findCourse(g.courseId);
        if (c == nullptr) continue;
        std::cout << std::left << std::setw(14) << c->id << c->name << "\t\t\t" << c->credit
                  << "\t" << g.score << "\t" << CreditManager::scoreToGpa(g.score) << '\n';
    }
    printLine();

    const CreditReport r = cm.report(sid);
    std::cout << "  已修学分: " << r.totalCredits << "    已获学分: " << r.earnedCredits
              << "    不及格门数: " << r.failedCount << '\n';
    std::cout << "  加权平均分: " << r.averageScore << "    平均绩点(GPA): " << r.gpa << '\n';
    if (r.totalCredits > 0.0 && r.earnedCredits >= r.totalCredits)
        std::cout << "  [提示] 该生已修课程全部及格，学分已修满。\n";
    pause();
}

// 9. 学分排行榜
static void uiRanking(const CreditManager& cm) {
    printLine();
    std::cout << "【学分排行榜】按已获学分降序\n";
    printLine();
    std::cout << std::left << std::setw(8) << "名次" << std::setw(14) << "学号"
              << "姓名\t\t已获学分\tGPA\n";
    printLine();

    const auto rank = cm.ranking();
    int no = 0;
    for (const auto& e : rank) {
        ++no;
        const Student* s = cm.findStudent(e.first);
        std::cout << std::left << std::setw(8) << no << std::setw(14) << e.first
                  << (s ? s->name : std::string("-")) << "\t\t" << e.second << "\t\t"
                  << cm.report(e.first).gpa << '\n';
    }
    if (rank.empty()) std::cout << "  （暂无学生数据）\n";
    pause();
}

// 10. 载入演示数据
static void uiDemo(CreditManager& cm) {
    printLine();
    std::cout << "【载入演示数据】\n";
    cm.addCourse("CS101", "程序设计基础", 4.0);
    cm.addCourse("CS102", "数据结构", 4.0);
    cm.addCourse("MA101", "高等数学", 5.0);
    cm.addCourse("EN101", "大学英语", 3.0);
    cm.addCourse("PE101", "体育", 1.0);

    cm.addStudent("2024001", "张三", "计算机科学与技术");
    cm.addStudent("2024002", "李四", "计算机科学与技术");
    cm.addStudent("2024003", "王五", "软件工程");

    const double scores[3][5] = {
        {92, 88, 76, 85, 90}, // 张三
        {78, 59, 64, 72, 88}, // 李四（数据结构不及格）
        {85, 90, 91, 80, 95}, // 王五
    };
    const char* sids[3] = {"2024001", "2024002", "2024003"};
    const char* cids[5] = {"CS101", "CS102", "MA101", "EN101", "PE101"};

    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 5; ++j)
            cm.setScore(sids[i], cids[j], scores[i][j]);

    std::cout << "  [OK] 已载入 3 名学生、5 门课程及全部成绩（重复执行不会产生重复数据）。\n";
    pause();
}

// ============================================================
//  四、主程序
// ============================================================
int main(int argc, char** argv) {
    setupConsole();

    const std::string dataFile = (argc > 1) ? argv[1] : "data.txt";

    CreditManager cm;
    const bool loaded = cm.load(dataFile);

    std::cout << "============================================================\n";
    std::cout << "  学生学分管理系统 —— 数据文件: " << dataFile << '\n';
    std::cout << (loaded ? "  [OK] 已载入原有数据。\n"
                         : "  [提示] 未找到数据文件，将从空数据开始。\n");
    std::cout << "============================================================\n";

    bool running = true;
    while (running) {
        showMenu();
        const int choice = readInt("请输入操作编号: ");

        if (g_inputClosed || std::cin.eof()) {
            running = false; // 输入流已结束（Ctrl+Z 或重定向输入用完）
            continue;
        }

        switch (choice) {
            case 1:  uiAddStudent(cm);    break;
            case 2:  uiListStudents(cm);  break;
            case 3:  uiRemoveStudent(cm); break;
            case 4:  uiAddCourse(cm);     break;
            case 5:  uiListCourses(cm);   break;
            case 6:  uiRemoveCourse(cm);  break;
            case 7:  uiSetScore(cm);      break;
            case 8:  uiReport(cm);        break;
            case 9:  uiRanking(cm);       break;
            case 10: uiDemo(cm);          break;
            case 11:
                std::cout << (cm.save(dataFile) ? "  [OK] 数据已保存。\n"
                                                : "  [X] 保存失败，请检查文件权限。\n");
                pause();
                break;
            case 0:
                running = false;
                break;
            default:
                std::cout << "  [!] 没有编号为 " << choice << " 的功能，请重新选择。\n";
                break;
        }
    }

    // 退出时自动保存
    if (cm.save(dataFile))
        std::cout << "\n  [OK] 数据已保存到 " << dataFile << "，再见！\n";
    else
        std::cout << "\n  [X] 退出时保存失败（数据未写入磁盘）。\n";
    return 0;
}