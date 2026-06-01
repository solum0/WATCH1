# -*- coding: utf-8 -*-
from pathlib import Path
for p in sorted(Path(r'C:/Users/94122/Desktop/毕设/1/out').glob('heart_sensor_version1.1_backup_20260413_section_2_3_3*.docx')):
    print(p.name)
