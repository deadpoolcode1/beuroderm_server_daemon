#!/usr/bin/env python3
import sys
import time
import subprocess
import os
import re
import socket
from collections import defaultdict

# Track failure counts by specific BIT test name
bit_failure_counts = defaultdict(int)
iteration_results = []

# Device connection settings
DEVICE_IP = "10.0.0.36"  # Adjust this to your device IP
DEVICE_PORT = 5797       # Default daemon port (from server.c)

def send_socket_command(command, ip=DEVICE_IP, port=DEVICE_PORT, timeout=10):
    """
    Send a command to the device daemon via socket.
    Returns the response or None on error.
    """
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        sock.connect((ip, port))
        sock.sendall(command.encode('utf-8'))
        response = sock.recv(4096).decode('utf-8', errors='ignore')
        sock.close()
        return response
    except Exception as e:
        print(f"  Socket error: {e}")
        return None

def trigger_fresh_bit_test():
    """
    Trigger a fresh BIT test on the device.
    This sends 'write_bit:bittest' command to the daemon.
    """
    print("  Triggering fresh BIT test via socket...")
    response = send_socket_command("write_bit:bittest")
    if response:
        print(f"  BIT test triggered, response: {response.strip()}")
        return True
    return False

def read_bit_result_via_socket():
    """
    Read BIT result directly from daemon via socket.
    Returns the JSON BIT status string.
    """
    print("  Reading BIT result via socket...")
    response = send_socket_command("read_bit:bittest")
    if response:
        print(f"  BIT response received")
        return response
    return None

def parse_bit_failures(bit_output):
    """
    Parse BIT output to find all BIT test failures.
    Returns a list of failed BIT test names.
    """
    failed_bits = []

    # Pattern 1: JSON-style "BIT_XXX": false
    json_pattern = r'"(BIT_[A-Z0-9_]+)":\s*false'
    matches = re.findall(json_pattern, bit_output)
    failed_bits.extend(matches)

    # Pattern 2: bit_result:0 (general failure indicator)
    if "bit_result:0" in bit_output and not failed_bits:
        # If we see bit_result:0 but no specific BIT failures, mark as unknown
        failed_bits.append("BIT_UNKNOWN_FAILURE")

    # Pattern 3: Look for other common BIT failure patterns
    other_patterns = [
        r'BIT\s+(\w+)\s+failed',
        r'BIT_(\w+)\s*=\s*FAIL',
        r'(\w+_BIT)\s*:\s*FAIL',
    ]
    for pattern in other_patterns:
        matches = re.findall(pattern, bit_output, re.IGNORECASE)
        for match in matches:
            bit_name = f"BIT_{match.upper()}" if not match.startswith("BIT_") else match.upper()
            if bit_name not in failed_bits:
                failed_bits.append(bit_name)

    return failed_bits

def wait_for_device_ready(timeout=120):
    """
    Wait for device to be fully ready (adb shell responsive and daemon socket open).
    Returns True if device is ready, False on timeout.
    """
    print("  Waiting for device to be ready...")
    start_time = time.time()

    # First wait for adb to be responsive
    while time.time() - start_time < timeout:
        try:
            result = subprocess.run(
                ['adb', 'shell', 'echo', 'ready'],
                capture_output=True,
                timeout=5
            )
            if result.returncode == 0 and 'ready' in result.stdout.decode():
                print("  ADB shell responsive")
                break
        except (subprocess.TimeoutExpired, subprocess.CalledProcessError):
            pass
        time.sleep(2)
    else:
        print("  Timeout waiting for ADB")
        return False

    # Now wait for daemon socket to be available
    print("  Waiting for daemon socket to be available...")
    socket_wait_start = time.time()
    while time.time() - socket_wait_start < 60:
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(3)
            sock.connect((DEVICE_IP, DEVICE_PORT))
            sock.close()
            print("  Daemon socket is available")
            return True
        except:
            time.sleep(2)

    print("  Timeout waiting for daemon socket")
    return False

def check_bit_result_fresh(iteration):
    """
    Trigger a fresh BIT test and check results.
    This ensures we're testing the current state, not early boot state.
    Returns tuple: (passed: bool, failed_bits: list)
    """
    global bit_failure_counts

    # Trigger fresh BIT test
    if not trigger_fresh_bit_test():
        print("  Warning: Could not trigger fresh BIT test, falling back to logcat")
        return check_bit_result_from_logcat(iteration)

    # Wait for BIT test to complete (it runs various hardware tests)
    print("  Waiting 5s for BIT test to complete...")
    time.sleep(5)

    # Read BIT result via socket
    bit_response = read_bit_result_via_socket()

    if bit_response:
        failed_bits = parse_bit_failures(bit_response)

        if failed_bits:
            # Save the BIT response on failure
            timestamp = time.strftime('%Y-%m-%d_%H-%M-%S')
            log_dir = "logs"
            os.makedirs(log_dir, exist_ok=True)
            log_filename = os.path.join(log_dir, f"bit_test_fail_iter{iteration}_{timestamp}.log")
            with open(log_filename, 'w') as log_file:
                log_file.write(f"BIT Response:\n{bit_response}\n\n")
                # Also capture logcat for additional context
                try:
                    logcat = subprocess.check_output('adb logcat -d', shell=True, stderr=subprocess.STDOUT)
                    log_file.write(f"Logcat:\n{logcat.decode('utf-8', errors='ignore')}")
                except:
                    pass
            print(f"  Log saved to {log_filename}")

            # Update failure counts
            for bit_name in failed_bits:
                bit_failure_counts[bit_name] += 1

            return False, failed_bits

        return True, []

    # Fallback to logcat if socket read failed
    print("  Socket read failed, falling back to logcat")
    return check_bit_result_from_logcat(iteration)

def check_bit_result_from_logcat(iteration):
    """
    Fallback: Fetch logcat and check for BIT failures.
    Returns tuple: (passed: bool, failed_bits: list)
    """
    global bit_failure_counts

    command = 'adb logcat -d'
    try:
        result = subprocess.check_output(command, shell=True, stderr=subprocess.STDOUT)
        logcat_output = result.decode('utf-8', errors='ignore')

        failed_bits = parse_bit_failures(logcat_output)

        if failed_bits:
            # Save log on failure
            timestamp = time.strftime('%Y-%m-%d_%H-%M-%S')
            log_dir = "logs"
            os.makedirs(log_dir, exist_ok=True)
            log_filename = os.path.join(log_dir, f"bit_test_fail_iter{iteration}_{timestamp}_logcat.log")
            with open(log_filename, 'w') as log_file:
                log_file.write(logcat_output)
            print(f"  Log saved to {log_filename}")

            # Update failure counts
            for bit_name in failed_bits:
                bit_failure_counts[bit_name] += 1

            return False, failed_bits

        return True, []

    except subprocess.CalledProcessError as e:
        print(f"Error fetching logcat: {e.output.decode(errors='ignore')}")
        return None, ["ADB_ERROR"]

def reboot_device():
    try:
        # Clear logcat before reboot
        subprocess.check_output('adb logcat -c', shell=True, stderr=subprocess.STDOUT)
        subprocess.check_output('adb reboot', shell=True, stderr=subprocess.STDOUT)
        print("  Rebooting device...")
        time.sleep(30)
    except subprocess.CalledProcessError as e:
        print(f"Error rebooting device: {e.output.decode(errors='ignore')}")

def print_iteration_result(iteration, passed, failed_bits):
    """Print result for a single iteration."""
    timestamp = time.strftime('%Y-%m-%d %H:%M:%S')
    status = "PASS" if passed else "FAIL"

    print(f"\n  [{timestamp}] Iteration {iteration}: {status}")

    if not passed and failed_bits:
        print(f"  Failed BITs: {', '.join(failed_bits)}")

    # Store for summary
    iteration_results.append({
        'iteration': iteration,
        'timestamp': timestamp,
        'passed': passed,
        'failed_bits': failed_bits
    })

def generate_summary_report(passed, failed, total):
    """Generate comprehensive summary report."""
    print("\n")
    print("=" * 70)
    print(" " * 25 + "TEST SUMMARY REPORT")
    print("=" * 70)

    # Overall results
    print(f"\n{'Overall Results':-^70}")
    print(f"  Total Tests:  {total}")
    print(f"  Passed:       {passed} ({100*passed/total:.1f}%)" if total > 0 else "  Passed:       0")
    print(f"  Failed:       {failed} ({100*failed/total:.1f}%)" if total > 0 else "  Failed:       0")

    # Per-iteration results table
    print(f"\n{'Per-Iteration Results':-^70}")
    print(f"  {'Iter':<6} {'Time':<20} {'Status':<8} {'Failed BITs':<30}")
    print("  " + "-" * 66)

    for result in iteration_results:
        status = "PASS" if result['passed'] else "FAIL"
        failed_str = ', '.join(result['failed_bits']) if result['failed_bits'] else '-'
        # Truncate if too long
        if len(failed_str) > 28:
            failed_str = failed_str[:25] + "..."
        print(f"  {result['iteration']:<6} {result['timestamp']:<20} {status:<8} {failed_str:<30}")

    # Failure breakdown by BIT type
    if bit_failure_counts:
        print(f"\n{'Failure Breakdown by BIT Type':-^70}")
        print(f"  {'BIT Name':<40} {'Count':<10} {'% of Failures':<15}")
        print("  " + "-" * 66)

        total_bit_failures = sum(bit_failure_counts.values())
        sorted_failures = sorted(bit_failure_counts.items(), key=lambda x: x[1], reverse=True)

        for bit_name, count in sorted_failures:
            percentage = 100 * count / total_bit_failures if total_bit_failures > 0 else 0
            print(f"  {bit_name:<40} {count:<10} {percentage:.1f}%")

        print("  " + "-" * 66)
        print(f"  {'TOTAL':<40} {total_bit_failures:<10}")
    else:
        print(f"\n{'No BIT failures detected':-^70}")

    print("\n" + "=" * 70)

def main():
    global DEVICE_IP, DEVICE_PORT

    if len(sys.argv) < 2:
        print("Usage: python3 lifetime.py <iterations> [device_ip] [device_port]")
        print("       Use -1 for infinite iterations")
        print("       Default device_ip: 10.0.0.36")
        print("       Default device_port: 5797")
        sys.exit(1)

    try:
        iterations = int(sys.argv[1])
    except ValueError:
        print("Invalid number of iterations, please provide an integer.")
        sys.exit(1)

    # Optional: device IP and port
    if len(sys.argv) >= 3:
        DEVICE_IP = sys.argv[2]
    if len(sys.argv) >= 4:
        DEVICE_PORT = int(sys.argv[3])

    print(f"Device IP: {DEVICE_IP}, Port: {DEVICE_PORT}")

    iteration = 0
    passed_count = 0
    failed_count = 0

    print(f"Starting BIT lifetime test: {iterations if iterations != -1 else 'infinite'} iterations")
    print("=" * 70)
    print("\nNOTE: This script triggers a FRESH BIT test after device stabilizes,")
    print("      rather than reading the early boot BIT result from logcat.")
    print("=" * 70)

    try:
        while iterations == -1 or iteration < iterations:
            iteration += 1
            iter_str = f"{iteration}/{iterations}" if iterations != -1 else f"{iteration}/inf"
            print(f"\n[Iteration {iter_str}]")

            # Wait for device to be fully ready (ADB + daemon socket)
            if not wait_for_device_ready(timeout=120):
                print("  Device not ready, skipping iteration")
                continue

            # Additional stabilization time for hardware/filesystem
            print("  Waiting 30s for system stabilization...")
            time.sleep(30)

            print("  Checking BIT results (triggering fresh test)...")
            passed, failed_bits = check_bit_result_fresh(iteration)

            if passed is None:
                print("  Skipping iteration due to error")
                continue

            if passed:
                passed_count += 1
            else:
                failed_count += 1

            print_iteration_result(iteration, passed, failed_bits)
            reboot_device()

    except KeyboardInterrupt:
        print("\n\nTest interrupted by user.")

    generate_summary_report(passed_count, failed_count, passed_count + failed_count)
    print("Test complete.")

if __name__ == "__main__":
    main()
