# -*- coding: utf-8 -*-
from zipfile import ZipFile
from pathlib import Path
p = Path(r'C:/Users/94122/Desktop/毕设/1/out/heart_sensor_version1.1_backup_20260413_section_2_3_3.docx')
with ZipFile(p) as z:
    xml = z.read('word/document.xml').decode('utf-8', errors='replace')
for key in ['人机交互单元主要用于完成模式切换、时间设置、闹钟设置以及功能启停等操作', '综合比较后，本设计选用独立轻触按键作为人机交互单元的主要输入器件', '综上所述，独立轻触按键在体积、成本、可靠性及软件实现复杂度等方面均符合本设计需求']:
    print(key[:12], key in xml)
