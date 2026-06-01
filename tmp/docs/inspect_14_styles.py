# -*- coding: utf-8 -*-
from pathlib import Path
from docx import Document
path = Path(r'C:/Users/94122/Desktop/毕设/1/out/heart_sensor_version1.1_backup_20260413_section_2_3_3.docx')
doc = Document(str(path))
for idx in [85,86,87,88,89,90,92]:
    p = doc.paragraphs[idx]
    fmt = p.paragraph_format
    print('idx', idx, 'text=', p.text[:30])
    print('  style=', p.style.name if p.style else None)
    print('  first_line_indent=', fmt.first_line_indent)
    print('  left_indent=', fmt.left_indent)
    print('  align=', p.alignment)
    if p.runs:
        r = p.runs[0]
        print('  font=', r.font.name, 'size=', r.font.size, 'bold=', r.bold)
