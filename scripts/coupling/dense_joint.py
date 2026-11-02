"""Explicit 10Hz full-state retention; existing live12 numerical path unchanged."""
from pathlib import Path
import hashlib
from coupling.live_joint import LiveFramework
from surface_sources.growth_fire_v1 import require

class DenseLiveFramework(LiveFramework):
    RETENTION_PROFILE='fire_fluid_dense_10hz/v1'

    def __init__(self,config,store):
        super().__init__(config,store)
        self.selected=set(range(self.config['windows']+1))
        require(len(self.selected)<=121,'dense10 fixed120window retention ceiling')

    def input_hashes(self):
        result=super().input_hashes()
        result[self.RETENTION_PROFILE]=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
        return result
