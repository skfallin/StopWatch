"""Check actual production drawings throughout both Home swipe directions."""
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile

root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="stopwatch-home-animation-") as folder:
    output=Path(folder)
    subprocess.run([sys.executable,str(root/"tools/render_clock_preview.py"),folder,"--scene"],check=True)
    frames=list(output.glob("home-swipe-*.jsonl"))
    assert len(frames)==30
    for path in frames:
        group=int(path.name.split("-")[2])
        expected=[(233,174,79),(233,318,54)] if group==0 else [(233,188,84),(130,309,54),(336,309,54)]
        records=[json.loads(line) for line in path.read_text().splitlines()]
        circles=[record[1:4] for record in records if record[0]=="circle" and record[3]>40]
        assert len(circles)==len(expected),(path.name,circles)
        for (x,y,r),(cx,cy,radius) in zip(circles,expected):
            assert (x,y)==(cx,cy) and radius-3<=r<=radius,(path.name,circles)
        for i,(x,y,r) in enumerate(circles):
            for a,b,s in circles[i+1:]:
                assert math.hypot(x-a,y-b)>r+s,(path.name,circles)
        names={"Home","Notes","Badge","Clock","Spotify","Bridge","Key","System","Focus","Slide"}
        labels=[record for record in records if record[0]=="text" and record[1] in names]
        assert len(labels)==len(expected),(path.name,labels)
print("OK: 30 fotogrammi, tutti i gruppi e due versi, posizioni stabili, icone e testi separati")
