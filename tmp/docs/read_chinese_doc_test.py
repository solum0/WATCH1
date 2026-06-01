# -*- coding: utf-8 -*-
from docx import Document
p = r'D:/SSTM32/PCB/test1/tmp/docs/chinese_test.docx'
d = Document(p)
for para in d.paragraphs:
    print(para.text)
