import csv

input_path = "host8_375_sector_physics_regression.csv"
output_path = "host8_375_sector_physics_provenance_v2.csv"

pinned_hashes = {
    0: "2dfe7d757ecb012475806b86fcdefc602680d042db836ffbcb2bfe924f2a18ee",
    187: "6752ab9e2603cc5042f7c7a41c3ddc48ae5571fd0aeb945d0a3f63b1a948a652",
    374: "963805be806e8a290aab8dee48b0d214d03015c84e02bf844f0de87e2b37c10a"
}

with open(input_path, "r", newline="") as infile:
    reader = csv.DictReader(infile)
    rows = list(reader)

assert len(rows) == 375, f"Expected 375 rows, got {len(rows)}"

v2_rows = []
for r in rows:
    sec = int(r["sector"])
    h = r["sha256_hash"]
    if sec in pinned_hashes:
        if h == pinned_hashes[sec]:
            auth_status = "PINNED_MATCH"
        else:
            auth_status = "HASH_MISMATCH"
    else:
        auth_status = "HASH_RECORDED"
    
    physics_status = r["status"]
    
    v2_rows.append({
        "sector": sec,
        "filepath": r["filepath"],
        "room_offset": r["room_offset"],
        "byte_offset": r["byte_offset"],
        "num_bytes": r["num_bytes"],
        "num_rooms": r["num_rooms"],
        "num_trits": r["num_trits"],
        "sha256_hash": h,
        "auth_status": auth_status,
        "forward_ticks": r["forward_ticks"],
        "reverse_ticks": r["reverse_ticks"],
        "entropy_trit": r["entropy_trit"],
        "e_total_dev": r["e_total_dev"],
        "max_vorticity": r["max_vorticity"],
        "loschmidt_mismatches": r["loschmidt_mismatches"],
        "physics_status": physics_status
    })

fieldnames = [
    "sector", "filepath", "room_offset", "byte_offset", "num_bytes",
    "num_rooms", "num_trits", "sha256_hash", "auth_status",
    "forward_ticks", "reverse_ticks", "entropy_trit", "e_total_dev",
    "max_vorticity", "loschmidt_mismatches", "physics_status"
]

with open(output_path, "w", newline="") as outfile:
    writer = csv.DictWriter(outfile, fieldnames=fieldnames)
    writer.writeheader()
    writer.writerows(v2_rows)

pinned_cnt = sum(1 for r in v2_rows if r["auth_status"] == "PINNED_MATCH")
rec_cnt = sum(1 for r in v2_rows if r["auth_status"] == "HASH_RECORDED")
print(f"Successfully generated {output_path} with {len(v2_rows)} rows: {pinned_cnt} PINNED_MATCH, {rec_cnt} HASH_RECORDED.")
