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


PATCH_FIELDS = [
    ("magic", 0x00, "MAGIC"),
    ("version", 0x04, "VERSION"),
    ("flags", 0x08, "FLAGS"),
    ("uart_base", 0x0C, "UART_BASE"),
    ("uart_tx_off", 0x10, "UART_TX_OFF"),
    ("uart_rx_off", 0x14, "UART_RX_OFF"),
    ("uart_stat_off", 0x18, "UART_STAT_OFF"),
    ("uart_tx_ready_mask", 0x1C, "UART_TX_MASK"),
    ("uart_rx_ready_mask", 0x20, "UART_RX_MASK"),
    ("uart_tx_ready_polarity", 0x24, "UART_TX_POL"),
    ("uart_rx_ready_polarity", 0x28, "UART_RX_POL"),
]


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
        f"PROFILE: {profile.get('seed_profile', profile.get('profile', 'UNKNOWN'))}",
        f"VERSION: {profile['version']}",
        f"LOAD: 0x{load_address:08X}",
        f"MSP0: 0x{initial_msp:08X}",
        f"ENTRY: 0x{entry:08X}",
        f"LEN: 0x{len(data):08X}",
        f"HASH32: {whole_crc:08X}",
        "",
        f"PATCH TABLE OFFSET: {format_hex32(parse_u32(profile['patch_table_offset']))}",
        "PATCH SOURCE: apply the separate profile patch sheet before first run.",
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
    lines.extend(f"- {note}" for note in profile.get("seed_notes", profile.get("notes", [])))
    return lines


def patch_bytes(patch_table):
    values = bytearray()
    for field_name, _, _ in PATCH_FIELDS:
        values.extend(parse_u32(patch_table[field_name]).to_bytes(4, "little"))
    return bytes(values)


def build_patch_document(profile):
    patch_table = profile["patch_table"]
    load_address = parse_u32(profile["load_address"])
    patch_table_offset = parse_u32(profile["patch_table_offset"])
    patch_base = load_address + patch_table_offset
    patch_hash = binascii.crc32(patch_bytes(patch_table)) & 0xFFFFFFFF

    lines = [
        "EMBER PROFILE PATCH SHEET",
        "",
        f"ARCH: {profile['architecture']}",
        f"SEED PROFILE: {profile.get('seed_profile', profile.get('profile', 'UNKNOWN'))}",
        f"PATCH PROFILE: {profile.get('patch_profile', profile.get('profile', 'UNKNOWN'))}",
        f"VERSION: {profile['version']}",
        f"SEED BASE: {format_hex32(load_address)}",
        f"PATCH TABLE OFFSET: {format_hex32(patch_table_offset)}",
        f"PATCH TABLE BASE: {format_hex32(patch_base)}",
        f"PATCH HASH32: {patch_hash:08X}",
        "",
        "WRITE32 LINES:",
    ]

    for field_name, field_offset, label in PATCH_FIELDS:
        absolute = patch_base + field_offset
        value = parse_u32(patch_table[field_name])
        lines.append(
            f"{field_offset:02X}: {label:<12} {format_hex32(value)} -> {format_hex32(absolute)}"
        )

    lines.extend(["", "OPERATOR NOTES:"])
    lines.extend(f"- {note}" for note in profile.get("patch_notes", []))
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
    parser.add_argument("--input", help="Flat seed image.")
    parser.add_argument("--text", help="Write the paper seed as plain text.")
    parser.add_argument("--pdf", help="Write the paper seed as PDF.")
    parser.add_argument("--patch-text", help="Write the profile patch sheet as plain text.")
    parser.add_argument("--patch-pdf", help="Write the profile patch sheet as PDF.")
    args = parser.parse_args()

    if not args.text and not args.pdf and not args.patch_text and not args.patch_pdf:
        raise SystemExit("at least one output path is required")

    profile = read_profile(args.profile)
    if args.text or args.pdf:
        if not args.input:
            raise SystemExit("--input is required for seed paper output")
        data = Path(args.input).read_bytes()
        if len(data) < 8:
            raise SystemExit("seed image must include at least an MSP and entry vector")
        lines = build_document(profile, data)
        if args.text:
            write_text(args.text, lines)
        if args.pdf:
            write_pdf(args.pdf, lines)

    if args.patch_text or args.patch_pdf:
        patch_lines = build_patch_document(profile)
        if args.patch_text:
            write_text(args.patch_text, patch_lines)
        if args.patch_pdf:
            write_pdf(args.patch_pdf, patch_lines)


if __name__ == "__main__":
    main()
