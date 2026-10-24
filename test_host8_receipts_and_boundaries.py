"""
test_host8_receipts_and_boundaries.py
=============================================================================
Automated Regression and Provenance Test Suite for Host8 Stage D.
Validates:
1. Historical 375-sector physics regression receipt (host8_375_sector_physics_regression.csv)
2. Corrected provenance v2 artifact (host8_375_sector_physics_provenance_v2.csv)
3. Negative security & fail-closed tests (missing files, corrupt blocks, truncation, EOF overshoot)
4. Mode A boundary window arithmetic and empirical overlap verification
=============================================================================
"""

import os
import csv
import hashlib
import tempfile
import subprocess
import pytest

PINNED_HASHES = {
    0: "2dfe7d757ecb012475806b86fcdefc602680d042db836ffbcb2bfe924f2a18ee",
    187: "6752ab9e2603cc5042f7c7a41c3ddc48ae5571fd0aeb945d0a3f63b1a948a652",
    374: "963805be806e8a290aab8dee48b0d214d03015c84e02bf844f0de87e2b37c10a",
}

SECTOR_ROOMS = 100_000_000
SECTOR_BYTES = SECTOR_ROOMS * 8  # 800 MB
WINDOW_ROOMS = 13_824            # 24^3
WINDOW_BYTES = WINDOW_ROOMS * 8  # 110,592 bytes
WINDOW_TRITS = WINDOW_ROOMS * 27 # 373,248 trits
TOTAL_SECTORS = 375

RECEIPT_V1 = "host8_375_sector_physics_regression.csv"
RECEIPT_V2 = "host8_375_sector_physics_provenance_v2.csv"
BOUNDARY_RECEIPT = "host8_mode_a_boundary_results.csv"


class TestPublishedReceipts:
    """Validates the historical published 375-sector physics regression receipt."""

    def test_v1_file_exists_and_hash(self):
        assert os.path.exists(RECEIPT_V1), f"Historical receipt {RECEIPT_V1} missing!"
        with open(RECEIPT_V1, "rb") as f:
            h = hashlib.sha256(f.read()).hexdigest()
        assert h in (
            "1ba45e2d4ea26a8bdf3b2c4a2bd5708551f79e604825c46c765e6a8c2916502e", # Windows CRLF
            "1a4b379eed784d5477db54342bc316091c84f719a5475cb3c6ebe27696070667"  # Linux/CI LF
        ), f"V1 receipt SHA-256 diverged: {h}"

    def test_v1_structural_integrity(self):
        with open(RECEIPT_V1, "r", newline="") as f:
            reader = csv.DictReader(f)
            rows = list(reader)

        assert len(rows) == TOTAL_SECTORS, f"Expected {TOTAL_SECTORS} rows, got {len(rows)}"

        seen_sectors = set()
        hashes = set()
        total_mismatches = 0

        for r in rows:
            sec = int(r["sector"])
            seen_sectors.add(sec)
            assert int(r["room_offset"]) == 0
            assert int(r["byte_offset"]) == 0
            assert int(r["num_bytes"]) == WINDOW_BYTES
            assert int(r["num_rooms"]) == WINDOW_ROOMS
            assert int(r["num_trits"]) == WINDOW_TRITS
            assert int(r["forward_ticks"]) == 48
            assert int(r["reverse_ticks"]) == 16
            assert float(r["e_total_dev"]) < 1e-12

            mismatches = int(r["loschmidt_mismatches"])
            assert mismatches == 0
            total_mismatches += mismatches

            sha = r["sha256_hash"].lower()
            assert len(sha) == 64
            hashes.add(sha)

            if sec in PINNED_HASHES:
                assert sha == PINNED_HASHES[sec], f"Pinned sector {sec} hash mismatch!"

        assert len(seen_sectors) == TOTAL_SECTORS
        assert min(seen_sectors) == 0
        assert max(seen_sectors) == TOTAL_SECTORS - 1
        assert len(hashes) == TOTAL_SECTORS, "All 375 sector hashes must be distinct"
        assert total_mismatches == 0


class TestProvenanceV2Semantics:
    """Validates the corrected provenance v2 artifact distinguishing PINNED_MATCH from HASH_RECORDED."""

    def test_v2_file_exists(self):
        assert os.path.exists(RECEIPT_V2), f"Provenance v2 receipt {RECEIPT_V2} missing!"
        with open(RECEIPT_V2, "rb") as f:
            h = hashlib.sha256(f.read()).hexdigest()
        assert h in (
            "51034733e1543ad5e6e67adf573c9a3b1a1cf1226d40352adb4c22c801265b02", # Windows CRLF
            "4a57162b06a013dc79a16b2bc57fba80af3bf69390de8ea2fa174149da69f0a8"  # Linux/CI LF
        ), f"V2 receipt SHA-256 diverged: {h}"

    def test_v2_authentication_and_physics_status(self):
        with open(RECEIPT_V2, "r", newline="") as f:
            reader = csv.DictReader(f)
            rows = list(reader)

        assert len(rows) == TOTAL_SECTORS

        pinned_matches = 0
        hashes_recorded = 0
        hash_mismatches = 0
        read_failures = 0
        verified_count = 0

        for r in rows:
            sec = int(r["sector"])
            auth = r["auth_status"]
            phys = r["physics_status"]

            if sec in PINNED_HASHES:
                assert auth == "PINNED_MATCH", f"Sector {sec} must be PINNED_MATCH, got {auth}"
                pinned_matches += 1
            else:
                assert auth == "HASH_RECORDED", f"Sector {sec} must be HASH_RECORDED, got {auth}"
                hashes_recorded += 1

            if auth == "HASH_MISMATCH": hash_mismatches += 1
            if auth == "READ_FAILURE": read_failures += 1
            if phys == "VERIFIED": verified_count += 1

            assert int(r["loschmidt_mismatches"]) == 0
            assert float(r["e_total_dev"]) < 1e-12

        assert pinned_matches == 3, f"Expected exactly 3 pinned matches, got {pinned_matches}"
        assert hashes_recorded == 372, f"Expected exactly 372 recorded hashes, got {hashes_recorded}"
        assert hash_mismatches == 0
        assert read_failures == 0
        assert verified_count == TOTAL_SECTORS


class TestNegativeScenariosAndFixtures:
    """Negative tests for fail-closed verification: missing files, corruption, truncation, EOF overshoot."""

    def test_negative_missing_sector_file(self):
        exe = "./asolaria_equilibrium_suite.exe" if os.name == "nt" else "./asolaria_equilibrium_suite"
        if not os.path.exists(exe):
            pytest.skip("Physics engine executable not built.")

        with tempfile.TemporaryDirectory() as tmpdir:
            out_csv = os.path.join(tmpdir, "out.csv")
            # Empty directory - sector-000.rooms does not exist
            res = subprocess.run([
                exe, "--host8-sectors", "0",
                "--host8-dir", tmpdir,
                "--output", out_csv,
                "--host8-strict"
            ], capture_output=True, text=True)
            assert res.returncode != 0, "Missing sector file must fail closed in strict mode!"
            assert "Unable to open or read" in (res.stdout + res.stderr)

    def test_negative_corrupted_pinned_block(self):
        exe = "./asolaria_equilibrium_suite.exe" if os.name == "nt" else "./asolaria_equilibrium_suite"
        if not os.path.exists(exe):
            pytest.skip("Physics engine executable not built.")

        with tempfile.TemporaryDirectory() as tmpdir:
            out_csv = os.path.join(tmpdir, "out.csv")
            # Create a corrupted sector 000 file with random bytes
            sec0_path = os.path.join(tmpdir, "sector-000.rooms")
            with open(sec0_path, "wb") as f:
                f.write(b"\x00" * WINDOW_BYTES)

            res = subprocess.run([
                exe, "--host8-sectors", "0",
                "--host8-dir", tmpdir,
                "--output", out_csv,
                "--host8-strict"
            ], capture_output=True, text=True)
            assert res.returncode != 0, "Corrupted pinned hash must fail strict verification!"
            assert "Cryptographic mismatch" in (res.stdout + res.stderr)

    def test_negative_truncated_read(self):
        exe = "./asolaria_equilibrium_suite.exe" if os.name == "nt" else "./asolaria_equilibrium_suite"
        if not os.path.exists(exe):
            pytest.skip("Physics engine executable not built.")

        with tempfile.TemporaryDirectory() as tmpdir:
            out_csv = os.path.join(tmpdir, "out.csv")
            # File is only 50,000 bytes (less than required 110,592 bytes)
            sec0_path = os.path.join(tmpdir, "sector-000.rooms")
            with open(sec0_path, "wb") as f:
                f.write(b"\xAA" * 50_000)

            res = subprocess.run([
                exe, "--host8-sectors", "0",
                "--host8-dir", tmpdir,
                "--output", out_csv,
                "--host8-strict"
            ], capture_output=True, text=True)
            assert res.returncode != 0, "Truncated sector file must fail closed!"

    def test_negative_offset_beyond_eof(self):
        exe = "./asolaria_equilibrium_suite.exe" if os.name == "nt" else "./asolaria_equilibrium_suite"
        if not os.path.exists(exe):
            pytest.skip("Physics engine executable not built.")

        with tempfile.TemporaryDirectory() as tmpdir:
            out_csv = os.path.join(tmpdir, "negative_eof.csv")
            sec0_path = os.path.join(tmpdir, "sector-000.rooms")
            # Create a fixture sector file of size WINDOW_BYTES (110,592 bytes = 13,824 rooms)
            with open(sec0_path, "wb") as f:
                f.write(b"\x00" * WINDOW_BYTES)

            # Offset 99,990,000 rooms is far beyond the 13,824 rooms present in this fixture
            res = subprocess.run([
                exe, "--host8-sectors", "0",
                "--host8-dir", tmpdir,
                "--host8-offset", "99990000",
                "--output", out_csv,
                "--host8-strict"
            ], capture_output=True, text=True)
            assert res.returncode != 0, "Reading beyond sector EOF must fail closed!"
            assert "Unable to open or read" in (res.stdout + res.stderr)


class TestModeABoundaryAccounting:
    """Validates the exact mathematics of Mode A boundary window traversal and empirical overlap."""

    def test_boundary_arithmetic(self):
        # Full non-overlapping windows
        full_windows = SECTOR_ROOMS // WINDOW_ROOMS
        assert full_windows == 7233
        covered_non_overlapping = full_windows * WINDOW_ROOMS
        assert covered_non_overlapping == 99_988_992

        # Remainder rooms at end of sector
        remainder_rooms = SECTOR_ROOMS - covered_non_overlapping
        assert remainder_rooms == 11_008

        # Mode A overlapping window start
        final_offset = SECTOR_ROOMS - WINDOW_ROOMS
        assert final_offset == 99_986_176
        assert final_offset * 8 == 799_889_408
        assert (final_offset + WINDOW_ROOMS) * 8 == SECTOR_BYTES

        # Overlap with window 7232 (which ended at 99,988,992)
        overlap_rooms = covered_non_overlapping - final_offset
        assert overlap_rooms == 2_816
        assert overlap_rooms * 8 == 22_528

        # Newly covered unique rooms
        newly_covered = WINDOW_ROOMS - overlap_rooms
        assert newly_covered == remainder_rooms
        assert newly_covered == 11_008

        # Total coverage
        total_unique = covered_non_overlapping + newly_covered
        assert total_unique == SECTOR_ROOMS
        assert total_unique * 8 == SECTOR_BYTES

        # Full 375-sector traversal metrics
        total_windows_all = TOTAL_SECTORS * (full_windows + 1)
        assert total_windows_all == 2_712_750
        total_rooms_all = TOTAL_SECTORS * SECTOR_ROOMS
        assert total_rooms_all == 37_500_000_000
        assert total_rooms_all * 8 == 300_000_000_000

    def test_boundary_receipt_if_present(self):
        if not os.path.exists(BOUNDARY_RECEIPT):
            pytest.skip("Boundary receipt not generated yet.")

        with open(BOUNDARY_RECEIPT, "r", newline="") as f:
            reader = csv.DictReader(f)
            rows = list(reader)

        assert len(rows) == 6  # 2 windows x 3 sectors
        for r in rows:
            assert int(r["loschmidt_mismatches"]) == 0
            assert float(r["dE"]) < 1e-12
            assert r["physics_status"] == "VERIFIED"
            if int(r["window_index"]) == 7233:
                assert int(r["start_room_offset"]) == 99_986_176
                assert int(r["end_room_offset"]) == 100_000_000
                assert int(r["overlap_rooms"]) == 2_816
                assert int(r["unique_rooms"]) == 11_008
                assert int(r["cumulative_sector_rooms"]) == 100_000_000
            elif int(r["window_index"]) == 7232:
                assert int(r["start_room_offset"]) == 99_975_168
                assert int(r["end_room_offset"]) == 99_988_992
                assert int(r["overlap_rooms"]) == 0
                assert int(r["unique_rooms"]) == 13_824
                assert int(r["cumulative_sector_rooms"]) == 99_988_992


class TestModeABatchExecution:
    """Validates Mode A batch execution, ledger schema, and deterministic resumption."""

    def test_mode_a_execution_and_resumption(self):
        exe = "./asolaria_equilibrium_suite.exe" if os.name == "nt" else "./asolaria_equilibrium_suite"
        if not os.path.exists(exe):
            pytest.skip("Physics engine executable not built.")

        with tempfile.TemporaryDirectory() as tmpdir:
            # Create a mock sector file for non-pinned sector 001 with 3 windows of data (3 * 110,592 bytes)
            sec1_path = os.path.join(tmpdir, "sector-001.rooms")
            with open(sec1_path, "wb") as f:
                f.write(b"\x01\x02\x03\x04\x05\x06\x07\x08" * (WINDOW_ROOMS * 3))

            ledger_file = os.path.join(tmpdir, "test_mode_a.ledger")
            manifest_file = os.path.join(tmpdir, "test_manifest.json")

            # Run 2 windows on sector 1
            res1 = subprocess.run([
                exe, "--host8-mode-a",
                "--host8-dir", tmpdir,
                "--start-sector", "1",
                "--end-sector", "1",
                "--windows", "2",
                "--checkpoint-ledger", ledger_file,
                "--host8-manifest", manifest_file,
            ], capture_output=True, text=True)
            assert res1.returncode == 0, f"Mode A run 1 failed: {res1.stderr}"

            # Verify ledger has header + 2 entries
            with open(ledger_file, "r") as f:
                lines = [l.strip() for l in f if l.strip()]
            assert len(lines) == 3, f"Expected header + 2 rows, got {len(lines)}"

            # Resume and advance to target 3 windows
            res2 = subprocess.run([
                exe, "--host8-mode-a",
                "--host8-dir", tmpdir,
                "--start-sector", "1",
                "--end-sector", "1",
                "--windows", "3",
                "--resume",
                "--checkpoint-ledger", ledger_file,
                "--host8-manifest", manifest_file,
            ], capture_output=True, text=True)
            assert res2.returncode == 0, f"Mode A run 2 (resume) failed: {res2.stderr}"

            # Verify output shows 2 resumed windows
            assert "2 resumed" in res2.stdout or "Windows Resumed from Ledger : 2" in res2.stdout

            with open(ledger_file, "r") as f:
                lines = [l.strip() for l in f if l.strip()]
            assert len(lines) == 4, f"Expected header + 3 rows, got {len(lines)}"
