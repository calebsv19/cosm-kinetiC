"""Trusted-local session path admission; never silently follow a supplied link."""
import os
from pathlib import Path
import stat


def checked(path):
    raw=Path(path)
    if '..' in raw.parts or len(raw.parts)>256:
        raise ValueError('Session path traversal/depth held')
    path=Path(os.path.abspath(raw))
    # macOS system aliases are admitted only with their exact expected targets.
    for alias,target in ((Path('/tmp'),Path('/private/tmp')),(Path('/var'),Path('/private/var'))):
        if path==alias or path.is_relative_to(alias):
            if alias.is_symlink():
                actual=Path(os.path.abspath(alias.parent/os.readlink(alias)))
                if actual!=target:raise ValueError('Unexpected system path alias held')
                path=target/path.relative_to(alias)
    for current in reversed((path,*path.parents)):
        try:info=current.lstat()
        except FileNotFoundError:continue
        if stat.S_ISLNK(info.st_mode):raise ValueError('Session path symlink held: '+str(current))
        if current!=path and not stat.S_ISDIR(info.st_mode):
            raise ValueError('Session path parent is not a directory')
    return path


def root_path(path, repo):
    root=checked(path);repo=checked(repo)
    system_roots=[Path(name) for name in ('/System','/usr','/bin','/sbin','/etc','/Applications','/Library')]
    broad={Path(name) for name in ('/','/Users','/Volumes','/private','/private/tmp','/private/var')}
    home=checked(Path.home())
    broad.update(home/name for name in ('','Desktop','Documents','Downloads','Library','Library/Application Support'))
    if root in broad or any(root==base or root.is_relative_to(base) for base in system_roots):
        raise ValueError('Session root selects broad/system storage')
    if root==repo or repo.is_relative_to(root):
        raise ValueError('Session root overlaps source checkout/ancestor')
    if any(part in ('.git','.aws','.ssh') or part.endswith('.app') for part in root.parts):
        raise ValueError('Session root selects protected storage')
    allowed=[repo/name for name in ('build','tmp','data/experiments','data/runtime')]
    if root.is_relative_to(repo) and not any(root.is_relative_to(base) and root!=base for base in allowed):
        raise ValueError('Session root must select generated storage, not source files')
    for parent in (root,*root.parents):
        if parent!=repo and (parent/'.git').exists() and not parent.is_relative_to(repo):
            raise ValueError('Session root selects another checkout')
    if root.exists() and not root.is_dir():raise ValueError('Session root is not a directory')
    return root


def directory(path):
    path=checked(path)
    if path.exists() and not path.is_dir():raise ValueError('Session storage is not a directory')
    return path


def asset_path(scene,name):
    if not isinstance(name,str) or not name or Path(name).is_absolute() or '..' in Path(name).parts:
        raise ValueError('Scene asset path escapes its scene')
    selected=checked(scene/name);base=checked(scene)
    if selected==base or not selected.is_relative_to(base):raise ValueError('Scene asset path escapes its scene')
    return selected
