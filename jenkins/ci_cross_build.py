#!/usr/bin/env python3

# NOTE: This is for Couchbase internal CI usage.  
# This room is full of dragons, so you *will* get confused.  
# You have been warned.

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

def check_toolchain(name: str):
    toolchain_path = Path.home() / '.cbl_cross' / f'{name}-toolchain'
    if toolchain_path.exists() and toolchain_path.is_dir() and len(os.listdir(toolchain_path)) > 0:
        print(f'{toolchain_path} found, not downloading...')
        return toolchain_path

    json_data=read_manifest()
    if not name in json_data:
        raise ValueError(f'Unknown target {name}')

    if json_data[name]['toolchain']:
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
        return toolchain_path
    else:
        print("No toolchain specified, using generic installed...")
        return ""


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
    urllib.request.urlretrieve(f'http://downloads.build.couchbase.com/mobile/sysroot/{sysroot_name}', f'{sysroot_name}', show_download_progress)
    os.makedirs(sysroot_path, 0o755, True)
    print(f'Extracting {name} sysroot to {sysroot_path}...')
    if sysroot_name.endswith("zip"):
        with zipfile.ZipFile('sysroot.zip', 'r') as zip:
            # Python doesn't have support for zipped symlinks?!
            SYMLINK_TYPE  = 0xA
            zip_total = len(zip.infolist())
            zip_done = 0
            pbar = ProgressBar(maxval=zip_total)
            pbar.start()
            for zipinfo in zip.infolist():
                if (zipinfo.external_attr >> 28) == SYMLINK_TYPE:
                    linkpath = zip.read(zipinfo.filename).decode('utf-8')
                    destpath = os.path.join(sysroot_path, zipinfo.filename)
                    os.symlink(linkpath, destpath)
                else:
                    zip.extract(zipinfo, sysroot_path)
                
                zip_done += 1
                pbar.update(zip_done)
    elif sysroot_name.endswith("tar.gz"):
        # Eventually let's make them all tarball
        with tarfile.open(sysroot_name, 'r:gz') as tar:
            tar.extractall(sysroot_path, members=tar_extract_callback(tar))
    else:
        raise NotImplementedError("Unknown file type for sysroot")

    os.remove(sysroot_name)

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
    
    toolchain_path = check_toolchain(args.os)
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

    sysroot_path = Path.home() / '.cbl_cross' / f'{args.os}-sysroot'
    existing_path = os.environ['PATH']
    if toolchain_path:
        os.environ['PATH'] = f'{str(toolchain_path)}/bin:{existing_path}'

    os.environ['ROOTFS'] = str(sysroot_path)
    cmake_args=['cmake', '..', f'-DEDITION={args.edition}', f'-DCMAKE_INSTALL_PREFIX={os.getcwd()}/libcblite-{args.version}',
        '-DCMAKE_BUILD_TYPE=MinSizeRel', f'-DCMAKE_TOOLCHAIN_FILE={args.toolchain}']
    if args.os == "raspbian9" or args.os == "debian9-x86_64":
        cmake_args.append('-DCBL_STATIC_CXX=ON')
    elif args.os == "raspios10-arm64":
        cmake_args.append('-D64_BIT=ON')

    subprocess.run(cmake_args, check=True)
    subprocess.run(['make', '-j8'], check=True)

    print(f"==== Stripping binary using {args.strip_prefix}strip")
    subprocess.run([str(workspace_path / 'couchbase-lite-c' / 'jenkins' / 'strip.sh'), project_dir, 
        str(args.strip_prefix)], check=True)
    subprocess.run(['make', 'install'], check=True)

    shutil.copy2(Path(project_dir) / 'libcblite.so.sym', f'./libcblite-{args.version}')
    os.chdir(workspace)

    package_name = f'{args.product}-{args.edition}-{args.version}-{args.bld_num}-{args.os}.tar.gz'
    print()
    print(f"=== Creating {workspace}/{package_name} package ===")
    print()

    os.chdir(str(workspace_path / 'build_release'))
    shutil.copy2(workspace_path / 'product-texts' / 'mobile' / 'couchbase-lite' / 'license' / f'LICENSE_{args.edition}.txt',
        f'libcblite-{args.version}/LICENSE.txt')

    pbar = ProgressBar(maxval=3)
    pbar.start()
    with tarfile.open(f'{workspace}/{package_name}', 'w:gz') as tar:
        tar.add(f'libcblite-{args.version}/include', recursive=True)
        pbar.update(1)
        tar.add(f'libcblite-{args.version}/lib', recursive=True)
        pbar.update(2)
        tar.add(f'libcblite-{args.version}/LICENSE.txt')
        pbar.update(3)
        pbar.finish()

    symbols_package_name = f'{args.product}-{args.edition}-{args.version}-{args.bld_num}-{args.os}-symbols.tar.gz'
    with tarfile.open(f'{workspace}/{symbols_package_name}', 'w:gz') as tar:
        tar.add(f'libcblite-{args.version}/libcblite.so.sym')

    

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
