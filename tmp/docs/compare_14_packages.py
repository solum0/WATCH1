# -*- coding: utf-8 -*-
from zipfile import ZipFile
from pathlib import Path
paths = [
    Path(r'C:/Users/94122/Desktop/毕设/1/out/heart_sensor_version1.1_backup_20260413_section_2_3_3.docx'),
    Path(r'C:/Users/94122/Desktop/毕设/1/out/heart_sensor_version1.1_backup_20260413_section_2_3_3_v2.docx')
]
for path in paths:
    with ZipFile(path) as z:
        media = [n for n in z.namelist() if n.startswith('word/media/')]
        comments = [n for n in z.namelist() if 'comments' in n]
        notes = [n for n in z.namelist() if 'footnotes' in n or 'endnotes' in n]
        print(path.name)
        print('  files', len(z.namelist()))
        print('  media', len(media))
        print('  comments', len(comments))
        print('  notes', len(notes))
        print('  docxml', len(z.read('word/document.xml')))
