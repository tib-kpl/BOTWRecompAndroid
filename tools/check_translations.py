#!/usr/bin/env python3
"""Validate menu catalogs and Android resources before an APK is built."""
import json
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
PAIR = re.compile(r'\{("(?:[^"\\]|\\.)*"),\s*("(?:[^"\\]|\\.)*")\}')
FORMAT = re.compile(r'%(?:\d+\$)?[-+#0]*(?:\d+|\*)?(?:\.(?:\d+|\*))?(?:hh|ll|[hljztL])?[diuoxXfFeEgGaAcspn%]')


def placeholders(text):
    return FORMAT.findall(text)


def catalog(path):
    pairs = [(json.loads(a), json.loads(b)) for a, b in PAIR.findall(path.read_text(encoding='utf-8'))]
    assert pairs, f'{path}: empty catalog'
    assert len(dict(pairs)) == len(pairs), f'{path}: duplicate keys'
    for source, translated in pairs:
        assert translated.strip(), f'{path}: empty translation for {source!r}'
        assert placeholders(source) == placeholders(translated), f'{path}: format mismatch for {source!r}'
        if '##' in source:
            assert source.split('##', 1)[1] == translated.split('##', 1)[1], f'{path}: changed ImGui ID'
    return dict(pairs)


def main():
    reference = catalog(ROOT / 'runtime/ui/i18n_pt.inc')
    for path in sorted((ROOT / 'runtime/ui').glob('i18n_*.inc')):
        entries = catalog(path)
        assert reference.keys() <= entries.keys(), f'{path}: missing keys: {reference.keys() - entries.keys()}'
        print(f'{path.name}: {len(entries)} menu translations OK')
    res = ROOT / 'android/botwrecompandroid/app/src/main/res'
    defaults = {s.attrib['name']: s.text or '' for s in ET.parse(res / 'values/strings.xml').getroot()}
    for path in sorted(res.glob('values-*/strings.xml')):
        nodes = ET.parse(path).getroot()
        translated = {s.attrib['name']: s.text or '' for s in nodes}
        assert len(translated) == len(nodes), f'{path}: duplicate resource keys'
        assert defaults.keys() == translated.keys(), f'{path}: Android resource keys differ from English'
        for key, value in translated.items():
            assert value.strip(), f'{path}: empty {key}'
            assert placeholders(value) == placeholders(defaults[key]), f'{path}: format mismatch for {key}'
        print(f'{path.parent.name}: {len(translated)} Android translations OK')


if __name__ == '__main__':
    try:
        main()
    except (AssertionError, ValueError, ET.ParseError) as error:
        sys.exit(str(error))
