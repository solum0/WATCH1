from docx import Document


DOC_PATH = r"D:/SSTM32/PCB/test1/tmp/docs/heart_sensor_version1.1_edit.docx"


CONCLUSION_PARAGRAPHS = [
    "本文围绕基于 FreeRTOS 与 STM32 的健康运动监测仪设计展开研究，结合可穿戴设备对实时性、小型化、多功能集成和低功耗运行的要求，完成了系统总体方案设计、硬件电路设计、软件程序设计以及整机调试与测试等工作。论文在前期分析健康运动监测需求的基础上，完成了主控平台、显示单元、实时时钟单元、环境检测单元、心率检测单元、运动姿态检测单元以及蓝牙通信单元等关键模块的选型与设计，并构建了较为完整的系统硬件平台。",
    "在硬件实现方面，系统以 STM32F411CEU6 为核心控制器，结合 OLED 显示屏、DS3231 实时时钟模块、BME280 温湿度传感器、EM7028 心率检测模块、MPU6050 六轴传感器以及电池电量检测电路，完成了健康运动监测仪的核心功能硬件搭建。在软件实现方面，系统基于 FreeRTOS 搭建多任务运行框架，将按键处理、界面显示、时间与闹钟设置、心率检测、计步检测、电量更新以及串口或 BLE 指令交互等功能进行模块化设计，提高了系统结构的清晰性、任务执行的实时性以及整体运行的稳定性。",
    "通过对样机进行功能测试与联调验证可以看出，系统能够较好地实现时间显示、闹钟设置、温湿度检测、秒表计时、心率检测、计步统计、电量显示以及外部控制等功能。测试结果表明，系统在多模式切换、传感器数据采集、OLED 实时显示以及后台任务协同运行等方面均表现较为稳定，基本达到了课题预期的设计目标。这说明采用 FreeRTOS 多任务架构结合多传感器信息采集的方案具有较好的可行性，也验证了本设计在健康运动监测场景中的应用价值。",
    "尽管本系统已完成主要功能实现，但仍存在一定的改进空间。例如，心率检测结果在剧烈运动或佩戴状态不稳定时仍可能受到动作干扰影响，计步算法在复杂运动场景下仍有进一步优化的余地；同时，系统在低功耗管理、历史数据存储、移动端交互界面以及整机外观结构优化等方面还可继续完善。后续工作可围绕心率抗干扰算法优化、运动状态识别精度提升、BLE 通信协议完善、数据记录与云端同步以及整机功耗控制等方向展开，从而进一步提升系统的实用性、智能化水平与工程应用价值。",
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

    i_conclusion = None
    i_next = None
    for index, paragraph in enumerate(paragraphs):
        text = paragraph.text.strip()
        if text == "6 结  语":
            i_conclusion = index
        elif i_conclusion is not None and text == "参考文献":
            i_next = index
            break

    if i_conclusion is None or i_next is None:
        raise RuntimeError("Failed to locate the conclusion section.")

    conclusion_heading = paragraphs[i_conclusion]
    next_heading = paragraphs[i_next]

    for paragraph in paragraphs[i_conclusion + 1:i_next]:
        delete_paragraph(paragraph)

    for text in CONCLUSION_PARAGRAPHS:
        next_heading.insert_paragraph_before(text, style="本科论文正文")

    document.save(DOC_PATH)
    print(f"Updated: {DOC_PATH}")


if __name__ == "__main__":
    main()
