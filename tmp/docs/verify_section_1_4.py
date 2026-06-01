# -*- coding: utf-8 -*-
from pathlib import Path
from zipfile import ZipFile
paths = [
    Path(r'C:/Users/94122/Desktop/毕设/1/out/heart_sensor_version1.1_backup_20260413_section_2_3_3.docx'),
    Path(r'C:/Users/94122/Desktop/毕设/1/out/heart_sensor_version1.1_backup_20260413_section_2_3_3_v2.docx'),
]
for path in paths:
    if path.exists():
        with ZipFile(path) as z:
            xml = z.read('word/document.xml').decode('utf-8', errors='replace')
        ok = all(s in xml for s in [
            '为了便于对本文研究内容与实现过程进行系统阐述，全文共分为六章，各章内容安排如下：',
            '（1）第 1 章为绪论。',
            '（6）第 6 章为结语。'
        ])
        print(path)
        print('verified=', ok)
        idx = xml.find('1.4 文章设计结构')
        print('anchor=', idx)
