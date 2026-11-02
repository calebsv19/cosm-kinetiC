"""Bounded validated snapshot batch for read-only dense playback export."""
def validated_view(host,revisions):
    from contextlib import closing
    from types import SimpleNamespace
    from surface_sources.growth_fire_v1 import require
    require(type(revisions) is list and 1<=len(revisions)<=4 and len(set(revisions))==len(revisions),'one-to-four export revisions')
    with closing(host.db()) as db:
        db.execute('BEGIN')
        rows,_=host.bound(db);head=len(rows)-1
        require(all(type(i) is int and 0<i<=head for i in revisions),'accepted export revisions')
        # bound() verifies the complete chain, all retained payload hashes and
        # native head/prior. Each requested payload and genesis is decoded and
        # independently native-validated once, within the same read transaction.
        selected={i:rows[i] for i in {0,head,*revisions}}
        require(len(selected)<=6,'bounded playback snapshot working set')
    class BatchRows:
        def __len__(self):return head+1
        def __getitem__(self,index):
            require(type(index) is int,'explicit batch revision')
            if index<0:index+=head+1
            require(index in selected,'revision outside validated export batch')
            return selected[index]
    return SimpleNamespace(config=host.config,fire=host.fire,selected_frames=lambda:BatchRows())
