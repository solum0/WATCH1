# -*- coding: utf-8 -*-
from zipfile import ZipFile
from pathlib import Path
p = Path(r'C:/Users/94122/Desktop/毕设/1/out/heart_sensor_version1.1_backup_20260413_section_2_3_3.docx')
with ZipFile(p) as z:
    xml = z.read('word/document.xml').decode('utf-8', errors='replace')
for key in ['人机交互单元主要用于完成模式切换', '独立轻触按键', '????????', '图2.3']:
    print(key, key in xml)
idx = xml.find('图2.3')
print('idx', idx)
print(xml[max(0, idx-1200): idx+400])
