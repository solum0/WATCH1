# -*- coding: utf-8 -*-
from docx import Document
from pathlib import Path
p = Path(r'D:/SSTM32/PCB/test1/tmp/docs/chinese_test.docx')
d = Document()
d.add_paragraph('人机交互单元主要用于完成模式切换、时间设置。')
d.save(str(p))
print(p)
