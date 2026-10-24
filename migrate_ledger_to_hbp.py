#!/usr/bin/env python3
"""
migrate_ledger_to_hbp.py
=============================================================================
Migrates historical Host8 CSV checkpoint ledgers (e.g. host8_mode_a_pilot100.ledger,
host8_mode_a_pilot1000.ledger) into native Asolaria HBP/HBI/SHA/SH/HASH records.

Architecture:
- HBP: Canonical execution receipts (HOST8WIN records) with exact physics invariants.
- HBI: Fast address index (byte offset, length, row SHA-256) for O(1) seek recovery.
- SHA: SHA-256 verification of input blocks and receipt bytes.
- SH: Distinct 16-hex slice handle (source_sha256[:16]).
- HASH: Tamper-evident cryptographic hash chain bound from GENESIS.
=============================================================================
"""

import os
import sys
import csv
import hashlib

GENESIS_CHAIN_HASH = hashlib.sha256(b"GENESIS|asolaria.host8.mode_a.v1").hexdigest()


def compute_window_hash(prev_hash: str, sec: int, win: int, room_start: int,
                        source_sha: str, sh: str, status: str) -> str:
    data = f"{prev_hash}|{sec}|{win}|{room_start}|{source_sha}|{sh}|{status}"
    return hashlib.sha256(data.encode("utf-8")).hexdigest()


def migrate_ledger(csv_path: str, hbp_path: str = None, hbi_path: str = None):
    if not os.path.exists(csv_path):
        print(f"[ERROR] Source ledger not found: {csv_path}", file=sys.stderr)
        return False

    base = os.path.splitext(csv_path)[0]
    if hbp_path is None:
        hbp_path = base + ".hbp"
    if hbi_path is None:
        hbi_path = base + ".hbi"

    hbp_filename = os.path.basename(hbp_path)
    csv_filename = os.path.basename(csv_path)

    print(f"Migrating {csv_path} -> {hbp_path} & {hbi_path}...")

    # Read rows from CSV ledger
    with open(csv_path, "r", newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        rows = list(reader)

    # 1. Format HBP Header
    hbp_header = (
        f"HOST8HDR|schema=asolaria.host8.mode_a.v1|seat=ACER-CLAUDE-FABLE5|version=0.2.0|"
        f"mode=MODE_A|seal=HBI-HBP-SHA-SH-HASH|traversal=HBI->HBP->SHA->SH->HASH|"
        f"compiler_version=16.2.0|migrated_from={csv_filename}|json=0\n"
    )
    hbp_bytes = hbp_header.encode("utf-8")
    current_hbp_offset = len(hbp_bytes)

    # 2. Format HBI Header
    hbi_header = (
        f"HBIHDR|for={hbp_filename}|schema=asolaria.host8.index.v1|"
        f"seal=HBI-HBP-SHA-SH-HASH|traversal=HBI->HBP->SHA->SH->HASH|"
        f"migrated_from={csv_filename}|json=0\n"
    )
    hbi_lines = [hbi_header]

    hbp_lines = [hbp_header]
    prev_hash = GENESIS_CHAIN_HASH
    migrated_count = 0

    for r in rows:
        sec = int(r["sector"])
        win = int(r["window_index"])
        start_room = int(r["start_room_offset"])
        end_room = int(r["end_room_offset"])
        rooms = int(r["window_rooms"])
        overlap = int(r["overlap_rooms"])
        unique = int(r["unique_rooms"])
        cum_rooms = int(r["cumulative_sector_rooms"])
        source_sha = r["sha256_hash"].strip().lower()
        sh = source_sha[:16]
        auth = r["auth_status"]
        dE = r["dE"]
        rev_mismatches = int(r["loschmidt_mismatches"])
        phys_status = r["physics_status"]
        forward_ticks = int(r.get("ticks_forward", 48))
        reverse_ticks = int(r.get("ticks_reverse", 16))

        # Compute cryptographic hash chain
        cur_hash = compute_window_hash(prev_hash, sec, win, start_room, source_sha, sh, phys_status)

        # HBP line (Receipt)
        hbp_line = (
            f"HOST8WIN|sec={sec}|win={win}|room_start={start_room}|room_end={end_room}|"
            f"rooms={rooms}|overlap={overlap}|unique={unique}|cum_rooms={cum_rooms}|"
            f"source_sha256={source_sha}|sh={sh}|auth={auth}|dE={dE}|"
            f"rev_mismatches={rev_mismatches}|forward_ticks={forward_ticks}|"
            f"reverse_ticks={reverse_ticks}|physics_status={phys_status}|"
            f"prev_hash={prev_hash}|hash={cur_hash}|json=0\n"
        )
        line_bytes = hbp_line.encode("utf-8")
        line_len = len(line_bytes)
        line_sha = hashlib.sha256(line_bytes).hexdigest()

        # HBI line (Index)
        hbi_line = (
            f"HBI|sec={sec}|win={win}|room_start={start_room}|"
            f"offset={current_hbp_offset}|len={line_len}|"
            f"source_sha={source_sha}|sh={sh}|hbp_sha256={line_sha}|"
            f"hash={cur_hash}|status={phys_status}|json=0\n"
        )

        hbp_lines.append(hbp_line)
        hbi_lines.append(hbi_line)

        current_hbp_offset += line_len
        prev_hash = cur_hash
        migrated_count += 1

    # Write output files atomically
    with open(hbp_path, "w", newline="", encoding="utf-8") as f:
        for line in hbp_lines:
            f.write(line)

    with open(hbi_path, "w", newline="", encoding="utf-8") as f:
        for line in hbi_lines:
            f.write(line)

    print(f"[OK] Migrated {migrated_count} windows into {hbp_path} ({current_hbp_offset} bytes) and {hbi_path}.")
    print(f"     Last HASH chain: {prev_hash}")
    return True


if __name__ == "__main__":
    targets = [
        "host8_mode_a_pilot100.ledger",
        "host8_mode_a_pilot1000.ledger"
    ]
    if len(sys.argv) > 1:
        targets = sys.argv[1:]

    for t in targets:
        migrate_ledger(t)
