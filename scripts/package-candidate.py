"""Package a verified candidate into a new directory without executing it."""
from pathlib import Path
import argparse
import ctypes as C
import hashlib
import json
import shutil
import struct
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
RUNTIME_FILES = (
    'GTASA_Center_ZH.exe', 'GTASASteam.dll', 'GTASA_TrainRestore_Steam.asi', 'mscomctl.ocx', 'TabCtl32.ocx',
    'zh-CN.tsv', 'GTASAConsole.ini', 'GTASAConfig.dat', 'GTASAData.dat',
    'GTASACarPics.dat', 'GTASACheats.dat', 'GTASALocs.dat',
    'app-local-controls.manifest',
)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def check_pe(path, exports=False):
    raw = Path(path).read_bytes()
    assert raw[:2] == b'MZ', 'Missing DOS header'
    pe = struct.unpack_from('<I', raw, 0x3c)[0]
    assert raw[pe:pe + 4] == b'PE\0\0'
    assert struct.unpack_from('<H', raw, pe + 4)[0] == 0x14c, 'Expected x86'
    optional = pe + 24
    assert struct.unpack_from('<H', raw, optional)[0] == 0x10b, 'Expected PE32'
    if not exports:
        return []
    count = struct.unpack_from('<H', raw, pe + 6)[0]
    sections = optional + struct.unpack_from('<H', raw, pe + 20)[0]

    def offset(rva):
        for index in range(count):
            size, address, disk_size, disk = struct.unpack_from(
                '<IIII', raw, sections + index * 40 + 8)
            if address <= rva < address + max(size, disk_size):
                result = disk + rva - address
                assert result < len(raw)
                return result
        raise ValueError('RVA outside sections')

    table = offset(struct.unpack_from('<I', raw, optional + 96)[0])
    name_count = struct.unpack_from('<I', raw, table + 24)[0]
    assert 0 < name_count < 1000
    names = offset(struct.unpack_from('<I', raw, table + 32)[0])
    result = []
    for index in range(name_count):
        position = offset(struct.unpack_from('<I', raw, names + index * 4)[0])
        end = raw.index(b'\0', position, position + 256)
        result.append(raw[position:end].decode('ascii'))
    return sorted(result)


def check_resources(exe, manifest, preset):
    kernel = C.WinDLL('kernel32', use_last_error=True)
    kernel.LoadLibraryExW.argtypes = [C.c_wchar_p, C.c_void_p, C.c_ulong]
    kernel.LoadLibraryExW.restype = C.c_void_p
    kernel.FindResourceW.argtypes = [C.c_void_p, C.c_void_p, C.c_void_p]
    kernel.FindResourceW.restype = C.c_void_p
    kernel.SizeofResource.argtypes = [C.c_void_p, C.c_void_p]
    kernel.SizeofResource.restype = C.c_ulong
    kernel.LoadResource.argtypes = [C.c_void_p, C.c_void_p]
    kernel.LoadResource.restype = C.c_void_p
    kernel.LockResource.argtypes = [C.c_void_p]
    kernel.LockResource.restype = C.c_void_p
    kernel.FreeLibrary.argtypes = [C.c_void_p]
    module = kernel.LoadLibraryExW(str(exe), None, 2)  # AS_DATAFILE; never runs code.
    if not module:
        raise C.WinError(C.get_last_error())
    try:
        def resource(kind, name):
            buffer = C.create_unicode_buffer(kind) if isinstance(kind, str) else None
            pointer = C.cast(buffer, C.c_void_p) if buffer is not None else C.c_void_p(kind)
            handle = kernel.FindResourceW(module, C.c_void_p(name), pointer)
            if not handle:
                raise C.WinError(C.get_last_error())
            size = kernel.SizeofResource(module, handle)
            data = kernel.LockResource(kernel.LoadResource(module, handle))
            assert data and size
            return C.string_at(data, size)

        assert resource(24, 1) == manifest.read_bytes(), 'Manifest differs'
        assert resource('SETTINGS', 102) == preset.read_bytes(), 'Shortcut resource differs'
    finally:
        kernel.FreeLibrary(module)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--offline-ui', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    build, ui, output = args.build.resolve(), args.offline_ui.resolve(), args.output.resolve()
    archive = output.with_suffix('.zip')
    assert not output.exists() and not archive.exists(), 'Choose new output paths'
    assert output != ROOT and ROOT not in output.parents, 'Package outside source checkout'
    status = subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT)
    assert not status, 'Commit the reviewed source before packaging'
    commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    branch = subprocess.check_output(['git', 'branch', '--show-current'], cwd=ROOT, text=True).strip()
    subprocess.run([sys.executable, '-B', str(ROOT / 'scripts/verify_files.py')], check=True)
    result = json.loads((build / 'build-result.json').read_text(encoding='utf8'))
    ui_result = json.loads((ui / 'offline-ui-result.json').read_text(encoding='utf8'))
    assert result['success'] and result['native_unit_test_passed'] and result['compile_errors'] == 0
    assert not result['trainer_launched'] and not result['real_gta_accessed']
    assert ui_result['success'] and not ui_result['real_game_discovery_or_access']
    assert not ui_result['production_trainer_launched']
    for source_receipt in (result['native_source_inputs'],ui_result['source_inputs']):
        for name,expected_hash in source_receipt.items():
            assert sha(ROOT / name)==expected_hash, 'Validation source differs: '+name
    assert ui_result['captured_pages']==8 and ui_result['native_cheat_and_lock_clicks']==88
    assert ui_result['localized_spawn_button_verified'] and ui_result['coffee_native_clicks_verified'] and ui_result['coffee_visibility_verified']
    project = build / 'GTASAConsole'
    for file in (ROOT / 'GTASAConsole').glob('*'):
        if file.suffix.lower() in ('.frm', '.frx', '.bas', '.cls', '.ctl', '.ctx', '.res'):
            assert sha(file) == sha(project / file.name), 'Build source differs: ' + file.name
    assert sha(project / 'GTASA_Center_ZH.exe') == result['exe_sha256']
    assert sha(project / 'GTASASteam.dll') == result['native_dll_sha256']
    check_pe(project / 'GTASA_Center_ZH.exe')
    check_pe(project / 'GTASA_TrainRestore_Steam.asi')
    assert sha(project / 'GTASA_TrainRestore_Steam.asi') == result['train_component_sha256']
    exports = check_pe(project / 'GTASASteam.dll', True)
    expected = sorted(line.strip().split('=')[0] for line in
                      (ROOT / 'native/exports.def').read_text().splitlines() if '=' in line)
    assert exports == expected and not any('Fixture' in name for name in exports)
    config = ROOT / 'resources/defaults/GTASAConfig.default.dat'
    assert sha(config) == sha(project / 'GTASAConfig.dat')
    records = [line.split(b'|') for line in config.read_bytes().splitlines()
               if len(line.split(b'|')) == 12 and not line.startswith(b'#')]
    assert len(records) == 69 and sum(line[3] == b'1' for line in records) == 65
    check_resources(project / 'GTASA_Center_ZH.exe', ROOT / 'GTASAConsole/app-local-controls.manifest', config)
    log = (build / 'native-build.log').read_text(encoding='utf8')
    assert all(marker in log for marker in (
        'LEGACY_GARAGE=PASS', 'FEATURE_HOOKS=PASS', 'SCRIPT_FEEDBACK_RC=PASS',
        'TRAIN_RESTORE=PASS', 'TRAIN_COMPONENT=PASS', 'PASS: plate game-thread dispatch',
        'PASS: real x86 game-thread queue'))
    assert 'warning C' not in log and 'warning LNK' not in log
    output.mkdir(parents=True)
    for name in RUNTIME_FILES:
        shutil.copy2(project / name, output / name)
    for name in ('CarPics', 'Images', 'Help'):
        shutil.copytree(project / name, output / name)
    for name in ('Restore-OriginalDrivingShortcuts.ps1', 'Restore-OriginalDrivingShortcuts.cmd', 'Install-TrainRestore.ps1', 'Install-TrainRestore.cmd'):
        shutil.copy2(ROOT / 'scripts' / name, output / name)
    for name in ('NOTICE.md', '开发记录20261006.md', '开发记录20261006-r2.md', '开发记录20261015-r3.md'):
        shutil.copy2(ROOT / name, output / name)
    (output / '使用说明.md').write_text(
        '# 20261015 r3 界面修复候选版\n\n'
        '解压到新目录，运行 GTASA_Center_ZH.exe。候选版已通过编译、原生测试和旧界面离线联调，真实游戏验收待进行。\n\n'
        'r3 修复生成载具按钮及提示的乱码，在玩家状态页右侧显示 Hot Coffee 脚本开关，并说明它的实际范围。保留 r2 已开发功能和原生接口。\n\n'
        '使用前关闭旧修改器并重启游戏，保留旧发行包及存档备份。首次启用代码功能、反馈和持续锁定时先按 Esc 暂停；女友和 Hot Coffee 修改也要求暂停。\n\n'
        '车库写入前关闭车库、在室外步行并暂停，然后读取车库、编辑、写入。飞机及直升机使用游戏原有还原流程；大型载具的空间、碰撞及读档表现待逐项实测。\n\n'
        '火车需要可选的 GTASA_TrainRestore_Steam.asi 组件。完全退出游戏后运行 Install-TrainRestore.cmd，输入游戏目录；游戏需已有 ASI 加载器。工具核对版本和文件，组件加载确认后车库才追加火车车型。本次开发未安装组件。移除前必须在组件仍启用时清空所有车库火车记录并重新保存。\n\n'
        '车牌仅支持 1 至 8 个英文字母、数字或空格。进入带车牌车辆后填写文字、点击修改按钮，再继续游戏等待更新；首次使用前先暂停。状态栏颜色和提示说明处理结果，画面效果仍待实机确认。\n\n'
        'Hot Coffee：载入稳定场景后按 Esc 暂停，在修改器的玩家状态页右侧点击，状态变为“开启（脚本）”后返回女友约会流程。它只修改审查标志，不会直接启动小游戏或补充游戏资源；完整小游戏仍待实机确认，不能保证仅开启就能进入。显示“脚本不支持”时无法开启。详见开发记录20261015-r3.md。游戏内中文反馈仍待实机确认。\n\n'
        '包内为全新默认配置。需沿用旧设置时，在旧配置的副本上运行快捷键恢复工具；工具保存完整备份，仅恢复原版 14 条驾驶辅助的启用状态。双击 CMD 工具会处理同目录的 GTASAConfig.dat。\n',
        encoding='utf8')
    metadata = {
        'version': '20261015-ui-fix-candidate-r3', 'source_commit': commit, 'branch': branch,
        'source_files_manifest_sha256': sha(ROOT / 'FILES.sha256'),
        'build': result, 'offline_ui': ui_result, 'native_warnings': 0,
        'exports': exports, 'shortcut_total': 69, 'shortcut_active': 65,
        'embedded_shortcuts_match': True, 'real_game_acceptance': 'pending',
        'fixture_exports_present': False, 'trainer_executed': False,
    }
    (output / 'validation.json').write_text(json.dumps(metadata, indent=2, ensure_ascii=False) + '\n', encoding='utf8')
    files = sorted(path for path in output.rglob('*') if path.is_file())
    (output / 'FILES.sha256').write_text(''.join(sha(path) + '  ' + path.relative_to(output).as_posix() + '\n'
                                               for path in files), encoding='utf8')
    files.append(output / 'FILES.sha256')
    with zipfile.ZipFile(archive, 'x', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as package:
        for path in files:
            package.write(path, (Path(output.name) / path.relative_to(output)).as_posix())
    with zipfile.ZipFile(archive) as package:
        assert package.testzip() is None
        assert len(package.namelist()) == len(files)
        for path in files:
            entry = (Path(output.name) / path.relative_to(output)).as_posix()
            assert hashlib.sha256(package.read(entry)).hexdigest() == sha(path)
    receipt = {'success': True, 'archive': str(archive), 'sha256': sha(archive),
               'bytes': archive.stat().st_size, 'files': len(files), 'source_commit': commit,
               'exe_sha256': result['exe_sha256'], 'dll_sha256': result['native_dll_sha256'], 'train_component_sha256': result['train_component_sha256'],
               'real_game_acceptance': 'pending'}
    archive.with_suffix('.zip.sha256').write_text(receipt['sha256'] + '  ' + archive.name + '\n', encoding='utf8')
    archive.with_suffix('.receipt.json').write_text(json.dumps(receipt, indent=2, ensure_ascii=False) + '\n', encoding='utf8')
    print(json.dumps(receipt, ensure_ascii=False))


if __name__ == '__main__':
    main()
