# -*- coding: utf-8 -*-
from pathlib import Path
from zipfile import ZipFile
p = Path(r'C:/Users/94122/Desktop/毕设/1/out/heart_sensor_version1.1_backup_20260413_section_2_3_3_v2.docx')
with ZipFile(p) as z:
    xml = z.read('word/document.xml').decode('utf-8', errors='replace')
for s in [
    '（2）第 2 章为总体设计方案。',
    '（5）第 5 章为系统实现与测试。',
    '（6）第 6 章为结语。'
]:
    print(s, s in xml)
