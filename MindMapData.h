#pragma once
#include <string>
#include <vector>
#include <windows.h>
#include <commctrl.h>

// 样式结构体：专门存储节点的样式信息
struct MindNodeStyle {
    //节点位置属性
    int x;
    int y;
    int width;
    int height;

    // 节点样式属性
    std::wstring shape;           // 形状: rectangle, circle, ellipse, diamond等
    std::wstring fillColor;       // 填充颜色: 如 #FFFFFF
    int borderWidth;             // 边框粗细: 像素值
    std::wstring borderColor;     // 边框颜色: 如 #000000
    std::wstring borderStyle;     // 边框样式: solid, dashed, dotted

    // 文字样式属性
    std::wstring fontFamily;      // 字体: 如 Arial, 微软雅黑
    int fontSize;                // 字体大小: 像素值
    std::wstring textColor;       // 文字颜色: 如 #000000
    bool bold;                   // 是否加粗
    bool italic;                 // 是否斜体
    bool underline;              // 是否启用下划线
    std::wstring textAlignment;   // 文字对齐方式: left, center, right

    // 分支样式属性
    int branchWidth;             // 分支粗细: 像素值
    std::wstring branchColor;     // 分支颜色: 如 #000000

    // 节点结构样式
    std::wstring nodeStructure;   // 节点结构样式: mindmap, tree, orgchart等

    // 构造函数，设置默认值
    MindNodeStyle()
        : borderWidth(1), fontSize(12), bold(false), italic(false), underline(false),
        branchWidth(1) {
        // 设置默认值
        shape = L"rectangle";
        fillColor = L"#FFFFFF";
        borderColor = L"#000000";
        borderStyle = L"solid";
        fontFamily = L"Arial";
        textColor = L"#000000";
        textAlignment = L"center";
        branchColor = L"#000000";
        nodeStructure = L"mindmap";
    }

    // 复制构造函数
    MindNodeStyle(const MindNodeStyle& other) {
        shape = other.shape;
        fillColor = other.fillColor;
        borderWidth = other.borderWidth;
        borderColor = other.borderColor;
        borderStyle = other.borderStyle;
        fontFamily = other.fontFamily;
        fontSize = other.fontSize;
        textColor = other.textColor;
        bold = other.bold;
        italic = other.italic;
        underline = other.underline;
        textAlignment = other.textAlignment;
        branchWidth = other.branchWidth;
        branchColor = other.branchColor;
        nodeStructure = other.nodeStructure;
    }

    // 赋值运算符
    MindNodeStyle& operator=(const MindNodeStyle& other) {
        if (this != &other) {
            shape = other.shape;
            fillColor = other.fillColor;
            borderWidth = other.borderWidth;
            borderColor = other.borderColor;
            borderStyle = other.borderStyle;
            fontFamily = other.fontFamily;
            fontSize = other.fontSize;
            textColor = other.textColor;
            bold = other.bold;
            italic = other.italic;
            underline = other.underline;
            textAlignment = other.textAlignment;
            branchWidth = other.branchWidth;
            branchColor = other.branchColor;
            nodeStructure = other.nodeStructure;
        }
        return *this;
    }
};


struct MindNode {
    int id;//ID
    std::wstring text;//文本
    MindNode* parent;//父节点
    std::vector<MindNode*> children;//子节点数组
    HTREEITEM hTreeItem;
    std::wstring label;//标签信息
    std::wstring note;//笔记信息
    std::wstring outlinkFile;//关联文件路径
    std::wstring outlinkPage;//关联页面
    int outlinkTopicId;//关联主题ID
    std::wstring paintPath;//绘图路径
    std::wstring summary;//摘要信息
    std::wstring frameStyle;//框架样式
    std::wstring outlinkURL;//关联网址
    std::wstring annotation;//注释信息
    std::vector<int> linkedNodeIds;

    // 新增：节点样式
    MindNodeStyle style;

    MindNode(int id, std::wstring text, MindNode* parent = nullptr)
        : id(id), text(std::move(text)), parent(parent), hTreeItem(NULL), outlinkTopicId(0) {
        // 使用MindNodeStyle的默认构造函数初始化
    }

    ~MindNode() {
        for (auto child : children) {
            delete child;
        }
        children.clear();
    }
};

struct MindFrame {
    std::vector<int> nodeIds;
    std::wstring style;
};

