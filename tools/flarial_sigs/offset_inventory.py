import argparse
import json
from pathlib import Path
import re


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    adapted = root / "dll/src/flarial"
    source = (adapted / "Utils/Memory/Game/Offset/OffsetInit.cpp").read_text(encoding="utf-8")
    versions = (adapted / "Utils/VersionUtils.cpp").read_text(encoding="utf-8")
    functions = list(re.finditer(r"void OffsetInit::(\w+)\(\)", source))
    definitions = {}
    for index, match in enumerate(functions):
        end = functions[index + 1].start() if index + 1 < len(functions) else len(source)
        definitions[match[1]] = dict(re.findall(
            r'^\s*ADD_OFFSET\("([^"\n]+)",\s*(0x[\da-fA-F]+|\d+)\s*\)',
            source[match.end():end], re.MULTILINE))
    order = re.findall(r"OffsetInit::(\w+)", versions)
    effective = {}
    for function in reversed(order):
        for name, value in definitions.get(function, {}).items():
            effective[name] = {"value": int(value, 0), "source": function}
    files = {}
    upstream = root / "vendor/flarial/src"
    for path in upstream.rglob("*"):
        if path.suffix not in (".hpp", ".cpp"):
            continue
        relative = path.relative_to(upstream)
        actual = adapted / relative
        if not actual.exists():
            actual = path
        for name in set(re.findall(r'GET_OFFSET\("([^"\n]+)"\)',
                                  actual.read_text(encoding="utf-8", errors="replace"))):
            files.setdefault(name, []).append(relative.as_posix())
    for name, entry in effective.items():
        entry["explicit_2652"] = entry["source"] == "init2652"
        entry["users"] = sorted(files.get(name, []))
    report = {
        "status": "coverage inventory; inherited values are not verified",
        "total": len(effective),
        "explicit_2652": sum(entry["explicit_2652"] for entry in effective.values()),
        "inherited_used": sum(not entry["explicit_2652"] and bool(entry["users"])
                              for entry in effective.values()),
        "missing_definitions": sorted(set(files) - set(effective)),
        "offsets": dict(sorted(effective.items())),
    }
    Path(args.output).write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps({key: value for key, value in report.items() if key != "offsets"}))


if __name__ == "__main__":
    main()
