from docx import Document


DOC_PATH = r"D:/SSTM32/PCB/test1/tmp/docs/heart_sensor_version1.1_toc_edit.docx"


TOC_ITEMS = [
    ("toc 1", "1 绪  论\t1"),
    ("toc 2", "1.1 论文研究背景\t1"),
    ("toc 2", "1.2 国内外的研究现状\t1"),
    ("toc 2", "1.3 论文主要研究内容\t2"),
    ("toc 1", "1.4 文章设计结构\t2"),
    ("toc 1", "2 总体方案设计\t4"),
    ("toc 2", "2.1 设计内容及要求\t4"),
    ("toc 2", "2.2 系统整体方案设计\t4"),
    ("toc 2", "2.3 器件的选择\t5"),
    ("toc 1", "3 硬件设计\t11"),
    ("toc 2", "3.1 系统硬件整体方案设计\t11"),
    ("toc 1", "4 系统程序设计\t17"),
    ("toc 2", "4.1 嵌入式软件开发工具介绍\t17"),
    ("toc 2", "4.2 嵌入式硬件设计工具介绍\t17"),
    ("toc 2", "4.3 主程序流程设计\t17"),
    ("toc 2", "4.4 子程序流程设计\t17"),
    ("toc 2", "  4.4.1 主程序初始化流程图设计\t18"),
    ("toc 2", "  4.4.2 按键与模式切换流程图设计\t19"),
    ("toc 2", "  4.4.3 时间设置流程图设计\t20"),
    ("toc 2", "  4.4.4 闹钟设置与提醒流程图设计\t20"),
    ("toc 2", "  4.4.5 温湿度采集与显示流程图设计\t21"),
    ("toc 2", "  4.4.6 心率检测与显示流程流程图设计\t21"),
    ("toc 1", "5 系统实现与测试\t30"),
    ("toc 2", "5.1 系统实现\t30"),
    ("toc 2", "5.2 系统测试\t31"),
    ("toc 2", "  5.2.1 模式切换与界面显示测试\t31"),
    ("toc 2", "  5.2.2 时间显示与时间设置测试\t31"),
    ("toc 2", "  5.2.3 闹钟设置与提醒测试\t32"),
    ("toc 2", "  5.2.4 温湿度采集与显示测试\t32"),
    ("toc 2", "  5.2.5 秒表计时测试\t32"),
    ("toc 2", "  5.2.6 心率检测测试\t32"),
    ("toc 2", "  5.2.7 计步与抬腕亮屏测试\t32"),
    ("toc 2", "  5.2.8 电量检测与显示测试\t33"),
    ("toc 2", "  5.2.9 串口/BLE 控制测试\t33"),
    ("toc 2", "  5.2.10 系统稳定性测试\t33"),
    ("toc 1", "6 结    语\t34"),
    ("toc 1", "致    谢\t35"),
    ("toc 1", "参考文献\t36"),
]


def delete_paragraph(paragraph):
    element = paragraph._element
    parent = element.getparent()
    if parent is not None:
        parent.remove(element)
    paragraph._p = paragraph._element = None


def main():
    document = Document(DOC_PATH)
    paragraphs = list(document.paragraphs)

    toc_title_index = None
    body_start_index = None

    for index, paragraph in enumerate(paragraphs):
        text = paragraph.text.strip()
        if text == "目    录":
            toc_title_index = index
        elif toc_title_index is not None and text == "1 绪  论":
            body_start_index = index
            break

    if toc_title_index is None or body_start_index is None:
        raise RuntimeError("Failed to locate the manual TOC range.")

    body_start_paragraph = paragraphs[body_start_index]

    for paragraph in paragraphs[toc_title_index + 1:body_start_index]:
        delete_paragraph(paragraph)

    for style_name, text in TOC_ITEMS:
        body_start_paragraph.insert_paragraph_before(text, style=style_name)

    body_start_paragraph.insert_paragraph_before("", style="Normal")

    document.save(DOC_PATH)
    print(f"Updated: {DOC_PATH}")


if __name__ == "__main__":
    main()
