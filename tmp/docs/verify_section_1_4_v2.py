# -*- coding: utf-8 -*-
from pathlib import Path
from zipfile import ZipFile
for p in Path(r'C:/Users/94122/Desktop/毕设/1/out').glob('heart_sensor_version1.1_backup_20260413_section_2_3_3*.docx'):
    print('FILE', p)
    with ZipFile(p) as z:
        xml = z.read('word/document.xml').decode('utf-8', errors='replace')
    for s in [
        '为了便于对本文研究内容与实现过程进行系统阐述，全文共分为六章，各章内容安排如下：',
        '（1）第 1 章为绪论。',
        '（6）第 6 章为结语。'
    ]:
        print(s[:14], s in xml)
