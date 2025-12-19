"""
OTA Update API Test Harness

Tests typical OTA update scenarios:
1. Golden path: one firmware, one node
2. Multiple nodes: two nodes update successfully
3. Partial failure: one node succeeds, one fails

Usage:
    cd server/api
    pip install httpx pytest pytest-asyncio
    pytest tests/test_ota_scenarios.py -v

Or run directly:
    python tests/test_ota_scenarios.py
"""

import asyncio
import httpx
import sys

# Windows console encoding fix
if sys.platform == "win32":
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

BASE_URL = "http://localhost:8000"


async def cleanup_test_data(client: httpx.AsyncClient):
    """Remove test firmware and updates."""
    # List and delete all firmware
    resp = await client.get(f"{BASE_URL}/api/v1/firmware")
    if resp.status_code == 200:
        data = resp.json()
        for fw in data.get("items", []):
            await client.delete(f"{BASE_URL}/api/v1/firmware/{fw['id']}")

    # List and delete all OTA updates
    resp = await client.get(f"{BASE_URL}/api/v1/ota/updates")
    if resp.status_code == 200:
        for update in resp.json():
            try:
                await client.delete(f"{BASE_URL}/api/v1/ota/updates/{update['id']}")
            except:
                pass  # May fail if in progress


async def upload_test_firmware(client: httpx.AsyncClient, node_type: str, version: str) -> int:
    """Upload a test firmware binary."""
    # Create fake firmware content
    fake_binary = f"FAKE_FIRMWARE_{node_type}_{version}".encode() * 100  # ~2KB

    files = {"file": (f"{node_type}_{version}.bin", fake_binary, "application/octet-stream")}
    data = {
        "node_type": node_type,
        "version": version,
        "release_notes": f"Test firmware for {node_type} v{version}",
    }

    resp = await client.post(f"{BASE_URL}/api/v1/firmware/upload", files=files, data=data)
    assert resp.status_code == 200, f"Firmware upload failed: {resp.text}"

    firmware = resp.json()
    print(f"  ✓ Uploaded firmware: {node_type} v{version} (id={firmware['id']}, size={firmware['size_bytes']})")
    return firmware["id"]


async def create_update_job(client: httpx.AsyncClient, firmware_id: int, target_node_id: str = None) -> int:
    """Create an OTA update job."""
    payload = {"firmware_id": firmware_id}
    if target_node_id:
        payload["target_node_id"] = target_node_id

    resp = await client.post(f"{BASE_URL}/api/v1/ota/updates", json=payload)
    assert resp.status_code == 200, f"Create update failed: {resp.text}"

    update = resp.json()
    target = target_node_id or "all nodes"
    print(f"  ✓ Created update job: id={update['id']} for {target}")
    return update["id"]


async def simulate_gateway_poll(client: httpx.AsyncClient) -> list:
    """Simulate gateway polling for pending updates."""
    resp = await client.get(f"{BASE_URL}/api/v1/ota/updates/pending")
    assert resp.status_code == 200, f"Poll failed: {resp.text}"

    updates = resp.json()
    print(f"  ✓ Gateway polled: {len(updates)} pending update(s)")
    return updates


async def simulate_gateway_start(client: httpx.AsyncClient, update_id: int):
    """Simulate gateway starting distribution."""
    resp = await client.post(f"{BASE_URL}/api/v1/ota/updates/{update_id}/start")
    assert resp.status_code == 200, f"Start failed: {resp.text}"
    print(f"  ✓ Gateway started distribution for update {update_id}")


async def simulate_node_progress(
    client: httpx.AsyncClient,
    update_id: int,
    node_id: str,
    total_parts: int,
    fail_at_part: int = None,
):
    """Simulate a node receiving OTA update chunks."""
    for part in range(total_parts + 1):
        if fail_at_part and part == fail_at_part:
            # Simulate failure
            progress = {
                "current_part": part,
                "total_parts": total_parts,
                "status": "failed",
                "error_message": f"Simulated failure at part {part}",
            }
            resp = await client.post(
                f"{BASE_URL}/api/v1/ota/updates/{update_id}/node/{node_id}/progress",
                json=progress,
            )
            assert resp.status_code == 200
            print(f"  ✗ Node {node_id}: FAILED at part {part}/{total_parts}")
            return False

        status = "completed" if part == total_parts else "downloading"
        progress = {
            "current_part": part,
            "total_parts": total_parts,
            "status": status,
        }
        resp = await client.post(
            f"{BASE_URL}/api/v1/ota/updates/{update_id}/node/{node_id}/progress",
            json=progress,
        )
        assert resp.status_code == 200

        if part == total_parts:
            print(f"  ✓ Node {node_id}: COMPLETED ({total_parts} parts)")

    return True


async def get_update_status(client: httpx.AsyncClient, update_id: int) -> dict:
    """Get detailed update status."""
    resp = await client.get(f"{BASE_URL}/api/v1/ota/updates/{update_id}")
    assert resp.status_code == 200, f"Get status failed: {resp.text}"
    return resp.json()


async def complete_update(client: httpx.AsyncClient, update_id: int):
    """Mark update as completed."""
    resp = await client.post(f"{BASE_URL}/api/v1/ota/updates/{update_id}/complete")
    assert resp.status_code == 200
    print(f"  ✓ Update {update_id} marked as completed")


async def fail_update(client: httpx.AsyncClient, update_id: int, error: str):
    """Mark update as failed."""
    resp = await client.post(f"{BASE_URL}/api/v1/ota/updates/{update_id}/fail", params={"error_message": error})
    assert resp.status_code == 200
    print(f"  ✗ Update {update_id} marked as failed: {error}")


# =============================================================================
# Test Scenarios
# =============================================================================

async def test_scenario_1_golden_path_single_node():
    """
    Scenario 1: Golden Path - One Firmware, One Node

    1. Upload firmware for PIR node
    2. Create update job
    3. Gateway polls and finds pending update
    4. Gateway starts distribution
    5. Single node receives all chunks successfully
    6. Update marked as completed
    """
    print("\n" + "=" * 60)
    print("SCENARIO 1: Golden Path - Single Node")
    print("=" * 60)

    async with httpx.AsyncClient(timeout=30.0) as client:
        await cleanup_test_data(client)

        # Step 1: Upload firmware
        print("\n[Step 1] Upload firmware")
        firmware_id = await upload_test_firmware(client, "pir", "1.0.0")

        # Step 2: Create update job
        print("\n[Step 2] Create update job")
        update_id = await create_update_job(client, firmware_id)

        # Step 3: Gateway polls
        print("\n[Step 3] Gateway polls for pending updates")
        pending = await simulate_gateway_poll(client)
        assert len(pending) == 1, "Expected 1 pending update"
        assert pending[0]["update_id"] == update_id

        num_parts = pending[0]["num_parts"]
        print(f"         Firmware has {num_parts} parts")

        # Step 4: Gateway starts distribution
        print("\n[Step 4] Gateway starts distribution")
        await simulate_gateway_start(client, update_id)

        # Verify no longer in pending
        pending = await simulate_gateway_poll(client)
        assert len(pending) == 0, "Update should no longer be pending"

        # Step 5: Node receives update
        print("\n[Step 5] Node receives update")
        success = await simulate_node_progress(client, update_id, "node_pir_1", num_parts)
        assert success, "Node should complete successfully"

        # Step 6: Mark update completed
        print("\n[Step 6] Complete update")
        await complete_update(client, update_id)

        # Verify final status
        status = await get_update_status(client, update_id)
        assert status["status"] == "completed"
        assert len(status["nodes"]) == 1
        assert status["nodes"][0]["status"] == "completed"

        print("\n✓ SCENARIO 1 PASSED")
        return True


async def test_scenario_2_multiple_nodes():
    """
    Scenario 2: Multiple Nodes - Two Nodes Update Successfully

    1. Upload firmware for LED node
    2. Create update job targeting all LED nodes
    3. Gateway polls and starts distribution
    4. Two nodes receive update successfully
    5. Update marked as completed
    """
    print("\n" + "=" * 60)
    print("SCENARIO 2: Multiple Nodes - Two Nodes Success")
    print("=" * 60)

    async with httpx.AsyncClient(timeout=30.0) as client:
        await cleanup_test_data(client)

        # Step 1: Upload firmware
        print("\n[Step 1] Upload firmware")
        firmware_id = await upload_test_firmware(client, "led", "2.0.0")

        # Step 2: Create update job
        print("\n[Step 2] Create update job for all LED nodes")
        update_id = await create_update_job(client, firmware_id)

        # Step 3: Gateway polls and starts
        print("\n[Step 3] Gateway polls and starts")
        pending = await simulate_gateway_poll(client)
        num_parts = pending[0]["num_parts"]
        await simulate_gateway_start(client, update_id)

        # Step 4: Two nodes receive update
        print("\n[Step 4] Two nodes receive update")
        node1_success = await simulate_node_progress(client, update_id, "node_led_1", num_parts)
        node2_success = await simulate_node_progress(client, update_id, "node_led_2", num_parts)

        assert node1_success and node2_success, "Both nodes should succeed"

        # Step 5: Complete update
        print("\n[Step 5] Complete update")
        await complete_update(client, update_id)

        # Verify final status
        status = await get_update_status(client, update_id)
        assert status["status"] == "completed"
        assert len(status["nodes"]) == 2
        assert all(n["status"] == "completed" for n in status["nodes"])

        print("\n✓ SCENARIO 2 PASSED")
        return True


async def test_scenario_3_partial_failure():
    """
    Scenario 3: Partial Failure - One Succeeds, One Fails

    1. Upload firmware for button node
    2. Create update job
    3. Gateway starts distribution
    4. Node 1 completes successfully
    5. Node 2 fails midway
    6. Update marked as failed (partial)
    """
    print("\n" + "=" * 60)
    print("SCENARIO 3: Partial Failure - One Success, One Failure")
    print("=" * 60)

    async with httpx.AsyncClient(timeout=30.0) as client:
        await cleanup_test_data(client)

        # Step 1: Upload firmware
        print("\n[Step 1] Upload firmware")
        firmware_id = await upload_test_firmware(client, "button", "1.5.0")

        # Step 2: Create update job
        print("\n[Step 2] Create update job")
        update_id = await create_update_job(client, firmware_id)

        # Step 3: Gateway polls and starts
        print("\n[Step 3] Gateway polls and starts")
        pending = await simulate_gateway_poll(client)
        num_parts = pending[0]["num_parts"]
        await simulate_gateway_start(client, update_id)

        # Step 4: Node 1 succeeds
        print("\n[Step 4] Node 1 receives update (success)")
        node1_success = await simulate_node_progress(client, update_id, "node_btn_1", num_parts)
        assert node1_success, "Node 1 should succeed"

        # Step 5: Node 2 fails
        print("\n[Step 5] Node 2 receives update (fails at part 1)")
        fail_part = min(1, num_parts)  # Fail early
        node2_success = await simulate_node_progress(
            client, update_id, "node_btn_2", num_parts, fail_at_part=fail_part
        )
        assert not node2_success, "Node 2 should fail"

        # Step 6: Mark update as failed
        print("\n[Step 6] Mark update as failed (partial)")
        await fail_update(client, update_id, "Partial failure: 1/2 nodes succeeded")

        # Verify final status
        status = await get_update_status(client, update_id)
        assert status["status"] == "failed"
        assert len(status["nodes"]) == 2

        node_statuses = {n["node_id"]: n["status"] for n in status["nodes"]}
        assert node_statuses["node_btn_1"] == "completed"
        assert node_statuses["node_btn_2"] == "failed"

        print("\n✓ SCENARIO 3 PASSED")
        return True


async def test_scenario_4_cancel_pending():
    """
    Scenario 4: Cancel a pending update before distribution starts.
    """
    print("\n" + "=" * 60)
    print("SCENARIO 4: Cancel Pending Update")
    print("=" * 60)

    async with httpx.AsyncClient(timeout=30.0) as client:
        await cleanup_test_data(client)

        # Upload and create
        print("\n[Step 1] Upload firmware and create update")
        firmware_id = await upload_test_firmware(client, "watcher", "1.0.0")
        update_id = await create_update_job(client, firmware_id)

        # Verify pending
        pending = await simulate_gateway_poll(client)
        assert len(pending) == 1

        # Cancel
        print("\n[Step 2] Cancel update before distribution")
        resp = await client.delete(f"{BASE_URL}/api/v1/ota/updates/{update_id}")
        assert resp.status_code == 200
        print(f"  ✓ Update {update_id} cancelled")

        # Verify no longer pending
        pending = await simulate_gateway_poll(client)
        assert len(pending) == 0, "Update should be cancelled"

        print("\n✓ SCENARIO 4 PASSED")
        return True


async def run_all_scenarios():
    """Run all test scenarios."""
    print("\n" + "=" * 60)
    print("OTA UPDATE API TEST HARNESS")
    print("=" * 60)
    print(f"Target: {BASE_URL}")

    # Check server is running
    async with httpx.AsyncClient(timeout=5.0) as client:
        try:
            resp = await client.get(f"{BASE_URL}/health")
            if resp.status_code != 200:
                print(f"\n✗ Server health check failed: {resp.status_code}")
                return False
            print("✓ Server is healthy")
        except httpx.ConnectError:
            print(f"\n✗ Cannot connect to server at {BASE_URL}")
            print("  Make sure the server is running: docker-compose up")
            return False

    results = []

    try:
        results.append(("Scenario 1: Golden Path", await test_scenario_1_golden_path_single_node()))
        results.append(("Scenario 2: Multiple Nodes", await test_scenario_2_multiple_nodes()))
        results.append(("Scenario 3: Partial Failure", await test_scenario_3_partial_failure()))
        results.append(("Scenario 4: Cancel Pending", await test_scenario_4_cancel_pending()))
    except Exception as e:
        print(f"\n✗ Test failed with error: {e}")
        import traceback
        traceback.print_exc()
        return False

    # Summary
    print("\n" + "=" * 60)
    print("TEST SUMMARY")
    print("=" * 60)

    all_passed = True
    for name, passed in results:
        status = "✓ PASS" if passed else "✗ FAIL"
        print(f"  {status}: {name}")
        if not passed:
            all_passed = False

    print("=" * 60)
    if all_passed:
        print("ALL TESTS PASSED")
    else:
        print("SOME TESTS FAILED")

    return all_passed


if __name__ == "__main__":
    success = asyncio.run(run_all_scenarios())
    sys.exit(0 if success else 1)
