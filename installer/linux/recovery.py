#!/usr/bin/env python3
"""Private conversion snapshots and no-follow, exclusive staged restoration."""
import hashlib
import json
import os
import secrets
import stat
import sys
from contextlib import ExitStack, contextmanager

FILES = ('state.json', 'agent.json', 'agent.bin', 'ipxe.efi', 'manifest.json', 'created-entry')


@contextmanager
def directory(path):
    if not os.path.isabs(path) or '..' in path.split(os.sep):
        raise ValueError('Expected an absolute path without parent traversal')
    fd = os.open('/', os.O_RDONLY | os.O_DIRECTORY)
    try:
        for name in path.split(os.sep):
            if not name or name == '.':
                continue
            next_fd = os.open(name, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=fd)
            os.close(fd)
            fd = next_fd
        yield fd
    finally:
        os.close(fd)


def regular(fd, name, missing=False):
    try:
        info = os.stat(name, dir_fd=fd, follow_symlinks=False)
    except FileNotFoundError:
        if missing:
            return None
        raise
    if not stat.S_ISREG(info.st_mode) or info.st_nlink != 1:
        raise ValueError('Unsafe non-regular, linked or shared file: ' + name)
    return info


def read(fd, name):
    regular(fd, name)
    handle = os.open(name, os.O_RDONLY | os.O_NOFOLLOW, dir_fd=fd)
    with os.fdopen(handle, 'rb') as stream:
        info = os.fstat(stream.fileno())
        if not stat.S_ISREG(info.st_mode) or info.st_nlink != 1:
            raise ValueError('Unsafe opened backup file: ' + name)
        return stream.read()


def replace(fd, name, data, mode, uid=None, gid=None):
    # Never reuse predictable .restore/.new names; keep the exclusive fd open.
    temporary = '.rb-restore-' + secrets.token_hex(16)
    handle = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600, dir_fd=fd)
    try:
        with os.fdopen(handle, 'wb') as stream:
            stream.write(data)
            stream.flush()
            if uid is not None:
                os.fchown(stream.fileno(), uid, gid)
            os.fchmod(stream.fileno(), mode)
            os.fsync(stream.fileno())
        os.replace(temporary, name, src_dir_fd=fd, dst_dir_fd=fd)
    finally:
        try:
            os.unlink(temporary, dir_fd=fd)
        except FileNotFoundError:
            pass


def seal(backup):
    with directory(backup) as fd:
        state = json.loads(read(fd, 'state.json'))
        for key in ('config', 'binary'):
            with directory(os.path.dirname(state[key])) as parent:
                info = regular(parent, os.path.basename(state[key]))
                state.setdefault(key + '_uid', info.st_uid)
                state.setdefault(key + '_gid', info.st_gid)
        replace(fd, 'state.json', json.dumps(state).encode(), 0o600)
        digests = {name: hashlib.sha256(read(fd, name)).hexdigest() if regular(fd, name, True) else None for name in FILES}
        replace(fd, 'integrity.json', json.dumps(digests).encode(), 0o600)


def checked(backup, destinations=True):
    with directory(backup) as fd:
        digests = json.loads(read(fd, 'integrity.json'))
        if set(digests) != set(FILES):
            raise ValueError('Incomplete backup integrity inventory')
        if any(not digests[name] for name in ('state.json', 'agent.json', 'agent.bin')):
            raise ValueError('Required original missing from backup inventory')
        contents = {}
        for name in FILES:
            exists = regular(fd, name, True)
            if (exists is None) != (digests[name] is None):
                raise ValueError('Backup presence mismatch: ' + name)
            if exists:
                contents[name] = read(fd, name)
                if hashlib.sha256(contents[name]).hexdigest() != digests[name]:
                    raise ValueError('Backup integrity mismatch: ' + name)
        state = json.loads(contents['state.json'])
        if state['target'] != state['esp_mount'].rstrip('/') + '/EFI/iPXE':
            raise ValueError('EFI target escapes the validated ESP')
        for key in ('config', 'binary'):
            with directory(os.path.dirname(state[key])) as parent:
                regular(parent, os.path.basename(state[key]), True)
        if destinations:
            with directory(state['target']) as parent:
                for name in ('ipxe.efi', 'manifest.json'):
                    regular(parent, name, True)
        return state, contents


def pathcheck(path):
    if not os.path.isabs(path) or '..' in path.split(os.sep):
        raise ValueError('Expected absolute destination without parent traversal')
    parts = path.split(os.sep)
    for index in range(1, len(parts) + 1):
        ancestor = '/'.join(parts[:index]) or '/'
        try:
            info = os.lstat(ancestor)
        except FileNotFoundError:
            break
        if stat.S_ISLNK(info.st_mode):
            raise ValueError('Symlink destination component: ' + ancestor)
        if index < len(parts) and not stat.S_ISDIR(info.st_mode):
            raise ValueError('Non-directory destination ancestor: ' + ancestor)


def metadata(backup):
    state, _ = checked(backup, destinations=False)
    with directory(os.path.dirname(state['config'])) as parent:
        name = os.path.basename(state['config'])
        regular(parent, name)
        fd = os.open(name, os.O_RDONLY | os.O_NOFOLLOW, dir_fd=parent)
        try:
            info = os.fstat(fd)
            if not stat.S_ISREG(info.st_mode) or info.st_nlink != 1:
                raise ValueError('Unsafe opened configuration')
            os.fchown(fd, state['config_uid'], state['config_gid'])
            os.fchmod(fd, int(state['config_mode'], 8))
        finally:
            os.close(fd)


def restore(backup):
    state, contents = checked(backup)
    # Open every output directory and validate every destination before any write.
    with ExitStack() as stack:
        config_dir = stack.enter_context(directory(os.path.dirname(state['config'])))
        binary_dir = stack.enter_context(directory(os.path.dirname(state['binary'])))
        efi_dir = stack.enter_context(directory(state['target']))
        for fd, name in ((config_dir, os.path.basename(state['config'])), (binary_dir, os.path.basename(state['binary'])), (efi_dir, 'ipxe.efi'), (efi_dir, 'manifest.json')):
            regular(fd, name, True)
        for key, fd, source in (('config', config_dir, 'agent.json'), ('binary', binary_dir, 'agent.bin')):
            replace(fd, os.path.basename(state[key]), contents[source], int(state[key + '_mode'], 8), state[key + '_uid'], state[key + '_gid'])
        for name in ('ipxe.efi', 'manifest.json'):
            if name in contents:
                replace(efi_dir, name, contents[name], 0o600)
            else:
                try:
                    os.unlink(name, dir_fd=efi_dir)
                except FileNotFoundError:
                    pass


if __name__ == '__main__':
    try:
        action, backup = sys.argv[1:]
        {'seal': seal, 'check': checked, 'metadata': metadata, 'restore': restore, 'pathcheck': pathcheck}[action](backup if action == 'pathcheck' else os.path.abspath(backup))
    except (OSError, ValueError, KeyError) as error:
        raise SystemExit('Unsafe/incomplete conversion recovery: ' + str(error))
