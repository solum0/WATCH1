# -*- coding: utf-8 -*-
from zipfile import ZipFile
from pathlib import Path
src = Path(r'C:/Users/94122/Desktop/毕设/1/out/heart_sensor_version1.1.backup_20260413_目录前.docx')
out = Path(r'C:/Users/94122/Desktop/毕设/1/out/heart_sensor_version1.1_backup_20260413_section_2_3_3.docx')
for path in [src, out]:
    with ZipFile(path) as z:
        media = [n for n in z.namelist() if n.startswith('word/media/')]
        foot = [n for n in z.namelist() if 'footnotes' in n or 'endnotes' in n]
        comments = [n for n in z.namelist() if 'comments' in n]
        print(path.name)
        print('  files', len(z.namelist()))
        print('  media', len(media))
        print('  comments', comments)
        print('  notes', foot)
        print('  document.xml size', len(z.read('word/document.xml')))
