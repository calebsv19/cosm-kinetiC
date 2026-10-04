"""Early phase enforcement of the unchanged reference resource caps."""
RSS_CAP=1800*1024**2
WALL_CAP=180.


class PhaseResourceStopped(RuntimeError):
    def __init__(self,record):
        self.record=record
        super().__init__('reference phase resource cap')


def enforce_phase(phase,peak_rss_bytes,wall_s):
    if peak_rss_bytes>=RSS_CAP or wall_s>=WALL_CAP:
        raise PhaseResourceStopped(dict(phase=phase,peak_rss_bytes=int(peak_rss_bytes),wall_s=float(wall_s),
            rss_cap_bytes=RSS_CAP,wall_cap_s=WALL_CAP))
