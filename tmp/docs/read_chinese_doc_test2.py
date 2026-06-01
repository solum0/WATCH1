# -*- coding: utf-8 -*-
from docx import Document
from pathlib import Path
p = Path(r'D:/SSTM32/PCB/test1/tmp/docs/chinese_test.docx')
d = Document(str(p))
for para in d.paragraphs:
    print(para.text)
