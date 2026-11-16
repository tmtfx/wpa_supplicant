#!/bin/python3
import sys
import os
from pathlib import Path

def update_catkeys(locales_dir_path=None):
    if locales_dir_path:
        locales_dir = Path(locales_dir_path)
    else:
        # Default to locales dir next to this script or in current directory
        script_dir = Path(__file__).resolve().parent
        candidate = script_dir / "locales"
        if candidate.is_dir():
            locales_dir = candidate
        else:
            locales_dir = Path("wpa_gui-haiku/locales")

    en_file = locales_dir / "en.catkeys"
    if not en_file.is_file():
        print(f"Error: Base catalog not found at {en_file}", file=sys.stderr)
        return 1

    with open(en_file, "r", encoding="utf-8") as f:
        en_lines = [line.rstrip("\r\n") for line in f]

    if not en_lines:
        print(f"Error: {en_file} is empty", file=sys.stderr)
        return 1

    en_header_parts = en_lines[0].split("\t")
    if len(en_header_parts) < 4:
        print(f"Error: Invalid header in {en_file}", file=sys.stderr)
        return 1

    archive_version = en_header_parts[0]
    en_signature = en_header_parts[2]
    new_fingerprint = en_header_parts[3]

    en_entries = []
    for line in en_lines[1:]:
        if not line:
            continue
        parts = line.split("\t")
        source = parts[0]
        context = parts[1] if len(parts) > 1 else ""
        comment = parts[2] if len(parts) > 2 else ""
        translation = parts[3] if len(parts) > 3 else source
        en_entries.append((source, context, comment, translation))

    catkey_files = sorted(locales_dir.glob("*.catkeys"))
    updated_count = 0

    for catkey_path in catkey_files:
        if catkey_path.name == "en.catkeys":
            continue

        with open(catkey_path, "r", encoding="utf-8") as f:
            target_lines = [line.rstrip("\r\n") for line in f]

        if not target_lines:
            continue

        target_header_parts = target_lines[0].split("\t")
        target_version = target_header_parts[0] if target_header_parts else archive_version
        target_lang = target_header_parts[1] if len(target_header_parts) > 1 else catkey_path.stem
        target_sig = target_header_parts[2] if len(target_header_parts) > 2 else en_signature

        # Read existing translations
        translations = {}
        for line in target_lines[1:]:
            if not line:
                continue
            parts = line.split("\t")
            source = parts[0]
            context = parts[1] if len(parts) > 1 else ""
            comment = parts[2] if len(parts) > 2 else ""
            trans = parts[3] if len(parts) > 3 else ""
            translations[(source, context, comment)] = trans

        # Reconstruct updated catalog
        output_lines = [f"{target_version}\t{target_lang}\t{target_sig}\t{new_fingerprint}"]
        for source, context, comment, fallback in en_entries:
            trans = translations.get((source, context, comment), fallback)
            output_lines.append(f"{source}\t{context}\t{comment}\t{trans}")

        with open(catkey_path, "w", encoding="utf-8") as f:
            f.write("\n".join(output_lines) + "\n")

        updated_count += 1
        print(f"Updated {catkey_path.name} ({target_lang})")

    print(f"\nDone: {updated_count} catkeys synchronized with fingerprint {new_fingerprint}.")
    return 0

if __name__ == "__main__":
    locales_arg = sys.argv[1] if len(sys.argv) > 1 else None
    sys.exit(update_catkeys(locales_arg))
