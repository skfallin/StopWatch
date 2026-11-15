"""Check actual production drawings of the Home ring in both swipe directions."""
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile

root=Path(__file__).resolve().parents[1]
ring=[(233+round(165*math.sin(math.tau*slot/8)),233-round(165*math.cos(math.tau*slot/8))) for slot in range(8)]
names={"Home","Notes","Badge","Clock","Spotify","Bridge","System","Focus"}
with tempfile.TemporaryDirectory(prefix="stopwatch-home-animation-") as folder:
    output=Path(folder)
    subprocess.run([sys.executable,str(root/"tools/render_clock_preview.py"),folder,"--scene"],check=True)
    frames=list(output.glob("home-swipe-*.jsonl"))
    assert len(frames)==10
    for path in frames:
        records=[json.loads(line) for line in path.read_text().splitlines()]
        circles=[record[1:4] for record in records if record[0]=="circle" and record[3]>40]
        centres=list(dict.fromkeys((x,y) for x,y,_ in circles))
        assert centres==ring,(path.name,centres)
        assert all(43<=r<=46 for _,_,r in circles),(path.name,circles)
        for i,(x,y) in enumerate(centres):
            # On the round glass, apart from each other and clear of the clock in the middle.
            assert math.hypot(x-233,y-233)+46<=216 and math.hypot(x-233,y-233)-46>=110,(path.name,x,y)
            for a,b in centres[i+1:]:
                assert math.hypot(x-a,y-b)>92,(path.name,centres)
        texts=[record for record in records if record[0]=="text"]
        assert len([record for record in texts if record[1] in names])==1,(path.name,texts)
        for record in texts:
            assert record[2]==233 and abs(record[3]-233)<110,(path.name,record)
print("OK: 10 fotogrammi, due versi, anello stabile, orologio e nome al centro senza sovrapposizioni")
