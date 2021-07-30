#!/usr/bin/env python3

from genericpath import isdir
from pathlib import Path
from progressbar import ProgressBar
import json
import argparse
import urllib.request
import tarfile
import zipfile
import os
import shutil
import subprocess

SCRIPT_DIR=os.path.dirname(os.path.realpath(__file__))

json_data={}
def read_manifest():
    global json_data
    if len(json_data) == 0:
        with open(str(Path(SCRIPT_DIR) / 'cross_manifest.json'), 'r') as fin:
            data=fin.read()
        
        json_data=json.loads(data)

    return json_data

pbar=None
def show_download_progress(block_num, block_size, total_size):
    global pbar
    if pbar is None:
        pbar = ProgressBar(maxval=total_size)
        pbar.start()
    
    downloaded = block_size * block_num
    if downloaded < total_size:
        pbar.update(downloaded)
    else:
        pbar.finish()
        pbar = None

def tar_extract_callback(archive : tarfile.TarFile):
    global pbar
    count=0
    pbar = ProgressBar(maxval=len(archive.getnames()))
    pbar.start()

    for member in archive:
        count += 1
        pbar.update(count)
        yield member

    pbar.finish()
    pbar = None

def zip_extract_callback(archive: zipfile.ZipFile):
    global pbar
    count=0
    pbar = ProgressBar(maxval=len(archive.namelist()))
    pbar.start()

    for member in archive.namelist():
        count += 1
        pbar.update(count)
        yield member

    pbar.finish()
    pbar = None

def check_toolchain(name: str):
    toolchain_path = Path.home() / '.cbl_cross' / f'{name}-toolchain'
    if toolchain_path.exists() and toolchain_path.is_dir() and len(os.listdir(toolchain_path)) > 0:
        print(f'{toolchain_path} found, not downloading...')
        return

    json_data=read_manifest()
    if not name in json_data:
        raise ValueError(f'Unknown target {name}')

    # For now, assume that the toolchain is tar.gz
    print(f'Downloading {name} toolchain...')
    urllib.request.urlretrieve(json_data[name]['toolchain'], "toolchain.tar.gz", show_download_progress)
    os.makedirs(toolchain_path, 0o755, True)
    print(f'Extracting {name} toolchain to {toolchain_path}...')
    with tarfile.open('toolchain.tar.gz', 'r:gz') as tar:
        tar.extractall(toolchain_path, members=tar_extract_callback(tar))
        
    outer_dir = toolchain_path / os.listdir(toolchain_path)[0]
    files_to_move = outer_dir.glob("**/*")
    for file in files_to_move:
        relative = file.relative_to(outer_dir)
        os.makedirs(toolchain_path / relative.parent, 0o755, True)
        shutil.move(str(file), toolchain_path / relative.parent)

    os.rmdir(outer_dir)
    os.remove("toolchain.tar.gz")

def check_sysroot(name: str):
    sysroot_path = Path.home() / '.cbl_cross' / f'{name}-sysroot'
    if sysroot_path.exists() and sysroot_path.is_dir() and len(os.listdir(sysroot_path)) > 0:
        print(f'{sysroot_path} found, not downloading...')
        return

    json_data=read_manifest()
    if not name in json_data:
        raise ValueError(f'Unknown target {name}')

    print(f'Downloading {name} sysroot...')
    sysroot_name=json_data[name]['sysroot']
    urllib.request.urlretrieve(f'http://downloads.build.couchbase.com/mobile/sysroot/{sysroot_name}', 'sysroot.zip', show_download_progress)
    os.makedirs(sysroot_path, 0o755, True)
    print(f'Extracting {name} sysroot to {sysroot_path}...')
    with zipfile.ZipFile('sysroot.zip', 'r') as zip:
        zip.extractall(sysroot_path, members=zip_extract_callback(zip))

    os.remove("sysroot.zip")

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Perform a cross compilation of Couchbase Lite C')
    parser.add_argument('product', type=str, help='The product name to use for the build')
    parser.add_argument('bld_num', type=int, help='The build number for this build')
    parser.add_argument('version', type=str, help='The version of the build')
    parser.add_argument('edition', type=str, choices=['enterprise', 'community'], help='The edition of the product to build')
    parser.add_argument('os', type=str, help="The target OS to compile for")
    parser.add_argument('strip_prefix', type=str, help='The prefix to use on the binary for stripping the final product')
    parser.add_argument('toolchain', type=str, help='The CMake toolchain file to use for building')
    args = parser.parse_args()
    
    check_toolchain(args.os)
    check_sysroot(args.os)

    if 'WORKSPACE' in os.environ:
        workspace = os.environ['WORKSPACE']
    else:
        workspace = os.getcwd()
    
    workspace_path = Path(workspace)
    os.makedirs(workspace_path / 'build_release', 0o755, True)
    prop_file = workspace_path / 'publish.prop'
    project_dir = 'couchbase-lite-c'
    print(f'VERSION={args.version}')

    if args.edition == "enterprise":
        os.symlink(str(workspace_path / 'couchbase-lite-c-ee' / 'couchbase-lite-core-EE'), 
            str(workspace_path / 'couchbase-lite-c' /'vendor' / 'couchbase-lite-core-EE'))
    
    print(f"====  Cross Building Release binary using {args.toolchain}  ===")
    os.chdir(str(workspace_path / 'build_release'))

    toolchain_path = Path.home() / '.cbl_cross' / f'{args.os}-toolchain' / 'bin'
    sysroot_path = Path.home() / '.cbl_cross' / f'{args.os}-sysroot'

    existing_path = os.environ['PATH']
    os.environ['PATH'] = f'{str(toolchain_path)}:{existing_path}'
    os.environ['RASPBIAN_ROOTFS'] = str(sysroot_path)
    cmake_args=['cmake', '..', f'-DEDITION={args.edition}', f'-DCMAKE_INSTALL_PREFIX={os.getcwd()}/install',
        '-DCMAKE_BUILD_TYPE=MinSizeRel', f'-DCMAKE_TOOLCHAIN_FILE={args.toolchain}']
    if args.os == "raspbian9":
        cmake_args.append('-DCBL_STATIC_CXX=ON')

    subprocess.run(cmake_args)
    subprocess.run(['make', '-j8'])

    print(f"==== Stripping binary using {args.strip_prefix}strip")
    subprocess.run([str(workspace_path / 'couchbase-lite-c' / 'jenkins' / 'strip.sh'), project_dir, 
        str(args.strip_prefix)])
    subprocess.run(['make', 'install'])

    shutil.copy2(Path(project_dir) / 'libcblite.so.sym', './install')
    os.chdir(workspace)

    package_name = f'{args.product}-{args.os}-{args.version}-{args.bld_num}-{args.edition}.tar.gz'
    print()
    print(f"=== Creating {workspace}/{package_name} package ===")
    print()

    os.chdir(str(workspace_path / 'build_release' / 'install'))
    pbar = ProgressBar(maxval=2)
    pbar.start()
    with tarfile.open(f'{workspace}/{package_name}', 'w:gz') as tar:
        tar.add('include', recursive=True)
        pbar.update(1)
        tar.add('lib', recursive=True)
        pbar.update(2)
        pbar.finish()

    symbols_package_name = f'{args.product}-{args.os}-{args.version}-{args.bld_num}-{args.edition}-symbols.tar.gz'
    with tarfile.open(f'{workspace}/{symbols_package_name}', 'w:gz') as tar:
        tar.add("libcblite.so.sym")

    os.chdir(workspace)
    with open(prop_file, 'w') as fout:
        fout.write(f'PRODUCT={args.product}\n')
        fout.write(f'BLD_NUM={args.bld_num}\n')
        fout.write(f'VERSION={args.version}\n')
        fout.write('PKG_TYPE=tar.gz\n')
        fout.write(f'RELEASE_PACKAGE_NAME={package_name}\n')
        fout.write(f'SYMBOLS_RELEASE_PACKAGE_NAME={symbols_package_name}')

    print()
    print(f"=== Created {prop_file} ===")
    print()

    with open(prop_file, 'r') as fin:
        print(fin.read())