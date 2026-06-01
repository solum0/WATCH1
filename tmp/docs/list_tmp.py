from pathlib import Path
for p in Path(r'D:/SSTM32/PCB/test1/tmp/docs').glob('*'):
    print(p)
