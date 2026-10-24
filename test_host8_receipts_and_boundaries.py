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
            hbp_file = os.path.join(tmpdir, "test_mode_a.hbp")
            hbi_file = os.path.join(tmpdir, "test_mode_a.hbi")

            # Run 2 windows on sector 1
            res1 = subprocess.run([
                exe, "--host8-mode-a",
                "--host8-dir", tmpdir,
                "--start-sector", "1",
                "--end-sector", "1",
                "--windows", "2",
                "--hbp", hbp_file,
                "--hbi", hbi_file,
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
                "--hbp", hbp_file,
                "--hbi", hbi_file,
                "--checkpoint-ledger", ledger_file,
                "--host8-manifest", manifest_file,
            ], capture_output=True, text=True)
            assert res2.returncode == 0, f"Mode A run 2 (resume) failed: {res2.stderr}"

            # Verify output shows 2 resumed windows
            assert "2 resumed" in res2.stdout or "Windows Resumed from Ledger : 2" in res2.stdout

            with open(ledger_file, "r") as f:
                lines = [l.strip() for l in f if l.strip()]
            assert len(lines) == 4, f"Expected header + 3 rows, got {len(lines)}"


class TestNativeHbpFabricAndIntegrity:
    """Validates the native HBI/HBP/SHA/SH/HASH fabric and fail-closed integrity checks."""

    def test_pilot100_verification(self):
        exe = "./asolaria_equilibrium_suite.exe" if os.name == "nt" else "./asolaria_equilibrium_suite"
        if not os.path.exists(exe):
            pytest.skip("Physics engine executable not built.")
        if not (os.path.exists("host8_mode_a_pilot100.hbp") and os.path.exists("host8_mode_a_pilot100.hbi")):
            pytest.skip("Pilot 100 HBP/HBI not present.")

        res = subprocess.run([
            exe, "--verify-hbp",
            "--hbp", "host8_mode_a_pilot100.hbp",
            "--hbi", "host8_mode_a_pilot100.hbi"
        ], capture_output=True, text=True)
        assert res.returncode == 0, f"Pilot 100 HBP verification failed: {res.stderr}\n{res.stdout}"
        assert "Windows Verified: 100" in res.stdout
        assert "100% UNBROKEN CRYPTOGRAPHIC INTEGRITY" in res.stdout

    def test_pilot1000_verification(self):
        exe = "./asolaria_equilibrium_suite.exe" if os.name == "nt" else "./asolaria_equilibrium_suite"
        if not os.path.exists(exe):
            pytest.skip("Physics engine executable not built.")
        if not (os.path.exists("host8_mode_a_pilot1000.hbp") and os.path.exists("host8_mode_a_pilot1000.hbi")):
            pytest.skip("Pilot 1000 HBP/HBI not present.")

        res = subprocess.run([
            exe, "--verify-hbp",
            "--hbp", "host8_mode_a_pilot1000.hbp",
            "--hbi", "host8_mode_a_pilot1000.hbi"
        ], capture_output=True, text=True)
        assert res.returncode == 0, f"Pilot 1000 HBP verification failed: {res.stderr}\n{res.stdout}"
        assert "Windows Verified: 1000" in res.stdout
        assert "100% UNBROKEN CRYPTOGRAPHIC INTEGRITY" in res.stdout

    def test_negative_corrupted_hbp_byte(self):
        exe = "./asolaria_equilibrium_suite.exe" if os.name == "nt" else "./asolaria_equilibrium_suite"
        if not os.path.exists(exe) or not os.path.exists("host8_mode_a_pilot100.hbp"):
            pytest.skip("Prerequisites not present.")

        with tempfile.TemporaryDirectory() as tmpdir:
            bad_hbp = os.path.join(tmpdir, "corrupted.hbp")
            bad_hbi = os.path.join(tmpdir, "corrupted.hbi")

            with open("host8_mode_a_pilot100.hbp", "rb") as f:
                hbp_data = bytearray(f.read())
            # Corrupt a byte in the middle of a window receipt
            hbp_data[500] = ord(b'X') if hbp_data[500] != ord(b'X') else ord(b'Y')

            with open(bad_hbp, "wb") as f:
                f.write(hbp_data)
            with open("host8_mode_a_pilot100.hbi", "rb") as f:
                with open(bad_hbi, "wb") as out_f:
                    out_f.write(f.read())

            res = subprocess.run([
                exe, "--verify-hbp",
                "--hbp", bad_hbp,
                "--hbi", bad_hbi
            ], capture_output=True, text=True)
            assert res.returncode != 0, "Corrupted HBP byte must fail verification!"
            assert "HBP digest mismatch" in (res.stdout + res.stderr)

    def test_negative_altered_hbi_offset(self):
        exe = "./asolaria_equilibrium_suite.exe" if os.name == "nt" else "./asolaria_equilibrium_suite"
        if not os.path.exists(exe) or not os.path.exists("host8_mode_a_pilot100.hbp"):
            pytest.skip("Prerequisites not present.")

        with tempfile.TemporaryDirectory() as tmpdir:
            bad_hbp = os.path.join(tmpdir, "bad_offset.hbp")
            bad_hbi = os.path.join(tmpdir, "bad_offset.hbi")

            with open("host8_mode_a_pilot100.hbp", "rb") as f:
                with open(bad_hbp, "wb") as out_f:
                    out_f.write(f.read())

            with open("host8_mode_a_pilot100.hbi", "r") as f:
                lines = f.readlines()
            # Tamper with the offset in the second line
            lines[1] = lines[1].replace("offset=", "offset=999999")

            with open(bad_hbi, "w") as f:
                f.writelines(lines)

            res = subprocess.run([
                exe, "--verify-hbp",
                "--hbp", bad_hbp,
                "--hbi", bad_hbi
            ], capture_output=True, text=True)
            assert res.returncode != 0, "Altered HBI offset must fail verification!"

    def test_negative_tampered_hbi_sha(self):
        exe = "./asolaria_equilibrium_suite.exe" if os.name == "nt" else "./asolaria_equilibrium_suite"
        if not os.path.exists(exe) or not os.path.exists("host8_mode_a_pilot100.hbp"):
            pytest.skip("Prerequisites not present.")

        with tempfile.TemporaryDirectory() as tmpdir:
            bad_hbp = os.path.join(tmpdir, "bad_sha.hbp")
            bad_hbi = os.path.join(tmpdir, "bad_sha.hbi")

            with open("host8_mode_a_pilot100.hbp", "rb") as f:
                with open(bad_hbp, "wb") as out_f:
                    out_f.write(f.read())

            with open("host8_mode_a_pilot100.hbi", "r") as f:
                lines = f.readlines()
            # Corrupt hbp_sha256 in second entry
            lines[1] = lines[1].replace("hbp_sha256=", "hbp_sha256=0000000000000000000000000000000000000000000000000000000000000000")

            with open(bad_hbi, "w") as f:
                f.writelines(lines)

            res = subprocess.run([
                exe, "--verify-hbp",
                "--hbp", bad_hbp,
                "--hbi", bad_hbi
            ], capture_output=True, text=True)
            assert res.returncode != 0, "Tampered HBI SHA-256 must fail verification!"

    def test_negative_broken_hash_chain(self):
        exe = "./asolaria_equilibrium_suite.exe" if os.name == "nt" else "./asolaria_equilibrium_suite"
        if not os.path.exists(exe) or not os.path.exists("host8_mode_a_pilot100.hbp"):
            pytest.skip("Prerequisites not present.")

        with tempfile.TemporaryDirectory() as tmpdir:
            bad_hbp = os.path.join(tmpdir, "broken_chain.hbp")
            bad_hbi = os.path.join(tmpdir, "broken_chain.hbi")

            with open("host8_mode_a_pilot100.hbp", "r", encoding="utf-8", newline="\n") as f:
                hbp_lines = f.readlines()
            with open("host8_mode_a_pilot100.hbi", "r", encoding="utf-8", newline="\n") as f:
                hbi_lines = f.readlines()

            fake_hash = "deadbeef00000000000000000000000000000000000000000000000000000000"

            # Update HBP line 2
            hbp_p = hbp_lines[2].strip().split("|")
            for i, p in enumerate(hbp_p):
                if p.startswith("hash="):
                    hbp_p[i] = f"hash={fake_hash}"
            hbp_lines[2] = "|".join(hbp_p) + "\n"

            new_hbp_sha = hashlib.sha256(hbp_lines[2].encode("utf-8")).hexdigest()
            new_len = len(hbp_lines[2].encode("utf-8"))

            # Update HBI line 2
            hbi_p = hbi_lines[2].strip().split("|")
            for i, p in enumerate(hbi_p):
                if p.startswith("hash="):
                    hbi_p[i] = f"hash={fake_hash}"
                elif p.startswith("hbp_sha256="):
                    hbi_p[i] = f"hbp_sha256={new_hbp_sha}"
                elif p.startswith("len="):
                    hbi_p[i] = f"len={new_len}"
            hbi_lines[2] = "|".join(hbi_p) + "\n"

            with open(bad_hbp, "w", encoding="utf-8", newline="\n") as out_f:
                out_f.writelines(hbp_lines)
            with open(bad_hbi, "w", encoding="utf-8", newline="\n") as out_f:
                out_f.writelines(hbi_lines)

            res = subprocess.run([
                exe, "--verify-hbp",
                "--hbp", bad_hbp,
                "--hbi", bad_hbi
            ], capture_output=True, text=True)
            assert res.returncode != 0, "Broken hash chain must fail verification!"
            assert "HASH chain broken" in (res.stdout + res.stderr)

    def test_negative_corrupted_sh_handle(self):
        exe = "./asolaria_equilibrium_suite.exe" if os.name == "nt" else "./asolaria_equilibrium_suite"
        if not os.path.exists(exe) or not os.path.exists("host8_mode_a_pilot100.hbp"):
            pytest.skip("Prerequisites not present.")

        with tempfile.TemporaryDirectory() as tmpdir:
            bad_hbp = os.path.join(tmpdir, "bad_sh.hbp")
            bad_hbi = os.path.join(tmpdir, "bad_sh.hbi")

            with open("host8_mode_a_pilot100.hbp", "r", encoding="utf-8", newline="\n") as f:
                hbp_lines = f.readlines()
            # Corrupt SH handle in line 2
            hbp_lines[2] = hbp_lines[2].replace("|sh=", "|sh=0123456789abcdef_bad|old_sh=")

            with open("host8_mode_a_pilot100.hbi", "r", encoding="utf-8", newline="\n") as f:
                hbi_lines = f.readlines()

            with open(bad_hbp, "w", encoding="utf-8", newline="\n") as f:
                f.writelines(hbp_lines)
            with open(bad_hbi, "w", encoding="utf-8", newline="\n") as f:
                f.writelines(hbi_lines)

            res = subprocess.run([
                exe, "--verify-hbp",
                "--hbp", bad_hbp,
                "--hbi", bad_hbi
            ], capture_output=True, text=True)
            assert res.returncode != 0, "Corrupted SH handle must fail verification!"

    def test_negative_truncated_commit(self):
        exe = "./asolaria_equilibrium_suite.exe" if os.name == "nt" else "./asolaria_equilibrium_suite"
        if not os.path.exists(exe) or not os.path.exists("host8_mode_a_pilot100.hbp"):
            pytest.skip("Prerequisites not present.")

        with tempfile.TemporaryDirectory() as tmpdir:
            bad_hbp = os.path.join(tmpdir, "truncated.hbp")
            bad_hbi = os.path.join(tmpdir, "truncated.hbi")

            with open("host8_mode_a_pilot100.hbp", "rb") as f:
                data = f.read()
            # Truncate last 25 bytes (partial line without newline)
            with open(bad_hbp, "wb") as f:
                f.write(data[:-25])

            with open("host8_mode_a_pilot100.hbi", "rb") as f:
                with open(bad_hbi, "wb") as f_out:
                    f_out.write(f.read())

            res = subprocess.run([
                exe, "--verify-hbp",
                "--hbp", bad_hbp,
                "--hbi", bad_hbi
            ], capture_output=True, text=True)
            assert res.returncode != 0, "Truncated HBP file must fail verification!"


class TestStageGTrustChainHardening:
    """Stage G: Rigorous bidirectional cross-validation, physical disk reauth,
    exact integer invariant enforcement, trust anchor cryptography, and crash repair.
    """

    def test_negative_bidirectional_mismatch(self):
        exe = "./asolaria_equilibrium_suite.exe" if os.name == "nt" else "./asolaria_equilibrium_suite"
        if not os.path.exists(exe) or not os.path.exists("host8_mode_a_pilot100.hbp"):
            pytest.skip("Prerequisites not present.")

        with tempfile.TemporaryDirectory() as tmpdir:
            bad_hbp = os.path.join(tmpdir, "bidirectional.hbp")
            bad_hbi = os.path.join(tmpdir, "bidirectional.hbi")

            with open("host8_mode_a_pilot100.hbp", "rb") as f:
                with open(bad_hbp, "wb") as out_f:
                    out_f.write(f.read())

            with open("host8_mode_a_pilot100.hbi", "r", encoding="utf-8", newline="\n") as f:
                hbi_lines = f.readlines()

            # Alter room_start in HBI line 2 without modifying HBP
            hbi_lines[2] = hbi_lines[2].replace("|room_start=13824|", "|room_start=99999|")

            with open(bad_hbi, "w", encoding="utf-8", newline="\n") as f:
                f.writelines(hbi_lines)

            res = subprocess.run([
                exe, "--verify-hbp",
                "--hbp", bad_hbp,
                "--hbi", bad_hbi
            ], capture_output=True, text=True)
            assert res.returncode != 0, "Bidirectional divergence must fail!"
            assert "HBP/HBI bidirectional mismatch" in (res.stdout + res.stderr)

    def test_negative_energy_conservation_violation(self):
        exe = "./asolaria_equilibrium_suite.exe" if os.name == "nt" else "./asolaria_equilibrium_suite"
        if not os.path.exists(exe) or not os.path.exists("host8_mode_a_pilot100.hbp"):
            pytest.skip("Prerequisites not present.")

        with tempfile.TemporaryDirectory() as tmpdir:
            bad_hbp = os.path.join(tmpdir, "de_bad.hbp")
            bad_hbi = os.path.join(tmpdir, "de_bad.hbi")

            with open("host8_mode_a_pilot100.hbp", "r", encoding="utf-8", newline="\n") as f:
                hbp_lines = f.readlines()
            hbp_lines[2] = hbp_lines[2].replace("|dE=0|", "|dE=1|")

            # Update HBI line 2 length and sha to match modified line
            new_sha = hashlib.sha256(hbp_lines[2].encode("utf-8")).hexdigest()
            new_len = len(hbp_lines[2].encode("utf-8"))

            with open("host8_mode_a_pilot100.hbi", "r", encoding="utf-8", newline="\n") as f:
                hbi_lines = f.readlines()
            hbi_p = hbi_lines[2].strip().split("|")
            for i, p in enumerate(hbi_p):
                if p.startswith("hbp_sha256="):
                    hbi_p[i] = f"hbp_sha256={new_sha}"
                elif p.startswith("len="):
                    hbi_p[i] = f"len={new_len}"
            hbi_lines[2] = "|".join(hbi_p) + "\n"

            with open(bad_hbp, "w", encoding="utf-8", newline="\n") as f:
                f.writelines(hbp_lines)
            with open(bad_hbi, "w", encoding="utf-8", newline="\n") as f:
                f.writelines(hbi_lines)

            res = subprocess.run([
                exe, "--verify-hbp",
                "--hbp", bad_hbp,
                "--hbi", bad_hbi
            ], capture_output=True, text=True)
            assert res.returncode != 0, "Non-zero dE must fail verification!"
            assert "dE conservation failure" in (res.stdout + res.stderr)

    def test_negative_loschmidt_reversal_violation(self):
        exe = "./asolaria_equilibrium_suite.exe" if os.name == "nt" else "./asolaria_equilibrium_suite"
        if not os.path.exists(exe) or not os.path.exists("host8_mode_a_pilot100.hbp"):
            pytest.skip("Prerequisites not present.")

        with tempfile.TemporaryDirectory() as tmpdir:
            bad_hbp = os.path.join(tmpdir, "rev_bad.hbp")
            bad_hbi = os.path.join(tmpdir, "rev_bad.hbi")

            with open("host8_mode_a_pilot100.hbp", "r", encoding="utf-8", newline="\n") as f:
                hbp_lines = f.readlines()
            hbp_lines[2] = hbp_lines[2].replace("|rev_mismatches=0|", "|rev_mismatches=4|")

            new_sha = hashlib.sha256(hbp_lines[2].encode("utf-8")).hexdigest()
            new_len = len(hbp_lines[2].encode("utf-8"))

            with open("host8_mode_a_pilot100.hbi", "r", encoding="utf-8", newline="\n") as f:
                hbi_lines = f.readlines()
            hbi_p = hbi_lines[2].strip().split("|")
            for i, p in enumerate(hbi_p):
                if p.startswith("hbp_sha256="):
                    hbi_p[i] = f"hbp_sha256={new_sha}"
                elif p.startswith("len="):
                    hbi_p[i] = f"len={new_len}"
            hbi_lines[2] = "|".join(hbi_p) + "\n"

            with open(bad_hbp, "w", encoding="utf-8", newline="\n") as f:
                f.writelines(hbp_lines)
            with open(bad_hbi, "w", encoding="utf-8", newline="\n") as f:
                f.writelines(hbi_lines)

            res = subprocess.run([
                exe, "--verify-hbp",
                "--hbp", bad_hbp,
                "--hbi", bad_hbi
            ], capture_output=True, text=True)
            assert res.returncode != 0, "Non-zero rev_mismatches must fail verification!"
            assert "HBP Loschmidt mismatch" in (res.stdout + res.stderr)

    def test_negative_canonical_geometry_violation(self):
        exe = "./asolaria_equilibrium_suite.exe" if os.name == "nt" else "./asolaria_equilibrium_suite"
        if not os.path.exists(exe) or not os.path.exists("host8_mode_a_pilot100.hbp"):
            pytest.skip("Prerequisites not present.")

        with tempfile.TemporaryDirectory() as tmpdir:
            bad_hbp = os.path.join(tmpdir, "geom_bad.hbp")
            bad_hbi = os.path.join(tmpdir, "geom_bad.hbi")

            with open("host8_mode_a_pilot100.hbp", "r", encoding="utf-8", newline="\n") as f:
                hbp_lines = f.readlines()
            hbp_lines[2] = hbp_lines[2].replace("|rooms=13824|", "|rooms=13825|")

            new_sha = hashlib.sha256(hbp_lines[2].encode("utf-8")).hexdigest()
            new_len = len(hbp_lines[2].encode("utf-8"))

            with open("host8_mode_a_pilot100.hbi", "r", encoding="utf-8", newline="\n") as f:
                hbi_lines = f.readlines()
            hbi_p = hbi_lines[2].strip().split("|")
            for i, p in enumerate(hbi_p):
                if p.startswith("hbp_sha256="):
                    hbi_p[i] = f"hbp_sha256={new_sha}"
                elif p.startswith("len="):
                    hbi_p[i] = f"len={new_len}"
            hbi_lines[2] = "|".join(hbi_p) + "\n"

            with open(bad_hbp, "w", encoding="utf-8", newline="\n") as f:
                f.writelines(hbp_lines)
            with open(bad_hbi, "w", encoding="utf-8", newline="\n") as f:
                f.writelines(hbi_lines)

            res = subprocess.run([
                exe, "--verify-hbp",
                "--hbp", bad_hbp,
                "--hbi", bad_hbi
            ], capture_output=True, text=True)
            assert res.returncode != 0, "Non-canonical room count must fail verification!"
            assert "invalid rooms count" in (res.stdout + res.stderr)

    def test_trust_anchor_and_crash_repair(self):
        exe = "./asolaria_equilibrium_suite.exe" if os.name == "nt" else "./asolaria_equilibrium_suite"
        if not os.path.exists(exe):
            pytest.skip("Executable not present.")

        with tempfile.TemporaryDirectory() as tmpdir:
            # Create synthetic 2-window fixture for sector 1
            fix_sec = os.path.join(tmpdir, "sector-001.rooms")
            with open(fix_sec, "wb") as f:
                f.write(b"\x00" * (13824 * 8 * 3))

            hbp = os.path.join(tmpdir, "repair_test.hbp")
            hbi = os.path.join(tmpdir, "repair_test.hbi")
            anchor = os.path.join(tmpdir, "repair_test.anchor")

            # Run 2 windows in sector 1
            res = subprocess.run([
                exe, "--host8-mode-a",
                "--windows", "2",
                "--start-sector", "1",
                "--end-sector", "1",
                "--host8-dir", tmpdir,
                "--hbp", hbp,
                "--hbi", hbi,
                "--anchor", anchor,
                "--threads", "1"
            ], capture_output=True, text=True)
            assert res.returncode == 0, f"Execution failed: {res.stdout}\n{res.stderr}"

            # Verify trust anchor
            v_anc = subprocess.run([
                exe, "--verify-anchor",
                "--anchor", anchor,
                "--hbp", hbp,
                "--hbi", hbi
            ], capture_output=True, text=True)
            assert v_anc.returncode == 0, f"Trust anchor verification must succeed: {v_anc.stderr}"
            assert "ASOLARIA HOST8 TRUST ANCHOR VERIFICATION: PASS" in v_anc.stdout

            # Test crash corruption: simulate half-written trailing line
            with open(hbp, "a", encoding="utf-8", newline="\n") as f:
                f.write("HOST8WIN|sec=1|win=2|room_start=27648|partially_written_line_without_newline")
            with open(hbi, "a", encoding="utf-8", newline="\n") as f:
                f.write("HBI|sec=1|win=2|partial")

            # Verification without --repair-interrupted should fail
            v_fail = subprocess.run([
                exe, "--verify-hbp",
                "--hbp", hbp,
                "--hbi", hbi
            ], capture_output=True, text=True)
            assert v_fail.returncode != 0, "Partial trailing line must fail standard verification!"

            # Verification with --repair-interrupted should truncate and succeed!
            v_rep = subprocess.run([
                exe, "--verify-hbp",
                "--hbp", hbp,
                "--hbi", hbi,
                "--repair-interrupted"
            ], capture_output=True, text=True)
            assert v_rep.returncode == 0, f"Repair must succeed: {v_rep.stdout}\n{v_rep.stderr}"
            assert "Recovered from partial commit" in v_rep.stdout
            assert "100% UNBROKEN CRYPTOGRAPHIC INTEGRITY" in v_rep.stdout
