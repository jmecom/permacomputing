#!/usr/bin/env python3

import argparse
import binascii
import json
from pathlib import Path
from textwrap import wrap

try:
    from reportlab.lib.pagesizes import A4
    from reportlab.pdfgen import canvas
except ImportError:
    A4 = None
    canvas = None


def parse_u32(value):
    if isinstance(value, int):
        return value
    return int(value, 0)


def format_hex32(value):
    return f"0x{value:08X}"


def row_sum(data):
    return sum(data) & 0xFF


def row_xor(data):
    value = 0
    for byte in data:
        value ^= byte
    return value


def read_profile(path):
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def format_rows(data, line_bytes):
    rows = []
    for offset in range(0, len(data), line_bytes):
        chunk = data[offset : offset + line_bytes]
        groups = wrap(chunk.hex().upper(), 4)
        rows.append(
            f"{offset:04X}: {' '.join(groups):<39} | S={row_sum(chunk):02X} X={row_xor(chunk):02X}"
        )
    return rows


def build_document(profile, data):
    load_address = parse_u32(profile["load_address"])
    initial_msp = int.from_bytes(data[0:4], "little")
    entry = int.from_bytes(data[4:8], "little")
    line_bytes = int(profile.get("line_bytes", 16))
    whole_crc = binascii.crc32(data) & 0xFFFFFFFF

    lines = [
        "EMBER PAPER SEED",
        "",
        f"ARCH: {profile['architecture']}",
        f"PROFILE: {profile.get('profile', 'UNKNOWN')}",
        f"VERSION: {profile['version']}",
        f"LOAD: 0x{load_address:08X}",
        f"MSP0: 0x{initial_msp:08X}",
        f"ENTRY: 0x{entry:08X}",
        f"LEN: 0x{len(data):08X}",
        f"HASH32: {whole_crc:08X}",
        f"RECOVERY HANDOFF: {format_hex32(parse_u32(profile['recovery_handoff_address']))}",
        f"RECOVERY PAYLOAD: {format_hex32(parse_u32(profile['recovery_payload_address']))}",
        f"RECOVERY LIMIT: {format_hex32(parse_u32(profile['recovery_payload_limit']))}",
        f"HANDOFF MAGIC: {format_hex32(parse_u32(profile['recovery_handoff_magic']))}",
        "",
    ]

    lines.extend(
        [
        "ROWS: little-endian image bytes grouped as 16-bit hex words.",
        "CHECK: per-row byte sum mod 256 and bytewise XOR in the right column.",
        "MANUAL: S = low 8 bits of the byte sum. X = XOR of all bytes in the row.",
        "",
        ]
    )
    lines.extend(format_rows(data, line_bytes))
    lines.extend(["", "OPERATOR NOTES:"])
    lines.extend(f"- {note}" for note in profile.get("notes", []))
    return lines


def write_text(path, lines):
    output = Path(path)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_pdf(path, lines):
    if canvas is None or A4 is None:
        raise SystemExit("reportlab is required for PDF output")

    output = Path(path)
    output.parent.mkdir(parents=True, exist_ok=True)

    page_width, page_height = A4
    margin = 36
    line_height = 11
    font_name = "Courier"
    font_size = 9

    doc = canvas.Canvas(str(output), pagesize=A4)
    doc.setTitle("Ember paper seed")
    doc.setAuthor("permacomputing")
    doc.setFont(font_name, font_size)

    y = page_height - margin
    for line in lines:
        if y < margin:
            doc.showPage()
            doc.setFont(font_name, font_size)
            y = page_height - margin
        doc.drawString(margin, y, line)
        y -= line_height

    doc.save()


def main():
    parser = argparse.ArgumentParser(description="Render an ember seed as text and PDF.")
    parser.add_argument("--profile", required=True, help="Path to the target profile JSON.")
    parser.add_argument("--input", required=True, help="Flat seed image.")
    parser.add_argument("--text", help="Write the paper seed as plain text.")
    parser.add_argument("--pdf", help="Write the paper seed as PDF.")
    args = parser.parse_args()

    if not args.text and not args.pdf:
        raise SystemExit("at least one output path is required")

    profile = read_profile(args.profile)
    data = Path(args.input).read_bytes()
    if len(data) < 8:
        raise SystemExit("seed image must include at least an MSP and entry vector")
    lines = build_document(profile, data)
    if args.text:
        write_text(args.text, lines)
    if args.pdf:
        write_pdf(args.pdf, lines)


if __name__ == "__main__":
    main()
