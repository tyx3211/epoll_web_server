---
marp: true
theme: uncover
paginate: true
footer: 'epoll Server展示'
---

<style>
/* 
  您好！这是为您定制的 Marp 演示文稿样式。
  我已经在这里添加了更丰富的样式，并用中文注释标明了所有您可以修改的地方。
  您可以直接修改这里的数值来调整外观。
*/

/* --- 1. 全局与页面布局 --- */
section {
    /* 字体族: 优先使用系统UI字体，确保中英文显示效果。*/
    font-family: -apple-system, BlinkMacSystemFont, "Helvetica Neue", "Segoe UI", "Hiragino Kaku Gothic ProN", "Hiragino Sans", "ヒラギノ角ゴ ProN W3", Arial, "メイリオ", Meiryo, "Microsoft YaHei", "微软雅黑", sans-serif;
    
    /* 基础字号: 调整这个值可以改变整个页面的基础字体大小。*/
    font-size: 24px;

    /* 页面背景色: */
    background-color: #ffffff; /* 纯白背景 */

    /* 默认文字颜色: */
    color: #333; /* 深灰色文字，比纯黑更柔和，阅读更舒适 */

    /* 内容对齐: 全局左对齐，符合严谨的阅读习惯。*/
    text-align: left;
    
    /* 内容垂直对齐: 从顶部开始，而不是垂直居中。*/
    justify-content: start;
    
    /* 页面内边距: 页面内容与边缘的距离。第一个值是上下，第二个值是左右。*/
    padding: 50px 70px;
}

/* --- 2. 标题样式 (H1, H2, H3) --- */
h1, h2, h3, h4, h5, h6 {
    /* 标题颜色: */
    color: #003366; /* 标题使用深蓝色，显得专业 */
    
    /* 标题字重: 600是半粗体。*/
    font-weight: 600;

    /* 标题下方间距: */
    margin-bottom: 0.6em; /* 标题和下方内容之间留出空隙 */
}

h1 {
    /* H1 标题字号 */
    font-size: 44px;
    border-bottom: 2px solid #0055aa; /* 给主标题加一条下划线 */
    padding-bottom: 0.2em; /* 下划线与文字的距离 */
}
h2 { font-size: 36px; }
h3 { font-size: 28px; }

/* --- 3. 文本与段落样式 --- */
p {
    /* 文本行距: 这是最重要的排版设置之一！调整这个值可以改变段落的行间距。*/
    line-height: 1.8;
    
    /* 段落下方间距: */
    margin-bottom: 1em;
}

/* --- 4. 列表样式 (无序/有序) --- */
ul, ol {
    /* 列表行距: */
    line-height: 1.8;
    
    /* 列表左侧缩进: */
    margin-left: 30px;
}

/* --- 5. 其他元素样式 (链接/引用/代码/表格/图片) --- */
/* 链接样式 */
a {
    color: #0066cc; /* 链接颜色 */
    text-decoration: none; /* 去掉下划线 */
}
a:hover {
    text-decoration: underline; /* 鼠标悬浮时显示下划线 */
}

/* 引用块样式 */
blockquote {
    background: #f9f9f9; /* 浅灰色背景 */
    border-left: 5px solid #ccc; /* 左侧的灰色竖线 */
    margin: 1.5em 0;
    padding: 0.5em 20px;
    font-style: italic; /* 斜体 */
}

/* 代码样式 */
/* 内联代码 `code` */
code {
    background-color: #eef;
    padding: 0.2em 0.4em;
    border-radius: 3px;
    font-size: 0.9em;
}
/* 代码块 <pre><code> */
pre code {
    background-color: #f7f7f7;
    border: 1px solid #ddd;
    display: block;
    padding: 1em;
    border-radius: 5px;
    font-size: 0.85em;
    line-height: 1.6;
}

/* 表格样式 */
table {
    width: 100%;
    border-collapse: collapse; /* 合并边框 */
    margin-bottom: 1em;
}
th, td {
    border: 1px solid #ddd; /* 单元格边框 */
    padding: 8px 12px; /* 单元格内边距 */
    text-align: left;
}
th {
    background-color: #f2f2f2; /* 表头背景色 */
    font-weight: bold;
}

/* --- 6. 页脚与页码 --- */
footer {
    position: absolute;
    bottom: 25px; /* 距离底部的位置 */
    left: 40px;
    right: 40px;
    font-size: 16px; /* 页脚字号 */
    color: #777;
}

/* --- 7. 特殊页面样式：章节页 --- */
/*
  我们不再需要 .chapter 类和伪元素，
  直接对 Marp 的原生 <header> 元素进行样式化。
*/
section > header {
    /* 定位与样式 */
    position: absolute; /* 绝对定位 */
    top: 30px;          /* 距离顶部 30px */
    left: 70px;         /* 距离左侧 70px (与页面内边距一致) */
    right: auto;        /* 覆盖掉可能存在的右对齐 */
    
    /* 外观 */
    font-size: 20px;    /* 字体大小 */
    font-weight: bold;  /* 字体加粗 */
    color: #555;        /* 页眉颜色 */
    text-align: left;   /* 确保文字左对齐 */
}

</style>

# 利用epoll技术的高性能Web服务器开发

- 报告人：谭宇轩
- 日期：2025-7-22